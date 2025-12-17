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
    struct cell_attr *next;
} cell_attr;

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
            } else if (type == CMARK_NODE_TABLE_CELL) {
                char *attrs = (char *)cmark_node_get_user_data(node);
                if (attrs) {
                    /* Store this cell's attributes */
                    cell_attr *attr = malloc(sizeof(cell_attr));
                    if (attr) {
                        attr->table_index = table_index;
                        attr->row_index = row_index;
                        attr->col_index = col_index;
                        attr->attributes = strdup(attrs);
                        attr->next = list;
                        list = attr;
                    }
                }
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
                if (user_data && strstr(user_data, "data-caption=")) {
                    /* Extract caption text */
                    char caption[512];
                    if (sscanf(user_data, " data-caption=\"%[^\"]\"", caption) == 1) {
                        table_caption *cap = malloc(sizeof(table_caption));
                        if (cap) {
                            cap->table_index = table_index;
                            cap->caption = strdup(caption);
                            cap->next = list;
                            list = cap;
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
char *apex_inject_table_attributes(const char *html, cmark_node *document) {
    if (!html || !document) return (char *)html;

    /* Collect all cells with attributes */
    cell_attr *attrs = collect_table_cell_attributes(document);

    /* Collect all table captions and paragraphs to remove */
    para_to_remove *paras_to_remove = NULL;
    table_caption *captions = collect_table_captions(document, &paras_to_remove);

    /* If nothing to do, return original */
    if (!attrs && !captions && !paras_to_remove) return (char *)html;

    /* Allocate output buffer (same size as input, we'll realloc if needed) */
    size_t capacity = strlen(html) * 2;
    char *output = malloc(capacity);
    if (!output) {
        /* Clean up and return original */
        while (attrs) {
            cell_attr *next = attrs->next;
            free(attrs->attributes);
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
    int col_idx = 0;
    int para_idx = -1;  /* Track paragraph index */
    bool in_table = false;
    bool in_row = false;

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
        if (strncmp(read, "<table", 6) == 0 && (read[6] == '>' || read[6] == ' ' || read[6] == 'i' || isalnum((unsigned char)read[6]))) {
            /* Fix missing space before id or class attributes */
            if (read[6] == 'i' && strncmp(read + 6, "id=", 3) == 0) {
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
            in_table = true;
            table_idx++; /* New table */
            row_idx = -1; /* Reset for each new table */

            /* Check if this table has a caption */
            table_caption *cap = NULL;
            for (table_caption *c = captions; c; c = c->next) {
                if (c->table_index == table_idx) {
                    cap = c;
                    break;
                }
            }

            if (cap) {
                /* Wrap table in <figure> and add <figcaption> */
                const char *fig_open = "<figure class=\"table-figure\">\n<figcaption>";
                size_t fig_open_len = strlen(fig_open);
                const char *fig_close_cap = "</figcaption>\n";
                size_t fig_close_cap_len = strlen(fig_close_cap);

                /* Ensure we have space */
                while (written + fig_open_len + strlen(cap->caption) + fig_close_cap_len + 100 > capacity) {
                    capacity *= 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) break;
                    output = new_output;
                    write = output + written;
                }

                /* Write <figure><figcaption>caption</figcaption> */
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
            }
        } else if (strncmp(read, "</table>", 8) == 0) {
            /* Check if this table had a caption */
            table_caption *cap = NULL;
            for (table_caption *c = captions; c; c = c->next) {
                if (c->table_index == table_idx) {
                    cap = c;
                    break;
                }
            }

            if (cap) {
                /* Close </figure> after table */
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

                /* Write </table> first */
                memcpy(write, read, 8);
                write += 8;
                read += 8;
                written += 8;

                /* Then write </figure> */
                memcpy(write, fig_close, fig_close_len);
                write += fig_close_len;
                written += fig_close_len;

                in_table = false;
                continue; /* Skip the normal copy below */
            }

            in_table = false;
        } else if (in_table && strncmp(read, "<tr>", 4) == 0) {
            row_idx++;
            col_idx = 0;
            /* Check if this row should be completely removed (all cells marked for removal) */
            /* Count cells in this row that are marked for removal */
            int cells_in_row = 0;
            int cells_to_remove = 0;
            for (cell_attr *a = attrs; a; a = a->next) {
                if (a->table_index == table_idx && a->row_index == row_idx) {
                    cells_in_row++;
                    if (strstr(a->attributes, "data-remove")) {
                        cells_to_remove++;
                    }
                }
            }
            /* If all cells in this row are marked for removal, skip the entire row */
            if (cells_in_row > 0 && cells_in_row == cells_to_remove) {
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
            /* Otherwise, this is a normal row - copy <tr> and process cells */
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

            if (para_remove && para_remove->text_fingerprint) {
                /* Extract paragraph text to match */
                const char *para_start = read + 3;
                const char *para_end = strstr(para_start, "</p>");
                if (para_end) {
                    /* Check if paragraph starts with [ (caption format) */
                    const char *text_start = para_start;
                    /* Skip any leading whitespace */
                    while (*text_start && isspace((unsigned char)*text_start)) text_start++;

                    if (*text_start == '[' ||
                        (*text_start == '&' && strncmp(text_start, "&lt;", 4) == 0)) {
                        /* This looks like a caption paragraph - check if fingerprint matches */
                        size_t para_len = para_end - para_start;
                        size_t fingerprint_len = strlen(para_remove->text_fingerprint);

                        /* Simple match: check if fingerprint appears in paragraph */
                        /* We check the first part of the paragraph text */
                        size_t check_len = para_len < fingerprint_len ? para_len : fingerprint_len;
                        if (check_len > 0) {
                            /* Compare, handling potential HTML entities */
                            bool matches = true;
                            const char *para_check = para_start;
                            const char *fingerprint_check = para_remove->text_fingerprint;
                            size_t checked = 0;

                            while (checked < check_len && *para_check && *fingerprint_check) {
                                if (*para_check == '&') {
                                    /* Skip HTML entities - just advance para_check */
                                    if (strncmp(para_check, "&lt;", 4) == 0) {
                                        if (*fingerprint_check == '<') {
                                            para_check += 4;
                                            fingerprint_check++;
                                            checked++;
                                            continue;
                                        }
                                    } else if (strncmp(para_check, "&gt;", 4) == 0) {
                                        if (*fingerprint_check == '>') {
                                            para_check += 4;
                                            fingerprint_check++;
                                            checked++;
                                            continue;
                                        }
                                    } else if (strncmp(para_check, "&amp;", 5) == 0) {
                                        if (*fingerprint_check == '&') {
                                            para_check += 5;
                                            fingerprint_check++;
                                            checked++;
                                            continue;
                                        }
                                    }
                                    matches = false;
                                    break;
                                } else if (*para_check == *fingerprint_check) {
                                    para_check++;
                                    fingerprint_check++;
                                    checked++;
                                } else if (isspace((unsigned char)*para_check) && isspace((unsigned char)*fingerprint_check)) {
                                    /* Both are whitespace, skip both */
                                    while (isspace((unsigned char)*para_check)) para_check++;
                                    while (isspace((unsigned char)*fingerprint_check)) fingerprint_check++;
                                    checked++;
                                } else {
                                    matches = false;
                                    break;
                                }
                            }

                            if (matches && checked >= fingerprint_len / 2) {
                                /* Skip this entire paragraph */
                                read = para_end + 4; /* Skip past </p> */
                                continue; /* Skip normal copy */
                            }
                        }
                    }
                }
            }
        }

        /* Check for cell opening tags */
        if (in_row && (strncmp(read, "<td", 3) == 0 || strncmp(read, "<th", 3) == 0)) {
            /* Find matching attribute (match table_idx, row_idx, AND col_idx) */
            cell_attr *matching = NULL;
            for (cell_attr *a = attrs; a; a = a->next) {
                if (a->table_index == table_idx && a->row_index == row_idx && a->col_index == col_idx) {
                    matching = a;
                    break;
                }
            }

            if (matching && strstr(matching->attributes, "data-remove")) {
                /* Skip this entire cell */
                bool is_th = strncmp(read, "<th", 3) == 0;
                const char *close_tag = is_th ? "</th>" : "</td>";

                /* Skip opening tag */
                while (*read && *read != '>') read++;
                if (*read == '>') read++;

                /* Skip content until closing tag */
                while (*read && strncmp(read, close_tag, 5) != 0) read++;
                if (strncmp(read, close_tag, 5) == 0) read += 5;

                col_idx++;
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
                continue;
            }

            col_idx++;
        }

        /* Copy character */
        *write++ = *read++;
        written++;
    }

    *write = '\0';

    /* Clean up attributes list */
    while (attrs) {
        cell_attr *next = attrs->next;
        free(attrs->attributes);
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

