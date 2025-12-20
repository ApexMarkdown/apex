/**
 * Table HTML Postprocessing
 * Implementation
 *
 * This is a pragmatic solution: we walk the AST to collect cells with
 * rowspan/colspan attributes, then do pattern matching on the HTML to inject them.
 */

#include "table_html_postprocess.h"
#include "cmark-gfm-core-extensions.h"
#include "table.h"  /* For CMARK_NODE_TABLE_ROW, CMARK_NODE_TABLE_CELL */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* Structure to track cells with attributes */
typedef struct cell_attr {
    int table_index;   /* Which table (0, 1, 2, ...) */
    int row_index;
    int col_index;
    char *attributes;  /* e.g. " rowspan=\"2\"" or " data-remove=\"true\"" */
    char *cell_text;  /* Store cell content for content-based matching */
    struct cell_attr *next;
} cell_attr;

/* Structure to track rows that should be in tfoot */
typedef struct tfoot_row {
    int table_index;
    int row_index;
    struct tfoot_row *next;
} tfoot_row;

/* Structure to track table captions */
typedef struct table_caption {
    int table_index;   /* Which table (0, 1, 2, ...) */
    char *caption;     /* Caption text */
    struct table_caption *next;
} table_caption;

/* Structure to track paragraphs to remove (caption paragraphs) */
typedef struct para_to_remove {
    int para_index;    /* Which paragraph (0, 1, 2, ...) */
    char *text_fingerprint;  /* First 50 chars for matching */
    struct para_to_remove *next;
} para_to_remove;

/* Structure to track all cells (for mapping calculation) */
typedef struct all_cell {
    int table_index;
    int row_index;
    int col_index;
    bool is_removed;  /* true if marked with data-remove */
    struct all_cell *next;
} all_cell;

/**
 * Walk AST and collect all cells (for mapping calculation)
 */
static all_cell *collect_all_cells(cmark_node *document) {
    all_cell *list = NULL;

    cmark_iter *iter = cmark_iter_new(document);
    cmark_event_type ev_type;

    int table_index = -1; /* Track which table we're in */
    int row_index = -1;  /* Start at -1, will increment to 0 on first row */
    int col_index = 0;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        cmark_node_type type = cmark_node_get_type(node);

        if (ev_type == CMARK_EVENT_ENTER) {
            if (type == CMARK_NODE_TABLE) {
                table_index++;  /* New table */
                row_index = -1;  /* Reset row index for new table */
            } else if (type == CMARK_NODE_TABLE_ROW) {
                row_index++;  /* Increment for each row */
                col_index = 0;
            } else if (type == CMARK_NODE_TABLE_CELL) {
                char *attrs = (char *)cmark_node_get_user_data(node);
                bool is_removed = (attrs && strstr(attrs, "data-remove"));

                /* Store ALL cells for mapping calculation */
                all_cell *cell = malloc(sizeof(all_cell));
                if (cell) {
                    cell->table_index = table_index;
                    cell->row_index = row_index;
                    cell->col_index = col_index;
                    cell->is_removed = is_removed;
                    cell->next = list;
                    list = cell;
                }

                col_index++;
            }
        }
    }

    cmark_iter_free(iter);
    return list;
}

/**
 * Walk AST and collect cells with attributes
 */
static cell_attr *collect_table_cell_attributes(cmark_node *document) {
    cell_attr *list = NULL;

    cmark_iter *iter = cmark_iter_new(document);
    cmark_event_type ev_type;

    int table_index = -1; /* Track which table we're in */
    int row_index = -1;  /* Start at -1, will increment to 0 on first row */
    int col_index = 0;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        cmark_node_type type = cmark_node_get_type(node);

        if (ev_type == CMARK_EVENT_ENTER) {
            if (type == CMARK_NODE_TABLE) {
                table_index++;  /* New table */
                row_index = -1;  /* Reset row index for new table */
            } else if (type == CMARK_NODE_TABLE_ROW) {
                row_index++;  /* Increment for each row */
                col_index = 0;

                /* Check if this row is marked as tfoot */
                char *row_attrs = (char *)cmark_node_get_user_data(node);
                if (row_attrs && strstr(row_attrs, "data-tfoot")) {
                    /* Store this row as a tfoot row */
                    tfoot_row *tfoot = malloc(sizeof(tfoot_row));
                    if (tfoot) {
                        tfoot->table_index = table_index;
                        tfoot->row_index = row_index;
                        /* We'll add this to a list later - for now just mark it on the row */
                        /* We'll check for it during HTML processing */
                    }
                }
            } else if (type == CMARK_NODE_TABLE_CELL) {
                char *attrs = (char *)cmark_node_get_user_data(node);
                if (attrs) {
                    /* Get cell content for debugging */
                    cmark_node *text_node = cmark_node_first_child(node);
                    const char *cell_text = text_node ? cmark_node_get_literal(text_node) : "?";

                    /* Store this cell's attributes */
                    cell_attr *attr = malloc(sizeof(cell_attr));
                    if (attr) {
                        attr->table_index = table_index;
                        attr->row_index = row_index;
                        attr->col_index = col_index;
                        attr->attributes = strdup(attrs);
                        attr->cell_text = cell_text ? strdup(cell_text) : NULL;
                        attr->next = list;
                        list = attr;
                    }
                }
                /* Count all cells (including removed ones) to match the column indices
                 * used in advanced_tables.c when finding target cells for rowspan.
                 * The HTML renderer will remove cells with data-remove, but we need to
                 * match based on the original column positions. */
                col_index++;
            }
        }
    }

    cmark_iter_free(iter);
    return list;
}

/**
 * Get text fingerprint from paragraph node (first 50 chars for matching)
 */
static char *get_para_text_fingerprint(cmark_node *node) {
    if (!node || cmark_node_get_type(node) != CMARK_NODE_PARAGRAPH) return NULL;

    cmark_node *child = cmark_node_first_child(node);
    if (child && cmark_node_get_type(child) == CMARK_NODE_TEXT) {
        const char *text = cmark_node_get_literal(child);
        if (text) {
            size_t len = strlen(text);
            if (len > 50) len = 50;
            char *fingerprint = malloc(len + 1);
            if (fingerprint) {
                memcpy(fingerprint, text, len);
                fingerprint[len] = '\0';
                return fingerprint;
            }
        }
    }
    return NULL;
}

/**
 * Walk AST and collect table captions and paragraphs to remove
 */
static table_caption *collect_table_captions(cmark_node *document, para_to_remove **paras_to_remove) {
    table_caption *list = NULL;
    *paras_to_remove = NULL;

    cmark_iter *iter = cmark_iter_new(document);
    cmark_event_type ev_type;

    int table_index = -1; /* Track which table we're in */
    int para_index = -1;  /* Track paragraph index */

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        cmark_node_type type = cmark_node_get_type(node);

        if (ev_type == CMARK_EVENT_ENTER) {
            if (type == CMARK_NODE_TABLE) {
                table_index++;  /* New table */

                /* Check for caption in user_data */
                char *user_data = (char *)cmark_node_get_user_data(node);
                bool caption_found = false;
                if (user_data && strstr(user_data, "data-caption=")) {
                    caption_found = true;
                    /* Extract caption text - handle both " data-caption=" and just "data-caption=" */
                    char caption[512];
                    const char *caption_start = strstr(user_data, "data-caption=");
                    if (caption_start) {
                        caption_start += strlen("data-caption=");
                        /* Skip the opening quote */
                        if (*caption_start == '"') {
                            caption_start++;
                            /* Extract until closing quote */
                            int i = 0;
                            while (*caption_start && *caption_start != '"' && i < 511) {
                                caption[i++] = *caption_start++;
                            }
                            caption[i] = '\0';
                            if (i > 0) {
                                table_caption *cap = malloc(sizeof(table_caption));
                                if (cap) {
                                    cap->table_index = table_index;
                                    cap->caption = strdup(caption);
                                    cap->next = list;
                                    list = cap;
                                }
                            }
                        }
                    }
                }
                /* If caption not found in user_data, check for caption paragraph before/after table */
                /* This handles cases where IAL processing replaced user_data */
                if (!caption_found) {
                    /* Check previous node for caption */
                    cmark_node *prev = cmark_node_previous(node);
                    if (prev && cmark_node_get_type(prev) == CMARK_NODE_PARAGRAPH) {
                        /* Check if previous paragraph is a caption */
                        cmark_node *text_node = cmark_node_first_child(prev);
                        if (text_node && cmark_node_get_type(text_node) == CMARK_NODE_TEXT) {
                            const char *text = cmark_node_get_literal(text_node);
                            if (text && text[0] == '[') {
                                const char *end = strchr(text + 1, ']');
                                if (end) {
                                    const char *after = end + 1;
                                    while (*after && isspace((unsigned char)*after)) after++;
                                    if (*after == '\0') {
                                        /* This is a caption - extract it */
                                        size_t caption_len = end - text - 1;
                                        char *caption = malloc(caption_len + 1);
                                        if (caption) {
                                            memcpy(caption, text + 1, caption_len);
                                            caption[caption_len] = '\0';
                                            table_caption *cap = malloc(sizeof(table_caption));
                                            if (cap) {
                                                cap->table_index = table_index;
                                                cap->caption = caption;
                                                cap->next = list;
                                                list = cap;
                                                caption_found = true;
                                            } else {
                                                free(caption);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    /* Also check next node for caption */
                    if (!caption_found) {
                        cmark_node *next = cmark_node_next(node);
                        if (next && cmark_node_get_type(next) == CMARK_NODE_PARAGRAPH) {
                            cmark_node *text_node = cmark_node_first_child(next);
                            if (text_node && cmark_node_get_type(text_node) == CMARK_NODE_TEXT) {
                                const char *text = cmark_node_get_literal(text_node);
                                if (text && text[0] == '[') {
                                    const char *end = strchr(text + 1, ']');
                                    if (end) {
                                        const char *after = end + 1;
                                        while (*after && isspace((unsigned char)*after)) after++;
                                        if (*after == '\0') {
                                            /* This is a caption - extract it */
                                            size_t caption_len = end - text - 1;
                                            char *caption = malloc(caption_len + 1);
                                            if (caption) {
                                                memcpy(caption, text + 1, caption_len);
                                                caption[caption_len] = '\0';
                                                table_caption *cap = malloc(sizeof(table_caption));
                                                if (cap) {
                                                    cap->table_index = table_index;
                                                    cap->caption = caption;
                                                    cap->next = list;
                                                    list = cap;
                                                } else {
                                                    free(caption);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            } else if (type == CMARK_NODE_PARAGRAPH) {
                para_index++;

                /* Check if this paragraph is marked for removal */
                char *user_data = (char *)cmark_node_get_user_data(node);
                if (user_data && strstr(user_data, "data-remove")) {
                    char *fingerprint = get_para_text_fingerprint(node);
                    if (fingerprint) {
                        para_to_remove *para = malloc(sizeof(para_to_remove));
                        if (para) {
                            para->para_index = para_index;
                            para->text_fingerprint = fingerprint;
                            para->next = *paras_to_remove;
                            *paras_to_remove = para;
                        } else {
                            free(fingerprint);
                        }
                    }
                }
            }
        }
    }

    cmark_iter_free(iter);
    return list;
}

/**
 * Inject attributes into HTML or remove cells
 * Also wraps tables with captions in <figure> tags
 */
/**
 * Collect rows that should be in tfoot sections
 */
static tfoot_row *collect_tfoot_rows(cmark_node *document) {
    tfoot_row *list = NULL;

    cmark_iter *iter = cmark_iter_new(document);
    cmark_event_type ev_type;

    int table_index = -1;
    int row_index = -1;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        cmark_node_type type = cmark_node_get_type(node);

        if (ev_type == CMARK_EVENT_ENTER) {
            if (type == CMARK_NODE_TABLE) {
                table_index++;
                row_index = -1;
            } else if (type == CMARK_NODE_TABLE_ROW) {
                row_index++;
                char *row_attrs = (char *)cmark_node_get_user_data(node);
                if (row_attrs && strstr(row_attrs, "data-tfoot")) {
                    tfoot_row *tfoot = malloc(sizeof(tfoot_row));
                    if (tfoot) {
                        tfoot->table_index = table_index;
                        tfoot->row_index = row_index;
                        tfoot->next = list;
                        list = tfoot;
                    }
                }
            }
        }
    }

    cmark_iter_free(iter);
    return list;
}

char *apex_inject_table_attributes(const char *html, cmark_node *document, int caption_position) {
    if (!html || !document) return (char *)html;

    /* Collect all cells (for mapping calculation) */
    all_cell *all_cells = collect_all_cells(document);

    /* Collect all cells with attributes */
    cell_attr *attrs = collect_table_cell_attributes(document);

    /* Collect tfoot rows */
    tfoot_row *tfoot_rows = collect_tfoot_rows(document);

    /* Collect all table captions and paragraphs to remove */
    para_to_remove *paras_to_remove = NULL;
    table_caption *captions = collect_table_captions(document, &paras_to_remove);

    /* If nothing to do, return original */
    if (!attrs && !captions && !paras_to_remove) return (char *)html;

    /* Allocate output buffer (same size as input, we'll realloc if needed) */
    size_t capacity = strlen(html) * 2;
    char *output = malloc(capacity);
    if (!output) {
        /* Clean up */
        while (all_cells) {
            all_cell *next = all_cells->next;
            free(all_cells);
            all_cells = next;
        }
        while (attrs) {
            cell_attr *next = attrs->next;
            free(attrs->attributes);
            if (attrs->cell_text) free(attrs->cell_text);
            free(attrs);
            attrs = next;
        }
        while (captions) {
            table_caption *next = captions->next;
            free(captions->caption);
            free(captions);
            captions = next;
        }
        while (paras_to_remove) {
            para_to_remove *next = paras_to_remove->next;
            free(paras_to_remove->text_fingerprint);
            free(paras_to_remove);
            paras_to_remove = next;
        }
        return (char *)html;
    }

    const char *read = html;
    char *write = output;
    size_t written = 0;
    int table_idx = -1; /* Track which table we're in */
    int row_idx = -1;
    int ast_row_idx = -1;  /* AST row index corresponding to current HTML row */
    int col_idx = 0;
    int para_idx = -1;  /* Track paragraph index */
    bool in_table = false;
    bool in_row = false;
    bool in_tbody = false;  /* Track if we're currently in tbody */
    bool in_tfoot = false;  /* Track if we're currently in tfoot */
    bool current_row_is_tfoot = false;  /* Track if current row is a tfoot row */

    /* Pre-calculated mapping for current row: HTML position -> original column index */
    int row_col_mapping[50];  /* row_col_mapping[html_pos] = original_col_index */
    int row_col_mapping_size = 0;

    /* Track active rowspan cells per column (inspired by Jekyll Spaceship approach).
     * active_rowspan_cells[col] points to the cell_attr for the cell that's currently
     * being rowspanned in that column. When we see a ^^ cell, we increment its rowspan. */
    cell_attr *active_rowspan_cells[50];  /* One per column */
    for (int i = 0; i < 50; i++) active_rowspan_cells[i] = NULL;

    /* Track the previous cell's matching status to check for colspan */
    cell_attr *prev_cell_matching = NULL;

    while (*read) {
        /* Ensure we have space (realloc if needed) */
        if (written + 100 > capacity) {
            capacity *= 2;
            char *new_output = realloc(output, capacity);
            if (!new_output) break;
            output = new_output;
            write = output + written;
        }
        /* Track table structure (BEFORE cell processing so indices are correct) */
        /* Also fix missing space in table tag (e.g., <tableid= -> <table id=) */
        if (strncmp(read, "<table", 6) == 0 && (read[6] == '>' || read[6] == ' ' || (read[6] == 'i' && strncmp(read + 6, "id=", 3) == 0) || isalnum((unsigned char)read[6]))) {
            in_table = true;
            table_idx++; /* New table */
            row_idx = -1; /* Reset for each new table */

            /* Check if this table has a caption (before fixing spacing so we can add figure tag) */
            table_caption *cap = NULL;
            for (table_caption *c = captions; c; c = c->next) {
                if (c->table_index == table_idx) {
                    cap = c;
                    break;
                }
            }

            /* Fix missing space before id or class attributes */
            if (read[6] == 'i' && strncmp(read + 6, "id=", 3) == 0) {
                /* If we have a caption and it should be above, write figure tag and caption first */
                if (cap && caption_position == 0) {
                    /* Write <figure><figcaption>caption</figcaption> */
                    const char *fig_open = "<figure class=\"table-figure\">\n<figcaption>";
                    size_t fig_open_len = strlen(fig_open);
                    const char *fig_close_cap = "</figcaption>\n";
                    size_t fig_close_cap_len = strlen(fig_close_cap);

                    /* Ensure we have space */
                    while (written + fig_open_len + strlen(cap->caption) * 6 + fig_close_cap_len + 100 > capacity) {
                        capacity *= 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) break;
                        output = new_output;
                        write = output + written;
                    }

                    memcpy(write, fig_open, fig_open_len);
                    write += fig_open_len;
                    written += fig_open_len;

                    /* Write caption text (escape HTML entities if needed) */
                    const char *cap_text = cap->caption;
                    while (*cap_text) {
                        if (*cap_text == '&') {
                            const char *amp = "&amp;";
                            memcpy(write, amp, 5);
                            write += 5;
                            written += 5;
                        } else if (*cap_text == '<') {
                            const char *lt = "&lt;";
                            memcpy(write, lt, 4);
                            write += 4;
                            written += 4;
                        } else if (*cap_text == '>') {
                            const char *gt = "&gt;";
                            memcpy(write, gt, 4);
                            write += 4;
                            written += 4;
                        } else if (*cap_text == '"') {
                            const char *quot = "&quot;";
                            memcpy(write, quot, 6);
                            write += 6;
                            written += 6;
                        } else {
                            *write++ = *cap_text;
                            written++;
                        }
                        cap_text++;
                    }

                    memcpy(write, fig_close_cap, fig_close_cap_len);
                    write += fig_close_cap_len;
                    written += fig_close_cap_len;
                } else if (cap && caption_position == 1) {
                    /* Caption below - just open figure tag */
                    const char *fig_open = "<figure class=\"table-figure\">\n";
                    size_t fig_open_len = strlen(fig_open);
                    while (written + fig_open_len + 100 > capacity) {
                        capacity *= 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) break;
                        output = new_output;
                        write = output + written;
                    }
                    memcpy(write, fig_open, fig_open_len);
                    write += fig_open_len;
                    written += fig_open_len;
                }

                /* Write "<table " then copy the rest of the tag */
                memcpy(write, read, 6);
                write += 6;
                *write++ = ' ';
                written += 7;
                read += 6;
                /* Copy the rest of the tag until closing > */
                while (*read && *read != '>') {
                    *write++ = *read++;
                    written++;
                }
                if (*read == '>') {
                    *write++ = *read++;
                    written++;
                }
                continue; /* Skip the normal copy below, we've handled it */
            }

            /* Normal table tag (no spacing fix needed) - check for caption */
            if (cap) {
                if (caption_position == 0) {
                    /* Caption above - write <figure><figcaption>caption</figcaption> */
                    const char *fig_open = "<figure class=\"table-figure\">\n<figcaption>";
                    size_t fig_open_len = strlen(fig_open);
                    const char *fig_close_cap = "</figcaption>\n";
                    size_t fig_close_cap_len = strlen(fig_close_cap);

                    /* Ensure we have space */
                    while (written + fig_open_len + strlen(cap->caption) * 6 + fig_close_cap_len + 100 > capacity) {
                        capacity *= 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) break;
                        output = new_output;
                        write = output + written;
                    }

                    memcpy(write, fig_open, fig_open_len);
                    write += fig_open_len;
                    written += fig_open_len;

                    /* Write caption text (escape HTML entities if needed) */
                    const char *cap_text = cap->caption;
                    while (*cap_text) {
                        if (*cap_text == '&') {
                            const char *amp = "&amp;";
                            memcpy(write, amp, 5);
                            write += 5;
                            written += 5;
                        } else if (*cap_text == '<') {
                            const char *lt = "&lt;";
                            memcpy(write, lt, 4);
                            write += 4;
                            written += 4;
                        } else if (*cap_text == '>') {
                            const char *gt = "&gt;";
                            memcpy(write, gt, 4);
                            write += 4;
                            written += 4;
                        } else if (*cap_text == '"') {
                            const char *quot = "&quot;";
                            memcpy(write, quot, 6);
                            write += 6;
                            written += 6;
                        } else {
                            *write++ = *cap_text;
                            written++;
                        }
                        cap_text++;
                    }

                    memcpy(write, fig_close_cap, fig_close_cap_len);
                    write += fig_close_cap_len;
                    written += fig_close_cap_len;
                } else {
                    /* Caption below - just open figure tag */
                    const char *fig_open = "<figure class=\"table-figure\">\n";
                    size_t fig_open_len = strlen(fig_open);
                    while (written + fig_open_len + 100 > capacity) {
                        capacity *= 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) break;
                        output = new_output;
                        write = output + written;
                    }
                    memcpy(write, fig_open, fig_open_len);
                    write += fig_open_len;
                    written += fig_open_len;
                }
            }
        } else if (strncmp(read, "</table>", 8) == 0) {
            /* Close tfoot or tbody if still open */
            if (in_tfoot) {
                const char *tfoot_close = "</tfoot>\n";
                size_t tfoot_close_len = strlen(tfoot_close);
                while (written + tfoot_close_len > capacity) {
                    capacity *= 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) break;
                    output = new_output;
                    write = output + written;
                }
                memcpy(write, tfoot_close, tfoot_close_len);
                write += tfoot_close_len;
                written += tfoot_close_len;
                in_tfoot = false;
            } else if (in_tbody) {
                const char *tbody_close = "</tbody>\n";
                size_t tbody_close_len = strlen(tbody_close);
                while (written + tbody_close_len > capacity) {
                    capacity *= 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) break;
                    output = new_output;
                    write = output + written;
                }
                memcpy(write, tbody_close, tbody_close_len);
                write += tbody_close_len;
                written += tbody_close_len;
                in_tbody = false;
            }

            /* Check if this table had a caption */
            table_caption *cap = NULL;
            for (table_caption *c = captions; c; c = c->next) {
                if (c->table_index == table_idx) {
                    cap = c;
                    break;
                }
            }

            if (cap) {
                /* Write </table> first */
                memcpy(write, read, 8);
                write += 8;
                read += 8;
                written += 8;

                if (caption_position == 1) {
                    /* Caption below - write <figcaption>caption</figcaption> before </figure> */
                    const char *fig_cap_open = "<figcaption>";
                    const char *fig_cap_close = "</figcaption>\n";
                    size_t fig_cap_open_len = strlen(fig_cap_open);
                    size_t fig_cap_close_len = strlen(fig_cap_close);

                    /* Ensure we have space */
                    while (written + fig_cap_open_len + strlen(cap->caption) * 6 + fig_cap_close_len + 100 > capacity) {
                        capacity *= 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) break;
                        output = new_output;
                        write = output + written;
                    }

                    memcpy(write, fig_cap_open, fig_cap_open_len);
                    write += fig_cap_open_len;
                    written += fig_cap_open_len;

                    /* Write caption text (escape HTML entities if needed) */
                    const char *cap_text = cap->caption;
                    while (*cap_text) {
                        if (*cap_text == '&') {
                            const char *amp = "&amp;";
                            memcpy(write, amp, 5);
                            write += 5;
                            written += 5;
                        } else if (*cap_text == '<') {
                            const char *lt = "&lt;";
                            memcpy(write, lt, 4);
                            write += 4;
                            written += 4;
                        } else if (*cap_text == '>') {
                            const char *gt = "&gt;";
                            memcpy(write, gt, 4);
                            write += 4;
                            written += 4;
                        } else if (*cap_text == '"') {
                            const char *quot = "&quot;";
                            memcpy(write, quot, 6);
                            write += 6;
                            written += 6;
                        } else {
                            *write++ = *cap_text;
                            written++;
                        }
                        cap_text++;
                    }

                    memcpy(write, fig_cap_close, fig_cap_close_len);
                    write += fig_cap_close_len;
                    written += fig_cap_close_len;
                }

                /* Close </figure> */
                const char *fig_close = "</figure>\n";
                size_t fig_close_len = strlen(fig_close);

                /* Ensure we have space */
                while (written + fig_close_len + 100 > capacity) {
                    capacity *= 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) break;
                    output = new_output;
                    write = output + written;
                }

                memcpy(write, fig_close, fig_close_len);
                write += fig_close_len;
                written += fig_close_len;

                in_table = false;
                continue; /* Skip the normal copy below */
            }

            in_table = false;
        } else if (in_table && strncmp(read, "<tbody>", 7) == 0) {
            /* We're entering tbody - mark it */
            in_tbody = true;
            in_tfoot = false;
        } else if (in_table && strncmp(read, "</tbody>", 8) == 0) {
            /* We're leaving tbody - but only if we haven't already closed it for tfoot */
            if (!in_tfoot) {
                in_tbody = false;
            } else {
                /* We're already in tfoot, so skip this </tbody> tag (we already closed it) */
                read += 8;
                continue;
            }
        } else if (in_table && strncmp(read, "<tr>", 4) == 0) {
            row_idx++;
            col_idx = 0;
            prev_cell_matching = NULL;  /* Reset previous cell matching for new row */

            /* Map HTML row index to AST row index.
             * HTML rows skip separator rows (which are marked for removal in AST).
             * So we need to find the AST row that corresponds to this HTML row. */
            ast_row_idx = -1;
            int html_row_count = -1;  /* Start at -1, will be 0 for header */
            for (int r = 0; r < 100; r++) {  /* Check up to 100 AST rows */
                /* Check if this AST row has any non-removed cells */
                bool has_non_removed = false;
                for (all_cell *c = all_cells; c; c = c->next) {
                    if (c->table_index == table_idx &&
                        c->row_index == r &&
                        !c->is_removed) {
                        has_non_removed = true;
                        break;
                    }
                }
                /* If this AST row has non-removed cells, it appears in HTML */
                if (has_non_removed) {
                    html_row_count++;
                    if (html_row_count == row_idx) {
                        ast_row_idx = r;
                        break;
                    }
                }
            }

            if (ast_row_idx == -1) {
                /* Fallback: use row_idx directly (shouldn't happen) */
                ast_row_idx = row_idx;
            }

            /* Pre-calculate which original columns will be visible in this row's HTML.
             * This creates a mapping: HTML position -> original column index.
             * IMPORTANT: Only include columns that render NEW <td> tags in this HTML row.
             * Columns occupied by rowspans from previous rows should NOT be included,
             * because they don't generate new <td> tags in this row. */
            row_col_mapping_size = 0;

            /* For each original column, check if it renders a NEW cell in this HTML row */
            for (int orig_col = 0; orig_col < 50 && row_col_mapping_size < 50; orig_col++) {
                bool renders_new_cell = false;

                /* Check if this column has a non-removed cell in current AST row */
                for (all_cell *c = all_cells; c; c = c->next) {
                    if (c->table_index == table_idx &&
                        c->row_index == ast_row_idx &&
                        c->col_index == orig_col &&
                        !c->is_removed) {
                        /* This column has a new cell in the current row */
                        renders_new_cell = true;
                        break;
                    }
                }

                /* Check if this column is covered by an active rowspan from a previous row.
                 * If a previous row has a cell with rowspan that's still active (spans to this row),
                 * then this column won't render a new <td> tag in this HTML row. */
                bool covered_by_rowspan = false;
                if (renders_new_cell && row_idx > 0) {
                    /* Check all previous HTML rows to see if any have a rowspan covering this column */
                    for (int prev_html_row = 0; prev_html_row < row_idx; prev_html_row++) {
                        /* Map previous HTML row to AST row */
                        int prev_ast_row = -1;
                        int prev_html_row_count = -1;
                        for (int r = 0; r < 100; r++) {
                            bool has_non_removed = false;
                            for (all_cell *c = all_cells; c; c = c->next) {
                                if (c->table_index == table_idx &&
                                    c->row_index == r &&
                                    !c->is_removed) {
                                    has_non_removed = true;
                                    break;
                                }
                            }
                            if (has_non_removed) {
                                prev_html_row_count++;
                                if (prev_html_row_count == prev_html_row) {
                                    prev_ast_row = r;
                                    break;
                                }
                            }
                        }
                        if (prev_ast_row >= 0) {
                            /* Check if this AST row has a cell at orig_col with rowspan that spans to current row */
                            for (cell_attr *a = attrs; a; a = a->next) {
                                if (a->table_index == table_idx &&
                                    a->row_index == prev_ast_row &&
                                    a->col_index == orig_col &&
                                    strstr(a->attributes, "rowspan=")) {
                                    int rowspan_val = 1;
                                    sscanf(strstr(a->attributes, "rowspan="), "rowspan=\"%d\"", &rowspan_val);
                                    /* Check if this rowspan spans to the current HTML row */
                                    int rows_spanned = row_idx - prev_html_row;
                                    if (rows_spanned < rowspan_val) {
                                        covered_by_rowspan = true;
                                        break;
                                    }
                                }
                            }
                            if (covered_by_rowspan) break;
                        }
                    }
                }

                /* Do NOT include columns occupied by rowspans from previous rows,
                 * because they don't generate new <td> tags in this HTML row */

                if (renders_new_cell && !covered_by_rowspan) {
                    row_col_mapping[row_col_mapping_size++] = orig_col;
                }
            }

            /* Store the mapping in a way we can access it during cell processing */
            /* We'll use a simple approach: store it in a static array keyed by table_idx and row_idx */
            /* Actually, let's use a simpler approach: process cells immediately using this mapping */

            /* Note: Captions immediately following tables (with no blank line) are not supported.
             * When a caption like [Caption] appears on the line immediately after a table, cmark-gfm
             * parses it as a table row rather than a paragraph, making it difficult to detect and extract
             * reliably. Captions work correctly when:
             * - They appear before the table (with or without blank line)
             * - They appear after the table with a blank line between (parsed as a paragraph)
             * To use a caption after a table, include a blank line between the table and caption.
             */

            /* Check if this row should be in tfoot.
             * tfoot_rows are stored with AST row indices, so we need to check ast_row_idx. */
            current_row_is_tfoot = false;
            for (tfoot_row *t = tfoot_rows; t; t = t->next) {
                if (t->table_index == table_idx && t->row_index == ast_row_idx) {
                    current_row_is_tfoot = true;
                    break;
                }
            }

            /* If we're in tbody and this is a tfoot row, close tbody and open tfoot */
            if (current_row_is_tfoot && in_tbody && !in_tfoot) {
                /* Close tbody */
                const char *tbody_close = "</tbody>\n";
                size_t tbody_close_len = strlen(tbody_close);
                while (written + tbody_close_len > capacity) {
                    capacity *= 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) break;
                    output = new_output;
                    write = output + written;
                }
                memcpy(write, tbody_close, tbody_close_len);
                write += tbody_close_len;
                written += tbody_close_len;
                in_tbody = false;

                /* Open tfoot */
                const char *tfoot_open = "<tfoot>\n";
                size_t tfoot_open_len = strlen(tfoot_open);
                while (written + tfoot_open_len > capacity) {
                    capacity *= 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) break;
                    output = new_output;
                    write = output + written;
                }
                memcpy(write, tfoot_open, tfoot_open_len);
                write += tfoot_open_len;
                written += tfoot_open_len;
                in_tfoot = true;
            }
            /* Note: Once we're in tfoot, we stay in tfoot - don't reopen tbody */
            /* tfoot rows should be at the end of the table */

            /* Check if this row should be completely removed (all cells marked for removal) */
            /* For tfoot rows that are pure === markers (all cells marked), skip the entire row */
            /* For tfoot rows with actual content, render them normally */
            bool should_skip_row = false;
            /* Count ALL cells in this row (using all_cells) to see if any are non-removed */
            int total_cells_in_row = 0;
            int removed_cells_in_row = 0;
            for (all_cell *c = all_cells; c; c = c->next) {
                if (c->table_index == table_idx && c->row_index == ast_row_idx) {
                    total_cells_in_row++;
                    if (c->is_removed) {
                        removed_cells_in_row++;
                    }
                }
            }
            /* If all cells in this row are marked for removal, skip the entire row */
            /* This applies to both regular rows and tfoot rows that are pure === markers */
            if (total_cells_in_row > 0 && total_cells_in_row == removed_cells_in_row) {
                should_skip_row = true;
            }
            /* Also check if this row contains === markers by checking the AST row directly.
             * This handles cases where the row mapping might have issues. */
            if (!should_skip_row && current_row_is_tfoot) {
                /* Check if any cells in current AST row contain === */
                bool has_equals = false;
                for (cell_attr *a = attrs; a; a = a->next) {
                    if (a->table_index == table_idx && a->row_index == ast_row_idx &&
                        strstr(a->attributes, "data-remove")) {
                        if (a->cell_text) {
                            const char *text = a->cell_text;
                            while (*text && isspace((unsigned char)*text)) text++;
                            if (text[0] == '=' && text[1] == '=' && text[2] == '=') {
                                has_equals = true;
                                break;
                            }
                        }
                    }
                }
                /* If this row contains === markers, skip it */
                if (has_equals) {
                    should_skip_row = true;
                } else {
                    /* Also check previous AST row in case row mapping is off */
                    if (ast_row_idx > 0) {
                        int prev_ast_row = ast_row_idx - 1;
                        for (cell_attr *a = attrs; a; a = a->next) {
                            if (a->table_index == table_idx && a->row_index == prev_ast_row &&
                                strstr(a->attributes, "data-remove") && a->cell_text) {
                                const char *text = a->cell_text;
                                while (*text && isspace((unsigned char)*text)) text++;
                                if (text[0] == '=' && text[1] == '=' && text[2] == '=') {
                                    has_equals = true;
                                    /* If previous row has === and this is first tfoot row, this HTML row is the === row */
                                    if (row_idx <= 3) {
                                        should_skip_row = true;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            if (should_skip_row) {
                /* Skip the opening <tr> tag and everything until </tr> */
                read += 4;
                const char *tr_end = strstr(read, "</tr>");
                if (tr_end) {
                    read = tr_end + 5;
                } else {
                    /* Fallback: skip to next tag */
                    while (*read && strncmp(read, "</tr>", 5) != 0) read++;
                    if (*read) read += 5;
                }
                continue; /* Don't set in_row, skip to next iteration (won't copy <tr>) */
            }
            /* Otherwise, this is a normal row (or tfoot row) - copy <tr> and process cells */
            in_row = true;
        } else if (in_row && strncmp(read, "</tr>", 5) == 0) {
            in_row = false;
        } else if (strncmp(read, "<p>", 3) == 0) {
            /* Check if this paragraph should be removed */
            para_idx++;
            para_to_remove *para_remove = NULL;
            for (para_to_remove *p = paras_to_remove; p; p = p->next) {
                if (p->para_index == para_idx) {
                    para_remove = p;
                    break;
                }
            }

            /* Extract paragraph text to check */
            const char *para_start = read + 3;
            const char *para_end = strstr(para_start, "</p>");
            if (para_end) {
                /* Check if paragraph starts with [ (caption format) */
                const char *text_start = para_start;
                /* Skip any leading whitespace */
                while (*text_start && text_start < para_end && isspace((unsigned char)*text_start)) text_start++;

                if (*text_start == '[' ||
                    (text_start < para_end - 4 && strncmp(text_start, "&lt;", 4) == 0)) {
                    /* This looks like a caption paragraph */

                    /* First check: if we have a fingerprint match from AST */
                    if (para_remove && para_remove->text_fingerprint) {
                        const char *fingerprint_text = para_remove->text_fingerprint;
                        if (fingerprint_text && strlen(fingerprint_text) > 0) {
                            /* Simple substring match - check if fingerprint appears anywhere in paragraph content */
                            if (strstr(para_start, fingerprint_text) != NULL) {
                                /* Skip this entire paragraph */
                                read = para_end + 4; /* Skip past </p> */
                                continue; /* Skip normal copy */
                            }
                        }
                    }

                    /* Second check: if we're near a table/figure that has a caption, remove this paragraph */
                    /* This is a fallback for cases where fingerprint matching fails */
                    /* Check if any table has a caption, and if this paragraph's caption text matches */
                    /* Extract caption text from paragraph (just the text between brackets) */
                    const char *bracket_start = text_start;
                    if (*bracket_start == '[') bracket_start++;
                    else if (text_start < para_end - 4 && strncmp(bracket_start, "&lt;", 4) == 0) bracket_start += 4;
                    const char *bracket_end = strchr(bracket_start, ']');
                    if (!bracket_end || bracket_end >= para_end) {
                        /* Try HTML entity version */
                        const char *gt_entity = strstr(bracket_start, "&gt;");
                        if (gt_entity && gt_entity < para_end) bracket_end = gt_entity;
                    }
                    if (bracket_end && bracket_end < para_end) {
                        size_t caption_len = bracket_end - bracket_start;
                        if (caption_len > 0 && caption_len < 512) {
                            char para_caption[512];
                            memcpy(para_caption, bracket_start, caption_len);
                            para_caption[caption_len] = '\0';
                            /* Compare with all table captions - if any match, remove this paragraph */
                            for (table_caption *cap = captions; cap; cap = cap->next) {
                                if (cap->caption && strcmp(para_caption, cap->caption) == 0) {
                                    /* Match! Remove this paragraph */
                                    read = para_end + 4;
                                    continue;
                                }
                            }
                        }
                    }
                }
            }
        }

        /* Check for cell opening tags */
        if (in_row && (strncmp(read, "<td", 3) == 0 || strncmp(read, "<th", 3) == 0)) {
            /* Extract cell content for debugging */
            char cell_preview[100] = {0};
            const char *content_start = strchr(read, '>');
            if (content_start) {
                const char *content_end = strstr(content_start + 1, "</td>");
                if (!content_end) content_end = strstr(content_start + 1, "</th>");
                if (content_end && content_end - content_start - 1 < 99) {
                    strncpy(cell_preview, content_start + 1, content_end - content_start - 1);
                    cell_preview[content_end - content_start - 1] = '\0';
                }
            }

            /* Use pre-calculated mapping: HTML position -> original column index */
            int target_original_col = -1;
            if (col_idx < row_col_mapping_size) {
                target_original_col = row_col_mapping[col_idx];
            }

            /* Find matching attribute using the mapped original column index and AST row index.
             * Also verify that the cell content matches to avoid matching cells that are covered
             * by rowspans from previous rows.
             *
             * If no match found in current row, also check the previous AST row in case of row
             * detection issues (e.g., when a cell with rowspan is processed in the wrong HTML row).
             *
             * For tfoot rows with === markers, also check the previous AST row since the row mapping
             * might skip the === row if all its cells are marked for removal. */
            cell_attr *matching = NULL;
            if (target_original_col >= 0) {
                /* First, try to match in the current AST row */
                for (cell_attr *a = attrs; a; a = a->next) {
                    if (a->table_index == table_idx &&
                        a->row_index == ast_row_idx &&
                        a->col_index == target_original_col) {
                        /* Verify content matches to avoid matching cells covered by rowspans from previous rows */
                        bool content_matches = true;
                        if (a->cell_text && cell_preview[0] != '\0') {
                            /* Compare cell content - trim whitespace for comparison */
                            const char *attr_text = a->cell_text;
                            const char *html_text = cell_preview;
                            /* Skip leading whitespace */
                            while (*attr_text && isspace((unsigned char)*attr_text)) attr_text++;
                            while (*html_text && isspace((unsigned char)*html_text)) html_text++;
                            /* Compare (case-sensitive, but we can make it more lenient if needed) */
                            content_matches = (strncmp(attr_text, html_text, strlen(attr_text)) == 0 &&
                                             (html_text[strlen(attr_text)] == '\0' || isspace((unsigned char)html_text[strlen(attr_text)])));
                        }
                        if (content_matches) {
                            matching = a;
                            break;
                        }
                    }
                }

                /* If no match found in current row, also check the previous AST row.
                 * This is especially important for tfoot rows with === markers, where the row mapping
                 * might skip the === row if all its cells are marked for removal. */
                if (!matching && ast_row_idx > 0) {
                    for (cell_attr *a = attrs; a; a = a->next) {
                        if (a->table_index == table_idx &&
                            a->row_index == ast_row_idx - 1 &&
                            a->col_index == target_original_col &&
                            strstr(a->attributes, "data-remove")) {
                            /* Check if content matches (for === cells) */
                            bool content_matches = true;
                            if (a->cell_text && cell_preview[0] != '\0') {
                                const char *attr_text = a->cell_text;
                                const char *html_text = cell_preview;
                                while (*attr_text && isspace((unsigned char)*attr_text)) attr_text++;
                                while (*html_text && isspace((unsigned char)*html_text)) html_text++;
                                content_matches = (strncmp(attr_text, html_text, strlen(attr_text)) == 0 &&
                                                 (html_text[strlen(attr_text)] == '\0' || isspace((unsigned char)html_text[strlen(attr_text)])));
                            }
                            if (content_matches) {
                                matching = a;
                                break;
                            }
                        }
                    }
                }

                /* If no match found in current row, try the previous AST row as a fallback.
                 * This handles cases where row detection is off by one. We try matching by
                 * column position in the previous row. This is safe because:
                 * 1. We only do this if there's no match in the current row
                 * 2. We match by column position, not content (works for all tables)
                 * 3. This is a fallback for row detection issues, not the primary matching method
                 *
                 * IMPORTANT: Don't match empty cells - they should be removed, not matched to previous row cells. */
                if (!matching && row_idx > 0 && target_original_col >= 0 && cell_preview[0] != '\0') {
                    int prev_html_row = row_idx - 1;
                    int prev_ast_row = -1;
                    int prev_html_row_count = -1;
                    for (int r = 0; r < 100; r++) {
                        bool has_non_removed = false;
                        for (all_cell *c = all_cells; c; c = c->next) {
                            if (c->table_index == table_idx &&
                                c->row_index == r &&
                                !c->is_removed) {
                                has_non_removed = true;
                                break;
                            }
                        }
                        if (has_non_removed) {
                            prev_html_row_count++;
                            if (prev_html_row_count == prev_html_row) {
                                prev_ast_row = r;
                                break;
                            }
                        }
                    }
                    if (prev_ast_row >= 0) {
                        /* Try to match in previous AST row. Check both target_original_col and target_original_col-1
                         * because column mapping might be off by one due to rowspan issues.
                         * IMPORTANT: Don't match cells marked for removal - they shouldn't be in the previous row. */
                        for (cell_attr *a = attrs; a; a = a->next) {
                            if (a->table_index == table_idx &&
                                a->row_index == prev_ast_row &&
                                (a->col_index == target_original_col ||
                                 (target_original_col > 0 && a->col_index == target_original_col - 1)) &&
                                !strstr(a->attributes, "data-remove")) {  /* Don't match removed cells */
                                matching = a;
                                break;
                            }
                        }
                    }
                }
            }

            /* Also check if this cell contains "^^" (rowspan marker) - these should be removed */
            /* Check both the preview and the actual content in the HTML */
            bool is_rowspan_marker = (strstr(cell_preview, "^^") != NULL);
            if (!is_rowspan_marker) {
                /* Also check the actual HTML content for "^^" */
                const char *content_check = strstr(read, ">");
                if (content_check) {
                    const char *close_check = strstr(content_check + 1, "</td>");
                    if (!close_check) close_check = strstr(content_check + 1, "</th>");
                    if (close_check && close_check - content_check - 1 < 100) {
                        char check_buf[100];
                        strncpy(check_buf, content_check + 1, close_check - content_check - 1);
                        check_buf[close_check - content_check - 1] = '\0';
                        is_rowspan_marker = (strstr(check_buf, "^^") != NULL);
                    }
                }
            }

            /* Check if this cell should be removed:
             * 1. If it's matched and marked for removal
             * 2. If it contains "^^" (rowspan marker)
             * 3. If it's empty and the previous cell in the same row has colspan (empty cells after colspan should be removed) */
            bool should_remove_cell = false;
            if (matching && strstr(matching->attributes, "data-remove")) {
                should_remove_cell = true;
            } else if (is_rowspan_marker) {
                should_remove_cell = true;
            } else if (cell_preview[0] == '\0' && prev_cell_matching) {
                /* Check if previous cell has colspan.
                 * If the previous cell has colspan > 1, and this empty cell is
                 * at the next column position, it should be removed (it's the cell that was merged). */
                if (strstr(prev_cell_matching->attributes, "colspan")) {
                    int colspan_val = 1;
                    if (strstr(prev_cell_matching->attributes, "colspan=")) {
                        sscanf(strstr(prev_cell_matching->attributes, "colspan="), "colspan=\"%d\"", &colspan_val);
                    }
                    /* If the previous cell has colspan > 1, and this empty cell is
                     * at the next column position, it should be removed */
                    if (colspan_val > 1) {
                        /* Check if this cell is at the next column after the previous cell */
                        int prev_col = prev_cell_matching->col_index;
                        if (target_original_col == prev_col + 1 || target_original_col == prev_col) {
                            should_remove_cell = true;
                        }
                    }
                }
            }

            if (should_remove_cell) {
                /* Skip this entire cell (including for tfoot rows - === rows are skipped entirely) */
                /* Note: Removed cells are not rendered by the HTML renderer, so we shouldn't see them here.
                 * But if we do (e.g., from cmark-gfm before our processing), skip them.
                 * Also remove cells containing "^^" (rowspan markers) even if they're not matched.
                 * We still increment col_idx to match the column index used when collecting attributes
                 * (which counts all cells including removed ones). */
                bool is_th = strncmp(read, "<th", 3) == 0;
                const char *close_tag = is_th ? "</th>" : "</td>";

                /* Skip opening tag */
                while (*read && *read != '>') read++;
                if (*read == '>') read++;

                /* Skip content until closing tag */
                while (*read && strncmp(read, close_tag, 5) != 0) read++;
                if (strncmp(read, close_tag, 5) == 0) read += 5;

                col_idx++;  /* Increment to match column index from collection (counts all cells) */
                prev_cell_matching = NULL;  /* Reset previous cell matching for removed cells */
                continue;
            } else if (matching && (strstr(matching->attributes, "rowspan") || strstr(matching->attributes, "colspan"))) {
                /* Copy opening tag */
                while (*read && *read != '>') {
                    *write++ = *read++;
                }
                /* Inject attributes before > */
                const char *attr_str = matching->attributes;
                while (*attr_str) {
                    *write++ = *attr_str++;
                }
                /* Copy the > */
                if (*read == '>') {
                    *write++ = *read++;
                }
                col_idx++;
                prev_cell_matching = matching;  /* Track this cell for next cell's colspan check */
                continue;
            }

            col_idx++;
            prev_cell_matching = matching;  /* Track this cell for next cell's colspan check */
        }

        /* Copy character */
        *write++ = *read++;
        written++;
    }

    *write = '\0';

    /* Clean up all_cells list */
    while (all_cells) {
        all_cell *next = all_cells->next;
        free(all_cells);
        all_cells = next;
    }

    /* Clean up attributes list */
    while (attrs) {
        cell_attr *next = attrs->next;
        free(attrs->attributes);
        if (attrs->cell_text) free(attrs->cell_text);
        free(attrs);
        attrs = next;
    }

    /* Clean up captions list */
    while (captions) {
        table_caption *next = captions->next;
        free(captions->caption);
        free(captions);
        captions = next;
    }

    /* Clean up paragraphs to remove list */
    while (paras_to_remove) {
        para_to_remove *next = paras_to_remove->next;
        free(paras_to_remove->text_fingerprint);
        free(paras_to_remove);
        paras_to_remove = next;
    }

    return output;
}

