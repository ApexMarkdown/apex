/**
 * HTML Markdown Attributes Extension for Apex
 *
 * Parse markdown inside HTML tags based on the `markdown` attribute:
 *
 * <div markdown="1">
 * ## This markdown is parsed (block-level)
 * </div>
 *
 * <span markdown="span">*emphasis* works</span>
 *
 * <div markdown="block">
 * Same as markdown="1"
 * </div>
 *
 * <div markdown="0">
 * ## This is literal, not parsed
 * </div>
 */

#ifndef APEX_HTML_MARKDOWN_H
#define APEX_HTML_MARKDOWN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Process HTML tags with markdown attributes (preprocessing)
 * Returns newly allocated string with markdown content parsed
 */
char *apex_process_html_markdown(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* APEX_HTML_MARKDOWN_H */

