# Apex Architecture

## Overview

Apex is built on top of cmark-gfm, the GitHub-maintained CommonMark parser. It extends cmark-gfm with additional syntax support for MultiMarkdown, Kramdown, and Marked's special features.

## Components

### Core Library (`src/apex.c`)

- **apex_options**: Configuration structure for processor modes and features
- **apex_markdown_to_html()**: Main conversion function
- **apex_to_cmark_options()**: Maps Apex options to cmark-gfm flags
- **apex_register_extensions()**: Registers cmark-gfm extensions based on mode

### CLI Tool (`cli/main.c`)

Command-line interface that accepts:

- Input from files or stdin
- Various processor modes (commonmark, gfm, mmd, kramdown, unified)
- Feature flags to enable/disable specific syntax

### cmark-gfm Integration (`vendor/cmark-gfm/`)

Apex uses cmark-gfm's extension system to add features:

- **Parser**: Tokenizes and builds AST (Abstract Syntax Tree)
- **AST nodes**: Structured representation of the document
- **Extensions**: Pluggable syntax additions (tables, strikethrough, etc.)
- **Renderers**: Convert AST to HTML, LaTeX, CommonMark, etc.

## Processing Pipeline

1. **Input** → Markdown text + options
2. **Parser creation** → `cmark_parser_new()` with flags
3. **Extension registration** → Attach syntax extensions based on mode
4. **Parsing** → `cmark_parser_feed()` + `cmark_parser_finish()`
5. **AST** → Tree of `cmark_node` structures
6. **Rendering** → `cmark_render_html()` walks AST and generates HTML
7. **Output** → HTML string

## Extension System

cmark-gfm's extension system allows hooking into:

- **Block parsing**: Custom block-level syntax (like tables, callouts)
- **Inline parsing**: Custom inline syntax (like wiki links, math)
- **Rendering**: Custom HTML/LaTeX output for extension nodes

### Existing Extensions (from cmark-gfm)

- **table**: GFM-style tables with pipes
- **strikethrough**: `~~text~~` syntax
- **autolink**: Automatic URL linking
- **tasklist**: `- [ ]` and `- [x]` checkboxes
- **tagfilter**: HTML tag filtering for security

### Planned Apex Extensions

- **metadata**: YAML, MMD, and Pandoc metadata blocks
- **definition_lists**: Kramdown-style definition lists
- **attributes**: `{: #id .class}` syntax on any element
- **footnotes_inline**: `^[inline footnote]` syntax (extends cmark-gfm footnotes)
- **math**: `$inline$` and `$$display$$` math blocks
- **critic_markup**: `{++add++}`, `{--del--}`, etc.
- **wiki_links**: `[[Page Name]]` syntax
- **callouts**: `> [!NOTE]` Obsidian/Bear style
- **marked_special**: `<!--TOC-->`, `<<[include]>>`, page breaks, etc.

## Processor Modes

### CommonMark
Pure CommonMark spec compliance. No extensions.

### GFM (GitHub Flavored Markdown)
- Tables
- Strikethrough
- Task lists
- Autolinks
- Hard line breaks

### MultiMarkdown
- Metadata blocks
- Footnotes
- Tables
- Smart typography
- Math support
- File includes
- Metadata variable replacement `[%key]`

### Kramdown
- Attributes `{: #id .class}`
- Definition lists
- Footnotes
- Tables
- Smart typography
- Math support

### Unified (default)
All features enabled - the superset of all modes.

## Building

```bash
mkdir build && cd build
cmake ..
make
```

Outputs:

- `apex` - CLI binary
- `libapex.dylib` / `libapex.so` - Shared library
- `libapex.a` - Static library
- `Apex.framework` - macOS framework (if on macOS)

## Next Steps

1. Implement metadata parsing extension
2. Add definition lists support
3. Implement Kramdown attributes
4. Add wiki-style links
5. Implement callouts (Obsidian/Bear style)
6. Add Marked's special syntax
7. Implement math block detection
8. Add Critic Markup support
9. Comprehensive test suite

