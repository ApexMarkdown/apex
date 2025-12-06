/**
 * Custom HTML Renderer for Apex
 * Extends cmark-gfm's HTML renderer to support IAL attributes
 */

#ifndef APEX_HTML_RENDERER_H
#define APEX_HTML_RENDERER_H

#include "cmark-gfm.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Render document to HTML with IAL attribute support
 * This is a wrapper around cmark_render_html that injects attributes
 */
char *apex_render_html_with_attributes(cmark_node *document, int options);

/**
 * Inject header IDs into HTML output
 * @param html The HTML output
 * @param document The AST document
 * @param generate_ids Whether to generate IDs
 * @param use_anchors Whether to use <a> anchor tags instead of header IDs
 * @param id_format 0=GFM (with dashes), 1=MMD (no dashes)
 * @return Newly allocated HTML with IDs injected
 */
char *apex_inject_header_ids(const char *html, cmark_node *document, bool generate_ids, bool use_anchors, int id_format);

/**
 * Clean up HTML tag spacing
 * - Compresses multiple spaces in tags to single spaces
 * - Removes spaces before closing >
 * @param html The HTML to clean
 * @return Newly allocated cleaned HTML (must be freed)
 */
char *apex_clean_html_tag_spacing(const char *html);

/**
 * Convert thead to tbody for relaxed tables
 * Converts <thead><tr><th>...</th></tr></thead> to <tbody><tr><td>...</td></tr></tbody>
 * for tables that were created from relaxed table input (no separator rows)
 * @param html The HTML to process
 * @return Newly allocated HTML with relaxed table thead converted to tbody (must be freed)
 */
char *apex_convert_relaxed_table_headers(const char *html);

/**
 * Remove blank lines within tables
 * Removes lines containing only whitespace/newlines between <table> and </table> tags
 * @param html The HTML to process
 * @return Newly allocated HTML with blank lines removed from tables (must be freed)
 */
char *apex_remove_table_blank_lines(const char *html);

/**
 * Remove table rows that contain only em dashes (separator rows incorrectly rendered as data rows)
 * This happens when smart typography converts --- to — in separator rows
 * @param html The HTML to process
 * @return Newly allocated HTML with separator rows removed (must be freed)
 */
char *apex_remove_table_separator_rows(const char *html);

#ifdef __cplusplus
}
#endif

#endif /* APEX_HTML_RENDERER_H */

