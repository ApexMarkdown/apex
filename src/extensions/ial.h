/**
 * Kramdown Inline Attribute Lists (IAL) Extension for Apex
 *
 * Supports:
 * - Block IAL: {: #id .class key="value"} after blocks
 * - Span IAL: {:.class} after spans
 * - ALD (Attribute List Definitions): {:ref-name: #id .class}
 * - References: {: ref-name} to use defined attributes
 */

#ifndef APEX_IAL_H
#define APEX_IAL_H

#include <stdbool.h>
#include "cmark-gfm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Attribute structure
 */
typedef struct apex_attributes {
    char *id;                  /* Element ID */
    char **classes;            /* Array of class names */
    int class_count;
    char **keys;               /* Key-value pairs */
    char **values;
    int attr_count;
} apex_attributes;

/**
 * ALD (Attribute List Definition) entry
 */
typedef struct ald_entry {
    char *name;
    apex_attributes *attrs;
    struct ald_entry *next;
} ald_entry;

/**
 * Extract ALDs from text (preprocessing)
 * Pattern: {:ref-name: #id .class key="value"}
 */
ald_entry *apex_extract_alds(char **text_ptr);

/**
 * Process IAL in AST (postprocessing)
 * Attaches attributes to nodes based on IAL markers
 */
void apex_process_ial_in_tree(cmark_node *document, ald_entry *alds);

/**
 * Free ALD list
 */
void apex_free_alds(ald_entry *alds);

/**
 * Free attributes structure
 */
void apex_free_attributes(apex_attributes *attrs);

#ifdef __cplusplus
}
#endif

#endif /* APEX_IAL_H */

