/**
 * Table of Contents Extension for Apex
 * Implementation
 */

#include "toc.h"
#include "header_ids.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/**
 * Collect headers from AST
 */
typedef struct header_item {
    int level;
    char *text;
    char *id;
    struct header_item *next;
} header_item;

static void free_headers(header_item *headers) {
    while (headers) {
        header_item *next = headers->next;
        free(headers->text);
        free(headers->id);
        free(headers);
        headers = next;
    }
}



/**
 * Collect all headers from document
 */
static header_item *collect_headers(cmark_node *node, header_item **tail) {
    if (!node) return NULL;

    header_item *headers = NULL;
    if (!tail) tail = &headers;

    /* Check if current node is a header */
    if (cmark_node_get_type(node) == CMARK_NODE_HEADING) {
        /* Skip headings marked with the Kramdown-style ".no_toc" class.
         * The IAL processor stores attributes as a raw HTML attribute
         * string in the node's user_data, e.g. id="..." class="a b no_toc".
         * If we see "no_toc" in the attribute string, we exclude this
         * heading from the generated table of contents.
         */
        const char *attrs = (const char *)cmark_node_get_user_data(node);
        if (!(attrs && strstr(attrs, "no_toc") != NULL)) {
            header_item *item = malloc(sizeof(header_item));
            if (item) {
                item->level = cmark_node_get_heading_level(node);
                item->text = apex_extract_heading_text(node);
                item->id = NULL;  /* Will be set later with format */
                item->next = NULL;

                if (*tail) {
                    (*tail)->next = item;
                } else {
                    headers = item;
                }
                *tail = item;
            }
        }
    }

    /* Recursively process children */
    for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
        header_item *child_headers = collect_headers(child, tail);
        if (!headers) headers = child_headers;
    }

    return headers;
}

/**
 * Generate TOC HTML from headers
 * Produces valid ul > li > ul nesting (nested lists inside list items)
 */
static char *generate_toc_html(header_item *headers, int min_level, int max_level) {
    if (!headers) return strdup("");

    size_t capacity = 4096;
    char *html = malloc(capacity);
    if (!html) return strdup("");

    char *write = html;
    size_t remaining = capacity;
    int current_level = 0;
    int last_level = 0;  /* level of last item added (0 = none yet) */

    #define APPEND(str) do { \
        size_t len = strlen(str); \
        if (len < remaining) { \
            memcpy(write, str, len); \
            write += len; \
            remaining -= len; \
        } \
    } while(0)

    APPEND("<nav class=\"toc\">\n");

    for (header_item *h = headers; h; h = h->next) {
        /* Skip headers outside min/max range */
        if (h->level < min_level || h->level > max_level) continue;

        /* Going up: close </ul></li> for each level (nested ul inside parent li) */
        while (current_level > h->level) {
            APPEND("</ul>\n</li>\n");
            current_level--;
        }

        /* Going down: open one <ul> inside the previous li before adding child.
         * At root (current_level 0): only open one ul - never ul > ul.
         * When going deeper: open exactly one ul per step - never ul > ul. */
        while (current_level < h->level) {
            if (current_level == 0) {
                APPEND("<ul>\n");
                current_level = 1;
                break;
            }
            if (last_level > 0) {
                APPEND("<ul>\n");
                current_level++;
                break;
            } else {
                break;  /* No parent li - add as direct child of root ul */
            }
        }

        /* Close previous li when adding sibling (same or shallower level) */
        if (last_level > 0 && h->level <= last_level) {
            APPEND("</li>\n");
        }

        /* Add list item (leave open - may contain nested ul) */
        char item[1024];
        snprintf(item, sizeof(item), "<li><a href=\"#%s\">%s</a>",
                 h->id, h->text);
        APPEND(item);
        last_level = h->level;
    }

    /* Close remaining: </li></ul> for each open level */
    while (current_level > 0) {
        APPEND("</li>\n</ul>\n");
        current_level--;
    }

    APPEND("</nav>\n");

    #undef APPEND

    *write = '\0';
    return html;
}

/**
 * Parse TOC marker for min/max levels
 */
static void parse_toc_marker(const char *marker, int *min_level, int *max_level) {
    *min_level = 1;
    *max_level = 6;

    /* Look for max and min parameters */
    const char *max_str = strstr(marker, "max");
    const char *min_str = strstr(marker, "min");

    if (max_str) {
        max_str += 3;
        while (*max_str && !isdigit((unsigned char)*max_str)) max_str++;
        if (*max_str) *max_level = atoi(max_str);
    }

    if (min_str) {
        min_str += 3;
        while (*min_str && !isdigit((unsigned char)*min_str)) min_str++;
        if (*min_str) *min_level = atoi(min_str);
    }

    /* Check for Pandoc style {{TOC:2-5}} */
    const char *colon = strchr(marker, ':');
    if (colon) {
        colon++;
        while (*colon && isspace((unsigned char)*colon)) colon++;
        if (isdigit((unsigned char)*colon)) {
            *min_level = atoi(colon);
            const char *dash = strchr(colon, '-');
            if (dash) {
                dash++;
                if (isdigit((unsigned char)*dash)) {
                    *max_level = atoi(dash);
                }
            }
        }
    }
}

/**
 * Process TOC markers in HTML
 */
char *apex_process_toc(const char *html, cmark_node *document, int id_format) {
    if (!html || !document) return html ? strdup(html) : NULL;

    /* Check if there are any TOC markers */
    const char *toc_marker = strstr(html, "<!--TOC");
    const char *toc_mmd = strstr(html, "{{TOC");

    if (!toc_marker && !toc_mmd) {
        return strdup(html);  /* No TOC markers, return as-is */
    }

    /* Collect headers from document */
    header_item *tail = NULL;
    header_item *headers = collect_headers(document, &tail);
    if (!headers) return strdup(html);

    /* Generate IDs for all headers using the specified format */
    for (header_item *h = headers; h; h = h->next) {
        h->id = apex_generate_header_id(h->text, (apex_id_format_t)id_format);
    }

    /* Find the marker and parse it */
    const char *marker = toc_marker ? toc_marker : toc_mmd;
    int min_level, max_level;
    parse_toc_marker(marker, &min_level, &max_level);

    /* Generate TOC HTML */
    char *toc_html = generate_toc_html(headers, min_level, max_level);
    free_headers(headers);

    if (!toc_html) return strdup(html);

    /* Replace marker with TOC */
    size_t html_len = strlen(html);
    size_t toc_len = strlen(toc_html);
    size_t output_capacity = html_len + toc_len + 100;
    char *output = malloc(output_capacity);
    if (!output) {
        free(toc_html);
        return strdup(html);
    }

    /* Find end of marker */
    const char *marker_end = NULL;
    if (toc_marker) {
        marker_end = strstr(marker, "-->");
        if (marker_end) marker_end += 3;
    } else if (toc_mmd) {
        marker_end = strstr(marker, "}}");
        if (marker_end) marker_end += 2;
    }

    if (!marker_end) {
        free(toc_html);
        free(output);
        return strdup(html);
    }

    /* Build output: before + TOC + after */
    size_t before_len = marker - html;
    size_t after_len = strlen(marker_end);

    memcpy(output, html, before_len);
    memcpy(output + before_len, toc_html, toc_len);
    memcpy(output + before_len + toc_len, marker_end, after_len + 1);

    free(toc_html);
    return output;
}

