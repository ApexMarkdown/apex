# Changelog

All notable changes to Apex will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.19] - 2025-12-09

#### CHANGED

- HTML comments now replaced with "raw HTML omitted" in CommonMark and GFM modes by default
- Added enable_sup_sub flag to apex_options struct
- Updated mode configurations to enable sup/sub in appropriate modes
- Added sup_sub.c to CMakeLists.txt build configuration
- Removed unused variables to resolve compiler warnings
- Tag filter (GFM security feature) now only applies in GFM mode, not Unified mode, allowing raw HTML and autolinks in Unified mode as intended.
- Autolink extension registration now respects the enable_autolink option flag.

#### NEW

- Added MultiMarkdown-style superscript (^text^) and subscript (~text~) syntax support
- Added --[no-]sup-sub command-line option to enable/disable superscript/subscript
- Superscript/subscript enabled by default in unified and MultiMarkdown modes
- Created sup_sub extension (sup_sub.c and sup_sub.h) for processing ^ and ~ syntax
- Added --[no-]unsafe command-line option to control raw HTML handling
- Added test_sup_sub() function with 13 tests covering superscript and
- Added test_mixed_lists() function with 10 tests covering mixed list
- Added test_unsafe_mode() function with 8 tests covering raw HTML
- Added preprocessing for angle-bracket autolinks (<http://...>) to convert them to explicit markdown links, ensuring they work correctly with custom rendering paths.
- Added --[no-]autolink CLI option to control automatic linking of URLs and email addresses. Autolinking is enabled by default in GFM, MultiMarkdown, Kramdown, and unified modes, and disabled in CommonMark mode.
- Added enable_autolink field to apex_options structure to control autolink behavior programmatically.
- Added underline syntax support: ~text~ now renders as <u>text</u> when there's a closing ~ with no space before it.

#### IMPROVED

- Test suite now includes 36 additional tests, increasing total test
- Autolink preprocessing now skips processing inside code spans (`...`) and code blocks (```...```), preventing URLs from being converted to links when they appear in code examples.
- Metadata replacement retains HTML edge-case handling and properly cleans up intermediate buffers.

#### FIXED

- Unified mode now correctly enables mixed list markers and alpha lists by default when no --mode is specified
- ^ marker now properly separates lists by creating a paragraph break instead of just blank lines
- Empty paragraphs created by ^ marker are now removed from final HTML output
- Superscript and subscript processing now skips ^ and ~ characters
- Superscript processing now skips ^ when part of footnote reference
- Subscript processing now skips ~ when part of critic markup patterns
- Setext headers are no longer broken when followed by highlight syntax (==text==). Highlight processing now stops at line breaks to prevent interference with header parsing.
- Metadata parser no longer incorrectly treats URLs and angle-bracket autolinks as metadata. Lines containing < or URLs (http://, https://, mailto:) are now skipped during metadata extraction.
- Superscript/subscript processor now correctly differentiates between ~text~ (underline), ~word (subscript), and ~~text~~ (strikethrough). Double-tilde sequences are skipped so strikethrough extension can handle them.
- Subscript processing now stops at sentence terminators (. , ; : ! ?) instead of including them in the subscript content.
- Metadata variable replacement now runs before autolinking so [%key] values containing URLs are turned into links when autolinking is enabled.
- MMD metadata parsing no longer incorrectly rejects entries with URL values; only URL-like keys or '<' characters in keys are rejected, allowing "URL: https://example.com" as valid metadata.
- Headers starting with `#` are now correctly recognized instead of being treated as autolinks. The autolink preprocessor now skips `#` at the start of a line when followed by whitespace.
- Math processor now validates that `\(...\)` sequences contain actual math content (letters, numbers, or operators) before processing them. This prevents false positives like `\(%\)` from being treated as math when they only contain special characters.



## [0.1.18] - 2025-12-06

### Fixed
- GitHub Actions workflow now properly builds separate Linux x86_64 and ARM64 binaries

## [0.1.17] - 2025-12-06

### Fixed
- Relaxed tables now disabled by default for CommonMark, GFM, and MultiMarkdown modes (only enabled for Kramdown and Unified modes)
- Header ID extraction no longer incorrectly parses metadata variables like `[%title]` as MMD-style header IDs
- Tables with alignment/separator rows now correctly generate `<thead>` even when relaxed table mode is enabled
- Relaxed tables preprocessor preserves input newline behavior in output
- Memory management bug in IAL preprocessing removed unnecessary free call

## [0.1.16] - 2025-12-06

### Fixed
- IAL (Inline Attribute List) markers appearing immediately after content without a blank line are now correctly parsed
- Added `apex_preprocess_ial()` function to ensure Kramdown-style IAL syntax works correctly with cmark-gfm parser

## [0.1.15] - 2025-12-06

### Fixed
- Homebrew formula updated with correct version and commit hash

## [0.1.10] - 2025-12-06

### Changed
- License changed to MIT

### Added
- Homebrew formula update scripts

## [0.1.9] - 2025-12-06

### Fixed
- Shell syntax in Linux checksum step for GitHub Actions

## [0.1.8] - 2025-12-06

### Fixed
- Link order for Linux static builds

## [0.1.7] - 2025-12-06

### Fixed
- Added write permissions for GitHub releases

## [0.1.6] - 2025-12-06

### Fixed
- `.gitignore` pattern fixed to properly include apex headers (was incorrectly matching `include/apex/`)

## [0.1.5] - 2025-12-06

### Changed
- Added verbose build output for CI debugging

## [0.1.4] - 2025-12-06

### Fixed
- CMake build rules updated

## [0.1.3] - 2025-12-06

### Fixed
- CMake policy version for cmark-gfm compatibility

## [0.1.2] - 2025-12-06

### Fixed
- GitHub Actions workflow fixes

## [0.1.1] - 2025-12-04

### Added
- CMake setup documentation

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

[0.1.18]: https://github.com/ttscoff/apex/releases/tag/v0.1.18
[0.1.17]: https://github.com/ttscoff/apex/releases/tag/v0.1.17
[0.1.16]: https://github.com/ttscoff/apex/releases/tag/v0.1.16
[0.1.15]: https://github.com/ttscoff/apex/releases/tag/v0.1.15
[0.1.10]: https://github.com/ttscoff/apex/releases/tag/v0.1.10
[0.1.9]: https://github.com/ttscoff/apex/releases/tag/v0.1.9
[0.1.8]: https://github.com/ttscoff/apex/releases/tag/v0.1.8
[0.1.7]: https://github.com/ttscoff/apex/releases/tag/v0.1.7
[0.1.6]: https://github.com/ttscoff/apex/releases/tag/v0.1.6
[0.1.5]: https://github.com/ttscoff/apex/releases/tag/v0.1.5
[0.1.4]: https://github.com/ttscoff/apex/releases/tag/v0.1.4
[0.1.3]: https://github.com/ttscoff/apex/releases/tag/v0.1.3
[0.1.2]: https://github.com/ttscoff/apex/releases/tag/v0.1.2
[0.1.1]: https://github.com/ttscoff/apex/releases/tag/v0.1.1
[0.1.0]: https://github.com/ttscoff/apex/releases/tag/v0.1.0

