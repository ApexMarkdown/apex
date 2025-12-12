% APEX(1)
% Brett Terpstra
% December 2025

# NAME

apex - Unified Markdown processor supporting CommonMark, GFM, MultiMarkdown, and Kramdown

# SYNOPSIS

**apex** [*options*] [*file*]

# DESCRIPTION

Apex is a unified Markdown processor that combines the best features from CommonMark, GitHub Flavored Markdown (GFM), MultiMarkdown, Kramdown, and Marked. One processor to rule them all.

If no file is specified, **apex** reads from stdin.

# OPTIONS

## Processing Modes

**-m** *MODE*, **--mode** *MODE*
:   Processor mode: **commonmark**, **gfm**, **mmd** (or **multimarkdown**), **kramdown**, or **unified** (default). Each mode enables different features and syntax compatibility.

## Input/Output

**-o** *FILE*, **--output** *FILE*
:   Write output to *FILE* instead of stdout.

**-s**, **--standalone**
:   Generate complete HTML document with `<html>`, `<head>`, and `<body>` tags.

**--style** *FILE*
:   Link to CSS file in document head (requires **--standalone**).

**--title** *TITLE*
:   Document title (requires **--standalone**, default: "Document").

**--pretty**
:   Pretty-print HTML with indentation and whitespace.

## Feature Flags

**--accept**
:   Accept all Critic Markup changes (apply edits).

**--reject**
:   Reject all Critic Markup changes (revert edits).

**--enable-includes**
:   Enable file inclusion.

**--meta-file** *FILE*
:   Load metadata from an external file. Auto-detects format: YAML (starts with `---`), MultiMarkdown (key: value pairs), or Pandoc (starts with `%`). Metadata from the file is merged with document metadata, with document metadata taking precedence.

**--meta** *KEY=VALUE*
:   Set a metadata key-value pair. Can be used multiple times. Supports comma-separated pairs (e.g., `--meta KEY1=value1,KEY2=value2`). Values can be quoted to include spaces and special characters. Command-line metadata takes precedence over both file and document metadata.

**--hardbreaks**
:   Treat newlines as hard breaks.

**--no-footnotes**
:   Disable footnote support.

**--no-math**
:   Disable math support.

**--no-smart**
:   Disable smart typography.

**--no-tables**
:   Disable table support.

**--no-ids**
:   Disable automatic header ID generation.

**--header-anchors**
:   Generate `<a>` anchor tags instead of header IDs.

**--wikilinks**, **--no-wikilinks**
:   Enable wiki link syntax `[[PageName]]`. Default: disabled.

## Header ID Format

**--id-format** *FORMAT*
:   Header ID format: **gfm** (default), **mmd**, or **kramdown**. Modes auto-set format; use this to override in unified mode.

## List Options

**--alpha-lists**, **--no-alpha-lists**
:   Support alpha list markers (a., b., c. and A., B., C.).

**--mixed-lists**, **--no-mixed-lists**
:   Allow mixed list markers at same level (inherit type from first item).

## Table Options

**--relaxed-tables**, **--no-relaxed-tables**
:   Enable relaxed table parsing (no separator rows required). Default: enabled in unified/kramdown modes, disabled in commonmark/gfm/multimarkdown modes.

## HTML and Links

**--unsafe**, **--no-unsafe**
:   Allow raw HTML in output. Default: true for unified/mmd/kramdown modes, false for commonmark/gfm modes.

**--autolink**, **--no-autolink**
:   Enable autolinking of URLs and email addresses. Default: enabled in GFM, MultiMarkdown, Kramdown, and unified modes; disabled in CommonMark mode.

**--obfuscate-emails**
:   Obfuscate email links and text using HTML entities (hex-encoded).

## Superscript/Subscript

**--sup-sub**, **--no-sup-sub**
:   Enable MultiMarkdown-style superscript and subscript syntax. The `^` character creates superscript for the text immediately following it (stops at space or punctuation). The `~` character creates subscript when used within a word/identifier (e.g., `H~2~O` creates H₂O). When tildes are at word boundaries (e.g., `~text~`), they create underline instead. Default: enabled in unified and MultiMarkdown modes.

## General Options

**-h**, **--help**
:   Show help message and exit.

**-v**, **--version**
:   Show version information and exit.

# EXAMPLES

Process a markdown file:

    apex input.md

Output to a file:

    apex input.md -o output.html

Generate standalone HTML document:

    apex input.md --standalone --title "My Document"

Pretty-print HTML output:

    apex input.md --pretty

Use GFM mode:

    apex input.md --mode gfm

Use Kramdown mode with relaxed tables:

    apex input.md --mode kramdown

Process from stdin:

    echo "# Hello" | apex

# PROCESSING MODES

**commonmark**
:   Pure CommonMark specification. Minimal features, maximum compatibility.

**gfm**
:   GitHub Flavored Markdown. Includes tables, strikethrough, task lists, autolinks, and more.

**mmd**, **multimarkdown**
:   MultiMarkdown compatibility. Includes metadata, definition lists, footnotes, and more.

**kramdown**
:   Kramdown compatibility. Includes relaxed tables, IAL (Inline Attribute Lists), and more.

**unified** (default)
:   All features enabled. Combines features from all modes.

# FEATURES

Apex supports a wide range of Markdown extensions:

- **Tables**: GFM-style tables with alignment
- **Footnotes**: Reference-style footnotes
- **Math**: Inline (`$...$`) and display (`$$...$$`) math with LaTeX
- **Wiki Links**: `[[Page]]`, `[[Page|Display]]`, `[[Page#Section]]`
- **Critic Markup**: All 5 types ({++add++}, {--del--}, {~~sub~~}, {==mark==}, {>>comment<<})
- **Smart Typography**: Smart quotes, dashes, ellipsis
- **Definition Lists**: MultiMarkdown-style definition lists
- **Task Lists**: GFM-style task lists
- **Metadata**: YAML front matter, MultiMarkdown metadata, Pandoc title blocks
- **Metadata Transforms**: Transform metadata values with `[%key:transform]` syntax (case conversion, string manipulation, regex replacement, date formatting, etc.)
- **Header IDs**: Automatic or manual header IDs with multiple format options
- **Relaxed Tables**: Support for tables without separator rows (Kramdown-style)
- **Superscript/Subscript**: MultiMarkdown-style superscript (`^text`) and subscript (`~text~` within words) syntax. Subscript uses paired tildes within word boundaries (e.g., `H~2~O`), while tildes at word boundaries create underline

# SEE ALSO

For complete documentation, see the [Apex Wiki](https://github.com/ttscoff/apex/wiki).

# AUTHOR

Brett Terpstra

# COPYRIGHT

Copyright (c) 2025 Brett Terpstra. Licensed under MIT License.

# BUGS

Report bugs at <https://github.com/ttscoff/apex/issues>.
