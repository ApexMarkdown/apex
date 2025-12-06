/**
 * GitHub Emoji Extension for Apex
 * Complete implementation with 200+ common emoji
 */

#include <string.h>
#include <stdlib.h>
#include "emoji_data.h"

/**
 * Find emoji by name (binary search would be faster, but linear is fine for now)
 */
static const char *find_emoji(const char *name, int len) {
    for (int i = 0; complete_emoji_map[i].name; i++) {
        if (strlen(complete_emoji_map[i].name) == (size_t)len &&
            strncmp(complete_emoji_map[i].name, name, len) == 0) {
            return complete_emoji_map[i].unicode;
        }
    }
    return NULL;
}

/**
 * Replace :emoji: patterns in HTML
 */
char *apex_replace_emoji(const char *html) {
    if (!html) return NULL;

    size_t capacity = strlen(html) * 2;
    char *output = malloc(capacity);
    if (!output) return strdup(html);

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        if (*read == ':') {
            /* Look for closing : */
            const char *end = strchr(read + 1, ':');
            if (end && (end - read) < 50) {  /* Reasonable emoji name length */
                /* Extract emoji name */
                int name_len = end - (read + 1);
                const char *emoji = find_emoji(read + 1, name_len);

                if (emoji) {
                    /* Replace with emoji unicode */
                    size_t emoji_len = strlen(emoji);
                    if (emoji_len < remaining) {
                        memcpy(write, emoji, emoji_len);
                        write += emoji_len;
                        remaining -= emoji_len;
                    }
                    read = end + 1;
                    continue;
                }
            }
        }

        /* Not an emoji, copy character */
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

