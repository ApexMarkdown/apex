# Apex Node.js npm Package Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship `@apexmarkdown/apex` as a Node.js native binding that converts
Markdown with Apex modes and serializable options on macOS/Linux without
system C libraries.

**Architecture:** Create a sibling `apex-node` repository that vendors Apex as
a submodule, builds a CMake Node-API addon linking bundled cmark-gfm and
libyaml, and distributes platform `.node` binaries as optional npm packages
selected by a small JavaScript loader.

**Tech Stack:** Node.js 20+, Node-API (`node-addon-api`), CMake, C++17,
vendored Apex/cmark-gfm/libyaml, npm optionalDependencies, GitHub Actions.

## Global Constraints

- Package name is `@apexmarkdown/apex`.
- Repository path is `/Users/ttscoff/Desktop/Code/apex-node`.
- Supported platforms for v1: macOS and Linux, x64 and arm64.
- Supported Node.js: 20 or newer.
- Bundle Apex, cmark-gfm, and libyaml; do not require system libraries.
- Enable `APEX_HAVE_LIBYAML=1`.
- Public API is synchronous `convert(markdown, options?)` and `version`.
- Option names are camelCase only.
- Expose every serializable user-facing `apex_options` field.
- Exclude callbacks and TOC output pointers.
- Unknown names or wrong types throw `TypeError`.
- No WASM, Windows, async workers, or structured TOC API in v1.
- Work and commits happen in `apex-node`, not in the Apex core tree
  (except this plan/spec already stored under Apex docs).

---

## File Structure

All paths below are relative to `/Users/ttscoff/Desktop/Code/apex-node`
unless noted.

- Create `package.json`: public package metadata, scripts, optionalDeps.
- Create `README.md`, `LICENSE`, `.gitignore`, `.npmrc`.
- Create `vendor/apex`: Apex git submodule.
- Create `binding/CMakeLists.txt`: build Apex static lib + Node addon.
- Create `binding/src/addon.cpp`: Node-API `convert` / `version`.
- Create `binding/src/options.h` / `options.cpp`: JS object → `apex_options`.
- Create `lib/index.js`: platform loader and re-exports.
- Create `lib/index.d.ts`: TypeScript declarations.
- Create `lib/load-native.js`: resolve platform optional package.
- Create `test/*.test.js`: conversion, options, loader, errors.
- Create `scripts/build.js`: cmake-js wrapper for local builds.
- Create `scripts/package-prebuild.js`: emit a platform package tarball dir.
- Create `platforms/package.template.json`: template for platform packages.
- Create `.github/workflows/ci.yml` and `publish-prebuilds.yml`.

### Task 1: Scaffold Repository and Failing Smoke Tests

**Files:**

- Create: `/Users/ttscoff/Desktop/Code/apex-node/` (new git repo)
- Create: `package.json`
- Create: `.gitignore`
- Create: `LICENSE`
- Create: `test/convert.test.js`
- Create: `test/options-validation.test.js`
- Create: `vendor/apex` (submodule)

**Interfaces:**

- Produces: npm package skeleton named `@apexmarkdown/apex` at version
  `0.1.0`
- Produces: tests that import `{ convert, version }` from `../lib/index.js`
- Consumes: Apex submodule at GitHub `ApexMarkdown/apex`

- [ ] **Step 1: Create the repository and submodule**

```bash
mkdir -p /Users/ttscoff/Desktop/Code/apex-node
cd /Users/ttscoff/Desktop/Code/apex-node
git init
git submodule add https://github.com/ApexMarkdown/apex.git vendor/apex
git submodule update --init --recursive
```

Expected: `vendor/apex/include/apex/apex.h` exists and
`vendor/apex/vendor/cmark-gfm` / `vendor/apex/vendor/libyaml` are populated.

- [ ] **Step 2: Add package metadata and ignore rules**

Create `.gitignore`:

```gitignore
node_modules/
build/
prebuilds/
*.node
.DS_Store
coverage/
npm-debug.log*
```

Create `package.json`:

```json
{
  "name": "@apexmarkdown/apex",
  "version": "0.1.0",
  "description": "Node.js bindings for the Apex unified Markdown processor",
  "license": "MIT",
  "type": "commonjs",
  "main": "./lib/index.js",
  "types": "./lib/index.d.ts",
  "exports": {
    ".": {
      "types": "./lib/index.d.ts",
      "require": "./lib/index.js",
      "import": "./lib/index.js"
    }
  },
  "engines": {
    "node": ">=20"
  },
  "files": [
    "lib/",
    "binding/",
    "scripts/",
    "README.md",
    "LICENSE"
  ],
  "scripts": {
    "build": "node scripts/build.js",
    "test": "node --test test/**/*.test.js",
    "package:prebuild": "node scripts/package-prebuild.js"
  },
  "optionalDependencies": {
    "@apexmarkdown/apex-darwin-arm64": "0.1.0",
    "@apexmarkdown/apex-darwin-x64": "0.1.0",
    "@apexmarkdown/apex-linux-arm64": "0.1.0",
    "@apexmarkdown/apex-linux-x64": "0.1.0"
  },
  "devDependencies": {
    "cmake-js": "^7.3.0",
    "node-addon-api": "^8.3.0"
  }
}
```

Create `LICENSE` as MIT, copyright Brett Terpstra.

- [ ] **Step 3: Write failing smoke tests**

Create `test/convert.test.js`:

```js
const { describe, it } = require("node:test");
const assert = require("node:assert/strict");
const { convert, version } = require("../lib/index.js");

describe("convert", () => {
  it("renders a heading by default", () => {
    const html = convert("# Hello");
    assert.match(html, /<h1[^>]*>Hello<\/h1>/);
  });

  it("honors gfm mode for strikethrough", () => {
    const html = convert("~~x~~", { mode: "gfm" });
    assert.match(html, /<del>x<\/del>/);
  });

  it("returns empty string for empty input", () => {
    assert.equal(convert(""), "");
  });

  it("preserves utf8", () => {
    const html = convert("café 漢字");
    assert.match(html, /café 漢字/);
  });
});

describe("version", () => {
  it("exposes a non-empty version string", () => {
    assert.equal(typeof version, "string");
    assert.notEqual(version.length, 0);
  });
});
```

Create `test/options-validation.test.js`:

```js
const { describe, it } = require("node:test");
const assert = require("node:assert/strict");
const { convert } = require("../lib/index.js");

describe("option validation", () => {
  it("rejects unknown option names", () => {
    assert.throws(
      () => convert("# x", { notARealOption: true }),
      (err) => err instanceof TypeError && /Unknown option/.test(err.message)
    );
  });

  it("rejects wrong boolean types", () => {
    assert.throws(
      () => convert("# x", { unsafe: "yes" }),
      (err) => err instanceof TypeError && /unsafe/.test(err.message)
    );
  });

  it("rejects invalid mode strings", () => {
    assert.throws(
      () => convert("# x", { mode: "pandoc" }),
      (err) => err instanceof TypeError && /mode/.test(err.message)
    );
  });

  it("rejects non-string markdown", () => {
    assert.throws(
      () => convert(42),
      (err) => err instanceof TypeError
    );
  });
});
```

- [ ] **Step 4: Install deps and confirm tests fail**

```bash
cd /Users/ttscoff/Desktop/Code/apex-node
npm install
npm test
```

Expected: FAIL because `lib/index.js` does not exist / native addon missing.

- [ ] **Step 5: Commit scaffold**

```bash
cd /Users/ttscoff/Desktop/Code/apex-node
git add .
git commit -m "$(cat <<'EOF'
Scaffold Apex Node package and failing smoke tests.

@new **Node package scaffold** for @apexmarkdown/apex with Apex submodule and smoke tests.

EOF
)"
```

### Task 2: CMake Build and Minimal Native Convert

**Files:**

- Create: `binding/CMakeLists.txt`
- Create: `binding/src/addon.cpp`
- Create: `binding/src/options.h`
- Create: `binding/src/options.cpp` (stub that only accepts empty/undefined options)
- Create: `scripts/build.js`
- Create: `lib/load-native.js`
- Create: `lib/index.js`
- Create: `lib/index.d.ts` (minimal)

**Interfaces:**

- Produces native exports:
  - `convert(markdown: string, options?: object): string`
  - `version: string`
- Produces:
  `bool apex_node_options_from_js(Napi::Env, Napi::Value, apex_options *,
  ApexNodeOptionStorage *, std::string *error)`
- Produces loader:
  `function loadNative(): { convert, version }`

- [ ] **Step 1: Add CMake that builds bundled Apex and the addon**

Create `binding/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.15)
project(apex_node LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(APEX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/apex")

set(CMARK_TESTS OFF CACHE BOOL "" FORCE)
set(CMARK_SHARED OFF CACHE BOOL "" FORCE)
set(CMARK_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory("${APEX_ROOT}/vendor/cmark-gfm" "${CMAKE_BINARY_DIR}/cmark-gfm")

set(YAML_SRC
  ${APEX_ROOT}/vendor/libyaml/src/api.c
  ${APEX_ROOT}/vendor/libyaml/src/dumper.c
  ${APEX_ROOT}/vendor/libyaml/src/emitter.c
  ${APEX_ROOT}/vendor/libyaml/src/loader.c
  ${APEX_ROOT}/vendor/libyaml/src/parser.c
  ${APEX_ROOT}/vendor/libyaml/src/reader.c
  ${APEX_ROOT}/vendor/libyaml/src/scanner.c
  ${APEX_ROOT}/vendor/libyaml/src/writer.c
)
add_library(apex_yaml STATIC ${YAML_SRC})
target_include_directories(apex_yaml PUBLIC
  ${APEX_ROOT}/vendor/libyaml/include
  ${APEX_ROOT}/vendor/libyaml/src
)
target_compile_definitions(apex_yaml PUBLIC
  YAML_VERSION_MAJOR=0
  YAML_VERSION_MINOR=2
  YAML_VERSION_PATCH=5
  YAML_VERSION_STRING="0.2.5"
  HAVE_CONFIG_H=0
)

set(APEX_LIB_SOURCES
  ${APEX_ROOT}/src/apex.c
  ${APEX_ROOT}/src/ast_json.c
  ${APEX_ROOT}/src/ast_markdown.c
  ${APEX_ROOT}/src/ast_terminal.c
  ${APEX_ROOT}/src/ast_man.c
  ${APEX_ROOT}/src/filters_ast.c
  ${APEX_ROOT}/src/plugins_env.c
  ${APEX_ROOT}/src/plugins.c
  ${APEX_ROOT}/src/plugins_remote.c
  ${APEX_ROOT}/src/plugin_catalog.c
  ${APEX_ROOT}/src/html_renderer.c
  ${APEX_ROOT}/src/extensions/metadata.c
  ${APEX_ROOT}/src/extensions/wiki_links.c
  ${APEX_ROOT}/src/extensions/math.c
  ${APEX_ROOT}/src/extensions/critic.c
  ${APEX_ROOT}/src/extensions/callouts.c
  ${APEX_ROOT}/src/extensions/raw_content.c
  ${APEX_ROOT}/src/extensions/code_fence_attrs.c
  ${APEX_ROOT}/src/extensions/quarto_diagrams.c
  ${APEX_ROOT}/src/extensions/quarto_shortcodes.c
  ${APEX_ROOT}/src/extensions/quarto_polish.c
  ${APEX_ROOT}/src/extensions/quarto_lists.c
  ${APEX_ROOT}/src/extensions/includes.c
  ${APEX_ROOT}/src/extensions/inline_tables.c
  ${APEX_ROOT}/src/extensions/toc.c
  ${APEX_ROOT}/src/extensions/abbreviations.c
  ${APEX_ROOT}/src/extensions/emoji.c
  ${APEX_ROOT}/src/extensions/special_markers.c
  ${APEX_ROOT}/src/extensions/ial.c
  ${APEX_ROOT}/src/extensions/bear_image_attrs.c
  ${APEX_ROOT}/src/extensions/definition_list.c
  ${APEX_ROOT}/src/extensions/advanced_footnotes.c
  ${APEX_ROOT}/src/extensions/advanced_tables.c
  ${APEX_ROOT}/src/extensions/html_markdown.c
  ${APEX_ROOT}/src/extensions/fenced_divs.c
  ${APEX_ROOT}/src/extensions/table_html_postprocess.c
  ${APEX_ROOT}/src/extensions/inline_footnotes.c
  ${APEX_ROOT}/src/extensions/highlight.c
  ${APEX_ROOT}/src/extensions/insert.c
  ${APEX_ROOT}/src/extensions/sup_sub.c
  ${APEX_ROOT}/src/extensions/header_ids.c
  ${APEX_ROOT}/src/extensions/relaxed_tables.c
  ${APEX_ROOT}/src/extensions/grid_tables.c
  ${APEX_ROOT}/src/extensions/citations.c
  ${APEX_ROOT}/src/extensions/index.c
  ${APEX_ROOT}/src/extensions/syntax_highlight.c
  ${APEX_ROOT}/src/pretty_html.c
  ${APEX_ROOT}/src/buffer.c
  ${APEX_ROOT}/src/parser.c
  ${APEX_ROOT}/src/renderer.c
  ${APEX_ROOT}/src/utf8.c
)

add_library(apex_static STATIC ${APEX_LIB_SOURCES})
target_include_directories(apex_static PUBLIC
  ${APEX_ROOT}/include
  ${APEX_ROOT}/include/apex
  ${APEX_ROOT}/vendor/cmark-gfm/src
  ${APEX_ROOT}/vendor/cmark-gfm/extensions
  ${APEX_ROOT}/vendor/libyaml/include
)
target_compile_definitions(apex_static PUBLIC APEX_HAVE_LIBYAML=1)
target_link_libraries(apex_static PUBLIC
  libcmark-gfm-extensions_static
  libcmark-gfm_static
  apex_yaml
)

add_library(${PROJECT_NAME} SHARED
  src/addon.cpp
  src/options.cpp
)
set_target_properties(${PROJECT_NAME} PROPERTIES
  PREFIX ""
  SUFFIX ".node"
)
target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_JS_INC}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(${PROJECT_NAME} PRIVATE apex_static ${CMAKE_JS_LIB})
target_compile_definitions(${PROJECT_NAME} PRIVATE NAPI_VERSION=8)
```

If cmake-js does not inject `CMAKE_JS_INC` / `CMAKE_JS_LIB` correctly on the
host, pass them explicitly from `scripts/build.js` using
`require("node-addon-api").include` and Node’s `process.execPath` include
paths.

- [ ] **Step 2: Implement options stub and addon entry points**

Create `binding/src/options.h`:

```cpp
#pragma once

#include <napi.h>
#include <string>
#include <vector>
#include "apex/apex.h"

struct ApexNodeOptionStorage {
  std::vector<std::string> owned_strings;
  std::vector<std::vector<char>> owned_cstrings;
  std::vector<const char *> stylesheet_ptrs;
  std::vector<char *> bibliography_ptrs;
  std::vector<char *> script_tag_ptrs;
  std::vector<const char *> ast_filter_ptrs;
};

bool apex_node_options_from_js(Napi::Env env,
                               Napi::Value value,
                               apex_options *out,
                               ApexNodeOptionStorage *storage,
                               std::string *error);
```

Create `binding/src/options.cpp` stub:

```cpp
#include "options.h"

bool apex_node_options_from_js(Napi::Env env,
                               Napi::Value value,
                               apex_options *out,
                               ApexNodeOptionStorage *storage,
                               std::string *error) {
  (void)storage;
  if (value.IsUndefined() || value.IsNull()) {
    *out = apex_options_default();
    return true;
  }
  if (!value.IsObject()) {
    *error = "options must be an object";
    return false;
  }
  Napi::Object obj = value.As<Napi::Object>();
  Napi::Array names = obj.GetPropertyNames();
  if (names.Length() > 0) {
    *error = "Unknown option: " +
             names.Get(uint32_t(0)).As<Napi::String>().Utf8Value();
    return false;
  }
  *out = apex_options_default();
  return true;
}
```

Create `binding/src/addon.cpp`:

```cpp
#include <napi.h>
#include <string>
#include "apex/apex.h"
#include "options.h"

static Napi::Value Convert(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "markdown must be a string").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string markdown = info[0].As<Napi::String>().Utf8Value();
  ApexNodeOptionStorage storage;
  apex_options options;
  std::string error;
  Napi::Value opts_val = info.Length() >= 2 ? info[1] : env.Undefined();
  if (!apex_node_options_from_js(env, opts_val, &options, &storage, &error)) {
    Napi::TypeError::New(env, error).ThrowAsJavaScriptException();
    return env.Null();
  }

  char *html = apex_markdown_to_html(markdown.c_str(), markdown.size(), &options);
  if (!html) {
    Napi::Error::New(env, "Apex conversion failed").ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::String result = Napi::String::New(env, html);
  apex_free_string(html);
  return result;
}

static Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("convert", Napi::Function::New(env, Convert));
  exports.Set("version", Napi::String::New(env, apex_version_string()));
  return exports;
}

NODE_API_MODULE(apex_node, Init)
```

- [ ] **Step 3: Add build script and development loader**

Create `scripts/build.js`:

```js
const { spawnSync } = require("node:child_process");
const path = require("node:path");

const root = path.join(__dirname, "..");
const result = spawnSync(
  "npx",
  ["cmake-js", "compile", "--directory", "binding"],
  { cwd: root, stdio: "inherit", shell: process.platform === "win32" }
);
process.exit(result.status ?? 1);
```

Create `lib/load-native.js`:

```js
const fs = require("node:fs");
const path = require("node:path");
const os = require("node:os");

function platformPackageName() {
  const platform = process.platform;
  const arch = process.arch;
  if (platform === "darwin" && arch === "arm64") return "@apexmarkdown/apex-darwin-arm64";
  if (platform === "darwin" && arch === "x64") return "@apexmarkdown/apex-darwin-x64";
  if (platform === "linux" && arch === "arm64") return "@apexmarkdown/apex-linux-arm64";
  if (platform === "linux" && arch === "x64") return "@apexmarkdown/apex-linux-x64";
  return null;
}

function loadNative() {
  const candidates = [];
  const pkg = platformPackageName();
  if (pkg) {
    try {
      const resolved = require.resolve(`${pkg}/apex.node`);
      candidates.push(resolved);
    } catch (_) {
      // optional dependency may be absent during local development
    }
  }

  candidates.push(
    path.join(__dirname, "..", "build", "Release", "apex_node.node"),
    path.join(__dirname, "..", "binding", "build", "Release", "apex_node.node")
  );

  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return require(candidate);
    }
  }

  const detected = `${process.platform}-${process.arch}`;
  throw new Error(
    `Apex native addon is unavailable for ${detected}. ` +
      `Install a supported optional dependency or run npm run build.`
  );
}

module.exports = { loadNative, platformPackageName };
```

Create `lib/index.js`:

```js
const { loadNative } = require("./load-native.js");
const native = loadNative();

module.exports = {
  convert: native.convert,
  version: native.version
};
```

Create minimal `lib/index.d.ts`:

```ts
export interface ApexOptions {
  [key: string]: unknown;
}

export declare function convert(markdown: string, options?: ApexOptions): string;
export declare const version: string;
```

- [ ] **Step 4: Build and run convert smoke tests**

```bash
cd /Users/ttscoff/Desktop/Code/apex-node
npm run build
node --test test/convert.test.js
```

Expected: PASS for convert/version. Validation tests still FAIL for
non-empty options because the stub rejects every property.

- [ ] **Step 5: Commit native convert path**

```bash
git add binding scripts lib
git commit -m "$(cat <<'EOF'
Add CMake Node-API convert binding for Apex.

@new **Native convert()** binding that bundles Apex, cmark-gfm, and libyaml.

EOF
)"
```

### Task 3: Complete Serializable Options Mapper

**Files:**

- Modify: `binding/src/options.cpp`
- Modify: `binding/src/options.h` if storage helpers need expansion
- Modify: `lib/index.d.ts`
- Modify: `test/options-validation.test.js`
- Create: `test/options-behavior.test.js`

**Interfaces:**

- Produces complete `apex_node_options_from_js(...)` covering every
  serializable field listed below
- Produces typed `ApexOptions` in `lib/index.d.ts`

Serializable camelCase fields to implement (exclude callbacks / TOC outs):

**Booleans:**
`enablePlugins`, `allowExternalPluginDetection`, `enableTables`,
`enableFootnotes`, `enableDefinitionLists`, `enableSmartTypography`,
`enableMath`, `enableCriticMarkup`, `enableWikiLinks`, `enableTaskLists`,
`enableAttributes`, `enableCallouts`, `enablePyCallouts`,
`enableQuartoCallouts`, `enableQuartoExtensions`, `enableQuartoRaw`,
`enableQuartoExampleLists`, `enableQuartoLineBlocks`,
`enableQuartoRomanLists`, `enableQuartoCodeAttrs`, `enableQuartoDiagrams`,
`enableQuartoShortcodes`, `enableQuartoStrictLists`, `enableQuartoXrefs`,
`enableMarkedExtensions`, `enableDivs`, `enableSpans`, `enableGridTables`,
`stripMetadata`, `enableMetadataVariables`, `enableMetadataTransforms`,
`enableFileIncludes`, `unsafe`, `validateUtf8`, `githubPreLang`,
`standalone`, `pretty`, `xhtml`, `strictXhtml`, `paginate`,
`paginateSymbols`, `terminalInlineImages`, `hardbreaks`, `nobreaks`,
`generateHeaderIds`, `headerAnchors`, `relaxedTables`, `perCellAlignment`,
`allowMixedListMarkers`, `allowAlphaLists`, `enableSupSub`,
`enableStrikethrough`, `enableAutolink`, `obfuscateEmails`, `embedImages`,
`enableImageCaptions`, `titleCaptionsOnly`, `enableCitations`,
`suppressBibliography`, `linkCitations`, `showTooltips`, `enableIndices`,
`enableMmarkIndexSyntax`, `enableTextindexSyntax`,
`enableLeanpubIndexSyntax`, `suppressIndex`, `groupIndexByLetter`,
`wikilinkSanitize`, `embedStylesheet`, `enableAria`,
`enableEmojiAutocorrect`, `codeLineNumbers`, `highlightLanguageOnly`,
`enableWidont`, `codeIsPoetry`, `enableMarkdownInHtml`,
`randomFootnoteIds`, `enableHashtags`, `styleHashtags`, `proofreaderMode`,
`hrPageBreak`, `titleFromH1`, `pageBreakBeforeFootnotes`,
`astFilterStrict`

**Integers:**
`maxIncludeDepth`, `terminalWidth`, `terminalImageWidth`, `tocMin`, `tocMax`

**Strings:**
`baseDirectory`, `documentTitle`, `themeName`, `cslFile`, `nocite`,
`wikilinkExtension`, `codeHighlighter`, `codeHighlightTheme`,
`inputFilePath`

**Enums (string unions):**
`mode`, `outputFormat`, `criticMode`, `idFormat`, `captionPosition`,
`wikilinkSpace`

**String arrays:**
`stylesheetPaths` → `stylesheet_paths` + `stylesheet_count`
`bibliographyFiles` → NULL-terminated `bibliography_files`
`scriptTags` → NULL-terminated `script_tags`
`astFilterCommands` → `ast_filter_commands` + `ast_filter_count`

- [ ] **Step 1: Expand validation/behavior tests first**

Append to `test/options-validation.test.js`:

```js
  it("accepts camelCase mode aliases only as documented strings", () => {
    assert.doesNotThrow(() => convert("# x", { mode: "unified" }));
    assert.doesNotThrow(() => convert("# x", { mode: "commonmark" }));
    assert.doesNotThrow(() => convert("# x", { mode: "quarto" }));
  });

  it("rejects snake_case option names", () => {
    assert.throws(
      () => convert("# x", { enable_tables: true }),
      TypeError
    );
  });
```

Create `test/options-behavior.test.js`:

```js
const { describe, it } = require("node:test");
const assert = require("node:assert/strict");
const { convert } = require("../lib/index.js");

describe("option behavior", () => {
  it("disables header ids when generateHeaderIds is false", () => {
    const withIds = convert("# Hello", { mode: "gfm", generateHeaderIds: true });
    const withoutIds = convert("# Hello", { mode: "gfm", generateHeaderIds: false });
    assert.match(withIds, /id=/);
    assert.doesNotMatch(withoutIds, /id=/);
  });

  it("applies criticMode accept", () => {
    const html = convert("{++added++}", {
      mode: "unified",
      enableCriticMarkup: true,
      criticMode: "accept"
    });
    assert.match(html, /added/);
    assert.doesNotMatch(html, /ins/);
  });

  it("accepts stylesheetPaths array without throwing", () => {
    const html = convert("# Hi", {
      standalone: true,
      documentTitle: "Doc",
      stylesheetPaths: []
    });
    assert.match(html, /<html/i);
  });
});
```

Run:

```bash
npm test
```

Expected: FAIL on unknown-option / unimplemented mapper behavior.

- [ ] **Step 2: Implement table-driven options parsing**

Replace `binding/src/options.cpp` with a complete mapper:

1. Read optional `mode` first.
2. Initialize with `apex_options_for_mode(mode)` (or
   `apex_options_default()` when mode omitted).
3. Iterate `Object.getOwnPropertyNames`.
4. For each name, look up a handler in a `std::unordered_map`.
5. Unknown names set `error = "Unknown option: " + name` and return false.
6. Copy strings into `storage->owned_strings` and point `const char *`
   fields at `c_str()`.
7. For arrays, fill pointer vectors and NULL-terminate
   `bibliography_files` / `script_tags`; set counts for stylesheets and
   AST filters.

Enum maps:

```cpp
// mode
{"commonmark", APEX_MODE_COMMONMARK},
{"gfm", APEX_MODE_GFM},
{"multimarkdown", APEX_MODE_MULTIMARKDOWN},
{"kramdown", APEX_MODE_KRAMDOWN},
{"unified", APEX_MODE_UNIFIED},
{"quarto", APEX_MODE_QUARTO}

// outputFormat
{"html", APEX_OUTPUT_HTML},
{"json", APEX_OUTPUT_JSON},
{"jsonFiltered", APEX_OUTPUT_JSON_FILTERED},
{"markdown", APEX_OUTPUT_MARKDOWN},
{"mmd", APEX_OUTPUT_MMD},
{"commonmark", APEX_OUTPUT_COMMONMARK},
{"kramdown", APEX_OUTPUT_KRAMDOWN},
{"gfm", APEX_OUTPUT_GFM},
{"terminal", APEX_OUTPUT_TERMINAL},
{"terminal256", APEX_OUTPUT_TERMINAL256},
{"man", APEX_OUTPUT_MAN},
{"manHtml", APEX_OUTPUT_MAN_HTML},
{"toc", APEX_OUTPUT_TOC}

// criticMode
{"markup", 0}, {"accept", 1}, {"reject", 2}

// idFormat
{"gfm", 0}, {"mmd", 1}

// captionPosition
{"above", 0}, {"below", 1}

// wikilinkSpace
{"dash", 0}, {"none", 1}, {"underscore", 2}, {"space", 3}
```

Leave these C fields unset / NULL always:

`plugin_register`, `progress_callback`, `progress_user_data`,
`cmark_init`, `cmark_done`, `cmark_user_data`,
`toc_entries_out`, `toc_entries_count_out`.

- [ ] **Step 3: Write complete TypeScript declarations**

Replace `lib/index.d.ts` with an explicit `ApexOptions` interface listing
every supported property and the enum string unions from the design spec.
Do not include an index signature that would allow unknown keys.

- [ ] **Step 4: Rebuild and verify all option tests**

```bash
npm run build
npm test
```

Expected: all tests PASS.

- [ ] **Step 5: Commit options mapper**

```bash
git add binding/src/options.cpp binding/src/options.h lib/index.d.ts test
git commit -m "$(cat <<'EOF'
Map complete camelCase Apex options in the Node binding.

@new **Complete ApexOptions mapping** for serializable camelCase Node options with strict validation.

EOF
)"
```

### Task 4: Platform Package Packaging

**Files:**

- Create: `scripts/package-prebuild.js`
- Create: `platforms/package.template.json`
- Create: `test/load-native.test.js`
- Modify: `package.json` scripts if needed

**Interfaces:**

- Produces directory
  `prebuilds/@apexmarkdown/apex-<platform>-<arch>/` containing:
  - `package.json`
  - `apex.node`
  - `README.md`
- Produces npm package name matching `platformPackageName()`

- [ ] **Step 1: Add loader unit test for unsupported platforms**

Create `test/load-native.test.js`:

```js
const { describe, it } = require("node:test");
const assert = require("node:assert/strict");
const { platformPackageName } = require("../lib/load-native.js");

describe("platformPackageName", () => {
  it("returns a scoped package for the current host when supported", () => {
    const name = platformPackageName();
    if (process.platform === "darwin" || process.platform === "linux") {
      if (process.arch === "arm64" || process.arch === "x64") {
        assert.match(name, /^@apexmarkdown\/apex-(darwin|linux)-(arm64|x64)$/);
        return;
      }
    }
    assert.equal(name, null);
  });
});
```

- [ ] **Step 2: Implement package-prebuild script**

Create `platforms/package.template.json`:

```json
{
  "name": "@apexmarkdown/apex-PLATFORM_ARCH",
  "version": "PACKAGE_VERSION",
  "description": "Apex native addon for PLATFORM_ARCH",
  "os": ["OS"],
  "cpu": ["CPU"],
  "main": "apex.node",
  "files": ["apex.node", "README.md"],
  "license": "MIT",
  "engines": {
    "node": ">=20"
  }
}
```

Create `scripts/package-prebuild.js`:

```js
const fs = require("node:fs");
const path = require("node:path");
const { platformPackageName } = require("../lib/load-native.js");

const root = path.join(__dirname, "..");
const pkg = require("../package.json");
const name = platformPackageName();
if (!name) {
  console.error(`Unsupported platform: ${process.platform}-${process.arch}`);
  process.exit(1);
}

const addonCandidates = [
  path.join(root, "build", "Release", "apex_node.node"),
  path.join(root, "binding", "build", "Release", "apex_node.node")
];
const addon = addonCandidates.find((p) => fs.existsSync(p));
if (!addon) {
  console.error("Built addon not found; run npm run build first");
  process.exit(1);
}

const outDir = path.join(root, "prebuilds", name);
fs.rmSync(outDir, { recursive: true, force: true });
fs.mkdirSync(outDir, { recursive: true });

const os = process.platform;
const cpu = process.arch;
const rendered = fs
  .readFileSync(path.join(root, "platforms", "package.template.json"), "utf8")
  .replaceAll("PLATFORM_ARCH", `${os === "darwin" ? "darwin" : "linux"}-${cpu}`)
  .replaceAll("PACKAGE_VERSION", pkg.version)
  .replaceAll("\"OS\"", `"${os}"`)
  .replaceAll("\"CPU\"", `"${cpu}"`);

fs.writeFileSync(path.join(outDir, "package.json"), rendered);
fs.copyFileSync(addon, path.join(outDir, "apex.node"));
fs.writeFileSync(
  path.join(outDir, "README.md"),
  `# ${name}\n\nNative Apex addon for ${os}-${cpu}.\n`
);
console.log(`Wrote ${outDir}`);
```

- [ ] **Step 3: Build and package on the current host**

```bash
npm run build
npm run package:prebuild
node -e "console.log(require('./prebuilds/'+require('./lib/load-native').platformPackageName()+'/package.json').name)"
npm test
```

Expected: platform directory exists; tests still PASS.

- [ ] **Step 4: Commit packaging scripts**

```bash
git add scripts/package-prebuild.js platforms test/load-native.test.js
git commit -m "$(cat <<'EOF'
Add platform prebuild packaging for optional native deps.

@new **Platform prebuild packaging** for @apexmarkdown/apex optional native packages.

EOF
)"
```

### Task 5: CI Workflows and README

**Files:**

- Create: `.github/workflows/ci.yml`
- Create: `.github/workflows/publish-prebuilds.yml`
- Create: `README.md`
- Modify: `package.json` repository/homepage fields

**Interfaces:**

- CI matrix builds and tests:
  - `macos-14` (arm64)
  - `macos-13` or `macos-15-intel` equivalent for x64 if available; otherwise
    document cross-compile/skip with an explicit matrix entry that uses
    `cmake` targeting `x86_64-apple-darwin` only if reliable
  - `ubuntu-24.04` (x64)
  - `ubuntu-24.04-arm` (arm64) when available on the org plan
- Publish workflow uploads platform package artifacts on tag `v*`

Practical v1 matrix if ARM Linux runners are unavailable:

```yaml
strategy:
  matrix:
    include:
      - os: macos-14
        package: "@apexmarkdown/apex-darwin-arm64"
      - os: macos-13
        package: "@apexmarkdown/apex-darwin-x64"
      - os: ubuntu-24.04
        package: "@apexmarkdown/apex-linux-x64"
```

If Linux arm64 CI is unavailable at implementation time, keep the optional
dependency declared, build it manually when needed, and note the gap in
README. Do not silently drop the package name from `package.json`.

- [ ] **Step 1: Write CI workflow**

Create `.github/workflows/ci.yml` that:

1. Checks out with `submodules: recursive`
2. Sets up Node 20
3. Runs `npm ci`
4. Runs `npm run build`
5. Runs `npm test`
6. Runs `npm run package:prebuild`
7. Uploads `prebuilds/**` as artifacts named by matrix package

- [ ] **Step 2: Write publish workflow**

Create `.github/workflows/publish-prebuilds.yml` triggered on `push: tags: ['v*']`:

1. Build matrix packages
2. `npm publish` each platform package from its `prebuilds/...` directory
3. Publish `@apexmarkdown/apex` from the repo root

Require `NPM_TOKEN` repository secret. Use
`npm publish --access public` for scoped packages.

- [ ] **Step 3: Write README**

Create `README.md` covering:

```md
# @apexmarkdown/apex

Node.js bindings for [Apex](https://github.com/ApexMarkdown/apex).

## Install

npm install @apexmarkdown/apex

## Usage

const { convert, version } = require("@apexmarkdown/apex");
const html = convert("# Hello", { mode: "gfm", generateHeaderIds: true });

## Supported platforms

- macOS arm64 / x64
- Linux arm64 / x64
- Node.js 20+

No system cmark-gfm or libyaml install is required.

## Options

camelCase options matching Apex's serializable apex_options fields.
Unknown options throw TypeError.
```

Also document local development:

```bash
git submodule update --init --recursive
npm install
npm run build
npm test
```

- [ ] **Step 4: Fill package repository metadata**

In `package.json` set:

```json
"repository": {
  "type": "git",
  "url": "git+https://github.com/ApexMarkdown/apex-node.git"
},
"homepage": "https://github.com/ApexMarkdown/apex-node",
"bugs": {
  "url": "https://github.com/ApexMarkdown/apex-node/issues"
}
```

- [ ] **Step 5: Final verification on the development machine**

```bash
npm run build
npm test
npm run package:prebuild
node -e "const {convert,version}=require('.'); console.log(version); console.log(convert('**hi**'))"
```

Expected: version prints, HTML contains `<strong>hi</strong>`.

- [ ] **Step 6: Commit docs and CI**

```bash
git add .github README.md package.json
git commit -m "$(cat <<'EOF'
Add CI, publish workflow, and Node package README.

@new **Node package CI and docs** for building and publishing @apexmarkdown/apex prebuilds.

EOF
)"
```

- [ ] **Step 7: Create GitHub repository and push**

```bash
cd /Users/ttscoff/Desktop/Code/apex-node
gh repo create ApexMarkdown/apex-node --public --source=. --remote=origin --push
```

Only run this step when the user asks to publish the repository.

---

## Spec Coverage Checklist

| Spec requirement | Task |
| --- | --- |
| Sibling `apex-node` repo + submodule | Task 1 |
| `@apexmarkdown/apex` package identity | Task 1 |
| CMake + bundled cmark-gfm/libyaml | Task 2 |
| Sync `convert` / `version` | Task 2 |
| Complete serializable camelCase options | Task 3 |
| Strict TypeError validation | Task 3 |
| Exclude callbacks / TOC outs | Task 3 |
| Platform optional packages | Task 4 |
| macOS/Linux x64/arm64 distribution | Tasks 4–5 |
| Tests listed in design | Tasks 1, 3, 4 |
| No WASM / Windows / async / TOC API | Global Constraints |

## Placeholder / Ambiguity Scan

- No TBD/TODO left in task steps.
- Linux arm64 CI availability is handled with an explicit fallback note,
  not a silent omission of the package name.
- cmake-js include/lib injection has an explicit fallback instruction.
- Repository creation/push is gated on explicit user request.
