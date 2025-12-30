# IAL (Inline Attribute Lists) - Current Status

## Overview

Apex has **full support** for Kramdown Inline Attribute Lists (IAL) and Attribute List Definitions (ALDs). All features are implemented and working, including inline IALs for span-level elements.

**Note:** This document is maintained for historical reference. For up-to-date documentation, see:
- [Wiki: Inline Attribute Lists](https://github.com/yourusername/apex.wiki/Inline-Attribute-Lists) (complete user guide)
- [Docs: IAL Features](IAL_FEATURES.md) (technical implementation details)

## What Works ✅

### Block-Level IAL (Following Line)

IAL can be applied to blocks when placed on the line AFTER the block:

```markdown
## My Heading

{: #custom-id .my-class}

Paragraph text.
{: #para-id .important}
```

**Supported blocks:**
- ✅ Headings (H1-H6)
- ✅ Paragraphs
- ✅ Block quotes
- ✅ Code blocks
- ✅ Lists and list items
- ✅ Tables (via IAL on following line)

**Output:**

```html
<h2 id="custom-id" class="my-class">My Heading</h2>
<p id="para-id" class="important">Paragraph text.</p>
```

### Attribute Types Supported

- ✅ **IDs** - `#my-id`
- ✅ **Classes** - `.class1 .class2`
- ✅ **Key-value** - `key="value"` or `key='value'`
- ✅ **ALD References** - `{:ref}` (if ALD defined)

### Attribute List Definitions (ALD)

```markdown
{:note: .callout .callout-note #note-style}

> This is a note
{:note}
```

Works correctly!

## What Now Works ✅ (Updated)

### Inline IAL (Same Line for Headings)

**NOW SUPPORTED:**

```markdown
## Heading {: #id .class}          ← ✅ Works
```

Headings can have IALs on the same line or the next line.

### Span-Level IAL

**NOW FULLY SUPPORTED:**

```markdown
This is [styled link](url){: .highlight} ← ✅ Works
**bold text**{:.bold-style}              ← ✅ Works
*italic text*{:.italic-style}            ← ✅ Works
`code span`{:.code-class}                ← ✅ Works
```

All span-level elements (links, images, emphasis, strong, code) now support inline IALs.

### Robust Element Matching ✅

**FIXED:** The HTML renderer now uses **element indexing** with separate counters for each element type:

- Links, images, strong, emphasis, and code spans each have their own counter
- Attributes are matched by element type and index position
- Works correctly even when multiple elements share the same URL or content

**Example that now works:**

```markdown
Para 1.
{: .first}

Para 2.
{: .second}
```

Both paragraphs correctly receive their attributes.

## Current Implementation

### Stage 1: AST Processing (`ial.c`)

1. **Extract ALDs** - Find `{:ref: ...}` definitions and store them
2. **Process IAL Paragraphs** - Find `{: ...}` paragraphs
3. **Extract Attributes** - Parse ID, classes, key-value pairs
4. **Store in user_data** - Attach HTML attribute string to node
5. **Remove IAL Paragraphs** - Unlink and free pure IAL paragraphs

### Stage 2: HTML Rendering (`html_renderer.c`)

1. **Collect Nodes** - Walk AST gathering nodes with user_data, track element indices
2. **Match by Type and Index** - Match HTML tags to AST nodes using element type and index (✅ robust!)
3. **Inject Attributes** - Insert attribute string into opening tags with proper spacing
4. **Skip Special Attrs** - Ignore `data-remove`, `data-caption`, `colspan`, `rowspan`

## Issues Resolved ✅

### Issue 1: Element Matching - ✅ FIXED

**Solution Implemented**: Element indexing with separate counters for each inline element type (links, images, strong, emphasis, code).

**Result**: Reliable matching even when multiple elements share the same URL or content.

### Issue 2: Inline Heading Syntax - ✅ FIXED

**Solution Implemented**: IAL parsing for headings supports both same-line and next-line syntax.

**Result**: `## Heading {: #id}` now works correctly.

### Issue 3: Span-Level IAL - ✅ FIXED

**Solution Implemented**: Recursive IAL processing for inline elements (links, images, strong, emphasis, code spans), including nested structures.

**Result**: `[text]{: .class}`, `**bold**{:.style}`, and nested structures all work correctly.

## Workarounds

### For Headings

Use the next-line syntax instead of inline:

```markdown
## My Heading

{: #custom-id .header-class}
```

### For Emphasis/Spans

Use raw HTML:

```markdown
This has <span class="highlight">styled content</span> inline.
```

Or use the HTML markdown attribute:

```markdown
<span markdown="span" class="highlight">*styled* content</span>
```

### For Multiple Elements

Separate them with blank lines to improve matching:

```markdown
First paragraph.
{: .first}

Second paragraph.
{: .second}

Third paragraph.
{: .third}
```

## Test Results

### Working Tests

```markdown
# Test 1: Paragraph IAL
Para.
{: .works}
→ <p class="works">Para.</p> ✅

# Test 2: Heading IAL (next line)
## Heading

{: #works}
→ <h2 id="works">Heading</h2> ✅

# Test 3: Table IAL
| A | B |
|---|---|
| 1 | 2 |

{: .table-class}
→ <table class="table-class">... ✅
```

### Previously Failing Tests (Now Working ✅)

```markdown
# Test 4: Inline heading IAL
## Heading {: #works}
→ <h2 id="works">Heading</h2> ✅

# Test 5: Span IAL
[text](url){: .works}
→ <p><a href="url" class="works">text</a></p> ✅

# Test 6: Multiple paragraphs
Para 1.
{: .first}
Para 2.
{: .second}
→ Both get attributes correctly ✅
```

## Recommendations

### For Production Use

**DO:**
- ✅ Use IAL on separate lines after blocks OR on same line for headings
- ✅ Use for headings, paragraphs, lists, blockquotes, tables
- ✅ Use inline IALs for links, images, emphasis, strong, code spans
- ✅ Use ALDs for reusable attribute sets
- ✅ Use key-value pairs for custom attributes
- ✅ Test output to verify attributes applied (good practice)

**DON'T:**
- ❌ Put blank lines between block elements and their IALs (IAL won't apply)
- ❌ Use invalid attribute syntax (always use proper quotes for values with spaces)

## Comparison with Kramdown

| Feature | Kramdown | Apex | Status |
|---------|----------|------|--------|
| Block IAL (next line) | ✅ | ✅ | ✅ Working |
| Inline heading IAL | ✅ | ✅ | ✅ Implemented |
| Span IAL | ✅ | ✅ | ✅ Implemented |
| ALD (references) | ✅ | ✅ | ✅ Working |
| All block types | ✅ | ✅ | ✅ Supported |
| Key-value pairs | ✅ | ✅ | ✅ Supported |
| Nested inline IALs | ✅ | ✅ | ✅ Supported |
| Reliable matching | ✅ | ✅ | ✅ Fixed (element indexing) |

## Conclusion

IAL support is **fully functional and production-ready**. All Kramdown IAL features are implemented:
- ✅ Block-level IALs (all block types)
- ✅ Inline IALs (links, images, emphasis, strong, code)
- ✅ Same-line and next-line syntax for headings
- ✅ Attribute List Definitions (ALDs)
- ✅ Key-value pairs
- ✅ Nested inline elements
- ✅ Robust element matching

**Recommendation**: Ready for production use. For complete documentation, see [Wiki: Inline Attribute Lists](https://github.com/yourusername/apex.wiki/Inline-Attribute-Lists).

---

*Last Updated: 2025-01-XX*
*Status: Fully Implemented*

