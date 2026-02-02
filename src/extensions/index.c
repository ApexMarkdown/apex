/**
 * Index Extension for Apex
 * Implementation
 */

#include "index.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

/* Index placeholder prefix - we'll use a unique marker */
#define INDEX_PLACEHOLDER_PREFIX "<!--IDX:"
#define INDEX_PLACEHOLDER_SUFFIX "-->"

/**
 * Check if character is valid in index term
 * Index terms can contain letters, digits, spaces, and common punctuation
 */
static bool is_valid_index_char(char c) {
    return isalnum(c) || c == ' ' || c == '-' || c == '_' || c == '/' ||
           c == '.' || c == ',' || c == ':' || c == ';' || c == '\'' || c == '"';
}

/**
 * Trim whitespace from string (in-place)
 */
static char *trim_string(char *str) {
    if (!str) return NULL;

    /* Trim leading whitespace */
    while (*str && isspace((unsigned char)*str)) str++;

    /* Trim trailing whitespace */
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return str;
}

/**
 * Parse mmark index syntax: (!item), (!item, subitem), (!!item, subitem)
 * Returns length consumed, or 0 if not a match
 */
static int parse_mmark_index(const char *text, int pos, int len,
                             apex_index_entry **entry_out) {
    if (pos + 3 >= len) return 0;

    const char *p = text + pos;

    /* Must start with (! */
    if (*p != '(' || p[1] != '!') return 0;

    p += 2;  /* Skip (! */

    /* Check for primary entry (!!) */
    bool primary = false;
    if (*p == '!') {
        primary = true;
        p++;
    }

    /* Extract item */
    const char *item_start = p;
    while (p < text + len && *p != ',' && *p != ')') {
        if (!is_valid_index_char(*p) && *p != '!') {
            return 0;  /* Invalid character */
        }
        p++;
    }

    if (p == item_start) return 0;  /* No item found */

    size_t item_len = p - item_start;
    char *item = malloc(item_len + 1);
    if (!item) return 0;
    memcpy(item, item_start, item_len);
    item[item_len] = '\0';
    trim_string(item);

    if (strlen(item) == 0) {
        free(item);
        return 0;
    }

    char *subitem = NULL;

    /* Check for subitem */
    if (*p == ',') {
        p++;  /* Skip comma */
        while (p < text + len && isspace((unsigned char)*p)) p++;  /* Skip whitespace */

        const char *subitem_start = p;
        while (p < text + len && *p != ')') {
            if (!is_valid_index_char(*p)) {
                free(item);
                return 0;  /* Invalid character */
            }
            p++;
        }

        if (p > subitem_start) {
            size_t subitem_len = p - subitem_start;
            subitem = malloc(subitem_len + 1);
            if (subitem) {
                memcpy(subitem, subitem_start, subitem_len);
                subitem[subitem_len] = '\0';
                trim_string(subitem);
            }
        }
    }

    /* Must end with ) */
    if (p >= text + len || *p != ')') {
        free(item);
        free(subitem);
        return 0;
    }
    p++;  /* Skip ) */

    /* Create index entry */
    apex_index_entry *entry = apex_index_entry_new(item, APEX_INDEX_MMARK);
    if (entry) {
        entry->subitem = subitem;
        entry->primary = primary;
        *entry_out = entry;
    } else {
        free(item);
        free(subitem);
    }

    return p - (text + pos);
}

/**
 * Parse TextIndex syntax: {^}, [term]{^}, {^params}
 * Returns length consumed, or 0 if not a match
 *
 * TextIndex syntax is: word{^} or [term]{^} or {^params}
 * We look for {^ pattern and extract the term from before it
 */
static int parse_textindex(const char *text, int pos, int len,
                           apex_index_entry **entry_out) {
    if (pos + 2 >= len) return 0;

    const char *p = text + pos;

    /* Look for {^ pattern */
    if (*p != '{' || p[1] != '^') return 0;

    const char *brace_start = p;
    p += 2;  /* Skip {^ */

    /* Extract parameters from {^params} */
    const char *params_start = p;
    while (p < text + len && *p != '}' && *p != '\n') {
        p++;
    }

    if (p >= text + len || *p != '}') return 0;

    size_t params_len = p - params_start;
    char *params = NULL;
    if (params_len > 0) {
        params = malloc(params_len + 1);
        if (params) {
            memcpy(params, params_start, params_len);
            params[params_len] = '\0';
        }
    }

    p++;  /* Skip } */
    int consumed = p - (text + pos);

    /* Check for explicit term before {^: [term]{^} */
    char *term = NULL;
    if (brace_start > text && brace_start[-1] == ']') {
        /* Look backwards for [ */
        const char *bracket_start = brace_start - 1;
        int lookback = 0;
        while (bracket_start > text && *bracket_start != '[' && lookback < 200) {
            bracket_start--;
            lookback++;
        }

        if (*bracket_start == '[') {
            /* Term is content between [ and ], excluding the brackets */
            size_t term_len = (brace_start - 1) - (bracket_start + 1);
            if (term_len > 0 && term_len < 200) {
                term = malloc(term_len + 1);
                if (term) {
                    memcpy(term, bracket_start + 1, term_len);
                    term[term_len] = '\0';
                    trim_string(term);
                }
            }
        }
    }

    /* If no explicit term, extract word/phrase before {^ */
    if (!term || strlen(term) == 0) {
        const char *word_end = brace_start;
        /* Skip backwards over whitespace */
        while (word_end > text && isspace((unsigned char)word_end[-1])) {
            word_end--;
        }

        /* Extract word/phrase (up to 50 chars backwards) */
        const char *word_start = word_end;
        int word_chars = 0;
        while (word_start > text && word_chars < 50) {
            char c = word_start[-1];
            if (isalnum(c) || c == ' ' || c == '-' || c == '_') {
                word_start--;
                word_chars++;
            } else {
                break;
            }
        }

        if (word_chars > 0) {
            size_t term_len = word_end - word_start;
            if (term_len > 0) {
                free(term);  /* Free if we allocated empty term above */
                term = malloc(term_len + 1);
                if (term) {
                    memcpy(term, word_start, term_len);
                    term[term_len] = '\0';
                    trim_string(term);
                }
            }
        }
    }

    if (!term || strlen(term) == 0) {
        free(params);
        return 0;  /* No term found */
    }

    /* Parse params for subitem (simplified - TextIndex has complex param syntax) */
    char *subitem = NULL;
    if (params) {
        /* For now, if params contain a space or comma, treat as subitem */
        char *space = strchr(params, ' ');
        char *comma = strchr(params, ',');
        if (space || comma) {
            const char *sub_start = (space && (!comma || space < comma)) ? space + 1 : comma + 1;
            while (*sub_start && isspace((unsigned char)*sub_start)) sub_start++;
            if (*sub_start) {
                subitem = strdup(sub_start);
                trim_string(subitem);
            }
        }
    }

    /* Create index entry */
    apex_index_entry *entry = apex_index_entry_new(term, APEX_INDEX_TEXTINDEX);
    if (entry) {
        entry->subitem = subitem;
        *entry_out = entry;
    } else {
        free(term);
        free(subitem);
    }

    free(params);

    return consumed;
}

/**
 * Strip Leanpub formatting (*italics*, **bold*) from index term for display
 */
static void strip_leanpub_formatting(char *str) {
    if (!str) return;

    char *w = str;
    const char *r = str;

    while (*r) {
        if (*r == '*') {
            /* Skip * or ** */
            if (r[1] == '*') {
                r += 2;
            } else {
                r++;
            }
            continue;
        }
        *w++ = *r++;
    }
    *w = '\0';
}

/**
 * Parse Leanpub index syntax: {i: term}, {i: "term"}, {i: "Main!sub"}
 * See https://help.leanpub.com/en/articles/6961502-how-to-create-an-index-in-a-leanpub-book
 * Returns length consumed, or 0 if not a match
 */
static int parse_leanpub_index(const char *text, int pos, int len,
                               apex_index_entry **entry_out) {
    if (pos + 4 >= len) return 0;

    const char *p = text + pos;

    /* Must start with {i: */
    if (p[0] != '{' || p[1] != 'i' || p[2] != ':') return 0;
    p += 3;

    /* Skip space after colon */
    while (p < text + len && (*p == ' ' || *p == '\t')) p++;
    if (p >= text + len) return 0;

    char *item = NULL;
    char *subitem = NULL;

    if (*p == '"') {
        /* Quoted: {i: "term"} or {i: "Main!sub"} */
        p++;  /* Skip opening quote */
        const char *start = p;

        while (p < text + len && *p != '"') {
            if (*p == '\\' && p + 1 < text + len) {
                p += 2;  /* Skip escaped char */
            } else {
                p++;
            }
        }
        if (p >= text + len || *p != '"') return 0;

        size_t term_len = p - start;
        char *term = malloc(term_len + 1);
        if (!term) return 0;
        memcpy(term, start, term_len);
        term[term_len] = '\0';

        /* Parse Main!sub hierarchy */
        char *excl = strchr(term, '!');
        if (excl) {
            *excl = '\0';
            item = strdup(term);
            subitem = strdup(excl + 1);
            trim_string(item);
            trim_string(subitem);
            strip_leanpub_formatting(item);
            strip_leanpub_formatting(subitem);
            free(term);
        } else {
            strip_leanpub_formatting(term);
            trim_string(term);
            item = term;
        }

        p++;  /* Skip closing quote */
    } else {
        /* Unquoted: {i: Ishmael} */
        const char *start = p;
        while (p < text + len && *p != '}' && *p != '\n') {
            if (is_valid_index_char(*p)) {
                p++;
            } else {
                return 0;  /* Invalid char in unquoted term */
            }
        }
        if (p >= text + len || *p != '}') {
            return 0;
        }

        size_t term_len = p - start;
        if (term_len == 0) return 0;

        item = malloc(term_len + 1);
        if (!item) return 0;
        memcpy(item, start, term_len);
        item[term_len] = '\0';
        trim_string(item);
        if (strlen(item) == 0) {
            free(item);
            return 0;
        }
    }

    /* Must end with } */
    while (p < text + len && (*p == ' ' || *p == '\t')) p++;
    if (p >= text + len || *p != '}') {
        free(item);
        free(subitem);
        return 0;
    }
    p++;  /* Skip } */

    apex_index_entry *entry = apex_index_entry_new(item, APEX_INDEX_LEANPUB);
    if (entry) {
        entry->subitem = subitem;
        *entry_out = entry;
    } else {
        free(item);
        free(subitem);
    }

    return p - (text + pos);
}

/**
 * Create a new index entry
 */
apex_index_entry *apex_index_entry_new(const char *item, apex_index_syntax_t syntax_type) {
    if (!item) return NULL;

    apex_index_entry *entry = malloc(sizeof(apex_index_entry));
    if (!entry) return NULL;

    entry->item = strdup(item);
    entry->subitem = NULL;
    entry->primary = false;
    entry->position = 0;
    entry->anchor_id = NULL;
    entry->syntax_type = syntax_type;
    entry->next = NULL;

    return entry;
}

/**
 * Free an index entry
 */
void apex_index_entry_free(apex_index_entry *entry) {
    if (!entry) return;

    free(entry->item);
    free(entry->subitem);
    free(entry->anchor_id);
    free(entry);
}

/**
 * Free index registry
 */
void apex_free_index_registry(apex_index_registry *registry) {
    if (!registry) return;

    apex_index_entry *entry = registry->entries;
    while (entry) {
        apex_index_entry *next = entry->next;
        apex_index_entry_free(entry);
        entry = next;
    }

    registry->entries = NULL;
    registry->count = 0;
    registry->next_ref_id = 0;
}

/**
 * Process index entries in text via preprocessing
 */
char *apex_process_index_entries(const char *text, apex_index_registry *registry, const apex_options *options) {
    if (!text || !registry || !options->enable_indices) {
        return NULL;
    }

    size_t text_len = strlen(text);

    /* Quick scan: check if any index patterns exist before processing */
    bool has_mmark_pattern = false;
    bool has_textindex_pattern = false;
    bool has_leanpub_pattern = false;

    if (options->enable_mmark_index_syntax) {
        /* Look for (! or (!! patterns */
        const char *p = text;
        while (*p && p < text + text_len - 2) {
            if (*p == '(' && p[1] == '!') {
                has_mmark_pattern = true;
                break;
            }
            p++;
        }
    }

    if (options->enable_textindex_syntax && !has_mmark_pattern) {
        /* Look for {^ pattern */
        const char *p = text;
        while (*p && p < text + text_len - 1) {
            if (*p == '{' && p[1] == '^') {
                has_textindex_pattern = true;
                break;
            }
            p++;
        }
    }

    if (options->enable_leanpub_index_syntax && !has_mmark_pattern) {
        /* Look for {i: pattern */
        const char *p = text;
        while (*p && p < text + text_len - 4) {
            if (p[0] == '{' && p[1] == 'i' && p[2] == ':') {
                has_leanpub_pattern = true;
                break;
            }
            p++;
        }
    }

    /* Early exit if no patterns found */
    if (!has_mmark_pattern && !has_textindex_pattern && !has_leanpub_pattern) {
        return NULL;
    }

    size_t capacity = text_len * 2;  /* Generous buffer */
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        apex_index_entry *entry = NULL;
        int consumed = 0;

        /* Try mmark syntax first if enabled */
        if (options->enable_mmark_index_syntax) {
            consumed = parse_mmark_index(text, read - text, text_len, &entry);
        }

        /* Try TextIndex syntax if mmark didn't match and TextIndex is enabled */
        /* TextIndex uses {^} which we need to scan forward for */
        if (!entry && options->enable_textindex_syntax && *read == '{' && read + 1 < text + text_len && read[1] == '^') {
            consumed = parse_textindex(text, read - text, text_len, &entry);
        }

        /* Try Leanpub syntax if no match yet and Leanpub is enabled */
        if (!entry && options->enable_leanpub_index_syntax && *read == '{' && read + 3 < text + text_len &&
            read[1] == 'i' && read[2] == ':') {
            consumed = parse_leanpub_index(text, read - text, text_len, &entry);
        }

        if (entry && consumed > 0) {
            /* Add entry to registry */
            entry->position = read - text;
            char anchor_id[64];
            snprintf(anchor_id, sizeof(anchor_id), "idxref-%d", registry->next_ref_id);
            entry->anchor_id = strdup(anchor_id);
            entry->next = registry->entries;
            registry->entries = entry;
            registry->count++;
            registry->next_ref_id++;

            /* Replace with placeholder */
            size_t placeholder_len = strlen(INDEX_PLACEHOLDER_PREFIX) +
                                   strlen(anchor_id) +
                                   strlen(INDEX_PLACEHOLDER_SUFFIX);

            if (remaining < placeholder_len + 1) {
                /* Expand buffer */
                size_t used = write - output;
                capacity = (used + placeholder_len + 1) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    apex_index_entry_free(entry);
                    return NULL;
                }
                output = new_output;
                write = output + used;
                remaining = capacity - used;
            }

            snprintf(write, remaining, "%s%s%s",
                    INDEX_PLACEHOLDER_PREFIX, anchor_id, INDEX_PLACEHOLDER_SUFFIX);
            write += placeholder_len;
            remaining -= placeholder_len;

            read += consumed;
        } else {
            /* Copy character as-is */
            if (remaining < 2) {
                size_t used = write - output;
                capacity = (used + 1) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + used;
                remaining = capacity - used;
            }
            *write++ = *read++;
            remaining--;
        }
    }

    *write = '\0';
    return output;
}

/**
 * Render index markers in HTML output
 */
char *apex_render_index_markers(const char *html, apex_index_registry *registry, const apex_options *options) {
    if (!html || !registry || registry->count == 0 || !options->enable_indices) {
        return NULL;
    }

    size_t html_len = strlen(html);
    size_t capacity = html_len * 2;
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        /* Look for placeholder */
        if (strncmp(read, INDEX_PLACEHOLDER_PREFIX, strlen(INDEX_PLACEHOLDER_PREFIX)) == 0) {
            read += strlen(INDEX_PLACEHOLDER_PREFIX);

            /* Extract anchor ID */
            const char *id_start = read;
            while (*read && *read != '>' && strncmp(read, INDEX_PLACEHOLDER_SUFFIX, strlen(INDEX_PLACEHOLDER_SUFFIX)) != 0) {
                read++;
            }

            if (strncmp(read, INDEX_PLACEHOLDER_SUFFIX, strlen(INDEX_PLACEHOLDER_SUFFIX)) == 0) {
                size_t id_len = read - id_start;
                char anchor_id[64];
                if (id_len < sizeof(anchor_id)) {
                    memcpy(anchor_id, id_start, id_len);
                    anchor_id[id_len] = '\0';

                    /* Replace with HTML span */
                    size_t span_len = snprintf(NULL, 0, "<span class=\"index\" id=\"%s\"></span>", anchor_id);
                    if (remaining < span_len + 1) {
                        size_t used = write - output;
                        capacity = (used + span_len + 1) * 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) {
                            free(output);
                            return NULL;
                        }
                        output = new_output;
                        write = output + used;
                        remaining = capacity - used;
                    }

                    snprintf(write, remaining, "<span class=\"index\" id=\"%s\"></span>", anchor_id);
                    write += span_len;
                    remaining -= span_len;

                    read += strlen(INDEX_PLACEHOLDER_SUFFIX);
                    continue;
                }
            }
        }

        /* Copy character as-is */
        if (remaining < 2) {
            size_t used = write - output;
            capacity = (used + 1) * 2;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                free(output);
                return NULL;
            }
            output = new_output;
            write = output + used;
            remaining = capacity - used;
        }
        *write++ = *read++;
        remaining--;
    }

    *write = '\0';
    return output;
}

/**
 * Compare function for sorting index entries
 */
static int compare_index_entries(const void *a, const void *b) {
    const apex_index_entry *entry_a = *(const apex_index_entry **)a;
    const apex_index_entry *entry_b = *(const apex_index_entry **)b;

    /* Compare items (case-insensitive) */
    int item_cmp = strcasecmp(entry_a->item, entry_b->item);
    if (item_cmp != 0) return item_cmp;

    /* If items are equal, compare subitems */
    if (entry_a->subitem && entry_b->subitem) {
        return strcasecmp(entry_a->subitem, entry_b->subitem);
    } else if (entry_a->subitem) {
        return 1;  /* Entry with subitem comes after entry without */
    } else if (entry_b->subitem) {
        return -1;
    }

    return 0;
}

/**
 * Get first letter of index term (for grouping)
 */
static char get_first_letter(const char *term) {
    if (!term || *term == '\0') return '?';

    /* Skip leading whitespace and punctuation */
    while (*term && (!isalnum((unsigned char)*term))) {
        term++;
    }

    if (*term) {
        return toupper((unsigned char)*term);
    }

    return '?';
}

/**
 * Generate index HTML from collected entries
 */
char *apex_generate_index_html(apex_index_registry *registry, const apex_options *options) {
    if (!registry || registry->count == 0) {
        return strdup("");
    }

    /* Collect entries into array for sorting */
    apex_index_entry **entries = malloc(registry->count * sizeof(apex_index_entry *));
    if (!entries) return strdup("");

    size_t idx = 0;
    for (apex_index_entry *entry = registry->entries; entry; entry = entry->next) {
        entries[idx++] = entry;
    }

    /* Sort entries */
    qsort(entries, registry->count, sizeof(apex_index_entry *), compare_index_entries);

    /* Generate HTML */
    size_t capacity = 8192;
    char *html = malloc(capacity);
    if (!html) {
        free(entries);
        return strdup("");
    }

    char *write = html;
    size_t remaining = capacity;

    #define APPEND(str) do { \
        size_t len = strlen(str); \
        if (len < remaining) { \
            memcpy(write, str, len); \
            write += len; \
            remaining -= len; \
        } else { \
            size_t used = write - html; \
            capacity = (used + len + 1) * 2; \
            char *new_html = realloc(html, capacity); \
            if (!new_html) { \
                free(html); \
                free(entries); \
                return strdup(""); \
            } \
            html = new_html; \
            write = html + used; \
            remaining = capacity - used; \
            memcpy(write, str, len); \
            write += len; \
            remaining -= len; \
        } \
    } while(0)

    APPEND("<h1 id=\"index-section\">Index</h1>\n");
    APPEND("<div class=\"index\">\n");

    if (options->group_index_by_letter) {
        /* Group by first letter */
        char current_letter = '\0';
        bool in_group = false;

        for (size_t i = 0; i < registry->count; i++) {
            apex_index_entry *entry = entries[i];
            char letter = get_first_letter(entry->item);

            if (letter != current_letter) {
                if (in_group) {
                    APPEND("</ul>\n</dd>\n</dl>\n");
                }

                current_letter = letter;
                char letter_str[2] = {letter, '\0'};
                char group_html[256];
                snprintf(group_html, sizeof(group_html), "<dl>\n<dt>%s</dt>\n<dd>\n<ul>\n", letter_str);
                APPEND(group_html);
                in_group = true;
            }

            /* Collect all entries with same item */
            char item_html[2048];
            snprintf(item_html, sizeof(item_html), "<li>\n%s", entry->item);
            APPEND(item_html);

            if (entry->primary) {
                APPEND(" <strong>");
            }

            /* Add link */
            char link_html[256];
            snprintf(link_html, sizeof(link_html), " <a class=\"index-return\" href=\"#%s\"><sup>[go]</sup></a>",
                    entry->anchor_id ? entry->anchor_id : "");
            APPEND(link_html);

            if (entry->primary) {
                APPEND("</strong>");
            }

            /* Add subitems if any */
            if (entry->subitem) {
                APPEND("<ul>\n<li>\n");
                APPEND(entry->subitem);
                snprintf(link_html, sizeof(link_html), " <a class=\"index-return\" href=\"#%s\"><sup>[go]</sup></a>",
                        entry->anchor_id ? entry->anchor_id : "");
                APPEND(link_html);
                APPEND("</li>\n</ul>\n");
            }

            APPEND("</li>\n");
        }

        if (in_group) {
            APPEND("</ul>\n</dd>\n</dl>\n");
        }
    } else {
        /* Simple list without grouping */
        APPEND("<ul>\n");

        for (size_t i = 0; i < registry->count; i++) {
            apex_index_entry *entry = entries[i];

            char item_html[2048];
            snprintf(item_html, sizeof(item_html), "<li>\n%s", entry->item);
            APPEND(item_html);

            if (entry->primary) {
                APPEND(" <strong>");
            }

            char link_html[256];
            snprintf(link_html, sizeof(link_html), " <a class=\"index-return\" href=\"#%s\"><sup>[go]</sup></a>",
                    entry->anchor_id ? entry->anchor_id : "");
            APPEND(link_html);

            if (entry->primary) {
                APPEND("</strong>");
            }

            if (entry->subitem) {
                APPEND("<ul>\n<li>\n");
                APPEND(entry->subitem);
                snprintf(link_html, sizeof(link_html), " <a class=\"index-return\" href=\"#%s\"><sup>[go]</sup></a>",
                        entry->anchor_id ? entry->anchor_id : "");
                APPEND(link_html);
                APPEND("</li>\n</ul>\n");
            }

            APPEND("</li>\n");
        }

        APPEND("</ul>\n");
    }

    APPEND("</div>\n");

    #undef APPEND

    *write = '\0';
    free(entries);
    return html;
}

/**
 * Insert index at <!--INDEX--> marker or end of document
 */
char *apex_insert_index(const char *html, apex_index_registry *registry, const apex_options *options) {
    if (!html || !registry || registry->count == 0 || !options->enable_indices || options->suppress_index) {
        return NULL;
    }

    char *index_html = apex_generate_index_html(registry, options);
    if (!index_html || strlen(index_html) == 0) {
        return NULL;
    }

    /* Look for <!--INDEX--> marker */
    const char *marker = "<!--INDEX-->";
    const char *marker_pos = strstr(html, marker);

    if (marker_pos) {
        /* Insert at marker */
        size_t before_len = marker_pos - html;
        size_t after_len = strlen(marker_pos + strlen(marker));
        size_t index_len = strlen(index_html);
        size_t total_len = before_len + index_len + after_len + 1;

        char *output = malloc(total_len);
        if (!output) {
            free(index_html);
            return NULL;
        }

        memcpy(output, html, before_len);
        memcpy(output + before_len, index_html, index_len);
        memcpy(output + before_len + index_len, marker_pos + strlen(marker), after_len);
        output[total_len - 1] = '\0';

        free(index_html);
        return output;
    } else {
        /* Insert at end, before </body> if present, otherwise at very end */
        const char *body_end = strstr(html, "</body>");
        if (body_end) {
            size_t before_len = body_end - html;
            size_t index_len = strlen(index_html);
            size_t after_len = strlen(body_end);
            size_t total_len = before_len + index_len + after_len + 1;

            char *output = malloc(total_len);
            if (!output) {
                free(index_html);
                return NULL;
            }

            memcpy(output, html, before_len);
            memcpy(output + before_len, index_html, index_len);
            memcpy(output + before_len + index_len, body_end, after_len);
            output[total_len - 1] = '\0';

            free(index_html);
            return output;
        } else {
            /* Append at end */
            size_t html_len = strlen(html);
            size_t index_len = strlen(index_html);
            size_t total_len = html_len + index_len + 1;

            char *output = malloc(total_len);
            if (!output) {
                free(index_html);
                return NULL;
            }

            memcpy(output, html, html_len);
            memcpy(output + html_len, index_html, index_len);
            output[total_len - 1] = '\0';

            free(index_html);
            return output;
        }
    }
}
