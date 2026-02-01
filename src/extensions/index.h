/**
 * Index Extension for Apex
 *
 * Supports three index syntaxes:
 * - mmark/MultiMarkdown: (!item), (!item, subitem), (!!item, subitem)
 * - TextIndex: {^}, [term]{^}, {^params}
 * - Leanpub: {i: term}, {i: "term"}, {i: "Main!sub"}
 */

#ifndef APEX_INDEX_H
#define APEX_INDEX_H

#include <stdbool.h>
#include <stddef.h>
#include "../../include/apex/apex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Index syntax types */
typedef enum {
    APEX_INDEX_MMARK = 0,
    APEX_INDEX_TEXTINDEX = 1,
    APEX_INDEX_LEANPUB = 2
} apex_index_syntax_t;

/* Index entry structure */
typedef struct apex_index_entry {
    char *item;                    /* Main index term */
    char *subitem;                 /* Sub-item (optional) */
    bool primary;                  /* Primary entry flag (mmark) */
    int position;                  /* Position in document */
    char *anchor_id;               /* Generated anchor ID (e.g., "idxref-0") */
    apex_index_syntax_t syntax_type;  /* MMARK or TEXTINDEX */
    struct apex_index_entry *next;  /* Linked list */
} apex_index_entry;

/* Index registry */
typedef struct {
    apex_index_entry *entries;     /* Linked list of index entries */
    size_t count;                  /* Number of entries */
    int next_ref_id;               /* Next reference ID for anchors */
} apex_index_registry;

/**
 * Process index entries in text via preprocessing
 * Extracts index entries and stores them in registry
 * Returns modified text with index markers
 */
char *apex_process_index_entries(const char *text, apex_index_registry *registry, const apex_options *options);

/**
 * Render index markers in HTML output
 * Replaces index markers with formatted HTML spans
 */
char *apex_render_index_markers(const char *html, apex_index_registry *registry, const apex_options *options);

/**
 * Generate index HTML from collected entries
 * Returns formatted index HTML
 */
char *apex_generate_index_html(apex_index_registry *registry, const apex_options *options);

/**
 * Insert index at <!--INDEX--> marker or end of document
 * Returns HTML with index inserted
 */
char *apex_insert_index(const char *html, apex_index_registry *registry, const apex_options *options);

/**
 * Free index registry
 */
void apex_free_index_registry(apex_index_registry *registry);

/**
 * Create a new index entry
 */
apex_index_entry *apex_index_entry_new(const char *item, apex_index_syntax_t syntax_type);

/**
 * Free an index entry
 */
void apex_index_entry_free(apex_index_entry *entry);

#ifdef __cplusplus
}
#endif

#endif /* APEX_INDEX_H */
