# IAL (Inline Attribute Lists) - Current Status

## Overview

Apex has **partial support** for Kramdown Inline Attribute Lists (IAL). The feature is functional for basic use cases but has significant limitations.

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

## What Doesn't Work ❌

### Inline IAL (Same Line)

**NOT SUPPORTED:**

```markdown
## Heading {: #id .class}          ← Does NOT work
This is [styled text]{: .highlight} ← Does NOT work
```

These appear literally in the output instead of being processed.

**Reason**: The IAL parser only looks for attributes on SEPARATE paragraphs, not inline within headings or spans.

### Fragile Matching

The HTML injection uses **line-number proximity matching** which is unreliable:

- Only the LAST element on a given line gets attributes
- Multiple elements on adjacent lines may not match correctly
- Complex documents with lots of nesting fail to match

**Example that fails:**

```markdown
Para 1.
{: .first}

Para 2.
{: .second}
```

Often only one paragraph gets attributes, not both.

## Current Implementation

### Stage 1: AST Processing (`ial.c`)

1. **Extract ALDs** - Find `{:ref: ...}` definitions and store them
2. **Process IAL Paragraphs** - Find `{: ...}` paragraphs
3. **Extract Attributes** - Parse ID, classes, key-value pairs
4. **Store in user_data** - Attach HTML attribute string to node
5. **Remove IAL Paragraphs** - Unlink and free pure IAL paragraphs

### Stage 2: HTML Rendering (`html_renderer.c`)

1. **Collect Nodes** - Walk AST gathering nodes with user_data
2. **Match by Line** - When rendering HTML, match tags by line number (⚠️ fragile!)
3. **Inject Attributes** - Insert attribute string into opening tags
4. **Skip Special Attrs** - Ignore `data-remove`, `data-caption`, `colspan`, `rowspan`

## Known Issues

### Issue 1: Line Number Matching

**Problem**: Matching HTML tags to AST nodes by line number is unreliable.

**Impact**:
- Multiple IALs in a document may not all render
- First/last bias in matching
- Fails in complex nested structures

**Solution Needed**: Use node-to-HTML tracking or unique markers instead of line numbers.

### Issue 2: No Inline Syntax

**Problem**: `## Heading {: #id}` syntax is not parsed.

**Impact**: All IAL must be on separate lines, which is verbose and non-standard for Kramdown.

**Solution Needed**: Parse IAL within heading text nodes and other inline contexts.

### Issue 3: No Span-Level IAL

**Problem**: `[text]{: .class}` syntax is not supported.

**Impact**: Cannot apply attributes to inline elements (spans, links, emphasis, etc.).

**Solution Needed**: Implement span-level IAL parser and renderer.

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

### Failing Tests

```markdown
# Test 4: Inline heading IAL
## Heading {: #fails}
→ <h2>Heading {: #fails}</h2> ❌

# Test 5: Span IAL
[text]{: .fails}
→ <p>[text]{: .fails}</p> ❌

# Test 6: Multiple paragraphs
Para 1.
{: .first}
Para 2.
{: .second}
→ Only one gets attributes ❌
```

## Recommendations

### For Production Use

**DO:**
- ✅ Use IAL on separate lines after blocks
- ✅ Use for headings, paragraphs, tables
- ✅ Keep IAL simple (1-2 classes, one ID)
- ✅ Test output to verify attributes applied

**DON'T:**
- ❌ Use inline IAL syntax `{: }` on same line as heading
- ❌ Use span-level IAL `[text]{: }`
- ❌ Rely on multiple IALs rendering in complex documents
- ❌ Use for production-critical styling (verify each case)

### Priority Improvements

1. **High Priority**: Fix line-number matching to be deterministic
2. **Medium Priority**: Add inline heading support `## H {: #id}`
3. **Lower Priority**: Add span-level IAL `[text]{: .class}`

## Comparison with Kramdown

| Feature | Kramdown | Apex | Status |
|---------|----------|------|--------|
| Block IAL (next line) | ✅ | ✅ | Working |
| Inline heading IAL | ✅ | ❌ | Not implemented |
| Span IAL | ✅ | ❌ | Not implemented |
| ALD (references) | ✅ | ✅ | Working |
| All block types | ✅ | ⚠️ | Partial |
| Reliable matching | ✅ | ❌ | Needs fix |

## Conclusion

IAL support is **functional but limited**. It works well for:
- Simple documents
- One IAL per element
- Block-level attributes only
- Testing/verification workflow

It is **NOT production-ready** for:
- Complex multi-IAL documents
- Inline attribute syntax
- Mission-critical styling

**Recommendation**: Use with caution, test output, have fallback styling.

---

*Last Updated: 2025-12-05*
*Apex Version: 0.1.0*

