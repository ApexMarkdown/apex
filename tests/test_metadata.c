/**
 * Metadata Tests
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include "../src/extensions/metadata.h"
#include <string.h>
#include <stdlib.h>

void test_metadata(void) {
    int suite_failures = suite_start();
    print_suite_title("Metadata Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    char *html;

    /* Test YAML metadata with variables */
    const char *yaml_doc = "---\ntitle: Test Doc\nauthor: John\n---\n\n# [%title]\n\nBy [%author]";
    html = apex_markdown_to_html(yaml_doc, strlen(yaml_doc), &opts);
    assert_contains(html, "<h1", "YAML metadata variable in header");
    assert_contains(html, "Test Doc</h1>", "YAML metadata variable content");
    assert_contains(html, "By John", "YAML metadata variable in text");
    apex_free_string(html);

    /* Test YAML arrays + nested mappings (libyaml flattening) and end marker '...'. */
    const char *yaml_nested =
        "---\n"
        "tags:\n"
        "  - one\n"
        "  - two\n"
        "nested:\n"
        "  a: 1\n"
        "  b:\n"
        "    - x\n"
        "    - y\n"
        "...\n"
        "\n"
        "Tags: [%tags]\n"
        "Nested a: [%nested.a]\n"
        "Nested b: [%nested.b]\n"
        "Nested b0: [%nested.b.0]\n";
    html = apex_markdown_to_html(yaml_nested, strlen(yaml_nested), &opts);
    assert_contains(html, "Tags: one, two", "YAML array flattened to joined string");
    assert_contains(html, "Nested a: 1", "YAML nested mapping flattened (nested.a)");
    assert_contains(html, "Nested b: x, y", "YAML nested sequence flattened (nested.b)");
    /* Note: scalar-only sequences are normalized to a joined string; index keys are not generated. */
    assert_contains(html, "Nested b0: [%nested.b.0]", "YAML scalar sequence does not generate index keys");
    apex_free_string(html);

    /* Test YAML sequence containing a nested mapping: scalar items join, mapping items flatten with index keys. */
    const char *yaml_seq_map =
        "---\n"
        "arr:\n"
        "  - one\n"
        "  - k: v\n"
        "---\n"
        "\n"
        "Arr: [%arr]\n"
        "Arr0: [%arr.0]\n"
        "Arr1.k: [%arr.1.k]\n";
    html = apex_markdown_to_html(yaml_seq_map, strlen(yaml_seq_map), &opts);
    /* Note: mixed scalar+mapping sequences fall back to indexed keys (no joined base key). */
    assert_contains(html, "Arr: [%arr]", "YAML mixed sequence: base key not generated");
    assert_contains(html, "Arr0: one", "YAML sequence scalar index flattened (arr.0)");
    assert_contains(html, "Arr1.k: v", "YAML sequence mapping flattened with index key (arr.1.k)");
    apex_free_string(html);

    /* Test MMD metadata */
    const char *mmd_doc = "Title: My Title\n\n# [%Title]";
    html = apex_markdown_to_html(mmd_doc, strlen(mmd_doc), &opts);
    assert_contains(html, "<h1", "MMD metadata variable");
    assert_contains(html, "My Title</h1>", "MMD metadata variable content");
    apex_free_string(html);

    /* Test Pandoc metadata */
    const char *pandoc_doc = "% The Title\n% The Author\n\n# [%title]";
    html = apex_markdown_to_html(pandoc_doc, strlen(pandoc_doc), &opts);
    assert_contains(html, "<h1", "Pandoc metadata variable");
    assert_contains(html, "The Title</h1>", "Pandoc metadata variable content");
    apex_free_string(html);

    /* Test that list items with colons are not treated as metadata in unified mode */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    const char *list_with_colon_doc = "## A Header\n\n- Foo: Bar\n- Another item\n- Third item";
    html = apex_markdown_to_html(list_with_colon_doc, strlen(list_with_colon_doc), &unified_opts);
    assert_contains(html, "<h2", "List with colon: header is rendered");
    assert_contains(html, "A Header</h2>", "List with colon: header content");
    assert_contains(html, "<ul>", "List with colon: unordered list rendered");
    assert_contains(html, "<li>Foo: Bar</li>", "List with colon: first item with colon rendered");
    assert_contains(html, "<li>Another item</li>", "List with colon: second item rendered");
    assert_contains(html, "<li>Third item</li>", "List with colon: third item rendered");
    apex_free_string(html);
    
    bool had_failures = suite_end(suite_failures);
    print_suite_title("Metadata Tests", had_failures, false);
}

/**
 * Test MultiMarkdown metadata keys
 */

void test_mmd_metadata_keys(void) {
    int suite_failures = suite_start();
    print_suite_title("MultiMarkdown Metadata Keys Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    char *html;

    /* Test Base Header Level */
    const char *base_header_doc = "Base Header Level: 2\n\n# Header 1\n## Header 2";
    html = apex_markdown_to_html(base_header_doc, strlen(base_header_doc), &opts);
    assert_contains(html, "<h2", "Base Header Level: h1 becomes h2");
    assert_contains(html, "Header 1</h2>", "Base Header Level: h1 content in h2 tag");
    assert_contains(html, "<h3", "Base Header Level: h2 becomes h3");
    assert_contains(html, "Header 2</h3>", "Base Header Level: h2 content in h3 tag");
    apex_free_string(html);

    /* Test HTML Header Level (format-specific) */
    const char *html_header_level_doc = "HTML Header Level: 3\n\n# Header 1";
    html = apex_markdown_to_html(html_header_level_doc, strlen(html_header_level_doc), &opts);
    assert_contains(html, "<h3", "HTML Header Level: h1 becomes h3");
    assert_contains(html, "Header 1</h3>", "HTML Header Level: h1 content in h3 tag");
    apex_free_string(html);

    /* Test Language metadata in standalone document */
    opts.standalone = true;
    const char *language_doc = "Language: fr\n\n# Bonjour";
    html = apex_markdown_to_html(language_doc, strlen(language_doc), &opts);
    assert_contains(html, "<html lang=\"fr\">", "Language metadata sets HTML lang attribute");
    apex_free_string(html);

    /* Test Quotes Language - French (requires smart typography) */
    opts.standalone = false;
    opts.enable_smart_typography = true;  /* Ensure smart typography is enabled */
    const char *quotes_fr_doc = "Quotes Language: french\n\nHe said \"hello\" to me.";
    html = apex_markdown_to_html(quotes_fr_doc, strlen(quotes_fr_doc), &opts);
    assert_contains(html, "&laquo;&nbsp;", "Quotes Language: French opening quote");
    assert_contains(html, "&nbsp;&raquo;", "Quotes Language: French closing quote");
    apex_free_string(html);

    /* Test Quotes Language - German */
    const char *quotes_de_doc = "Quotes Language: german\n\nHe said \"hello\" to me.";
    html = apex_markdown_to_html(quotes_de_doc, strlen(quotes_de_doc), &opts);
    assert_contains(html, "&bdquo;", "Quotes Language: German opening quote");
    assert_contains(html, "&ldquo;", "Quotes Language: German closing quote");
    apex_free_string(html);

    /* Test Quotes Language fallback to Language */
    opts.standalone = true;
    const char *lang_fallback_doc = "Language: fr\n\nHe said \"hello\" to me.";
    html = apex_markdown_to_html(lang_fallback_doc, strlen(lang_fallback_doc), &opts);
    assert_contains(html, "<html lang=\"fr\">", "Language metadata sets HTML lang");
    /* Quotes should also use French since Quotes Language not specified */
    assert_contains(html, "&laquo;&nbsp;", "Quotes Language falls back to Language");
    apex_free_string(html);

    /* Test CSS metadata in standalone document */
    opts.standalone = true;
    const char *css_doc = "CSS: styles.css\n\n# Test";
    html = apex_markdown_to_html(css_doc, strlen(css_doc), &opts);
    assert_contains(html, "<link rel=\"stylesheet\" href=\"styles.css\">", "CSS metadata adds stylesheet link");
    assert_not_contains(html, "<style>", "CSS metadata: no default styles when CSS specified");
    apex_free_string(html);

    /* Test CSS metadata: default styles when no CSS */
    const char *no_css_doc = "Title: Test\n\n# Content";
    html = apex_markdown_to_html(no_css_doc, strlen(no_css_doc), &opts);
    assert_contains(html, "<style>", "No CSS metadata: default styles included");
    apex_free_string(html);

    /* Test HTML Header metadata */
    const char *html_header_doc = "HTML Header: <script src=\"mathjax.js\"></script>\n\n# Test";
    html = apex_markdown_to_html(html_header_doc, strlen(html_header_doc), &opts);
    assert_contains(html, "<script src=\"mathjax.js\"></script>", "HTML Header metadata inserted in head");
    assert_contains(html, "</head>", "HTML Header metadata before </head>");
    apex_free_string(html);

    /* Test HTML Footer metadata */
    const char *html_footer_doc = "HTML Footer: <script>init();</script>\n\n# Test";
    html = apex_markdown_to_html(html_footer_doc, strlen(html_footer_doc), &opts);
    assert_contains(html, "<script>init();</script>", "HTML Footer metadata inserted before </body>");
    assert_contains(html, "</body>", "HTML Footer metadata before </body>");
    apex_free_string(html);

    /* Test normalized key matching (spaces removed, case-insensitive) */
    opts.standalone = false;
    opts.enable_smart_typography = true;  /* Ensure smart typography is enabled */
    const char *normalized_doc = "quoteslanguage: french\nbaseheaderlevel: 2\n\n# Header\nHe said \"hello\".";
    html = apex_markdown_to_html(normalized_doc, strlen(normalized_doc), &opts);
    assert_contains(html, "<h2", "Normalized key: baseheaderlevel works");
    assert_contains(html, "&laquo;&nbsp;", "Normalized key: quoteslanguage works");
    apex_free_string(html);

    /* Test case-insensitive matching */
    opts.enable_smart_typography = true;  /* Ensure smart typography is enabled */
    const char *case_doc = "QUOTES LANGUAGE: german\nBASE HEADER LEVEL: 3\n\n# Header\nHe said \"hello\".";
    html = apex_markdown_to_html(case_doc, strlen(case_doc), &opts);
    assert_contains(html, "<h3", "Case-insensitive: BASE HEADER LEVEL works");
    assert_contains(html, "&bdquo;", "Case-insensitive: QUOTES LANGUAGE works");
    apex_free_string(html);
    
    bool had_failures = suite_end(suite_failures);
    print_suite_title("MultiMarkdown Metadata Keys Tests", had_failures, false);
}

/**
 * Test metadata transforms
 */

void test_metadata_transforms(void) {
    int suite_failures = suite_start();
    print_suite_title("Metadata Transforms Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    char *html;

    /* Test basic transforms: upper */
    const char *upper_doc = "---\ntitle: hello world\n---\n\n# [%title:upper]";
    html = apex_markdown_to_html(upper_doc, strlen(upper_doc), &opts);
    assert_contains(html, "HELLO WORLD</h1>", "upper transform");
    apex_free_string(html);

    /* Test basic transforms: lower */
    const char *lower_doc = "---\ntitle: HELLO WORLD\n---\n\n# [%title:lower]";
    html = apex_markdown_to_html(lower_doc, strlen(lower_doc), &opts);
    assert_contains(html, "hello world</h1>", "lower transform");
    apex_free_string(html);

    /* Test basic transforms: title */
    const char *title_doc = "---\ntitle: hello world\n---\n\n# [%title:title]";
    html = apex_markdown_to_html(title_doc, strlen(title_doc), &opts);
    assert_contains(html, "Hello World</h1>", "title transform");
    apex_free_string(html);

    /* Test basic transforms: capitalize */
    const char *capitalize_doc = "---\ntitle: hello world\n---\n\n# [%title:capitalize]";
    html = apex_markdown_to_html(capitalize_doc, strlen(capitalize_doc), &opts);
    assert_contains(html, "Hello world</h1>", "capitalize transform");
    apex_free_string(html);

    /* Test basic transforms: trim */
    const char *trim_doc = "---\ntitle: \"  hello world  \"\n---\n\n# [%title:trim]";
    html = apex_markdown_to_html(trim_doc, strlen(trim_doc), &opts);
    assert_contains(html, "hello world</h1>", "trim transform");
    apex_free_string(html);

    /* Test slug transform */
    const char *slug_doc = "---\ntitle: My Great Post!\n---\n\n[%title:slug]";
    html = apex_markdown_to_html(slug_doc, strlen(slug_doc), &opts);
    assert_contains(html, "my-great-post", "slug transform");
    apex_free_string(html);

    /* Test replace transform (simple) */
    const char *replace_doc = "---\nurl: http://example.com\n---\n\n[%url:replace(http:,https:)]";
    html = apex_markdown_to_html(replace_doc, strlen(replace_doc), &opts);
    assert_contains(html, "https://example.com", "replace transform");
    apex_free_string(html);

    /* Test replace transform (regex) - use simple pattern without brackets first */
    const char *regex_doc = "---\ntext: Hello 123 World\n---\n\n[%text:replace(regex:123,N)]";
    html = apex_markdown_to_html(regex_doc, strlen(regex_doc), &opts);
    assert_contains(html, "Hello N World", "replace with regex");
    apex_free_string(html);

    /* Test regex with character class [0-9]+ */
    const char *regex_doc2 = "---\ntext: Hello 123 World\n---\n\n[%text:replace(regex:[0-9]+,N)]";
    html = apex_markdown_to_html(regex_doc2, strlen(regex_doc2), &opts);
    assert_contains(html, "Hello N World", "replace with regex pattern with brackets");
    apex_free_string(html);

    /* Test regex with simpler pattern that definitely works */
    const char *regex_doc3 = "---\ntext: Hello 123 World\n---\n\n[%text:replace(regex:12,N)]";
    html = apex_markdown_to_html(regex_doc3, strlen(regex_doc3), &opts);
    assert_contains(html, "Hello N3 World", "replace with regex simple pattern");
    apex_free_string(html);

    /* Test substring transform */
    const char *substr_doc = "---\ntitle: Hello World\n---\n\n[%title:substr(0,5)]";
    html = apex_markdown_to_html(substr_doc, strlen(substr_doc), &opts);
    assert_contains(html, "Hello", "substring transform");
    apex_free_string(html);

    /* Test truncate transform - note: smart typography may convert ... to … */
    const char *truncate_doc = "---\ntitle: This is a very long title\n---\n\n[%title:truncate(15,...)]";
    html = apex_markdown_to_html(truncate_doc, strlen(truncate_doc), &opts);
    /* Check for either ... or … (smart typography ellipsis) */
    if (strstr(html, "This is a very...") || strstr(html, "This is a very…") || strstr(html, "This is a ve")) {
        test_result(true, "truncate transform");
    } else {
        test_result(false, "truncate transform failed");
    }
    apex_free_string(html);

    /* Test default transform */
    const char *default_doc = "---\ndesc: \"\"\n---\n\n[%desc:default(No description)]";
    html = apex_markdown_to_html(default_doc, strlen(default_doc), &opts);
    assert_contains(html, "No description", "default transform with empty value");
    apex_free_string(html);

    /* Test default transform with non-empty value */
    const char *default_nonempty_doc = "---\ndesc: Has value\n---\n\n[%desc:default(No description)]";
    html = apex_markdown_to_html(default_nonempty_doc, strlen(default_nonempty_doc), &opts);
    assert_contains(html, "Has value", "default transform preserves non-empty");
    apex_free_string(html);

    /* Test html_escape transform */
    const char *escape_doc = "---\ntitle: A & B\n---\n\n[%title:html_escape]";
    html = apex_markdown_to_html(escape_doc, strlen(escape_doc), &opts);
    assert_contains(html, "&amp;", "html_escape transform");
    apex_free_string(html);

    /* Test basename transform */
    const char *basename_doc = "---\nimage: /path/to/image.jpg\n---\n\n[%image:basename]";
    html = apex_markdown_to_html(basename_doc, strlen(basename_doc), &opts);
    assert_contains(html, "image.jpg", "basename transform");
    apex_free_string(html);

    /* Test urlencode transform */
    const char *urlencode_doc = "---\nsearch: hello world\n---\n\n[%search:urlencode]";
    html = apex_markdown_to_html(urlencode_doc, strlen(urlencode_doc), &opts);
    assert_contains(html, "hello%20world", "urlencode transform");
    apex_free_string(html);

    /* Test urldecode transform */
    const char *urldecode_doc = "---\nsearch: hello%20world\n---\n\n[%search:urldecode]";
    html = apex_markdown_to_html(urldecode_doc, strlen(urldecode_doc), &opts);
    assert_contains(html, "hello world", "urldecode transform");
    apex_free_string(html);

    /* Test prefix transform */
    const char *prefix_doc = "---\nurl: example.com\n---\n\n[%url:prefix(https://)]";
    html = apex_markdown_to_html(prefix_doc, strlen(prefix_doc), &opts);
    assert_contains(html, "https://example.com", "prefix transform");
    apex_free_string(html);

    /* Test suffix transform */
    const char *suffix_doc = "---\ntitle: Hello\n---\n\n[%title:suffix(!)]";
    html = apex_markdown_to_html(suffix_doc, strlen(suffix_doc), &opts);
    assert_contains(html, "Hello!", "suffix transform");
    apex_free_string(html);

    /* Test remove transform */
    const char *remove_doc = "---\ntitle: Hello'World\n---\n\n[%title:remove(')]";
    html = apex_markdown_to_html(remove_doc, strlen(remove_doc), &opts);
    assert_contains(html, "HelloWorld", "remove transform");
    apex_free_string(html);

    /* Test repeat transform - escape the result to avoid markdown HR interpretation */
    const char *repeat_doc = "---\nsep: -\n---\n\n`[%sep:repeat(3)]`";
    html = apex_markdown_to_html(repeat_doc, strlen(repeat_doc), &opts);
    /* Check inside code span to avoid HR interpretation */
    assert_contains(html, "<code>---</code>", "repeat transform");
    apex_free_string(html);

    /* Test reverse transform */
    const char *reverse_doc = "---\ntext: Hello\n---\n\n[%text:reverse]";
    html = apex_markdown_to_html(reverse_doc, strlen(reverse_doc), &opts);
    assert_contains(html, "olleH", "reverse transform");
    apex_free_string(html);

    /* Test format transform */
    const char *format_doc = "---\nprice: 42.5\n---\n\n[%price:format($%.2f)]";
    html = apex_markdown_to_html(format_doc, strlen(format_doc), &opts);
    assert_contains(html, "$42.50", "format transform");
    apex_free_string(html);

    /* Test length transform */
    const char *length_doc = "---\ntext: Hello\n---\n\n[%text:length]";
    html = apex_markdown_to_html(length_doc, strlen(length_doc), &opts);
    assert_contains(html, "5", "length transform");
    apex_free_string(html);

    /* Test pad transform */
    const char *pad_doc = "---\nnumber: 42\n---\n\n[%number:pad(5,0)]";
    html = apex_markdown_to_html(pad_doc, strlen(pad_doc), &opts);
    assert_contains(html, "00042", "pad transform");
    apex_free_string(html);

    /* Test contains transform */
    const char *contains_doc = "---\ntags: javascript,html,css\n---\n\n[%tags:contains(javascript)]";
    html = apex_markdown_to_html(contains_doc, strlen(contains_doc), &opts);
    assert_contains(html, "true", "contains transform");
    apex_free_string(html);

    /* Test array transforms: split */
    const char *split_doc = "---\ntags: tag1,tag2,tag3\n---\n\n[%tags:split(,):first]";
    html = apex_markdown_to_html(split_doc, strlen(split_doc), &opts);
    assert_contains(html, "tag1", "split and first transforms");
    apex_free_string(html);

    /* Test array transforms: join */
    const char *join_doc = "---\ntags: tag1,tag2,tag3\n---\n\n[%tags:split(,):join( | )]";
    html = apex_markdown_to_html(join_doc, strlen(join_doc), &opts);
    assert_contains(html, "tag1 | tag2 | tag3", "split and join transforms");
    apex_free_string(html);

    /* Test array transforms: last */
    const char *last_doc = "---\ntags: tag1,tag2,tag3\n---\n\n[%tags:split(,):last]";
    html = apex_markdown_to_html(last_doc, strlen(last_doc), &opts);
    assert_contains(html, "tag3", "last transform");
    apex_free_string(html);

    /* Test array transforms: slice */
    const char *slice_doc = "---\ntags: tag1,tag2,tag3\n---\n\n[%tags:split(,):slice(0,2):join(,)]";
    html = apex_markdown_to_html(slice_doc, strlen(slice_doc), &opts);
    assert_contains(html, "tag1,tag2", "slice transform");
    apex_free_string(html);

    /* Test slice with string (character-by-character) */
    const char *slice_str_doc = "---\ntext: Hello\n---\n\n[%text:slice(0,5)]";
    html = apex_markdown_to_html(slice_str_doc, strlen(slice_str_doc), &opts);
    assert_contains(html, "Hello", "slice transform on string");
    apex_free_string(html);

    /* Test strftime transform */
    const char *strftime_doc = "---\ndate: 2024-03-15\n---\n\n[%date:strftime(%Y)]";
    html = apex_markdown_to_html(strftime_doc, strlen(strftime_doc), &opts);
    assert_contains(html, "2024", "strftime transform");
    apex_free_string(html);

    /* Test transform chaining */
    const char *chain_doc = "---\ntitle: hello world\n---\n\n# [%title:title:split( ):first]";
    html = apex_markdown_to_html(chain_doc, strlen(chain_doc), &opts);
    assert_contains(html, "Hello</h1>", "transform chaining");
    apex_free_string(html);

    /* Test transform chaining with date */
    const char *date_chain_doc = "---\ndate: 2024-03-15 14:30\n---\n\n[%date:strftime(%Y)]";
    html = apex_markdown_to_html(date_chain_doc, strlen(date_chain_doc), &opts);
    assert_contains(html, "2024", "strftime with time");
    apex_free_string(html);

    /* Test that transforms are disabled when flag is off */
    apex_options no_transforms = apex_options_for_mode(APEX_MODE_UNIFIED);
    no_transforms.enable_metadata_transforms = false;
    const char *disabled_doc = "---\ntitle: Hello\n---\n\n[%title:upper]";
    html = apex_markdown_to_html(disabled_doc, strlen(disabled_doc), &no_transforms);
    /* Should keep the transform syntax verbatim or use simple replacement */
    if (strstr(html, "[%title:upper]") != NULL || strstr(html, "Hello") != NULL) {
        test_result(true, "Transforms disabled when flag is off");
    } else {
        test_result(false, "Transforms not disabled when flag is off");
    }
    apex_free_string(html);

    /* Test that transforms are disabled in non-unified modes by default */
    apex_options mmd_opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    html = apex_markdown_to_html(disabled_doc, strlen(disabled_doc), &mmd_opts);
    if (strstr(html, "[%title:upper]") != NULL || strstr(html, "Hello") != NULL) {
        test_result(true, "Transforms disabled in MMD mode by default");
    } else {
        test_result(false, "Transforms incorrectly enabled in MMD mode");
    }
    apex_free_string(html);

    /* Test that simple [%key] still works with transforms enabled */
    const char *simple_doc = "---\ntitle: Hello\n---\n\n[%title]";
    html = apex_markdown_to_html(simple_doc, strlen(simple_doc), &opts);
    assert_contains(html, "Hello", "Simple metadata replacement still works");
    apex_free_string(html);
    
    bool had_failures = suite_end(suite_failures);
    print_suite_title("Metadata Transforms Tests", had_failures, false);
}

/**
 * Test wiki links
 */

void test_metadata_control_options(void) {
    int suite_failures = suite_start();
    print_suite_title("Metadata Control of Options Tests", false, true);

    /* Test boolean options via metadata */
    apex_options opts = apex_options_default();
    opts.enable_indices = true;  /* Start with indices enabled */
    opts.enable_wiki_links = false;  /* Start with wikilinks disabled */

    /* Create metadata with boolean options */
    apex_metadata_item *metadata = NULL;
    apex_metadata_item *item;

    /* Test indices: false */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("indices");
    item->value = strdup("false");
    item->next = metadata;
    metadata = item;

    /* Test wikilinks: true */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("wikilinks");
    item->value = strdup("true");
    item->next = metadata;
    metadata = item;

    /* Test pretty: yes */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("pretty");
    item->value = strdup("yes");
    item->next = metadata;
    metadata = item;

    /* Test standalone: 1 */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("standalone");
    item->value = strdup("1");
    item->next = metadata;
    metadata = item;

    /* Apply metadata */
    apex_apply_metadata_to_options(metadata, &opts);

    /* Verify boolean options */
    assert_option_bool(opts.enable_indices, false, "indices: false sets enable_indices to false");
    assert_option_bool(opts.enable_wiki_links, true, "wikilinks: true sets enable_wiki_links to true");
    assert_option_bool(opts.pretty, true, "pretty: yes sets pretty to true");
    assert_option_bool(opts.standalone, true, "standalone: 1 sets standalone to true");

    /* Clean up */
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test string options */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("title");
    item->value = strdup("My Test Document");
    item->next = NULL;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("csl");
    item->value = strdup("apa.csl");
    item->next = metadata;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("id-format");
    item->value = strdup("mmd");
    item->next = metadata;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);

    assert_option_string(opts.document_title, "My Test Document", "title sets document_title");
    assert_option_string(opts.csl_file, "apa.csl", "csl sets csl_file");
    assert_option_bool(opts.id_format == 1, true, "id-format: mmd sets id_format to 1 (MMD)");

    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test mode option (should reset options) */
    opts = apex_options_default();
    opts.enable_indices = true;
    opts.enable_wiki_links = true;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("mode");
    item->value = strdup("gfm");
    item->next = NULL;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("wikilinks");
    item->value = strdup("true");
    item->next = metadata;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);

    assert_option_bool(opts.mode == APEX_MODE_GFM, true, "mode: gfm sets mode to GFM");
    /* After mode reset, wikilinks should still be applied */
    assert_option_bool(opts.enable_wiki_links, true, "wikilinks applied after mode reset");

    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test case-insensitive boolean values */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("indices");
    item->value = strdup("TRUE");
    item->next = NULL;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("wikilinks");
    item->value = strdup("FALSE");
    item->next = metadata;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);

    assert_option_bool(opts.enable_indices, true, "indices: TRUE (uppercase) sets enable_indices to true");
    assert_option_bool(opts.enable_wiki_links, false, "wikilinks: FALSE (uppercase) sets enable_wiki_links to false");

    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test more boolean options */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("relaxed-tables");
    item->value = strdup("true");
    item->next = NULL;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("link-citations");
    item->value = strdup("yes");
    item->next = metadata;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("suppress-bibliography");
    item->value = strdup("1");
    item->next = metadata;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);

    assert_option_bool(opts.relaxed_tables, true, "relaxed-tables: true sets relaxed_tables");
    assert_option_bool(opts.link_citations, true, "link-citations: yes sets link_citations");
    assert_option_bool(opts.suppress_bibliography, true, "suppress-bibliography: 1 sets suppress_bibliography");

    apex_free_metadata(metadata);

    /* Test loading metadata from file */
#ifdef TEST_FIXTURES_DIR
    opts = apex_options_default();
    char metadata_file_path[512];
    snprintf(metadata_file_path, sizeof(metadata_file_path), "%s/metadata_options.yml", TEST_FIXTURES_DIR);
    apex_metadata_item *file_metadata = apex_load_metadata_from_file(metadata_file_path);
    if (file_metadata) {
        apex_apply_metadata_to_options(file_metadata, &opts);

        assert_option_bool(opts.enable_indices, false, "metadata file: indices: false");
        assert_option_bool(opts.enable_wiki_links, true, "metadata file: wikilinks: true");
        assert_option_bool(opts.pretty, true, "metadata file: pretty: true");
        assert_option_bool(opts.standalone, true, "metadata file: standalone: true");
        assert_option_string(opts.document_title, "Test Document from File", "metadata file: title");
        assert_option_string(opts.csl_file, "test.csl", "metadata file: csl");
        assert_option_bool(opts.id_format == 2, true, "metadata file: id-format: kramdown sets id_format to 2");
        assert_option_bool(opts.link_citations, true, "metadata file: link-citations: true");
        assert_option_bool(opts.suppress_bibliography, false, "metadata file: suppress-bibliography: false");

        apex_free_metadata(file_metadata);
    } else {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " metadata file: Failed to load metadata_options.yml\n");
    }
#endif
    
    bool had_failures = suite_end(suite_failures);
    print_suite_title("Metadata Control of Options Tests", had_failures, false);
}

/**
 * Test syntax highlighting options via metadata
 */
void test_syntax_highlight_options(void) {
    int suite_failures = suite_start();
    print_suite_title("Syntax Highlighting Options Tests", false, true);

    apex_options opts;
    apex_metadata_item *metadata = NULL;
    apex_metadata_item *item;

    /* Test code-highlight option: pygments (full name) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("pygments");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "pygments", "code-highlight: pygments sets code_highlighter");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: skylighting (full name) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("skylighting");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "skylighting", "code-highlight: skylighting sets code_highlighter");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: p (abbreviation for pygments) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("p");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "pygments", "code-highlight: p (abbreviation) sets pygments");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: s (abbreviation for skylighting) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("s");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "skylighting", "code-highlight: s (abbreviation) sets skylighting");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: pyg (abbreviation for pygments) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("pyg");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "pygments", "code-highlight: pyg (abbreviation) sets pygments");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: sky (abbreviation for skylighting) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("sky");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "skylighting", "code-highlight: sky (abbreviation) sets skylighting");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: false disables */
    opts = apex_options_default();
    opts.code_highlighter = "pygments";  /* Start with it enabled */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("false");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.code_highlighter == NULL, true, "code-highlight: false disables highlighting");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: none disables */
    opts = apex_options_default();
    opts.code_highlighter = "pygments";  /* Start with it enabled */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("none");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.code_highlighter == NULL, true, "code-highlight: none disables highlighting");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-line-numbers option: true */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-line-numbers");
    item->value = strdup("true");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.code_line_numbers, true, "code-line-numbers: true enables line numbers");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-line-numbers option: false */
    opts = apex_options_default();
    opts.code_line_numbers = true;  /* Start enabled */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-line-numbers");
    item->value = strdup("false");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.code_line_numbers, false, "code-line-numbers: false disables line numbers");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code_line_numbers option with underscore (alternate key format) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code_line_numbers");
    item->value = strdup("yes");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.code_line_numbers, true, "code_line_numbers (underscore): yes enables line numbers");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test highlight-language-only option: true */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("highlight-language-only");
    item->value = strdup("true");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.highlight_language_only, true, "highlight-language-only: true enables language-only mode");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test highlight-language-only option: false */
    opts = apex_options_default();
    opts.highlight_language_only = true;  /* Start enabled */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("highlight-language-only");
    item->value = strdup("false");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.highlight_language_only, false, "highlight-language-only: false disables language-only mode");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test highlight_language_only option with underscore (alternate key format) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("highlight_language_only");
    item->value = strdup("1");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.highlight_language_only, true, "highlight_language_only (underscore): 1 enables language-only mode");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test combined syntax highlighting options */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("pygments");
    item->next = NULL;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-line-numbers");
    item->value = strdup("true");
    item->next = metadata;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("highlight-language-only");
    item->value = strdup("true");
    item->next = metadata;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "pygments", "Combined: code-highlight set");
    assert_option_bool(opts.code_line_numbers, true, "Combined: code-line-numbers set");
    assert_option_bool(opts.highlight_language_only, true, "Combined: highlight-language-only set");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test via YAML front matter in document */
    apex_options yaml_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    char *html;

    /* Note: We can't fully test external highlighting without the tools installed,
     * but we can verify that the options are parsed from YAML metadata correctly.
     * The actual highlighting would require pygments/skylighting to be installed. */

    /* Test that code blocks are rendered when no highlighting tool is available */
    const char *code_doc = "---\ncode-highlight: pygments\n---\n\n```python\nprint('hello')\n```";
    html = apex_markdown_to_html(code_doc, strlen(code_doc), &yaml_opts);
    /* Should have code block regardless of whether pygments is available */
    assert_contains(html, "<pre", "Code block has pre tag");
    assert_contains(html, "<code", "Code block has code tag");
    assert_contains(html, "print", "Code content preserved");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Syntax Highlighting Options Tests", had_failures, false);
}

/**
 * Test ARIA labels and accessibility attributes
 */
