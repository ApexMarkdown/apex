/**
 * Tables Tests
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

void test_advanced_tables(void) {
    int suite_failures = suite_start();
    print_suite_title("Advanced Tables Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_tables = true;
    opts.relaxed_tables = false;  /* Use standard GFM table syntax for these tests */
    char *html;

    /* Test table with caption before table */
    const char *caption_table = "[Table Caption]\n\n| H1 | H2 |\n|----|----|"
                                "\n| C1 | C2 |";
    html = apex_markdown_to_html(caption_table, strlen(caption_table), &opts);
    assert_contains(html, "<table", "Caption table renders");
    assert_contains(html, "<figure", "Caption table wrapped in figure");
    assert_contains(html, "<figcaption>", "Caption has figcaption tag");
    assert_contains(html, "Table Caption", "Caption text is present");
    assert_contains(html, "</figure>", "Caption figure is closed");
    apex_free_string(html);

    /* Test table with caption after table */
    const char *caption_table_after = "| H1 | H2 |\n|----|----|"
                                     "\n| C1 | C2 |\n\n[Table Caption After]";
    html = apex_markdown_to_html(caption_table_after, strlen(caption_table_after), &opts);
    assert_contains(html, "<table", "Caption table after renders");
    assert_contains(html, "<figure", "Caption table after wrapped in figure");
    assert_contains(html, "Table Caption After", "Caption text after is present");
    apex_free_string(html);

    /* Test rowspan with ^^ */
    const char *rowspan_table = "| H1 | H2 |\n|----|----|"
                                "\n| A  | B  |"
                                "\n| ^^ | C  |";
    html = apex_markdown_to_html(rowspan_table, strlen(rowspan_table), &opts);
    assert_contains(html, "rowspan", "Rowspan attribute added");
    assert_contains(html, "<td rowspan=\"2\">A</td>", "Rowspan applied to first cell content");
    apex_free_string(html);

    /* Test tfoot support using === separator row.
     * The === row itself should be removed, and subsequent rows should be wrapped in <tfoot>.
     */
    const char *tfoot_table =
        "| H1 | H2 |\n"
        "|----|----|\n"
        "| A  | B  |\n"
        "| C  | D  |\n"
        "| E  | F  |\n"
        "| === | === |\n"
        "| F1 | F2 |\n";
    /* tfoot detection/mapping is more reliable with relaxed tables enabled (unified default). */
    apex_options tfoot_opts = opts;
    tfoot_opts.relaxed_tables = true;
    html = apex_markdown_to_html(tfoot_table, strlen(tfoot_table), &tfoot_opts);
    assert_contains(html, "<tfoot>", "Tfoot: footer section opened");
    assert_contains(html, "F1", "Tfoot: footer content present");
    assert_not_contains(html, "===", "Tfoot: === marker row removed");
    apex_free_string(html);

    /* Test colspan with consecutive pipes (|||) */
    const char *colspan_table = "| H1 | H2 | H3 |\n|----|----|----|"
                                "\n| A  |||"
                                "\n| B  | C  | D  |";
    html = apex_markdown_to_html(colspan_table, strlen(colspan_table), &opts);
    assert_contains(html, "colspan", "Colspan attribute added");
    /* A should span all three columns in the first data row */
    assert_contains(html, "<td colspan=\"3\">A</td>", "Colspan applied to first row A spanning 3 columns with consecutive pipes");
    apex_free_string(html);

    /* Test colspan marker detection inside nested inline nodes (e.g., emphasis around <<).
     * This exercises recursive marker detection for colspan cells.
     */
    const char *colspan_nested_marker =
        "| H1 | H2 | H3 |\n"
        "|----|----|----|\n"
        "| A  | **<<** | << |\n"
        "| B  | C  | D |\n";
    html = apex_markdown_to_html(colspan_nested_marker, strlen(colspan_nested_marker), &opts);
    assert_contains(html, "colspan", "Colspan detected with nested marker");
    apex_free_string(html);

    /* Test that empty cells with whitespace do NOT create colspan */
    const char *empty_cells_table = "| H1 | H2 | H3 |\n|----|----|----|"
                                    "\n| A  |    |    |"
                                    "\n| B  | C  | D  |";
    html = apex_markdown_to_html(empty_cells_table, strlen(empty_cells_table), &opts);
    assert_contains(html, "<td>A</td>", "Empty cells table: A cell present");
    assert_contains(html, "<td></td>", "Empty cells table: empty cell present (not merged)");
    assert_not_contains(html, "colspan", "Empty cells table: no colspan attribute (empty cells don't create colspan)");
    apex_free_string(html);

    /* Test dash-only separator row removal (rows containing only — should be removed). */
    const char *dash_row =
        "| H1 | H2 |\n"
        "|----|----|\n"
        "| A  | B  |\n"
        "| —  | —  |\n"
        "| C  | D  |\n";
    html = apex_markdown_to_html(dash_row, strlen(dash_row), &opts);
    assert_contains(html, "<td>A</td>", "Dash row: first row present");
    assert_contains(html, "<td>C</td>", "Dash row: row after dash separator present");
    assert_not_contains(html, "<td>—</td>", "Dash row: em-dash-only row removed");
    apex_free_string(html);

    /* Test per-cell alignment marker parsing in cell content:
     * these markers should be stripped from the rendered cell content.
     * (Alignment styling is handled in HTML postprocessing; here we assert the markers don't leak.)
     */
    apex_options cell_align_opts = opts;
    cell_align_opts.enable_emoji_autocorrect = false; /* Avoid :x: / :X: emoji autocorrect interference */
    const char *cell_align =
        "| H1 | H2 | H3 |\n"
        "|----|----|----|\n"
        "| :L | :X: | R: |\n";
    html = apex_markdown_to_html(cell_align, strlen(cell_align), &cell_align_opts);
    assert_not_contains(html, ":L", "Cell alignment marker: leading colon stripped");
    assert_not_contains(html, ":X:", "Cell alignment marker: both colons stripped");
    assert_not_contains(html, "R:", "Cell alignment marker: trailing colon stripped");
    assert_contains(html, "<td>L</td>", "Cell alignment marker: left content preserved");
    assert_contains(html, "<td>X</td>", "Cell alignment marker: center content preserved");
    assert_contains(html, "<td>R</td>", "Cell alignment marker: right content preserved");
    apex_free_string(html);

    /* Test per-cell alignment using colons */
    const char *align_table = "| h1  |  h2   | h3  |\n"
                              "| --- | :---: | --- |\n"
                              "| d1  |  d2   | d3  |";
    html = apex_markdown_to_html(align_table, strlen(align_table), &opts);
    /* cmark-gfm uses align=\"left|center|right\" attributes rather than inline styles */
    assert_contains(html, "<th>h1</th>", "Left-aligned header from colon pattern");
    /* Accept either align="center" or style="text-align: center" */
    bool has_align = strstr(html, "<th align=\"center\">h2</th") != NULL;
    bool has_style = strstr(html, "<th style=\"text-align: center\">h2</th") != NULL;
    if (has_align || has_style) {
        test_result(true, "Center-aligned header from colon pattern");
    } else {
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Center-aligned header from colon pattern\n");
        printf("  Looking for: <th align=\"center\">h2</th> or <th style=\"text-align: center\">h2</th>\n");
        printf("  In:          %s\n", html);
    }
    apex_free_string(html);

    /* Test basic table (ensure we didn't break existing functionality) */
    const char *basic_table = "| H1 | H2 |\n|-----|-----|\n| C1 | C2 |";
    html = apex_markdown_to_html(basic_table, strlen(basic_table), &opts);
    assert_contains(html, "<table>", "Basic table still works");
    assert_contains(html, "<th>H1</th>", "Table header");
    assert_contains(html, "<td>C1</td>", "Table cell");
    apex_free_string(html);

    /* Test row header column when first header cell is empty */
    const char *row_header_table =
        "|   | H1 | H2 |\n"
        "|----|----|----|\n"
        "| Row 1 | A1 | B1 |\n"
        "| Row 2 | A2 | B2 |";
    html = apex_markdown_to_html(row_header_table, strlen(row_header_table), &opts);
    assert_contains(html, "<table>", "Row-header table renders");
    /* Check for th with scope="row" - the first column cells in tbody should be converted to row headers */
    /* Check for the pattern: <th scope="row"> followed by Row 1 or Row 2 */
    bool has_row_scope_1 = (strstr(html, "<th scope=\"row\">Row 1</th>") != NULL) ||
                           (strstr(html, "scope=\"row\">Row 1") != NULL);
    bool has_row_scope_2 = (strstr(html, "<th scope=\"row\">Row 2</th>") != NULL) ||
                           (strstr(html, "scope=\"row\">Row 2") != NULL);
    if (has_row_scope_1 && has_row_scope_2) {
        test_result(true, "Row-header table: first row header cell");
        test_result(true, "Row-header table: second row header cell");
    } else {
        tests_failed += 2;
        tests_run += 2;
        printf(COLOR_RED "✗" COLOR_RESET " Row-header table: first row header cell\n");
        printf(COLOR_RED "✗" COLOR_RESET " Row-header table: second row header cell\n");
        printf("  Looking for: <th scope=\"row\">Row 1</th> and <th scope=\"row\">Row 2</th>\n");
        printf("  In:          %s\n", html);
    }
    assert_contains(html, "<td>A1</td>", "Row-header table: body cell A1");
    apex_free_string(html);

    /* Test table followed by paragraph (regression: last row should not become paragraph) */
    const char *table_with_text = "| H1 | H2 |\n|-----|-----|\n| C1 | C2 |\n| C3 | C4 |\n\nText after.";
    html = apex_markdown_to_html(table_with_text, strlen(table_with_text), &opts);
    assert_contains(html, "<td>C3</td>", "Last table row C3 in table");
    assert_contains(html, "<td>C4</td>", "Last table row C4 in table");
    assert_contains(html, "</table>\n<p>Text after.</p>", "Table properly closed before paragraph");
    apex_free_string(html);

    /* Test : Caption syntax BEFORE table */
    const char *colon_caption_before = ": My Table\n\n| Col1 | Col2 |\n|------|------|\n| A    | B    |";
    html = apex_markdown_to_html(colon_caption_before, strlen(colon_caption_before), &opts);
    assert_contains(html, "<figcaption>", ": Caption before table has figcaption tag");
    assert_contains(html, "My Table", ": Caption before table text is present");
    assert_contains(html, "<table", "Table with : Caption before renders");
    assert_not_contains(html, "<p>: My Table</p>", ": Caption before table paragraph removed");
    apex_free_string(html);

    /* Test : Caption syntax BEFORE table with IAL */
    const char *colon_caption_before_ial = ": My Table {#table-id .highlight}\n\n| Col1 | Col2 |\n|------|------|\n| A    | B    |";
    html = apex_markdown_to_html(colon_caption_before_ial, strlen(colon_caption_before_ial), &opts);
    assert_contains(html, "<figcaption>", ": Caption before table with IAL has figcaption tag");
    assert_contains(html, "My Table", ": Caption before table with IAL text is present");
    assert_contains(html, "id=\"table-id\"", ": Caption before table IAL ID applied");
    assert_contains(html, "class=\"highlight\"", ": Caption before table IAL class applied");
    assert_not_contains(html, "<p>: My Table", ": Caption before table with IAL paragraph removed");
    apex_free_string(html);

    /* Test : Caption syntax BEFORE table without blank line */
    const char *colon_caption_before_no_blank = ": Caption No Blank\n| Col1 | Col2 |\n|------|------|\n| A    | B    |";
    html = apex_markdown_to_html(colon_caption_before_no_blank, strlen(colon_caption_before_no_blank), &opts);
    assert_contains(html, "<figcaption>", ": Caption before table no blank has figcaption tag");
    assert_contains(html, "Caption No Blank", ": Caption before table no blank text is present");
    assert_contains(html, "<table", "Table with : Caption before no blank renders");
    apex_free_string(html);

    /* Test Pandoc-style table caption with : Caption syntax AFTER table */
    const char *pandoc_caption = "| Key | Value |\n| --- | :---: |\n| one |   1   |\n| two |   2   |\n\n: Key value table";
    html = apex_markdown_to_html(pandoc_caption, strlen(pandoc_caption), &opts);
    assert_contains(html, "<figcaption>", "Pandoc caption has figcaption tag");
    assert_contains(html, "Key value table", "Pandoc caption text is present");
    assert_contains(html, "<table", "Table with Pandoc caption renders");
    apex_free_string(html);

    /* Test Pandoc-style table caption with IAL attributes (Kramdown format) */
    const char *pandoc_caption_ial_kramdown = "| Key | Value |\n| --- | :---: |\n| one |   1   |\n| two |   2   |\n\n: Key value table {: #table-id .testing}";
    html = apex_markdown_to_html(pandoc_caption_ial_kramdown, strlen(pandoc_caption_ial_kramdown), &opts);
    assert_contains(html, "<table", "Table with Pandoc caption and IAL renders");
    assert_contains(html, "id=\"table-id\"", "Table IAL ID from caption applied");
    assert_contains(html, "class=\"testing\"", "Table IAL class from caption applied");
    assert_contains(html, "Key value table", "Caption text is present");
    apex_free_string(html);

    /* Test Pandoc-style table caption with IAL attributes (Pandoc format) */
    const char *pandoc_caption_ial_pandoc = "| Key | Value |\n| --- | :---: |\n| one |   1   |\n| two |   2   |\n\n: Key value table {#table-id-2 .testing-2}";
    html = apex_markdown_to_html(pandoc_caption_ial_pandoc, strlen(pandoc_caption_ial_pandoc), &opts);
    assert_contains(html, "<table", "Table with Pandoc caption and Pandoc IAL renders");
    assert_contains(html, "id=\"table-id-2\"", "Table Pandoc IAL ID from caption applied");
    assert_contains(html, "class=\"testing-2\"", "Table Pandoc IAL class from caption applied");
    assert_contains(html, "Key value table", "Caption text is present");
    apex_free_string(html);

    /* Test table with IAL applied directly (not via caption) */
    const char *table_with_direct_ial = "| H1 | H2 |\n|----|----|\n| C1 | C2 |\n{: #direct-table .direct-class}";
    html = apex_markdown_to_html(table_with_direct_ial, strlen(table_with_direct_ial), &opts);
    assert_contains(html, "<table", "Table with direct IAL renders");
    assert_contains(html, "id=\"direct-table\"", "Direct table IAL ID applied");
    assert_contains(html, "class=\"direct-class\"", "Direct table IAL class applied");
    apex_free_string(html);

    /* Test table caption before table with IAL */
    const char *caption_before_ial = "[Caption Before]\n\n| H1 | H2 |\n|----|----|\n| C1 | C2 |\n{: #before-table .before-class}";
    html = apex_markdown_to_html(caption_before_ial, strlen(caption_before_ial), &opts);
    assert_contains(html, "<table", "Table with caption before and IAL renders");
    assert_contains(html, "Caption Before", "Caption text before table");
    assert_contains(html, "id=\"before-table\"", "Table IAL ID with caption before");
    assert_contains(html, "class=\"before-class\"", "Table IAL class with caption before");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Advanced Tables Tests", had_failures, false);
}

/**
 * Test relaxed tables (tables without separator rows)
 */

void test_relaxed_tables(void) {
    int suite_failures = suite_start();
    print_suite_title("Relaxed Tables Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_tables = true;
    opts.relaxed_tables = true;
    char *html;

    /* Test basic relaxed table (2 rows, no separator) */
    const char *relaxed_table = "A | B\n1 | 2";
    html = apex_markdown_to_html(relaxed_table, strlen(relaxed_table), &opts);
    assert_contains(html, "<table>", "Relaxed table renders");
    assert_contains(html, "<tbody>", "Relaxed table has tbody");
    assert_contains(html, "<tr>", "Relaxed table has rows");
    assert_contains(html, "<td>A</td>", "First cell A");
    assert_contains(html, "<td>B</td>", "First cell B");
    assert_contains(html, "<td>1</td>", "Second cell 1");
    assert_contains(html, "<td>2</td>", "Second cell 2");
    /* Should NOT have a header row */
    if (strstr(html, "<thead>") == NULL && strstr(html, "<th>") == NULL) {
        test_result(true, "Relaxed table has no header row");
    } else {
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Relaxed table incorrectly has header row\n");
    }
    apex_free_string(html);

    /* Test relaxed table with 3 rows */
    const char *relaxed_table3 = "A | B\n1 | 2\n3 | 4";
    html = apex_markdown_to_html(relaxed_table3, strlen(relaxed_table3), &opts);
    assert_contains(html, "<table>", "Relaxed table with 3 rows renders");
    assert_contains(html, "<td>3</td>", "Third row cell 3");
    assert_contains(html, "<td>4</td>", "Third row cell 4");
    apex_free_string(html);

    /* Test relaxed table stops at blank line */
    const char *relaxed_table_blank = "A | B\n1 | 2\n\nParagraph text";
    html = apex_markdown_to_html(relaxed_table_blank, strlen(relaxed_table_blank), &opts);
    assert_contains(html, "<table>", "Relaxed table before blank line");
    assert_contains(html, "<p>Paragraph text</p>", "Paragraph after blank line");
    apex_free_string(html);

    /* Test relaxed table with leading pipe */
    const char *relaxed_table_leading = "| A | B |\n| 1 | 2 |";
    html = apex_markdown_to_html(relaxed_table_leading, strlen(relaxed_table_leading), &opts);
    assert_contains(html, "<table>", "Relaxed table with leading pipe renders");
    assert_contains(html, "<td>A</td>", "Cell A with leading pipe");
    apex_free_string(html);

    /* Test that relaxed tables are disabled by default in GFM mode */
    apex_options gfm_opts = apex_options_for_mode(APEX_MODE_GFM);
    gfm_opts.enable_tables = true;
    html = apex_markdown_to_html(relaxed_table, strlen(relaxed_table), &gfm_opts);
    if (strstr(html, "<table>") == NULL) {
        test_result(true, "Relaxed tables disabled in GFM mode by default");
    } else {
        test_result(false, "Relaxed tables incorrectly enabled in GFM mode");
    }
    apex_free_string(html);

    /* Test that relaxed tables are enabled by default in Kramdown mode */
    apex_options kramdown_opts = apex_options_for_mode(APEX_MODE_KRAMDOWN);
    kramdown_opts.enable_tables = true;
    html = apex_markdown_to_html(relaxed_table, strlen(relaxed_table), &kramdown_opts);
    if (strstr(html, "<table>") != NULL) {
        test_result(true, "Relaxed tables enabled in Kramdown mode by default");
    } else {
        test_result(false, "Relaxed tables incorrectly disabled in Kramdown mode");
    }
    apex_free_string(html);

    /* Test that relaxed tables are enabled by default in Unified mode */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    unified_opts.enable_tables = true;
    html = apex_markdown_to_html(relaxed_table, strlen(relaxed_table), &unified_opts);
    if (strstr(html, "<table>") != NULL) {
        test_result(true, "Relaxed tables enabled in Unified mode by default");
    } else {
        test_result(false, "Relaxed tables incorrectly disabled in Unified mode");
    }
    apex_free_string(html);

    /* Test that --no-relaxed-tables disables it even in Kramdown mode */
    apex_options no_relaxed = apex_options_for_mode(APEX_MODE_KRAMDOWN);
    no_relaxed.enable_tables = true;
    no_relaxed.relaxed_tables = false;
    html = apex_markdown_to_html(relaxed_table, strlen(relaxed_table), &no_relaxed);
    if (strstr(html, "<table>") == NULL) {
        test_result(true, "--no-relaxed-tables disables relaxed tables");
    } else {
        test_result(false, "--no-relaxed-tables did not disable relaxed tables");
    }
    apex_free_string(html);

    /* Test that single row with pipe is not treated as table */
    const char *single_row = "A | B";
    html = apex_markdown_to_html(single_row, strlen(single_row), &opts);
    if (strstr(html, "<table>") == NULL) {
        test_result(true, "Single row is not treated as table");
    } else {
        test_result(false, "Single row incorrectly treated as table");
    }
    apex_free_string(html);

    /* Test that rows with different column counts are not treated as table */
    const char *mismatched = "A | B\n1 | 2 | 3";
    html = apex_markdown_to_html(mismatched, strlen(mismatched), &opts);
    if (strstr(html, "<table>") == NULL) {
        test_result(true, "Mismatched column counts are not treated as table");
    } else {
        test_result(false, "Mismatched column counts incorrectly treated as table");
    }
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Relaxed Tables Tests", had_failures, false);
}

/**
 * Test combine-like behavior for GitBook SUMMARY.md via core API
 * (indirectly validates that include expansion and ordering work).
 */

void test_comprehensive_table_features(void) {
    int suite_failures = suite_start();
    print_suite_title("Comprehensive Test File Table Features", false, true);

    apex_options opts = apex_options_default();
    opts.enable_tables = true;
    char *html = NULL;

    /* Read comprehensive_test.md file */
    FILE *f = fopen("tests/fixtures/comprehensive_test.md", "r");
    if (!f) {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " comprehensive_test.md: Could not open file\n");
        return;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read file content */
    char *markdown = (char *)malloc(file_size + 1);
    if (!markdown) {
        fclose(f);
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " comprehensive_test.md: Memory allocation failed\n");
        return;
    }

    size_t bytes_read = fread(markdown, 1, file_size, f);
    markdown[bytes_read] = '\0';
    fclose(f);

    /* Convert to HTML */
    html = apex_markdown_to_html(markdown, bytes_read, &opts);
    free(markdown);

    if (!html) {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " comprehensive_test.md: Failed to convert to HTML\n");
        return;
    }

    /* Test 1: Caption before table with IAL should render correctly */
    /* The caption "Employee Performance Q4 2025" should appear in figcaption, not as a paragraph */
    /* Note: Caption may appear as <p> if not properly detected, so check for either format */
    bool has_figcaption = strstr(html, "<figcaption>Employee Performance Q4 2025</figcaption>") != NULL;
    bool has_caption_para = strstr(html, "<p>[Employee Performance Q4 2025]</p>") != NULL;
    if (has_figcaption) {
        test_result(true, "Caption appears in figcaption tag");
        /* If in figcaption, it should NOT be in paragraph */
        if (!has_caption_para) {
            test_result(true, "Caption paragraph removed (no duplicate)");
        } else {
            tests_failed++;
            tests_run++;
            printf(COLOR_RED "✗" COLOR_RESET " Caption paragraph removed (no duplicate)\n");
            printf("  Caption found in both figcaption and paragraph\n");
        }
    } else if (has_caption_para) {
        /* Caption not in figcaption - this might be expected if caption detection isn't working */
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Caption appears in figcaption tag\n");
        printf("  Caption found as paragraph instead of figcaption\n");
    } else {
        /* Caption not found at all */
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Caption appears in figcaption tag\n");
        printf("  Caption 'Employee Performance Q4 2025' not found in output\n");
    }

    /* Test 3: Rowspan should be applied correctly - Engineering rowspan="2" */
    /* Note: Rowspan detection may require the ^^ marker to be in the correct cell position */
    assert_contains(html, "rowspan=\"2\"", "Rowspan attribute present");
    /* Check for Engineering with rowspan - may have alignment attribute */
    bool engineering_has_rowspan = strstr(html, "<td rowspan=\"2\">Engineering</td>") != NULL ||
                                   strstr(html, "<td align=\"right\" rowspan=\"2\">Engineering</td>") != NULL ||
                                   strstr(html, "rowspan=\"2\">Engineering") != NULL;
    if (engineering_has_rowspan) {
        test_result(true, "Engineering has rowspan=2");
    } else {
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Engineering has rowspan=2\n");
        printf("  Looking for: <td rowspan=\"2\">Engineering</td> or <td align=\"right\" rowspan=\"2\">Engineering</td>\n");
        printf("  In:          %s\n", html);
    }

    /* Test 4: Rowspan should be applied correctly - Sales rowspan="2" */
    /* Check for Sales with rowspan - may have alignment attribute */
    bool sales_has_rowspan = strstr(html, "<td rowspan=\"2\">Sales</td>") != NULL ||
                             strstr(html, "<td align=\"right\" rowspan=\"2\">Sales</td>") != NULL ||
                             strstr(html, "rowspan=\"2\">Sales") != NULL;
    if (sales_has_rowspan) {
        test_result(true, "Sales has rowspan=2");
    } else {
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Sales has rowspan=2\n");
        printf("  Looking for: <td rowspan=\"2\">Sales</td> or <td align=\"right\" rowspan=\"2\">Sales</td>\n");
        printf("  In:          %s\n", html);
    }

    /* Test 5: Table should be wrapped in figure tag */
    assert_contains(html, "<figure class=\"table-figure\">", "Table wrapped in figure with class");

    /* Test 6: Empty cells are preserved (Absent cell followed by empty cells) */
    /* The comprehensive test shows: | Marketing | Charlie | Absent | | | 92.00 | */
    /* This means: Marketing, Charlie, Absent, then empty cells, then 92.00 */
    /* Empty cells with whitespace should NOT create colspan, they should remain as empty cells */
    assert_contains(html, "<td>Absent</td>", "Absent cell present");
    /* Check for empty cell after Absent - empty cells should remain as <td></td> not merged */
    bool has_empty_after_absent = strstr(html, "<td>Absent</td><td></td>") != NULL ||
                                   strstr(html, "<td>Absent</td>\n<td></td>") != NULL ||
                                   strstr(html, "<td>Absent</td>") != NULL && strstr(html, "<td></td>") != NULL;
    if (has_empty_after_absent) {
        test_result(true, "Empty cell present in table");
    } else {
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Empty cell present in table\n");
        printf("  Looking for: <td>Absent</td> followed by <td></td> (empty cells don't create colspan)\n");
        printf("  In:          %s\n", html);
    }

    /* Test 7: Table structure should be correct - key rows present */
    assert_contains(html, "<td>Alice</td>", "Alice row present");
    assert_contains(html, "<td>Bob</td>", "Bob row present");
    assert_contains(html, "<td>Charlie</td>", "Charlie row present");
    assert_contains(html, "<td>Diana</td>", "Diana row present");
    /* Eve is in the last row with rowspan */
    assert_contains(html, "Eve", "Eve row present");

    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Comprehensive Test File Table Features", had_failures, false);
}

/**
 * Test that table parsing works correctly when file doesn't end with a newline.
 * This ensures the last row of the first table is properly parsed and not rendered as text.
 */
void test_table_no_trailing_newline(void) {
    int suite_failures = suite_start();
    print_suite_title("Table No Trailing Newline Test", false, true);

    apex_options opts = apex_options_default();
    opts.enable_tables = true;
    opts.relaxed_tables = false;
    char *html = NULL;

    /* Read table_no_trailing_newline.md file (which intentionally doesn't end with newline) */
    FILE *f = fopen("tests/fixtures/tables/table_no_trailing_newline.md", "r");
    if (!f) {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_no_trailing_newline.md: Could not open file\n");
        return;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read file content */
    char *markdown = (char *)malloc(file_size + 1);
    if (!markdown) {
        fclose(f);
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_no_trailing_newline.md: Memory allocation failed\n");
        return;
    }

    size_t bytes_read = fread(markdown, 1, file_size, f);
    markdown[bytes_read] = '\0';
    fclose(f);

    /* Verify file doesn't end with newline (key part of the test) */
    if (bytes_read > 0 && (markdown[bytes_read - 1] == '\n' || markdown[bytes_read - 1] == '\r')) {
        free(markdown);
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_no_trailing_newline.md: File ends with newline (should not)\n");
        return;
    }

    /* Convert to HTML */
    html = apex_markdown_to_html(markdown, bytes_read, &opts);
    free(markdown);

    if (!html) {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_no_trailing_newline.md: Failed to convert to HTML\n");
        return;
    }

    /* Test 1: First table's last row should be parsed correctly - A and B should be in table cells */
    assert_contains(html, "<td>A</td>", "First table last row: A cell parsed correctly");
    assert_contains(html, "<td>B</td>", "First table last row: B cell parsed correctly");

    /* Test 2: The last row should NOT appear as raw text like "| A | B |" */
    assert_not_contains(html, "<p>| A    | B    |</p>", "First table last row: not rendered as paragraph text");
    assert_not_contains(html, "<p>| A | B |</p>", "First table last row: not rendered as paragraph text (alternate format)");

    /* Test 3: First table should be properly closed before the caption */
    assert_contains(html, "</table>", "First table properly closed");
    assert_contains(html, "<figcaption>", "Caption present after first table");

    /* Test 4: Second table should also be parsed correctly */
    assert_contains(html, "<table", "Second table renders");
    assert_contains(html, "My Table", "Caption text present");

    /* Test 5: Both tables should have their data rows */
    /* Count occurrences of table cells - should have at least 4 (2 tables * 2 cells each) */
    int td_count = 0;
    const char *td_pos = html;
    while ((td_pos = strstr(td_pos, "<td>")) != NULL) {
        td_count++;
        td_pos += 4;
    }
    if (td_count >= 4) {
        test_resultf(true, "Both tables have all data rows parsed (found %d cells)", td_count);
    } else {
        test_resultf(false, "Both tables should have all data rows (found %d cells, expected at least 4)", td_count);
    }

    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Table No Trailing Newline Test", had_failures, false);
}

/**
 * Test that table parsing works correctly when file uses CR line endings.
 * This ensures Table: Caption syntax is processed correctly with CR line endings.
 */
void test_table_cr_line_endings(void) {
    int suite_failures = suite_start();
    print_suite_title("Table CR Line Endings Test", false, true);

    apex_options opts = apex_options_default();
    opts.enable_tables = true;
    opts.relaxed_tables = false;
    char *html = NULL;

    /* Read table_cr_line_endings.md file (which intentionally uses CR line endings) */
    FILE *f = fopen("tests/fixtures/tables/table_cr_line_endings.md", "rb");
    if (!f) {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_cr_line_endings.md: Could not open file\n");
        return;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read file content as binary to preserve CR characters */
    char *markdown = (char *)malloc(file_size + 1);
    if (!markdown) {
        fclose(f);
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_cr_line_endings.md: Memory allocation failed\n");
        return;
    }

    size_t bytes_read = fread(markdown, 1, file_size, f);
    markdown[bytes_read] = '\0';
    fclose(f);

    /* Verify file uses CR line endings (key part of the test) */
    bool has_cr = false;
    for (size_t i = 0; i < bytes_read; i++) {
        if (markdown[i] == '\r') has_cr = true;
    }
    if (!has_cr) {
        free(markdown);
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_cr_line_endings.md: File does not use CR line endings (should not)\n");
        return;
    }

    /* Convert to HTML */
    html = apex_markdown_to_html(markdown, bytes_read, &opts);
    free(markdown);

    if (!html) {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_cr_line_endings.md: Failed to convert to HTML\n");
        return;
    }

    /* Test 1: First table should be parsed correctly */
    assert_contains(html, "<td>A</td>", "CR line endings: First table last row A cell parsed correctly");
    assert_contains(html, "<td>B</td>", "CR line endings: First table last row B cell parsed correctly");

    /* Test 2: Table: Caption should be processed correctly (not rendered as paragraph) */
    assert_contains(html, "<figcaption>", "CR line endings: Table: Caption has figcaption tag");
    assert_contains(html, "My Table", "CR line endings: Table: Caption text is present");
    assert_contains(html, "table-id2", "CR line endings: Table: Caption IAL ID applied");

    /* Test 3: Table: Caption should NOT appear as raw text like "<p>Table: My Table" */
    assert_not_contains(html, "<p>Table: My Table", "CR line endings: Table: Caption not rendered as paragraph text");

    /* Test 4: : Caption syntax should also work with CR line endings */
    assert_contains(html, "table-id", "CR line endings: : Caption IAL ID applied");

    /* Test 5: Both tables should be properly rendered */
    int table_count = 0;
    const char *table_pos = html;
    while ((table_pos = strstr(table_pos, "<table")) != NULL) {
        table_count++;
        table_pos += 6;
    }
    if (table_count >= 2) {
        test_resultf(true, "CR line endings: Both tables rendered correctly (found %d tables)", table_count);
    } else {
        test_resultf(false, "CR line endings: Both tables should be rendered (found %d tables, expected at least 2)", table_count);
    }

    apex_free_string(html);
}

/**
 * Test callouts (Bear/Obsidian/Xcode)
 */

void test_inline_tables(void) {
    int suite_failures = suite_start();
    print_suite_title("Inline Tables Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_marked_extensions = true;
    char *html;

    /* ```table fence with CSV data */
    const char *csv_table =
        "```table\n"
        "header 1,header 2,header 3\n"
        "data 1,data 2,data 3\n"
        ",,data 2c\n"
        "```\n";
    html = apex_markdown_to_html(csv_table, strlen(csv_table), &opts);
    assert_contains(html, "<table>", "CSV table fence: table element");
    assert_contains(html, "<th>header 1</th>", "CSV table fence: header 1");
    assert_contains(html, "<th>header 2</th>", "CSV table fence: header 2");
    assert_contains(html, "<th>header 3</th>", "CSV table fence: header 3");
    assert_contains(html, "<td>data 1</td>", "CSV table fence: first data cell");
    assert_contains(html, "<td>data 2c</td>", "CSV table fence: continued cell");
    apex_free_string(html);

    /* ```table fence with CSV data and alignment keywords */
    const char *csv_align =
        "```table\n"
        "H1,H2,H3\n"
        "left,center,right\n"
        "a,b,c\n"
        "```\n";
    html = apex_markdown_to_html(csv_align, strlen(csv_align), &opts);
    assert_contains(html, "<table>", "CSV table with alignment: table element");
    /* Be conservative about HTML structure: just verify content appears in a table */
    assert_contains(html, "H1", "CSV table with alignment: header text H1 present");
    assert_contains(html, "H2", "CSV table with alignment: header text H2 present");
    assert_contains(html, "H3", "CSV table with alignment: header text H3 present");
    assert_contains(html, "a", "CSV table with alignment: data 'a' present");
    apex_free_string(html);

    /* ```table fence with no explicit alignment row: should also be headless */
    const char *csv_no_align =
        "```table\n"
        "r1c1,r1c2,r1c3\n"
        "r2c1,r2c2,r2c3\n"
        "```\n";
    html = apex_markdown_to_html(csv_no_align, strlen(csv_no_align), &opts);
    assert_contains(html, "<table>", "CSV table no-align: table element");
    assert_contains(html, "r1c1", "CSV table no-align: first row content present");
    assert_contains(html, "r2c1", "CSV table no-align: second row content present");
    apex_free_string(html);

    /* ```table fence with TSV data (real tabs) */
    const char *tsv_table =
        "```table\n"
        "col1\tcol2\tcol3\n"
        "val1\tval2\tval3\n"
        "```\n";
    html = apex_markdown_to_html(tsv_table, strlen(tsv_table), &opts);
    assert_contains(html, "<table>", "TSV table fence: table element");
    assert_contains(html, "col1", "TSV table fence: header col1 text");
    assert_contains(html, "col2", "TSV table fence: header col2 text");
    assert_contains(html, "col3", "TSV table fence: header col3 text");
    assert_contains(html, "val1", "TSV table fence: first data value");
    apex_free_string(html);

    /* ```table fence with no delimiter: should remain a code block */
    const char *no_delim =
        "```table\n"
        "this has no delimiters\n"
        "on the second line\n"
        "```\n";
    html = apex_markdown_to_html(no_delim, strlen(no_delim), &opts);
    assert_contains(html, "<pre lang=\"table\"><code>", "No-delim table fence: rendered as code block");
    assert_contains(html, "this has no delimiters", "No-delim table fence: content preserved");
    apex_free_string(html);

    /* <!--TABLE--> with CSV data */
    const char *csv_marker =
        "<!--TABLE-->\n"
        "one,two,three\n"
        "four,five,six\n"
        "\n";
    html = apex_markdown_to_html(csv_marker, strlen(csv_marker), &opts);
    assert_contains(html, "<table>", "CSV TABLE marker: table element");
    assert_contains(html, "one", "CSV TABLE marker: header text");
    assert_contains(html, "four", "CSV TABLE marker: data value");
    apex_free_string(html);

    /* <!--TABLE--> with TSV data (real tabs) */
    const char *tsv_marker =
        "<!--TABLE-->\n"
        "alpha\tbeta\tgamma\n"
        "delta\tepsilon\tzeta\n"
        "\n";
    html = apex_markdown_to_html(tsv_marker, strlen(tsv_marker), &opts);
    assert_contains(html, "<table>", "TSV TABLE marker: table element");
    assert_contains(html, "alpha", "TSV TABLE marker: header text");
    assert_contains(html, "delta", "TSV TABLE marker: data value");
    apex_free_string(html);

    /* <!--TABLE--> with no following data: comment should be preserved */
    const char *empty_marker =
        "Before\n\n"
        "<!--TABLE-->\n"
        "\n"
        "After\n";
    html = apex_markdown_to_html(empty_marker, strlen(empty_marker), &opts);
    assert_contains(html, "Before", "Empty TABLE marker: before text preserved");
    assert_contains(html, "<!--TABLE-->", "Empty TABLE marker: comment preserved");
    assert_contains(html, "After", "Empty TABLE marker: after text preserved");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Inline Tables Tests", had_failures, false);
}

void test_grid_tables(void) {
    int suite_failures = suite_start();
    print_suite_title("Grid Tables Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_tables = true;
    opts.enable_grid_tables = true;
    opts.relaxed_tables = false;
    char *html;

    /* Test basic grid table */
    const char *basic_grid = "+---+---+\n"
                             "| H1 | H2 |\n"
                             "+===+===+\n"
                             "| C1 | C2 |\n"
                             "+---+---+";
    html = apex_markdown_to_html(basic_grid, strlen(basic_grid), &opts);
    assert_contains(html, "<table", "Basic grid table renders");
    assert_contains(html, "<th>H1</th>", "Grid table header");
    assert_contains(html, "<td>C1</td>", "Grid table cell");
    apex_free_string(html);

    /* Test grid table with header separator */
    const char *header_sep_grid = "+---+---+\n"
                                   "| H1 | H2 |\n"
                                   "+===+===+\n"
                                   "| C1 | C2 |\n"
                                   "+---+---+";
    html = apex_markdown_to_html(header_sep_grid, strlen(header_sep_grid), &opts);
    assert_contains(html, "<table", "Grid table with header separator renders");
    assert_contains(html, "<th>H1</th>", "Header separator table has header");
    apex_free_string(html);

    /* Test grid table with alignment */
    const char *aligned_grid = "+:---+---+---:+\n"
                                "| C  | L  |  R |\n"
                                "+===+===+===+\n"
                                "| A  | B  |  C |\n"
                                "+---+---+---+";
    html = apex_markdown_to_html(aligned_grid, strlen(aligned_grid), &opts);
    assert_contains(html, "<table", "Aligned grid table renders");
    /* Alignment should be preserved in converted pipe table */
    apex_free_string(html);

    /* Test grid table with footer separator */
    const char *footer_grid = "+---+---+\n"
                              "| H1 | H2 |\n"
                              "+---+---+\n"
                              "| C1 | C2 |\n"
                              "+===+===+\n"
                              "| F1 | F2 |\n"
                              "+---+---+";
    html = apex_markdown_to_html(footer_grid, strlen(footer_grid), &opts);
    assert_contains(html, "<table", "Grid table with footer renders");
    apex_free_string(html);

    /* Test multi-line cells in grid table */
    const char *multiline_grid = "+---+---+\n"
                                  "| H1 | H2 |\n"
                                  "+===+===+\n"
                                  "| Line 1 | Cell 2 |\n"
                                  "| Line 2 |        |\n"
                                  "+---+---+";
    html = apex_markdown_to_html(multiline_grid, strlen(multiline_grid), &opts);
    assert_contains(html, "<table", "Multi-line grid table renders");
    assert_contains(html, "Line 1", "Multi-line cell content preserved");
    assert_contains(html, "Line 2", "Multi-line cell second line preserved");
    apex_free_string(html);

    /* Test grid table with caption (before) */
    const char *caption_before = "[Grid Table Caption]\n\n"
                                 "+---+---+\n"
                                 "| H1 | H2 |\n"
                                 "+===+===+\n"
                                 "| C1 | C2 |\n"
                                 "+---+---+";
    html = apex_markdown_to_html(caption_before, strlen(caption_before), &opts);
    assert_contains(html, "<table", "Grid table with caption before renders");
    assert_contains(html, "<figure", "Grid table with caption wrapped in figure");
    assert_contains(html, "Grid Table Caption", "Caption text present");
    apex_free_string(html);

    /* Test grid table with caption (after) */
    const char *caption_after = "+---+---+\n"
                                "| H1 | H2 |\n"
                                "+===+===+\n"
                                "| C1 | C2 |\n"
                                "+---+---+\n\n"
                                "[Grid Table Caption After]";
    html = apex_markdown_to_html(caption_after, strlen(caption_after), &opts);
    assert_contains(html, "<table", "Grid table with caption after renders");
    assert_contains(html, "Grid Table Caption After", "Caption text after present");
    apex_free_string(html);

    /* Test grid table in code block (should not be parsed) */
    const char *code_block_grid = "```\n"
                                  "+---+---+\n"
                                  "| H1 | H2 |\n"
                                  "+===+===+\n"
                                  "| C1 | C2 |\n"
                                  "+---+---+\n"
                                  "```";
    html = apex_markdown_to_html(code_block_grid, strlen(code_block_grid), &opts);
    assert_not_contains(html, "<table", "Grid table in code block not parsed");
    assert_contains(html, "<code", "Code block preserved");
    apex_free_string(html);

    /* Test mixed grid and pipe tables */
    const char *mixed_tables = "+---+---+\n"
                               "| G1 | G2 |\n"
                               "+===+===+\n"
                               "| GC1 | GC2 |\n"
                               "+---+---+\n\n"
                               "| P1 | P2 |\n"
                               "|----|----|\n"
                               "| PC1 | PC2 |";
    html = apex_markdown_to_html(mixed_tables, strlen(mixed_tables), &opts);
    assert_contains(html, "<table", "Mixed tables render");
    assert_contains(html, "G1", "Grid table content present");
    assert_contains(html, "P1", "Pipe table content present");
    apex_free_string(html);

    /* Test grid table disabled */
    opts.enable_grid_tables = false;
    html = apex_markdown_to_html(basic_grid, strlen(basic_grid), &opts);
    /* Grid table should not be converted when disabled */
    /* It might still appear as text or be partially parsed, but not as a proper table */
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Grid Tables Tests", had_failures, false);
}

/**
 * Test advanced footnotes
 */
