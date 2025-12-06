/**
 * GitHub Emoji Extension for Apex
 */

#ifndef APEX_EMOJI_H
#define APEX_EMOJI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Replace :emoji: patterns with Unicode emoji
 */
char *apex_replace_emoji(const char *html);

#ifdef __cplusplus
}
#endif

#endif /* APEX_EMOJI_H */

