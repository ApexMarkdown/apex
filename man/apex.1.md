% APEX(1)
% Brett Terpstra
% December 2025

# NAME

apex - Unified Markdown processor supporting CommonMark,
GFM, MultiMarkdown, and Kramdown

# SYNOPSIS

**apex** [*options*] [*file*]

**apex** --combine [*files*...]

**apex** --mmd-merge [*index files*...]

# DESCRIPTION

Apex is a unified Markdown processor that combines the best
features from CommonMark, GitHub Flavored Markdown (GFM),
MultiMarkdown, Kramdown, and Marked. One processor to rule
them all.

If no file is specified, **apex** reads from stdin.

# OPTIONS

## Processing Modes

**-m** *MODE*, **--mode** *MODE*
: Processor mode: **commonmark**, **gfm**, **mmd** (or
**multimarkdown**), **kramdown**, or **unified** (default).
Each mode enables different features and syntax
compatibility.

## Input/Output

**-o** *FILE*, **--output** *FILE*
:   Write output to *FILE* instead of stdout.

**-s**, **--standalone**
: Generate complete HTML document with `<html>`, `<head>`,
and `<body>` tags.

**--style** *FILE*, **--css** *FILE*
: Link to CSS file(s) in document head (requires
**--standalone**). Can be used multiple times or accept
comma-separated list (e.g., `--css style.css --css syntax.css`
or `--css style.css,syntax.css`). Overrides CSS metadata if
specified.

**--embed-css**
:   When used with **--css FILE**, read the CSS file(s) and embed
their contents into `<style>` tags in the document head instead
of emitting `<link rel="stylesheet">` tags. All specified
stylesheets are embedded.

**--code-highlight** *TOOL*
: Use external tool for syntax highlighting of code blocks.
*TOOL* must be **pygments** (or **p**, **pyg**) or **skylighting**
(or **s**, **sky**). Code blocks are sent to the external tool
with their language specifier (if present) or with
auto-detection enabled. The highlighted HTML output replaces the
original code block in the document.

**--code-line-numbers**
: Include line numbers in syntax-highlighted code blocks.
Requires **--code-highlight**. When used with Pygments, adds
`linenos=1` option. When used with Skylighting, adds `-n` flag.

**--highlight-language-only**
: Only apply syntax highlighting to code blocks that have a language
specified (via ` ```language ` or IAL). Code blocks without a language
will be left unhighlighted. Requires **--code-highlight**.

**--script** *VALUE*
:   Inject `<script>` tags either before `</body>` in standalone mode or at the end of the HTML fragment in snippet mode. *VALUE* can be a path, a URL, or one of the following shorthands: `mermaid`, `mathjax`, `katex`, `highlightjs`, `highlight.js`, `prism`, `prismjs`, `htmx`, `alpine`, `alpinejs`. Can be used multiple times or with a comma-separated list (e.g., `--script mermaid,mathjax`).

**--title** *TITLE*
: Document title (requires **--standalone**, default:
"Document").

**--pretty**
:   Pretty-print HTML with indentation and whitespace.

**--aria**
: Add ARIA labels and accessibility attributes to HTML
output. When enabled, adds:

`aria-label="Table of contents"` to TOC navigation elements (`<nav class="toc">`)

    - `role="figure"` to `<figure>` elements
    - `role="table"` to `<table>` elements
    - `id` attributes to `<figcaption>` elements within

      table figures (if missing)

    - `aria-describedby` attributes to tables linking them

      to their captions

This enhances screen reader support and makes the HTML
output more accessible. Default: disabled.

## Feature Flags

**--accept**
:   Accept all Critic Markup changes (apply edits).

**--reject**
:   Reject all Critic Markup changes (revert edits).

**--code-highlight** *TOOL*
: Use external tool for syntax highlighting of code blocks.
*TOOL* must be **pygments** (or abbreviations **p**, **pyg**) or
**skylighting** (or abbreviations **s**, **sky**). Code blocks
are sent to the external tool with their language specifier (if
present) or with auto-detection enabled. The highlighted HTML
output replaces the original code block in the document.

**--code-line-numbers**
: Include line numbers in syntax-highlighted code blocks.
Requires **--code-highlight**. When used with Pygments, adds
`linenos=1` option. When used with Skylighting, adds `-n` flag.

**--highlight-language-only**
: Only apply syntax highlighting to code blocks that have a language
specified (via ` ```language ` or IAL). Code blocks without a language
will be left unhighlighted. Requires **--code-highlight**.

**--includes**, **--no-includes**
: Enable or disable file inclusion. Enabled by default in
unified mode.

**--transforms**, **--no-transforms**
:: Enable or disable metadata variable transforms
(`[%key:transform]`). When enabled, metadata values can be
transformed (case conversion, string manipulation, regex
replacement, date formatting, etc.) when inserted into the
document. Enabled by default in unified mode.

**--meta-file** *FILE*
:   Load metadata from an external file. Auto-detects format: YAML (starts with `---`), MultiMarkdown (key: value pairs), or Pandoc (starts with `%`). Metadata from the file is merged with document metadata, with document metadata taking precedence. Metadata can also control command-line options (see METADATA CONTROL OF OPTIONS below). If no `--meta-file` is provided, Apex will automatically load `$XDG_CONFIG_HOME/apex/config.yml` (or `~/.config/apex/config.yml` when `XDG_CONFIG_HOME` is not set) if it exists, as if it were passed via `--meta-file`.

**--meta** *KEY=VALUE*
:   Set a metadata key-value pair. Can be used multiple times. Supports comma-separated pairs (e.g., `--meta KEY1=value1,KEY2=value2`). Values can be quoted to include spaces and special characters. Command-line metadata takes precedence over both file and document metadata. Metadata can also control command-line options (see METADATA CONTROL OF OPTIONS below).

**--hardbreaks**
:   Treat newlines as hard breaks.

**--widont**
:   Prevent short widows in headings by inserting non-breaking spaces
    (`&nbsp;`) between trailing words when their combined length is 10
    characters or less. Applies to h1-h6 headings.

**--code-is-poetry**
:   Treat code blocks without a language as poetry by adding the `poetry`
    class. Automatically enables **--highlight-language-only**.

**--markdown-in-html**, **--no-markdown-in-html**
:   Enable or disable markdown processing inside HTML blocks with
    `markdown` attributes. Enabled by default in unified mode.

**--random-footnote-ids**
:   Use hash-based footnote IDs (e.g., `fn-a7b3c9d2-1`) instead of
    sequential IDs to avoid collisions when combining multiple documents.

**--hashtags**
:   Convert `#tags` into span-wrapped hashtags with the `mkhashtag` class.

**--style-hashtags**
:   Use the `mkstyledtag` class instead of `mkhashtag` for hashtags.
    Requires **--hashtags**.

**--proofreader**
:   Treat `==highlight==` and `~~delete~~` as CriticMarkup highlight and
    deletion syntax. Automatically enables CriticMarkup processing.

**--hr-page-break**
:   Replace `<hr>` elements with Marked-style page break divs.

**--title-from-h1**
:   Use the first H1 heading as the document title when no title is
    specified via **--title** or metadata. Requires **--standalone**.

**--page-break-before-footnotes**
:   Insert a page break before the footnotes section.

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
: Enable wiki link syntax `[[PageName]]`. Default: disabled.

## Header ID Format

**--id-format** *FORMAT*
: Header ID format: **gfm** (default), **mmd**, or
**kramdown**. Modes auto-set format; use this to override in
unified mode.

## List Options

**--alpha-lists**, **--no-alpha-lists**
:   Support alpha list markers (a., b., c. and A., B., C.).

**--mixed-lists**, **--no-mixed-lists**
: Allow mixed list markers at same level (inherit type from
first item).

## Table Options

**--relaxed-tables**, **--no-relaxed-tables**
: Enable relaxed table parsing (no separator rows required).

**--per-cell-alignment**, **--no-per-cell-alignment**
: Enable per-cell alignment markers in tables. When enabled, cells
starting with a colon (`:`) are left-aligned, ending with a colon
(`:`) are right-aligned, or both (`:content:`) are center-aligned.
The colons are stripped from the output and replaced with
`style="text-align: ..."` attributes. Default: enabled in unified
mode, disabled in commonmark, gfm, mmd, and kramdown modes.

**--captions** *POSITION*
: Table caption position: **above** or **below** (default:
**below**). Controls where table captions appear relative to
the table.

## HTML and Links

**--unsafe**, **--no-unsafe**
: Allow raw HTML in output. Default: true for
unified/mmd/kramdown modes, false for commonmark/gfm modes.

**--autolink**, **--no-autolink**
: Enable autolinking of URLs and email addresses. Default:
enabled in GFM, MultiMarkdown, Kramdown, and unified modes;
disabled in CommonMark mode.

**--obfuscate-emails**
: Obfuscate email links and text using HTML entities
(hex-encoded).

**--wikilink-space** *MODE*
:: Control how spaces in wiki link page names are handled in
the generated URL. **MODE** must be one of:

`dash` - Convert spaces to dashes: `[[Home Page]]` → `href="Home-Page"`

`none` - Remove spaces: `[[Home Page]]` → `href="HomePage"`

`underscore` - Convert spaces to underscores: `[[Home Page]]` → `href="Home_Page"`

`space` - Keep spaces (rendered as `%%20` in HTML): `[[Home Page]]` → `href="Home%20Page"`

    Default: `dash`.

**--wikilink-extension** *EXT*
::   Add a file extension to wiki link URLs. The extension is automatically prefixed with a dot if not provided. For example, `--wikilink-extension html` creates `href="Page.html"` and `--wikilink-extension .html` also creates `href="Page.html"`.

**--wikilink-sanitize**, **--no-wikilink-sanitize**
:   Sanitize wiki link URLs for cleaner, more compatible links. When enabled:

- Removes apostrophes and quotation marks (i.e. removes `"'\`´‘’“”`)
- Converts select latin-1 characters to ASCII (e.g. e-acute -> e)
- Converts uppercase to lowercase
- Replaces non-ascii and any non-alphanumeric ascii characters with the space-mode character (except `/` and `.`)
- Removes duplicate space-mode characters
- Removes leading and trailing space-mode characters

For example, with `--wikilink-sanitize --wikilink-space dash`:

`[[O'Brien's Page]]` → `href="obriens-page"`

`[[Hello   World!!!]]` → `href="hello-world"`

`[[path/to/FILE.MD]]` → `href="path/to/file.md"`

Default: disabled.

## Image Embedding

**--embed-images**
:   Embed local images as base64 data URLs in HTML output. Only local images (file paths) are embedded; remote images (http://, https://) are not processed. Images are read from the filesystem and encoded as base64 data URLs (e.g., `data:image/png;base64,...`). Relative paths are resolved using the base directory (see **--base-dir**).

**--image-captions**, **--no-image-captions**
:   Wrap images with title or alt text in `<figure>` elements with `<figcaption>`. Default: enabled in unified and MultiMarkdown modes; disabled in commonmark, gfm, and kramdown modes.

**--emoji-autocorrect**, **--no-emoji-autocorrect**
:   Convert emoji names (e.g., `:rocket:`) to Unicode emoji characters. Default: enabled in unified mode; disabled in other modes.

## Path Resolution

**--base-dir** *DIR*
:   Base directory for resolving relative paths. Used for:

    - Image embedding (with **--embed-images**)
    - File includes/transclusions
    - Relative path resolution when reading from stdin or

      when the working directory differs from the document
      location

If not specified and reading from a file, the base directory
is automatically set to the input file's directory. When
reading from stdin, this flag must be used to resolve
relative paths.

## Superscript/Subscript

**--sup-sub**, **--no-sup-sub**
:   Enable MultiMarkdown-style superscript and subscript syntax. The `^` character creates superscript for the text immediately following it (stops at space or punctuation). The `~` character creates subscript when used within a word/identifier (e.g., `H~2~O` creates H₂O). When tildes are at word boundaries (e.g., `~text~`), they create underline instead. Default: enabled in unified and MultiMarkdown modes.

**--strikethrough**, **--no-strikethrough**
:   Enable or disable GFM-style strikethrough processing (`~~text~~`). When enabled, `~~text~~` renders as `<del>text</del>`. Default: enabled in GFM and unified modes; disabled in commonmark, mmd, and kramdown modes.

**--divs**, **--no-divs**
:   Enable or disable Pandoc fenced divs syntax (`::::: {#id .class} ... :::::`). Fenced divs allow you to create HTML block elements with attributes using a special fence syntax. By default, fenced divs create `<div>` elements, but you can specify different block types using the `>blocktype` syntax (e.g., `::: >aside {.sidebar} ... :::` creates an `<aside>` element instead). Opening fences must have at least 3 colons and attributes; closing fences need at least 3 colons. Fenced divs can be nested, including different block types. Default: enabled in unified mode only.

**--spans**, **--no-spans**
:   Enable or disable Pandoc-style bracketed spans syntax (`[text]{#id .class key="val"}`). Bracketed spans allow you to create HTML `<span>` elements with attributes. The text inside the brackets is processed as markdown. If the bracketed text matches a reference link definition, it will be treated as a link instead of a span. Default: enabled in unified mode only.

## Citations and Bibliography

**--bibliography** *FILE*
: Bibliography file in BibTeX, CSL JSON, or CSL YAML format.
Can be specified multiple times to load multiple
bibliography files. Citations are automatically enabled when
this option is used. Bibliography can also be specified in
document metadata.

**--csl** *FILE*
: Citation Style Language (CSL) file for formatting
citations and bibliography. Citations are automatically
enabled when this option is used. CSL file can also be
specified in document metadata.

**--no-bibliography**
: Suppress bibliography output even when citations are
present.

**--link-citations**
: Link citations to their corresponding bibliography
entries. Citations will include `href` attributes pointing
to the bibliography entry.

**--show-tooltips**
: Show tooltips on citations when hovering (requires CSS
support).

Citation syntax is supported in MultiMarkdown and unified
modes:

- Pandoc: `[@key]`, `[@key1; @key2]`, `@key`
- MultiMarkdown: `[#key]`
- mmark: `[@RFC1234]`

Bibliography is inserted at the `<!-- REFERENCES -->` marker
or appended to the end of the document if no marker is
found.

## Indices

**--indices**
: Enable index processing. Supports both mmark and TextIndex
syntax. Default: enabled in MultiMarkdown and unified modes.

**--no-indices**
:   Disable index processing.

**--no-index**
: Suppress index generation at the end of the document.
Index markers are still created in the document, but the
index section is not generated.

Index syntax is supported in MultiMarkdown and unified
modes:

- **mmark syntax**: `(!item)`, `(!item, subitem)`, `(!!item,

  subitem)` for primary entries

- **TextIndex syntax**: `word{^}`, `[term]{^}`, `{^params}`

The index is automatically generated at the end of the
document or at the `<!--INDEX-->` marker if present. Entries
are sorted alphabetically and can be grouped by first
letter.

## Plugins

**--plugins**, **--no-plugins**
: Enable or disable external/plugin processing. Plugins
extend Apex with custom processing capabilities.

**--list-plugins**
: List installed plugins and available plugins from the
remote directory. Shows both locally installed plugins and
plugins available for installation from the Apex plugin
directory.

**--install-plugin** *ID*
:   Install a plugin by ID from the remote directory, or by Git URL/GitHub shorthand (user/repo). Plugins are installed to `$XDG_CONFIG_HOME/apex/plugins` (or `~/.config/apex/plugins` when `XDG_CONFIG_HOME` is not set). When installing from a URL or GitHub shorthand, Apex will prompt for confirmation since plugins execute unverified code.

**--uninstall-plugin** *ID*
: Uninstall a plugin by ID. Removes the plugin directory
from the plugins folder. Apex will prompt for confirmation
before removing the plugin.

## General Options

**-h**, **--help**
:   Show help message and exit.

**-v**, **--version**
:   Show version information and exit.

**--progress**, **--no-progress**
:   Show progress indicator during processing. Default: enabled when stderr is a TTY.

## Multi-file Utilities

**--combine** *files...*
: Concatenate one or more Markdown files into a single
Markdown stream, expanding all supported include syntaxes.
When a `SUMMARY.md` file is provided, Apex treats it as a
GitBook-style index and combines the linked files in order.
Output is raw Markdown suitable for piping back into Apex.

**--mmd-merge** *index files...*
:   Merge files from one or more MultiMarkdown `mmd_merge`-style index files into a single Markdown stream. Each non-empty, non-comment line in an index file specifies a document to include. Lines whose first non-whitespace character is `#` are treated as comments and ignored. Indentation (tabs or groups of four spaces) before the filename increases the header level of the included document (each indent level shifts all Markdown headings in that file down one level). Output is raw Markdown suitable for piping into Apex, for example:

apex --mmd-merge index.txt | apex --mode mmd --standalone -o
book.html

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

Process document with citations and bibliography:

    apex document.md --bibliography refs.bib

Use metadata to specify bibliography:

    apex document.md

(With bibliography specified in YAML front matter)

Use Kramdown mode with relaxed tables:

    apex input.md --mode kramdown

Process from stdin:

    echo "# Hello" | apex

# PROCESSING MODES

**commonmark**
: Pure CommonMark specification. Minimal features, maximum
compatibility.

**gfm**
: GitHub Flavored Markdown. Includes tables, strikethrough,
task lists, autolinks, and more.

**mmd**, **multimarkdown**
: MultiMarkdown compatibility. Includes metadata, definition
lists, footnotes, and more.

**kramdown**
: Kramdown compatibility. Includes relaxed tables, IAL
(Inline Attribute Lists) for adding HTML attributes to
elements, and more.

**unified** (default)
:   All features enabled. Combines features from all modes.

# METADATA CONTROL OF OPTIONS

Most command-line options can be controlled via document
metadata, allowing different files to be processed with
different settings when processing batches. This enables
per-document configuration without needing separate
command-line invocations.

**Boolean options** accept `true`/`false`, `yes`/`no`, or
`1`/`0` (case-insensitive). **String options** use the value
directly.

**Supported boolean options:**
`indices`, `wikilinks`, `wikilink-sanitize`, `includes`, `relaxed-tables`, `per-cell-alignment`, `alpha-lists`, `mixed-lists`, `sup-sub`, `strikethrough`, `autolink`, `transforms`, `unsafe`, `tables`, `footnotes`, `smart`, `math`, `ids`, `header-anchors`, `embed-images`, `image-captions`, `link-citations`, `show-tooltips`, `suppress-bibliography`, `suppress-index`, `group-index-by-letter`, `obfuscate-emails`, `pretty`, `standalone`, `hardbreaks`, `plugins`, `emoji-autocorrect`, `code-line-numbers`, `highlight-language-only`, `markdown-in-html`

**Supported string options:**
`bibliography`, `csl`, `title`, `style` (or `css`),
`id-format`, `base-dir`, `mode`, `wikilink-space`,
`wikilink-extension`

**Example YAML front matter:**
```
---
indices: false
wikilinks: true
bibliography: references.bib
title: My Research Paper
pretty: true
standalone: true
---

```

**Example MultiMarkdown metadata:**
```
indices: false
wikilinks: true
bibliography: references.bib
title: My Research Paper

```

When processing multiple files with `apex *.md`, each file can use its own configuration via metadata. You can also use `--meta-file` to specify a shared configuration file that applies to all processed files.

**Note:** If `mode` is specified in metadata, it resets all
options to that mode's defaults before applying other
metadata options.

# FEATURES

Apex supports a wide range of Markdown extensions:

- **Tables**: GFM-style tables with alignment
- **Strikethrough**: GFM-style `~~text~~` (controlled by **--strikethrough**)
- **Footnotes**: Reference-style footnotes
- **Math**: Inline (`$...$`) and display (`$$...$$`) math

  with LaTeX

- **Wiki Links**: `[[Page]]`, `[[Page|Display]]`,

  `[[Page#Section]]`

- **Critic Markup**: All 5 types ({++add++}, {--del--},

  {~~sub~~}, {==mark==}, {>>comment<<})

- **Smart Typography**: Smart quotes, dashes, ellipsis
- **Definition Lists**: MultiMarkdown-style definition lists
- **Task Lists**: GFM-style task lists
- **Metadata**: YAML front matter, MultiMarkdown metadata,

  Pandoc title blocks

- **Metadata Transforms**: Transform metadata values with

  `[%key:transform]` syntax (case conversion, string
  manipulation, regex replacement, date formatting, etc.)

- **Metadata Control of Options**: Control command-line

  options via metadata for per-document configuration

- **Header IDs**: Automatic or manual header IDs with

  multiple format options

- **Relaxed Tables**: Support for tables without separator
rows (Kramdown-style). Enabled by default in unified and
Kramdown modes.

- **Per-Cell Alignment**: Support for alignment markers using
colons at the start and/or end of table cells. Enabled by default
in unified mode only.

  rows (Kramdown-style)

- **Inline Tables from CSV/TSV**: Convert inline CSV/TSV

  text to tables using ```table fences or `<!--TABLE-->`
  markers

**Superscript/Subscript**: MultiMarkdown-style superscript (`^text`) and subscript (`~text~` within words) syntax. Subscript uses paired tildes within word boundaries (e.g., `H~2~O`), while tildes at word boundaries create underline

- **Image Embedding**: Embed local images as base64 data

  URLs with `--embed-images` flag

**Inline Attribute Lists (IAL)**: Kramdown-style syntax for adding HTML attributes (IDs, classes, key-value pairs) to block-level and inline elements. Supports Attribute List Definitions (ALDs) for reusable attribute sets. Available in kramdown and unified modes. See [Inline Attribute Lists](https://github.com/ttscoff/apex/wiki/Inline-Attribute-Lists) for complete documentation

# SEE ALSO

**apex-config**(5), **apex-plugins**(7), **pandoc**(1), **markdown**(7)

For complete documentation, see the [Apex Wiki](https://github.com/ttscoff/apex/wiki).

Project homepage: <https://github.com/ApexMarkdown/apex>

# AUTHOR

Brett Terpstra

# COPYRIGHT

Copyright (c) 2025 Brett Terpstra. Licensed under MIT
License.

# BUGS

Report bugs at <https://github.com/ttscoff/apex/issues>.
