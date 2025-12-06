/**
 * Metadata Extension for Apex
 *
 * Supports three metadata formats:
 * - YAML front matter (--- delimited blocks)
 * - MultiMarkdown metadata (key: value pairs)
 * - Pandoc title blocks (% lines)
 */

#ifndef APEX_METADATA_H
#define APEX_METADATA_H

#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Custom node type for metadata blocks */
extern cmark_node_type APEX_NODE_METADATA;

/**
 * Metadata key-value pair structure
 */
typedef struct apex_metadata_item {
    char *key;
    char *value;
    struct apex_metadata_item *next;
} apex_metadata_item;

/**
 * Create and return the metadata extension (stub for now)
 * Metadata is handled via preprocessing rather than as a block extension
 */
cmark_syntax_extension *create_metadata_extension(void);

/**
 * Extract metadata from the beginning of text (preprocessing approach)
 * Modifies *text_ptr to point past the metadata section
 * Returns the extracted metadata list
 */
apex_metadata_item *apex_extract_metadata(char **text_ptr);

/**
 * Get metadata from a document node
 * Returns a linked list of key-value pairs
 */
apex_metadata_item *apex_get_metadata(cmark_node *document);

/**
 * Free metadata list
 */
void apex_free_metadata(apex_metadata_item *metadata);

/**
 * Get a specific metadata value by key (case-insensitive)
 * Returns NULL if not found
 */
const char *apex_metadata_get(apex_metadata_item *metadata, const char *key);

/**
 * Replace [%key] patterns in text with metadata values
 */
char *apex_metadata_replace_variables(const char *text, apex_metadata_item *metadata);

#ifdef __cplusplus
}
#endif

#endif /* APEX_METADATA_H */

