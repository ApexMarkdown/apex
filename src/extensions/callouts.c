/**
 * Callouts Extension for Apex
 * Implementation
 */

#include "callouts.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>

/**
 * Detect callout type from string (case-insensitive)
 */
static callout_type_t detect_callout_type(const char *type_str, int len) {
    char type_upper[64];
    if (len >= sizeof(type_upper)) return CALLOUT_NONE;

    /* Convert to uppercase for comparison */
    for (int i = 0; i < len; i++) {
        type_upper[i] = toupper((unsigned char)type_str[i]);
    }
    type_upper[len] = '\0';

    /* Check all callout types */
    if (strcmp(type_upper, "NOTE") == 0) return CALLOUT_NOTE;
    if (strcmp(type_upper, "ABSTRACT") == 0 || strcmp(type_upper, "SUMMARY") == 0 || strcmp(type_upper, "TLDR") == 0) return CALLOUT_ABSTRACT;
    if (strcmp(type_upper, "INFO") == 0) return CALLOUT_INFO;
    if (strcmp(type_upper, "TODO") == 0) return CALLOUT_TODO;
    if (strcmp(type_upper, "TIP") == 0 || strcmp(type_upper, "HINT") == 0 || strcmp(type_upper, "IMPORTANT") == 0) return CALLOUT_TIP;
    if (strcmp(type_upper, "SUCCESS") == 0 || strcmp(type_upper, "CHECK") == 0 || strcmp(type_upper, "DONE") == 0) return CALLOUT_SUCCESS;
    if (strcmp(type_upper, "QUESTION") == 0 || strcmp(type_upper, "HELP") == 0 || strcmp(type_upper, "FAQ") == 0) return CALLOUT_QUESTION;
    if (strcmp(type_upper, "WARNING") == 0 || strcmp(type_upper, "CAUTION") == 0 || strcmp(type_upper, "ATTENTION") == 0) return CALLOUT_WARNING;
    if (strcmp(type_upper, "FAILURE") == 0 || strcmp(type_upper, "FAIL") == 0 || strcmp(type_upper, "MISSING") == 0) return CALLOUT_FAILURE;
    if (strcmp(type_upper, "DANGER") == 0 || strcmp(type_upper, "ERROR") == 0) return CALLOUT_DANGER;
    if (strcmp(type_upper, "BUG") == 0) return CALLOUT_BUG;
    if (strcmp(type_upper, "EXAMPLE") == 0) return CALLOUT_EXAMPLE;
    if (strcmp(type_upper, "QUOTE") == 0 || strcmp(type_upper, "CITE") == 0) return CALLOUT_QUOTE;

    return CALLOUT_NONE;
}

/**
 * Get callout type name for HTML class
 */
static const char *callout_type_name(callout_type_t type) {
    switch (type) {
        case CALLOUT_NOTE: return "note";
        case CALLOUT_ABSTRACT: return "abstract";
        case CALLOUT_INFO: return "info";
        case CALLOUT_TODO: return "todo";
        case CALLOUT_TIP: return "tip";
        case CALLOUT_SUCCESS: return "success";
        case CALLOUT_QUESTION: return "question";
        case CALLOUT_WARNING: return "warning";
        case CALLOUT_FAILURE: return "failure";
        case CALLOUT_DANGER: return "danger";
        case CALLOUT_BUG: return "bug";
        case CALLOUT_EXAMPLE: return "example";
        case CALLOUT_QUOTE: return "quote";
        default: return "note";
    }
}

/**
 * Check if a blockquote is a Bear/Obsidian style callout
 * Pattern: > [!TYPE] Title or > [!TYPE]+ Title or > [!TYPE]- Title
 */
static bool is_bear_callout(cmark_node *blockquote, callout_type_t *type,
                            char **title, bool *collapsible, bool *default_open) {
    if (cmark_node_get_type(blockquote) != CMARK_NODE_BLOCK_QUOTE) return false;

    /* Get first child (should be paragraph) */
    cmark_node *first_child = cmark_node_first_child(blockquote);
    if (!first_child || cmark_node_get_type(first_child) != CMARK_NODE_PARAGRAPH) return false;

    /* Get the text content */
    cmark_node *text_node = cmark_node_first_child(first_child);
    if (!text_node || cmark_node_get_type(text_node) != CMARK_NODE_TEXT) return false;

    const char *text = cmark_node_get_literal(text_node);
    if (!text) return false;

    /* Check for [!TYPE] pattern */
    if (text[0] != '[' || text[1] != '!') return false;

    const char *type_start = text + 2;
    const char *type_end = strchr(type_start, ']');
    if (!type_end) return false;

    /* Extract type first */
    int type_len = type_end - type_start;
    if (type_len <= 0) return false;

    *type = detect_callout_type(type_start, type_len);
    if (*type == CALLOUT_NONE) return false;

    /* Check for collapsible markers + or - after the ] */
    *collapsible = false;
    *default_open = true;

    if (*(type_end + 1) == '+') {
        *collapsible = true;
        *default_open = true;
        type_end++;
    } else if (*(type_end + 1) == '-') {
        *collapsible = true;
        *default_open = false;
        type_end++;
    }

    /* Now type_end points to either ] or the +/- marker */
    int type_len_unused = type_len;
    /* Extract title (rest of the line after ] or +/-) */
    const char *title_start = type_end + 1;
    while (*title_start == ' ' || *title_start == '\t') title_start++;

    if (*title_start) {
        /* Find end of line */
        const char *title_end = strchr(title_start, '\n');
        if (title_end) {
            *title = strndup(title_start, title_end - title_start);
        } else {
            *title = strdup(title_start);
        }
    }

    return true;
}

/**
 * Convert blockquote to callout HTML
 */
static void convert_blockquote_to_callout(cmark_node *blockquote, callout_type_t type,
                                         const char *title, bool collapsible, bool default_open) {
    const char *type_name = callout_type_name(type);

    /* Build callout HTML */
    char html_start[1024];
    char html_end[256];

    if (collapsible) {
        snprintf(html_start, sizeof(html_start),
                "<details class=\"callout callout-%s\"%s>\n<summary>%s</summary>\n<div class=\"callout-content\">\n",
                type_name, default_open ? " open" : "", title ? title : type_name);
        strcpy(html_end, "\n</div>\n</details>");
    } else {
        snprintf(html_start, sizeof(html_start),
                "<div class=\"callout callout-%s\">\n<div class=\"callout-title\">%s</div>\n<div class=\"callout-content\">\n",
                type_name, title ? title : type_name);
        strcpy(html_end, "\n</div>\n</div>");
    }

    /* Get blockquote content (skip first paragraph with [!TYPE]) */
    cmark_node *first_para = cmark_node_first_child(blockquote);
    if (first_para) {
        /* Remove the [!TYPE] line from first paragraph */
        cmark_node *first_text = cmark_node_first_child(first_para);
        if (first_text && cmark_node_get_type(first_text) == CMARK_NODE_TEXT) {
            const char *text = cmark_node_get_literal(first_text);
            if (text) {
                /* Skip to content after the title line */
                const char *newline = strchr(text, '\n');
                if (newline && *(newline + 1)) {
                    cmark_node_set_literal(first_text, newline + 1);
                } else {
                    /* Remove the text node entirely if it's just the [!TYPE] line */
                    cmark_node_unlink(first_text);
                    cmark_node_free(first_text);
                }
            }
        }
    }

    /* Create HTML wrapper */
    cmark_node *html_before = cmark_node_new(CMARK_NODE_HTML_BLOCK);
    cmark_node_set_literal(html_before, html_start);

    cmark_node *html_after = cmark_node_new(CMARK_NODE_HTML_BLOCK);
    cmark_node_set_literal(html_after, html_end);

    /* Insert HTML nodes */
    cmark_node_insert_before(blockquote, html_before);
    cmark_node_insert_after(blockquote, html_after);

    /* Convert blockquote to div (we'll let the content render normally) */
    /* Actually, we can just keep it as blockquote and wrap it */
}

/**
 * Process callouts in AST
 */
void apex_process_callouts_in_tree(cmark_node *node) {
    if (!node) return;

    /* Check if current node is a blockquote callout */
    if (cmark_node_get_type(node) == CMARK_NODE_BLOCK_QUOTE) {
        callout_type_t type;
        char *title = NULL;
        bool collapsible, default_open;

        if (is_bear_callout(node, &type, &title, &collapsible, &default_open)) {
            convert_blockquote_to_callout(node, type, title, collapsible, default_open);
            free(title);
            return;  /* Don't recurse into modified node */
        }
    }

    /* Recursively process children */
    cmark_node *child = cmark_node_first_child(node);
    while (child) {
        cmark_node *next = cmark_node_next(child);
        apex_process_callouts_in_tree(child);
        child = next;
    }
}

