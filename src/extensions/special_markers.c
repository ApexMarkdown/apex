/**
 * Special Markers Extension for Apex
 * Implementation
 */

#include "special_markers.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

/**
 * Process special markers in text
 */
char *apex_process_special_markers(const char *text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    /* Page break divs are ~64 bytes each, so need generous capacity */
    size_t capacity = len * 4;  /* Room for expansion */
    char *output = malloc(capacity);
    if (!output) return strdup(text);

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        /* Check for End of Block marker (Kramdown) */
        /* Pattern: ^ on a line by itself (with optional leading whitespace) */
        if (*read == '^') {
            /* Check if it's on its own line */
            const char *before = read - 1;
            bool line_start = (read == text);

            /* Skip back over whitespace to check for line start */
            while (!line_start && before >= text && (*before == ' ' || *before == '\t')) {
                before--;
            }
            if (!line_start && before >= text && *before == '\n') {
                line_start = true;
            }

            /* Check what comes after */
            const char *after = read + 1;
            bool line_end = (*after == '\n' || *after == '\0');
            while (!line_end && (*after == ' ' || *after == '\t')) {
                after++;
            }
            if (!line_end && (*after == '\n' || *after == '\0')) {
                line_end = true;
            }

            if (line_start && line_end) {
                /* This is an end-of-block marker */
                /* Replace with a paragraph containing zero-width space (U+200B) to force block separation */
                /* This ensures lists are not merged by the parser, and the paragraph won't render visibly */
                const char *replacement = "\n\n\u200B\n\n";
                size_t repl_len = strlen(replacement);
                if (repl_len < remaining) {
                    memcpy(write, replacement, repl_len);
                    write += repl_len;
                    remaining -= repl_len;
                }
                /* Skip to after the ^ and any trailing whitespace/newline */
                read = after;
                if (*read == '\n') read++;
                continue;
            }
        }

        /* Check for <!--BREAK--> */
        if (strncmp(read, "<!--BREAK-->", 12) == 0) {
            const char *replacement = "<div class=\"page-break\" style=\"page-break-after: always;\"></div>";
            size_t repl_len = strlen(replacement);
            if (repl_len < remaining) {
                memcpy(write, replacement, repl_len);
                write += repl_len;
                remaining -= repl_len;
            }
            read += 12;
            continue;
        }

        /* Check for <!--PAUSE:X--> */
        if (strncmp(read, "<!--PAUSE:", 10) == 0) {
            const char *num_start = read + 10;
            const char *num_end = num_start;
            while (isdigit((unsigned char)*num_end)) num_end++;

            if (*num_end == '-' && num_end[1] == '-' && num_end[2] == '>') {
                /* Valid PAUSE marker */
                int seconds = atoi(num_start);
                char replacement[256];
                snprintf(replacement, sizeof(replacement),
                        "<div class=\"autoscroll-pause\" data-pause=\"%d\"></div>",
                        seconds);

                size_t repl_len = strlen(replacement);
                if (repl_len < remaining) {
                    memcpy(write, replacement, repl_len);
                    write += repl_len;
                    remaining -= repl_len;
                }
                read = num_end + 3;
                continue;
            }
        }

        /* Check for {::pagebreak /} (Leanpub style) */
        if (strncmp(read, "{::pagebreak /}", 15) == 0) {
            const char *replacement = "<div class=\"page-break\" style=\"page-break-after: always;\"></div>";
            size_t repl_len = strlen(replacement);
            if (repl_len < remaining) {
                memcpy(write, replacement, repl_len);
                write += repl_len;
                remaining -= repl_len;
            }
            read += 15;
            continue;
        }

        /* Not a special marker, copy character */
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

