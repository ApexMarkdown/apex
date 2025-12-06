/**
 * Header ID Generation Extension
 * Implementation
 */

#include "header_ids.h"
#include "cmark-gfm.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/**
 * Convert diacritic characters to ASCII equivalents
 * Handles common Latin diacritics
 */
static char normalize_char(unsigned char c) {
    /* Basic ASCII alphanumeric */
    if (isalnum(c)) {
        return tolower(c);
    }

    /* Common diacritics → ASCII */
    /* This is a simplified mapping - full Unicode normalization would require ICU */
    switch (c) {
        /* Latin-1 Supplement */
        case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: return 'a'; /* ÀÁÂÃÄÅ */
        case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: return 'a'; /* àáâãäå */
        case 0xC7: return 'c'; /* Ç */
        case 0xE7: return 'c'; /* ç */
        case 0xC8: case 0xC9: case 0xCA: case 0xCB: return 'e'; /* ÈÉÊË */
        case 0xE8: case 0xE9: case 0xEA: case 0xEB: return 'e'; /* èéêë */
        case 0xCC: case 0xCD: case 0xCE: case 0xCF: return 'i'; /* ÌÍÎÏ */
        case 0xEC: case 0xED: case 0xEE: case 0xEF: return 'i'; /* ìíîï */
        case 0xD1: return 'n'; /* Ñ */
        case 0xF1: return 'n'; /* ñ */
        case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD8: return 'o'; /* ÒÓÔÕÖØ */
        case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF8: return 'o'; /* òóôõöø */
        case 0xD9: case 0xDA: case 0xDB: case 0xDC: return 'u'; /* ÙÚÛÜ */
        case 0xF9: case 0xFA: case 0xFB: case 0xFC: return 'u'; /* ùúûü */
        case 0xDD: case 0xFD: case 0xFF: return 'y'; /* Ýýÿ */
        case 0xDF: return 's'; /* ß */
        default: return 0; /* Not a valid character to keep */
    }
}

/**
 * Generate header ID from text
 */
char *apex_generate_header_id(const char *text, apex_id_format_t format) {
    if (!text) return strdup("");

    size_t len = strlen(text);
    char *id = malloc(len * 3 + 1);  /* Extra space for UTF-8 expansion */
    if (!id) return strdup("");

    char *write = id;
    bool last_was_dash = false;
    bool first_char = true;
    bool last_was_punct_dash = false;  /* Track if last dash came from punctuation (for kramdown) */

    for (const char *read = text; *read; read++) {
        unsigned char c = (unsigned char)*read;

        /* Skip if already processed as part of UTF-8 sequence */
        if ((c & 0xC0) == 0x80) continue;

        /* Check for em dash (—) U+2014: 0xE2 0x80 0x94 */
        if (c == 0xE2 && read[1] != '\0' && read[2] != '\0' &&
            (unsigned char)read[1] == 0x80 && (unsigned char)read[2] == 0x94) {
            if (format == APEX_ID_FORMAT_MMD) {
                /* MMD: preserve em dash as-is */
                *write++ = 0xE2;
                *write++ = 0x80;
                *write++ = 0x94;
                last_was_dash = false;
                first_char = false;
            }
            /* GFM and Kramdown: remove em dash (skip it) */
            read += 2;  /* Skip the next 2 bytes (for loop will increment past 0x94) */
            continue;
        }

        /* Check for en dash (–) U+2013: 0xE2 0x80 0x93 */
        if (c == 0xE2 && read[1] != '\0' && read[2] != '\0' &&
            (unsigned char)read[1] == 0x80 && (unsigned char)read[2] == 0x93) {
            if (format == APEX_ID_FORMAT_MMD) {
                /* MMD: preserve en dash as-is */
                *write++ = 0xE2;
                *write++ = 0x80;
                *write++ = 0x93;
                last_was_dash = false;
                first_char = false;
            }
            /* GFM and Kramdown: remove en dash (skip it) */
            read += 2;  /* Skip the next 2 bytes (for loop will increment past 0x93) */
            continue;
        }

        if (format == APEX_ID_FORMAT_MMD) {
            /* MMD format: preserve dashes, lowercase alphanumerics, preserve diacritics, skip spaces/punctuation */
            if (c == '-') {
                /* Regular dash: preserve as-is */
                *write++ = '-';
                last_was_dash = false;
                first_char = false;
            } else if (isalnum((unsigned char)c)) {
                /* ASCII alphanumeric: lowercase */
                *write++ = tolower(c);
                last_was_dash = false;
                first_char = false;
            } else if (c == ' ') {
                /* Space: skip (remove) */
                /* Do nothing */
            } else if (c < 0x80) {
                /* ASCII non-alphanumeric (punctuation, etc.): skip */
                /* Do nothing */
            } else {
                /* UTF-8 character (diacritics, etc.): preserve as-is, but lowercase if it's a letter */
                /* Check if this is the start of a UTF-8 sequence */
                int utf8_len = 0;
                if ((c & 0xE0) == 0xC0) utf8_len = 2;  /* 2-byte sequence */
                else if ((c & 0xF0) == 0xE0) utf8_len = 3;  /* 3-byte sequence */
                else if ((c & 0xF8) == 0xF0) utf8_len = 4;  /* 4-byte sequence */

                if (utf8_len > 0) {
                    /* Copy the entire UTF-8 sequence */
                    for (int i = 0; i < utf8_len && read[i] != '\0'; i++) {
                        *write++ = read[i];
                    }
                    read += utf8_len - 1;  /* -1 because for loop will increment */
                    last_was_dash = false;
                    first_char = false;
                } else {
                    /* Invalid UTF-8 or single byte: skip */
                }
            }
        } else if (format == APEX_ID_FORMAT_KRAMDOWN) {
            /* Kramdown format: spaces→dashes (not collapsed), remove diacritics, remove em/en dashes */
            /* Trailing punctuation is removed, not converted to dash */
            /* If punctuation is followed by space, skip the space */
            const char *next = read + 1;
            while (*next && (*next == ' ' || *next == '\t' || *next == '\n' || *next == '\r')) next++;
            bool is_trailing = (*next == '\0');

            if (c == '-') {
                /* Regular dash: preserve as-is */
                *write++ = '-';
                last_was_dash = false;
                last_was_punct_dash = false;
                first_char = false;
            } else if (isalnum((unsigned char)c)) {
                /* ASCII alphanumeric: lowercase */
                *write++ = tolower(c);
                last_was_dash = false;
                last_was_punct_dash = false;
                first_char = false;
            } else if (c == ' ') {
                /* Space: check if it follows punctuation that became a dash */
                if (last_was_punct_dash) {
                    /* Space after punctuation: skip (don't convert to dash) */
                    last_was_punct_dash = false;
                    /* Do nothing */
                } else {
                    /* Regular space: convert to dash (don't collapse multiple spaces) */
                    *write++ = '-';
                    last_was_dash = false;
                    last_was_punct_dash = false;
                    first_char = false;
                }
            } else if (c < 0x80) {
                /* ASCII non-alphanumeric (punctuation, etc.) */
                if (is_trailing) {
                    /* Trailing punctuation: remove (skip) */
                    last_was_punct_dash = false;
                    /* Do nothing */
                } else {
                    /* Middle punctuation: convert to dash */
                    *write++ = '-';
                    last_was_dash = false;
                    last_was_punct_dash = true;  /* Mark that this dash came from punctuation */
                    first_char = false;
                }
            } else {
                /* UTF-8 character (diacritics, etc.): remove (skip) */
                /* Check if this is the start of a UTF-8 sequence */
                int utf8_len = 0;
                if ((c & 0xE0) == 0xC0) utf8_len = 2;  /* 2-byte sequence */
                else if ((c & 0xF0) == 0xE0) utf8_len = 3;  /* 3-byte sequence */
                else if ((c & 0xF8) == 0xF0) utf8_len = 4;  /* 4-byte sequence */

                if (utf8_len > 0) {
                    /* Skip the entire UTF-8 sequence */
                    read += utf8_len - 1;  /* -1 because for loop will increment */
                }
                /* Otherwise skip the byte */
            }
        } else {
            /* GFM format: spaces become dashes, other whitespace/punctuation removed, normalize diacritics */
            char normalized = normalize_char(c);

            if (normalized) {
                /* Valid alphanumeric character (normalized diacritic) */
                *write++ = normalized;
                last_was_dash = false;
                first_char = false;
            } else if (isalnum(c)) {
                /* ASCII alphanumeric not in our diacritic map */
                *write++ = tolower(c);
                last_was_dash = false;
                first_char = false;
            } else if (c == ' ') {
                /* Space: convert to dash (collapsed) */
                if (!last_was_dash && !first_char) {
                    *write++ = '-';
                    last_was_dash = true;
                }
            } else if (c == '-') {
                /* Regular dash: preserve (but don't add multiple consecutive) */
                if (!last_was_dash && !first_char) {
                    *write++ = '-';
                    last_was_dash = true;
                }
            } else {
                /* Other whitespace and punctuation: remove (skip) */
                /* Do nothing */
            }
        }
    }

    *write = '\0';

    /* Trim dashes from start and end */
    if (format == APEX_ID_FORMAT_GFM) {
        /* GFM: trim both leading and trailing dashes */
        char *start = id;
        char *end = write - 1;

        while (*start == '-') start++;
        while (end > start && *end == '-') end--;

        if (start > id) {
            size_t new_len = end - start + 1;
            memmove(id, start, new_len);
            id[new_len] = '\0';
        } else if (end < write - 1) {
            *(end + 1) = '\0';
        }
    } else if (format == APEX_ID_FORMAT_KRAMDOWN) {
        /* Kramdown: trim only leading dashes, preserve trailing */
        char *start = id;
        while (*start == '-') start++;

        if (start > id) {
            size_t new_len = write - start;
            memmove(id, start, new_len);
            id[new_len] = '\0';
        }
    }
    /* MMD format: preserve leading/trailing dashes */

    /* If result is empty, use "header" */
    if (strlen(id) == 0) {
        free(id);
        return strdup("header");
    }

    return id;
}

/**
 * Extract text content from a heading node
 */
char *apex_extract_heading_text(cmark_node *heading_node) {
    if (!heading_node || cmark_node_get_type(heading_node) != CMARK_NODE_HEADING) {
        return strdup("");
    }

    /* Walk children and collect text */
    size_t capacity = 256;
    char *text = malloc(capacity);
    if (!text) return strdup("");

    char *write = text;
    size_t remaining = capacity;

    cmark_node *child = cmark_node_first_child(heading_node);
    while (child) {
        cmark_node_type type = cmark_node_get_type(child);

        if (type == CMARK_NODE_TEXT) {
            const char *literal = cmark_node_get_literal(child);
            if (literal) {
                size_t len = strlen(literal);
                if (len >= remaining) {
                    size_t new_capacity = capacity * 2;
                    char *new_text = realloc(text, new_capacity);
                    if (!new_text) {
                        free(text);
                        return strdup("");
                    }
                    write = new_text + (write - text);
                    text = new_text;
                    remaining = new_capacity - (write - text);
                }
                memcpy(write, literal, len);
                write += len;
                remaining -= len;
            }
        } else if (type == CMARK_NODE_CODE) {
            const char *literal = cmark_node_get_literal(child);
            if (literal) {
                size_t len = strlen(literal);
                if (len >= remaining) {
                    size_t new_capacity = capacity * 2;
                    char *new_text = realloc(text, new_capacity);
                    if (!new_text) {
                        free(text);
                        return strdup("");
                    }
                    write = new_text + (write - text);
                    text = new_text;
                    remaining = new_capacity - (write - text);
                }
                memcpy(write, literal, len);
                write += len;
                remaining -= len;
            }
        }
        /* Skip other inline elements for ID generation */

        child = cmark_node_next(child);
    }

    *write = '\0';
    return text;
}

/**
 * Extract manual header ID from heading text
 * Supports:
 * - MultiMarkdown: "Heading [id]" -> returns "id", removes "[id]" from text
 * - Kramdown: "Heading {#id}" -> returns "id", removes "{#id}" from text
 *
 * Note: IAL format "Heading {: #id}" is handled separately by IAL processor
 *
 * @param heading_text Heading text (will be modified to remove ID syntax)
 * @param manual_id_out Output parameter for extracted ID (must be freed by caller)
 * @return true if manual ID was found and extracted
 */
bool apex_extract_manual_header_id(char **heading_text, char **manual_id_out) {
    if (!heading_text || !*heading_text || !manual_id_out) {
        return false;
    }

    char *text = *heading_text;
    size_t len = strlen(text);
    if (len == 0) return false;

    /* Try MultiMarkdown format: [id] at the end */
    const char *mmd_start = strrchr(text, '[');
    if (mmd_start) {
        const char *mmd_end = strchr(mmd_start, ']');
        if (mmd_end && mmd_end > mmd_start + 1) {
            /* Check nothing after ] except whitespace */
            const char *after = mmd_end + 1;
            while (*after && isspace((unsigned char)*after)) after++;
            if (*after == '\0') {
                /* Extract ID */
                size_t id_len = mmd_end - mmd_start - 1;
                if (id_len > 0) {
                    *manual_id_out = malloc(id_len + 1);
                    if (*manual_id_out) {
                        memcpy(*manual_id_out, mmd_start + 1, id_len);
                        (*manual_id_out)[id_len] = '\0';

                        /* Remove [id] from text */
                        size_t prefix_len = mmd_start - text;
                        char *new_text = malloc(prefix_len + 1);
                        if (new_text) {
                            memcpy(new_text, text, prefix_len);
                            new_text[prefix_len] = '\0';

                            /* Trim trailing whitespace */
                            char *end = new_text + prefix_len - 1;
                            while (end >= new_text && isspace((unsigned char)*end)) *end-- = '\0';

                            free(*heading_text);
                            *heading_text = new_text;
                            return true;
                        } else {
                            free(*manual_id_out);
                            *manual_id_out = NULL;
                        }
                    }
                }
            }
        }
    }

    /* Try Kramdown format: {#id} at the end */
    const char *kramdown_start = strrchr(text, '{');
    if (kramdown_start && kramdown_start[1] == '#') {
        const char *kramdown_end = strchr(kramdown_start, '}');
        if (kramdown_end && kramdown_end > kramdown_start + 2) {
            /* Check nothing after } except whitespace */
            const char *after = kramdown_end + 1;
            while (*after && isspace((unsigned char)*after)) after++;
            if (*after == '\0') {
                /* Extract ID */
                size_t id_len = kramdown_end - kramdown_start - 2;  /* Skip {# */
                if (id_len > 0) {
                    *manual_id_out = malloc(id_len + 1);
                    if (*manual_id_out) {
                        memcpy(*manual_id_out, kramdown_start + 2, id_len);
                        (*manual_id_out)[id_len] = '\0';

                        /* Remove {#id} from text */
                        size_t prefix_len = kramdown_start - text;
                        char *new_text = malloc(prefix_len + 1);
                        if (new_text) {
                            memcpy(new_text, text, prefix_len);
                            new_text[prefix_len] = '\0';

                            /* Trim trailing whitespace */
                            char *end = new_text + prefix_len - 1;
                            while (end >= new_text && isspace((unsigned char)*end)) *end-- = '\0';

                            free(*heading_text);
                            *heading_text = new_text;
                            return true;
                        } else {
                            free(*manual_id_out);
                            *manual_id_out = NULL;
                        }
                    }
                }
            }
        }
    }

    return false;
}

/**
 * Process manual header IDs in a heading node
 * Extracts MMD [id] or Kramdown {#id} syntax and stores ID in user_data
 * Updates the heading text node to remove the manual ID syntax
 */
bool apex_process_manual_header_id(cmark_node *heading_node) {
    if (!heading_node || cmark_node_get_type(heading_node) != CMARK_NODE_HEADING) {
        return false;
    }

    /* Get the text node inside the heading */
    cmark_node *text_node = cmark_node_first_child(heading_node);
    if (!text_node || cmark_node_get_type(text_node) != CMARK_NODE_TEXT) {
        return false;
    }

    const char *text = cmark_node_get_literal(text_node);
    if (!text) return false;

    /* Extract text and try to find manual ID */
    char *text_copy = strdup(text);
    if (!text_copy) return false;

    char *manual_id = NULL;
    bool found = apex_extract_manual_header_id(&text_copy, &manual_id);

    if (found && manual_id) {
        /* Store ID in user_data as id="..." */
        char *id_attr = malloc(strlen(manual_id) + 6);  /* id="" + null */
        if (id_attr) {
            sprintf(id_attr, "id=\"%s\"", manual_id);

            /* Merge with existing user_data if present */
            char *existing = (char *)cmark_node_get_user_data(heading_node);
            if (existing) {
                /* Append to existing */
                char *combined = malloc(strlen(existing) + strlen(id_attr) + 2);  /* + space + null */
                if (combined) {
                    sprintf(combined, "%s %s", existing, id_attr);
                    cmark_node_set_user_data(heading_node, combined);
                    free(id_attr);
                } else {
                    cmark_node_set_user_data(heading_node, id_attr);
                }
            } else {
                cmark_node_set_user_data(heading_node, id_attr);
            }
        }

        /* Update the text node to remove manual ID syntax */
        cmark_node_set_literal(text_node, text_copy);

        free(manual_id);
        free(text_copy);
        return true;
    }

    free(text_copy);
    if (manual_id) free(manual_id);
    return false;
}

