//
// Created by Sbarex on 10/02/26.
//

#include "test_helpers.h"
#include "apex/apex.h"
#include <stdlib.h>


/* cmark-gfm headers */
#include "cmark-gfm.h"
#include <string.h>

static char * my_plugin_callback(const char *text, __attribute__((unused)) apex_plugin *plugin, apex_plugin_phase_mask phase_mask, __attribute__((unused)) const apex_options *options) {
    if (phase_mask & APEX_PLUGIN_PHASE_PRE_PARSE) {
        const char *suffix = "\n_Hello Sbarex_\n";
        size_t len1 = strlen(text);
        size_t len2 = strlen(suffix);

        char *result = malloc(len1 + len2 + 1);
        if (!result) return NULL;

        memcpy(result, text, len1);
        memcpy(result + len1, suffix, len2 + 1); // include '\0'
        return result;
    } else if (phase_mask & APEX_PLUGIN_PHASE_POST_RENDER) {
        const char *suffix = "\n<p>Everything is fine</p>";
        size_t len1 = strlen(text);
        size_t len2 = strlen(suffix);

        char *result = malloc(len1 + len2 + 1);
        if (!result) return NULL;

        memcpy(result, text, len1);
        memcpy(result + len1, suffix, len2 + 1);
        return result;
    }
    return NULL;
}

static void my_plugin_register(apex_plugin_manager *manager, __attribute__((unused)) const apex_options *options) {
    apex_plugin *plugin1 = init_plugin();

    plugin1->id = strdup("my_plugin");
    plugin1->title = strdup("my_plugin title");
    plugin1->author = strdup("sbarex");
    plugin1->description = NULL;
    plugin1->homepage = NULL;
    plugin1->repo = NULL;
    plugin1->phases = APEX_PLUGIN_PHASE_PRE_PARSE;
    plugin1->handler_command = NULL;
    plugin1->priority = 100;
    plugin1->timeout_ms = 0;
    plugin1->has_regex = 0;
    plugin1->replacement = NULL;
    plugin1->replacement = NULL;
    plugin1->dir_path = NULL;
    plugin1->support_dir = NULL;
    plugin1->pattern = NULL;
    plugin1->callback = my_plugin_callback;

    printf(COLOR_GREEN "✓" COLOR_RESET " Custom plugin register callback called\n");

    /* Attach to appropriate phase lists, enforcing per-list id uniqueness */
    if (plugin_register(manager, plugin1)) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Custom plugin has been registered for pre parse\n");
    } else {
        printf(COLOR_RED "✗" COLOR_RESET " Custom plugin has not been registered for pre parse\n");
    }

    apex_plugin *plugin2 = init_plugin();

    plugin2->id = strdup("my_plugin");
    plugin2->title = strdup("my_plugin title");
    plugin2->author = strdup("sbarex");
    plugin2->description = NULL;
    plugin2->homepage = NULL;
    plugin2->repo = NULL;
    plugin2->phases = APEX_PLUGIN_PHASE_POST_RENDER;
    plugin2->handler_command = NULL;
    plugin2->priority = 100;
    plugin2->timeout_ms = 0;
    plugin2->has_regex = 0;
    plugin2->replacement = NULL;
    plugin2->replacement = NULL;
    plugin2->dir_path = NULL;
    plugin2->support_dir = NULL;
    plugin2->pattern = NULL;
    plugin2->callback = my_plugin_callback;

    /* Attach to appropriate phase lists, enforcing per-list id uniqueness */
    if (plugin_register(manager, plugin2)) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Custom plugin has been registered for post render\n");
    } else {
        printf(COLOR_RED "✗" COLOR_RESET " Custom plugin has not been registered for post render\n");
    }
}

void test_custom_plugins(void) {
    int suite_failures = suite_start();
    print_suite_title("Custom plugins Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_plugins = true;
    opts.allow_external_plugin_detection = false;
    opts.plugin_register = my_plugin_register;

    const char *s = "# Custom plugin callback test\n\n";

    char *html;
    html = apex_markdown_to_html(s, strlen(s), &opts);
    assert_contains(html, "<em>Hello Sbarex</em>", "Custom pre parse plugin executed");
    assert_contains(html, "<p>Everything is fine</p>", "Custom post render plugin executed");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Cmark Callbacks Tests", had_failures, false);
}
