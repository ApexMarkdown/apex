# Wiki Links Implementation Issue

## Problem

Wiki links (`[[Page]]`) are not being detected because:

1. The `[` character is already registered by cmark-gfm's standard link parser
2. When `[` is encountered, the standard link parser gets priority
3. Our extension's match function is either not called, or is called after the standard link parser has already consumed the `[`

## Attempted Solutions

### Attempt 1: Register as inline extension
- Added `[` as special character
- Set match function
- **Result**: Not called or called too late

### Attempt 2: Check for `[[` in match function
- Added check for double `[[` at start of match function
- **Result**: Still not working - match function may not be getting called at all

## Root Cause

cmark-gfm processes inline elements in a specific order:
1. Built-in syntax (links, emphasis) is handled first
2. Extension syntax is handled after
3. Since `[` triggers link processing, standard markdown link syntax wins

## Possible Solutions

### Option A: Preprocessing
Convert `[[...]]` to temporary markers before parsing, then convert back in HTML

```
[[Page]] → ⟦⟦Page⟧⟧  (preprocessing)
Parse with cmark-gfm
⟦⟦Page⟧⟧ → <a href="Page">Page</a>  (postprocessing)
```

### Option B: Postprocessing
Let markdown parse normally, then walk AST and convert text nodes containing `[[...]]`

### Option C: Custom inline parser hook (if available)
Hook into inline parsing at a lower level to intercept `[[` before link parsing

### Option D: Different syntax
Use a character that doesn't conflict: `{{Page}}` or `<<Page>>`

## Recommendation

**Use postprocessing (Option B)** - Most reliable approach:
1. Parse markdown normally with cmark-gfm
2. Walk the AST looking for TEXT nodes
3. Find `[[...]]` patterns in text
4. Split text node and insert LINK nodes

This is how Marked currently handles wiki links and it works reliably.

## Implementation Plan

1. Remove the inline match approach
2. Add `apex_process_wiki_links(cmark_node *document)` function
3. Walk AST, find TEXT nodes
4. Use regex or manual parsing to find `[[...]]`
5. Split text, insert link nodes
6. Call this after `cmark_parser_finish()` but before rendering

