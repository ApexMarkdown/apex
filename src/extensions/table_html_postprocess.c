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

/* Structure to track cells with attributes */
typedef struct cell_attr {
    int table_index;   /* Which table (0, 1, 2, ...) */
    int row_index;
    int col_index;
    char *attributes;  /* e.g. " rowspan=\"2\"" or " data-remove=\"true\"" */
    struct cell_attr *next;
} cell_attr;

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
 * Inject attributes into HTML or remove cells
 */
char *apex_inject_table_attributes(const char *html, cmark_node *document) {
    if (!html || !document) return (char *)html;

    /* Collect all cells with attributes */
    cell_attr *attrs = collect_table_cell_attributes(document);
    if (!attrs) return (char *)html; /* Nothing to do */

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
        return (char *)html;
    }

    const char *read = html;
    char *write = output;
    size_t written = 0;
    int table_idx = -1; /* Track which table we're in */
    int row_idx = -1;
    int col_idx = 0;
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
        if (strncmp(read, "<table>", 7) == 0) {
            in_table = true;
            table_idx++; /* New table */
            row_idx = -1; /* Reset for each new table */
        } else if (strncmp(read, "</table>", 8) == 0) {
            in_table = false;
        } else if (in_table && strncmp(read, "<tr>", 4) == 0) {
            in_row = true;
            row_idx++;
            col_idx = 0;
        } else if (in_row && strncmp(read, "</tr>", 5) == 0) {
            in_row = false;
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

    return output;
}

