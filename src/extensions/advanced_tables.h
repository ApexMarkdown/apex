/**
 * Advanced Tables Extension for Apex
 *
 * Extends cmark-gfm tables with:
 * - Column spans (empty cells or << marker)
 * - Row spans (^^ marker)
 * - Table captions ([Caption] before/after table)
 * - Multi-line cells (with \\ marker in headers)
 *
 * This is a postprocessing extension that enhances parsed tables
 * without modifying the core table parser, ensuring compatibility.
 */

#ifndef APEX_ADVANCED_TABLES_H
#define APEX_ADVANCED_TABLES_H

#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Post-process tables to add advanced features
 * This walks the AST and enhances table nodes
 */
cmark_node *apex_process_advanced_tables(cmark_node *root);

/**
 * Create advanced tables extension
 */
cmark_syntax_extension *create_advanced_tables_extension(void);

#ifdef __cplusplus
}
#endif

#endif /* APEX_ADVANCED_TABLES_H */

