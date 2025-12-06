/**
 * Critic Markup Extension for Apex
 * Implementation
 */

#include "critic.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

/**
 * Scan for Critic Markup patterns
 * Returns type and length if found
 */
typedef enum {
    CRITIC_NONE = 0,
    CRITIC_ADD,         /* {++text++} */
    CRITIC_DEL,         /* {--text--} */
    CRITIC_SUB,         /* {~~old~>new~~} */
    CRITIC_HIGHLIGHT,   /* {==text==} */
    CRITIC_COMMENT      /* {>>text<<} */
} critic_type_t;

static critic_type_t scan_critic_markup(const char *input, int len, int *consumed,
                                        const char **content, int *content_len,
                                        const char **old_text, int *old_len) {
    if (len < 6) return CRITIC_NONE;  /* Minimum: {++x++} */

    if (input[0] != '{') return CRITIC_NONE;

    /* Check type */
    critic_type_t type = CRITIC_NONE;
    const char *open_marker = NULL;
    const char *close_marker = NULL;

    if (input[1] == '+' && input[2] == '+') {
        type = CRITIC_ADD;
        open_marker = "{++";
        close_marker = "++}";
    } else if (input[1] == '-' && input[2] == '-') {
        type = CRITIC_DEL;
        open_marker = "{--";
        close_marker = "--}";
    } else if (input[1] == '~' && input[2] == '~') {
        type = CRITIC_SUB;
        open_marker = "{~~";
        close_marker = "~~}";
    } else if (input[1] == '=' && input[2] == '=') {
        type = CRITIC_HIGHLIGHT;
        open_marker = "{==";
        close_marker = "==}";
    } else if (input[1] == '>' && input[2] == '>') {
        type = CRITIC_COMMENT;
        open_marker = "{>>";
        close_marker = "<<}";
    } else {
        return CRITIC_NONE;
    }

    *content = input + 3;  /* Skip opening marker */

    /* Find closing marker */
    const char *closer = strstr(*content, close_marker);
    if (!closer) return CRITIC_NONE;

    *content_len = closer - *content;
    *consumed = (closer - input) + 3;  /* Include closing marker */

    /* For substitutions, split on ~> */
    if (type == CRITIC_SUB) {
        const char *sep = strstr(*content, "~>");
        if (sep && sep < closer) {
            *old_len = sep - *content;
            *old_text = *content;
            *content = sep + 2;  /* Skip ~> */
            *content_len = closer - *content;
        }
    }

    return type;
}

/**
 * Create HTML for critic markup based on mode
 */
static char *critic_to_html(critic_type_t type, const char *content, int content_len,
                            const char *old_text, int old_len, critic_mode_t mode) {
    char *html = NULL;
    size_t html_len = content_len + old_len + 200;
    html = malloc(html_len);
    if (!html) return NULL;

    switch (mode) {
        case CRITIC_ACCEPT:
            /* Show only additions and new text in substitutions */
            switch (type) {
                case CRITIC_ADD:
                    snprintf(html, html_len, "%.*s", content_len, content);
                    break;
                case CRITIC_SUB:
                    snprintf(html, html_len, "%.*s", content_len, content);
                    break;
                case CRITIC_HIGHLIGHT:
                    /* Show highlight text without markup */
                    snprintf(html, html_len, "%.*s", content_len, content);
                    break;
                case CRITIC_DEL:
                case CRITIC_COMMENT:
                    html[0] = '\0';  /* Remove deletions and comments */
                    break;
                default:
                    html[0] = '\0';
                    break;
            }
            break;

        case CRITIC_REJECT:
            /* Show only original text */
            switch (type) {
                case CRITIC_SUB:
                    if (old_text && old_len > 0) {
                        snprintf(html, html_len, "%.*s", old_len, old_text);
                    } else {
                        html[0] = '\0';
                    }
                    break;
                case CRITIC_DEL:
                    snprintf(html, html_len, "%.*s", content_len, content);
                    break;
                case CRITIC_HIGHLIGHT:
                    /* Show highlight text without markup */
                    snprintf(html, html_len, "%.*s", content_len, content);
                    break;
                case CRITIC_ADD:
                case CRITIC_COMMENT:
                    html[0] = '\0';  /* Remove additions and comments */
                    break;
                default:
                    html[0] = '\0';
                    break;
            }
            break;

        case CRITIC_MARKUP:
            /* Show markup with HTML classes */
            switch (type) {
                case CRITIC_ADD:
                    snprintf(html, html_len, "<ins class=\"critic\">%.*s</ins>", content_len, content);
                    break;
                case CRITIC_DEL:
                    snprintf(html, html_len, "<del class=\"critic\">%.*s</del>", content_len, content);
                    break;
                case CRITIC_SUB:
                    if (old_text && old_len > 0) {
                        snprintf(html, html_len,
                                "<del class=\"critic break\">%.*s</del><ins class=\"critic break\">%.*s</ins>",
                                old_len, old_text, content_len, content);
                    } else {
                        snprintf(html, html_len, "<ins class=\"critic\">%.*s</ins>", content_len, content);
                    }
                    break;
                case CRITIC_HIGHLIGHT:
                    snprintf(html, html_len, "<mark class=\"critic\">%.*s</mark>", content_len, content);
                    break;
                case CRITIC_COMMENT:
                    snprintf(html, html_len, "<span class=\"critic comment\">%.*s</span>", content_len, content);
                    break;
                default:
                    html[0] = '\0';
                    break;
            }
            break;
    }

    return html;
}

/**
 * Process Critic Markup in text nodes
 */
static void process_critic_in_text_node(cmark_node *node, critic_mode_t mode) {
    const char *literal = cmark_node_get_literal(node);
    if (!literal) return;

    /* Look for critic markup - scan through entire string looking for { */
    const char *search = literal;
    const char *start = NULL;

    while ((start = strchr(search, '{')) != NULL) {
        /* Check if it's valid critic markup */
        int consumed;
        const char *content;
        int content_len;
        const char *old_text = NULL;
        int old_len = 0;

        critic_type_t type = scan_critic_markup(start, strlen(start), &consumed,
                                               &content, &content_len, &old_text, &old_len);

        if (type != CRITIC_NONE) {
            /* Found valid critic markup */
            size_t prefix_len = start - literal;
            const char *suffix = start + consumed;

            /* Generate HTML for the critic markup */
            char *html = critic_to_html(type, content, content_len, old_text, old_len, mode);
            if (!html) {
                search = start + 1;
                continue;
            }

            /* Create prefix text node if there's text before the markup */
            if (prefix_len > 0) {
                char *prefix = malloc(prefix_len + 1);
                if (prefix) {
                    memcpy(prefix, literal, prefix_len);
                    prefix[prefix_len] = '\0';
                    cmark_node_set_literal(node, prefix);
                    free(prefix);
                }
            }

            /* Create HTML inline node for the critic markup */
            cmark_node *html_node = cmark_node_new(CMARK_NODE_HTML_INLINE);
            cmark_node_set_literal(html_node, html);
            free(html);

            if (prefix_len > 0) {
                /* Insert HTML after the prefix text node */
                cmark_node_insert_after(node, html_node);
            } else {
                /* Replace the node entirely */
                cmark_node_insert_before(node, html_node);
                cmark_node_unlink(node);
                cmark_node_free(node);
                node = html_node;  /* Continue from the new node */
            }

            /* If there's suffix, create new text node and process it */
            if (*suffix) {
                cmark_node *suffix_node = cmark_node_new(CMARK_NODE_TEXT);
                cmark_node_set_literal(suffix_node, suffix);
                cmark_node_insert_after(html_node, suffix_node);

                /* Recursively process the suffix */
                process_critic_in_text_node(suffix_node, mode);
            }

            return;  /* Done processing this node */
        }

        /* Not critic markup, continue searching */
        search = start + 1;
    }
}

/**
 * Recursively process Critic Markup in AST
 */
void apex_process_critic_markup_in_tree(cmark_node *node, critic_mode_t mode) {
    if (!node) return;

    /* Process current node if it's text */
    if (cmark_node_get_type(node) == CMARK_NODE_TEXT) {
        process_critic_in_text_node(node, mode);
        return;  /* Don't recurse after modifying */
    }

    /* Recursively process children */
    cmark_node *child = cmark_node_first_child(node);
    while (child) {
        cmark_node *next = cmark_node_next(child);  /* Save next before potential modification */
        apex_process_critic_markup_in_tree(child, mode);
        child = next;
    }
}

/**
 * Process Critic Markup in raw text (preprocessing approach)
 * This is better than postprocessing because it avoids smart typography interference
 */
char *apex_process_critic_markup_text(const char *text, critic_mode_t mode) {
    if (!text) return NULL;

    size_t len = strlen(text);
    size_t output_capacity = len * 2;  /* Generous estimate */
    char *output = malloc(output_capacity);
    if (!output) return NULL;

    const char *read_pos = text;
    char *write_pos = output;
    size_t remaining_capacity = output_capacity;

    while (*read_pos) {
        /* Look for { */
        if (*read_pos == '{') {
            int consumed;
            const char *content;
            int content_len;
            const char *old_text = NULL;
            int old_len = 0;

            critic_type_t type = scan_critic_markup(read_pos, strlen(read_pos), &consumed,
                                                   &content, &content_len, &old_text, &old_len);

            if (type != CRITIC_NONE) {
                /* Found valid critic markup - convert to HTML */
                char *html = critic_to_html(type, content, content_len, old_text, old_len, mode);
                if (html) {
                    size_t html_len = strlen(html);
                    if (html_len < remaining_capacity) {
                        memcpy(write_pos, html, html_len);
                        write_pos += html_len;
                        remaining_capacity -= html_len;
                    }
                    free(html);
                }
                read_pos += consumed;
                continue;
            }
        }

        /* Not critic markup, copy character */
        if (remaining_capacity > 0) {
            *write_pos++ = *read_pos;
            remaining_capacity--;
        }
        read_pos++;
    }

    *write_pos = '\0';
    return output;
}

