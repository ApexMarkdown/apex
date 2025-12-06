# Standalone HTML Document Output - NEW FEATURE

## Overview

Apex can now generate complete, self-contained HTML5 documents with proper structure, metadata, and styling.

## Usage

### Basic Standalone Output

```bash
apex --standalone document.md
# or shorthand:
apex -s document.md
```

### With Custom Title

```bash
apex --standalone --title "My Report" report.md
```

### With External CSS

```bash
apex --standalone --style /path/to/styles.css document.md
```

### Combined Example

```bash
apex -s --title "Project Report" --style report.css report.md -o report.html
```

## Generated HTML Structure

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta name="generator" content="Apex 0.1.0">
  <title>Document Title</title>
  
  <!-- Either external CSS: -->
  <link rel="stylesheet" href="styles.css">
  
  <!-- OR default inline styles -->
  <style>
    /* Responsive, modern styling */
    body { font-family: system-ui; max-width: 800px; margin: 0 auto; }
    /* ... more styles ... */
  </style>
</head>
<body>

  <!-- Your content here -->

</body>
</html>
```

## Default Styles

When no `--style` is provided, Apex includes beautiful default inline styles:

### Typography
- Modern system font stack (`-apple-system`, `BlinkMacSystemFont`, `Segoe UI`, etc.)
- Readable line-height (1.6)
- Clean color scheme (#333 on white)

### Layout
- Responsive centered layout
- Max-width: 800px
- Comfortable margins and padding
- Mobile-friendly viewport

### Element Styling
- **Code blocks**: Light gray background, horizontal scrolling
- **Inline code**: Rounded corners, subtle background
- **Blockquotes**: Left border, indented, muted color
- **Tables**: Bordered cells, header row styling
- **Callouts**: Colored borders and backgrounds (note, warning, tip, danger)
- **Page breaks**: Print-friendly styling

## Use Cases

### Documentation Sites
```bash
apex -s --title "API Docs" --style docs.css api.md -o index.html
```

### Reports
```bash
apex -s --title "Q4 Report" --style corporate.css report.md -o report.html
```

### Blog Posts
```bash
apex -s --title "My Post" --style blog.css post.md -o post.html
```

### Quick Previews
```bash
# No CSS needed - beautiful defaults
apex -s document.md > preview.html
open preview.html
```

### Email HTML
```bash
# Inline styles work great for email
apex -s --title "Newsletter" newsletter.md > email.html
```

## Fragment Mode (Default)

Without `--standalone`, Apex generates HTML fragments (body content only):

```bash
apex document.md  # Just the content, no <html> wrapper
```

This is useful for:

- CMS integration
- Template systems
- AJAX content
- Partial views

## Options Summary

| Option | Description | Implies |
|--------|-------------|---------|
| `-s`, `--standalone` | Generate complete HTML document | - |
| `--title TITLE` | Set document title | - |
| `--style FILE` | Link external CSS | `--standalone` |

**Note**: Using `--style` automatically enables `--standalone` mode.

## Test Coverage

✓ 14 tests covering standalone output
✓ Doctype and HTML structure
✓ Meta tags (charset, viewport, generator)
✓ Title handling (custom and default)
✓ CSS linking
✓ Default inline styles
✓ Fragment mode preserved

All 152 tests passing!
