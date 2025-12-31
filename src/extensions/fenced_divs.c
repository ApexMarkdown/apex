/**
 * Pandoc Fenced Divs Extension for Apex
 * Implementation
 */

#include "fenced_divs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/**
 * Count consecutive colons at the start of a line
 * Returns the number of colons found
 */
static size_t count_colons(const char *line) {
    size_t count = 0;
    const char *p = line;

    /* Skip leading whitespace */
    while (isspace((unsigned char)*p)) p++;

    /* Count consecutive colons */
    while (*p == ':') {
        count++;
        p++;
    }

    return count;
}

/**
 * Check if a line is a fenced div opening (has attributes)
 * Returns true if it's an opening fence with attributes
 */
static bool is_opening_fence(const char *line, size_t colon_count,
                             const char **attr_start, size_t *attr_len) {
    if (colon_count < 3) return false;

    const char *p = line;

    /* Skip leading whitespace */
    while (isspace((unsigned char)*p)) p++;

    /* Skip colons */
    p += colon_count;

    /* Skip whitespace after colons */
    while (isspace((unsigned char)*p)) p++;

    /* Check if there are attributes (non-whitespace, non-colon content) */
    const char *attr_begin = p;
    const char *line_end = p;
    while (*line_end && *line_end != '\n' && *line_end != '\r') {
        line_end++;
    }

    /* Find the end of attributes (before any trailing colons) */
    const char *attr_end = line_end;
    const char *check = line_end - 1;

    /* Work backwards to find trailing colons */
    while (check >= attr_begin && *check == ':') {
        check--;
    }

    /* Skip whitespace before trailing colons */
    while (check >= attr_begin && isspace((unsigned char)*check)) {
        check--;
    }

    /* If we found non-colon content, attributes end at check+1 */
    if (check >= attr_begin) {
        attr_end = check + 1;
    }

    /* Check if we have actual attribute content (not just whitespace/colons) */
    const char *content_check = attr_begin;
    while (content_check < attr_end && (isspace((unsigned char)*content_check) || *content_check == ':')) {
        content_check++;
    }

    if (content_check < attr_end) {
        *attr_start = attr_begin;
        *attr_len = attr_end - attr_begin;
        return true;
    }

    return false;
}

/**
 * Check if a line is a closing fence (just colons, no attributes)
 */
static bool is_closing_fence(const char *line, size_t colon_count) {
    if (colon_count < 3) return false;

    const char *p = line;

    /* Skip leading whitespace */
    while (isspace((unsigned char)*p)) p++;

    /* Skip colons */
    p += colon_count;

    /* Skip trailing whitespace */
    while (isspace((unsigned char)*p)) p++;

    /* Should be end of line */
    return (*p == '\0' || *p == '\n' || *p == '\r');
}

/**
 * Parse attributes from fenced div info string
 * Format: {#id .class .class2 key="value" key2='value2'}
 * Or: .class (single unbraced word treated as class)
 * Returns newly allocated HTML attribute string, or NULL on error
 */
static char *parse_attributes(const char *attr_text, size_t attr_len) {
    if (!attr_text || attr_len == 0) return NULL;

    char *buffer = malloc(attr_len + 1);
    if (!buffer) return NULL;
    memcpy(buffer, attr_text, attr_len);
    buffer[attr_len] = '\0';

    char *id = NULL;
    char **classes = NULL;
    size_t class_count = 0;
    size_t class_capacity = 0;
    char **keys = NULL;
    char **values = NULL;
    size_t attr_count = 0;
    size_t attr_capacity = 0;

    char *p = buffer;

    /* Trim whitespace */
    while (isspace((unsigned char)*p)) p++;
    char *end = buffer + attr_len;
    while (end > p && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';

    /* Check if it's wrapped in braces */
    bool has_braces = false;
    if (*p == '{') {
        has_braces = true;
        p++;
        if (end > p && *(end - 1) == '}') {
            end--;
            *end = '\0';
        }
    }

    /* If no braces and single word, treat as class */
    if (!has_braces) {
        char *word_start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (p > word_start) {
            size_t word_len = p - word_start;
            char *class = malloc(word_len + 1);
            if (class) {
                memcpy(class, word_start, word_len);
                class[word_len] = '\0';

                class_capacity = 1;
                classes = malloc(sizeof(char*));
                if (classes) {
                    classes[0] = class;
                    class_count = 1;
                } else {
                    free(class);
                }
            }
        }
    } else {
        /* Parse attributes inside braces */
        while (*p) {
            /* Skip whitespace */
            while (isspace((unsigned char)*p)) p++;
            if (!*p) break;

            /* Check for ID (#id) */
            if (*p == '#') {
                p++;
                char *id_start = p;
                while (*p && !isspace((unsigned char)*p) && *p != '.' && *p != '}') p++;
                if (p > id_start) {
                    char saved = *p;
                    *p = '\0';
                    if (id) free(id);
                    id = strdup(id_start);
                    *p = saved;
                }
                continue;
            }

            /* Check for class (.class) */
            if (*p == '.') {
                p++;
                char *class_start = p;
                while (*p && !isspace((unsigned char)*p) && *p != '.' && *p != '#' && *p != '}') p++;
                if (p > class_start) {
                    char saved = *p;
                    *p = '\0';

                    if (class_count >= class_capacity) {
                        class_capacity = class_capacity ? class_capacity * 2 : 4;
                        classes = realloc(classes, sizeof(char*) * class_capacity);
                        if (!classes) break;
                    }
                    classes[class_count++] = strdup(class_start);
                    *p = saved;
                }
                continue;
            }

            /* Check for key="value" or key='value' */
            char *key_start = p;
            while (*p && *p != '=' && !isspace((unsigned char)*p) && *p != '}') p++;

            if (*p == '=') {
                char saved = *p;
                *p = '\0';
                char *key = strdup(key_start);
                *p = saved;
                p++; /* Skip = */

                /* Parse value */
                char *value = NULL;
                if (*p == '"' || *p == '\'') {
                    char quote = *p++;
                    char *value_start = p;
                    while (*p && *p != quote) {
                        if (*p == '\\' && *(p+1)) p++;
                        p++;
                    }
                    if (*p == quote) {
                        *p = '\0';
                        value = strdup(value_start);
                        p++; /* Skip closing quote */
                    }
                } else {
                    char *value_start = p;
                    while (*p && !isspace((unsigned char)*p) && *p != '}') p++;
                    char saved_val = *p;
                    *p = '\0';
                    value = strdup(value_start);
                    *p = saved_val;
                }

                if (key && value) {
                    if (attr_count >= attr_capacity) {
                        attr_capacity = attr_capacity ? attr_capacity * 2 : 4;
                        keys = realloc(keys, sizeof(char*) * attr_capacity);
                        values = realloc(values, sizeof(char*) * attr_capacity);
                        if (!keys || !values) {
                            free(key);
                            free(value);
                            break;
                        }
                    }
                    keys[attr_count] = key;
                    values[attr_count] = value;
                    attr_count++;
                } else {
                    if (key) free(key);
                    if (value) free(value);
                }
                continue;
            }

            /* Unknown token, skip */
            p++;
        }
    }

    /* Build HTML attribute string */
    size_t html_capacity = 256;
    char *html_attrs = malloc(html_capacity);
    if (!html_attrs) {
        if (id) free(id);
        for (size_t i = 0; i < class_count; i++) free(classes[i]);
        if (classes) free(classes);
        for (size_t i = 0; i < attr_count; i++) {
            free(keys[i]);
            free(values[i]);
        }
        if (keys) free(keys);
        if (values) free(values);
        free(buffer);
        return NULL;
    }

    size_t html_len = 0;
    html_attrs[0] = '\0';

    /* Add ID */
    if (id) {
        size_t needed = strlen(id) + 5; /* id="..." */
        if (html_len + needed >= html_capacity) {
            html_capacity = (html_len + needed) * 2;
            char *new_attrs = realloc(html_attrs, html_capacity);
            if (!new_attrs) {
                free(html_attrs);
                if (id) free(id);
                for (size_t i = 0; i < class_count; i++) free(classes[i]);
                if (classes) free(classes);
                for (size_t i = 0; i < attr_count; i++) {
                    free(keys[i]);
                    free(values[i]);
                }
                if (keys) free(keys);
                if (values) free(values);
                free(buffer);
                return NULL;
            }
            html_attrs = new_attrs;
        }
        html_len += snprintf(html_attrs + html_len, html_capacity - html_len, " id=\"%s\"", id);
    }

    /* Add classes */
    if (class_count > 0) {
        size_t class_str_len = 0;
        for (size_t i = 0; i < class_count; i++) {
            class_str_len += strlen(classes[i]) + 1; /* +1 for space */
        }
        size_t needed = class_str_len + 8; /* class="..." */
        if (html_len + needed >= html_capacity) {
            html_capacity = (html_len + needed) * 2;
            char *new_attrs = realloc(html_attrs, html_capacity);
            if (!new_attrs) {
                free(html_attrs);
                if (id) free(id);
                for (size_t i = 0; i < class_count; i++) free(classes[i]);
                if (classes) free(classes);
                for (size_t i = 0; i < attr_count; i++) {
                    free(keys[i]);
                    free(values[i]);
                }
                if (keys) free(keys);
                if (values) free(values);
                free(buffer);
                return NULL;
            }
            html_attrs = new_attrs;
        }
        html_len += snprintf(html_attrs + html_len, html_capacity - html_len, " class=\"");
        for (size_t i = 0; i < class_count; i++) {
            if (i > 0) html_len += snprintf(html_attrs + html_len, html_capacity - html_len, " ");
            html_len += snprintf(html_attrs + html_len, html_capacity - html_len, "%s", classes[i]);
        }
        html_len += snprintf(html_attrs + html_len, html_capacity - html_len, "\"");
    }

    /* Add other attributes */
    for (size_t i = 0; i < attr_count; i++) {
        size_t needed = strlen(keys[i]) + strlen(values[i]) + 4; /* key="value" */
        if (html_len + needed >= html_capacity) {
            html_capacity = (html_len + needed) * 2;
            char *new_attrs = realloc(html_attrs, html_capacity);
            if (!new_attrs) {
                free(html_attrs);
                if (id) free(id);
                for (size_t i = 0; i < class_count; i++) free(classes[i]);
                if (classes) free(classes);
                for (size_t i = 0; i < attr_count; i++) {
                    free(keys[i]);
                    free(values[i]);
                }
                if (keys) free(keys);
                if (values) free(values);
                free(buffer);
                return NULL;
            }
            html_attrs = new_attrs;
        }
        html_len += snprintf(html_attrs + html_len, html_capacity - html_len, " %s=\"%s\"", keys[i], values[i]);
    }

    /* Cleanup */
    if (id) free(id);
    for (size_t i = 0; i < class_count; i++) free(classes[i]);
    if (classes) free(classes);
    for (size_t i = 0; i < attr_count; i++) {
        free(keys[i]);
        free(values[i]);
    }
    if (keys) free(keys);
    if (values) free(values);
    free(buffer);

    return html_attrs;
}

/**
 * Process fenced divs in text
 */
char *apex_process_fenced_divs(const char *text) {
    if (!text) return NULL;

    size_t text_len = strlen(text);
    size_t output_capacity = text_len * 2;
    char *output = malloc(output_capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = output_capacity;

    /* Track nesting level for divs */
    typedef struct {
        size_t colon_count;
        char *html_attrs;
    } div_stack_item;

    div_stack_item *div_stack = NULL;
    size_t div_stack_size = 0;
    size_t div_stack_capacity = 0;

    while (*read) {
        /* Find end of current line */
        const char *line_end = read;
        while (*line_end && *line_end != '\n' && *line_end != '\r') {
            line_end++;
        }

        size_t line_len = line_end - read;
        char *line = malloc(line_len + 1);
        if (!line) {
            /* Cleanup and return */
            for (size_t i = 0; i < div_stack_size; i++) {
                if (div_stack[i].html_attrs) free(div_stack[i].html_attrs);
            }
            if (div_stack) free(div_stack);
            free(output);
            return NULL;
        }
        memcpy(line, read, line_len);
        line[line_len] = '\0';

        size_t colon_count = count_colons(line);
        const char *attr_start = NULL;
        size_t attr_len = 0;
        bool is_opening = is_opening_fence(line, colon_count, &attr_start, &attr_len);
        bool is_closing = is_closing_fence(line, colon_count);

        if (is_opening) {
            /* Opening fence with attributes */
            char *html_attrs = parse_attributes(attr_start, attr_len);

            /* Push to stack */
            if (div_stack_size >= div_stack_capacity) {
                div_stack_capacity = div_stack_capacity ? div_stack_capacity * 2 : 4;
                div_stack = realloc(div_stack, sizeof(div_stack_item) * div_stack_capacity);
                if (!div_stack) {
                    free(line);
                    for (size_t i = 0; i < div_stack_size; i++) {
                        if (div_stack[i].html_attrs) free(div_stack[i].html_attrs);
                    }
                    free(output);
                    return NULL;
                }
            }

            div_stack[div_stack_size].colon_count = colon_count;
            div_stack[div_stack_size].html_attrs = html_attrs;
            div_stack_size++;

            /* Write opening div tag with markdown="1" to enable markdown parsing inside */
            size_t markdown_attr_len = 13; /*  markdown="1" */
            size_t needed = 5 + (html_attrs ? strlen(html_attrs) : 0) + markdown_attr_len + 1; /* <div...> */
            if (remaining < needed) {
                size_t written = write - output;
                output_capacity = (written + needed) * 2;
                char *new_output = realloc(output, output_capacity);
                if (!new_output) {
                    free(line);
                    for (size_t i = 0; i < div_stack_size; i++) {
                        if (div_stack[i].html_attrs) free(div_stack[i].html_attrs);
                    }
                    if (div_stack) free(div_stack);
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = output_capacity - written;
            }

            if (html_attrs) {
                write += snprintf(write, remaining, "<div%s markdown=\"1\">", html_attrs);
            } else {
                write += snprintf(write, remaining, "<div markdown=\"1\">");
            }
            remaining = output_capacity - (write - output);

            /* Skip the fence line */
            read = line_end;
            if (*read == '\r') read++;
            if (*read == '\n') read++;
        } else if (is_closing && div_stack_size > 0) {
            /* Closing fence - pop from stack */
            div_stack_size--;
            if (div_stack[div_stack_size].html_attrs) {
                free(div_stack[div_stack_size].html_attrs);
            }

            /* Write closing div tag */
            size_t needed = 7; /* </div> */
            if (remaining < needed) {
                size_t written = write - output;
                output_capacity = (written + needed) * 2;
                char *new_output = realloc(output, output_capacity);
                if (!new_output) {
                    free(line);
                    for (size_t i = 0; i < div_stack_size; i++) {
                        if (div_stack[i].html_attrs) free(div_stack[i].html_attrs);
                    }
                    if (div_stack) free(div_stack);
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = output_capacity - written;
            }

            write += snprintf(write, remaining, "</div>");
            remaining = output_capacity - (write - output);

            /* Skip the fence line */
            read = line_end;
            if (*read == '\r') read++;
            if (*read == '\n') read++;
        } else {
            /* Regular line - copy as-is */
            size_t needed = line_len + 1; /* +1 for newline */
            if (remaining < needed) {
                size_t written = write - output;
                output_capacity = (written + needed) * 2;
                char *new_output = realloc(output, output_capacity);
                if (!new_output) {
                    free(line);
                    for (size_t i = 0; i < div_stack_size; i++) {
                        if (div_stack[i].html_attrs) free(div_stack[i].html_attrs);
                    }
                    if (div_stack) free(div_stack);
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = output_capacity - written;
            }

            memcpy(write, read, line_len);
            write += line_len;
            remaining -= line_len;

            /* Copy newline */
            if (*line_end == '\r') {
                *write++ = '\r';
                remaining--;
                line_end++;
            }
            if (*line_end == '\n') {
                *write++ = '\n';
                remaining--;
                line_end++;
            }

            read = line_end;
        }

        free(line);
    }

    /* Close any remaining divs (shouldn't happen in valid input) */
    while (div_stack_size > 0) {
        div_stack_size--;
        if (div_stack[div_stack_size].html_attrs) {
            free(div_stack[div_stack_size].html_attrs);
        }
        size_t needed = 7; /* </div> */
        if (remaining < needed) {
            size_t written = write - output;
            output_capacity = (written + needed) * 2;
            char *new_output = realloc(output, output_capacity);
            if (!new_output) {
                for (size_t i = 0; i < div_stack_size; i++) {
                    if (div_stack[i].html_attrs) free(div_stack[i].html_attrs);
                }
                if (div_stack) free(div_stack);
                free(output);
                return NULL;
            }
            output = new_output;
            write = output + written;
            remaining = output_capacity - written;
        }
        write += snprintf(write, remaining, "</div>");
        remaining = output_capacity - (write - output);
    }

    if (div_stack) free(div_stack);

    *write = '\0';
    return output;
}

