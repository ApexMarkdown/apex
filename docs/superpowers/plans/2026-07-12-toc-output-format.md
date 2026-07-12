# TOC Output Format Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `-t toc` that emits a Markdown unordered list of heading links, plus `--toc-min-max` defaults shared with HTML TOC markers.

**Architecture:** Early-exit `APEX_OUTPUT_TOC` after AST prep (same point as markdown/json-filtered). Refactor `src/extensions/toc.c` to share heading collection/ID assignment between HTML marker TOC and a new Markdown TOC emitter. Option defaults (`toc_min`/`toc_max`, default 1,3) seed both paths; marker-specified ranges still override.

**Tech Stack:** C (Apex + cmark-gfm), existing `tests/test_output.c` suite runner, man page / wiki docs.

**Spec:** `docs/superpowers/specs/2026-07-12-toc-output-format-design.md`

---

## File map

| File | Responsibility |
|------|----------------|
| `include/apex/apex.h` | `APEX_OUTPUT_TOC`; `toc_min` / `toc_max` on `apex_options` |
| `src/apex.c` | Defaults; early-exit for TOC; pass defaults into `apex_process_toc` |
| `src/extensions/toc.h` | Public Markdown helper; update `apex_process_toc` signature |
| `src/extensions/toc.c` | Prefer manual IDs; Markdown emitter; default min/max when marker omits range |
| `cli/main.c` | `-t toc`, `--toc-min-max`, help, option mask |
| `src/extensions/metadata.c` | `toc-min-max` metadata key |
| `tests/test_output.c` | `-t toc` + default-depth HTML TOC tests |
| `tests/test_metadata.c` | Metadata `toc-min-max` parsing |
| `man/apex.1.md` (+ regenerate `man/apex.1`) | Document format + flag |
| `apex.wiki/Command-Line-Options.md`, `Syntax.md`, `Header-IDs.md` as needed | User docs |

---

### Task 1: Options + enum surface

**Files:**
- Modify: `include/apex/apex.h`
- Modify: `src/apex.c` (`apex_options_default`, and any `apex_options_for_mode` copies that re-init options)

- [ ] **Step 1: Add enum value and option fields**

In `include/apex/apex.h`, extend the output format enum:

```c
    APEX_OUTPUT_MAN = 10,           /* roff (man page source) */
    APEX_OUTPUT_MAN_HTML = 11,      /* styled HTML man page */
    APEX_OUTPUT_TOC = 12            /* Markdown TOC list only */
```

Near the header ID fields on `apex_options` (after `id_format`), add:

```c
    /* TOC depth defaults (used by -t toc and by HTML TOC markers without an explicit range) */
    int toc_min;  /* Inclusive min heading level (default 1) */
    int toc_max;  /* Inclusive max heading level (default 3) */
```

- [ ] **Step 2: Initialize defaults in `apex_options_default`**

In `src/apex.c` inside `apex_options_default()`, after `opts.id_format = 0;`:

```c
    opts.toc_min = 1;
    opts.toc_max = 3;
```

If `apex_options_for_mode()` rebuilds options via `apex_options_default()`, no extra work. If any mode path zeroes the struct without going through default, set `toc_min`/`toc_max` there too.

- [ ] **Step 3: Commit**

```bash
git add include/apex/apex.h src/apex.c
git commit -m "Add APEX_OUTPUT_TOC and toc_min/toc_max options"
```

---

### Task 2: Prefer manual heading IDs in TOC collection

**Files:**
- Modify: `src/extensions/toc.c`
- Test: `tests/test_output.c`

- [ ] **Step 1: Write failing test for manual ID in HTML TOC**

Append inside `test_toc()` in `tests/test_output.c` (before suite_end):

```c
    /* Manual {#id} must appear in TOC hrefs */
    const char *manual_id_toc =
        "# Custom {#my-custom}\n\n{{TOC}}\n\n## Other";
    html = apex_markdown_to_html(manual_id_toc, strlen(manual_id_toc), &opts);
    assert_contains(html, "href=\"#my-custom\"", "TOC uses manual heading ID");
    apex_free_string(html);
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /Users/ttscoff/Desktop/Code/apex/build && make apex_test_runner -j4 && ./apex_test_runner toc 2>&1 | rg -i "manual|FAIL|passed|failed"
```

Expected: failure on "TOC uses manual heading ID" (current code always regenerates from text).

- [ ] **Step 3: Assign IDs from `user_data` when present**

In `src/extensions/toc.c`, replace the ID assignment loop in `apex_process_toc` (and share later with Markdown) with a helper:

```c
static char *heading_id_from_attrs_or_text(const char *attrs, const char *text,
                                           apex_id_format_t id_format) {
    if (attrs) {
        const char *id_attr = strstr(attrs, "id=\"");
        if (id_attr) {
            const char *id_start = id_attr + 4;
            const char *id_end = strchr(id_start, '"');
            if (id_end && id_end > id_start) {
                size_t id_len = (size_t)(id_end - id_start);
                char *id = malloc(id_len + 1);
                if (id) {
                    memcpy(id, id_start, id_len);
                    id[id_len] = '\0';
                    return id;
                }
            }
        }
    }
    return apex_generate_header_id(text, id_format);
}
```

Update `collect_headers` to store attrs (or look them up again). Simplest: keep attrs on the node and, when assigning IDs, pass `cmark_node` — better: store `char *attrs_copy` is unnecessary if we assign IDs during collection:

In `collect_headers`, after creating `item`:

```c
const char *attrs = (const char *)cmark_node_get_user_data(node);
/* no_toc check already uses attrs */
item->id = NULL; /* assigned in assign_header_ids */
```

Add:

```c
static void assign_header_ids(header_item *headers, cmark_node *document,
                              int id_format) {
    /* Walk headers list in document order; for each, find matching heading
     * OR store attrs on header_item during collect. Prefer storing attrs: */
}
```

Preferred approach — extend `header_item`:

```c
typedef struct header_item {
    int level;
    char *text;
    char *id;
    char *attrs; /* borrowed? NO — strdup user_data or extract id immediately */
    struct header_item *next;
} header_item;
```

Simplest reliable approach: extract ID during `collect_headers` once you know `id_format` — change signature to:

```c
static header_item *collect_headers(cmark_node *node, header_item **tail,
                                    int id_format);
```

and inside, after `normalize_toc_text`:

```c
item->id = heading_id_from_attrs_or_text(attrs, item->text,
                                         (apex_id_format_t)id_format);
```

Update `free_headers` (already frees `id`). Remove the separate ID loop in `apex_process_toc`.

- [ ] **Step 4: Run test to verify it passes**

```bash
cd /Users/ttscoff/Desktop/Code/apex/build && make apex_test_runner -j4 && ./apex_test_runner toc 2>&1 | rg -i "manual|FAIL|Results"
```

Expected: "TOC uses manual heading ID" passes; suite overall green enough to proceed (note: Task 3 will change bare-marker max depth).

- [ ] **Step 5: Commit**

```bash
git add src/extensions/toc.c tests/test_output.c
git commit -m "Prefer manual heading IDs when building TOC links"
```

---

### Task 3: Thread default min/max into HTML TOC markers

**Files:**
- Modify: `src/extensions/toc.h`
- Modify: `src/extensions/toc.c`
- Modify: `src/apex.c` (call site)
- Test: `tests/test_output.c`

- [ ] **Step 1: Write failing tests for option defaults on bare markers**

In `test_toc()`:

```c
    /* Bare {{TOC}} should use option defaults (1-3): exclude H4 */
    const char *bare_default_depth =
        "# H1\n\n{{TOC}}\n\n## H2\n\n### H3\n\n#### H4";
    html = apex_markdown_to_html(bare_default_depth, strlen(bare_default_depth), &opts);
    assert_contains(html, "href=\"#h1\"", "Bare TOC includes H1 by default");
    assert_contains(html, "href=\"#h3\"", "Bare TOC includes H3 by default");
    assert_not_contains(html, "href=\"#h4\"", "Bare TOC excludes H4 by default (max 3)");
    apex_free_string(html);

    /* --toc-min-max via options: 2,4 */
    apex_options depth_opts = opts;
    depth_opts.toc_min = 2;
    depth_opts.toc_max = 4;
    html = apex_markdown_to_html(bare_default_depth, strlen(bare_default_depth), &depth_opts);
    assert_not_contains(html, "href=\"#h1\"", "toc_min=2 excludes H1");
    assert_contains(html, "href=\"#h4\"", "toc_max=4 includes H4");
    apex_free_string(html);

    /* Marker override wins over options */
    const char *override_toc =
        "# H1\n\n{{TOC:1-6}}\n\n## H2\n\n### H3\n\n#### H4\n\n##### H5";
    html = apex_markdown_to_html(override_toc, strlen(override_toc), &depth_opts);
    assert_contains(html, "href=\"#h1\"", "Marker range overrides toc_min");
    assert_contains(html, "href=\"#h5\"", "Marker range overrides toc_max");
    apex_free_string(html);
```

- [ ] **Step 2: Run tests to verify failure on H4 exclusion**

```bash
cd /Users/ttscoff/Desktop/Code/apex/build && make apex_test_runner -j4 && ./apex_test_runner toc 2>&1 | rg -i "Bare TOC excludes|FAIL|Results"
```

Expected: "Bare TOC excludes H4 by default" fails (today bare marker uses max 6).

- [ ] **Step 3: Update API and `parse_toc_marker`**

`src/extensions/toc.h`:

```c
char *apex_process_toc(const char *html, cmark_node *document, int id_format,
                       int default_min, int default_max);
```

In `toc.c`, change `parse_toc_marker` to accept defaults:

```c
static void parse_toc_marker(const char *marker, int *min_level, int *max_level,
                             int default_min, int default_max) {
    *min_level = default_min;
    *max_level = default_max;
    /* ... existing parsing that overwrites when marker specifies min/max ... */
}
```

Clamp after parse:

```c
if (*min_level < 1) *min_level = 1;
if (*max_level > 6) *max_level = 6;
if (*min_level > *max_level) *max_level = *min_level;
```

Update `apex_process_toc` signature and call:

```c
parse_toc_marker(marker, &min_level, &max_level, default_min, default_max);
```

In `src/apex.c`:

```c
char *with_toc = apex_process_toc(html, document, options->id_format,
                                  options->toc_min, options->toc_max);
```

- [ ] **Step 4: Run `toc` suite**

```bash
cd /Users/ttscoff/Desktop/Code/apex/build && make apex_test_runner -j4 && ./apex_test_runner toc
```

Expected: all TOC tests pass. If any older test assumed H4+ from bare `{{TOC}}`, update that test to use an explicit marker range.

- [ ] **Step 5: Commit**

```bash
git add src/extensions/toc.h src/extensions/toc.c src/apex.c tests/test_output.c
git commit -m "Use toc_min/toc_max defaults for HTML TOC markers without a range"
```

---

### Task 4: Markdown TOC emitter + `-t toc` early exit

**Files:**
- Modify: `src/extensions/toc.h`
- Modify: `src/extensions/toc.c`
- Modify: `src/apex.c`
- Test: `tests/test_output.c`

- [ ] **Step 1: Write failing `-t toc` tests**

Add to `test_toc()`:

```c
    /* -t toc: Markdown list, default depth 1-3 */
    apex_options toc_opts = apex_options_default();
    toc_opts.output_format = APEX_OUTPUT_TOC;
    const char *doc =
        "# Introduction\n\n"
        "## Getting Started\n\n"
        "### Installation\n\n"
        "#### Too Deep\n\n"
        "## Configuration\n\n"
        "# API\n"
        "{: .no_toc}\n";
    char *md = apex_markdown_to_html(doc, strlen(doc), &toc_opts);
    assert_contains(md, "- [Introduction](#introduction)\n", "TOC md has H1");
    assert_contains(md, "  - [Getting Started](#getting-started)\n", "TOC md indents H2");
    assert_contains(md, "    - [Installation](#installation)\n", "TOC md indents H3");
    assert_not_contains(md, "Too Deep", "TOC md excludes H4 by default");
    assert_not_contains(md, "API", "TOC md excludes no_toc heading");
    assert_not_contains(md, "<nav", "TOC md is not HTML");
    apex_free_string(md);

    toc_opts.toc_min = 2;
    toc_opts.toc_max = 4;
    md = apex_markdown_to_html(doc, strlen(doc), &toc_opts);
    assert_not_contains(md, "Introduction", "toc-min-max excludes H1");
    assert_contains(md, "Too Deep", "toc-min-max includes H4");
    apex_free_string(md);

    /* id-format mmd */
    toc_opts = apex_options_default();
    toc_opts.output_format = APEX_OUTPUT_TOC;
    toc_opts.id_format = 1; /* MMD */
    md = apex_markdown_to_html("# Hello World\n", 14, &toc_opts);
    assert_contains(md, "- [Hello World](#HelloWorld)\n", "TOC md respects MMD id-format");
    apex_free_string(md);

    /* empty */
    toc_opts = apex_options_default();
    toc_opts.output_format = APEX_OUTPUT_TOC;
    md = apex_markdown_to_html("No headings here.\n", 18, &toc_opts);
    if (md && (md[0] == '\0' || strcmp(md, "\n") == 0)) {
        test_result(true, "TOC md empty when no headings");
    } else {
        test_result(false, "TOC md empty when no headings");
    }
    apex_free_string(md);
```

Adjust the MMD id expectation if `apex_generate_header_id` for "Hello World" differs — run a quick HTML render with `--id-format mmd` if needed and match that slug.

- [ ] **Step 2: Run tests — expect missing format / wrong output**

```bash
cd /Users/ttscoff/Desktop/Code/apex/build && make apex_test_runner -j4 && ./apex_test_runner toc 2>&1 | rg -i "TOC md|FAIL|Results"
```

Expected: failures (TOC still goes through HTML path or ignores `APEX_OUTPUT_TOC`).

- [ ] **Step 3: Implement `apex_generate_toc_markdown`**

`toc.h`:

```c
char *apex_generate_toc_markdown(cmark_node *document, int id_format,
                                 int min_level, int max_level);
```

`toc.c` — mirror nesting logic of `generate_toc_html`, but emit Markdown:

```c
char *apex_generate_toc_markdown(cmark_node *document, int id_format,
                                 int min_level, int max_level) {
    if (!document) return strdup("");
    if (min_level < 1) min_level = 1;
    if (max_level > 6) max_level = 6;
    if (min_level > max_level) max_level = min_level;

    header_item *tail = NULL;
    header_item *headers = collect_headers(document, &tail, id_format);
    if (!headers) return strdup("");

    size_t capacity = 4096;
    char *out = malloc(capacity);
    if (!out) {
        free_headers(headers);
        return strdup("");
    }
    char *write = out;
    size_t remaining = capacity;

    #define ENSURE(n) do { \
        if ((size_t)(n) >= remaining) { \
            size_t used = (size_t)(write - out); \
            capacity = used + (size_t)(n) + 1024; \
            char *nbuf = realloc(out, capacity); \
            if (!nbuf) { free(out); free_headers(headers); return strdup(""); } \
            out = nbuf; write = out + used; remaining = capacity - used; \
        } \
    } while (0)

    #define APPEND(str) do { \
        size_t len = strlen(str); \
        ENSURE(len + 1); \
        memcpy(write, str, len); \
        write += len; \
        remaining -= len; \
    } while (0)

    for (header_item *h = headers; h; h = h->next) {
        if (h->level < min_level || h->level > max_level) continue;
        int depth = h->level - min_level; /* 0 = flush */
        ENSURE((size_t)depth * 2 + strlen(h->text) + strlen(h->id) + 16);
        for (int i = 0; i < depth; i++) APPEND("  ");
        APPEND("- [");
        APPEND(h->text);
        APPEND("](#");
        APPEND(h->id);
        APPEND(")\n");
    }

    #undef APPEND
    #undef ENSURE

    *write = '\0';
    free_headers(headers);
    return out;
}
```

- [ ] **Step 4: Early-exit in `apex_markdown_to_html`**

In `src/apex.c`, immediately after the JSON_FILTERED early return (before Markdown dialects), add:

```c
    if (options->output_format == APEX_OUTPUT_TOC) {
        char *toc_md = apex_generate_toc_markdown(document, options->id_format,
                                                  options->toc_min, options->toc_max);
        return toc_md ? toc_md : strdup("");
    }
```

Include `toc.h` if not already included at the top of `apex.c`.

- [ ] **Step 5: Run tests**

```bash
cd /Users/ttscoff/Desktop/Code/apex/build && make apex_test_runner -j4 && ./apex_test_runner toc
```

Expected: all pass. Fix MMD slug assertion if the actual slug differs.

- [ ] **Step 6: Commit**

```bash
git add src/extensions/toc.h src/extensions/toc.c src/apex.c tests/test_output.c
git commit -m "Add -t toc Markdown TOC output format"
```

---

### Task 5: CLI `--to toc` and `--toc-min-max`

**Files:**
- Modify: `cli/main.c`

- [ ] **Step 1: Add option mask fields**

In `apex_cli_option_mask`:

```c
    bool toc_min_max;
```

In `apex_apply_cli_option_mask` (or equivalent merge helper), copy when masked:

```c
    if (m->toc_min_max) {
        opts->toc_min = snap->toc_min;
        opts->toc_max = snap->toc_max;
    }
```

- [ ] **Step 2: Parse `-t toc` and `--toc-min-max`**

In the `-t` / `--to` branch, after `man-html`:

```c
            } else if (strcmp(format, "toc") == 0) {
                options.output_format = APEX_OUTPUT_TOC;
```

Add flag parsing near `--id-format`:

```c
        } else if (strncmp(argv[i], "--toc-min-max=", 14) == 0 ||
                   strcmp(argv[i], "--toc-min-max") == 0) {
            const char *val = NULL;
            if (strncmp(argv[i], "--toc-min-max=", 14) == 0) {
                val = argv[i] + 14;
            } else {
                if (i + 1 >= argc) {
                    fprintf(stderr, "Error: --toc-min-max requires MIN,MAX (e.g. 1,3)\n");
                    return 1;
                }
                val = argv[++i];
            }
            int min = 0, max = 0;
            if (sscanf(val, "%d,%d", &min, &max) != 2 ||
                min < 1 || max > 6 || min > max) {
                fprintf(stderr, "Error: --toc-min-max must be MIN,MAX with 1 <= MIN <= MAX <= 6\n");
                return 1;
            }
            cli_opt_mask.toc_min_max = true;
            options.toc_min = min;
            options.toc_max = max;
```

- [ ] **Step 3: Help text**

In `print_usage` / help:

- Add `toc` to the `-t` formats list.
- Add:

```c
    fprintf(stderr, "  --toc-min-max MIN,MAX  TOC heading depth range (default: 1,3); also seeds HTML TOC markers without an explicit range\n");
```

- [ ] **Step 4: Manual CLI smoke check**

```bash
cd /Users/ttscoff/Desktop/Code/apex/build && make apex -j4
./apex -t toc --toc-min-max=2,4 <<'MD'
# A
## B
### C
#### D
MD
```

Expected:

```markdown
- [B](#b)
  - [C](#c)
    - [D](#d)
```

Invalid flag:

```bash
./apex -t toc --toc-min-max=0,9 </dev/null; echo exit:$?
```

Expected: non-zero exit and error message.

- [ ] **Step 5: Commit**

```bash
git add cli/main.c
git commit -m "Add CLI -t toc and --toc-min-max"
```

---

### Task 6: Metadata `toc-min-max`

**Files:**
- Modify: `src/extensions/metadata.c`
- Test: `tests/test_metadata.c` (suite that covers option keys — `test_metadata_control_options` or nearest)

- [ ] **Step 1: Write failing metadata test**

Follow existing `id-format` metadata tests in `tests/test_metadata.c`:

```c
    opts = apex_options_default();
    /* build metadata list with key toc-min-max value 2,4 — same pattern as id-format test */
    apex_apply_metadata_to_options(metadata, &opts); /* use actual helper name from nearby tests */
    assert_option_bool(opts.toc_min == 2, true, "toc-min-max sets toc_min");
    assert_option_bool(opts.toc_max == 4, true, "toc-min-max sets toc_max");
```

Read the surrounding `id-format` test and mirror its metadata list construction exactly.

- [ ] **Step 2: Run test — expect fail**

```bash
cd /Users/ttscoff/Desktop/Code/apex/build && make apex_test_runner -j4 && ./apex_test_runner metadata_control_options 2>&1 | rg -i "toc-min|FAIL|Results"
```

(If the suite name differs, use the suite that contains the id-format metadata assertions.)

- [ ] **Step 3: Parse in `metadata.c`**

Near the `id-format` branch:

```c
        } else if (strcasecmp(key, "toc-min-max") == 0 || strcasecmp(key, "toc_min_max") == 0) {
            int min = 0, max = 0;
            if (sscanf(value, "%d,%d", &min, &max) == 2 &&
                min >= 1 && max <= 6 && min <= max) {
                options->toc_min = min;
                options->toc_max = max;
            }
```

Ignore invalid values silently (match how other metadata keys behave) unless nearby keys error loudly — stay consistent.

- [ ] **Step 4: Run metadata + toc suites**

```bash
cd /Users/ttscoff/Desktop/Code/apex/build && make apex_test_runner -j4 && ./apex_test_runner toc && ./apex_test_runner metadata_control_options
```

- [ ] **Step 5: Commit**

```bash
git add src/extensions/metadata.c tests/test_metadata.c
git commit -m "Support toc-min-max metadata for TOC depth defaults"
```

---

### Task 7: Documentation

**Files:**
- Modify: `man/apex.1.md` (then `make man`)
- Modify: `/Users/ttscoff/Desktop/Code/apex.wiki/Command-Line-Options.md`
- Modify: `/Users/ttscoff/Desktop/Code/apex.wiki/Syntax.md` (TOC section)
- Optionally: `/Users/ttscoff/Desktop/Code/apex.wiki/Header-IDs.md` (note `-t toc` uses same IDs)

- [ ] **Step 1: Man page**

Under `-t` / `--to` list, add:

```markdown
- **toc** - Markdown unordered list of heading links only (default depth `#`–`###`)
```

Add flag section:

```markdown
**--toc-min-max** *MIN,MAX*
: Inclusive heading depth range for **--to toc** and for HTML TOC markers that do not specify their own range. Default **1,3**. Marker syntax such as `{{TOC:2-5}}` still overrides. Values must satisfy `1 <= MIN <= MAX <= 6`.
```

Regenerate:

```bash
cd /Users/ttscoff/Desktop/Code/apex && make man
```

- [ ] **Step 2: Wiki**

In `Command-Line-Options.md`, document `-t toc` and `--toc-min-max` near other output/format flags.

In `Syntax.md` TOC section, note:

- Bare `{{TOC}}` / `<!--TOC-->` / `{:toc}` now default to levels 1–3 (or `--toc-min-max` / `toc-min-max` metadata).
- `-t toc` extracts a Markdown TOC without markers.

- [ ] **Step 3: Commit**

```bash
git add man/apex.1.md man/apex.1
# wiki is a separate repo:
cd /Users/ttscoff/Desktop/Code/apex.wiki && git add Command-Line-Options.md Syntax.md Header-IDs.md && git status
```

Commit apex and wiki separately with clear messages. Do not push unless asked.

---

### Task 8: Final verification

- [ ] **Step 1: Full relevant test pass**

```bash
cd /Users/ttscoff/Desktop/Code/apex/build && make -j4 && ./apex_test_runner toc && ./apex_test_runner header_ids && ./apex_test_runner metadata_control_options
```

- [ ] **Step 2: End-to-end smoke**

```bash
./apex -t toc <<'MD'
# Intro
## Details
### Deep
#### Deeper
MD
./apex -t toc --toc-min-max=1,6 <<'MD'
# Intro
## Details
### Deep
#### Deeper
MD
./apex -t html <<'MD'
# Intro
{{TOC}}
## Details
### Deep
#### Deeper
MD
```

Confirm Markdown TOC and HTML bare TOC both stop at `###` by default; `--toc-min-max=1,6` includes `####`.

- [ ] **Step 3: Update `commit_message.txt` only if the user asks for a changelog commit message**

---

## Spec coverage checklist

| Spec requirement | Task |
|------------------|------|
| `-t toc` Markdown list `[Title](#id)` | 4, 5 |
| Default depth 1–3 | 1, 3, 4 |
| `--toc-min-max` | 5 |
| Flag seeds HTML markers; marker overrides | 3 |
| `.no_toc` excluded | 4 (existing collect_headers) |
| IDs match `--id-format` / manual IDs | 2, 4 |
| Metadata `toc-min-max` | 6 |
| Docs / man | 7 |
| Empty doc behavior | 4 |
| Invalid CLI values rejected | 5 |

## Notes for implementers

- `apex_markdown_to_html` is the library entry for all formats despite the name — follow existing early-return patterns for JSON/markdown.
- Bare HTML TOC default change (1–6 → 1–3) is intentional; call it out in changelog with `@changed` when the user requests a commit message.
- Do not add separate `--toc-min` / `--toc-max` flags.
- Wiki lives in `/Users/ttscoff/Desktop/Code/apex.wiki` (separate git repo).
