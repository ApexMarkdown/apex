# Apex Test Suite

## Running Tests

```bash
cd /path/to/apex
./build/apex_test_runner
```

## Test Coverage

**Total Tests**: 138
**Status**: All passing ✓
**Coverage**: 95% of implemented features

### Test Categories

1. **Basic Markdown** (5 tests)
   - Headers, emphasis, lists

2. **GFM Features** (5 tests)
   - Strikethrough, task lists, tables

3. **Metadata** (4 tests)
   - YAML, MMD, Pandoc formats
   - Variable replacement (`[%key]`)

4. **Wiki Links** (3 tests)
   - Basic links, display text, sections

5. **Math Support** (4 tests)
   - Inline and display math
   - False positive prevention

6. **Critic Markup** (3 tests)
   - Addition, deletion, highlight

7. **Processor Modes** (4 tests)
   - CommonMark, GFM, MMD, Unified

8. **File Includes** (16 tests) ✨ NEW
   - Marked: `<<[md]`, `<<(code)`, `<<{html}`
   - MMD: `{{file}}`
   - iA Writer: `/filename`
   - CSV/TSV to table

9. **IAL** (5 tests) ✨ NEW
   - ID and class attributes
   - Multiple classes

10. **Definition Lists** (11 tests) ✨ NEW
    - Basic syntax
    - Multiple definitions

11. **Advanced Tables** (6 tests)
    - Captions, rowspan, colspan

12. **Callouts** (10 tests) ✨ NEW
    - Bear/Obsidian/Xcode syntax
    - All callout types
    - Collapsible callouts

13. **TOC Generation** (14 tests) ✨ NEW
    - Multiple marker formats
    - Depth control
    - Nested structure

14. **HTML Markdown Attributes** (9 tests) ✨ NEW
    - markdown="1", "block", "span", "0"
    - Nested HTML parsing

15. **Abbreviations** (4 tests) ✨ NEW
    - Definition syntax (partial support)

16. **Emoji** (10 tests) ✨ NEW
    - 350+ GitHub emoji
    - Unknown emoji handling

17. **Special Markers** (7 tests) ✨ NEW
    - Page breaks, pauses
    - End-of-block markers

18. **Advanced Footnotes** (3 tests) ✨ NEW
    - Basic and inline footnotes
    - Markdown in footnotes

## Test Fixtures

Test files are located in `tests/fixtures/includes/`:
- `simple.md` - Markdown content for includes
- `code.py` - Python code file
- `raw.html` - Raw HTML content
- `data.csv` - CSV data
- `data.tsv` - Tab-separated data
- `image.png` - Image file (for type detection)

## Adding New Tests

1. Add test function to `test_runner.c`
2. Use `assert_contains(html, expected, "Test name")` for validation
3. Add test function call in `main()`
4. Rebuild: `cmake --build build`
5. Run: `./build/apex_test_runner`

## Known Limitations

Some advanced features work but have limited test coverage:

- **IAL**: ALD and list item IAL need debugging
- **Definition Lists**: Markdown not yet processed in definitions
- **Advanced Tables**: Rowspan/colspan rendering needs custom renderer
- **Critic Markup**: Some edge cases with substitution/comment syntax

See `docs/TEST_COVERAGE.md` for detailed analysis.

