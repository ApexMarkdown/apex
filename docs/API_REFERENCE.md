# Apex API Reference

**Version 0.1.0**

## Overview

Apex provides a simple C API for converting Markdown to HTML
with various processor modes and options.

## Core Types

### apex_mode_t

Processor compatibility modes:

```c
typedef enum {
    APEX_MODE_COMMONMARK = 0,      /* Pure CommonMark */
    APEX_MODE_GFM = 1,              /* GitHub Flavored Markdown */
    APEX_MODE_MULTIMARKDOWN = 2,    /* MultiMarkdown */
    APEX_MODE_KRAMDOWN = 3,         /* Kramdown */
    APEX_MODE_UNIFIED = 4           /* All features */
} apex_mode_t;

```

### apex_options

Configuration structure:

```c
typedef struct {
    apex_mode_t mode;

    /* Feature flags */
    bool enable_tables;
    bool enable_footnotes;
    bool enable_definition_lists;
    bool enable_smart_typography;
    bool enable_math;
    bool enable_critic_markup;
    bool enable_wiki_links;
    bool enable_task_lists;
    bool enable_attributes;
    bool enable_callouts;
    bool enable_marked_extensions;

    /* Metadata */
    bool strip_metadata;
    bool enable_metadata_variables;

    /* File inclusion */
    bool enable_file_includes;
    int max_include_depth;
    const char *base_directory;

    /* Output options */
    bool unsafe;
    bool validate_utf8;
    bool github_pre_lang;

    /* Line breaks */
    bool hardbreaks;
    bool nobreaks;

    /* Header ID generation */
    bool generate_header_ids;
    bool header_anchors;
    int id_format;

    /* Table options */
    bool relaxed_tables;

    /* List options */
    bool allow_mixed_list_markers;  /* Allow mixed list markers at same level (inherit type from first item) */
    bool allow_alpha_lists;  /* Support alpha list markers (a., b., c. and A., B., C.) */
} apex_options;

```

## Core Functions

### apex_options_default

Get default options with all features enabled (unified
mode).

```c
apex_options apex_options_default(void);

```

**Returns**: Default options structure

**Example**:
```c
apex_options opts = apex_options_default();
opts.enable_math = true;
opts.enable_wiki_links = true;

```

### apex_options_for_mode

Get options configured for a specific processor mode.

```c
apex_options apex_options_for_mode(apex_mode_t mode);

```

**Parameters**:

- `mode`: Desired processor mode

**Returns**: Options configured for the specified mode

**Example**:
```c
apex_options gfm_opts = apex_options_for_mode(APEX_MODE_GFM);
apex_options mmd_opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
// Unified mode has mixed list markers and alpha lists enabled by default

```

### apex_markdown_to_html

Main conversion function - converts Markdown to HTML.

```c
char *apex_markdown_to_html(const char *markdown, size_t len,
                            const apex_options *options);

```

**Parameters**:

- `markdown`: Input Markdown text (UTF-8)
- `len`: Length of input text
- `options`: Processing options (NULL for defaults)

**Returns**: Newly allocated HTML string (must be freed with
`apex_free_string`)

**Example**:
```c
const char *markdown = "# Hello\n\nThis is **bold**.";
apex_options opts = apex_options_default();
char *html = apex_markdown_to_html(markdown, strlen(markdown), &opts);

if (html) {
    printf("%s\n", html);
    apex_free_string(html);
}

```

### apex_free_string

Free a string allocated by Apex.

```c
void apex_free_string(char *str);

```

**Parameters**:

- `str`: String to free (can be NULL)

**Example**:
```c
char *html = apex_markdown_to_html(markdown, len, NULL);
// Use html...
apex_free_string(html);

```

### apex_version_string

Get version string.

```c
const char *apex_version_string(void);

```

**Returns**: Version string (e.g., "0.1.0")

### apex_version_major / apex_version_minor / apex_version_patch

Get individual version components.

```c
int apex_version_major(void);
int apex_version_minor(void);
int apex_version_patch(void);

```

**Returns**: Version component as integer

## Extension Functions

### Metadata

```c
#include "extensions/metadata.h"

// Extract metadata from text (preprocessing)
apex_metadata_item *apex_extract_metadata(char **text_ptr);

// Get specific metadata value
const char *apex_metadata_get(apex_metadata_item *metadata, const char *key);

// Replace [%key] variables
char *apex_metadata_replace_variables(const char *text, apex_metadata_item *metadata);

// Free metadata
void apex_free_metadata(apex_metadata_item *metadata);

```

### Wiki Links

```c
#include "extensions/wiki_links.h"

// Process wiki links in AST
void apex_process_wiki_links_in_tree(cmark_node *document, wiki_link_config *config);

// Configure wiki link behavior
wiki_link_config config = {
    .base_path = "/wiki/",
    .extension = ".html",
    .space_mode = WIKILINK_SPACE_DASH,  // Options: WIKILINK_SPACE_DASH, WIKILINK_SPACE_NONE, WIKILINK_SPACE_UNDERSCORE, WIKILINK_SPACE_SPACE
    .sanitize = true  // Lowercase, remove apostrophes, clean non-alphanumeric chars
};

```

### Critic Markup

```c
#include "extensions/critic.h"

// Process critic markup
void apex_process_critic_markup_in_tree(cmark_node *document, critic_mode_t mode);

// Or preprocessing approach
char *apex_process_critic_markup_text(const char *text, critic_mode_t mode);

// Modes: CRITIC_ACCEPT, CRITIC_REJECT, CRITIC_MARKUP

```

## List Options

### Mixed List Markers

When `allow_mixed_list_markers` is enabled, lists with different marker types at the same indentation level will inherit the type from the first item. This is enabled by default in **MultiMarkdown** and **Unified** modes.

```c
apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
// opts.allow_mixed_list_markers is true by default

// To enable in other modes:
opts.allow_mixed_list_markers = true;

// To disable in unified/multimarkdown modes:
opts.allow_mixed_list_markers = false;

```

**Example markdown:**
```markdown
1. First numbered item
* Second item (becomes numbered)
* Third item (becomes numbered)

```

### Alpha Lists

When `allow_alpha_lists` is enabled, alphabetic markers (`a.`, `b.`, `c.` for lower-alpha and `A.`, `B.`, `C.` for upper-alpha) are converted to HTML lists with appropriate `list-style-type` CSS. This is enabled by default in **Unified** mode only.

```c
apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
// opts.allow_alpha_lists is true by default

// To enable in other modes:
opts.allow_alpha_lists = true;

// To disable in unified mode:
opts.allow_alpha_lists = false;

```

**Example markdown:**
```markdown
a. First item
b. Second item
c. Third item

```

This produces HTML with `style="list-style-type: lower-alpha"` on the `<ol>` tag.

## Complete Example

```c
#include <apex/apex.h>
#include <stdio.h>
#include <string.h>

int main() {
    // Markdown with metadata and variables
    const char *markdown =
        "---\n"
        "title: Test Document\n"
        "author: Brett\n"
        "---\n"
        "\n"
        "# [%title]\n"
        "\n"
        "By [%author]\n"
        "\n"
        "This has **bold** and a [[WikiLink]].\n"
        "\n"
        "Math: $E = mc^2$\n"
        "\n"
        "Changes: {++added++} and {--removed--}\n"
        "\n"
        "1. Mixed markers\n"
        "* Second item\n"
        "\n"
        "a. Alpha list\n"
        "b. Second alpha\n";

    // Use unified mode with all features
    apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    // Mixed markers and alpha lists are enabled by default in unified mode

    // Convert to HTML
    char *html = apex_markdown_to_html(markdown, strlen(markdown), &opts);

    if (html) {
        printf("%s\n", html);
        apex_free_string(html);
        return 0;
    } else {
        fprintf(stderr, "Conversion failed\n");
        return 1;
    }
}

```

Compile and link:
```bash
gcc example.c -I/usr/local/include -L/usr/local/lib -lapex -o example
./example

```

## Thread Safety

Apex is thread-safe as long as each thread uses its own `apex_options` structure and doesn't share document nodes. The conversion function `apex_markdown_to_html` can be called from multiple threads simultaneously.

## Memory Management

- All strings returned by Apex must be freed with

  `apex_free_string()`

- `apex_options` structures are typically stack-allocated

  (no freeing needed)

Metadata structures must be freed with `apex_free_metadata()`

## Error Handling

- Functions return NULL or empty strings on error
- Check return values before use

Errors are generally due to malloc failures or invalid input

## Performance Tips

1. **Reuse options**: Create `apex_options` once and reuse

   for multiple conversions

2. **String size**: Provide accurate length to avoid

   strlen() calls

3. **Disable unused features**: Turn off extensions you

   don't need

**Batch processing**: Process multiple documents in parallel

## Building as a Library

### CMake Integration

```cmake
# In your CMakeLists.txt
add_subdirectory(apex)
target_link_libraries(your_app apex)

```

### Manual Linking

```bash
# Compile
gcc -c -I/path/to/apex/include your_app.c

# Link
gcc your_app.o -L/path/to/apex/build -lapex -o your_app

# Run (may need library path)
DYLD_LIBRARY_PATH=/path/to/apex/build ./your_app  # macOS
LD_LIBRARY_PATH=/path/to/apex/build ./your_app    # Linux

```

## Integration with Marked

See `MARKED_INTEGRATION.md` for detailed instructions on integrating Apex into the Marked application.

Quick integration:

```objective-c
#import <NSString+Apex.h>

NSString *html = [NSString convertWithApex:markdownText];

```

## See Also

- `USER_GUIDE.md` - User documentation
- `ARCHITECTURE.md` - Technical architecture
- `MARKED_INTEGRATION.md` - Marked integration guide
- `tests/test_runner.c` - Usage examples

