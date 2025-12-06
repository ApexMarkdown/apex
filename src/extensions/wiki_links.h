/**
 * Wiki Links Extension for Apex
 *
 * Supports wiki-style link syntax:
 * [[Page Name]]              -> link to page
 * [[Page Name|Display Text]] -> link with custom text
 * [[Page Name#Section]]      -> link to section
 */

#ifndef APEX_WIKI_LINKS_H
#define APEX_WIKI_LINKS_H

#include <stdbool.h>
#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration for wiki link behavior */
typedef struct {
    const char *base_path;      /* Base path for wiki links (e.g., "/wiki/") */
    const char *extension;      /* File extension to append (e.g., ".html") */
    bool spaces_to_underscores; /* Convert spaces to underscores in URLs */
} wiki_link_config;

/**
 * Create and return the wiki links extension (returns NULL - uses postprocessing)
 */
cmark_syntax_extension *create_wiki_links_extension(void);

/**
 * Set wiki link configuration for an extension
 */
void wiki_links_set_config(cmark_syntax_extension *ext, wiki_link_config *config);

/**
 * Process wiki links in an AST via postprocessing
 * Call this after parsing but before rendering
 */
void apex_process_wiki_links_in_tree(cmark_node *document, wiki_link_config *config);

#ifdef __cplusplus
}
#endif

#endif /* APEX_WIKI_LINKS_H */

