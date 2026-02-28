/*
 * Man page output: roff (man page source) and styled HTML.
 * Renders cmark AST to .TH/.SH-style roff or a self-contained man-style HTML document.
 */

#include "apex/ast_man.h"
#include <stdlib.h>
#include <string.h>

char *apex_cmark_to_man_roff(cmark_node *document, const apex_options *options)
{
    (void)document;
    (void)options;
    return strdup(".TH stub 1 \"\" \"\"\n");
}

char *apex_cmark_to_man_html(cmark_node *document, const apex_options *options)
{
    (void)document;
    (void)options;
    return strdup("<!DOCTYPE html><html><body><p>stub</p></body></html>");
}
