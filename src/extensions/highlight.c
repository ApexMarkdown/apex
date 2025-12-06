/**
 * Simple Highlight Extension
 * Converts ==text== to <mark>text</mark>
 */

#include "highlight.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * Process ==highlight== syntax as preprocessing
 * Converts to <mark>text</mark> before parsing
 */
char *apex_process_highlights(const char *text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    size_t capacity = len * 2;  /* Room for <mark> tags */
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;

    bool in_code_block = false;
    bool in_inline_code = false;

    while (*read) {
        /* Track code blocks (skip highlighting inside them) */
        if (*read == '`') {
            if (read[1] == '`' && read[2] == '`') {
                in_code_block = !in_code_block;
            } else if (!in_code_block) {
                in_inline_code = !in_inline_code;
            }
        }

        /* Look for ==highlight== (not in code, not Critic Markup) */
        /* Skip if preceded by { (Critic Markup) */
        bool is_critic = (read > text && read[-1] == '{');
        if (!in_code_block && !in_inline_code && !is_critic &&
            read[0] == '=' && read[1] == '=' && read[2] != '=' && read[2] != '}') {

            /* Find closing == */
            const char *close = read + 2;
            while (*close) {
                if (close[0] == '=' && close[1] == '=' &&
                    (close[2] != '=' || close[-1] == '}')) {  /* Not Critic ==} */
                    break;
                }
                close++;
            }

            if (close[0] == '=' && close[1] == '=') {
                /* Found complete ==highlight== */
                size_t content_len = close - (read + 2);

                /* Write <mark> */
                const char *open_tag = "<mark>";
                size_t tag_len = strlen(open_tag);
                if (tag_len < remaining) {
                    memcpy(write, open_tag, tag_len);
                    write += tag_len;
                    remaining -= tag_len;
                }

                /* Copy highlighted content */
                if (content_len < remaining) {
                    memcpy(write, read + 2, content_len);
                    write += content_len;
                    remaining -= content_len;
                }

                /* Write </mark> */
                const char *close_tag = "</mark>";
                tag_len = strlen(close_tag);
                if (tag_len < remaining) {
                    memcpy(write, close_tag, tag_len);
                    write += tag_len;
                    remaining -= tag_len;
                }

                /* Skip past the closing == */
                read = close + 2;
                continue;
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


