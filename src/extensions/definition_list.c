/**
 * Definition List Extension for Apex
 * Implementation
 *
 * Supports Kramdown/PHP Markdown Extra style definition lists:
 * Term
 * : Definition 1
 * : Definition 2
 *
 * With block-level content in definitions:
 * Term
 * : Definition with paragraphs
 *
 *   And code blocks
 *
 *       code here
 */

#include "definition_list.h"
#include "parser.h"
#include "node.h"
#include "html.h"
#include "render.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

/* Node type IDs */
cmark_node_type APEX_NODE_DEFINITION_LIST;
cmark_node_type APEX_NODE_DEFINITION_TERM;
cmark_node_type APEX_NODE_DEFINITION_DATA;

/**
 * Check if a line starts a definition (starts with : optionally indented up to 3 spaces)
 */
static bool is_definition_line(const unsigned char *input, int len, int *indent) {
    if (!input || len == 0) return false;

    int spaces = 0;

    /* Count leading spaces (up to 3 allowed) */
    while (spaces < 3 && spaces < len && input[spaces] == ' ') {
        spaces++;
    }

    if (spaces >= len) return false;

    /* Must start with : */
    if (input[spaces] != ':') return false;

    /* Must be followed by space or tab */
    if (spaces + 1 >= len) return false;
    if (input[spaces + 1] != ' ' && input[spaces + 1] != '\t') return false;

    *indent = spaces;
    return true;
}

/**
 * Open block - called when we see a ':' character that might start a definition
 */
static cmark_node *open_block(cmark_syntax_extension *ext,
                              int indented,
                              cmark_parser *parser,
                              cmark_node *parent_container,
                              unsigned char *input,
                              int len) {
    if (indented > 3) return NULL; /* Too indented */

    int def_indent;
    if (!is_definition_line(input, len, &def_indent)) return NULL;

    /* Check if previous block was a paragraph (term) */
    cmark_node *prev = cmark_node_last_child(parent_container);
    if (!prev || cmark_node_get_type(prev) != CMARK_NODE_PARAGRAPH) return NULL;

    /* Create definition list container */
    cmark_node *def_list = cmark_node_new_with_mem(APEX_NODE_DEFINITION_LIST, parser->mem);
    if (!def_list) return NULL;

    /* Convert previous paragraph to term */
    cmark_node *term = cmark_node_new_with_mem(APEX_NODE_DEFINITION_TERM, parser->mem);
    if (term) {
        /* Move paragraph children to term */
        cmark_node *child;
        while ((child = cmark_node_first_child(prev))) {
            cmark_node_unlink(child);
            cmark_node_append_child(term, child);
        }
        cmark_node_unlink(prev);
        cmark_node_free(prev);
        cmark_node_append_child(def_list, term);
    }

    return def_list;
}

/**
 * Match block - check if a line continues a definition list
 */
static int match_block(cmark_syntax_extension *ext,
                      cmark_parser *parser,
                      unsigned char *input,
                      int len,
                      cmark_node *container) {
    if (cmark_node_get_type(container) != APEX_NODE_DEFINITION_LIST &&
        cmark_node_get_type(container) != APEX_NODE_DEFINITION_DATA) {
        return 0;
    }

    int def_indent;
    if (is_definition_line(input, len, &def_indent)) {
        return 1; /* This line continues the definition list */
    }

    /* Also continue if line is blank or indented (block content in definition) */
    if (len == 0 || (len > 0 && (input[0] == ' ' || input[0] == '\t'))) {
        if (cmark_node_get_type(container) == APEX_NODE_DEFINITION_DATA) {
            return 1; /* Block content in definition */
        }
    }

    return 0;
}

/**
 * Can contain - definition data can contain block-level content
 */
static int can_contain(cmark_syntax_extension *ext,
                      cmark_node *node,
                      cmark_node_type child_type) {
    if (cmark_node_get_type(node) == APEX_NODE_DEFINITION_DATA) {
        /* Definition data can contain any block-level content */
        return child_type == CMARK_NODE_PARAGRAPH ||
               child_type == CMARK_NODE_CODE_BLOCK ||
               child_type == CMARK_NODE_BLOCK_QUOTE ||
               child_type == CMARK_NODE_LIST ||
               child_type == CMARK_NODE_HEADING ||
               child_type == CMARK_NODE_THEMATIC_BREAK;
    }
    return 0;
}

/**
 * Process definition lists - convert : syntax to HTML
 * This is a preprocessing approach
 */
char *apex_process_definition_lists(const char *text) {
    if (!text) return NULL;

    size_t text_len = strlen(text);
    size_t output_capacity = text_len * 3;  /* Generous for HTML tags */
    char *output = malloc(output_capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = output_capacity;

    bool in_def_list = false;
    bool in_term = false;
    char term_buffer[4096];
    int term_len = 0;

    while (*read) {
        const char *line_start = read;
        const char *line_end = strchr(read, '\n');
        if (!line_end) line_end = read + strlen(read);

        size_t line_length = line_end - line_start;

        /* Skip table rows (lines that start with |) */
        const char *p = line_start;
        while (p < line_end && (*p == ' ' || *p == '\t')) p++;
        bool is_table_row = (p < line_end && *p == '|');

        /* Check if line starts with : (definition) */
        p = line_start;
        int spaces = 0;
        while (*p == ' ' && spaces < 3 && p < line_end) {
            spaces++;
            p++;
        }

        bool is_def_line = false;
        if (!is_table_row && p < line_end && *p == ':' && (p + 1) < line_end &&
            (p[1] == ' ' || p[1] == '\t')) {
            is_def_line = true;
        }

        if (is_def_line) {
            /* Definition line */
            if (!in_def_list) {
                /* Start new definition list */
                const char *dl_start = "<dl>\n";
                size_t dl_len = strlen(dl_start);
                if (dl_len < remaining) {
                    memcpy(write, dl_start, dl_len);
                    write += dl_len;
                    remaining -= dl_len;
                }

                /* Write term from buffer */
                if (term_len > 0) {
                    const char *dt_start = "<dt>";
                    size_t dt_start_len = strlen(dt_start);
                    if (dt_start_len < remaining) {
                        memcpy(write, dt_start, dt_start_len);
                        write += dt_start_len;
                        remaining -= dt_start_len;
                    }

                    if (term_len < remaining) {
                        memcpy(write, term_buffer, term_len);
                        write += term_len;
                        remaining -= term_len;
                    }

                    const char *dt_end = "</dt>\n";
                    size_t dt_end_len = strlen(dt_end);
                    if (dt_end_len < remaining) {
                        memcpy(write, dt_end, dt_end_len);
                        write += dt_end_len;
                        remaining -= dt_end_len;
                    }

                    term_len = 0;
                }

                in_def_list = true;
            }

            /* Write definition */
            const char *dd_start = "<dd>";
            size_t dd_start_len = strlen(dd_start);
            if (dd_start_len < remaining) {
                memcpy(write, dd_start, dd_start_len);
                write += dd_start_len;
                remaining -= dd_start_len;
            }

            /* Extract definition text (after : and space) */
            p++;  /* Skip : */
            while (p < line_end && (*p == ' ' || *p == '\t')) p++;

            size_t def_text_len = line_end - p;

            /* Parse definition text as inline Markdown */
            char *def_html = NULL;
            if (def_text_len > 0) {
                /* Create temporary buffer for definition text */
                char *def_text = malloc(def_text_len + 1);
                if (def_text) {
                    memcpy(def_text, p, def_text_len);
                    def_text[def_text_len] = '\0';

                    /* Parse as Markdown and render to HTML */
                    cmark_parser *temp_parser = cmark_parser_new(CMARK_OPT_DEFAULT);
                    if (temp_parser) {
                        cmark_parser_feed(temp_parser, def_text, def_text_len);
                        cmark_node *doc = cmark_parser_finish(temp_parser);
                        if (doc) {
                            /* Render and extract just the content (strip <p> tags) */
                            char *full_html = cmark_render_html(doc, CMARK_OPT_DEFAULT, NULL);
                            if (full_html) {
                                /* Strip <p> and </p> tags if present */
                                char *content_start = full_html;
                                if (strncmp(content_start, "<p>", 3) == 0) {
                                    content_start += 3;
                                }
                                char *content_end = content_start + strlen(content_start);
                                if (content_end > content_start + 4 &&
                                    strcmp(content_end - 5, "</p>\n") == 0) {
                                    content_end -= 5;
                                    *content_end = '\0';
                                }
                                def_html = strdup(content_start);
                                free(full_html);
                            }
                            cmark_node_free(doc);
                        }
                        cmark_parser_free(temp_parser);
                    }
                    free(def_text);
                }
            }

            /* Write processed HTML or original text */
            if (def_html) {
                size_t html_len = strlen(def_html);
                if (html_len < remaining) {
                    memcpy(write, def_html, html_len);
                    write += html_len;
                    remaining -= html_len;
                }
                free(def_html);
            } else if (def_text_len < remaining) {
                /* Fallback to original text if parsing failed */
                memcpy(write, p, def_text_len);
                write += def_text_len;
                remaining -= def_text_len;
            }

            const char *dd_end = "</dd>\n";
            size_t dd_end_len = strlen(dd_end);
            if (dd_end_len < remaining) {
                memcpy(write, dd_end, dd_end_len);
                write += dd_end_len;
                remaining -= dd_end_len;
            }
        } else if (line_length == 0 || (line_length == 1 && *line_start == '\r')) {
            /* Blank line */
            if (in_def_list) {
                /* End definition list */
                const char *dl_end = "</dl>\n\n";
                size_t dl_end_len = strlen(dl_end);
                if (dl_end_len < remaining) {
                    memcpy(write, dl_end, dl_end_len);
                    write += dl_end_len;
                    remaining -= dl_end_len;
                }
                in_def_list = false;
                term_len = 0;
            } else {
                /* Flush any buffered term before writing blank line */
                if (term_len > 0) {
                    if (term_len < remaining) {
                        memcpy(write, term_buffer, term_len);
                        write += term_len;
                        remaining -= term_len;
                    }
                    if (remaining > 0) {
                        *write++ = '\n';
                        remaining--;
                    }
                    term_len = 0;
                }
                /* Regular blank line */
                if (remaining > 0) {
                    *write++ = '\n';
                    remaining--;
                }
            }
        } else {
            /* Regular line */
            if (in_def_list) {
                /* This could be a new term */
                /* End current list first */
                const char *dl_end = "</dl>\n\n";
                size_t dl_end_len = strlen(dl_end);
                if (dl_end_len < remaining) {
                    memcpy(write, dl_end, dl_end_len);
                    write += dl_end_len;
                    remaining -= dl_end_len;
                }
                in_def_list = false;
            }

            /* If we have a buffered term that wasn't used, write it first */
            if (term_len > 0) {
                if (term_len < remaining) {
                    memcpy(write, term_buffer, term_len);
                    write += term_len;
                    remaining -= term_len;
                }
                if (remaining > 0) {
                    *write++ = '\n';
                    remaining--;
                }
                term_len = 0;
            }

            /* If this is a table row, write it through immediately without buffering */
            if (is_table_row) {
                if (line_length < remaining) {
                    memcpy(write, line_start, line_length);
                    write += line_length;
                    remaining -= line_length;
                }
                if (remaining > 0 && *line_end == '\n') {
                    *write++ = '\n';
                    remaining--;
                }
            }
            /* Check if line contains IAL syntax - if so, write immediately without buffering */
            else if (strstr(line_start, "{:") != NULL) {
                /* Contains IAL - don't buffer it */
                if (line_length < remaining) {
                    memcpy(write, line_start, line_length);
                    write += line_length;
                    remaining -= line_length;
                }
                if (remaining > 0 && *line_end == '\n') {
                    *write++ = '\n';
                    remaining--;
                }
            }
            /* Save current line as potential term */
            else if (line_length < sizeof(term_buffer) - 1) {
                memcpy(term_buffer, line_start, line_length);
                term_len = line_length;
                term_buffer[term_len] = '\0';
                /* Don't write yet - wait to see if next line is definition */
            } else {
                /* Line too long for buffer, just copy through */
                if (line_length < remaining) {
                    memcpy(write, line_start, line_length);
                    write += line_length;
                    remaining -= line_length;
                }
                if (remaining > 0 && *line_end == '\n') {
                    *write++ = '\n';
                    remaining--;
                }
            }
        }

        /* Move to next line */
        read = line_end;
        if (*read == '\n') read++;
    }

    /* Close any open definition list */
    if (in_def_list) {
        const char *dl_end = "</dl>\n";
        size_t dl_end_len = strlen(dl_end);
        if (dl_end_len < remaining) {
            memcpy(write, dl_end, dl_end_len);
            write += dl_end_len;
            remaining -= dl_end_len;
        }
    }

    /* Write any remaining term */
    if (term_len > 0) {
        if (term_len < remaining) {
            memcpy(write, term_buffer, term_len);
            write += term_len;
            remaining -= term_len;
        }
        if (remaining > 0) {
            *write++ = '\n';
            remaining--;
        }
    }

    *write = '\0';
    return output;
}

/**
 * Post-process - no longer needed with preprocessing approach
 */
static cmark_node *postprocess(cmark_syntax_extension *ext,
                               cmark_parser *parser,
                               cmark_node *root) {
    /* Definition lists are now handled via preprocessing */
    return root;
}

/**
 * Render definition list to HTML
 */
static void html_render(cmark_syntax_extension *ext,
                       struct cmark_html_renderer *renderer,
                       cmark_node *node,
                       cmark_event_type ev_type,
                       int options) {
    cmark_strbuf *html = renderer->html;

    if (ev_type == CMARK_EVENT_ENTER) {
        if (node->type == APEX_NODE_DEFINITION_LIST) {
            cmark_strbuf_puts(html, "<dl>\n");
        } else if (node->type == APEX_NODE_DEFINITION_TERM) {
            cmark_strbuf_puts(html, "<dt>");
        } else if (node->type == APEX_NODE_DEFINITION_DATA) {
            cmark_strbuf_puts(html, "<dd>");
        }
    } else if (ev_type == CMARK_EVENT_EXIT) {
        if (node->type == APEX_NODE_DEFINITION_LIST) {
            cmark_strbuf_puts(html, "</dl>\n");
        } else if (node->type == APEX_NODE_DEFINITION_TERM) {
            cmark_strbuf_puts(html, "</dt>\n");
        } else if (node->type == APEX_NODE_DEFINITION_DATA) {
            cmark_strbuf_puts(html, "</dd>\n");
        }
    }
}

/**
 * Create definition list extension
 */
cmark_syntax_extension *create_definition_list_extension(void) {
    cmark_syntax_extension *ext = cmark_syntax_extension_new("definition_list");
    if (!ext) return NULL;

    /* Register node types */
    APEX_NODE_DEFINITION_LIST = cmark_syntax_extension_add_node(0);
    APEX_NODE_DEFINITION_TERM = cmark_syntax_extension_add_node(0);
    APEX_NODE_DEFINITION_DATA = cmark_syntax_extension_add_node(0);

    /* Set callbacks */
    cmark_syntax_extension_set_open_block_func(ext, open_block);
    cmark_syntax_extension_set_match_block_func(ext, match_block);
    cmark_syntax_extension_set_can_contain_func(ext, can_contain);
    cmark_syntax_extension_set_html_render_func(ext, html_render);
    cmark_syntax_extension_set_postprocess_func(ext, postprocess);

    return ext;
}
