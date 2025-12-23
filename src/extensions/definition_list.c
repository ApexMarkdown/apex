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
#include <ctype.h>
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
 * Extract reference link IDs from text (e.g., "1670-042" from "[KeyRemap4MacBook][1670-042]")
 * Returns a dynamically allocated array of strings (IDs), with NULL terminator
 * Caller must free each string and the array itself
 */
static char **extract_reference_link_ids(const char *text, size_t *count) {
    if (!text || !count) return NULL;

    *count = 0;
    size_t capacity = 16;
    char **ids = malloc(capacity * sizeof(char*));
    if (!ids) return NULL;

    const char *p = text;
    while (*p) {
        if (*p == '[') {
            const char *text_start = p + 1;
            const char *text_end = strchr(text_start, ']');
            if (text_end) {
                if (text_end[1] == '[' && text_end[2] == ']') {
                    /* Found shortcut reference [text][] - use text as the ID */
                    size_t text_len = text_end - text_start;
                    if (text_len > 0) {
                        char *id = malloc(text_len + 1);
                        if (id) {
                            memcpy(id, text_start, text_len);
                            id[text_len] = '\0';

                            /* Check if we already have this ID */
                            bool found = false;
                            for (size_t i = 0; i < *count; i++) {
                                if (strcmp(ids[i], id) == 0) {
                                    found = true;
                                    free(id);
                                    break;
                                }
                            }

                            if (!found) {
                                /* Add to array */
                                if (*count >= capacity) {
                                    capacity *= 2;
                                    char **new_ids = realloc(ids, capacity * sizeof(char*));
                                    if (!new_ids) {
                                        free(id);
                                        break;
                                    }
                                    ids = new_ids;
                                }
                                ids[*count] = id;
                                (*count)++;
                            }
                        }
                    }
                    p = text_end + 3;  /* Skip past ]] */
                    continue;
                } else if (text_end[1] == '[') {
                    /* Found [text][ref] pattern */
                    const char *ref_start = text_end + 2;
                    const char *ref_end = strchr(ref_start, ']');
                    if (ref_end) {
                        /* Extract the reference ID */
                        size_t ref_len = ref_end - ref_start;
                        if (ref_len > 0) {
                            char *id = malloc(ref_len + 1);
                            if (id) {
                                memcpy(id, ref_start, ref_len);
                                id[ref_len] = '\0';

                                /* Check if we already have this ID */
                                bool found = false;
                                for (size_t i = 0; i < *count; i++) {
                                    if (strcmp(ids[i], id) == 0) {
                                        found = true;
                                        free(id);
                                        break;
                                    }
                                }

                                if (!found) {
                                    /* Add to array */
                                    if (*count >= capacity) {
                                        capacity *= 2;
                                        char **new_ids = realloc(ids, capacity * sizeof(char*));
                                        if (!new_ids) {
                                            free(id);
                                            break;
                                        }
                                        ids = new_ids;
                                    }
                                    ids[*count] = id;
                                    (*count)++;
                                }
                            }
                        }
                        p = ref_end + 1;
                        continue;
                    }
                }
            }
        }
        p++;
    }

    /* Add NULL terminator */
    if (*count >= capacity) {
        char **new_ids = realloc(ids, (capacity + 1) * sizeof(char*));
        if (new_ids) ids = new_ids;
    }
    if (ids) ids[*count] = NULL;

    return ids;
}

/**
 * Extract specific reference definitions from the full reference definitions string
 * based on a list of reference IDs
 * Returns a string containing only the needed definitions, or NULL if none found
 * Caller must free the returned string
 */
static char *extract_specific_reference_definitions(const char *all_refs, char **needed_ids) {
    if (!all_refs || !needed_ids || !needed_ids[0]) return NULL;

    size_t result_capacity = 1024;
    size_t result_len = 0;
    char *result = malloc(result_capacity);
    if (!result) return NULL;
    result[0] = '\0';

    const char *p = all_refs;
    while (*p) {
        const char *line_start = p;
        const char *line_end = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);

        /* Skip leading whitespace */
        const char *content_start = line_start;
        while (content_start < line_end && (*content_start == ' ' || *content_start == '\t')) {
            content_start++;
        }

        /* Check if this is a reference link definition: [id]: URL */
        if (content_start < line_end && *content_start == '[') {
            const char *id_end = strchr(content_start + 1, ']');
            if (id_end && id_end < line_end && id_end[1] == ':') {
                /* Extract the ID from this definition */
                size_t def_id_len = id_end - (content_start + 1);
                char *def_id = malloc(def_id_len + 1);
                if (def_id) {
                    memcpy(def_id, content_start + 1, def_id_len);
                    def_id[def_id_len] = '\0';

                    /* Check if this ID is in our needed list */
                    bool needed = false;
                    for (size_t i = 0; needed_ids[i]; i++) {
                        if (strcmp(needed_ids[i], def_id) == 0) {
                            needed = true;
                            break;
                        }
                    }

                    if (needed) {
                        /* Include this definition */
                        size_t line_len = line_end - line_start;
                        if (line_end < p + strlen(p) && *line_end == '\n') {
                            line_len++; /* Include newline */
                        }

                        /* Expand buffer if needed */
                        if (result_len + line_len + 1 >= result_capacity) {
                            result_capacity = (result_len + line_len + 1) * 2;
                            char *new_result = realloc(result, result_capacity);
                            if (!new_result) {
                                free(def_id);
                                break;
                            }
                            result = new_result;
                        }

                        /* Copy the line */
                        memcpy(result + result_len, line_start, line_len);
                        result_len += line_len;
                        result[result_len] = '\0';
                    }

                    free(def_id);
                }
            }
        }

        /* Move to next line */
        p = line_end;
        if (*p == '\n') p++;
    }

    if (result_len == 0) {
        free(result);
        return NULL;
    }

    return result;
}

/**
 * Extract all reference link definitions from the document
 * Returns a string containing all reference definitions, or NULL if none found
 * Caller must free the returned string
 */
static char *extract_reference_definitions(const char *text) {
    if (!text) return NULL;

    size_t text_len = strlen(text);
    size_t refs_capacity = text_len + 1;
    char *refs = malloc(refs_capacity);
    if (!refs) return NULL;
    refs[0] = '\0';

    const char *p = text;
    size_t refs_len = 0;

    while (*p) {
        const char *line_start = p;
        const char *line_end = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);

        /* Skip leading whitespace */
        const char *content_start = line_start;
        while (content_start < line_end && (*content_start == ' ' || *content_start == '\t')) {
            content_start++;
        }

        /* Check if this is a reference link definition: [id]: URL */
        if (content_start < line_end && *content_start == '[') {
            const char *id_end = strchr(content_start + 1, ']');
            if (id_end && id_end < line_end && id_end[1] == ':') {
                /* This is a reference definition - extract the entire line */
                size_t line_len = line_end - line_start;
                if (line_end < p + strlen(p) && *line_end == '\n') {
                    line_len++; /* Include newline */
                }

                /* Check if we need to expand the buffer */
                if (refs_len + line_len + 1 >= refs_capacity) {
                    refs_capacity = (refs_len + line_len + 1) * 2;
                    char *new_refs = realloc(refs, refs_capacity);
                    if (!new_refs) {
                        free(refs);
                        return NULL;
                    }
                    refs = new_refs;
                }

                /* Copy the line */
                memcpy(refs + refs_len, line_start, line_len);
                refs_len += line_len;
                refs[refs_len] = '\0';
            }
        }

        /* Move to next line */
        p = line_end;
        if (*p == '\n') p++;
    }

    if (refs_len == 0) {
        free(refs);
        return NULL;
    }

    return refs;
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
    (void)ext;
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
    (void)ext;
    (void)parser;
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
    (void)ext;
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
char *apex_process_definition_lists(const char *text, bool unsafe) {
    if (!text) return NULL;

    size_t text_len = strlen(text);

    /* Quick scan: check if any definition list patterns exist before processing */
    /* Definition lists start with : (after up to 3 spaces or blockquote) */
    bool has_def_list_pattern = false;
    const char *p = text;
    const char *end = text + text_len;

    while (p < end) {
        /* Look for start of line (beginning of text or after newline) */
        if (p == text || p[-1] == '\n') {
            const char *check = p;
            int spaces = 0;

            /* Skip up to 3 leading spaces */
            while (spaces < 3 && check < end && *check == ' ') {
                spaces++;
                check++;
            }

            /* Skip blockquote prefix (>) */
            while (check < end && *check == '>') {
                check++;
                /* Skip optional space after > */
                if (check < end && (*check == ' ' || *check == '\t')) {
                    check++;
                }
            }

            /* Check if this line starts with : followed by space/tab */
            if (check < end && *check == ':' && (check + 1) < end && (check[1] == ' ' || check[1] == '\t')) {
                has_def_list_pattern = true;
                break;
            }

            /* Move to next line */
            while (p < end && *p != '\n') {
                p++;
            }
            if (p < end) p++; /* Skip the newline */
        } else {
            p++;
        }
    }

    /* Early exit if no definition list patterns found */
    if (!has_def_list_pattern) {
        return NULL;
    }

    size_t output_capacity = text_len * 3;  /* Generous for HTML tags */
    char *output = malloc(output_capacity + 1);  /* +1 for null terminator */
    if (!output) return NULL;

    /* Extract all reference link definitions from the document */
    char *ref_definitions = extract_reference_definitions(text);

    const char *read = text;
    char *write = output;
    /* Reserve 1 byte for null terminator, so we have output_capacity bytes to write */
    size_t remaining = output_capacity;

    /* Helper macro to expand buffer if needed */
    /* Always reserves 1 byte for null terminator */
    #define ENSURE_SPACE(needed) do { \
        /* We need 'needed' bytes for content + 1 for null terminator = needed+1 total */ \
        /* So we need remaining > needed (not >=) to have space for both */ \
        if (remaining <= (needed)) { \
            size_t used = write - output; \
            /* Allocate enough space: used + needed + 1 for null terminator, then double for safety */ \
            size_t min_capacity = used + (needed) + 1; \
            output_capacity = (min_capacity < 1024) ? 2048 : min_capacity * 2; \
            char *new_output = realloc(output, output_capacity + 1); \
            if (!new_output) { \
                free(output); \
                free(ref_definitions); \
                return NULL; \
            } \
            output = new_output; \
            write = output + used; \
            remaining = output_capacity - used; \
        } \
    } while(0)

    bool in_def_list = false;
    bool in_blockquote_context = false;  /* Track if we're processing blockquote-prefixed definition lists */
    int blockquote_depth = 0;  /* Track nesting depth of blockquotes (number of > characters) */
    char term_buffer[4096];
    int term_len = 0;
    bool term_has_blockquote = false;  /* Track if buffered term has blockquote prefix */
    int term_blockquote_depth = 0;  /* Track blockquote depth of buffered term */
    bool found_any_def_list = false;  /* Track if we actually created any definition lists */

    const char *prev_read_pos = NULL;
    int iteration_count = 0;
    const int MAX_ITERATIONS = 1000000;  /* Safety limit */

    while (*read) {
        /* Safety: prevent infinite loops */
        if (++iteration_count > MAX_ITERATIONS) {
            /* Something is wrong - return original text to avoid hanging */
            free(output);
            free(ref_definitions);
            return strdup(text);
        }

        /* Safety: if we haven't advanced, break to prevent infinite loop */
        if (prev_read_pos == read) {
            break;
        }
        prev_read_pos = read;

        const char *line_start = read;
        const char *line_end = strchr(read, '\n');
        if (!line_end) {
            /* No newline found - we're at the last line */
            /* Find the end by looking for null terminator */
            line_end = read;
            while (*line_end != '\0') line_end++;
            /* If line_end == read, we're at the end - break */
            if (line_end == read) {
                break;
            }
        }

        size_t line_length = line_end - line_start;

        /* Skip table rows (lines that start with |) */
        const char *p = line_start;
        while (p < line_end && (*p == ' ' || *p == '\t')) p++;
        bool is_table_row = (p < line_end && *p == '|');

        /* Check if line is a list item (starts with -, *, +, or number.) */
        const char *list_check = line_start;
        int list_spaces = 0;
        while (list_check < line_end && (*list_check == ' ' || *list_check == '\t') && list_spaces < 4) {
            list_spaces++;
            list_check++;
        }
        bool is_list_item = false;
        if (list_check < line_end) {
            if (*list_check == '-' || *list_check == '*' || *list_check == '+') {
                /* Check if followed by space or tab */
                if (list_check + 1 < line_end && (list_check[1] == ' ' || list_check[1] == '\t')) {
                    is_list_item = true;
                }
            } else if (*list_check >= '0' && *list_check <= '9') {
                /* Check for numbered list (digit followed by . and space) */
                const char *num_check = list_check;
                while (num_check < line_end && *num_check >= '0' && *num_check <= '9') {
                    num_check++;
                }
                if (num_check < line_end && *num_check == '.' &&
                    num_check + 1 < line_end && (num_check[1] == ' ' || num_check[1] == '\t')) {
                    is_list_item = true;
                }
            }
        }

        /* Check if line starts with : (definition) */
        /* Also handle blockquote prefixes: > : or >: */
        p = line_start;
        int spaces = 0;
        while (*p == ' ' && spaces < 3 && p < line_end) {
            spaces++;
            p++;
        }

        /* Check for blockquote prefix (may be nested: > > >) */
        bool has_blockquote_prefix = false;
        int current_blockquote_depth = 0;
        while (p < line_end && *p == '>') {
            has_blockquote_prefix = true;
            current_blockquote_depth++;
            p++;
            /* Skip optional space after > */
            if (p < line_end && (*p == ' ' || *p == '\t')) {
                p++;
            }
        }

        bool is_def_line = false;
        /* Only check for definition line if it's not a table row or list item */
        /* Definition lines must start with : (after whitespace/blockquote), not contain : */
        if (!is_table_row && !is_list_item && p < line_end && *p == ':' && (p + 1) < line_end &&
            (p[1] == ' ' || p[1] == '\t')) {
            /* Double-check: make sure : is at the start of the line content (after whitespace/blockquote) */
            /* p already points after whitespace and blockquote, so if *p == ':', it's a definition line */
            is_def_line = true;
        }

        if (is_def_line) {
            /* Definition line */
            if (!in_def_list) {
                found_any_def_list = true;  /* We're creating a definition list */
                /* Check if this definition list is in a blockquote context */
                in_blockquote_context = has_blockquote_prefix || term_has_blockquote;
                /* Use the maximum depth from current line or buffered term */
                blockquote_depth = term_has_blockquote ? term_blockquote_depth : current_blockquote_depth;
                if (has_blockquote_prefix && current_blockquote_depth > blockquote_depth) {
                    blockquote_depth = current_blockquote_depth;
                }

                /* Start new definition list */
                const char *dl_start = "<dl>\n";
                size_t dl_len = strlen(dl_start);
                    if (in_blockquote_context) {
                        /* Add > prefix(es) at start of line for blockquote context */
                        size_t prefix_needed = blockquote_depth * 2;
                        /* Need prefix_needed + 1 for null terminator */
                        ENSURE_SPACE(prefix_needed + 1);
                        for (int i = 0; i < blockquote_depth && remaining > 2; i++) {
                            *write++ = '>';
                            *write++ = ' ';
                            remaining -= 2;
                        }
                    }
                /* Need dl_len + 1 for null terminator */
                ENSURE_SPACE(dl_len + 1);
                memcpy(write, dl_start, dl_len);
                write += dl_len;
                remaining -= dl_len;

                /* Write term from buffer */
                if (term_len > 0) {
                    /* Strip blockquote prefix from term if present */
                    const char *term_content = term_buffer;
                    int term_content_len = term_len;
                    if (term_has_blockquote) {
                        /* Skip > and optional space */
                        term_content = term_buffer;
                        while (term_content < term_buffer + term_len &&
                               (*term_content == '>' || *term_content == ' ' || *term_content == '\t')) {
                            term_content++;
                            term_content_len--;
                        }
                    }

                    if (in_blockquote_context) {
                        /* Add > prefix(es) at start of line for blockquote context */
                        size_t prefix_needed = blockquote_depth * 2;
                        /* Need prefix_needed + 1 for null terminator */
                        ENSURE_SPACE(prefix_needed + 1);
                        for (int i = 0; i < blockquote_depth && remaining > 2; i++) {
                            *write++ = '>';
                            *write++ = ' ';
                            remaining -= 2;
                        }
                    }

                    const char *dt_start = "<dt>";
                    size_t dt_start_len = strlen(dt_start);
                    /* Need dt_start_len + 1 for null terminator */
                    ENSURE_SPACE(dt_start_len + 1);
                    memcpy(write, dt_start, dt_start_len);
                    write += dt_start_len;
                    remaining -= dt_start_len;

                    /* Parse term text as inline Markdown */
                    char *term_html = NULL;
                    if (term_content_len > 0) {
                        /* Quick check: does this text contain any markdown syntax? */
                        bool has_markdown = false;
                        const char *p = term_content;
                        const char *end = term_content + term_content_len;
                        while (p < end) {
                            char c = *p++;
                            /* Check for common markdown patterns */
                            if (c == '*' || c == '_' || c == '[' || c == ']' || c == '!' ||
                                c == '`' || c == '\\' || (c == '<' && p < end && (*p == '!' || isalnum((unsigned char)*p)))) {
                                has_markdown = true;
                                break;
                            }
                        }

                        if (!has_markdown) {
                            /* Plain text - just HTML escape */
                            size_t escaped_len = 0;
                            for (const char *p = term_content; p < term_content + term_content_len; p++) {
                                if (*p == '&') escaped_len += 5;  /* &amp; */
                                else if (*p == '<' || *p == '>') escaped_len += 4;  /* &lt; &gt; */
                                else if (*p == '"') escaped_len += 6;  /* &quot; */
                                else escaped_len += 1;
                            }
                            term_html = malloc(escaped_len + 1);
                            if (term_html) {
                                char *out = term_html;
                                for (const char *p = term_content; p < term_content + term_content_len; p++) {
                                    if (*p == '&') { memcpy(out, "&amp;", 5); out += 5; }
                                    else if (*p == '<') { memcpy(out, "&lt;", 4); out += 4; }
                                    else if (*p == '>') { memcpy(out, "&gt;", 4); out += 4; }
                                    else if (*p == '"') { memcpy(out, "&quot;", 6); out += 6; }
                                    else *out++ = *p;
                                }
                                *out = '\0';
                            }
                        } else {
                            /* Note: We don't prepend reference definitions here because:
                             * 1. It causes cmark to hang on large files with many references
                             * 2. Reference definitions are already available in the main document context
                             * 3. Inline parsing should work with references from the main document
                             */
                            char *term_text = malloc(term_content_len + 1);
                            if (term_text) {
                                memcpy(term_text, term_content, term_content_len);
                                term_text[term_content_len] = '\0';

                                /* Parse as Markdown and render to HTML */
                                int parser_opts = CMARK_OPT_DEFAULT | CMARK_OPT_SMART;  /* Enable smart typography */
                                int render_opts = CMARK_OPT_DEFAULT;
                                if (unsafe) {
                                    parser_opts |= CMARK_OPT_UNSAFE;
                                    render_opts |= CMARK_OPT_UNSAFE;
                                }

                            /* Extract only the reference definitions actually used in this term */
                            size_t final_term_len = term_content_len;
                            char *final_term_text = term_text;
                            if (ref_definitions) {
                                size_t id_count = 0;
                                char **needed_ids = extract_reference_link_ids(term_text, &id_count);
                                if (needed_ids && id_count > 0) {
                                    char *selected_refs = extract_specific_reference_definitions(ref_definitions, needed_ids);
                                    if (selected_refs) {
                                        size_t ref_len = strlen(selected_refs);
                                        size_t new_size = ref_len + term_content_len + 2;  /* +2 for newline and null */
                                        char *new_term_text = malloc(new_size);
                                        if (new_term_text) {
                                            size_t offset = 0;
                                            memcpy(new_term_text, selected_refs, ref_len);
                                            offset = ref_len;
                                            if (ref_len > 0 && selected_refs[ref_len - 1] != '\n') {
                                                new_term_text[offset++] = '\n';
                                            }
                                            memcpy(new_term_text + offset, term_text, term_content_len);
                                            new_term_text[offset + term_content_len] = '\0';
                                            free(term_text);
                                            final_term_text = new_term_text;
                                            final_term_len = offset + term_content_len;
                                        }
                                        free(selected_refs);
                                    }
                                    /* Free the IDs array */
                                    for (size_t i = 0; i < id_count; i++) {
                                        free(needed_ids[i]);
                                    }
                                    free(needed_ids);
                                }
                            }
                            cmark_parser *temp_parser = cmark_parser_new(parser_opts);
                            if (temp_parser) {
                                cmark_parser_feed(temp_parser, final_term_text, final_term_len);
                                cmark_node *doc = cmark_parser_finish(temp_parser);
                                if (doc) {
                                    /* Render and extract just the content (strip <p> tags) */
                                    char *full_html = cmark_render_html(doc, render_opts, NULL);
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
                                        term_html = strdup(content_start);
                                        free(full_html);
                                    }
                                    cmark_node_free(doc);
                                }
                                cmark_parser_free(temp_parser);
                            }
                            free(final_term_text);
                        }
                        }
                    }

                    /* Write processed HTML or original text */
                    if (term_html) {
                        size_t html_len = strlen(term_html);
                        /* Need html_len + 1 for null terminator */
                        ENSURE_SPACE(html_len + 1);
                        memcpy(write, term_html, html_len);
                        write += html_len;
                        remaining -= html_len;
                        free(term_html);
                    } else {
                        /* Need term_content_len + 1 for null terminator */
                        ENSURE_SPACE((size_t)term_content_len + 1);
                        memcpy(write, term_content, term_content_len);
                        write += term_content_len;
                        remaining -= (size_t)term_content_len;
                    }

                    const char *dt_end = "</dt>\n";
                    size_t dt_end_len = strlen(dt_end);
                    /* Need dt_end_len + 1 for null terminator */
                    ENSURE_SPACE(dt_end_len + 1);
                    memcpy(write, dt_end, dt_end_len);
                    write += dt_end_len;
                    remaining -= dt_end_len;

                    term_len = 0;
                    term_has_blockquote = false;
                }

                in_def_list = true;
            }

            /* Write definition */
            if (in_blockquote_context) {
                /* Add > prefix(es) at start of line for blockquote context */
                size_t prefix_needed = blockquote_depth * 2;
                /* Need prefix_needed + 1 for null terminator */
                ENSURE_SPACE(prefix_needed + 1);
                for (int i = 0; i < blockquote_depth && remaining > 2; i++) {
                    *write++ = '>';
                    *write++ = ' ';
                    remaining -= 2;
                }
            }

            const char *dd_start = "<dd>";
            size_t dd_start_len = strlen(dd_start);
            /* Need dd_start_len + 1 for null terminator */
            ENSURE_SPACE(dd_start_len + 1);
            memcpy(write, dd_start, dd_start_len);
            write += dd_start_len;
            remaining -= dd_start_len;

            /* Extract definition text (after : and space) */
            p++;  /* Skip : */
            while (p < line_end && (*p == ' ' || *p == '\t')) p++;

            size_t def_text_len = line_end - p;

            /* Parse definition text as inline Markdown */
            char *def_html = NULL;
            if (def_text_len > 0) {
                /* Quick check: does this text contain any markdown syntax? */
                bool has_markdown = false;
                const char *check_p = p;
                const char *check_end = p + def_text_len;
                while (check_p < check_end) {
                    char c = *check_p++;
                    /* Check for common markdown patterns */
                    if (c == '*' || c == '_' || c == '[' || c == ']' || c == '!' ||
                        c == '`' || c == '\\' || (c == '<' && check_p < check_end && (*check_p == '!' || isalnum((unsigned char)*check_p)))) {
                        has_markdown = true;
                        break;
                    }
                }

                if (!has_markdown) {
                    /* Plain text - just HTML escape */
                    size_t escaped_len = 0;
                    for (const char *esc_p = p; esc_p < p + def_text_len; esc_p++) {
                        if (*esc_p == '&') escaped_len += 5;  /* &amp; */
                        else if (*esc_p == '<' || *esc_p == '>') escaped_len += 4;  /* &lt; &gt; */
                        else if (*esc_p == '"') escaped_len += 6;  /* &quot; */
                        else escaped_len += 1;
                    }
                    def_html = malloc(escaped_len + 1);
                    if (def_html) {
                        char *out = def_html;
                        for (const char *esc_p = p; esc_p < p + def_text_len; esc_p++) {
                            if (*esc_p == '&') { memcpy(out, "&amp;", 5); out += 5; }
                            else if (*esc_p == '<') { memcpy(out, "&lt;", 4); out += 4; }
                            else if (*esc_p == '>') { memcpy(out, "&gt;", 4); out += 4; }
                            else if (*esc_p == '"') { memcpy(out, "&quot;", 6); out += 6; }
                            else *out++ = *esc_p;
                        }
                        *out = '\0';
                    }
                } else {
                    char *def_text = malloc(def_text_len + 1);
                    if (def_text) {
                        memcpy(def_text, p, def_text_len);
                        def_text[def_text_len] = '\0';

                        /* Parse as Markdown and render to HTML */
                        int parser_opts = CMARK_OPT_DEFAULT | CMARK_OPT_SMART;  /* Enable smart typography */
                        int render_opts = CMARK_OPT_DEFAULT;
                        if (unsafe) {
                            parser_opts |= CMARK_OPT_UNSAFE;
                            render_opts |= CMARK_OPT_UNSAFE;
                        }

                    /* Extract only the reference definitions actually used in this definition */
                    size_t final_def_len = def_text_len;
                    char *final_def_text = def_text;
                    if (ref_definitions) {
                        size_t id_count = 0;
                        char **needed_ids = extract_reference_link_ids(def_text, &id_count);
                        if (needed_ids && id_count > 0) {
                            char *selected_refs = extract_specific_reference_definitions(ref_definitions, needed_ids);
                            if (selected_refs) {
                                size_t ref_len = strlen(selected_refs);
                                size_t new_size = ref_len + def_text_len + 2;  /* +2 for newline and null */
                                char *new_def_text = malloc(new_size);
                                if (new_def_text) {
                                    size_t offset = 0;
                                    memcpy(new_def_text, selected_refs, ref_len);
                                    offset = ref_len;
                                    if (ref_len > 0 && selected_refs[ref_len - 1] != '\n') {
                                        new_def_text[offset++] = '\n';
                                    }
                                    memcpy(new_def_text + offset, def_text, def_text_len);
                                    new_def_text[offset + def_text_len] = '\0';
                                    free(def_text);
                                    final_def_text = new_def_text;
                                    final_def_len = offset + def_text_len;
                                }
                                free(selected_refs);
                            }
                            /* Free the IDs array */
                            for (size_t i = 0; i < id_count; i++) {
                                free(needed_ids[i]);
                            }
                            free(needed_ids);
                        }
                    }

                    cmark_parser *temp_parser = cmark_parser_new(parser_opts);
                    if (temp_parser) {
                        cmark_parser_feed(temp_parser, final_def_text, final_def_len);
                        cmark_node *doc = cmark_parser_finish(temp_parser);
                        if (doc) {
                            /* Render and extract just the content (strip <p> tags) */
                            char *full_html = cmark_render_html(doc, render_opts, NULL);
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
                    free(final_def_text);
                }
                }
            }

            /* Write processed HTML or original text */
            if (def_html) {
                size_t html_len = strlen(def_html);
                /* Need html_len + 1 for null terminator */
                ENSURE_SPACE(html_len + 1);
                memcpy(write, def_html, html_len);
                write += html_len;
                remaining -= html_len;
                free(def_html);
            } else {
                /* Need def_text_len + 1 for null terminator */
                ENSURE_SPACE(def_text_len + 1);
                memcpy(write, p, def_text_len);
                write += def_text_len;
                remaining -= def_text_len;
            }

            const char *dd_end = "</dd>\n";
            size_t dd_end_len = strlen(dd_end);
            /* Need dd_end_len + 1 for null terminator */
            ENSURE_SPACE(dd_end_len + 1);
            memcpy(write, dd_end, dd_end_len);
            write += dd_end_len;
            remaining -= dd_end_len;
        } else if (line_length == 0 || (line_length == 1 && *line_start == '\r')) {
            /* Blank line */
            if (in_def_list) {
                /* End definition list */
                const char *dl_end = "</dl>\n\n";
                size_t dl_end_len = strlen(dl_end);
                if (in_blockquote_context && dl_end_len < remaining) {
                    /* Add > prefix(es) at start of line for blockquote context */
                    size_t prefix_needed = blockquote_depth * 2;
                    ENSURE_SPACE(prefix_needed + 1);
                    for (int i = 0; i < blockquote_depth && remaining > 2; i++) {
                        *write++ = '>';
                        *write++ = ' ';
                        remaining -= 2;
                    }
                }
                if (dl_end_len < remaining) {
                    memcpy(write, dl_end, dl_end_len);
                    write += dl_end_len;
                    remaining -= dl_end_len;
                }
                in_def_list = false;
                in_blockquote_context = false;
                blockquote_depth = 0;
                term_len = 0;
                term_has_blockquote = false;
                term_blockquote_depth = 0;
            } else {
                /* Flush any buffered term before writing blank line */
                if (term_len > 0) {
                    /* Need term_len bytes + 1 for newline + 1 for null terminator */
                    ENSURE_SPACE((size_t)term_len + 2);
                    memcpy(write, term_buffer, term_len);
                    write += term_len;
                    remaining -= (size_t)term_len;
                    *write++ = '\n';
                    remaining--;
                    term_len = 0;
                }
                /* Regular blank line */
                ENSURE_SPACE(1);
                *write++ = '\n';
                remaining--;
            }
        } else {
            /* Regular line */
            if (in_def_list) {
                /* This could be a new term */
                /* End current list first */
                const char *dl_end = "</dl>\n\n";
                size_t dl_end_len = strlen(dl_end);
                    if (in_blockquote_context) {
                        /* Add > prefix(es) at start of line for blockquote context */
                        size_t prefix_needed = blockquote_depth * 2;
                        /* Need prefix_needed + 1 for null terminator */
                        ENSURE_SPACE(prefix_needed + 1);
                        for (int i = 0; i < blockquote_depth && remaining > 2; i++) {
                            *write++ = '>';
                            *write++ = ' ';
                            remaining -= 2;
                        }
                    }
                /* Need dl_end_len + 1 for null terminator */
                ENSURE_SPACE(dl_end_len + 1);
                memcpy(write, dl_end, dl_end_len);
                write += dl_end_len;
                remaining -= dl_end_len;
                in_def_list = false;
                in_blockquote_context = false;
                blockquote_depth = 0;
            }

            /* If we have a buffered term that wasn't used, write it first */
            if (term_len > 0) {
                /* Need term_len bytes + 1 for newline + 1 for null terminator */
                ENSURE_SPACE((size_t)term_len + 2);
                memcpy(write, term_buffer, term_len);
                write += term_len;
                remaining -= (size_t)term_len;
                *write++ = '\n';
                remaining--;
                term_len = 0;
            }

            /* Check if line is a header (starts with #) - check BEFORE other checks */
            const char *header_check = line_start;
            while (header_check < line_end && (*header_check == ' ' || *header_check == '\t')) {
                header_check++;
            }
            bool is_header = (header_check < line_end && *header_check == '#');

            /* If this is a table row, write it through immediately without buffering */
            if (is_table_row) {
                size_t needed = line_length + (*line_end == '\n' ? 1 : 0);
                /* Need needed + 1 for null terminator */
                ENSURE_SPACE(needed + 1);
                memcpy(write, line_start, line_length);
                write += line_length;
                remaining -= line_length;
                if (*line_end == '\n' && remaining > 0) {
                    *write++ = '\n';
                    remaining--;
                }
                /* Move to next line and continue */
                const char *old_read_continue = read;
                read = line_end;
                if (*read == '\n') read++;
                /* Safety: ensure we advanced */
                if (read <= old_read_continue) {
                    /* We're stuck - break instead of continue */
                    break;
                }
                continue;
            }
            /* If this is a header, write it through immediately without buffering */
            else if (is_header) {
                /* But first, flush any buffered term that wasn't used */
                if (term_len > 0) {
                    size_t term_needed = (size_t)term_len + 2;
                    ENSURE_SPACE(term_needed);
                    memcpy(write, term_buffer, term_len);
                    write += term_len;
                    remaining -= (size_t)term_len;
                    *write++ = '\n';
                    remaining--;
                    term_len = 0;
                }
                size_t needed = line_length + (*line_end == '\n' ? 1 : 0);
                /* Need needed + 1 for null terminator */
                ENSURE_SPACE(needed + 1);
                memcpy(write, line_start, line_length);
                write += line_length;
                remaining -= line_length;
                if (*line_end == '\n' && remaining > 0) {
                    *write++ = '\n';
                    remaining--;
                }
                /* Move to next line and continue */
                const char *old_read_continue = read;
                read = line_end;
                if (*read == '\n') read++;
                /* Safety: ensure we advanced */
                if (read <= old_read_continue) {
                    /* We're stuck - break instead of continue */
                    break;
                }
                continue;
            }
            /* If this is a list item, write it through immediately without buffering */
            else if (is_list_item) {
                size_t needed = line_length + (*line_end == '\n' ? 1 : 0);
                /* Need needed + 1 for null terminator */
                ENSURE_SPACE(needed + 1);
                memcpy(write, line_start, line_length);
                write += line_length;
                remaining -= line_length;
                if (*line_end == '\n' && remaining > 0) {
                    *write++ = '\n';
                    remaining--;
                }
                /* Move to next line and continue */
                const char *old_read_continue = read;
                read = line_end;
                if (*read == '\n') read++;
                /* Safety: ensure we advanced */
                if (read <= old_read_continue) {
                    /* We're stuck - break instead of continue */
                    break;
                }
                continue;
            }
            /* Check if line contains IAL syntax - if so, write immediately without buffering */
            else if (strstr(line_start, "{:") != NULL) {
                /* Contains IAL - don't buffer it */
                size_t needed = line_length + (*line_end == '\n' ? 1 : 0);
                /* Need needed + 1 for null terminator */
                ENSURE_SPACE(needed + 1);
                memcpy(write, line_start, line_length);
                write += line_length;
                remaining -= line_length;
                if (*line_end == '\n' && remaining > 0) {
                    *write++ = '\n';
                    remaining--;
                }
                /* Move to next line and continue */
                const char *old_read_continue = read;
                read = line_end;
                if (*read == '\n') read++;
                /* Safety: ensure we advanced */
                if (read <= old_read_continue) {
                    /* We're stuck - break instead of continue */
                    break;
                }
                continue;
            }
            /* Save current line as potential term */
            if (line_length < sizeof(term_buffer) - 1) {
                /* Check if line has blockquote prefix and count depth */
                const char *term_check = line_start;
                while (term_check < line_end && (*term_check == ' ' || *term_check == '\t')) term_check++;
                term_has_blockquote = false;
                term_blockquote_depth = 0;
                const char *depth_check = term_check;
                while (depth_check < line_end && *depth_check == '>') {
                    term_has_blockquote = true;
                    term_blockquote_depth++;
                    depth_check++;
                    /* Skip optional space after > */
                    if (depth_check < line_end && (*depth_check == ' ' || *depth_check == '\t')) {
                        depth_check++;
                    }
                }

                memcpy(term_buffer, line_start, line_length);
                term_len = line_length;
                term_buffer[term_len] = '\0';
                /* Don't write yet - wait to see if next line is definition */
            } else {
                /* Line too long for buffer, just copy through */
                size_t needed = line_length + (*line_end == '\n' ? 1 : 0);
                /* Need needed + 1 for null terminator */
                ENSURE_SPACE(needed + 1);
                memcpy(write, line_start, line_length);
                write += line_length;
                remaining -= line_length;
                if (*line_end == '\n' && remaining > 0) {
                    *write++ = '\n';
                    remaining--;
                }
            }
        }

        /* Move to next line - ensure we always advance */
        const char *old_read = read;
        read = line_end;
        if (*read == '\n') {
            read++;
        }
        /* If we're at the end of the string, break */
        if (*read == '\0') {
            break;
        }
        /* Critical safety check: if we haven't advanced, break immediately */
        if (read <= old_read) {
            /* We're stuck - this should never happen, but break to prevent infinite loop */
            /* Force advance one character as last resort */
            if (*read != '\0') {
                read++;
            } else {
                break;
            }
        }
        /* Additional safety: if we've processed more than the text length, something is wrong */
        if (read > text + text_len) {
            break;
        }
    }

    /* Close any open definition list */
    if (in_def_list) {
        const char *dl_end = "</dl>\n";
        size_t dl_end_len = strlen(dl_end);
        if (in_blockquote_context) {
            /* Add > prefix(es) at start of line for blockquote context */
            size_t prefix_needed = blockquote_depth * 2;
            /* Need prefix_needed + 1 for null terminator */
            ENSURE_SPACE(prefix_needed + 1);
            for (int i = 0; i < blockquote_depth && remaining > 2; i++) {
                *write++ = '>';
                *write++ = ' ';
                remaining -= 2;
            }
        }
        /* Need dl_end_len + 1 for null terminator */
        ENSURE_SPACE(dl_end_len + 1);
        memcpy(write, dl_end, dl_end_len);
        write += dl_end_len;
        remaining -= dl_end_len;
    }

    /* Write any remaining term */
    if (term_len > 0) {
        /* Need term_len bytes + 1 for newline + 1 for null terminator */
        ENSURE_SPACE((size_t)term_len + 2);
        memcpy(write, term_buffer, term_len);
        write += term_len;
        remaining -= (size_t)term_len;
        *write++ = '\n';
        remaining--;
    }

    /* Free reference definitions if we extracted them */
    if (ref_definitions) {
        free(ref_definitions);
    }

    /* Ensure space for null terminator */
    if (remaining < 1) {
        size_t used = write - output;
        output_capacity = (used + 2) * 2;
        char *new_output = realloc(output, output_capacity + 1);
        if (!new_output) {
            free(output);
            return NULL;
        }
        output = new_output;
        write = output + used;
        remaining = output_capacity - used;
    }
    /* Safety check: if we processed but didn't actually create any definition lists,
     * return NULL to use original text. This handles cases where the early exit
     * incorrectly detected a pattern but no definition lists were actually created. */
    if (!found_any_def_list) {
        /* No definition lists were created - if we processed but didn't create any DLs,
         * something went wrong. Return NULL to use original text. */
        free(output);
        free(ref_definitions);
        return NULL;
    }

    *write = '\0';

    #undef ENSURE_SPACE

    /* If we didn't write anything, return original text to avoid empty output */
    if (write == output) {
        free(output);
        free(ref_definitions);
        return NULL;  /* Return NULL to indicate no processing was done */
    }

    return output;
}

/**
 * Post-process - no longer needed with preprocessing approach
 */
static cmark_node *postprocess(cmark_syntax_extension *ext,
                               cmark_parser *parser,
                               cmark_node *root) {
    (void)ext;
    (void)parser;
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
    (void)ext;
    (void)options;
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
