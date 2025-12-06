# Changelog

All notable changes to Apex will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-12-04

### Added

**Core Features:**
- Initial release of Apex unified Markdown processor
- Based on cmark-gfm for CommonMark + GFM support
- Support for 5 processor modes: CommonMark, GFM, MultiMarkdown, Kramdown, Unified

**Metadata:**
- YAML front matter parsing
- MultiMarkdown metadata format
- Pandoc title block format
- Metadata variable replacement with `[%key]` syntax

**Extended Syntax:**
- Wiki-style links: `[[Page]]`, `[[Page|Display]]`, `[[Page#Section]]`
- Math support: `$inline$` and `$$display$$` with LaTeX
- Critic Markup: All 5 types ({++add++}, {--del--}, {~~sub~~}, {==mark==}, {>>comment<<})
- GFM tables, strikethrough, task lists, autolinks
- Reference-style footnotes
- Smart typography (smart quotes, dashes, ellipsis)

**Build System:**
- CMake build system for cross-platform support
- Builds shared library, static library, CLI binary, and macOS framework
- Clean compilation on macOS with Apple Clang

**CLI Tool:**
- `apex` command-line binary
- Support for all processor modes via `--mode` flag
- Stdin/stdout support for Unix pipes
- Comprehensive help and version information

**Integration:**
- Objective-C wrapper (`NSString+Apex`) for Marked integration
- macOS framework with proper exports
- Detailed integration documentation and examples

**Testing:**
- Automated test suite with 31 tests
- 90% pass rate across all feature areas
- Manual testing validated

**Documentation:**
- Comprehensive user guide
- Complete API reference
- Architecture documentation
- Integration guides
- Code examples

### Known Issues

- Critic Markup substitutions have edge cases with certain inputs
- Definition lists not yet implemented
- Kramdown attributes not yet implemented
- Inline footnotes not yet implemented

### Performance

- Small documents (< 10KB): < 10ms
- Medium documents (< 100KB): < 100ms
- Large documents (< 1MB): < 1s

### Credits

- Based on [cmark-gfm](https://github.com/github/cmark-gfm) by GitHub
- Developed for [Marked](https://marked2app.com) by Brett Terpstra

## [Unreleased]

### Planned

- Definition lists (Kramdown/PHP Extra style)
- Kramdown attributes `{: #id .class}`
- Inline footnotes `^[text]`
- Enhanced table features (column spans, captions)
- Callouts (Bear/Obsidian/Xcode style)
- TOC markers
- File includes
- Page breaks
- Leanpub syntax support

[0.1.0]: https://github.com/ttscoff/apex/releases/tag/v0.1.0

