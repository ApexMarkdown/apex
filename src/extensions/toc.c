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
 * Return true if position 'pos' in 'html' is inside a <code> or <pre> element.
 * Used to skip TOC markers that appear in code blocks or inline code.
 */
static int is_inside_code_or_pre(const char *html, size_t pos) {
    int in_code = 0;
    int in_pre = 0;
    size_t i = 0;
    size_t len = pos;

    while (i < len) {
        if (html[i] == '<') {
            if (i + 5 <= len && (html[i+1] == 'c' || html[i+1] == 'C') &&
                (html[i+2] == 'o' || html[i+2] == 'O') &&
                (html[i+3] == 'd' || html[i+3] == 'D') &&
                (html[i+4] == 'e' || html[i+4] == 'E')) {
                char next = (i + 5 < len) ? html[i + 5] : '\0';
                if (next == '>' || next == ' ' || next == '\t' || next == '\n') {
                    in_code++;
                    i += 5;
                    continue;
                }
            }
            if (i + 4 <= len && (html[i+1] == 'p' || html[i+1] == 'P') &&
                (html[i+2] == 'r' || html[i+2] == 'R') &&
                (html[i+3] == 'e' || html[i+3] == 'E')) {
                char next = (i + 4 < len) ? html[i + 4] : '\0';
                if (next == '>' || next == ' ' || next == '\t' || next == '\n') {
                    in_pre++;
                    i += 4;
                    continue;
                }
            }
            if (i + 7 <= len && html[i+1] == '/' &&
                (html[i+2] == 'c' || html[i+2] == 'C') &&
                (html[i+3] == 'o' || html[i+3] == 'O') &&
                (html[i+4] == 'd' || html[i+4] == 'D') &&
                (html[i+5] == 'e' || html[i+5] == 'E') && html[i+6] == '>') {
                if (in_code > 0) in_code--;
                i += 7;
                continue;
            }
            if (i + 6 <= len && html[i+1] == '/' &&
                (html[i+2] == 'p' || html[i+2] == 'P') &&
                (html[i+3] == 'r' || html[i+3] == 'R') &&
                (html[i+4] == 'e' || html[i+4] == 'E') && html[i+5] == '>') {
                if (in_pre > 0) in_pre--;
                i += 6;
                continue;
            }
        }
        i++;
    }
    return (in_code > 0 || in_pre > 0);
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
 * Find the first TOC marker (<!--TOC or {{TOC) that is not inside <code> or <pre>.
 * Returns pointer to the marker, or NULL if none valid. *is_html_comment is set
 * to 1 for <!--TOC, 0 for {{TOC.
 */
static const char *find_toc_marker_not_in_code(const char *html, int *is_html_comment) {
    const char *p = html;
    *is_html_comment = 0;

    while (1) {
        const char *next_comment = strstr(p, "<!--TOC");
        const char *next_mmd = strstr(p, "{{TOC");

        /* No more markers */
        if (!next_comment && !next_mmd) return NULL;

        /* Pick the earlier of the two */
        const char *cand = NULL;
        if (next_comment && next_mmd) {
            cand = (next_comment < next_mmd) ? next_comment : next_mmd;
        } else {
            cand = next_comment ? next_comment : next_mmd;
        }

        if (!is_inside_code_or_pre(html, (size_t)(cand - html))) {
            *is_html_comment = (cand == next_comment);
            return cand;
        }

        /* This occurrence is inside code; skip past it and search again */
        if (cand == next_comment) {
            const char *end = strstr(cand, "-->");
            p = end ? end + 3 : cand + 1;
        } else {
            const char *end = strstr(cand, "}}");
            p = end ? end + 2 : cand + 1;
        }
    }
}

/**
 * Process TOC markers in HTML
 */
char *apex_process_toc(const char *html, cmark_node *document, int id_format) {
    if (!html || !document) return html ? strdup(html) : NULL;

    int is_html_comment = 0;
    const char *marker = find_toc_marker_not_in_code(html, &is_html_comment);

    if (!marker) {
        return strdup(html);  /* No valid TOC marker (or all are in code), return as-is */
    }

    /* Collect headers from document */
    header_item *tail = NULL;
    header_item *headers = collect_headers(document, &tail);
    if (!headers) return strdup(html);

    /* Generate IDs for all headers using the specified format */
    for (header_item *h = headers; h; h = h->next) {
        h->id = apex_generate_header_id(h->text, (apex_id_format_t)id_format);
    }

    /* Parse the marker for min/max levels */
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
    if (is_html_comment) {
        marker_end = strstr(marker, "-->");
        if (marker_end) marker_end += 3;
    } else {
        marker_end = strstr(marker, "}}");
        if (marker_end) marker_end += 2;
    }

    if (!marker_end) {
        free(toc_html);
        free(output);
        return strdup(html);
    }

    /* Build output: before + TOC + after */
    size_t before_len = (size_t)(marker - html);
    size_t after_len = strlen(marker_end);

    memcpy(output, html, before_len);
    memcpy(output + before_len, toc_html, toc_len);
    memcpy(output + before_len + toc_len, marker_end, after_len + 1);

    free(toc_html);
    return output;
}

