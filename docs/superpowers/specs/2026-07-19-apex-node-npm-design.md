# Apex Node.js npm Package

## Goal

Publish `@apexmarkdown/apex` as a Node.js native binding for the Apex C
library, so JavaScript projects can convert Markdown to HTML with Apex's
modes and options without installing a separate CLI or system libraries.

## Repository and layout

Create a new repository at `/Users/ttscoff/Desktop/Code/apex-node`, parallel
to `apex-ruby`. It is not part of the Apex core tree.

Initial layout:

```text
apex-node/
  package.json                 # public package @apexmarkdown/apex
  README.md
  LICENSE
  vendor/apex/                 # pinned Apex git submodule
  binding/
    CMakeLists.txt
    src/addon.cpp              # Node-API entry points
    src/options.cpp            # JS object -> apex_options
    src/options.h
  lib/
    index.js                   # ESM/CJS loader
    index.d.ts                 # TypeScript declarations
  test/
  scripts/
    build.js
    package-prebuild.js
  .github/workflows/
    ci.yml
    publish-prebuilds.yml
```

Platform binaries ship as optional dependencies:

- `@apexmarkdown/apex-darwin-arm64`
- `@apexmarkdown/apex-darwin-x64`
- `@apexmarkdown/apex-linux-arm64`
- `@apexmarkdown/apex-linux-x64`

Each platform package contains a single `.node` addon for that target.

## Architecture

CMake builds:

1. Vendored `cmark-gfm` from `vendor/apex/vendor/cmark-gfm`
2. Vendored `libyaml` from `vendor/apex/vendor/libyaml`
3. The Apex C library from `vendor/apex`
4. A thin C++ Node-API adapter that links against Apex

The addon exposes a small native surface. Ergonomics live in JavaScript and
TypeScript declarations.

Distribution:

- Consumers install `@apexmarkdown/apex`.
- npm installs the matching optional platform package when available.
- The loader resolves the platform `.node` file at runtime.
- Unsupported OS/arch combinations throw a clear error naming the detected
  platform. Local compilation is not required for supported targets.

Supported Node.js: 20 or newer.

Supported first-release platforms: macOS and Linux, x64 and arm64.

## JavaScript API

ESM and CommonJS are both supported, with bundled TypeScript declarations:

```ts
import { convert, version } from "@apexmarkdown/apex";

const html = convert("# Hello", {
  mode: "gfm",
  unsafe: false,
  generateHeaderIds: true
});
```

Primary surface:

```ts
convert(markdown: string, options?: ApexOptions): string
version: string
```

`convert()` is synchronous. Async worker-thread conversion is out of scope
for v1.

## Options mapping

`ApexOptions` exposes every user-facing serializable field from
`apex_options` in camelCase. Defaults come from
`apex_options_for_mode(mode)` (or unified defaults when `mode` is omitted).
Explicit JavaScript properties overlay those defaults.

Enum fields use string unions rather than C integers:

| JS property | Values |
| --- | --- |
| `mode` | `"commonmark"`, `"gfm"`, `"multimarkdown"`, `"kramdown"`, `"unified"`, `"quarto"` |
| `outputFormat` | `"html"`, `"json"`, `"jsonFiltered"`, `"markdown"`, `"mmd"`, `"commonmark"`, `"kramdown"`, `"gfm"`, `"terminal"`, `"terminal256"`, `"man"`, `"manHtml"`, `"toc"` |
| `criticMode` | `"markup"`, `"accept"`, `"reject"` |
| `idFormat` | `"gfm"`, `"mmd"` |
| `captionPosition` | `"above"`, `"below"` |
| `wikilinkSpace` | `"dash"`, `"none"`, `"underscore"`, `"space"` |

Array fields accept `string[]`:

- `stylesheetPaths`
- `bibliographyFiles`
- `scriptTags`
- `astFilterCommands`

Path-like and other string options remain caller-owned for the duration of
the `convert()` call. The binding copies them into conversion-scoped native
storage before invoking Apex.

### Excluded native-only surfaces

These `apex_options` members are not exposed in JavaScript:

- `plugin_register` / `pluginRegister`
- `progress_callback` / `progressCallback`
- `progress_user_data` / `progressUserData`
- `cmark_init` / `cmarkInit`
- `cmark_done` / `cmarkDone`
- `cmark_user_data` / `cmarkUserData`
- `toc_entries_out` / `tocEntriesOut`
- `toc_entries_count_out` / `tocEntriesCountOut`

Structured TOC extraction via `apex_markdown_to_toc_entries()` may be added
later as a separate API; it is not part of v1.

## Data flow

For each `convert()` call:

1. Reject non-string Markdown with `TypeError`.
2. Validate every supplied option name and type.
3. Initialize `apex_options` from the selected mode.
4. Overlay explicit JavaScript options onto that struct.
5. Copy strings and arrays into conversion-scoped native storage.
6. Call `apex_markdown_to_html(markdown, len, &options)`.
7. Copy the returned UTF-8 buffer into a JavaScript string.
8. Free the Apex buffer with `apex_free_string()`.
9. Release conversion-scoped native storage.

`version` returns `apex_version_string()` as a static JavaScript string.

## Error handling

| Condition | Behavior |
| --- | --- |
| Unknown option name | `TypeError` |
| Wrong option type | `TypeError` |
| Invalid enum string | `TypeError` |
| Non-string Markdown | `TypeError` |
| Native allocation / conversion failure | `Error` |
| Unsupported OS / architecture | `Error` naming the detected platform |

No silent ignoring of unknown or invalid options.

## Build and packaging

- CMake is the build system for the native addon and bundled dependencies.
- `APEX_HAVE_LIBYAML` is enabled so YAML metadata matches the SwiftPM build.
- Prebuilds are produced in CI for the four supported platforms.
- The public package lists platform packages as optional dependencies and
  contains no install-time compile step for supported targets.
- Source builds remain available to maintainers via CMake; they are not the
  primary consumer path.

## Testing

Automated tests cover:

- Default conversion and each mode
- Representative boolean, string, enum, and array options
- Unknown option and wrong-type rejection
- Empty input and UTF-8 content
- `version` string presence
- Loader failure messaging when the platform package is missing

CI builds and tests each supported platform, then packages the `.node`
artifact for that target.

## Out of scope for v1

- WebAssembly / browser builds
- Windows prebuilds
- Asynchronous / worker-thread conversion APIs
- Structured TOC JavaScript API
- JavaScript bridges for C callbacks (`progress`, `cmark_init` / `cmark_done`,
  plugin registration)
- Requiring system `cmark-gfm` or `libyaml`

## Success criteria

- `npm install @apexmarkdown/apex` works on macOS and Linux x64/arm64 with
  no local C toolchain.
- `convert("# Hi")` returns HTML containing a heading.
- Mode and option overrides affect output as they do in Apex core.
- Invalid options throw rather than being ignored.
- TypeScript consumers get accurate declarations for the public API.
