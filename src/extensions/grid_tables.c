/**
 * Grid Tables Extension for Apex
 * Implementation
 *
 * Preprocessing extension that converts Pandoc grid table syntax to
 * pipe table format before the regular cmark parser runs.
 */

#include "grid_tables.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>

#define MAX_COLUMNS 64

/**
 * Check if a line starts with '+' (after whitespace)
 * This indicates a potential grid table
 */
static bool is_grid_table_start(const char *line) {
    if (!line) return false;
    
    /* Skip whitespace */
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    
    return *line == '+';
}

/**
 * Check if a line is a grid table separator (starts with + followed by - or =)
 * A separator row contains only +, -, =, :, spaces
 * It should NOT contain | characters (pipes indicate cell boundaries, not separators)
 * 
 * IMPORTANT: This function checks if a line is a TABLE-LEVEL separator (separating rows).
 * Nested separators within cells (like +-------+ within | Cell +-------+ |) are NOT
 * table separators - they're just part of the cell content.
 */
static bool is_grid_table_separator(const char *line) {
    if (!line) return false;
    
    /* Skip whitespace */
    const char *p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    
    if (*p != '+') return false;
    p++;
    
    /* Check for pattern: +---+ or +===+ */
    /* A separator should only contain +, -, =, :, and spaces - NO pipes */
    /* Note: === may have been converted to <mark> tags by earlier preprocessing,
     * so we need to handle that case too */
    bool has_dash_or_equal = false;
    bool in_mark_tag = false;
    int mark_tag_depth = 0;
    
    while (*p && *p != '\n' && *p != '\r') {
        if (*p == '|') {
            /* Pipes indicate this is a content row, not a separator */
            /* However, nested separators within cells might have pipes around them,
             * so we need to check if this is a full-line separator or nested */
            /* If we see a pipe, this is NOT a table-level separator */
            return false;
        } else if (*p == '<' && strncmp(p, "<mark>", 6) == 0) {
            /* Handle <mark> tag (converted from ===) */
            in_mark_tag = true;
            mark_tag_depth++;
            p += 6;
            continue;
        } else if (*p == '<' && strncmp(p, "</mark>", 7) == 0) {
            /* Handle </mark> tag */
            in_mark_tag = false;
            mark_tag_depth--;
            if (mark_tag_depth == 0) {
                has_dash_or_equal = true; /* Treat <mark> as equivalent to = */
            }
            p += 7;
            continue;
        } else if (in_mark_tag) {
            /* Inside <mark> tag, skip content */
            p++;
            continue;
        } else if (*p == '-' || *p == '=') {
            has_dash_or_equal = true;
        } else if (*p != ':' && *p != '+' && *p != ' ' && *p != '\t') {
            /* Contains other characters - not a separator */
            return false;
        }
        p++;
    }
    
    return has_dash_or_equal;
}

/**
 * Check if a line is a header separator (contains = characters)
 */
static bool is_header_separator(const char *line) {
    if (!line) return false;
    
    /* Skip whitespace */
    const char *p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    
    if (*p != '+') return false;
    
    /* Check if line contains '=' characters (header separator) */
    /* Note: === may have been converted to <mark> tags by earlier preprocessing */
    while (*p && *p != '\n' && *p != '\r') {
        if (*p == '<' && strncmp(p, "<mark>", 6) == 0) {
            /* Handle <mark> tag (converted from ===) - treat as = */
            return true;
        } else if (*p == '=') {
            return true;
        }
        p++;
    }
    
    return false;
}

/**
 * Parse alignment from a separator line
 * Returns alignment array (0=left, 1=center, 2=right)
 * Column count is returned via column_count parameter
 * Format: +:===:+ (center), +===:+ (right), +:===+ (left with colon), +---+ (left)
 */
static void parse_alignment(const char *line, int *alignments, size_t *column_count) {
    if (!line || !alignments || !column_count) return;
    
    *column_count = 0;
    
    /* Skip whitespace */
    const char *p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    
    if (*p != '+') return;
    p++;
    
    size_t col = 0;
    bool left_colon = false;
    bool right_colon = false;
    
    while (*p && col < MAX_COLUMNS) {
        if (*p == '+') {
            /* Column boundary - determine alignment */
            if (left_colon && right_colon) {
                alignments[col] = 1; /* Center: :---: or :===: */
            } else if (right_colon) {
                alignments[col] = 2; /* Right: ---: or ===: */
            } else {
                alignments[col] = 0; /* Left: --- or === or :--- */
            }
            col++;
            left_colon = false;
            right_colon = false;
        } else if (*p == ':') {
            if (p == line + 1 || (p > line && (p[-1] == '+' || p[-1] == ' ' || p[-1] == '\t'))) {
                left_colon = true;
            } else {
                right_colon = true;
            }
        } else if (*p == '-' || *p == '=') {
            /* Part of separator, continue */
        }
        p++;
    }
    
    *column_count = col;
}

/**
 * Extract cell count from a single line (for column count inference)
 */
static size_t count_cells_in_line(const char *line) {
    if (!line) return 0;
    
    const char *p = line;
    
    /* Skip whitespace */
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    
    /* Skip leading + if present */
    if (*p == '+') {
        p++;
    }
    
    bool starts_with_pipe = (*p == '|');
    size_t pipe_count = 0;
    
    /* Count | characters */
    while (*p && *p != '\n' && *p != '\r') {
        if (*p == '|') {
            pipe_count++;
        }
        p++;
    }
    
    /* Number of cells: if starts with |, cells = pipes - 1, else pipes + 1 */
    if (starts_with_pipe) {
        return pipe_count > 0 ? pipe_count - 1 : 0;
    } else {
        return pipe_count + 1;
    }
}

/**
 * Check if cell content contains block-level elements
 * (code blocks, lists, multiple paragraphs, etc.)
 */
static bool cell_has_block_elements(const char *content) {
    if (!content) return false;
    
    const char *p = content;
    bool in_code_block = false;
    int list_marker_count = 0;
    int paragraph_count = 0;
    bool prev_was_blank = false;
    
    while (*p) {
        /* Check for code blocks */
        if (strncmp(p, "```", 3) == 0 || strncmp(p, "~~~", 3) == 0) {
            return true; /* Code block found */
        }
        
        /* Check for list markers */
        if ((*p == '-' || *p == '*' || *p == '+') && 
            (p[1] == ' ' || p[1] == '\t')) {
            list_marker_count++;
        }
        if (isdigit((unsigned char)*p)) {
            const char *num_start = p;
            while (isdigit((unsigned char)*p)) p++;
            if (*p == '.' && (p[1] == ' ' || p[1] == '\t')) {
                list_marker_count++;
            }
            p = num_start;
        }
        
        /* Check for multiple paragraphs (blank line between content) */
        if (*p == '\n') {
            if (prev_was_blank && paragraph_count == 0) {
                /* Found blank line - check if there's content before and after */
                const char *before = p - 1;
                while (before > content && (*before == ' ' || *before == '\t' || *before == '\r')) {
                    before--;
                }
                if (before > content) {
                    const char *after = p + 1;
                    while (*after == '\n' || *after == '\r' || *after == ' ' || *after == '\t') {
                        after++;
                    }
                    if (*after && *after != '\0') {
                        paragraph_count++;
                    }
                }
            }
            prev_was_blank = true;
        } else if (*p != ' ' && *p != '\t' && *p != '\r') {
            prev_was_blank = false;
        }
        
        p++;
    }
    
    /* If we found lists or multiple paragraphs, it has block elements */
    return (list_marker_count > 0 || paragraph_count > 0);
}

/**
 * Convert block-level markdown in cell to HTML format
 * This allows block elements to be preserved in pipe table cells
 */
static char *convert_cell_block_elements_to_html(const char *content) {
    if (!content) return NULL;
    
    /* For now, preserve content as-is but wrap in a div for block elements */
    /* This is a simplified approach - full implementation would parse and convert each block type */
    size_t len = strlen(content);
    size_t html_len = len + 100; /* Extra space for HTML tags */
    char *html = malloc(html_len);
    if (!html) return strdup(content); /* Fallback to original */
    
    /* Check if it has block elements */
    if (!cell_has_block_elements(content)) {
        /* No block elements - return as-is */
        strcpy(html, content);
        return html;
    }
    
    /* Has block elements - wrap in div and preserve newlines as <br> or keep structure */
    /* Simple approach: wrap in <div> and preserve line breaks */
    char *p = html;
    const char *src = content;
    
    strcpy(p, "<div>");
    p += 5;
    
    /* Convert newlines to <br> for simple cases, or preserve structure for code blocks */
    bool in_code = false;
    while (*src) {
        if (strncmp(src, "```", 3) == 0 || strncmp(src, "~~~", 3) == 0) {
            in_code = !in_code;
            /* Copy code block marker */
            *p++ = *src++;
            *p++ = *src++;
            *p++ = *src++;
            continue;
        }
        
        if (*src == '\n') {
            if (in_code) {
                *p++ = '\n'; /* Preserve newlines in code */
            } else {
                /* Convert to <br> for paragraphs, but preserve double newlines for block separation */
                const char *next = src + 1;
                while (*next == ' ' || *next == '\t' || *next == '\r') next++;
                if (*next == '\n' || *next == '\0') {
                    /* Double newline or end - preserve as paragraph break */
                    strcpy(p, "</p><p>");
                    p += 7;
                } else {
                    strcpy(p, "<br>");
                    p += 4;
                }
            }
            src++;
        } else {
            *p++ = *src++;
        }
        
        /* Check buffer size */
        if ((size_t)(p - html) > html_len - 20) {
            size_t current_len = p - html;
            html_len *= 2;
            char *new_html = realloc(html, html_len);
            if (!new_html) {
                free(html);
                return strdup(content);
            }
            html = new_html;
            p = html + current_len;
        }
    }
    
    strcpy(p, "</div>");
    p += 6;
    *p = '\0';
    
    return html;
}

/**
 * Check if two lines have the same column structure (same number of pipes in same positions)
 * This helps determine if lines should be combined into multi-line cells
 */
static bool has_same_column_structure(const char *line1, const char *line2) {
    if (!line1 || !line2) return false;
    
    const char *p1 = line1;
    const char *p2 = line2;
    
    /* Skip leading whitespace */
    while (*p1 == ' ' || *p1 == '\t') p1++;
    while (*p2 == ' ' || *p2 == '\t') p2++;
    
    /* Skip leading + if present */
    if (*p1 == '+') p1++;
    if (*p2 == '+') p2++;
    
    /* Count pipes in both lines - they should have the same number */
    size_t pipe_count1 = 0, pipe_count2 = 0;
    const char *q1 = p1, *q2 = p2;
    while (*q1 && *q1 != '\n' && *q1 != '\r') {
        if (*q1 == '|') pipe_count1++;
        q1++;
    }
    while (*q2 && *q2 != '\n' && *q2 != '\r') {
        if (*q2 == '|') pipe_count2++;
        q2++;
    }
    
    /* Lines must have the same number of pipes to have the same column structure */
    return pipe_count1 > 0 && pipe_count1 == pipe_count2;
}

/**
 * Extract cell content from grid table row lines
 * Grid table rows can span multiple lines until the next separator
 * Returns array of cell strings (caller must free)
 */
static char **extract_cells_from_row_lines(char **row_lines, size_t row_line_count, size_t *cell_count) {
    if (!row_lines || row_line_count == 0 || !cell_count) return NULL;
    
    char **cells = malloc(MAX_COLUMNS * sizeof(char*));
    if (!cells) return NULL;
    
    *cell_count = 0;
    
    /* Collect all cell content from all row lines */
    for (size_t line_idx = 0; line_idx < row_line_count; line_idx++) {
        const char *line = row_lines[line_idx];
        if (!line) continue;
        
        /* Skip separator lines - they shouldn't be in row_lines, but double-check */
        if (is_grid_table_separator(line)) {
            continue;
        }
        
        /* Skip whitespace */
        const char *p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        
        /* Skip leading + if present (shouldn't be in content rows, but handle it) */
        if (*p == '+') {
            p++;
        }
        
        /* Extract cells from this line */
        const char *cell_start = NULL;
        size_t current_col = 0;
        bool first_pipe = true;
        
        while (*p && current_col < MAX_COLUMNS) {
            if (*p == '|') {
                /* Cell boundary */
                if (cell_start) {
                    /* Trim trailing whitespace */
                    const char *cell_end = p;
                    while (cell_end > cell_start && isspace((unsigned char)cell_end[-1])) {
                        cell_end--;
                    }
                    
                    size_t cell_len = cell_end - cell_start;
                    
                    if (current_col < *cell_count && cells[current_col]) {
                        /* Append to existing cell (multi-line cell) */
                        /* Join with newline to preserve block element structure */
                        size_t old_len = strlen(cells[current_col]);
                        size_t new_len = old_len + cell_len + 1; /* +1 for newline */
                        char *new_cell = realloc(cells[current_col], new_len + 1);
                        if (new_cell) {
                            cells[current_col] = new_cell;
                            if (old_len > 0) {
                                cells[current_col][old_len] = '\n'; /* Join with newline for block elements */
                                old_len++;
                            }
                            memcpy(cells[current_col] + old_len, cell_start, cell_len);
                            cells[current_col][old_len + cell_len] = '\0';
                        }
                    } else {
                        /* New cell */
                        if (current_col >= *cell_count) {
                            *cell_count = current_col + 1;
                        }
                        cells[current_col] = malloc(cell_len + 1);
                        if (cells[current_col]) {
                            memcpy(cells[current_col], cell_start, cell_len);
                            cells[current_col][cell_len] = '\0';
                        }
                    }
                    current_col++;
                } else if (!first_pipe) {
                    /* Empty cell (not at start of line) */
                    if (current_col >= *cell_count) {
                        *cell_count = current_col + 1;
                    }
                    cells[current_col] = malloc(1);
                    if (cells[current_col]) {
                        cells[current_col][0] = '\0';
                    }
                    current_col++;
                }
                /* Skip the first pipe (leading pipe) - don't create a cell for it */
                first_pipe = false;
                cell_start = p + 1;
            } else if (*p == '\n' || *p == '\r') {
                break;
            }
            p++;
        }
        
        /* Handle final cell if line doesn't end with | */
        if (cell_start && *cell_start && current_col < MAX_COLUMNS) {
            const char *cell_end = p;
            while (cell_end > cell_start && isspace((unsigned char)cell_end[-1])) {
                cell_end--;
            }
            
            size_t cell_len = cell_end - cell_start;
            if (current_col >= *cell_count) {
                *cell_count = current_col + 1;
            }
            
            if (cells[current_col]) {
                /* Append to existing */
                size_t old_len = strlen(cells[current_col]);
                size_t new_len = old_len + cell_len + 1;
                char *new_cell = realloc(cells[current_col], new_len + 1);
                if (new_cell) {
                    cells[current_col] = new_cell;
                    if (old_len > 0) {
                        cells[current_col][old_len] = '\n'; /* Join with newline */
                        old_len++;
                    }
                    memcpy(cells[current_col] + old_len, cell_start, cell_len);
                    cells[current_col][old_len + cell_len] = '\0';
                }
            } else {
                cells[current_col] = malloc(cell_len + 1);
                if (cells[current_col]) {
                    memcpy(cells[current_col], cell_start, cell_len);
                    cells[current_col][cell_len] = '\0';
                }
            }
        }
    }
    
    return cells;
}

/**
 * Create pipe table separator row from alignment array
 */
static char *create_pipe_separator(int *alignments, size_t column_count) {
    if (!alignments || column_count == 0) return NULL;
    
    /* Calculate size needed */
    size_t len = column_count * 8 + 2; /* | :---: | ... | */
    char *sep = malloc(len);
    if (!sep) return NULL;
    
    char *p = sep;
    
    for (size_t i = 0; i < column_count; i++) {
        *p++ = '|';
        
        int align = alignments[i];
        if (align == 1) {
            /* Center: :---: */
            *p++ = ' ';
            *p++ = ':';
            *p++ = '-';
            *p++ = '-';
            *p++ = '-';
            *p++ = ':';
            *p++ = ' ';
        } else if (align == 2) {
            /* Right: ---: */
            *p++ = ' ';
            *p++ = '-';
            *p++ = '-';
            *p++ = '-';
            *p++ = ':';
            *p++ = ' ';
        } else {
            /* Left: --- */
            *p++ = ' ';
            *p++ = '-';
            *p++ = '-';
            *p++ = '-';
            *p++ = ' ';
        }
    }
    
    *p++ = '|';
    
    *p = '\0';
    return sep;
}

/**
 * Convert grid table rows to pipe table format
 * Groups lines between separators into rows and handles multi-line cells
 */
static char *convert_grid_table_to_pipe(char **lines, size_t line_count, 
                                        int *alignments, size_t column_count) {
    if (!lines || line_count == 0 || !alignments || column_count == 0) {
        return NULL;
    }
    
    /* Calculate approximate size needed */
    size_t total_size = 0;
    for (size_t i = 0; i < line_count; i++) {
        if (lines[i]) {
            total_size += strlen(lines[i]) + column_count * 4 + 10;
        }
    }
    total_size += 200; /* Extra for separator and formatting */
    
    char *output = malloc(total_size);
    if (!output) return NULL;
    
    char *p = output;
    size_t remaining = total_size;
    
    /* Add blank lines at start of table for proper recognition */
    *p++ = '\n';
    *p++ = '\n';
    remaining -= 2;
    
    bool separator_processed = false;
    bool header_written = false;
    bool rows_written = false;  /* Track if we've written any rows */
    
    /* First, find the header separator to identify header vs body */
    size_t header_sep_idx = 0;
    bool has_header_sep = false;
    for (size_t i = 0; i < line_count; i++) {
        if (lines[i] && is_header_separator(lines[i])) {
            header_sep_idx = i;
            has_header_sep = true;
            break;
        }
    }
    
    /* Group lines into rows (lines between separators) */
    size_t row_start = 0;
    
    for (size_t i = 0; i < line_count; i++) {
        const char *line = lines[i];
        if (!line) continue;
        
        /* Check if this is a separator line */
        if (is_grid_table_separator(line)) {
            bool is_header_sep = is_header_separator(line);
            bool is_footer_sep = false;
            
            /* Check if this is a footer separator (last separator before blank line) */
            if (is_header_sep && i > 0) {
                /* Check if there are more non-separator lines after this */
                bool has_more_content = false;
                for (size_t j = i + 1; j < line_count; j++) {
                    if (lines[j] && !is_grid_table_separator(lines[j]) && 
                        strchr(lines[j], '|')) {
                        has_more_content = true;
                        break;
                    }
                }
                if (!has_more_content) {
                    is_footer_sep = true;
                }
            }
            
            /* Process rows between previous separator and this one */
            if (i > row_start) {
                /* Check if this is header section (before header separator) */
                bool is_header_section = (has_header_sep && i <= header_sep_idx);
                
                /* Group lines into logical rows (lines with same column structure are multi-line cells) */
                size_t line_idx = row_start;
                while (line_idx < i) {
                    /* Find all consecutive lines with same column structure */
                    size_t row_line_start = line_idx;
                    size_t row_line_count = 0;
                    
                    /* Collect lines that form a single logical row */
                    const char *first_line = NULL;
                    for (size_t j = line_idx; j < i; j++) {
                        if (lines[j] && !is_grid_table_separator(lines[j]) && strchr(lines[j], '|')) {
                            if (!first_line) {
                                first_line = lines[j];
                                row_line_start = j;
                                row_line_count = 1;
                            } else if (has_same_column_structure(first_line, lines[j])) {
                                /* Same column structure - part of multi-line cell */
                                row_line_count++;
                            } else {
                                /* Different column structure - new row */
                                /* Note: Lines with nested separators (like +-------+ within cells) will be
                                 * treated as separate rows, which is correct. The nested separator will be
                                 * included as literal text in the cell content. */
                                break;
                            }
                        } else if (first_line) {
                            /* Blank line or separator - end of current row */
                            break;
                        } else {
                            /* Skip blank lines before first content line */
                            line_idx++;
                        }
                    }
                    
                    if (row_line_count > 0) {
                        /* Extract cells from all lines in this logical row */
                        char **row_lines_array = malloc(row_line_count * sizeof(char*));
                        if (row_lines_array) {
                            for (size_t j = 0; j < row_line_count; j++) {
                                row_lines_array[j] = lines[row_line_start + j];
                            }
                            
                            size_t cell_count = 0;
                            char **cells = extract_cells_from_row_lines(row_lines_array, row_line_count, &cell_count);
                            
                                if (cells && cell_count > 0) {
                                /* Write header separator after first header row, before first body row */
                                if (is_header_section && !header_written && !separator_processed && !is_footer_sep) {
                                    header_written = true;
                                } else if (!is_header_section && !separator_processed && !is_footer_sep) {
                                    /* This is first body row - write separator before it */
                                    char *sep = create_pipe_separator(alignments, column_count);
                                    if (sep) {
                                        size_t sep_len = strlen(sep);
                                        if (sep_len < remaining) {
                                            memcpy(p, sep, sep_len);
                                            p += sep_len;
                                            remaining -= sep_len;
                                            *p++ = '\n';
                                            remaining--;
                                        }
                                        free(sep);
                                    }
                                    separator_processed = true;
                                }
                                
                                /* Convert to pipe table row */
                                if (remaining > column_count * 50) {
                                    *p++ = '|';
                                    remaining--;
                                    
                                    for (size_t k = 0; k < column_count; k++) {
                                        const char *cell_content = (k < cell_count && cells[k]) ? cells[k] : "";
                                        
                                        /* Trim leading/trailing whitespace from cell content */
                                        const char *content_start = cell_content;
                                        const char *content_end = cell_content + strlen(cell_content);
                                        while (content_start < content_end && isspace((unsigned char)*content_start)) {
                                            content_start++;
                                        }
                                        while (content_end > content_start && isspace((unsigned char)content_end[-1])) {
                                            content_end--;
                                        }
                                        size_t content_len = content_end - content_start;
                                        
                                        if (content_len > 0 && content_len < remaining - 5) {
                                            *p++ = ' ';
                                            remaining--;
                                            memcpy(p, content_start, content_len);
                                            p += content_len;
                                            remaining -= content_len;
                                            *p++ = ' ';
                                            remaining--;
                                        } else {
                                            *p++ = ' ';
                                            remaining--;
                                        }
                                        
                                        *p++ = '|';
                                        remaining--;
                                    }
                                    
                                    *p++ = '\n';
                                    remaining--;
                                    rows_written = true;  /* Mark that we've written a row */
                                }
                                
                                /* Free cells */
                                for (size_t k = 0; k < cell_count; k++) {
                                    if (cells[k]) free(cells[k]);
                                }
                                free(cells);
                            }
                            
                            free(row_lines_array);
                        }
                        
                        line_idx = row_line_start + row_line_count;
                    } else {
                        line_idx++;
                    }
                }
            }
            
            /* Write separator after header rows if this is the header separator */
            if (is_header_sep && !is_footer_sep && !separator_processed && header_written) {
                char *sep = create_pipe_separator(alignments, column_count);
                if (sep) {
                    size_t sep_len = strlen(sep);
                    if (sep_len < remaining) {
                        memcpy(p, sep, sep_len);
                        p += sep_len;
                        remaining -= sep_len;
                        *p++ = '\n';
                        remaining--;
                    }
                    free(sep);
                }
                separator_processed = true;
            } else if (!is_header_sep && !is_footer_sep && !separator_processed && rows_written) {
                /* Regular separator (not header) - write separator after rows have been written */
                /* But only if we haven't written header separator yet */
                if (!has_header_sep || i > header_sep_idx) {
                    char *sep = create_pipe_separator(alignments, column_count);
                    if (sep) {
                        size_t sep_len = strlen(sep);
                        if (sep_len < remaining) {
                            memcpy(p, sep, sep_len);
                            p += sep_len;
                            remaining -= sep_len;
                            *p++ = '\n';
                            remaining--;
                        }
                        free(sep);
                    }
                    separator_processed = true;
                }
            }
            
            row_start = i + 1;
            continue;
        }
    }
    
    /* Process final rows after last separator */
    if (row_start < line_count) {
        char **row_lines = malloc((line_count - row_start) * sizeof(char*));
        if (row_lines) {
            size_t row_line_count = 0;
            for (size_t j = row_start; j < line_count; j++) {
                /* Skip separator rows - only include content rows */
                if (lines[j] && !is_grid_table_separator(lines[j]) && strchr(lines[j], '|')) {
                    row_lines[row_line_count++] = lines[j];
                }
            }
            
            if (row_line_count > 0) {
                size_t cell_count = 0;
                char **cells = extract_cells_from_row_lines(row_lines, row_line_count, &cell_count);
                
                if (cells && cell_count > 0) {
                    if (remaining > column_count * 50) {
                        *p++ = '|';
                        remaining--;
                        
                        for (size_t j = 0; j < column_count; j++) {
                            const char *cell_content = (j < cell_count && cells[j]) ? cells[j] : "";
                            
                            /* Trim leading/trailing whitespace from cell content */
                            const char *content_start = cell_content;
                            const char *content_end = cell_content + strlen(cell_content);
                            while (content_start < content_end && isspace((unsigned char)*content_start)) {
                                content_start++;
                            }
                            while (content_end > content_start && isspace((unsigned char)content_end[-1])) {
                                content_end--;
                            }
                            size_t content_len = content_end - content_start;
                            
                            *p++ = '|';
                            remaining--;
                            
                            if (content_len > 0 && content_len < remaining - 5) {
                                *p++ = ' ';
                                remaining--;
                                memcpy(p, content_start, content_len);
                                p += content_len;
                                remaining -= content_len;
                                *p++ = ' ';
                                remaining--;
                            } else {
                                *p++ = ' ';
                                remaining--;
                            }
                        }
                        
                        *p++ = '\n';
                        remaining--;
                    }
                    
                    for (size_t j = 0; j < cell_count; j++) {
                        if (cells[j]) free(cells[j]);
                    }
                    free(cells);
                }
            }
            free(row_lines);
        }
    }
    
    /* Add blank line at end of table */
    if (p > output && p[-1] != '\n') {
        *p++ = '\n';
        remaining--;
    }
    *p++ = '\n';
    remaining--;
    
    *p = '\0';
    return output;
}

/**
 * Main preprocessing function
 */
char *apex_preprocess_grid_tables(const char *text) {
    if (!text) return NULL;
    
    size_t len = strlen(text);
    if (len == 0) return strdup("");
    
    /* Allocate output buffer (may grow) */
    size_t cap = len * 2 + 1;
    char *output = malloc(cap);
    if (!output) return NULL;
    
    const char *read = text;
    char *write = output;
    size_t remaining = cap;
    bool in_code_block = false;
    
    while (*read) {
        const char *line_start = read;
        
        /* Find line ending */
        const char *line_end = strchr(read, '\n');
        if (!line_end) {
            line_end = read + strlen(read);
        }
        
        size_t line_len = line_end - line_start;
        bool has_newline = (*line_end == '\n');
        
        /* Track code blocks */
        const char *p = line_start;
        while (p < line_end && (*p == ' ' || *p == '\t')) {
            p++;
        }
        
        if (!in_code_block &&
            (line_end - p) >= 3 &&
            ((p[0] == '`' && p[1] == '`' && p[2] == '`') ||
             (p[0] == '~' && p[1] == '~' && p[2] == '~'))) {
            in_code_block = true;
        } else if (in_code_block &&
                   (line_end - p) >= 3 &&
                   ((p[0] == '`' && p[1] == '`' && p[2] == '`') ||
                    (p[0] == '~' && p[1] == '~' && p[2] == '~'))) {
            in_code_block = false;
        }
        
        /* Check if this is a grid table start */
        if (!in_code_block && is_grid_table_start(line_start)) {
            /* Collect all consecutive non-blank lines until blank line */
            char **table_lines = malloc(100 * sizeof(char*));
            if (!table_lines) {
                /* Out of memory - copy line as-is */
                if (line_len < remaining) {
                    memcpy(write, line_start, line_len);
                    write += line_len;
                    remaining -= line_len;
                    if (has_newline) {
                        *write++ = '\n';
                        remaining--;
                    }
                }
                read = has_newline ? line_end + 1 : line_end;
                continue;
            }
            
            size_t table_line_count = 0;
            
            /* Collect table lines */
            while (*read && table_line_count < 100) {
                const char *current_line_start = read;
                const char *current_line_end = strchr(read, '\n');
                if (!current_line_end) {
                    current_line_end = read + strlen(read);
                }
                
                /* Check if blank line */
                bool is_blank = true;
                for (const char *q = current_line_start; q < current_line_end; q++) {
                    if (!isspace((unsigned char)*q)) {
                        is_blank = false;
                        break;
                    }
                }
                
                if (is_blank) {
                    break;
                }
                
                size_t current_line_len = current_line_end - current_line_start;
                table_lines[table_line_count] = malloc(current_line_len + 1);
                if (table_lines[table_line_count]) {
                    memcpy(table_lines[table_line_count], current_line_start, current_line_len);
                    table_lines[table_line_count][current_line_len] = '\0';
                    table_line_count++;
                }
                
                read = (*current_line_end == '\n') ? current_line_end + 1 : current_line_end;
            }
            
            if (table_line_count > 0) {
                /* Parse alignment from separator lines and find maximum column count */
                int alignments[MAX_COLUMNS] = {0};
                size_t column_count = 0;
                
                /* First pass: find maximum column count from all separators */
                for (size_t i = 0; i < table_line_count; i++) {
                    if (is_grid_table_separator(table_lines[i])) {
                        size_t sep_cols = 0;
                        int sep_alignments[MAX_COLUMNS] = {0};
                        parse_alignment(table_lines[i], sep_alignments, &sep_cols);
                        if (sep_cols > column_count) {
                            column_count = sep_cols;
                            /* Use alignments from the separator with most columns */
                            memcpy(alignments, sep_alignments, sizeof(sep_alignments));
                        }
                    }
                }
                
                /* If no separator found, try to infer from content rows */
                if (column_count == 0) {
                    for (size_t i = 0; i < table_line_count; i++) {
                        if (strchr(table_lines[i], '|') && !is_grid_table_separator(table_lines[i])) {
                            size_t row_cols = count_cells_in_line(table_lines[i]);
                            if (row_cols > column_count) {
                                column_count = row_cols;
                            }
                        }
                    }
                    /* Set default alignments */
                    for (size_t j = 0; j < column_count && j < MAX_COLUMNS; j++) {
                        alignments[j] = 0; /* Default left */
                    }
                }
                
                /* Convert grid table to pipe table */
                char *pipe_table = convert_grid_table_to_pipe(table_lines, table_line_count,
                                                              alignments, column_count);
                
                if (pipe_table) {
                    size_t pipe_len = strlen(pipe_table);
                    
                    /* Ensure we have enough space */
                    if (remaining < pipe_len) {
                        size_t written = write - output;
                        cap = (written + pipe_len + 100) * 2;
                        char *new_output = realloc(output, cap);
                        if (new_output) {
                            output = new_output;
                            write = output + written;
                            remaining = cap - written;
                        }
                    }
                    
                    /* Write the table (it already has blank lines at start and end) */
                    if (pipe_len <= remaining) {
                        memcpy(write, pipe_table, pipe_len);
                        write += pipe_len;
                        remaining -= pipe_len;
                    }
                    
                    free(pipe_table);
                }
                
                /* Free table lines */
                for (size_t i = 0; i < table_line_count; i++) {
                    if (table_lines[i]) free(table_lines[i]);
                }
            }
            
            free(table_lines);
            
            /* read is already pointing past all the table lines we just processed
             * (it was updated in the loop at line 1009) */
            /* Skip the blank line if present */
            if (*read == '\n') {
                read++;
            }
            continue;
        }
        
        /* Not a grid table - copy line as-is */
        if (line_len < remaining) {
            memcpy(write, line_start, line_len);
            write += line_len;
            remaining -= line_len;
            if (has_newline) {
                *write++ = '\n';
                remaining--;
            }
        } else {
            /* Need to reallocate */
            size_t written = write - output;
            cap = (written + line_len + 1) * 2;
            char *new_output = realloc(output, cap);
            if (new_output) {
                output = new_output;
                write = output + written;
                remaining = cap - written;
                memcpy(write, line_start, line_len);
                write += line_len;
                remaining -= line_len;
                if (has_newline) {
                    *write++ = '\n';
                    remaining--;
                }
            }
        }
        
        read = has_newline ? line_end + 1 : line_end;
    }
    
    *write = '\0';
    return output;
}
