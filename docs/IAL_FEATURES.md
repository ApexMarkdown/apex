# Inline Attribute Lists (IALs) and Attribute List Definitions (ALDs)

## Overview

Apex supports Kramdown-style Inline Attribute Lists (IALs) and Attribute List Definitions (ALDs), allowing you to add HTML attributes to Markdown elements. This feature is available in **Kramdown** and **unified** modes.

## Implementation Status

✅ **Fully Implemented**

### Block-Level IALs
- ✅ Headings (on same line or next line)
- ✅ Paragraphs
- ✅ Lists (ordered and unordered)
- ✅ Blockquotes
- ✅ Code blocks
- ✅ Tables

### Inline IALs
- ✅ Links
- ✅ Images
- ✅ Strong (bold) elements
- ✅ Emphasis (italic) elements
- ✅ Code spans
- ✅ Nested inline elements

### Attribute Types
- ✅ IDs (`#id`)
- ✅ Classes (`.class`, multiple classes supported)
- ✅ Key-value pairs (`key="value"` or `key='value'` or `key=value`)
- ✅ Quoted and unquoted values
- ✅ Escaped quotes in quoted values

### ALDs (Attribute List Definitions)
- ✅ ALD definition syntax (`{:ref-name: attributes}`)
- ✅ ALD reference syntax (`{:ref-name}`)
- ✅ Multiple references to same ALD
- ✅ ALD extraction and lookup

## Syntax Details

### Block-Level IALs

For block-level elements, IALs must appear immediately after the element (no blank line):

```markdown
Paragraph text.
{: #para-id .class1 .class2 title="Title" lang="en"}
```

### Inline IALs

For inline elements, IALs appear immediately after the element:

```markdown
[link](url){:.button title="Link Title"}
**bold**{:.bold-style}
*italic*{:.italic-style}
`code`{:.code-class}
```

### Key-Value Pairs

Supports three formats:
- `key="value"` (double quotes)
- `key='value'` (single quotes)
- `key=value` (unquoted, no spaces)

Examples:
```markdown
{: title="Important Note" lang="en" data-id="123"}
{: title='Single Quotes' lang=en data-visible=true}
```

### ALDs

Define once, reference multiple times:

```markdown
# Header 1
{:my-style}

# Header 2
{:my-style}

{:my-style: #section-title .important .highlight lang="en"}
```

## Technical Implementation

### File: `src/extensions/ial.c`

**Key Functions:**
- `parse_ial_content()` - Parses IAL/ALD attribute strings
- `apex_extract_alds()` - Extracts ALD definitions from text
- `apex_process_ial_in_tree()` - Processes IALs in AST
- `process_span_ial_in_container()` - Recursive function for inline IALs
- `find_ald()` - Looks up ALD references
- `merge_attributes()` - Merges ALD attributes
- `attributes_to_html()` - Converts attributes to HTML string

**Key Structures:**
- `apex_attributes` - Stores ID, classes, and key-value pairs
- `ald_entry` - Linked list of ALD definitions

### Element Matching

The HTML renderer uses element indexing to correctly match attributes to elements, especially when multiple elements share the same fingerprint (e.g., same URL for links, same text for strong/emph elements).

Separate counters are maintained for:
- Links (`link_count`)
- Images (`image_count`)
- Strong (`strong_count`)
- Emphasis (`emph_count`)
- Code spans (`code_inline_count`)

This ensures that when multiple links share the same URL, each one receives the correct attributes based on its position in the document.

### Processing Order

1. **Preprocessing**: ALDs are extracted from the text before parsing
2. **Parsing**: Markdown is parsed into AST by cmark-gfm
3. **Postprocessing**: IALs are processed and attributes attached to AST nodes
4. **Rendering**: HTML is rendered with attributes injected into tags

### Recursive Processing

Inline IALs are processed recursively to handle nested elements:
- Paragraphs are processed first
- Then STRONG, EMPH, and LINK nodes are processed recursively
- This allows IALs to work with nested structures like `**bold with *italic*{:.nested} inside**{:.wrapper}`

## Edge Cases Handled

1. **Multiple elements with same URL/content**: Uses element indexing to match correctly
2. **Nested inline elements**: Recursive processing ensures correct matching
3. **Text after inline IAL**: IAL text is removed, following text is preserved
4. **Block-level spacing**: Proper space injection between tag name and attributes
5. **ALD references**: Simple word references (no `#`, `.`, or `=`) are looked up as ALDs

## Testing

Comprehensive test coverage in `tests/test_runner.c`:
- Block-level IALs (headings, paragraphs, lists, blockquotes)
- Inline IALs (links, images, strong, emphasis, code)
- Key-value pairs
- ALD definitions and references
- Multiple IALs in same paragraph
- Nested inline elements
- Edge cases (whitespace, end of paragraph, etc.)

## References

- [Maruku IAL Proposal](https://golem.ph.utexas.edu/~distler/maruku/proposal.html)
- [Kramdown IAL Documentation](https://kramdown.gettalong.org/syntax.html#block-ials)

