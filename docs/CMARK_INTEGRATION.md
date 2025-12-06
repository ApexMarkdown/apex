# cmark-gfm Integration Plan

## Architecture Analysis

### cmark-gfm Structure

**Core Library** (`src/`):

- `parser.h/blocks.c/inlines.c` - Parsing Markdown to AST
- `node.c/node.h` - AST node structure and manipulation
- `render.c/render.h` - Rendering framework
- `html.c` - HTML rendering
- `commonmark.c` - CommonMark output
- `buffer.c/buffer.h` - Dynamic string buffer
- `utf8.c/utf8.h` - UTF-8 utilities
- `arena.c` - Memory arena allocator

**Extensions** (`extensions/`):

- `autolink.c` - Autolink URLs
- `strikethrough.c` - `~~strikethrough~~`
- `table.c` - GFM tables
- `tasklist.c` - `- [ ]` task lists
- `tagfilter.c` - HTML tag filtering

**Extension System**:

- `syntax_extension.c/h` - Extension registration
- `cmark-gfm-core-extensions.h` - Core extension API
- Each extension can:
  - Match block/inline syntax
  - Create custom nodes
  - Render custom nodes

### Key APIs

```c
// Simple API
char *cmark_markdown_to_html(const char *text, size_t len, int options);

// Parser API
cmark_parser *cmark_parser_new(int options);
void cmark_parser_feed(cmark_parser *parser, const char *buffer, size_t len);
cmark_node *cmark_parser_finish(cmark_parser *parser);
void cmark_parser_free(cmark_parser *parser);

// Node API
cmark_node_type cmark_node_get_type(cmark_node *node);
cmark_node *cmark_node_first_child(cmark_node *node);
cmark_node *cmark_node_next(cmark_node *node);

// Rendering API
char *cmark_render_html(cmark_node *root, int options, cmark_llist *extensions);
char *cmark_render_commonmark(cmark_node *root, int options, int width);

// Extension API
void cmark_parser_attach_syntax_extension(cmark_parser *parser, cmark_syntax_extension *ext);
cmark_syntax_extension *cmark_find_syntax_extension(const char *name);
```

### Extension System Design

Extensions can:
1. Register pattern matchers for blocks/inlines
2. Create custom node types
3. Provide custom rendering
4. Hook into parsing at various stages

## Integration Strategy

### Phase 1: Vendor cmark-gfm

1. Keep cmark-gfm in `vendor/cmark-gfm/`
2. Build it as part of Apex's CMake
3. Link statically into libapex

### Phase 2: Wrapper Layer

Create an Apex → cmark bridge:

```c
// apex/src/cmark_bridge.c
#include "apex/apex.h"
#include "cmark-gfm.h"
#include "cmark-gfm-core-extensions.h"

apex_node *apex_parse_cmark(const char *markdown, size_t len, const apex_options *opts) {
    // Create cmark parser
    int cmark_opts = apex_to_cmark_options(opts);
    cmark_parser *parser = cmark_parser_new(cmark_opts);

    // Attach GFM extensions if enabled
    if (opts->enable_tables) {
        cmark_parser_attach_syntax_extension(parser,
            cmark_find_syntax_extension("table"));
    }
    if (opts->enable_task_lists) {
        cmark_parser_attach_syntax_extension(parser,
            cmark_find_syntax_extension("tasklist"));
    }
    // ... more extensions

    // Parse
    cmark_parser_feed(parser, markdown, len);
    cmark_node *cmark_root = cmark_parser_finish(parser);

    // Convert cmark AST to Apex AST
    apex_node *apex_root = convert_cmark_to_apex(cmark_root);

    // Clean up
    cmark_node_free(cmark_root);
    cmark_parser_free(parser);

    return apex_root;
}
```

### Phase 3: Custom Extensions

Create Apex-specific extensions:

1. **Metadata Extension** (`apex_metadata_ext.c`)
   - Parse YAML/MMD/Pandoc metadata
   - Store in custom node type

2. **Definition List Extension** (`apex_deflist_ext.c`)
   - Parse `:` definition syntax
   - Create DL/DT/DD nodes

3. **Callout Extension** (`apex_callout_ext.c`)
   - Parse `> [!NOTE]` syntax
   - Create callout nodes with types

4. **Critic Markup Extension** (`apex_critic_ext.c`)
   - Parse `{++addition++}` etc.
   - Create critic markup nodes

5. **Math Extension** (`apex_math_ext.c`)
   - Parse `$math$` and `$$math$$`
   - Create math nodes

6. **Wiki Link Extension** (`apex_wikilink_ext.c`)
   - Parse `[[link]]`
   - Create wiki link nodes

7. **Marked Special Extension** (`apex_marked_ext.c`)
   - Parse `<!--TOC-->`, `<!--BREAK-->`, etc.
   - Handle file includes

### Phase 4: AST Conversion

Two options:

**Option A: Convert to Apex AST**
- cmark nodes → Apex nodes
- Pros: Full control, can extend freely
- Cons: Conversion overhead

**Option B: Use cmark AST directly**
- Wrap cmark_node as apex_node
- Pros: Zero-copy, faster
- Cons: Tied to cmark structure

Recommendation: **Option A initially**, can optimize to B later.

### Phase 5: Rendering

```c
char *apex_render_html(apex_node *root, const apex_options *opts) {
    // If using pure cmark features, use cmark renderer
    if (no_custom_extensions_used(root)) {
        cmark_node *cmark_root = convert_apex_to_cmark(root);
        char *html = cmark_render_html(cmark_root, opts->cmark_options, extensions);
        cmark_node_free(cmark_root);
        return html;
    }

    // Otherwise use Apex's renderer with custom node support
    return apex_render_html_custom(root, opts);
}
```

## Implementation Steps

1. ✅ **Clone cmark-gfm** - Done
2. **Study APIs** - In progress
3. **Integrate CMake** - Add cmark as subdirectory
4. **Create bridge layer** - Wrap cmark API
5. **Test basic integration** - CommonMark tests
6. **Add GFM extensions** - Tables, task lists, etc.
7. **Create custom extensions** - Metadata, callouts, etc.
8. **AST conversion** - Bidirectional cmark ↔ Apex
9. **Enhanced rendering** - Support custom nodes

## Benefits of This Approach

✅ **Immediate Results**: Full CommonMark + GFM support right away
✅ **Battle-tested**: cmark is used by GitHub, proven quality
✅ **Extensible**: Can add Apex features incrementally
✅ **Maintainable**: cmark updates can be merged upstream
✅ **Fast**: C implementation, no performance penalty

## Timeline

- **Week 1**: CMake integration + bridge layer
- **Week 2**: Basic tests passing, GFM working
- **Week 3**: Custom extensions (metadata, def lists)
- **Week 4**: More extensions (callouts, critic, math)
- **Week 5**: Polish and testing

**Target**: Full MVP in 4-5 weeks

