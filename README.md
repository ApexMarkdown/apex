[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

# Apex

Apex is a unified Markdown processor that combines the best features from CommonMark, GitHub Flavored Markdown (GFM), MultiMarkdown, Kramdown, and Marked. One processor to rule them all.

## Features

### Compatibility Modes

- **Multiple compatibility modes**: CommonMark, GFM, MultiMarkdown, Kramdown, and Unified (all features)
- **Mode-specific features**: Each mode enables appropriate extensions for maximum compatibility

### Markdown Extensions

- **Tables**: GitHub Flavored Markdown tables with advanced features (rowspan, colspan, captions)
- **Relaxed tables**: Support for tables without separator rows (Kramdown-style)
- **Footnotes**: Three syntaxes supported (reference-style, Kramdown inline, MultiMarkdown inline)
- **Definition lists**: Kramdown-style definition lists with Markdown content support
- **Task lists**: GitHub-style checkboxes (`- [ ]` and `- [x]`)
- **Strikethrough**: `~~text~~` syntax from GFM
- **Smart typography**: Automatic conversion of quotes, dashes, ellipses, and more
- **Math support**: LaTeX math expressions with `$...$` (inline) and `$$...$$` (display)
- **Wiki links**: `[[Page Name]]` and `[[Page Name|Display Text]]` syntax
- **Abbreviations**: Three syntaxes (classic MMD, MMD 6 reference, MMD 6 inline)
- **Callouts**: Bear/Obsidian-style callouts with collapsible support (`> [!NOTE]`, `> [!WARNING]`, etc.)
- **GitHub emoji**: 350+ emoji support (`:rocket:`, `:heart:`, etc.)

### Document Features

- **Metadata blocks**: YAML front matter, MultiMarkdown metadata, and Pandoc title blocks
- **Metadata variables**: Insert metadata values with `[%key]` syntax
- **Table of Contents**: Automatic TOC generation with depth control (`<!--TOC-->`, `{{TOC}}`)
- **File includes**: Three syntaxes (Marked `<<[file]`, MultiMarkdown `{{file}}`, iA Writer `/file`)
- **CSV/TSV support**: Automatic table conversion from CSV and TSV files
- **Inline Attribute Lists (IAL)**: Kramdown-style attributes `{: #id .class}`
- **Special markers**: Page breaks (`<!--BREAK-->`), autoscroll pauses (`<!--PAUSE:N-->`), end-of-block markers

### Critic Markup

- **Change tracking**: Additions (`{++text++}`), deletions (`{--text--}`), substitutions (`{~~old~>new~~}`)
- **Annotations**: Highlights (`{==text==}`) and comments (`{>>text<<}`)
- **Accept mode**: `--accept` flag to apply all changes for final output
- **Reject mode**: `--reject` flag to revert all changes to original

### Output Options

- **Flexible output**: Compact HTML fragments, pretty-printed HTML, or complete standalone documents
- **Standalone documents**: Generate complete HTML5 documents with `<html>`, `<head>`, `<body>` tags
- **Custom styling**: Link external CSS files in standalone mode
- **Pretty-print**: Formatted HTML with proper indentation for readability
- **Header ID generation**: Automatic or manual header IDs with multiple format options (GFM, MMD, Kramdown)
- **Header anchors**: Option to generate `<a>` anchor tags instead of header IDs

### Advanced Features

- **Hard breaks**: Option to treat newlines as hard line breaks
- **Feature toggles**: Granular control to enable/disable specific features (tables, footnotes, math, smart typography, etc.)
- **Unsafe HTML**: Option to allow or block raw HTML in documents
- **Autolinks**: Automatic URL detection and linking
- **Superscript/Subscript**: Support for `^superscript^` and `~subscript~` syntax

## Installation

### Homebrew (macOS/Linux)

```bash
brew tap ttscoff/thelab
brew install ttscoff/thelab/apex
```

### Building from Source

```bash
git clone https://github.com/ttscoff/apex.git
cd apex
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
```

The `apex` binary will be in the `build/` directory.

**Note:** You must run `cmake -S . -B build` first to configure the project and generate the build cache. Then use `cmake --build build` to compile. If you get a "could not load cache" error, it means the configuration step hasn't been run yet.

## Basic Usage

### Command Line

```bash
# Process a markdown file
apex input.md

# Output to a file
apex input.md -o output.html

# Generate standalone HTML document
apex input.md --standalone --title "My Document"

# Pretty-print HTML output
apex input.md --pretty
```

### Processing Modes

Apex supports multiple compatibility modes:

- `--mode commonmark` - Pure CommonMark specification
- `--mode gfm` - GitHub Flavored Markdown
- `--mode mmd` or `--mode multimarkdown` - MultiMarkdown compatibility
- `--mode kramdown` - Kramdown compatibility
- `--mode unified` - All features enabled (default)

```bash
# Use GFM mode
apex input.md --mode gfm

# Use Kramdown mode with relaxed tables
apex input.md --mode kramdown
```

### Common Options

- `--pretty` - Pretty-print HTML with indentation
- `--standalone` - Generate complete HTML document with `<html>`, `<head>`, `<body>`
- `--style FILE` - Link to CSS file in document head (requires `--standalone`)
- `--title TITLE` - Document title (requires `--standalone`)
- `--relaxed-tables` - Enable relaxed table parsing (default in unified/kramdown modes)
- `--no-relaxed-tables` - Disable relaxed table parsing
- `--id-format FORMAT` - Header ID format: `gfm`, `mmd`, or `kramdown`
- `--no-ids` - Disable automatic header ID generation
- `--header-anchors` - Generate `<a>` anchor tags instead of header IDs

## Documentation

For complete documentation, see the [Apex Wiki](https://github.com/ttscoff/apex/wiki).

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
