/**
 * Definition List Extension for Apex
 *
 * Supports Kramdown/PHP Markdown Extra style definition lists:
 * Term
 * : Definition 1
 * : Definition 2
 */

#ifndef APEX_DEFINITION_LIST_H
#define APEX_DEFINITION_LIST_H

#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Custom node types for definition lists */
extern cmark_node_type APEX_NODE_DEFINITION_LIST;
extern cmark_node_type APEX_NODE_DEFINITION_TERM;
extern cmark_node_type APEX_NODE_DEFINITION_DATA;

/**
 * Process definition lists via preprocessing
 * Converts : syntax to HTML before main parsing
 */
char *apex_process_definition_lists(const char *text);

/**
 * Create and return the definition list extension
 */
cmark_syntax_extension *create_definition_list_extension(void);

#ifdef __cplusplus
}
#endif

#endif /* APEX_DEFINITION_LIST_H */

