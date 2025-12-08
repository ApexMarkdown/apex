/**
 * Superscript and Subscript Extension
 * Converts ^text^ to <sup>text</sup> and ~text~ to <sub>text</sub>
 * MultiMarkdown-style syntax
 */

#include "sup_sub.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

/**
 * Process superscript and subscript syntax as preprocessing
 * Converts to <sup>text</sup> and <sub>text</sub> before parsing
 */
char *apex_process_sup_sub(const char *text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    /* Allocate enough space: original text + potential tag expansions
     * Each ^ or ~ can expand to ~20 chars (<sup>content</sup> or <sub>content</sub>)
     * Use len * 5 to be safe, with a minimum of 64 bytes */
    size_t capacity = (len * 5 > 64) ? len * 5 : 64;
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;

    bool in_code_block = false;
    bool in_inline_code = false;

    while (*read) {
        /* Track code blocks (skip processing inside them) */
        if (*read == '`') {
            if (read[1] == '`' && read[2] == '`') {
                in_code_block = !in_code_block;
            } else if (!in_code_block) {
                in_inline_code = !in_inline_code;
            }
        }

        /* Skip processing inside code */
        if (in_code_block || in_inline_code) {
            if (remaining > 0) {
                *write++ = *read++;
                remaining--;
            } else {
                read++;
            }
            continue;
        }

        /* Check for superscript: ^word (only first word, stops at space, ^, or end) */
        if (*read == '^' && read[1] != '\0' && read[1] != ' ' && read[1] != '\t' && read[1] != '\n' && read[1] != '^') {
            const char *content_start = read + 1;
            const char *content_end = content_start;

            /* Find end of word (first space, ^, newline, or end of string) */
            while (*content_end && *content_end != ' ' && *content_end != '\t' && *content_end != '\n' && *content_end != '^') {
                content_end++;
            }

            size_t content_len = content_end - content_start;

            /* Only process if we have content */
            if (content_len > 0) {
                const char *open_tag = "<sup>";
                const char *close_tag = "</sup>";
                size_t open_tag_len = strlen(open_tag);
                size_t close_tag_len = strlen(close_tag);
                size_t total_needed = open_tag_len + content_len + close_tag_len;

                /* Only write if we have enough space for all parts */
                if (remaining >= total_needed) {
                    /* Write <sup> */
                    memcpy(write, open_tag, open_tag_len);
                    write += open_tag_len;
                    remaining -= open_tag_len;

                    /* Copy superscript content */
                    memcpy(write, content_start, content_len);
                    write += content_len;
                    remaining -= content_len;

                    /* Write </sup> - we know we have space because we checked total_needed */
                    memcpy(write, close_tag, close_tag_len);
                    write += close_tag_len;
                    remaining -= close_tag_len;

                    /* Skip past the content (and the marker if we stopped at it) */
                    read = content_end;
                    /* If we stopped at ^ or ~, skip past it so it's not reprocessed */
                    if (*read == '^' || *read == '~') {
                        read++;
                    }
                    continue;
                }
            }
        }

        /* Check for subscript: ~word (only first word, stops at space, ~, or end) */
        if (*read == '~' && read[1] != '\0' && read[1] != ' ' && read[1] != '\t' && read[1] != '\n' && read[1] != '~') {
            const char *content_start = read + 1;
            const char *content_end = content_start;

            /* Find end of word (first space, ~, newline, or end of string) */
            while (*content_end && *content_end != ' ' && *content_end != '\t' && *content_end != '\n' && *content_end != '~') {
                content_end++;
            }

            size_t content_len = content_end - content_start;

            /* Only process if we have content */
            if (content_len > 0) {
                const char *open_tag = "<sub>";
                const char *close_tag = "</sub>";
                size_t open_tag_len = strlen(open_tag);
                size_t close_tag_len = strlen(close_tag);
                size_t total_needed = open_tag_len + content_len + close_tag_len;

                /* Only write if we have enough space for all parts */
                if (remaining >= total_needed) {
                    /* Write <sub> */
                    memcpy(write, open_tag, open_tag_len);
                    write += open_tag_len;
                    remaining -= open_tag_len;

                    /* Copy subscript content */
                    memcpy(write, content_start, content_len);
                    write += content_len;
                    remaining -= content_len;

                    /* Write </sub> - we know we have space because we checked total_needed */
                    memcpy(write, close_tag, close_tag_len);
                    write += close_tag_len;
                    remaining -= close_tag_len;

                    /* Skip past the content (and the marker if we stopped at it) */
                    read = content_end;
                    /* If we stopped at ^ or ~, skip past it so it's not reprocessed */
                    if (*read == '^' || *read == '~') {
                        read++;
                    }
                    continue;
                }
            }
        }

        /* Copy character */
        if (remaining > 0) {
            *write++ = *read++;
            remaining--;
        } else {
            read++;
        }
    }

    *write = '\0';
    return output;
}
