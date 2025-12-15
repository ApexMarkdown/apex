#ifndef APEX_PLUGINS_H
#define APEX_PLUGINS_H

#include "../include/apex/apex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Plugin phases */
typedef enum {
    APEX_PLUGIN_PHASE_PRE_PARSE  = 1 << 0,
    APEX_PLUGIN_PHASE_BLOCK      = 1 << 1,
    APEX_PLUGIN_PHASE_INLINE     = 1 << 2,
    APEX_PLUGIN_PHASE_POST_RENDER= 1 << 3
} apex_plugin_phase_mask;

typedef struct apex_plugin_manager apex_plugin_manager;

/* Discover and load plugins from project and user config dirs.
 * Returns NULL if no plugins are found or an error occurs. */
apex_plugin_manager *apex_plugins_load(const apex_options *options);

/* Free all plugin resources. */
void apex_plugins_free(apex_plugin_manager *manager);

/* Run all text-based plugins for the given phase over the provided text.
 * Returns newly allocated string on modification, or NULL if no changes.
 */
char *apex_plugins_run_text_phase(apex_plugin_manager *manager,
                                  apex_plugin_phase_mask phase,
                                  const char *text,
                                  const apex_options *options);

#ifdef __cplusplus
}
#endif

#endif /* APEX_PLUGINS_H */
