/**
 * Quarto/Pandoc list extensions: (@) continuation, line blocks, roman markers
 */

#ifndef APEX_QUARTO_LISTS_H
#define APEX_QUARTO_LISTS_H

#include <stdbool.h>

/**
 * Remove Pandoc list continuation markers (@) and insert merge hints when needed.
 * Returns newly allocated text, or NULL if unchanged.
 */
char *apex_preprocess_list_continuation(const char *text);

/**
 * Convert Pandoc line blocks (lines starting with |) to HTML div.line-block.
 * Requires unsafe HTML mode for passthrough. Returns NULL if unchanged.
 */
char *apex_preprocess_line_blocks(const char *text, bool unsafe);

/**
 * Convert roman list markers (i), ii), I), etc.) to numbered lists with style hints.
 * Returns NULL if unchanged.
 */
char *apex_preprocess_roman_lists(const char *text);

/**
 * Merge split ordered lists marked with <!-- apex-list-continue -->.
 * Returns NULL if unchanged.
 */
char *apex_postprocess_list_continuation_html(const char *html);

/**
 * Add list-style-type for roman list HTML comment markers.
 * Returns NULL if unchanged.
 */
char *apex_postprocess_roman_lists_html(const char *html);

#endif /* APEX_QUARTO_LISTS_H */
