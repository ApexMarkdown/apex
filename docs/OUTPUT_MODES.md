# Apex Output Modes

## Three Output Modes

### 1. **Default (Fragment)** - Compact HTML

```bash
apex document.md
```

**Output**: Compact HTML fragment (body content only)

```html
<h1>Header</h1>
<p>Paragraph with <strong>bold</strong>.</p>
<ul>
<li>Item 1</li>
<li>Item 2</li>
</ul>
```

**Use for**: CMS integration, templates, AJAX, partial views

---

### 2. **Pretty (--pretty)** - Formatted HTML

```bash
apex --pretty document.md
```

**Output**: Formatted HTML fragment with indentation

```html
<h1>
  Header
</h1>

<p>
  Paragraph with <strong>bold</strong>.
</p>

<ul>

  <li>
    Item 1
  </li>

  <li>
    Item 2
  </li>

</ul>
```

**Use for**: Debugging, viewing source, version control, learning

---

### 3. **Standalone (--standalone, -s)** - Complete Document

```bash
apex --standalone --title "My Doc" document.md
```

**Output**: Complete HTML5 document

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta name="generator" content="Apex 0.1.0">
  <title>My Doc</title>
  <style>
    /* Beautiful default styles */
  </style>
</head>
<body>
  [content]
</body>
</html>
```

**Use for**: Complete documents, reports, previews, blogs

---

### 4. **Standalone + Pretty** - The Best of Both 🌟

```bash
apex --standalone --pretty --title "Beautiful Doc" document.md
```

**Output**: Complete, beautifully formatted HTML5 document

```html
<!DOCTYPE html>
<html lang="en">

  <head>

      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <meta name="generator" content="Apex 0.1.0">
      <title>Beautiful Doc</title>
      <style>
        /* Beautiful default styles */
      </style>
  </head>

  <body>

    <h1>
      Header
    </h1>

    <p>
      Paragraph with <strong>bold</strong>.
    </p>

  </body>

</html>
```

**Use for**: Documentation, reports, source viewing, teaching, publishing

---

## Option Combinations

### Basic Usage

```bash
# Compact fragment (default)
apex doc.md

# Pretty fragment
apex --pretty doc.md

# Complete document
apex -s --title "Title" doc.md

# Complete + pretty
apex -s --pretty --title "Title" doc.md
```

### With CSS

```bash
# Standalone with external CSS
apex -s --style styles.css doc.md

# Standalone + pretty + CSS
apex -s --pretty --style styles.css --title "Styled" doc.md
```

### With Output File

```bash
# Everything combined
apex --standalone --pretty --title "Report" --style report.css \
     input.md -o output.html
```

---

## Comparison Table

| Option | Fragment | Complete | Formatted | Use Case |
|--------|----------|----------|-----------|----------|
| (default) | ✓ | - | - | Fast, compact, integration |
| `--pretty` | ✓ | - | ✓ | Readable fragment |
| `-s` | - | ✓ | - | Standalone document |
| `-s --pretty` | - | ✓ | ✓ | Beautiful document |

---

## Pretty-Print Details

### Indentation Rules

- **2 spaces** per nesting level
- Block elements on separate lines
- Inline elements stay inline
- Content within tags indented
- Nested structures clearly visible

### Element Types

**Block** (formatted with newlines):

- html, head, body, div, section, article, nav
- h1-h6, p, blockquote, pre
- ul, ol, li, dl, dt, dd
- table, thead, tbody, tr, th, td
- figure, figcaption, details

**Inline** (stay on same line):

- a, strong, em, code, span, abbr
- mark, del, ins, sup, sub, small

**Preserved** (no formatting changes):

- Content within `<pre>` and `<code>` blocks
- Maintains exact spacing and newlines

---

## Examples

### Simple Document

```bash
echo "# Hello World" | apex --pretty
```

Output:
```html
<h1>
  Hello World
</h1>
```

### Complex Nested Structure

```markdown
# Title

> Quote with **bold**

- List
  - Nested
```

With `--pretty`:
```html
<h1>
  Title
</h1>

<blockquote>

  <p>
    Quote with <strong>bold</strong>
  </p>

</blockquote>

<ul>

  <li>
    List
    <ul>

      <li>
        Nested
      </li>

    </ul>

  </li>

</ul>
```

---

## Performance Notes

- **Default**: Fastest (no post-processing)
- **--pretty**: Minimal overhead (~5-10% slower)
- **--standalone**: Minimal overhead (string wrapping)
- **Combined**: Both overheads, still very fast

For production pipelines where speed matters, use default mode.
For development and human consumption, use `--pretty`.

---

## Test Coverage

✓ 163 tests, all passing
✓ 11 tests for pretty mode
✓ 14 tests for standalone mode
✓ All combinations tested
✓ Indentation verified
✓ Inline preservation verified
✓ Nesting correctness verified

---

## Recommendation

**Development**: `apex --pretty doc.md`  
**Production**: `apex doc.md` (fast)  
**Complete docs**: `apex -s --title "Title" doc.md`  
**Beautiful complete docs**: `apex -s --pretty --title "Title" doc.md`  

Choose the mode that fits your workflow!
