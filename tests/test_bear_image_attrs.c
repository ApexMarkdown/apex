#include "test_helpers.h"
#include "apex/apex.h"
#include "extensions/bear_image_attrs.h"

#include <string.h>

static const char *bear_value(const apex_bear_image_attrs *attrs,
                              const char *key) {
    for (size_t i = 0; i < attrs->count; i++) {
        if (strcmp(attrs->items[i].key, key) == 0) {
            return attrs->items[i].value;
        }
    }
    return NULL;
}

static char *render_bear(
    const char *markdown, apex_mode_t mode, bool unsafe) {
    apex_options opts = apex_options_for_mode(mode);
    opts.unsafe = unsafe;
    return apex_markdown_to_html(markdown, strlen(markdown), &opts);
}

static void test_bear_inline_modes(void) {
    const char *md =
        "![](emperor-1.jpg)<!-- {\"width\":259} -->";
    const apex_mode_t modes[] = {
        APEX_MODE_COMMONMARK,
        APEX_MODE_GFM,
        APEX_MODE_MULTIMARKDOWN,
        APEX_MODE_UNIFIED
    };

    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        char *html = render_bear(md, modes[i], true);
        assert_contains(html, "width=\"259\"", "Bear width is applied");
        assert_contains(
            html,
            "<!-- {\"width\":259} -->",
            "Bear comment is preserved");
        apex_free_string(html);
    }

    char *safe = render_bear(md, APEX_MODE_COMMONMARK, false);
    assert_contains(safe, "width=\"259\"", "Safe mode applies Bear width");
    assert_contains(
        safe, "raw HTML omitted", "Safe mode keeps raw HTML policy");
    apex_free_string(safe);

    char *spaced = render_bear(
        "![](x.jpg) \t<!-- {\"width\":\"50%\",\"height\":\"20px\"} -->",
        APEX_MODE_UNIFIED,
        true);
    assert_contains(
        spaced, "height=\"20\"", "Pixel height uses an HTML attribute");
    assert_contains(
        spaced, "style=\"width: 50%\"", "Percent width uses style");
    apex_free_string(spaced);

    char *escaped = render_bear(
        "![](x.jpg)<!-- {\"title\":\"x\\\" onerror=\\\"boom\"} -->",
        APEX_MODE_UNIFIED,
        true);
    assert_contains(
        escaped,
        "title=\"x&quot; onerror=&quot;boom\"",
        "Bear values are HTML attribute escaped");
    assert_not_contains(
        escaped, " onerror=\"boom\"", "Bear values cannot inject attributes");
    apex_free_string(escaped);
}

void test_bear_image_attributes(void) {
    int failures = suite_start();
    print_suite_title("Bear Image Attribute Tests", false, true);

    const char *input = "<!-- {\"width\":259,\"title\":\"A & B\"} -->";
    const char *end = input + strlen(input);
    const char *comment_end = NULL;
    apex_bear_image_attrs attrs = {0};

    bool ok = apex_parse_bear_image_comment(
        input, end, &comment_end, &attrs);
    test_result(ok, "Bear parser accepts a flat JSON object");
    assert_option_string(
        bear_value(&attrs, "width"), "259", "Numeric width is normalized");
    assert_option_string(
        bear_value(&attrs, "title"), "A & B", "String value is decoded");
    test_result(comment_end == end, "Parser returns the comment end");
    apex_free_bear_image_attrs(&attrs);

    const char *unsafe =
        "<!-- {\"width\":259,\"onclick\":\"alert(1)\","
        "\"data-x\":\"bad\"} -->";
    end = unsafe + strlen(unsafe);
    attrs = (apex_bear_image_attrs){0};
    ok = apex_parse_bear_image_comment(
        unsafe, end, &comment_end, &attrs);
    test_result(ok, "Unsupported keys do not invalidate metadata");
    test_result(
        attrs.count == 1, "Only allowlisted attributes are returned");
    apex_free_bear_image_attrs(&attrs);

    const char *nested =
        "<!-- {\"width\":259,\"title\":{\"nested\":true}} -->";
    end = nested + strlen(nested);
    attrs = (apex_bear_image_attrs){0};
    ok = apex_parse_bear_image_comment(
        nested, end, &comment_end, &attrs);
    test_result(ok, "Unsupported nested values are skipped");
    test_result(attrs.count == 1, "Supported sibling value is retained");
    apex_free_bear_image_attrs(&attrs);

    const char *bad = "<!-- {\"width\":259,} -->";
    end = bad + strlen(bad);
    attrs = (apex_bear_image_attrs){0};
    test_result(
        !apex_parse_bear_image_comment(
            bad, end, &comment_end, &attrs),
        "Malformed JSON is rejected");

    /* Reusing a populated result for a failing parse must clean it. */
    const char *reused = "<!-- {\"width\":259,\"title\":\"A\"} -->";
    end = reused + strlen(reused);
    attrs = (apex_bear_image_attrs){0};
    ok = apex_parse_bear_image_comment(
        reused, end, &comment_end, &attrs);
    test_result(ok && attrs.count == 2, "Reused struct starts populated");

    const char *too_short = "<!-- x";
    end = too_short + strlen(too_short);
    ok = apex_parse_bear_image_comment(
        too_short, end, &comment_end, &attrs);
    test_result(!ok, "Too-short comment fails on a reused struct");
    test_result(
        attrs.count == 0, "Early failure leaves the reused struct empty");

    attrs = (apex_bear_image_attrs){0};
    ok = apex_parse_bear_image_comment(
        reused, end = reused + strlen(reused), &comment_end, &attrs);
    test_result(ok && attrs.count == 2, "Struct repopulated for reuse");

    const char *unterminated = "<!-- {\"width\":259}";
    end = unterminated + strlen(unterminated);
    ok = apex_parse_bear_image_comment(
        unterminated, end, &comment_end, &attrs);
    test_result(!ok, "Unterminated comment fails on a reused struct");
    test_result(
        attrs.count == 0,
        "Missing terminator leaves the reused struct empty");
    apex_free_bear_image_attrs(&attrs);

    test_bear_inline_modes();

    bool had_failures = suite_end(failures);
    print_suite_title("Bear Image Attribute Tests", had_failures, false);
}
