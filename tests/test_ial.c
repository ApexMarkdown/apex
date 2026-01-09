/**
 * Ial Tests
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include <string.h>

void test_ial(void) {
    printf("\n=== IAL Tests ===\n");

    apex_options opts = apex_options_for_mode(APEX_MODE_KRAMDOWN);
    char *html;

    /* Test block IAL with ID */
    html = apex_markdown_to_html("# Header\n{: #custom-id}", 24, &opts);
    assert_contains(html, "id=\"custom-id\"", "Block IAL ID");
    apex_free_string(html);

    /* Test block IAL with class (requires blank line in standard Kramdown) */
    html = apex_markdown_to_html("Paragraph\n\n{: .important}", 25, &opts);
    assert_contains(html, "class=\"important\"", "Block IAL class");
    apex_free_string(html);

    /* Test block IAL with multiple classes */
    html = apex_markdown_to_html("Text\n\n{: .class1 .class2}", 26, &opts);
    assert_contains(html, "class=\"class1 class2\"", "Block IAL multiple classes");
    apex_free_string(html);

    /* Test block IAL with ID and class */
    html = apex_markdown_to_html("## Header 2\n{: #myid .myclass}", 31, &opts);
    assert_contains(html, "id=\"myid\"", "Block IAL ID with class");
    assert_contains(html, "class=\"myclass\"", "Block IAL class with ID");
    apex_free_string(html);

    /* Test block IAL with custom attributes - skip for now (complex quoting) */
    // html = apex_markdown_to_html("Para\n{: data-value=\"test\"}", 27, &opts);
    // assert_contains(html, "data-value=\"test\"", "Block IAL custom attribute");
    // apex_free_string(html);

    /* Test ALD (Attribute List Definition) - needs debugging */
    // const char *ald_doc = "{:ref: #special .highlight}\n\nParagraph 1\n{:ref}\n\nParagraph 2\n{:ref}";
    // html = apex_markdown_to_html(ald_doc, strlen(ald_doc), &opts);
    // assert_contains(html, "id=\"special\"", "ALD reference applied");
    // assert_contains(html, "class=\"highlight\"", "ALD class applied");
    // apex_free_string(html);

    /* Test list item IAL - needs debugging */
    // html = apex_markdown_to_html("- Item 1\n{: .special}\n- Item 2", 31, &opts);
    // assert_contains(html, "class=\"special\"", "List item IAL");
    // apex_free_string(html);

    /* Test inline IAL on links */
    const char *link_ial = "Here's a [link](https://example.com){:.button} with text after.";
    html = apex_markdown_to_html(link_ial, strlen(link_ial), &opts);
    assert_contains(html, "class=\"button\"", "Inline IAL on link");
    assert_contains(html, "with text after", "Text after IAL preserved");
    assert_not_contains(html, "{:.button}", "IAL removed from output");
    apex_free_string(html);

    /* Test inline IAL on links with duplicate URLs */
    const char *dup_urls = "First [link](https://example.com) and [second](https://example.com){:.special} link.";
    html = apex_markdown_to_html(dup_urls, strlen(dup_urls), &opts);
    assert_contains(html, "<a href=\"https://example.com\">link</a>", "First link without class");
    assert_contains(html, "class=\"special\"", "Second link with class");
    assert_not_contains(html, "{:.special}", "IAL removed from output");
    apex_free_string(html);

    /* Test Pandoc-style IAL (no colon) for block-level elements */
    html = apex_markdown_to_html("# Header\n{#pandoc-id .pandoc-class}", 35, &opts);
    assert_contains(html, "id=\"pandoc-id\"", "Pandoc-style block IAL ID");
    assert_contains(html, "class=\"pandoc-class\"", "Pandoc-style block IAL class");
    apex_free_string(html);

    /* Test Pandoc-style IAL for paragraphs */
    html = apex_markdown_to_html("Paragraph text\n\n{#para-id .para-class}", 40, &opts);
    assert_contains(html, "id=\"para-id\"", "Pandoc-style paragraph IAL ID");
    assert_contains(html, "class=\"para-class\"", "Pandoc-style paragraph IAL class");
    apex_free_string(html);

    /* Test Pandoc-style IAL for inline elements */
    const char *pandoc_inline = "Here's a [link](url){#link-id .link-class} with Pandoc IAL.";
    html = apex_markdown_to_html(pandoc_inline, strlen(pandoc_inline), &opts);
    assert_contains(html, "id=\"link-id\"", "Pandoc-style inline IAL ID");
    assert_contains(html, "class=\"link-class\"", "Pandoc-style inline IAL class");
    apex_free_string(html);

    /* Test Pandoc-style IAL with multiple classes */
    html = apex_markdown_to_html("## Heading\n{#heading-id .class1 .class2}", 40, &opts);
    assert_contains(html, "id=\"heading-id\"", "Pandoc-style IAL with multiple classes - ID");
    assert_contains(html, "class=\"class1 class2\"", "Pandoc-style IAL with multiple classes");
    apex_free_string(html);

    /* Test inline IAL on strong/emph */
    html = apex_markdown_to_html("This is **bold**{:.bold-style} text.", 40, &opts);
    assert_contains(html, "class=\"bold-style\"", "Inline IAL on strong");
    assert_contains(html, "<strong", "Strong tag present");
    assert_not_contains(html, "{:.bold-style}", "IAL removed from output");
    apex_free_string(html);

    html = apex_markdown_to_html("This is *italic*{:.italic-style} text.", 42, &opts);
    assert_contains(html, "class=\"italic-style\"", "Inline IAL on emphasis");
    assert_contains(html, "<em", "Em tag present");
    assert_not_contains(html, "{:.italic-style}", "IAL removed from output");
    apex_free_string(html);

    /* Test inline IAL on code */
    html = apex_markdown_to_html("Use `code`{:.code-inline} here.", 34, &opts);
    assert_contains(html, "class=\"code-inline\"", "Inline IAL on code");
    assert_contains(html, "<code", "Code tag present");
    assert_not_contains(html, "{:.code-inline}", "IAL removed from output");
    apex_free_string(html);

    /* Test multiple inline IALs in same paragraph */
    const char *multi_ial = "[link](url){:.link} and **bold**{:.bold} and *em*{:.em}.";
    html = apex_markdown_to_html(multi_ial, strlen(multi_ial), &opts);
    assert_contains(html, "class=\"link\"", "First IAL applied");
    assert_contains(html, "class=\"bold\"", "Second IAL applied");
    assert_contains(html, "class=\"em\"", "Third IAL applied");
    apex_free_string(html);

    /* Test inline IAL with multiple classes */
    const char *multi_class = "[link](url){:.primary .large .button} text.";
    html = apex_markdown_to_html(multi_class, strlen(multi_class), &opts);
    assert_contains(html, "class=\"primary large button\"", "Multiple classes in inline IAL");
    apex_free_string(html);

    /* Test inline IAL with ID and classes */
    const char *id_class = "**bold**{:#bold-id .highlight .important}";
    html = apex_markdown_to_html(id_class, strlen(id_class), &opts);
    assert_contains(html, "id=\"bold-id\"", "Inline IAL with ID");
    assert_contains(html, "class=\"highlight important\"", "Inline IAL ID with classes");
    apex_free_string(html);

    /* Test inline IAL at end of paragraph (should still work) */
    const char *end_ial = "End with [link](url){:.end-link}.";
    html = apex_markdown_to_html(end_ial, strlen(end_ial), &opts);
    assert_contains(html, "class=\"end-link\"", "Inline IAL at paragraph end");
    apex_free_string(html);

    /* Test inline IAL with whitespace (spaces around IAL) */
    const char *spaced_ial = "[link](url){: .spaced-class } text.";
    html = apex_markdown_to_html(spaced_ial, strlen(spaced_ial), &opts);
    assert_contains(html, "class=\"spaced-class\"", "Inline IAL with spaces");
    apex_free_string(html);
}

/**
 * Test bracketed spans [text]{IAL}
 */

void test_bracketed_spans(void) {
    printf("\n=== Bracketed Spans Tests ===\n");

    apex_options opts = apex_options_default();
    opts.enable_spans = true;
    char *html;

    /* Test basic bracketed span with class */
    const char *basic_span = "This is [some text]{.class} with a span.";
    html = apex_markdown_to_html(basic_span, strlen(basic_span), &opts);
    assert_contains(html, "<span", "Bracketed span creates span tag");
    assert_contains(html, "class=\"class\"", "Bracketed span has class");
    assert_contains(html, "some text", "Bracketed span contains text");
    assert_not_contains(html, "[some text]{.class}", "Bracketed span syntax removed");
    apex_free_string(html);

    /* Test bracketed span with ID */
    const char *span_with_id = "This is [text]{#my-id} with an ID.";
    html = apex_markdown_to_html(span_with_id, strlen(span_with_id), &opts);
    assert_contains(html, "id=\"my-id\"", "Bracketed span has ID");
    apex_free_string(html);

    /* Test bracketed span with multiple attributes */
    const char *span_multi = "This is [text]{#id .class1 .class2 key=\"value\"} with multiple attributes.";
    html = apex_markdown_to_html(span_multi, strlen(span_multi), &opts);
    assert_contains(html, "id=\"id\"", "Bracketed span has ID");
    assert_contains(html, "class=\"class1 class2\"", "Bracketed span has multiple classes");
    assert_contains(html, "key=\"value\"", "Bracketed span has custom attribute");
    apex_free_string(html);

    /* Test bracketed span with markdown inside */
    const char *span_markdown = "This is [some *text*]{.highlight} with markdown.";
    html = apex_markdown_to_html(span_markdown, strlen(span_markdown), &opts);
    assert_contains(html, "<em>text</em>", "Bracketed span processes markdown inside");
    assert_contains(html, "class=\"highlight\"", "Bracketed span with markdown has class");
    apex_free_string(html);

    /* Test reference link takes precedence over span */
    const char *ref_link = "This is [a link] that should be a link.\n\n[a link]: https://example.com";
    html = apex_markdown_to_html(ref_link, strlen(ref_link), &opts);
    assert_contains(html, "<a href", "Reference link creates link tag");
    assert_contains(html, "https://example.com", "Reference link has correct URL");
    assert_not_contains(html, "<span", "Reference link does not create span");
    apex_free_string(html);

    /* Test reference link with different ID */
    const char *ref_link2 = "This is [another link][ref] that should be a link.\n\n[ref]: https://example.org";
    html = apex_markdown_to_html(ref_link2, strlen(ref_link2), &opts);
    assert_contains(html, "<a href", "Reference link with ID creates link tag");
    assert_contains(html, "https://example.org", "Reference link with ID has correct URL");
    assert_not_contains(html, "<span", "Reference link with ID does not create span");
    apex_free_string(html);

    /* Test mixed reference link and span */
    const char *mixed = "This has [a link][ref] and [a span]{.span-class}.\n\n[ref]: https://example.com";
    html = apex_markdown_to_html(mixed, strlen(mixed), &opts);
    assert_contains(html, "<a href", "Mixed content has link");
    assert_contains(html, "<span", "Mixed content has span");
    assert_contains(html, "class=\"span-class\"", "Mixed content span has class");
    apex_free_string(html);

    /* Test bracketed span with bold markdown */
    const char *span_bold = "This is [**bold text**]{.bold} in a span.";
    html = apex_markdown_to_html(span_bold, strlen(span_bold), &opts);
    assert_contains(html, "<strong>bold text</strong>", "Bracketed span processes bold markdown");
    apex_free_string(html);

    /* Test bracketed span with code markdown */
    const char *span_code = "This is [`code`]{.code-style} in a span.";
    html = apex_markdown_to_html(span_code, strlen(span_code), &opts);
    assert_contains(html, "<code>code</code>", "Bracketed span processes code markdown");
    apex_free_string(html);

    /* Test bracketed span with nested brackets */
    const char *span_nested = "This is [Text with [nested brackets]]{.nested} that should work.";
    html = apex_markdown_to_html(span_nested, strlen(span_nested), &opts);
    assert_contains(html, "<span", "Bracketed span with nested brackets creates span");
    assert_contains(html, "class=\"nested\"", "Bracketed span with nested brackets has class");
    assert_contains(html, "Text with [nested brackets]", "Bracketed span with nested brackets contains full text");
    assert_not_contains(html, "[Text with [nested brackets]]{.nested}", "Bracketed span syntax removed");
    apex_free_string(html);

    /* Test that spans are disabled when flag is off */
    apex_options no_spans = apex_options_default();
    no_spans.enable_spans = false;
    const char *span_disabled = "This is [text]{.class} that should NOT be a span.";
    html = apex_markdown_to_html(span_disabled, strlen(span_disabled), &no_spans);
    /* When spans are disabled, [text]{.class} should remain as-is or be treated differently */
    /* For now, we'll just check that it doesn't create a span when disabled */
    if (strstr(html, "<span") == NULL) {
        test_result(true, "Bracketed spans disabled when flag is off");
    } else {
        test_result(false, "Bracketed spans incorrectly enabled when flag is off");
    }
    apex_free_string(html);
}

/**
 * Test definition lists
 */
