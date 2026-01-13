/**
 * Links Tests
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include <string.h>

void test_wiki_links(void) {
    int suite_failures = suite_start();
    print_suite_title("Wiki Links Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_wiki_links = true;
    char *html;

    /* Test basic wiki link */
    html = apex_markdown_to_html("[[Page]]", 8, &opts);
    assert_contains(html, "<a href=\"Page\">Page</a>", "Basic wiki link");
    apex_free_string(html);

    /* Test wiki link with display text */
    html = apex_markdown_to_html("[[Page|Display]]", 16, &opts);
    assert_contains(html, "<a href=\"Page\">Display</a>", "Wiki link with display");
    apex_free_string(html);

    /* Test wiki link with section */
    html = apex_markdown_to_html("[[Page#Section]]", 16, &opts);
    assert_contains(html, "#Section", "Wiki link with section");
    apex_free_string(html);

    /* Test space mode: dash (default) */
    opts.wikilink_space = 0;  /* dash */
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"Home-Page\">Home Page</a>", "Wiki link space mode: dash");
    apex_free_string(html);

    /* Test space mode: none */
    opts.wikilink_space = 1;  /* none */
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"HomePage\">Home Page</a>", "Wiki link space mode: none");
    apex_free_string(html);

    /* Test space mode: underscore */
    opts.wikilink_space = 2;  /* underscore */
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"Home_Page\">Home Page</a>", "Wiki link space mode: underscore");
    apex_free_string(html);

    /* Test space mode: space (URL-encoded as %20) */
    opts.wikilink_space = 3;  /* space */
    opts.wikilink_extension = NULL;  /* Reset extension */
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"Home%20Page\">Home Page</a>", "Wiki link space mode: space (URL-encoded)");
    apex_free_string(html);

    /* Test extension without leading dot */
    opts.wikilink_space = 0;  /* dash (default) */
    opts.wikilink_extension = "html";
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"Home-Page.html\">Home Page</a>", "Wiki link with extension (no leading dot)");
    apex_free_string(html);

    /* Test extension with leading dot */
    opts.wikilink_extension = ".html";
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"Home-Page.html\">Home Page</a>", "Wiki link with extension (with leading dot)");
    apex_free_string(html);

    /* Test extension with section */
    opts.wikilink_extension = "html";
    html = apex_markdown_to_html("[[Home Page#Section]]", 21, &opts);
    assert_contains(html, "<a href=\"Home-Page.html#Section\">Home Page</a>", "Wiki link with extension and section");
    apex_free_string(html);

    /* Test extension with display text */
    {
        apex_options test_opts = apex_options_default();
        test_opts.enable_wiki_links = true;
        test_opts.wikilink_space = 0;  /* dash */
        test_opts.wikilink_extension = "html";
        html = apex_markdown_to_html("[[Home Page|Main]]", 18, &test_opts);
        assert_contains(html, "<a href=\"Home-Page.html\">Main</a>", "Wiki link with extension and display text");
        apex_free_string(html);
    }

    /* Test space mode none with extension */
    opts.wikilink_space = 1;  /* none */
    opts.wikilink_extension = "md";
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"HomePage.md\">Home Page</a>", "Wiki link space mode none with extension");
    apex_free_string(html);

    /* Test space mode underscore with extension */
    opts.wikilink_space = 2;  /* underscore */
    opts.wikilink_extension = "html";
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"Home_Page.html\">Home Page</a>", "Wiki link space mode underscore with extension");
    apex_free_string(html);

    /* Test multiple spaces with dash mode */
    {
        apex_options test_opts = apex_options_default();
        test_opts.enable_wiki_links = true;
        test_opts.wikilink_space = 0;  /* dash */
        test_opts.wikilink_extension = NULL;
        html = apex_markdown_to_html("[[My Home Page]]", 16, &test_opts);
        assert_contains(html, "<a href=\"My-Home-Page\">My Home Page</a>", "Wiki link multiple spaces with dash");
        apex_free_string(html);
    }

    /* Test multiple spaces with none mode */
    {
        apex_options test_opts = apex_options_default();
        test_opts.enable_wiki_links = true;
        test_opts.wikilink_space = 1;  /* none */
        test_opts.wikilink_extension = NULL;
        html = apex_markdown_to_html("[[My Home Page]]", 16, &test_opts);
        assert_contains(html, "<a href=\"MyHomePage\">My Home Page</a>", "Wiki link multiple spaces with none");
        apex_free_string(html);
    }

    /* Reset options */
    opts.wikilink_extension = NULL;
    opts.wikilink_space = 0;  /* dash (default) */
    
    bool had_failures = suite_end(suite_failures);
    print_suite_title("Wiki Links Tests", had_failures, false);
}

/**
 * Test math support
 */

void test_image_embedding(void) {
    int suite_failures = suite_start();
    print_suite_title("Image Embedding Tests", false, true);

    apex_options opts = apex_options_default();
    char *html;

    /* Test local image embedding */
    opts.embed_images = true;
    opts.base_directory = TEST_FIXTURES_DIR;
    const char *local_image_md = "![Test Image](test_image.png)";
    html = apex_markdown_to_html(local_image_md, strlen(local_image_md), &opts);
    assert_contains(html, "<img", "Local image generates img tag");
    assert_contains(html, "data:image/png;base64,", "Local image embedded as base64 data URL");
    assert_not_contains(html, "test_image.png", "Local image path replaced with data URL");
    apex_free_string(html);

    /* Test that local images are not embedded when flag is off */
    opts.embed_images = false;
    html = apex_markdown_to_html(local_image_md, strlen(local_image_md), &opts);
    assert_contains(html, "<img", "Local image generates img tag");
    assert_contains(html, "test_image.png", "Local image path preserved when embedding disabled");
    assert_not_contains(html, "data:image/png;base64,", "Local image not embedded when flag is off");
    apex_free_string(html);

    /* Test that remote images are not embedded (only local images supported) */
    opts.embed_images = true;
    const char *remote_image_md = "![Remote Image](https://fastly.picsum.photos/id/973/300/300.jpg?hmac=KKNEjIQImwiXzi0Xly-dB7LhYl5SX5koiFRx0HiSUmA)";
    html = apex_markdown_to_html(remote_image_md, strlen(remote_image_md), &opts);
    assert_contains(html, "<img", "Remote image generates img tag");
    assert_contains(html, "fastly.picsum.photos", "Remote image URL preserved (only local images are embedded)");
    assert_not_contains(html, "data:image/", "Remote image not embedded");
    apex_free_string(html);

    /* Test that already-embedded data URLs are not processed again */
    opts.embed_images = true;
    const char *data_url_md = "![Already Embedded](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==)";
    html = apex_markdown_to_html(data_url_md, strlen(data_url_md), &opts);
    assert_contains(html, "data:image/png;base64,", "Data URL preserved");
    /* Should only appear once (not duplicated) */
    const char *first = strstr(html, "data:image/png;base64,");
    const char *second = first ? strstr(first + 1, "data:image/png;base64,") : NULL;
    if (first && !second) {
        test_result(true, "Data URL not processed again");
    } else {
        test_result(false, "Data URL was processed again");
    }
    apex_free_string(html);

    /* Test base_directory for relative path resolution */
    opts.embed_images = true;
    opts.base_directory = NULL;  /* No base directory */
    const char *relative_image_md = "![Relative Image](test_image.png)";
    html = apex_markdown_to_html(relative_image_md, strlen(relative_image_md), &opts);
    /* Without base_directory, relative path might not be found, so it won't be embedded */
    assert_contains(html, "test_image.png", "Relative image path preserved when base_directory not set");
    apex_free_string(html);

    /* Test with base_directory set */
    opts.base_directory = TEST_FIXTURES_DIR;
    html = apex_markdown_to_html(relative_image_md, strlen(relative_image_md), &opts);
    assert_contains(html, "data:image/png;base64,", "Relative image embedded when base_directory is set");
    assert_not_contains(html, "test_image.png", "Relative image path replaced with data URL when base_directory set");
    apex_free_string(html);

    /* Test that absolute paths work regardless of base_directory */
    opts.base_directory = "/nonexistent/path";
    char abs_path[512];
    snprintf(abs_path, sizeof(abs_path), "![Absolute Image](%s/test_image.png)", TEST_FIXTURES_DIR);
    html = apex_markdown_to_html(abs_path, strlen(abs_path), &opts);
    assert_contains(html, "data:image/png;base64,", "Absolute path image embedded regardless of base_directory");
    apex_free_string(html);
    
    bool had_failures = suite_end(suite_failures);
    print_suite_title("Image Embedding Tests", had_failures, false);
}

/**
 * Test image width/height attribute conversion
 */

void test_image_width_height_conversion(void) {
    int suite_failures = suite_start();
    print_suite_title("Image Width/Height Conversion Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    char *html;

    /* Test 1: Percentage width → style attribute */
    html = apex_markdown_to_html("![](img.jpg){ width=50% }", strlen("![](img.jpg){ width=50% }"), &opts);
    assert_contains(html, "style=\"width: 50%\"", "Percentage width converted to style");
    assert_not_contains(html, "width=\"50%\"", "Percentage not in width attribute");
    apex_free_string(html);

    /* Test 2: Pixel width → integer width attribute */
    html = apex_markdown_to_html("![](img.jpg){ width=300px }", strlen("![](img.jpg){ width=300px }"), &opts);
    assert_contains(html, "width=\"300\"", "Pixel width converted to integer");
    assert_not_contains(html, "width=\"300px\"", "px suffix removed");
    assert_not_contains(html, "style=\"width: 300px\"", "Pixel not in style");
    apex_free_string(html);

    /* Test 3: Bare integer width → width attribute */
    html = apex_markdown_to_html("![](img.jpg){ width=300 }", strlen("![](img.jpg){ width=300 }"), &opts);
    assert_contains(html, "width=\"300\"", "Bare integer width preserved");
    apex_free_string(html);

    /* Test 4: Percentage height → style attribute */
    html = apex_markdown_to_html("![](img.jpg){ height=75% }", strlen("![](img.jpg){ height=75% }"), &opts);
    assert_contains(html, "style=\"height: 75%\"", "Percentage height converted to style");
    apex_free_string(html);

    /* Test 5: Pixel height → integer height attribute */
    html = apex_markdown_to_html("![](img.jpg){ height=200px }", strlen("![](img.jpg){ height=200px }"), &opts);
    assert_contains(html, "height=\"200\"", "Pixel height converted to integer");
    assert_not_contains(html, "height=\"200px\"", "px suffix removed");
    apex_free_string(html);

    /* Test 6: Both width and height with percentages → style */
    html = apex_markdown_to_html("![](img.jpg){ width=50% height=75% }", strlen("![](img.jpg){ width=50% height=75% }"), &opts);
    assert_contains(html, "style=\"width: 50%; height: 75%\"", "Both percentages in style");
    apex_free_string(html);

    /* Test 7: Mixed - pixel width, percentage height */
    html = apex_markdown_to_html("![](img.jpg){ width=300px height=50% }", strlen("![](img.jpg){ width=300px height=50% }"), &opts);
    assert_contains(html, "width=\"300\"", "Pixel width as attribute");
    assert_contains(html, "style=\"height: 50%\"", "Percentage height in style");
    apex_free_string(html);

    /* Test 8: Mixed - percentage width, pixel height */
    html = apex_markdown_to_html("![](img.jpg){ width=50% height=200px }", strlen("![](img.jpg){ width=50% height=200px }"), &opts);
    assert_contains(html, "height=\"200\"", "Pixel height as attribute");
    assert_contains(html, "style=\"width: 50%\"", "Percentage width in style");
    apex_free_string(html);

    /* Test 9: Other units (em, rem) → style */
    html = apex_markdown_to_html("![](img.jpg){ width=5em height=10rem }", strlen("![](img.jpg){ width=5em height=10rem }"), &opts);
    assert_contains(html, "style=\"width: 5em; height: 10rem\"", "Other units in style");
    apex_free_string(html);

    /* Test 10: Decimal pixel value → style */
    html = apex_markdown_to_html("![](img.jpg){ width=100.5px }", strlen("![](img.jpg){ width=100.5px }"), &opts);
    assert_contains(html, "style=\"width: 100.5px\"", "Decimal pixel in style");
    assert_not_contains(html, "width=\"100.5\"", "Decimal pixel not as attribute");
    apex_free_string(html);

    /* Test 11: Viewport units → style */
    html = apex_markdown_to_html("![](img.jpg){ width=50vw height=30vh }", strlen("![](img.jpg){ width=50vw height=30vh }"), &opts);
    assert_contains(html, "style=\"width: 50vw; height: 30vh\"", "Viewport units in style");
    apex_free_string(html);

    /* Test 12: Inline image with IAL and percentage */
    html = apex_markdown_to_html("![alt](img.jpg){ width=75% }", strlen("![alt](img.jpg){ width=75% }"), &opts);
    assert_contains(html, "style=\"width: 75%\"", "Inline image percentage in style");
    apex_free_string(html);

    /* Test 13: Mixed with other attributes (ID, class) */
    html = apex_markdown_to_html("![test](img.jpg){#test .image width=250px height=80% }", strlen("![test](img.jpg){#test .image width=250px height=80% }"), &opts);
    assert_contains(html, "id=\"test\"", "ID preserved");
    assert_contains(html, "class=\"image\"", "Class preserved");
    assert_contains(html, "width=\"250\"", "Pixel width as attribute");
    assert_contains(html, "style=\"height: 80%\"", "Percentage height in style");
    apex_free_string(html);

    /* Test 14: Zero pixel value */
    html = apex_markdown_to_html("![](img.jpg){ width=0px }", strlen("![](img.jpg){ width=0px }"), &opts);
    assert_contains(html, "width=\"0\"", "Zero pixel converted to integer");
    apex_free_string(html);
    
    bool had_failures = suite_end(suite_failures);
    print_suite_title("Image Width/Height Conversion Tests", had_failures, false);
}

/**
 * Test indices (mmark and TextIndex syntax)
 */
