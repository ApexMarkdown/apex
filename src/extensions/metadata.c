/**
 * Metadata Extension for Apex
 * Implementation (Simplified version - metadata handled via preprocessing)
 */

#include "metadata.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>

/* For now, we'll handle metadata as a preprocessing step rather than a block type
 * This is simpler and matches how MultiMarkdown actually works */

/* Node type for metadata blocks */
cmark_node_type APEX_NODE_METADATA = CMARK_NODE_CUSTOM_BLOCK;

/**
 * Free metadata items list
 */
void apex_free_metadata(apex_metadata_item *metadata) {
    while (metadata) {
        apex_metadata_item *next = metadata->next;
        free(metadata->key);
        free(metadata->value);
        free(metadata);
        metadata = next;
    }
}

/**
 * Trim whitespace from both ends of a string (in-place)
 */
static char *trim_whitespace(char *str) {
    char *end;

    /* Trim leading space */
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

/**
 * Add a metadata item to the list
 */
static void add_metadata_item(apex_metadata_item **list, const char *key, const char *value) {
    apex_metadata_item *item = malloc(sizeof(apex_metadata_item));
    if (!item) return;

    item->key = strdup(key);
    item->value = strdup(value);
    item->next = *list;
    *list = item;
}

/**
 * Parse YAML front matter
 * Format: --- at start, key: value pairs, --- to close
 */
static apex_metadata_item *parse_yaml_metadata(const char *text, size_t *consumed) {
    apex_metadata_item *items = NULL;
    const char *line_start = text;
    const char *line_end;

    /* Skip opening --- */
    if (strncmp(text, "---", 3) != 0) return NULL;
    line_start = strchr(text + 3, '\n');
    if (!line_start) return NULL;
    line_start++;

    while ((line_end = strchr(line_start, '\n')) != NULL) {
        size_t len = line_end - line_start;
        char line[1024];

        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, line_start, len);
        line[len] = '\0';

        /* Check for closing --- or ... */
        char *trimmed = trim_whitespace(line);
        if (strcmp(trimmed, "---") == 0 || strcmp(trimmed, "...") == 0) {
            *consumed = (line_end + 1) - text;
            return items;
        }

        /* Parse key: value */
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *key = trim_whitespace(line);
            char *value = trim_whitespace(colon + 1);

            if (*key && *value) {
                add_metadata_item(&items, key, value);
            }
        }

        line_start = line_end + 1;
    }

    return items;
}

/**
 * Parse MultiMarkdown metadata
 * Format: key: value pairs at start of document, blank line to end
 */
static apex_metadata_item *parse_mmd_metadata(const char *text, size_t *consumed) {
    apex_metadata_item *items = NULL;
    const char *line_start = text;
    const char *line_end;
    bool found_metadata = false;

    while ((line_end = strchr(line_start, '\n')) != NULL) {
        size_t len = line_end - line_start;
        char line[1024];

        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, line_start, len);
        line[len] = '\0';

        /* Check for blank line (end of metadata) */
        char *trimmed = trim_whitespace(line);
        if (*trimmed == '\0') {
            if (found_metadata) {
                *consumed = (line_end + 1) - text;
                return items;
            }
            /* Skip leading blank lines */
            line_start = line_end + 1;
            continue;
        }

        /* Skip abbreviation definitions (*[abbr]: expansion or [>abbr]: expansion) */
        if ((trimmed[0] == '*' && trimmed[1] == '[') ||
            (trimmed[0] == '[' && trimmed[1] == '>')) {
            /* This is an abbreviation, not metadata */
            if (found_metadata) {
                *consumed = line_start - text;
                return items;
            }
            /* Haven't found metadata yet, skip this line */
            line_start = line_end + 1;
            continue;
        }

        /* Skip HTML comments (<!--...-->) */
        if (strncmp(trimmed, "<!--", 4) == 0) {
            /* This is an HTML comment, not metadata */
            if (found_metadata) {
                *consumed = line_start - text;
                return items;
            }
            /* Haven't found metadata yet, skip this line */
            line_start = line_end + 1;
            continue;
        }

        /* Skip Kramdown markers ({::...}) */
        if (strncmp(trimmed, "{::", 3) == 0) {
            /* This is a Kramdown marker, not metadata */
            if (found_metadata) {
                *consumed = line_start - text;
                return items;
            }
            /* Haven't found metadata yet, skip this line */
            line_start = line_end + 1;
            continue;
        }

        /* Skip Markdown headings (# or ##) */
        if (trimmed[0] == '#') {
            /* This is a Markdown heading, not metadata */
            if (found_metadata) {
                *consumed = line_start - text;
                return items;
            }
            /* Haven't found metadata yet, skip this line */
            line_start = line_end + 1;
            continue;
        }

        /* Skip IAL/ALD syntax ({: ...}) */
        if (strncmp(trimmed, "{:", 2) == 0) {
            /* This is IAL/ALD, not metadata */
            if (found_metadata) {
                *consumed = line_start - text;
                return items;
            }
            /* Haven't found metadata yet, skip this line */
            line_start = line_end + 1;
            continue;
        }

        /* Skip TOC markers ({{TOC...}}) */
        if (strncmp(trimmed, "{{TOC", 5) == 0) {
            /* This is a TOC marker, not metadata */
            if (found_metadata) {
                *consumed = line_start - text;
                return items;
            }
            /* Haven't found metadata yet, skip this line */
            line_start = line_end + 1;
            continue;
        }

        /* Skip lines with Markdown links or images */
        if (strchr(trimmed, '[') && strchr(trimmed, ']') && strchr(trimmed, '(')) {
            /* Contains markdown link/image syntax - not metadata */
            if (found_metadata) {
                *consumed = line_start - text;
                return items;
            }
            /* Haven't found metadata yet - this isn't metadata */
            *consumed = 0;
            return NULL;
        }

        /* Parse key: value */
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *key = trim_whitespace(line);
            char *value = trim_whitespace(colon + 1);

            if (*key && *value) {
                add_metadata_item(&items, key, value);
                found_metadata = true;
            } else {
                /* Line has colon but invalid key/value */
                if (found_metadata) {
                    *consumed = line_start - text;
                    return items;
                }
                /* No metadata found yet - this isn't metadata */
                *consumed = 0;
                return NULL;
            }
        } else {
            /* Non-metadata line found (no colon) */
            if (found_metadata) {
                *consumed = line_start - text;
                return items;
            }
            /* No metadata found yet and hit non-metadata line - stop */
            *consumed = 0;
            return NULL;
        }

        line_start = line_end + 1;
    }

    if (found_metadata) {
        *consumed = strlen(text);
    }
    return items;
}

/**
 * Parse Pandoc title block metadata
 * Format: % Title, % Author, % Date as first three lines
 */
static apex_metadata_item *parse_pandoc_metadata(const char *text, size_t *consumed) {
    apex_metadata_item *items = NULL;
    const char *keys[] = {"title", "author", "date"};
    int key_index = 0;
    const char *line_start = text;
    const char *line_end;

    while (key_index < 3 && (line_end = strchr(line_start, '\n')) != NULL) {
        size_t len = line_end - line_start;
        char line[1024];

        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, line_start, len);
        line[len] = '\0';

        char *trimmed = trim_whitespace(line);

        /* Must start with % */
        if (*trimmed == '%') {
            char *value = trim_whitespace(trimmed + 1);
            if (*value) {
                add_metadata_item(&items, keys[key_index], value);
            }
            key_index++;
        } else {
            /* Non-Pandoc line */
            break;
        }

        line_start = line_end + 1;
    }

    if (key_index > 0) {
        *consumed = line_start - text;
    }
    return items;
}

/**
 * Detect and extract metadata from the start of document text
 * This modifies the input by removing the metadata section
 * Returns the extracted metadata
 */
apex_metadata_item *apex_extract_metadata(char **text_ptr) {
    if (!text_ptr || !*text_ptr || !**text_ptr) return NULL;

    char *text = *text_ptr;
    size_t consumed = 0;
    apex_metadata_item *items = NULL;

    /* Try YAML first (most explicit) */
    if (strncmp(text, "---", 3) == 0) {
        items = parse_yaml_metadata(text, &consumed);
    }
    /* Try Pandoc */
    else if (*text == '%') {
        items = parse_pandoc_metadata(text, &consumed);
    }
    /* Try MMD (least specific) */
    else {
        items = parse_mmd_metadata(text, &consumed);
    }

    /* Remove metadata from text if found */
    if (items && consumed > 0) {
        /* Skip past the metadata */
        *text_ptr = text + consumed;
    }

    return items;
}

/**
 * Placeholder extension creation - for future full integration
 * For now, metadata is handled via preprocessing
 */
cmark_syntax_extension *create_metadata_extension(void) {
    /* Return NULL for now - we handle metadata via preprocessing */
    /* In the future, this could create a proper block extension */
    return NULL;
}

/**
 * Get metadata from a document (stub for now)
 */
apex_metadata_item *apex_get_metadata(cmark_node *document) {
    /* For now, metadata must be extracted before parsing */
    /* This would require storing metadata in the document's user_data */
    return NULL;
}

/**
 * Get a specific metadata value (case-insensitive)
 */
const char *apex_metadata_get(apex_metadata_item *metadata, const char *key) {
    for (apex_metadata_item *item = metadata; item != NULL; item = item->next) {
        if (strcasecmp(item->key, key) == 0) {
            return item->value;
        }
    }
    return NULL;
}

/**
 * Replace [%key] patterns with metadata values
 */
char *apex_metadata_replace_variables(const char *text, apex_metadata_item *metadata) {
    if (!text || !metadata) {
        return text ? strdup(text) : NULL;
    }

    /* First pass: calculate size needed */
    size_t result_size = strlen(text) + 1;
    const char *p = text;

    while ((p = strstr(p, "[%")) != NULL) {
        const char *end = strchr(p + 2, ']');
        if (!end) {
            p += 2;
            continue;
        }

        /* Extract key */
        size_t key_len = end - (p + 2);
        char key[256];
        if (key_len >= sizeof(key)) {
            p = end + 1;
            continue;
        }

        memcpy(key, p + 2, key_len);
        key[key_len] = '\0';

        /* Look up value */
        const char *value = apex_metadata_get(metadata, key);
        if (value) {
            result_size += strlen(value) - (key_len + 3);
        }

        p = end + 1;
    }

    /* Second pass: build result */
    char *result = malloc(result_size);
    if (!result) return NULL;

    char *dest = result;
    const char *src = text;

    while ((p = strstr(src, "[%")) != NULL) {
        const char *end = strchr(p + 2, ']');
        if (!end) {
            /* Copy rest and break */
            strcpy(dest, src);
            break;
        }

        /* Copy text before [%...] */
        size_t prefix_len = p - src;
        memcpy(dest, src, prefix_len);
        dest += prefix_len;

        /* Extract and replace key */
        size_t key_len = end - (p + 2);
        char key[256];
        if (key_len < sizeof(key)) {
            memcpy(key, p + 2, key_len);
            key[key_len] = '\0';

            const char *value = apex_metadata_get(metadata, key);
            if (value) {
                strcpy(dest, value);
                dest += strlen(value);
            } else {
                /* Keep original if not found */
                memcpy(dest, p, (end - p) + 1);
                dest += (end - p) + 1;
            }
        }

        src = end + 1;
    }

    /* Copy remaining text */
    if (*src) {
        strcpy(dest, src);
    } else {
        *dest = '\0';
    }

    return result;
}
