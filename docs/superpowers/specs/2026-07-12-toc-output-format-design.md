# Design: `-t toc` Output Format

**Date:** 2026-07-12  
**Status:** Approved for implementation

## Summary

Add a `toc` output format that emits nothing but a Markdown unordered list of document headings, each linked to its header ID. Depth defaults to `#`–`###`, overridable via a single `--toc-min-max` option that also seeds HTML TOC markers when they do not specify their own range.

## Goals

- `-t toc` / `--to toc` outputs only a nested Markdown TOC.
- Header IDs match `--id-format` (and manual IDs from `{#id}` / IAL).
- Default depth is levels 1–3.
- One CLI flag `--toc-min-max=MIN,MAX` controls defaults for both `-t toc` and marker-based HTML TOC.
- Marker-specific min/max still overrides the defaults for that marker.
- Headings with `.no_toc` are excluded (same as HTML TOC).

## Non-Goals

- Changing the HTML TOC markup shape (`<nav class="toc">` / nested `<ul>`).
- Adding separate `--toc-min` / `--toc-max` flags.
- Emitting HTML, JSON, or terminal-styled TOC from `-t toc`.
- Auto-inserting a TOC into documents that have no marker when using `-t html`.

## Behavior

### Output shape

Given:

```markdown
# Introduction
## Getting Started
### Installation
#### Too Deep
## Configuration
# API
{: .no_toc}
```

With defaults (`1,3`), `-t toc` produces:

```markdown
- [Introduction](#introduction)
  - [Getting Started](#getting-started)
    - [Installation](#installation)
  - [Configuration](#configuration)
```

Notes:

- `#### Too Deep` omitted (above max).
- `# API` omitted (`.no_toc`).
- Links use `[Title](#id)` where `id` is generated with the active `--id-format`, or taken from a manual heading ID when present.
- Nesting is relative to the filtered min level: the shallowest included level is flush (`- `); each deeper included level adds two spaces of indent.
- No document body, no `<nav>`, no metadata dump.
- If there are no matching headings, output is empty (no list items). Prefer a single trailing newline only if that matches existing Apex empty-output conventions for other formats; otherwise empty string is fine.

### Depth: `--toc-min-max`

- CLI: `--toc-min-max=MIN,MAX` and `--toc-min-max MIN,MAX`.
- Defaults: `toc_min = 1`, `toc_max = 3`.
- Validation: `1 ≤ min ≤ max ≤ 6`. Invalid values are a CLI error.
- Optional metadata key with the same semantics: `toc-min-max: 2,4`.
- Applies to:
  - `-t toc` always (no marker involved).
  - HTML TOC markers (`{{TOC}}`, `<!--TOC-->`, `{:toc}`, etc.) when the marker does **not** specify its own min/max.
- Marker override examples that win over options defaults:
  - `{{TOC:2-5}}`
  - `<!--TOC max5 min2-->`
  - `{:toc max=5}` (and existing IAL/marker variants already supported)

**Default change for HTML markers:** markers with no explicit range currently behave as 1–6. After this change they use option defaults (1–3 unless `--toc-min-max` / metadata overrides). Explicit marker ranges are unchanged. This is intentional and should be noted in docs/changelog as a behavior change for unspecified markers.

## Architecture

### Approach

Early-exit output format + shared heading collection (Approach A).

1. Parse markdown and run the same AST prep used for other non-HTML early exits (filters, IAL, manual header IDs).
2. When `output_format == APEX_OUTPUT_TOC`, collect headings and emit Markdown TOC; return without HTML/terminal/man rendering.
3. Refactor `src/extensions/toc.c` so HTML marker TOC and Markdown TOC share collection / ID assignment / `no_toc` filtering.

### Options / API surface

| Piece | Change |
|-------|--------|
| `apex_output_format_t` | Add `APEX_OUTPUT_TOC` |
| `apex_options` | Add `toc_min`, `toc_max` (defaults 1, 3) |
| CLI (`cli/main.c`) | `-t toc`, `--toc-min-max`; help text |
| Metadata | Parse `toc-min-max` into options when present |
| `toc.h` / `toc.c` | Public Markdown TOC helper; thread default min/max into `apex_process_toc` |
| Tests | Defaults, flag, `no_toc`, id-format, marker override, empty doc |
| Man page / wiki | Document `-t toc` and `--toc-min-max` |

### Suggested public helpers

```c
/* Emit nested Markdown list of headings in [min,max], excluding .no_toc */
char *apex_generate_toc_markdown(cmark_node *document, int id_format,
                                 int min_level, int max_level);

/* Existing HTML marker processor, extended to accept defaults when
 * the marker does not specify min/max */
char *apex_process_toc(const char *html, cmark_node *document, int id_format,
                       int default_min, int default_max);
```

Exact signature may vary slightly for ABI/call-site convenience, but callers in `src/apex.c` must pass `options->toc_min` / `options->toc_max`.

### Nesting rules

Match current HTML TOC nesting semantics relative to the filtered range:

- Skip headings outside `[min_level, max_level]`.
- Indent depth for an included heading is `(level - min_level)` (two spaces per step).
- Closing / nesting of list levels follows the same open/close pattern as `generate_toc_html` so Markdown and HTML stay structurally aligned.

### ID consistency

- Prefer an existing manual ID on the heading (`user_data` / IAL / `{#id}`) when present.
- Otherwise call `apex_generate_header_id(text, id_format)`.
- `-t toc` must use the same ID rules as HTML header ID injection so links resolve when the same document is rendered to HTML with the same `--id-format`.

## Pipeline placement

In `apex_markdown_to_html()` (name retained historically), add an early return beside the markdown / JSON-filtered / terminal / man branches—after IAL and manual header ID processing, before HTML render.

Do not run HTML postprocessors (pretty HTML, image captions, etc.) for `-t toc`.

## Testing

Add coverage for:

1. Default `-t toc` includes levels 1–3 only.
2. `--toc-min-max=2,4` filters correctly.
3. `.no_toc` headings excluded.
4. `--id-format gfm|mmd|kramdown` changes href slugs as expected.
5. Manual `{#custom-id}` appears in the link target.
6. HTML marker with no range uses option defaults (1,3 or overridden flag).
7. HTML marker with explicit range overrides option defaults.
8. Document with no headings / no matching levels yields empty (or newline-only) output.
9. Invalid `--toc-min-max` rejected by CLI.

## Documentation

- Man page (`man/apex.1.md`): add `toc` to `-t` list; document `--toc-min-max`.
- Wiki: Command-Line-Options (and any TOC-related page) updated similarly.
- Changelog via usual `@new` / `@changed` labels when committing the feature.

## Open decisions (resolved)

| Question | Decision |
|----------|----------|
| Default depth | `#`–`###` (1,3) |
| Depth flag shape | Single `--toc-min-max=MIN,MAX` |
| Flag scope | Both `-t toc` and HTML markers; marker can override |
| `.no_toc` | Excluded |
| Implementation approach | Early-exit format + shared TOC builder |
