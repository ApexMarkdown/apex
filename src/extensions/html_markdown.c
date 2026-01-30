/**
 * HTML Markdown Attributes Extension for Apex
 * Implementation
 */

#include "html_markdown.h"
#include "ial.h"
#include "../html_renderer.h"
#include "cmark-gfm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/**
 * Find the next HTML tag with markdown attribute
 * Returns position of '<' or NULL if not found
 */
static const char *find_markdown_tag(const char *text, char *tag_name, size_t tag_name_size,
                                     char *markdown_attr, size_t attr_size, size_t *tag_length) {
    const char *pos = text;

    while (*pos) {
        /* Find opening < */
        if (*pos == '<' && pos[1] != '/' && pos[1] != '!') {
            const char *tag_start = pos;
            pos++;

            /* Extract tag name */
            const char *name_start = pos;
            while (*pos && (isalnum((unsigned char)*pos) || *pos == '-' || *pos == '_')) {
                pos++;
            }

            size_t name_len = pos - name_start;
            if (name_len == 0 || name_len >= tag_name_size) {
                pos = tag_start + 1;
                continue;
            }

            memcpy(tag_name, name_start, name_len);
            tag_name[name_len] = '\0';

            /* Look for markdown attribute in tag */
            const char *tag_end = strchr(pos, '>');
            if (!tag_end) {
                pos = tag_start + 1;
                continue;
            }

            /* Search for markdown= in attributes */
            const char *attr_search = pos;
            while (attr_search < tag_end) {
                /* Skip whitespace */
                while (attr_search < tag_end && isspace((unsigned char)*attr_search)) {
                    attr_search++;
                }

                /* Check for markdown attribute */
                if (strncmp(attr_search, "markdown=", 9) == 0) {
                    attr_search += 9;

                    /* Get attribute value */
                    char quote = 0;
                    if (*attr_search == '"' || *attr_search == '\'') {
                        quote = *attr_search;
                        attr_search++;
                    }

                    const char *value_start = attr_search;
                    const char *value_end = value_start;

                    if (quote) {
                        value_end = strchr(value_start, quote);
                        if (!value_end) value_end = tag_end;
                    } else {
                        while (*value_end && !isspace((unsigned char)*value_end) && *value_end != '>') {
                            value_end++;
                        }
                    }

                    size_t value_len = value_end - value_start;
                    if (value_len < attr_size) {
                        memcpy(markdown_attr, value_start, value_len);
                        markdown_attr[value_len] = '\0';

                        *tag_length = (tag_end - tag_start) + 1;
                        return tag_start;
                    }
                }

                /* Move to next attribute */
                while (attr_search < tag_end && !isspace((unsigned char)*attr_search)) {
                    attr_search++;
                }
            }

            pos = tag_start + 1;
        } else {
            pos++;
        }
    }

    return NULL;
}

/**
 * Find matching closing tag
 * Handles nested tags correctly
 */
static const char *find_closing_tag(const char *text, const char *tag_name) {
    int depth = 1;
    const char *pos = text;
    size_t tag_len = strlen(tag_name);

    while (*pos && depth > 0) {
        if (*pos == '<') {
            /* Check for closing tag */
            if (pos[1] == '/' && strncasecmp(pos + 2, tag_name, tag_len) == 0 &&
                (pos[2 + tag_len] == '>' || isspace((unsigned char)pos[2 + tag_len]))) {
                depth--;
                if (depth == 0) {
                    /* Find the > */
                    const char *end = strchr(pos, '>');
                    return end ? end + 1 : NULL;
                }
            }
            /* Check for opening tag (nested) */
            else if (pos[1] != '/' && pos[1] != '!' &&
                     strncasecmp(pos + 1, tag_name, tag_len) == 0 &&
                     (pos[1 + tag_len] == '>' || isspace((unsigned char)pos[1 + tag_len]))) {
                depth++;
            }
        }
        pos++;
    }

    return NULL;
}

/**
 * Process HTML tags with markdown attributes
 */
char *apex_process_html_markdown(const char *text) {
    if (!text) return NULL;

    size_t text_len = strlen(text);
    size_t output_capacity = text_len * 2;
    char *output = malloc(output_capacity);
    if (!output) return NULL;

    const char *read_pos = text;
    char *write_pos = output;
    size_t remaining = output_capacity;

    while (*read_pos) {
        char tag_name[64];
        char markdown_attr[64];
        size_t tag_length;

        /* Find next HTML tag with markdown attribute */
        const char *tag_start = find_markdown_tag(read_pos, tag_name, sizeof(tag_name),
                                                   markdown_attr, sizeof(markdown_attr), &tag_length);

        if (!tag_start) {
            /* No more markdown tags, copy rest */
            size_t rest_len = strlen(read_pos);
            if (rest_len < remaining) {
                memcpy(write_pos, read_pos, rest_len);
                write_pos += rest_len;
                remaining -= rest_len;
            }
            break;
        }

        /* Copy text before tag */
        size_t prefix_len = tag_start - read_pos;
        if (prefix_len < remaining) {
            memcpy(write_pos, read_pos, prefix_len);
            write_pos += prefix_len;
            remaining -= prefix_len;
        }

        /* Find content between tags */
        const char *content_start = tag_start + tag_length;
        const char *closing_tag = find_closing_tag(content_start, tag_name);

        if (!closing_tag) {
            /* No closing tag, just copy the opening tag */
            if (tag_length < remaining) {
                memcpy(write_pos, tag_start, tag_length);
                write_pos += tag_length;
                remaining -= tag_length;
            }
            read_pos = content_start;
            continue;
        }

        /* Extract content */
        size_t content_len = closing_tag - content_start;

        /* Find the actual closing tag start for later */
        const char *closing_tag_start = closing_tag;
        while (closing_tag_start > content_start && *(closing_tag_start - 1) != '<') {
            closing_tag_start--;
        }
        if (closing_tag_start > content_start && *(closing_tag_start - 1) == '<') {
            closing_tag_start--;
        }
        content_len = closing_tag_start - content_start;

        /* Process based on markdown attribute value */
        bool parse_markdown = false;
        bool parse_inline = false;

        if (strcmp(markdown_attr, "1") == 0 || strcmp(markdown_attr, "block") == 0) {
            parse_markdown = true;
            parse_inline = false;
        } else if (strcmp(markdown_attr, "span") == 0) {
            parse_markdown = true;
            parse_inline = true;
        } else if (strcmp(markdown_attr, "0") == 0) {
            parse_markdown = false;
        }

        if (parse_markdown && content_len > 0) {
            /* Extract content */
            char *content = malloc(content_len + 1);
            if (content) {
                memcpy(content, content_start, content_len);
                content[content_len] = '\0';

                /* Recursively process nested divs with markdown="1" BEFORE parsing */
                /* This ensures nested divs are processed before cmark-gfm sees them */
                char *processed_content = apex_process_html_markdown(content);
                if (processed_content) {
                    free(content);
                    content = processed_content;
                    content_len = strlen(content);
                }

                /* Create parser and parse */
                /* Use CMARK_OPT_UNSAFE to allow raw HTML (including nested divs) */
                int cmark_opts = CMARK_OPT_DEFAULT | CMARK_OPT_UNSAFE;
                cmark_parser *parser = cmark_parser_new(cmark_opts);
                if (parser) {
                    cmark_parser_feed(parser, content, content_len);
                    cmark_node *doc = cmark_parser_finish(parser);

                    if (doc) {
                        /* Process IAL in inner content so block/span IAL (e.g. {: .lead }) are applied */
                        apex_process_ial_in_tree(doc, NULL);
                        /* Render to HTML with IAL attributes injected */
                        char *html = apex_render_html_with_attributes(doc, cmark_opts);
                        if (html) {
                            /* Write opening tag (without markdown attribute) */
                            char opening_tag[2048];
                            size_t tag_written = 0;

                            /* Reconstruct tag without markdown attribute */
                            snprintf(opening_tag, sizeof(opening_tag), "<%s", tag_name);
                            tag_written = strlen(opening_tag);

                            /* Copy attributes except markdown */
                            const char *attrs_start = strchr(tag_start + 1, ' ');
                            if (attrs_start && attrs_start < content_start) {
                                const char *attrs_end = strchr(attrs_start, '>');
                                if (attrs_end) {
                                    /* Parse and copy attributes, filtering out markdown attribute */
                                    const char *attr_pos = attrs_start;
                                    while (attr_pos < attrs_end) {
                                        /* Skip whitespace */
                                        while (attr_pos < attrs_end && isspace((unsigned char)*attr_pos)) {
                                            attr_pos++;
                                        }
                                        if (attr_pos >= attrs_end) break;

                                        /* Check if this is the markdown attribute */
                                        if (strncmp(attr_pos, "markdown=", 9) == 0) {
                                            /* Skip markdown attribute */
                                            attr_pos += 9;
                                            /* Skip attribute value */
                                            if (*attr_pos == '"' || *attr_pos == '\'') {
                                                char quote = *attr_pos++;
                                                while (attr_pos < attrs_end && *attr_pos != quote) {
                                                    if (*attr_pos == '\\' && attr_pos + 1 < attrs_end) attr_pos++;
                                                    attr_pos++;
                                                }
                                                if (*attr_pos == quote) attr_pos++;
                                            } else {
                                                while (attr_pos < attrs_end && !isspace((unsigned char)*attr_pos) && *attr_pos != '>') {
                                                    attr_pos++;
                                                }
                                            }
                                            continue;
                                        }

                                        /* Copy this attribute */
                                        const char *attr_start = attr_pos;
                                        while (attr_pos < attrs_end && *attr_pos != '>') {
                                            /* Check if we've reached the start of the next attribute */
                                            if (attr_pos > attr_start && (isspace((unsigned char)*attr_pos) || *attr_pos == '>')) {
                                                /* Check if next token is markdown= */
                                                const char *next = attr_pos;
                                                while (next < attrs_end && isspace((unsigned char)*next)) next++;
                                                if (strncmp(next, "markdown=", 9) == 0) {
                                                    break; /* Stop before markdown attribute */
                                                }
                                            }
                                            attr_pos++;
                                        }

                                        /* Copy attribute to opening_tag */
                                        size_t attr_len = attr_pos - attr_start;
                                        if (tag_written + attr_len + 1 < sizeof(opening_tag)) {
                                            opening_tag[tag_written++] = ' ';
                                            memcpy(opening_tag + tag_written, attr_start, attr_len);
                                            tag_written += attr_len;
                                        }
                                    }
                                    opening_tag[tag_written++] = '>';
                                    opening_tag[tag_written] = '\0';
                                }
                            } else {
                                opening_tag[tag_written++] = '>';
                                opening_tag[tag_written] = '\0';
                            }

                            if (tag_written < remaining) {
                                memcpy(write_pos, opening_tag, tag_written);
                                write_pos += tag_written;
                                remaining -= tag_written;
                            }

                            /* Write parsed HTML (trim outer <p> tags if inline) */
                            char *html_content = html;
                            size_t html_len = strlen(html);

                            if (parse_inline && html_len > 7 &&
                                strncmp(html, "<p>", 3) == 0 &&
                                strcmp(html + html_len - 5, "</p>\n") == 0) {
                                /* Strip <p> tags for inline */
                                html_content = html + 3;
                                html_len -= 8;
                                html_content[html_len] = '\0';
                            }

                            if (html_len < remaining) {
                                memcpy(write_pos, html_content, html_len);
                                write_pos += html_len;
                                remaining -= html_len;
                            }

                            free(html);
                        }
                        cmark_node_free(doc);
                    }
                    cmark_parser_free(parser);
                }
                free(content);
            }

            /* Write closing tag */
            size_t closing_len = closing_tag - closing_tag_start;
            if (closing_len < remaining) {
                memcpy(write_pos, closing_tag_start, closing_len);
                write_pos += closing_len;
                remaining -= closing_len;
            }

            /* Ensure newline after closing tag so following markdown is parsed correctly */
            /* Check if closing tag ends with newline */
            bool needs_newline = true;
            if (closing_len > 0) {
                const char *last_char = closing_tag_start + closing_len - 1;
                if (*last_char == '\n' || (*last_char == '\r' && closing_len > 1 && *(last_char - 1) == '\n')) {
                    needs_newline = false;
                }
            }
            if (needs_newline && remaining > 0) {
                *write_pos++ = '\n';
                remaining--;
            }
        } else {
            /* markdown="0" or no parsing - copy everything as-is */
            size_t total_len = closing_tag - tag_start;
            if (total_len < remaining) {
                memcpy(write_pos, tag_start, total_len);
                write_pos += total_len;
                remaining -= total_len;
            }
        }

        read_pos = closing_tag;
    }

    *write_pos = '\0';
    return output;
}

