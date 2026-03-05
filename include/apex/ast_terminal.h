/**
 * ast_terminal.h - Convert cmark-gfm AST to ANSI-colored terminal output
 *
 * Supports 8/16-color and 256-color ANSI modes, with optional theming
 * via YAML theme files in ~/.config/apex/terminal/themes/NAME.theme.
 */

#ifndef APEX_AST_TERMINAL_H
#define APEX_AST_TERMINAL_H

#include "apex/apex.h"
#include "cmark-gfm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert a cmark-gfm document node to ANSI-colored terminal output.
 *
 * @param document   Root cmark document node (CMARK_NODE_DOCUMENT)
 * @param options    Apex options (used for mode, theme name, etc.)
 * @param use_256    When true, enable 256-color mode; otherwise 8/16-color
 *
 * @return Newly allocated string with ANSI escape codes, or NULL on error.
 *         Must be freed with apex_free_string (or free()) by the caller.
 */
char *apex_cmark_to_terminal(cmark_node *document,
                             const apex_options *options,
                             bool use_256);

#ifdef __cplusplus
}
#endif

#endif /* APEX_AST_TERMINAL_H */

