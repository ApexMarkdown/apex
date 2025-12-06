# Critic Markup in Apex

## Overview

Apex provides comprehensive support for [CriticMarkup](http://criticmarkup.com), a system for tracking changes and annotations in plain text.

## Syntax

CriticMarkup uses five markup patterns:

### 1. Addition `{++text++}`
Text that has been added to the document.

```markdown
The document now includes {++this new section++}.
```

### 2. Deletion `{--text--}`
Text that has been removed from the document.

```markdown
The {--old approach--} is no longer used.
```

### 3. Substitution `{~~old~>new~~}`
Text that has been replaced with new text.

```markdown
The process {~~was inefficient~>is now optimized~~}.
```

### 4. Highlight `{==text==}`
Text that should be highlighted for attention (annotation, not a change).

```markdown
{==This section is critical==} for understanding the concept.
```

### 5. Comment `{>>text<<}`
Editorial comments or notes (annotation, not a change).

```markdown
{>>TODO: Expand this section with more examples<<}
```

---

## Processing Modes

Apex offers three modes for processing Critic Markup:

### Markup Mode (Default)

Shows all changes with HTML tags for styling.

**Command**:
```bash
apex document.md
```

**Output**:
```html
<ins class="critic">addition</ins>
<del class="critic">deletion</del>
<del class="critic break">old</del><ins class="critic break">new</ins>
<mark class="critic">highlight</mark>
<span class="critic comment">comment</span>
```

**Use for**: Reviewing changes, showing tracked edits

---

### Accept Mode

Applies all changes, outputs clean final text.

**Command**:
```bash
apex --accept document.md
```

**Behavior**:

- `{++addition++}` → addition
- `{--deletion--}` → (removed)
- `{~~old~>new~~}` → new
- `{==highlight==}` → highlight (plain text)
- `{>>comment<<}` → (removed)

**Output**: Clean text with all edits applied

**Use for**: Publishing final version, accepting all changes

---

### Reject Mode

Reverts all changes, restores original text.

**Command**:
```bash
apex --reject document.md
```

**Behavior**:

- `{++addition++}` → (removed)
- `{--deletion--}` → deletion
- `{~~old~>new~~}` → old
- `{==highlight==}` → highlight (plain text)
- `{>>comment<<}` → (removed)

**Output**: Clean text with all edits rejected

**Use for**: Reverting to original, rejecting all changes

---

## Examples

### Source Document

```markdown
# Document Draft

This paragraph has {++new content++} and {--old content--} that needs review.

The formula {~~E=mc^2~>E=mc²~~} has been corrected.

{==This section is important==} for understanding the concept.

{>>TODO: Expand this section<<}

Final text.
```

### Markup Mode Output

```html
<h1>Document Draft</h1>

<p>This paragraph has <ins class="critic">new content</ins> and
<del class="critic">old content</del> that needs review.</p>

<p>The formula <del class="critic break">E=mc^2</del>
<ins class="critic break">E=mc²</ins> has been corrected.</p>

<p><mark class="critic">This section is important</mark> for
understanding the concept.</p>

<p><span class="critic comment">TODO: Expand this section</span></p>

<p>Final text.</p>
```

### Accept Mode Output

```html
<h1>Document Draft</h1>

<p>This paragraph has new content and that needs review.</p>

<p>The formula E=mc² has been corrected.</p>

<p>This section is important for understanding the concept.</p>

<p>Final text.</p>
```

### Reject Mode Output

```html
<h1>Document Draft</h1>

<p>This paragraph has and old content that needs review.</p>

<p>The formula E=mc^2 has been corrected.</p>

<p>This section is important for understanding the concept.</p>

<p>Final text.</p>
```

---

## Default Styling

When using `--standalone` mode, Apex includes default CSS for Critic Markup:

```css
ins {
  background: #d4fcbc;  /* Light green */
  text-decoration: none;
}

del {
  background: #fbb6c2;  /* Light red */
  text-decoration: line-through;
}

mark {
  background: #fff3cd;  /* Light yellow */
}

.critic.comment {
  background: #e7e7e7;  /* Light gray */
  color: #666;
  font-style: italic;
}
```

These styles make changes clearly visible in markup mode.

---

## Highlights vs Changes

**Important distinction**:

### Content Changes (affected by accept/reject):

- `{++additions++}` - Accept: keep, Reject: remove
- `{--deletions--}` - Accept: remove, Reject: keep
- `{~~old~>new~~}` - Accept: new, Reject: old

### Annotations (NOT changes):

- `{==highlights==}` - Both modes: show text without markup
- `{>>comments<<}` - Both modes: remove entirely

This distinction ensures that highlights (marking important content) and comments (editorial notes) don't affect the final text in either accept or reject mode.

---

## Workflow Examples

### Editorial Review Process

```bash
# 1. Author writes with Critic Markup
vim draft.md

# 2. Generate review version with visible changes
apex draft.md -o review.html

# 3. After approval, generate clean final version
apex --accept draft.md -o final.html

# 4. If changes rejected, revert to original
apex --reject draft.md -o original.html
```

### Collaboration

```bash
# Show changes for review
apex --standalone --title "Review" draft.md -o review.html

# Accept changes and publish
apex --accept --standalone --title "Published" draft.md -o published.html
```

### Side-by-Side Comparison

```bash
# Generate all three versions
apex draft.md -o markup.html
apex --accept draft.md -o accepted.html
apex --reject draft.md -o rejected.html

# Compare in browser
```

---

## Features

### Protected Contexts

Critic Markup is NOT processed inside:

- Code blocks (``` ```)
- Inline code (` `)
- HTML blocks

This ensures examples remain literal.

### Nested Markup

Critic Markup can contain Markdown:

```markdown
The {++**bold new text**++} is added.
Changes to {~~*old italic*~>**new bold**~~}.
{==Important with `code`==}
```

The Markdown inside is processed after Critic Markup is converted.

### Multiple Edits

Multiple changes in the same paragraph work correctly:

```markdown
Text with {++addition++} and {--deletion--} and {~~old~>new~~} in one line.
```

---

## API Usage

### C API

```c
#include <apex/apex.h>

apex_options opts = apex_options_default();

// Markup mode (default)
opts.critic_mode = 2;  // CRITIC_MARKUP
char *markup = apex_markdown_to_html(text, len, &opts);

// Accept mode
opts.critic_mode = 0;  // CRITIC_ACCEPT
char *accepted = apex_markdown_to_html(text, len, &opts);

// Reject mode
opts.critic_mode = 1;  // CRITIC_REJECT
char *rejected = apex_markdown_to_html(text, len, &opts);
```

### Enum Values

```c
typedef enum {
    CRITIC_ACCEPT = 0,  /* Accept all changes */
    CRITIC_REJECT = 1,  /* Reject all changes */
    CRITIC_MARKUP = 2   /* Show markup with classes */
} critic_mode_t;
```

---

## Testing

Apex includes comprehensive tests for all three modes:

```bash
# Run test suite
./build/apex_test_runner

# Manual testing
echo "Test {++add++} and {--del--}" | apex --accept
echo "Test {++add++} and {--del--}" | apex --reject
echo "Test {++add++} and {--del--}" | apex
```

---

## Best Practices

### For Authors

1. **Use additions for new content**: `{++text++}`
2. **Use deletions for removed content**: `{--text--}`
3. **Use substitutions for replacements**: `{~~old~>new~~}`
4. **Use highlights for emphasis**: `{==text==}`
5. **Use comments for notes**: `{>>note<<}`

### For Reviewers

1. **Review with markup mode**: See all changes clearly
2. **Test both modes**: Generate accepted and rejected versions
3. **Compare outputs**: Ensure changes are as intended
4. **Remove comments**: Use accept/reject to clean up editorial notes

### For Publishers

1. **Use --accept for final**: Clean published version
2. **Keep markup version**: For future reference
3. **Archive rejected**: In case revert needed
4. **Combine with --standalone**: Complete publishable documents

---

## Integration

### With Standalone Mode

```bash
# Beautiful review document
apex -s --pretty --title "Review Draft" draft.md -o review.html

# Clean final version
apex --accept -s --title "Final Version" draft.md -o final.html
```

### With Pretty-Print

```bash
# Readable markup for source viewing
apex --pretty draft.md > readable.html

# Clean accepted version, nicely formatted
apex --accept --pretty draft.md > accepted.html
```

---

## Troubleshooting

**Markup not processed**:

- Ensure you're in unified mode or MMD/Kramdown mode
- CommonMark and GFM modes don't enable Critic Markup by default

**Changes not applying with --accept**:

- Check syntax is correct (proper opening/closing markers)
- Ensure no spaces in markers: `{++` not `{ ++`

**Output has extra spaces**:

- Normal behavior when markup is removed
- Spaces before/after markup preserved

---

## Comparison with Other Tools

| Feature            | Apex | MultiMarkdown | Marked 2 |
| ------------------ | ---- | ------------- | -------- |
| Markup mode        | ✓    | ✓             | ✓        |
| Accept mode        | ✓    | -             | ✓        |
| Reject mode        | ✓    | -             | ✓        |
| Highlight handling | ✓    | ✓             | ✓        |
| Comment handling   | ✓    | ✓             | ✓        |
| Nested Markdown    | ✓    | ✓             | ✓        |
| CLI flags          | ✓    | -             | GUI      |

---

## Resources

- **CriticMarkup Website**: http://criticmarkup.com
- **Specification**: http://criticmarkup.com/spec.php
- **Apex Documentation**: See `docs/USER_GUIDE.md`
- **Examples**: See `tests/` directory

---

## Summary

Apex provides complete CriticMarkup support with three processing modes:

1. **Markup** - Visual review
2. **Accept** - Apply changes
3. **Reject** - Revert changes

Combined with Apex's output modes (standalone, pretty), you get a powerful editorial workflow tool!

