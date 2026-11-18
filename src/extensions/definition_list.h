/**
 * One-Line Definition List Extension for Apex
 *
 * Supports: Term :: Definition text  or  Term::Definition text
 * Multiple consecutive lines create one <dl> with multiple <dt>/<dd> pairs.
 */

#ifndef APEX_DEFINITION_LIST_H
#define APEX_DEFINITION_LIST_H

#include <stdbool.h>
#include "cmark-gfm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Process one-line definition lists via preprocessing.
 * Converts "Term :: Definition" lines to <dl><dt>Term</dt><dd>Definition</dd></dl>
 * @param text The markdown text to process
 * @param unsafe If true, allow raw HTML in output
 */
char *apex_process_definition_lists(const char *text, bool unsafe);

/**
 * Move footnote references that were shielded inside definition lists
 * back into the rendered HTML, and drop the temporary reference paragraphs.
 */
void apex_splice_definition_footnotes(cmark_node *root);

/**
 * Replace [^id] outside code with a placeholder and append a real reference
 * to stash. Returns a new string, or NULL when content has no footnote marker.
 * Caller frees the result.
 */
char *apex_shield_footnote_refs(const char *content, size_t len, bool in_code,
                                char **stash, size_t *stash_len, size_t *stash_cap);

/**
 * Debug touch - no-op for one-line format
 */
void apex_deflist_debug_touch(int enable_definition_lists);

#ifdef __cplusplus
}
#endif

#endif /* APEX_DEFINITION_LIST_H */
