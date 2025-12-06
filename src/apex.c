/**
 * Apex - Unified Markdown Processor
 * Core implementation
 */

#include "apex/apex.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* cmark-gfm headers */
#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"
#include "cmark-gfm-core-extensions.h"

/* Apex extensions */
#include "extensions/metadata.h"
#include "extensions/wiki_links.h"
#include "extensions/math.h"
#include "extensions/critic.h"
#include "extensions/callouts.h"
#include "extensions/includes.h"
#include "extensions/toc.h"
#include "extensions/abbreviations.h"
#include "extensions/emoji.h"
#include "extensions/special_markers.h"
#include "extensions/ial.h"
#include "extensions/definition_list.h"
#include "extensions/advanced_footnotes.h"
#include "extensions/advanced_tables.h"
#include "extensions/html_markdown.h"
#include "extensions/inline_footnotes.h"
#include "extensions/highlight.h"
#include "extensions/header_ids.h"
#include "extensions/relaxed_tables.h"

/* Custom renderer */
#include "html_renderer.h"

/**
 * Get default options with all features enabled (unified mode)
 */
apex_options apex_options_default(void) {
    apex_options opts;
    opts.mode = APEX_MODE_UNIFIED;

    /* Enable all features by default in unified mode */
    opts.enable_tables = true;
    opts.enable_footnotes = true;
    opts.enable_definition_lists = true;
    opts.enable_smart_typography = true;
    opts.enable_math = true;
    opts.enable_critic_markup = true;
    opts.enable_wiki_links = true;
    opts.enable_task_lists = true;
    opts.enable_attributes = true;
    opts.enable_callouts = true;
    opts.enable_marked_extensions = true;

    /* Critic markup mode (0=accept, 1=reject, 2=markup) */
    opts.critic_mode = 2;  /* Default: show markup */

    /* Metadata */
    opts.strip_metadata = true;
    opts.enable_metadata_variables = true;

    /* File inclusion */
    opts.enable_file_includes = true;
    opts.max_include_depth = 10;
    opts.base_directory = NULL;

    /* Output options */
    opts.unsafe = true;
    opts.validate_utf8 = true;
    opts.github_pre_lang = true;
    opts.standalone = false;
    opts.pretty = false;
    opts.stylesheet_path = NULL;
    opts.document_title = NULL;

    /* Line breaks */
    opts.hardbreaks = false;
    opts.nobreaks = false;

    /* Header IDs */
    opts.generate_header_ids = true;
    opts.header_anchors = false;  /* Use header IDs by default, not anchor tags */
    opts.id_format = 0;  /* GFM format (with dashes) */

    /* Table options */
    opts.relaxed_tables = true;  /* Default: enabled in unified mode (can be disabled with --no-relaxed-tables) */

    return opts;
}

/**
 * Get options configured for a specific processor mode
 */
apex_options apex_options_for_mode(apex_mode_t mode) {
    apex_options opts = apex_options_default();
    opts.mode = mode;

    switch (mode) {
        case APEX_MODE_COMMONMARK:
            /* Pure CommonMark - disable extensions */
            opts.enable_tables = false;
            opts.enable_footnotes = false;
            opts.enable_definition_lists = false;
            opts.enable_smart_typography = false;
            opts.enable_math = false;
            opts.enable_critic_markup = false;
            opts.enable_wiki_links = false;
            opts.enable_task_lists = false;
            opts.enable_attributes = false;
            opts.enable_callouts = false;
            opts.enable_marked_extensions = false;
            opts.enable_file_includes = false;
            opts.enable_metadata_variables = false;
            opts.hardbreaks = false;
            opts.id_format = 0;  /* GFM format (default) */
            break;

        case APEX_MODE_GFM:
            /* GFM - tables, task lists, strikethrough, autolinks */
            opts.enable_tables = true;
            opts.enable_task_lists = true;
            opts.enable_footnotes = false;
            opts.enable_definition_lists = false;
            opts.enable_smart_typography = false;
            opts.enable_math = false;
            opts.enable_critic_markup = false;
            opts.enable_wiki_links = false;
            opts.enable_attributes = false;
            opts.enable_callouts = false;
            opts.enable_marked_extensions = false;
            opts.enable_file_includes = false;
            opts.enable_metadata_variables = false;
            opts.hardbreaks = true;  /* GFM treats newlines as hard breaks */
            opts.id_format = 0;  /* GFM format */
            break;

        case APEX_MODE_MULTIMARKDOWN:
            /* MultiMarkdown - metadata, footnotes, tables, etc. */
            opts.enable_tables = true;
            opts.enable_footnotes = true;
            opts.enable_definition_lists = true;
            opts.enable_smart_typography = true;
            opts.enable_math = true;
            opts.enable_critic_markup = false;
            opts.enable_wiki_links = false;
            opts.enable_task_lists = false;
            opts.enable_attributes = false;
            opts.enable_callouts = false;
            opts.enable_marked_extensions = false;
            opts.enable_file_includes = true;
            opts.enable_metadata_variables = true;
            opts.hardbreaks = false;
            opts.id_format = 1;  /* MMD format */
            break;

        case APEX_MODE_KRAMDOWN:
            /* Kramdown - attributes, definition lists, footnotes */
            opts.enable_tables = true;
            opts.enable_footnotes = true;
            opts.enable_definition_lists = true;
            opts.enable_smart_typography = true;
            opts.enable_math = true;
            opts.enable_critic_markup = false;
            opts.enable_wiki_links = false;
            opts.enable_task_lists = false;
            opts.enable_attributes = true;
            opts.enable_callouts = false;
            opts.enable_marked_extensions = false;
            opts.enable_file_includes = false;
            opts.enable_metadata_variables = false;
            opts.hardbreaks = false;
            opts.id_format = 2;  /* Kramdown format */
            opts.relaxed_tables = true;  /* Kramdown supports relaxed tables */
            break;

        case APEX_MODE_UNIFIED:
            /* All features enabled - already the default */
            /* Unified mode should have everything on */
            opts.enable_wiki_links = true;
            opts.enable_math = true;
            opts.id_format = 0;  /* GFM format (default, can be overridden with --id-format) */
            opts.relaxed_tables = true;  /* Unified mode supports relaxed tables */
            break;
    }

    return opts;
}

/**
 * Convert cmark-gfm option flags based on Apex options
 */
static int apex_to_cmark_options(const apex_options *options) {
    int cmark_opts = CMARK_OPT_DEFAULT;

    if (options->validate_utf8) {
        cmark_opts |= CMARK_OPT_VALIDATE_UTF8;
    }

    if (options->unsafe) {
        cmark_opts |= CMARK_OPT_UNSAFE;
    }

    if (options->hardbreaks) {
        cmark_opts |= CMARK_OPT_HARDBREAKS;
    }

    if (options->nobreaks) {
        cmark_opts |= CMARK_OPT_NOBREAKS;
    }

    if (options->github_pre_lang) {
        cmark_opts |= CMARK_OPT_GITHUB_PRE_LANG;
    }

    if (options->enable_footnotes) {
        cmark_opts |= CMARK_OPT_FOOTNOTES;
    }

    if (options->enable_smart_typography) {
        cmark_opts |= CMARK_OPT_SMART;
    }

    /* Table style preference (use CSS classes instead of inline styles) */
    if (options->enable_tables) {
        /* Tables are handled via extension registration, not options */
        /* We could add CMARK_OPT_TABLE_PREFER_STYLE_ATTRIBUTES here if needed */
    }

    return cmark_opts;
}

/**
 * Register cmark-gfm extensions based on Apex options
 */
static void apex_register_extensions(cmark_parser *parser, const apex_options *options) {
    /* Ensure core extensions are registered */
    cmark_gfm_core_extensions_ensure_registered();

    /* Note: Metadata is handled via preprocessing, not as an extension */

    /* Add GFM extensions as needed */
    if (options->enable_tables) {
        cmark_syntax_extension *table_ext = cmark_find_syntax_extension("table");
        if (table_ext) {
            cmark_parser_attach_syntax_extension(parser, table_ext);
        }
    }

    if (options->enable_task_lists) {
        cmark_syntax_extension *tasklist_ext = cmark_find_syntax_extension("tasklist");
        if (tasklist_ext) {
            cmark_parser_attach_syntax_extension(parser, tasklist_ext);
        }
    }

    /* GFM strikethrough (for all modes that support it) */
    if (options->mode == APEX_MODE_GFM || options->mode == APEX_MODE_UNIFIED) {
        cmark_syntax_extension *strike_ext = cmark_find_syntax_extension("strikethrough");
        if (strike_ext) {
            cmark_parser_attach_syntax_extension(parser, strike_ext);
        }
    }

    /* GFM autolink */
    if (options->mode == APEX_MODE_GFM || options->mode == APEX_MODE_UNIFIED) {
        cmark_syntax_extension *autolink_ext = cmark_find_syntax_extension("autolink");
        if (autolink_ext) {
            cmark_parser_attach_syntax_extension(parser, autolink_ext);
        }
    }

    /* Tag filter (GFM security) */
    if (options->mode == APEX_MODE_GFM || options->mode == APEX_MODE_UNIFIED) {
        cmark_syntax_extension *tagfilter_ext = cmark_find_syntax_extension("tagfilter");
        if (tagfilter_ext) {
            cmark_parser_attach_syntax_extension(parser, tagfilter_ext);
        }
    }

    /* Note: Wiki links are handled via postprocessing, not as an extension */

    /* Math support (LaTeX) */
    if (options->enable_math) {
        cmark_syntax_extension *math_ext = create_math_extension();
        if (math_ext) {
            cmark_parser_attach_syntax_extension(parser, math_ext);
        }
    }

    /* Definition lists (Kramdown/PHP Extra style) */
    if (options->enable_definition_lists) {
        cmark_syntax_extension *deflist_ext = create_definition_list_extension();
        if (deflist_ext) {
            cmark_parser_attach_syntax_extension(parser, deflist_ext);
        }
    }

    /* Advanced footnotes (block-level content support) */
    if (options->enable_footnotes) {
        cmark_syntax_extension *adv_footnotes_ext = create_advanced_footnotes_extension();
        if (adv_footnotes_ext) {
            cmark_parser_attach_syntax_extension(parser, adv_footnotes_ext);
        }
    }

    /* Advanced tables (colspan, rowspan, captions) */
    if (options->enable_tables) {
        cmark_syntax_extension *adv_tables_ext = create_advanced_tables_extension();
        if (adv_tables_ext) {
            cmark_parser_attach_syntax_extension(parser, adv_tables_ext);
        }
    }
}

/**
 * Main conversion function using cmark-gfm
 */
char *apex_markdown_to_html(const char *markdown, size_t len, const apex_options *options) {
    if (!markdown || len == 0) {
        char *empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    /* Use default options if none provided */
    apex_options default_opts;
    if (!options) {
        default_opts = apex_options_default();
        options = &default_opts;
    }

    /* Extract metadata if enabled (preprocessing step) */
    char *working_text = malloc(len + 1);
    if (!working_text) return NULL;
    memcpy(working_text, markdown, len);
    working_text[len] = '\0';

    apex_metadata_item *metadata = NULL;
    abbr_item *abbreviations = NULL;
    ald_entry *alds = NULL;
    char *text_ptr = working_text;


    if (options->mode == APEX_MODE_MULTIMARKDOWN ||
        options->mode == APEX_MODE_KRAMDOWN ||
        options->mode == APEX_MODE_UNIFIED) {
        /* Extract metadata FIRST */
        metadata = apex_extract_metadata(&text_ptr);

        /* Extract ALDs for Kramdown */
        if (options->mode == APEX_MODE_KRAMDOWN || options->mode == APEX_MODE_UNIFIED) {
            alds = apex_extract_alds(&text_ptr);
        }

        /* Extract abbreviations */
        abbreviations = apex_extract_abbreviations(&text_ptr);
    }

    /* Process file includes before parsing (preprocessing) */
    char *includes_processed = NULL;
    if (options->enable_file_includes) {
        includes_processed = apex_process_includes(text_ptr, options->base_directory, 0);
        if (includes_processed) {
            text_ptr = includes_processed;
        }
    }

    /* Process special markers (page breaks, pauses) before parsing */
    char *markers_processed = NULL;
    if (options->enable_marked_extensions) {
        markers_processed = apex_process_special_markers(text_ptr);
        if (markers_processed) {
            text_ptr = markers_processed;
        }
    }

    /* Process inline footnotes before parsing (Kramdown ^[...] and MMD [^... ...]) */
    char *inline_footnotes_processed = NULL;
    if (options->enable_footnotes) {
        inline_footnotes_processed = apex_process_inline_footnotes(text_ptr);
        if (inline_footnotes_processed) {
            text_ptr = inline_footnotes_processed;
        }
    }

    /* Process ==highlight== syntax before parsing */
    char *highlights_processed = apex_process_highlights(text_ptr);
    if (highlights_processed) {
        text_ptr = highlights_processed;
    }

    /* Process relaxed tables before parsing (preprocessing) */
    char *relaxed_tables_processed = NULL;
    if (options->relaxed_tables && options->enable_tables) {
        relaxed_tables_processed = apex_process_relaxed_tables(text_ptr);
        if (relaxed_tables_processed) {
            text_ptr = relaxed_tables_processed;
        }
    }

    /* Process definition lists before parsing (preprocessing) */
    char *deflist_processed = NULL;
    if (options->enable_definition_lists) {
        deflist_processed = apex_process_definition_lists(text_ptr);
        if (deflist_processed) {
            text_ptr = deflist_processed;
        }
    }

    /* Process HTML markdown attributes before parsing (preprocessing) */
    char *html_markdown_processed = NULL;
    html_markdown_processed = apex_process_html_markdown(text_ptr);
    if (html_markdown_processed) {
        text_ptr = html_markdown_processed;
    }

    /* Process Critic Markup before parsing (preprocessing) */
    char *critic_processed = NULL;
    if (options->enable_critic_markup) {
        critic_mode_t critic_mode = (critic_mode_t)options->critic_mode;
        critic_processed = apex_process_critic_markup_text(text_ptr, critic_mode);
        if (critic_processed) {
            text_ptr = critic_processed;
        }
    }

    /* Convert options to cmark-gfm format */
    int cmark_opts = apex_to_cmark_options(options);

    /* Create parser */
    cmark_parser *parser = cmark_parser_new(cmark_opts);
    if (!parser) {
        free(working_text);
        apex_free_metadata(metadata);
        return NULL;
    }

    /* Register extensions based on mode and options */
    apex_register_extensions(parser, options);

    /* Parse the markdown (without metadata) */
    cmark_parser_feed(parser, text_ptr, strlen(text_ptr));
    cmark_node *document = cmark_parser_finish(parser);

    if (!document) {
        cmark_parser_free(parser);
        free(working_text);
        apex_free_metadata(metadata);
        return NULL;
    }

    /* Postprocess wiki links if enabled */
    if (options->enable_wiki_links) {
        apex_process_wiki_links_in_tree(document, NULL);
    }

    /* Postprocess callouts if enabled */
    if (options->enable_callouts) {
        apex_process_callouts_in_tree(document);
    }

    /* Process manual header IDs (MMD [id] and Kramdown {#id}) */
    if (options->generate_header_ids) {
        cmark_iter *iter = cmark_iter_new(document);
        cmark_event_type event;
        while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
            cmark_node *node = cmark_iter_get_node(iter);
            if (event == CMARK_EVENT_ENTER && cmark_node_get_type(node) == CMARK_NODE_HEADING) {
                apex_process_manual_header_id(node);
            }
        }
        cmark_iter_free(iter);
    }

    /* Process IAL (Inline Attribute Lists) if in Kramdown or Unified mode */
    if (alds || options->mode == APEX_MODE_KRAMDOWN || options->mode == APEX_MODE_UNIFIED) {
        apex_process_ial_in_tree(document, alds);
    }

    /* Note: Critic Markup is now handled via preprocessing (before parsing) */

    /* Render to HTML (use custom renderer if IAL is enabled) */
    char *html;
    if (alds || options->mode == APEX_MODE_KRAMDOWN || options->mode == APEX_MODE_UNIFIED) {
        html = apex_render_html_with_attributes(document, cmark_opts);
    } else {
        html = cmark_render_html(document, cmark_opts, NULL);
    }

    /* Post-process HTML for advanced table attributes (rowspan/colspan) */
    if (options->enable_tables && html) {
        extern char *apex_inject_table_attributes(const char *html, cmark_node *document);
        char *processed_html = apex_inject_table_attributes(html, document);
        if (processed_html && processed_html != html) {
            free(html);
            html = processed_html;
        }
    }

    /* Inject header IDs if enabled */
    if (options->generate_header_ids && html) {
        char *processed_html = apex_inject_header_ids(html, document, true, options->header_anchors, options->id_format);
        if (processed_html && processed_html != html) {
            free(html);
            html = processed_html;
        }
    }

    /* Apply metadata variable replacement if enabled */
    if (metadata && options->enable_metadata_variables && html) {
        char *replaced = apex_metadata_replace_variables(html, metadata);
        if (replaced) {
            free(html);
            html = replaced;
        }
    }

    /* Process TOC markers if enabled (Marked extensions) */
    if (options->enable_marked_extensions && html) {
        char *with_toc = apex_process_toc(html, document, options->id_format);
        if (with_toc) {
            free(html);
            html = with_toc;
        }
    }

    /* Replace abbreviations if any were found */
    if (abbreviations && html) {
        char *with_abbrs = apex_replace_abbreviations(html, abbreviations);
        if (with_abbrs) {
            free(html);
            html = with_abbrs;
        }
    }

    /* Replace GitHub emoji if in GFM or Unified mode */
    if ((options->mode == APEX_MODE_GFM || options->mode == APEX_MODE_UNIFIED) && html) {
        char *with_emoji = apex_replace_emoji(html);
        if (with_emoji) {
            free(html);
            html = with_emoji;
        }
    }

    /* Clean up HTML tag spacing (compress multiple spaces, remove spaces before >) */
    if (html) {
        char *cleaned = apex_clean_html_tag_spacing(html);
        if (cleaned) {
            free(html);
            html = cleaned;
        }
    }

    /* Convert thead to tbody for relaxed tables (if relaxed tables were processed) */
    if (html && options->relaxed_tables && options->enable_tables && relaxed_tables_processed) {
        char *converted = apex_convert_relaxed_table_headers(html);
        if (converted) {
            free(html);
            html = converted;
        }
    }

    /* Clean up */
    cmark_node_free(document);
    cmark_parser_free(parser);
    free(working_text);
    if (includes_processed) free(includes_processed);
    if (markers_processed) free(markers_processed);
    if (inline_footnotes_processed) free(inline_footnotes_processed);
    if (highlights_processed) free(highlights_processed);
    if (relaxed_tables_processed) free(relaxed_tables_processed);
    if (deflist_processed) free(deflist_processed);
    if (html_markdown_processed) free(html_markdown_processed);
    if (critic_processed) free(critic_processed);
    apex_free_metadata(metadata);
    apex_free_abbreviations(abbreviations);
    apex_free_alds(alds);

    /* Wrap in complete HTML document if requested */
    if (options->standalone && html) {
        char *document = apex_wrap_html_document(html, options->document_title, options->stylesheet_path);
        if (document) {
            free(html);
            html = document;
        }
    }

    /* Remove blank lines within tables (applies to both pretty and non-pretty) */
    if (html) {
        char *cleaned = apex_remove_table_blank_lines(html);
        if (cleaned) {
            free(html);
            html = cleaned;
        }
    }

    /* Remove table separator rows that were incorrectly rendered as data rows */
    /* This happens when smart typography converts --- to — in separator rows */
    if (html && options->enable_tables) {
        extern char *apex_remove_table_separator_rows(const char *html);
        char *cleaned = apex_remove_table_separator_rows(html);
        if (cleaned) {
            free(html);
            html = cleaned;
        }
    }

    /* Pretty-print HTML if requested */
    if (options->pretty && html) {
        char *pretty = apex_pretty_print_html(html);
        if (pretty) {
            free(html);
            html = pretty;
        }
    }

    return html;
}

/**
 * Wrap HTML content in complete HTML5 document structure
 */
char *apex_wrap_html_document(const char *content, const char *title, const char *stylesheet_path) {
    if (!content) return NULL;

    const char *doc_title = title ? title : "Document";

    /* Calculate buffer size */
    size_t content_len = strlen(content);
    size_t title_len = strlen(doc_title);
    size_t style_len = stylesheet_path ? strlen(stylesheet_path) : 0;
    /* Need generous space for styles (1.5KB) + structure */
    size_t capacity = content_len + title_len + style_len + 4096;

    char *output = malloc(capacity);
    if (!output) return strdup(content);

    char *write = output;

    /* HTML5 doctype and opening */
    write += sprintf(write, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n");

    /* Meta tags */
    write += sprintf(write, "  <meta charset=\"UTF-8\">\n");
    write += sprintf(write, "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    write += sprintf(write, "  <meta name=\"generator\" content=\"Apex %s\">\n", APEX_VERSION_STRING);

    /* Title */
    write += sprintf(write, "  <title>%s</title>\n", doc_title);

    /* Stylesheet link if provided */
    if (stylesheet_path) {
        write += sprintf(write, "  <link rel=\"stylesheet\" href=\"%s\">\n", stylesheet_path);
    } else {
        /* Include minimal default styles */
        write += sprintf(write, "  <style>\n");
        write += sprintf(write, "    body {\n");
        write += sprintf(write, "      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif;\n");
        write += sprintf(write, "      line-height: 1.6;\n");
        write += sprintf(write, "      max-width: 800px;\n");
        write += sprintf(write, "      margin: 2rem auto;\n");
        write += sprintf(write, "      padding: 0 1rem;\n");
        write += sprintf(write, "      color: #333;\n");
        write += sprintf(write, "    }\n");
        write += sprintf(write, "    pre { background: #f5f5f5; padding: 1rem; overflow-x: auto; }\n");
        write += sprintf(write, "    code { background: #f0f0f0; padding: 0.2em 0.4em; border-radius: 3px; }\n");
        write += sprintf(write, "    blockquote { border-left: 4px solid #ddd; margin: 0; padding-left: 1rem; color: #666; }\n");
        write += sprintf(write, "    table { border-collapse: collapse; width: 100%%; }\n");
        write += sprintf(write, "    th, td { border: 1px solid #ddd; padding: 0.5rem; }\n");
        write += sprintf(write, "    th { background: #f5f5f5; }\n");
        write += sprintf(write, "    .page-break { page-break-after: always; }\n");
        write += sprintf(write, "    .callout { padding: 1rem; margin: 1rem 0; border-left: 4px solid; }\n");
        write += sprintf(write, "    .callout-note { border-color: #3b82f6; background: #eff6ff; }\n");
        write += sprintf(write, "    .callout-warning { border-color: #f59e0b; background: #fffbeb; }\n");
        write += sprintf(write, "    .callout-tip { border-color: #10b981; background: #f0fdf4; }\n");
        write += sprintf(write, "    .callout-danger { border-color: #ef4444; background: #fef2f2; }\n");
        write += sprintf(write, "    ins { background: #d4fcbc; text-decoration: none; }\n");
        write += sprintf(write, "    del { background: #fbb6c2; text-decoration: line-through; }\n");
        write += sprintf(write, "    mark { background: #fff3cd; }\n");
        write += sprintf(write, "    .critic.comment { background: #e7e7e7; color: #666; font-style: italic; }\n");
        write += sprintf(write, "  </style>\n");
    }

    /* Close head, open body */
    write += sprintf(write, "</head>\n<body>\n\n");

    /* Content */
    strcpy(write, content);
    write += content_len;

    /* Close body and html */
    write += sprintf(write, "\n</body>\n</html>\n");

    return output;
}

/**
 * Free a string allocated by Apex
 */
void apex_free_string(char *str) {
    if (str) {
        free(str);
    }
}

/**
 * Version information
 */
const char *apex_version_string(void) {
    return APEX_VERSION_STRING;
}

int apex_version_major(void) {
    return APEX_VERSION_MAJOR;
}

int apex_version_minor(void) {
    return APEX_VERSION_MINOR;
}

int apex_version_patch(void) {
    return APEX_VERSION_PATCH;
}
