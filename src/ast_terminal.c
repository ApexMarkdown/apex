/**
 * ast_terminal.c - Convert cmark-gfm AST to ANSI-colored terminal output
 *
 * This module walks the cmark AST and emits formatted text using ANSI
 * escape codes. It supports:
 *   - 8/16-color mode (standard + bright colors)
 *   - 256-color mode (xterm-256 palette)
 *   - Optional YAML theme files in:
 *       ~/.config/apex/terminal/themes/NAME.theme
 *
 * The theme format is intentionally similar to mdless themes so that
 * existing configurations can be reused with minimal changes.
 */

#include "apex/ast_terminal.h"
#include "table.h"  /* For CMARK_NODE_TABLE, CMARK_NODE_TABLE_ROW, CMARK_NODE_TABLE_CELL */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#ifdef APEX_HAVE_LIBYAML
#include <yaml.h>
#endif

/* ------------------------------------------------------------------------- */
/* Buffer helper                                                             */
/* ------------------------------------------------------------------------- */

typedef struct {
    char *buf;
    size_t len;
    size_t capacity;
} terminal_buffer;

/* Alignment options for table cells */
typedef enum {
    TERM_ALIGN_DEFAULT = 0,
    TERM_ALIGN_LEFT,
    TERM_ALIGN_CENTER,
    TERM_ALIGN_RIGHT
} term_align_t;

/* Logical table cell/grid structures for span-aware layout */
typedef struct {
    cmark_node *cell;  /* Owning TABLE_CELL node */
    int row_span;      /* >= 1 */
    int col_span;      /* >= 1 */
    bool is_owner;     /* true only for top-left of a span */
    bool removed;      /* true if helper / data-remove */
    bool printed;      /* has this owner's content been rendered yet? */
} term_cell;

typedef struct {
    term_cell *cells;  /* Flattened rows*cols array */
    int rows;
    int cols;
} term_table_grid;

static term_cell *grid_at(term_table_grid *g, int r, int c) {
    if (!g || r < 0 || r >= g->rows || c < 0 || c >= g->cols) return NULL;
    return &g->cells[r * g->cols + c];
}

static void buffer_init(terminal_buffer *buf) {
    buf->buf = NULL;
    buf->len = 0;
    buf->capacity = 0;
}

static void buffer_append(terminal_buffer *buf, const char *str, size_t len) {
    if (!str || len == 0) return;

    if (buf->len + len + 1 > buf->capacity) {
        size_t new_cap = buf->capacity ? buf->capacity * 2 : 256;
        if (new_cap < buf->len + len + 1) {
            new_cap = buf->len + len + 1;
        }
        char *new_buf = (char *)realloc(buf->buf, new_cap);
        if (!new_buf) return;
        buf->buf = new_buf;
        buf->capacity = new_cap;
    }

    memcpy(buf->buf + buf->len, str, len);
    buf->len += len;
    buf->buf[buf->len] = '\0';
}

static void buffer_append_str(terminal_buffer *buf, const char *str) {
    if (str) {
        buffer_append(buf, str, strlen(str));
    }
}

/* Helper to detect alignment from cell/user attributes (advanced tables, IAL, etc.) */
static term_align_t
parse_alignment_from_attrs(const char *attrs) {
    if (!attrs) {
        return TERM_ALIGN_DEFAULT;
    }

    /* Prefer explicit text-align style if present */
    if (strstr(attrs, "text-align: center") || strstr(attrs, "text-align:center")) {
        return TERM_ALIGN_CENTER;
    }
    if (strstr(attrs, "text-align: right") || strstr(attrs, "text-align:right")) {
        return TERM_ALIGN_RIGHT;
    }
    if (strstr(attrs, "text-align: left") || strstr(attrs, "text-align:left")) {
        return TERM_ALIGN_LEFT;
    }

    /* Fallback: check legacy align attribute if it ever appears in user_data */
    if (strstr(attrs, "align=\"center\"")) {
        return TERM_ALIGN_CENTER;
    }
    if (strstr(attrs, "align=\"right\"")) {
        return TERM_ALIGN_RIGHT;
    }
    if (strstr(attrs, "align=\"left\"")) {
        return TERM_ALIGN_LEFT;
    }

    return TERM_ALIGN_DEFAULT;
}

/* Parse integer attribute value like colspan="3" from an attribute string. */
static int parse_span_attr(const char *attrs, const char *name) {
    if (!attrs || !name) return 1;
    const char *p = strstr(attrs, name);
    if (!p) return 1;
    p += strlen(name);
    if (*p != '=') return 1;
    p++;
    if (*p == '"' || *p == '\'') {
        char quote = *p++;
        int val = atoi(p);
        (void)quote;
        if (val <= 0) val = 1;
        return val;
    } else {
        int val = atoi(p);
        if (val <= 0) val = 1;
        return val;
    }
}

/* ------------------------------------------------------------------------- */
/* ANSI helpers                                                              */
/* ------------------------------------------------------------------------- */

static void append_ansi_reset(terminal_buffer *buf) {
    buffer_append_str(buf, "\x1b[0m");
}

/* Append a CSI sequence like "38;5;123m" wrapped with ESC[ ... */
static void append_ansi_seq(terminal_buffer *buf, const char *seq) {
    buffer_append_str(buf, "\x1b[");
    buffer_append_str(buf, seq);
    buffer_append_str(buf, "m");
}

/* Basic 8/16-color mapping from name to SGR code (foreground). */
static int ansi_color_code_from_name(const char *name, bool *is_background) {
    *is_background = false;
    if (!name || !*name) return -1;

    /* Background prefix: on_foo or bgFOO */
    if (strncmp(name, "on_", 3) == 0) {
        *is_background = true;
        name += 3;
    } else if (strncmp(name, "bg", 2) == 0 && isupper((unsigned char)name[2])) {
        *is_background = true;
        name += 2;
    }

    /* Intense / bright colors */
    bool intense = false;
    if (strncmp(name, "intense_", 8) == 0) {
        intense = true;
        name += 8;
    } else if (strncmp(name, "bright_", 7) == 0) {
        intense = true;
        name += 7;
    }

    int base = -1;
    if (strcmp(name, "black") == 0) base = 0;
    else if (strcmp(name, "red") == 0) base = 1;
    else if (strcmp(name, "green") == 0) base = 2;
    else if (strcmp(name, "yellow") == 0) base = 3;
    else if (strcmp(name, "blue") == 0) base = 4;
    else if (strcmp(name, "magenta") == 0) base = 5;
    else if (strcmp(name, "cyan") == 0) base = 6;
    else if (strcmp(name, "white") == 0) base = 7;

    if (base < 0) return -1;

    if (*is_background) {
        return (int)(intense ? (100 + base) : (40 + base));
    } else {
        return (int)(intense ? (90 + base) : (30 + base));
    }
}

/* Convert a hex nibble to int [0,15] */
static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

/* Parse 3- or 6-digit hex to RGB (0-255 each). Returns 0 on success. */
static int parse_hex_rgb(const char *hex, int *r, int *g, int *b) {
    size_t len = strlen(hex);
    const char *p = hex;
    if (*p == '#') {
        p++;
        len--;
    }
    if (len == 3) {
        int r1 = hex_nibble(p[0]);
        int g1 = hex_nibble(p[1]);
        int b1 = hex_nibble(p[2]);
        if (r1 < 0 || g1 < 0 || b1 < 0) return -1;
        *r = (r1 << 4) | r1;
        *g = (g1 << 4) | g1;
        *b = (b1 << 4) | b1;
        return 0;
    } else if (len == 6) {
        int r1 = hex_nibble(p[0]);
        int r2 = hex_nibble(p[1]);
        int g1 = hex_nibble(p[2]);
        int g2 = hex_nibble(p[3]);
        int b1 = hex_nibble(p[4]);
        int b2 = hex_nibble(p[5]);
        if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) return -1;
        *r = (r1 << 4) | r2;
        *g = (g1 << 4) | g2;
        *b = (b1 << 4) | b2;
        return 0;
    }
    return -1;
}

/* Map RGB 0–255 to nearest xterm-256 color index. */
static int rgb_to_256(int r, int g, int b) {
    /* Grayscale range 232-255 for near-gray colors */
    if (r == g && g == b) {
        if (r < 8) return 16;
        if (r > 248) return 231;
        int gray = (int)((r - 8) / 247.0 * 24.0 + 0.5);
        return 232 + (gray < 0 ? 0 : (gray > 23 ? 23 : gray));
    }

    /* 6x6x6 color cube 16-231 */
    int rc = (int)((r / 255.0) * 5.0 + 0.5);
    int gc = (int)((g / 255.0) * 5.0 + 0.5);
    int bc = (int)((b / 255.0) * 5.0 + 0.5);
    if (rc < 0) rc = 0; if (rc > 5) rc = 5;
    if (gc < 0) gc = 0; if (gc > 5) gc = 5;
    if (bc < 0) bc = 0; if (bc > 5) bc = 5;
    return 16 + 36 * rc + 6 * gc + bc;
}

/* Append color/style according to a single token in a theme string. */
static void apply_style_token(terminal_buffer *buf,
                              const char *token,
                              bool use_256_color) {
    if (!token || !*token) return;

    /* Common style flags */
    if (strcmp(token, "b") == 0 || strcmp(token, "bold") == 0) {
        append_ansi_seq(buf, "1");
        return;
    }
    if (strcmp(token, "d") == 0 || strcmp(token, "dark") == 0) {
        append_ansi_seq(buf, "2");
        return;
    }
    if (strcmp(token, "i") == 0 || strcmp(token, "italic") == 0) {
        append_ansi_seq(buf, "3");
        return;
    }
    if (strcmp(token, "u") == 0 || strcmp(token, "underline") == 0 ||
        strcmp(token, "underscore") == 0) {
        append_ansi_seq(buf, "4");
        return;
    }
    if (strcmp(token, "r") == 0 || strcmp(token, "reverse") == 0 ||
        strcmp(token, "negative") == 0) {
        append_ansi_seq(buf, "7");
        return;
    }

    /* Raw 256-color SGR like 38;5;123 or 48;5;45 */
    if (strstr(token, "38;5;") == token || strstr(token, "48;5;") == token) {
        append_ansi_seq(buf, token);
        return;
    }

    /* Hex colors: #RGB, #RRGGBB, bgFF0ACC, on_FF0ACC, etc. */
    if (token[0] == '#' || strncmp(token, "bg", 2) == 0 || strncmp(token, "on_", 3) == 0) {
        const char *hex_part = token;
        int is_bg = 0;
        if (strncmp(token, "bg", 2) == 0) {
            is_bg = 1;
            hex_part = token + 2;
        } else if (strncmp(token, "on_", 3) == 0) {
            is_bg = 1;
            hex_part = token + 3;
        }
        int r, g, b;
        if (parse_hex_rgb(hex_part, &r, &g, &b) == 0) {
            if (use_256_color) {
                int idx = rgb_to_256(r, g, b);
                char seq[32];
                if (is_bg) {
                    snprintf(seq, sizeof(seq), "48;5;%d", idx);
                } else {
                    snprintf(seq, sizeof(seq), "38;5;%d", idx);
                }
                append_ansi_seq(buf, seq);
            } else {
                /* Approximate to nearest 8-color: simple heuristic by max channel */
                const char *name = "white";
                if (r > g && r > b) name = "red";
                else if (g > r && g > b) name = "green";
                else if (b > r && b > g) name = "blue";
                bool is_background;
                int code = ansi_color_code_from_name(name, &is_background);
                if (code >= 0) {
                    char seq[8];
                    snprintf(seq, sizeof(seq), "%d", code);
                    append_ansi_seq(buf, seq);
                }
            }
            return;
        }
    }

    /* Named colors (including intense_*) */
    bool is_background;
    int c = ansi_color_code_from_name(token, &is_background);
    if (c >= 0) {
        char seq[8];
        snprintf(seq, sizeof(seq), "%d", c);
        append_ansi_seq(buf, seq);
        return;
    }
}

/* Apply a full style string like "b white on_intense_black" */
static void apply_style_string(terminal_buffer *buf,
                               const char *style,
                               bool use_256_color) {
    if (!style) return;
    /* Split on whitespace */
    const char *p = style;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t len = (size_t)(p - start);
        char token[64];
        if (len >= sizeof(token)) len = sizeof(token) - 1;
        memcpy(token, start, len);
        token[len] = '\0';
        apply_style_token(buf, token, use_256_color);
    }
}

/* ------------------------------------------------------------------------- */
/* Theme data structure                                                      */
/* ------------------------------------------------------------------------- */

typedef struct terminal_theme {
    /* We keep this simple and only store a few high-value styles.
     * The YAML can define more, but we will use at least these keys.
     */
    char *h1_color;
    char *h2_color;
    char *h3_color;
    char *h4_color;
    char *h5_color;
    char *h6_color;

    char *link_text;
    char *link_url;

    char *code_span;
    char *code_block;

    char *blockquote_marker;
    char *blockquote_color;

    /* Table-related styles */
    char *table_border;  /* ANSI style for table borders (box-drawing chars) */
} terminal_theme;

static void free_theme(terminal_theme *theme) {
    if (!theme) return;
    free(theme->h1_color);
    free(theme->h2_color);
    free(theme->h3_color);
    free(theme->h4_color);
    free(theme->h5_color);
    free(theme->h6_color);
    free(theme->link_text);
    free(theme->link_url);
    free(theme->code_span);
    free(theme->code_block);
    free(theme->blockquote_marker);
    free(theme->blockquote_color);
    free(theme->table_border);
    free(theme);
}

static char *dup_or_null(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

/* ------------------------------------------------------------------------- */
/* Theme loading (YAML, optional)                                            */
/* ------------------------------------------------------------------------- */

static char *build_theme_path(const char *name) {
    const char *home = getenv("HOME");
    if (!home || !*home) return NULL;

    /* ~/.config/apex/terminal/themes/NAME.theme */
    const char *base = "/.config/apex/terminal/themes/";
    size_t home_len = strlen(home);
    size_t base_len = strlen(base);
    size_t name_len = name ? strlen(name) : 0;
    const char *ext = ".theme";
    size_t ext_len = strlen(ext);

    size_t total = home_len + base_len + name_len + ext_len + 1;
    char *path = (char *)malloc(total);
    if (!path) return NULL;

    memcpy(path, home, home_len);
    memcpy(path + home_len, base, base_len);
    memcpy(path + home_len + base_len, name, name_len);
    memcpy(path + home_len + base_len + name_len, ext, ext_len);
    path[total - 1] = '\0';
    return path;
}

static terminal_theme *load_theme_from_yaml(const char *path) {
    (void)path;

#ifndef APEX_HAVE_LIBYAML
    /* libyaml not available, no theme support */
    return NULL;
#else
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }

    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) {
        fclose(fp);
        return NULL;
    }
    yaml_parser_set_input_file(&parser, fp);

    terminal_theme *theme = (terminal_theme *)calloc(1, sizeof(terminal_theme));
    if (!theme) {
        yaml_parser_delete(&parser);
        fclose(fp);
        return NULL;
    }

    /* Very simple YAML walker: track a key path up to two levels:
     *   h1.color
     *   link.text
     *   link.url
     *   code_span.color
     *   code_block.color
     *   blockquote.marker.character
     *   blockquote.color
     *   table.border
     */
    char current_level1[64] = {0};
    char current_level2[64] = {0};
    int depth = 0;
    char *last_key = NULL;

    int done = 0;
    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            break;
        }

        switch (event.type) {
            case YAML_MAPPING_START_EVENT:
                depth++;
                break;
            case YAML_MAPPING_END_EVENT:
                if (depth == 2) {
                    current_level2[0] = '\0';
                } else if (depth == 1) {
                    current_level1[0] = '\0';
                }
                depth--;
                break;
            case YAML_SCALAR_EVENT: {
                const char *value = (const char *)event.data.scalar.value;
                if (!last_key) {
                    /* This scalar is a key */
                    if (depth == 1) {
                        strncpy(current_level1, value, sizeof(current_level1) - 1);
                        current_level1[sizeof(current_level1) - 1] = '\0';
                    } else if (depth == 2) {
                        strncpy(current_level2, value, sizeof(current_level2) - 1);
                        current_level2[sizeof(current_level2) - 1] = '\0';
                    }
                    last_key = current_level2[0] ? current_level2 : current_level1;
                } else {
                    /* This scalar is a value */
                    const char *l1 = current_level1;
                    const char *l2 = current_level2;

                    if (strcmp(l1, "h1") == 0 && strcmp(last_key, "color") == 0) {
                        free(theme->h1_color);
                        theme->h1_color = dup_or_null(value);
                    } else if (strcmp(l1, "h2") == 0 && strcmp(last_key, "color") == 0) {
                        free(theme->h2_color);
                        theme->h2_color = dup_or_null(value);
                    } else if (strcmp(l1, "h3") == 0 && strcmp(last_key, "color") == 0) {
                        free(theme->h3_color);
                        theme->h3_color = dup_or_null(value);
                    } else if (strcmp(l1, "h4") == 0 && strcmp(last_key, "color") == 0) {
                        free(theme->h4_color);
                        theme->h4_color = dup_or_null(value);
                    } else if (strcmp(l1, "h5") == 0 && strcmp(last_key, "color") == 0) {
                        free(theme->h5_color);
                        theme->h5_color = dup_or_null(value);
                    } else if (strcmp(l1, "h6") == 0 && strcmp(last_key, "color") == 0) {
                        free(theme->h6_color);
                        theme->h6_color = dup_or_null(value);
                    } else if (strcmp(l1, "link") == 0 && strcmp(last_key, "text") == 0) {
                        free(theme->link_text);
                        theme->link_text = dup_or_null(value);
                    } else if (strcmp(l1, "link") == 0 && strcmp(last_key, "url") == 0) {
                        free(theme->link_url);
                        theme->link_url = dup_or_null(value);
                    } else if (strcmp(l1, "code_span") == 0 && strcmp(last_key, "color") == 0) {
                        free(theme->code_span);
                        theme->code_span = dup_or_null(value);
                    } else if (strcmp(l1, "code_block") == 0 && strcmp(last_key, "color") == 0) {
                        free(theme->code_block);
                        theme->code_block = dup_or_null(value);
                    } else if (strcmp(l1, "blockquote") == 0 && strcmp(l2, "marker") == 0 && strcmp(last_key, "character") == 0) {
                        free(theme->blockquote_marker);
                        theme->blockquote_marker = dup_or_null(value);
                    } else if (strcmp(l1, "blockquote") == 0 && strcmp(last_key, "color") == 0) {
                        free(theme->blockquote_color);
                        theme->blockquote_color = dup_or_null(value);
                    } else if (strcmp(l1, "table") == 0 && strcmp(last_key, "border") == 0) {
                        free(theme->table_border);
                        theme->table_border = dup_or_null(value);
                    }

                    last_key = NULL;
                }
                break;
            }
            case YAML_STREAM_END_EVENT:
                done = 1;
                break;
            default:
                break;
        }
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(fp);

    return theme;
#endif
}

static terminal_theme *load_theme(const apex_options *options) {
    const char *name = options && options->theme_name ? options->theme_name : NULL;

    /* 1. Explicit --theme NAME */
    if (name && *name) {
        char *path = build_theme_path(name);
        if (path) {
            terminal_theme *theme = load_theme_from_yaml(path);
            free(path);
            if (theme) {
                return theme;
            }
        }
    }

    /* 2. default.theme fallback */
    char *default_path = build_theme_path("default");
    if (default_path) {
        terminal_theme *theme = load_theme_from_yaml(default_path);
        free(default_path);
        if (theme) {
            return theme;
        }
    }

    return NULL;
}

/* ------------------------------------------------------------------------- */
/* AST serialization                                                         */
/* ------------------------------------------------------------------------- */

/* Compute a rough visible text width for a node, ignoring ANSI and styling.
 * Used for table column width calculation.
 */
static int node_plain_text_width(cmark_node *node) {
    if (!node) return 0;

    cmark_node_type t = cmark_node_get_type(node);
    const char *lit = cmark_node_get_literal(node);
    int width = 0;

    switch (t) {
        case CMARK_NODE_TEXT:
        case CMARK_NODE_CODE:
        case CMARK_NODE_HTML_INLINE:
            if (lit) {
                for (const char *p = lit; *p; p++) {
                    if (*p == '\n' || *p == '\r') {
                        width++; /* treat newline as a space */
                    } else {
                        width++;
                    }
                }
            }
            break;
        default: {
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                width += node_plain_text_width(child);
            }
            break;
        }
    }
    return width;
}

static void serialize_inline(terminal_buffer *buf,
                             cmark_node *node,
                             const apex_options *options,
                             const terminal_theme *theme,
                             bool use_256_color);

static void serialize_block(terminal_buffer *buf,
                            cmark_node *node,
                            const apex_options *options,
                            const terminal_theme *theme,
                            bool use_256_color,
                            int indent_level);

static void indent_spaces(terminal_buffer *buf, int indent_level) {
    for (int i = 0; i < indent_level; i++) {
        buffer_append_str(buf, "  ");
    }
}

static void serialize_inline(terminal_buffer *buf,
                             cmark_node *node,
                             const apex_options *options,
                             const terminal_theme *theme,
                             bool use_256_color) {
    (void)options;

    cmark_node_type type = cmark_node_get_type(node);
    const char *literal = cmark_node_get_literal(node);

    switch (type) {
        case CMARK_NODE_TEXT:
            if (literal) {
                /* Replace APEXLTLT placeholder (from escaped \<<) with << */
                const char *apexltlt = "APEXLTLT";
                const char *found = strstr(literal, apexltlt);
                if (found) {
                    /* Replace all occurrences */
                    const char *p = literal;
                    while ((found = strstr(p, apexltlt)) != NULL) {
                        /* Append text before the placeholder */
                        if (found > p) {
                            size_t len = (size_t)(found - p);
                            char *before = (char *)malloc(len + 1);
                            if (before) {
                                memcpy(before, p, len);
                                before[len] = '\0';
                                buffer_append_str(buf, before);
                                free(before);
                            }
                        }
                        /* Append the replacement */
                        buffer_append_str(buf, "<<");
                        p = found + 8; /* Skip "APEXLTLT" (8 chars) */
                    }
                    /* Append remaining text */
                    if (*p) {
                        buffer_append_str(buf, p);
                    }
                } else {
                    buffer_append_str(buf, literal);
                }
            }
            break;
        case CMARK_NODE_SOFTBREAK:
            buffer_append_str(buf, "\n");
            break;
        case CMARK_NODE_LINEBREAK:
            buffer_append_str(buf, "  \n");
            break;
        case CMARK_NODE_CODE:
            if (literal) {
                if (theme && theme->code_span) {
                    apply_style_string(buf, theme->code_span, use_256_color);
                } else {
                    apply_style_string(buf, "b white on_intense_black", use_256_color);
                }
                buffer_append_str(buf, literal);
                append_ansi_reset(buf);
            }
            break;
        case CMARK_NODE_EMPH:
            apply_style_string(buf, "i", use_256_color);
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_inline(buf, child, options, theme, use_256_color);
            }
            append_ansi_reset(buf);
            break;
        case CMARK_NODE_STRONG:
            apply_style_string(buf, "b", use_256_color);
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_inline(buf, child, options, theme, use_256_color);
            }
            append_ansi_reset(buf);
            break;
        case CMARK_NODE_LINK: {
            const char *url = cmark_node_get_url(node);
            if (theme && theme->link_text) {
                apply_style_string(buf, theme->link_text, use_256_color);
            } else {
                apply_style_string(buf, "u b blue", use_256_color);
            }
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_inline(buf, child, options, theme, use_256_color);
            }
            append_ansi_reset(buf);
            if (url && *url) {
                buffer_append_str(buf, " ");
                if (theme && theme->link_url) {
                    apply_style_string(buf, theme->link_url, use_256_color);
                } else {
                    apply_style_string(buf, "cyan", use_256_color);
                }
                buffer_append_str(buf, "(");
                buffer_append_str(buf, url);
                buffer_append_str(buf, ")");
                append_ansi_reset(buf);
            }
            break;
        }
        case CMARK_NODE_IMAGE: {
            const char *url = cmark_node_get_url(node);
            buffer_append_str(buf, "![");
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_inline(buf, child, options, theme, use_256_color);
            }
            buffer_append_str(buf, "]");
            if (url && *url) {
                buffer_append_str(buf, "(");
                buffer_append_str(buf, url);
                buffer_append_str(buf, ")");
            }
            break;
        }
        default:
            /* Recurse for any other inline container types */
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_inline(buf, child, options, theme, use_256_color);
            }
            break;
    }
}

static void serialize_block(terminal_buffer *buf,
                            cmark_node *node,
                            const apex_options *options,
                            const terminal_theme *theme,
                            bool use_256_color,
                            int indent_level) {
    cmark_node_type type = cmark_node_get_type(node);

    /* Table nodes come from the cmark-gfm table extension and use dynamic
     * node type IDs (CMARK_NODE_TABLE, CMARK_NODE_TABLE_ROW, etc.) that
     * are not compile-time constants, so they cannot be used directly in
     * switch case labels. Handle them up-front here instead. */
    if (type == CMARK_NODE_TABLE) {
        /* If advanced_tables or other processors marked this entire table for
         * removal, skip it. */
        char *table_attrs = (char *)cmark_node_get_user_data(node);
        if (table_attrs && strstr(table_attrs, "data-remove")) {
            return;
        }

        /* Build a span-aware grid of logical rows/columns */
        int phys_rows = 0;
        for (cmark_node *row = cmark_node_first_child(node); row; row = cmark_node_next(row)) {
            if (cmark_node_get_type(row) == CMARK_NODE_TABLE_ROW) {
                phys_rows++;
            }
        }
        if (phys_rows <= 0) return;

        /* First pass: estimate max logical columns (sum of colspans per row).
         * IMPORTANT: We must mirror advanced_tables' logical column indexing,
         * which counts helper cells (even ones later marked data-remove) so
         * col/row spans line up with HTML rendering. So we always advance
         * logical_cols by colspan, regardless of data-remove. */
        int max_cols = 0;
        for (cmark_node *row = cmark_node_first_child(node); row; row = cmark_node_next(row)) {
            if (cmark_node_get_type(row) != CMARK_NODE_TABLE_ROW) continue;
            int logical_cols = 0;
            for (cmark_node *cell = cmark_node_first_child(row); cell; cell = cmark_node_next(cell)) {
                if (cmark_node_get_type(cell) != CMARK_NODE_TABLE_CELL) continue;
                char *attrs = (char *)cmark_node_get_user_data(cell);
                int colspan = parse_span_attr(attrs, "colspan");
                if (colspan < 1) colspan = 1;
                logical_cols += colspan;
            }
            if (logical_cols > max_cols) max_cols = logical_cols;
        }
        if (max_cols <= 0) return;

        term_table_grid grid;
        grid.rows = phys_rows;
        grid.cols = max_cols;
        grid.cells = (term_cell *)calloc((size_t)(phys_rows * max_cols), sizeof(term_cell));
        if (!grid.cells) return;

        /* Populate grid with owners and spans */
        int r_index = 0;
        for (cmark_node *row = cmark_node_first_child(node); row; row = cmark_node_next(row)) {
            if (cmark_node_get_type(row) != CMARK_NODE_TABLE_ROW) continue;
            int logical_col = 0;
            for (cmark_node *cell = cmark_node_first_child(row); cell; cell = cmark_node_next(cell)) {
                if (cmark_node_get_type(cell) != CMARK_NODE_TABLE_CELL) continue;

                char *attrs = (char *)cmark_node_get_user_data(cell);
                int colspan = parse_span_attr(attrs, "colspan");
                int rowspan = parse_span_attr(attrs, "rowspan");
                if (colspan < 1) colspan = 1;
                if (rowspan < 1) rowspan = 1;

                bool removed = (attrs && strstr(attrs, "data-remove"));

                if (!removed) {
                    /* Find next free column (skip slots already occupied by rowspans) */
                    while (logical_col < max_cols) {
                        term_cell *slot = grid_at(&grid, r_index, logical_col);
                        if (!slot || !slot->cell) break;
                        logical_col++;
                    }
                    if (logical_col >= max_cols) break;

                    /* Place the span for real (visible) cells */
                    for (int rr = 0; rr < rowspan; rr++) {
                        for (int cc = 0; cc < colspan; cc++) {
                            int gr = r_index + rr;
                            int gc = logical_col + cc;
                            term_cell *slot = grid_at(&grid, gr, gc);
                            if (!slot) continue;
                            slot->cell = cell;
                            slot->row_span = rowspan;
                            slot->col_span = colspan;
                            slot->removed = false;
                            slot->is_owner = (rr == 0 && cc == 0);
                            slot->printed = false;
                        }
                    }
                }

                /* Always advance logical_col by colspan, even for removed helpers,
                 * so our logical column indices stay aligned with advanced_tables. */
                logical_col += colspan;
            }
            r_index++;
        }

        /* Compute column widths from grid */
        int *col_widths = (int *)calloc((size_t)max_cols, sizeof(int));
        if (!col_widths) {
            free(grid.cells);
            return;
        }
        for (int r = 0; r < grid.rows; r++) {
            for (int c = 0; c < grid.cols; c++) {
                term_cell *tc = grid_at(&grid, r, c);
                if (!tc || !tc->cell || !tc->is_owner) continue;
                int w = node_plain_text_width(tc->cell);
                int span = (tc->col_span > 0) ? tc->col_span : 1;
                if (span == 1) {
                    if (w > col_widths[c]) col_widths[c] = w;
                } else {
                    int per = (w + span - 1) / span; /* ceil */
                    for (int k = 0; k < span && c + k < grid.cols; k++) {
                        if (per > col_widths[c + k]) col_widths[c + k] = per;
                    }
                }
            }
        }

        /* Determine how many logical columns are actually used by any cell.
         * Some trailing columns may exist only as helpers for spans; we don't
         * want to render those as empty visual columns. */
        int last_used_col = -1;
        for (int r = 0; r < grid.rows; r++) {
            for (int c = 0; c < grid.cols; c++) {
                term_cell *tc = grid_at(&grid, r, c);
                if (tc && tc->cell) {
                    if (c > last_used_col) last_used_col = c;
                }
            }
        }
        int visual_cols = (last_used_col >= 0) ? (last_used_col + 1) : 0;
        if (visual_cols <= 0) {
            free(col_widths);
            free(grid.cells);
            return;
        }

        /* Per-column default alignment, primarily from header row attributes. */
        term_align_t *col_align = (term_align_t *)calloc((size_t)max_cols, sizeof(term_align_t));
        if (!col_align) {
            free(col_widths);
            free(grid.cells);
            return;
        }
        /* Derive default column alignment from first row's owners */
        for (int c = 0; c < max_cols; c++) {
            term_cell *tc = grid_at(&grid, 0, c);
            if (!tc || !tc->cell || !tc->is_owner) continue;
            const char *attrs = (const char *)cmark_node_get_user_data(tc->cell);
            term_align_t a = parse_alignment_from_attrs(attrs);
            if (a != TERM_ALIGN_DEFAULT) {
                col_align[c] = a;
            }
        }

        /* Box-drawing characters for borders */
        const char *h_line = "─";
        const char *v_line = "│";
        const char *top_left = "┌";
        const char *top_sep = "┬";
        const char *top_right = "┐";
        const char *mid_left = "├";
        const char *mid_sep = "┼";
        const char *mid_right = "┤";
        const char *bot_left = "└";
        const char *bot_sep = "┴";
        const char *bot_right = "┘";

        /* Border color: theme override, otherwise "dark" white / light gray. */
        const char *border_style = NULL;
        if (theme && theme->table_border) {
            border_style = theme->table_border;
        } else {
            border_style = use_256_color ? "38;5;250" : "white";
        }

        /* Top border */
        indent_spaces(buf, indent_level);
        if (border_style) {
            apply_style_string(buf, border_style, use_256_color);
        }
        buffer_append_str(buf, top_left);
        for (int c = 0; c < visual_cols; c++) {
            int inner = col_widths[c] > 0 ? col_widths[c] : 1;
            for (int i = 0; i < inner + 2; i++) {
                buffer_append_str(buf, h_line);
            }
            if (c == visual_cols - 1) {
                buffer_append_str(buf, top_right);
            } else {
                buffer_append_str(buf, top_sep);
            }
        }
        buffer_append_str(buf, "\n");
        if (border_style) {
            append_ansi_reset(buf);
        }

        /* Rows: render from grid */
        int row_index = 0;
        for (int r = 0; r < grid.rows; r++) {
            /* Detect footer separator rows: logical rows whose visible cell text is all '=' */
            bool is_footer_rule = false;
            {
                bool has_cells = false;
                bool all_equals = true;
                for (int c = 0; c < visual_cols; c++) {
                    term_cell *tc = grid_at(&grid, r, c);
                    if (!tc || !tc->cell || !tc->is_owner) continue;
                    has_cells = true;

                    cmark_node *text_node = cmark_node_first_child(tc->cell);
                    const char *text = NULL;
                    if (text_node && cmark_node_get_type(text_node) == CMARK_NODE_TEXT) {
                        text = cmark_node_get_literal(text_node);
                    }
                    if (!text) {
                        all_equals = false;
                        break;
                    }
                    const char *p = text;
                    while (*p && isspace((unsigned char)*p)) p++;
                    if (!*p) {
                        all_equals = false;
                        break;
                    }
                    const char *q = p + strlen(p) - 1;
                    while (q > p && isspace((unsigned char)*q)) q--;
                    for (const char *s = p; s <= q; s++) {
                        if (*s != '=') {
                            all_equals = false;
                            break;
                        }
                    }
                    if (!all_equals) break;
                }
                if (has_cells && all_equals) {
                    is_footer_rule = true;
                }
            }

            bool is_header = (row_index == 0); /* Heuristic: first row is header */

            if (is_footer_rule) {
                indent_spaces(buf, indent_level);
                if (border_style) {
                    apply_style_string(buf, border_style, use_256_color);
                }
                buffer_append_str(buf, mid_left);
                for (int c = 0; c < visual_cols; c++) {
                    int inner = col_widths[c] > 0 ? col_widths[c] : 1;
                    for (int i = 0; i < inner + 2; i++) {
                        buffer_append_str(buf, h_line);
                    }
                    if (c == visual_cols - 1) {
                        buffer_append_str(buf, mid_right);
                    } else {
                        buffer_append_str(buf, mid_sep);
                    }
                }
                buffer_append_str(buf, "\n");
                if (border_style) {
                    append_ansi_reset(buf);
                }
                row_index++;
                continue;
            }

            /* Content line */
            indent_spaces(buf, indent_level);
            if (border_style) {
                apply_style_string(buf, border_style, use_256_color);
            }
            buffer_append_str(buf, v_line);
            if (border_style) {
                append_ansi_reset(buf);
            }

            int c = 0;
            while (c < visual_cols) {
                term_cell *tc = grid_at(&grid, r, c);
                if (!tc || !tc->cell) {
                    /* Empty slot */
                    int target = col_widths[c] > 0 ? col_widths[c] : 1;
                    buffer_append_str(buf, " ");
                    for (int i = 0; i < target + 1; i++) {
                        buffer_append_str(buf, " ");
                    }
                    if (border_style) {
                        apply_style_string(buf, border_style, use_256_color);
                    }
                    buffer_append_str(buf, v_line);
                    if (border_style) {
                        append_ansi_reset(buf);
                    }
                    c++;
                    continue;
                }

                int span = tc->col_span > 0 ? tc->col_span : 1;

                if (!tc->is_owner) {
                    /* Inside a span: just draw empty space for this column block */
                    int target = col_widths[c] > 0 ? col_widths[c] : 1;
                    buffer_append_str(buf, " ");
                    for (int i = 0; i < target + 1; i++) {
                        buffer_append_str(buf, " ");
                    }
                    if (border_style) {
                        apply_style_string(buf, border_style, use_256_color);
                    }
                    buffer_append_str(buf, v_line);
                    if (border_style) {
                        append_ansi_reset(buf);
                    }
                    c++;
                    continue;
                }

                /* Owner cell: compute total width across its span.
                 * For 'span' columns normally: each column has border + space + content + space + border
                 * Total content+padding area for span columns: sum(col_widths) + span*2 spaces
                 * For colspan we render: space + content + space (the 2 borders are separate)
                 * So content area should be: sum(col_widths) + span*2 - 2 = sum(col_widths) + 2*(span-1)
                 * But we also need to account for the (span-1) borders that are removed,
                 * each border takes 1 character, so add (span-1): sum(col_widths) + 2*(span-1) + (span-1)
                 * = sum(col_widths) + 3*(span-1) = sum(col_widths) + 3*span - 3 */
                int block_width = 0;
                for (int k = 0; k < span && c + k < visual_cols; k++) {
                    block_width += (col_widths[c + k] > 0 ? col_widths[c + k] : 1);
                }
                /* Add the padding/border spaces that would normally be between columns.
                 * For span columns: sum(col_widths) + span*2 spaces + (span-1) borders removed
                 * = sum(col_widths) + 2*span + (span-1) = sum(col_widths) + 3*span - 1
                 * But we render 2 spaces (left+right pad), so: sum(col_widths) + 3*span - 1 - 2
                 * = sum(col_widths) + 3*span - 3 */
                if (span > 1) {
                    block_width += 3 * span - 3;
                }
                int target = block_width;
                int actual = 0;

                if (!tc->printed) {
                    /* First time rendering this owner: measure real content width */
                    actual = node_plain_text_width(tc->cell);
                    if (actual < 0) actual = 0;
                }

                /* Resolve effective alignment */
                term_align_t align = TERM_ALIGN_LEFT;
                if (col_align[c] != TERM_ALIGN_DEFAULT) {
                    align = col_align[c];
                }
                const char *attrs = (const char *)cmark_node_get_user_data(tc->cell);
                term_align_t cell_align = parse_alignment_from_attrs(attrs);
                if (cell_align != TERM_ALIGN_DEFAULT) {
                    align = cell_align;
                }

                int extra = target - actual;
                if (extra < 0) extra = 0;
                int left_extra = 0;
                int right_extra = 0;
                switch (align) {
                    case TERM_ALIGN_RIGHT:
                        left_extra = extra;
                        right_extra = 0;
                        break;
                    case TERM_ALIGN_CENTER:
                        left_extra = extra / 2;
                        right_extra = extra - left_extra;
                        break;
                    case TERM_ALIGN_LEFT:
                    default:
                        left_extra = 0;
                        right_extra = extra;
                        break;
                }
                int left_pad = 1 + left_extra;
                int right_pad = 1 + right_extra;

                for (int i = 0; i < left_pad; i++) {
                    buffer_append_str(buf, " ");
                }
                if (!tc->printed) {
                    if (is_header) {
                        apply_style_string(buf, "b", use_256_color);
                    }
                    for (cmark_node *child = cmark_node_first_child(tc->cell); child; child = cmark_node_next(child)) {
                        serialize_inline(buf, child, options, theme, use_256_color);
                    }
                    if (is_header) {
                        append_ansi_reset(buf);
                    }
                    tc->printed = true;
                }
                for (int i = 0; i < right_pad; i++) {
                    buffer_append_str(buf, " ");
                }
                if (border_style) {
                    apply_style_string(buf, border_style, use_256_color);
                }
                buffer_append_str(buf, v_line);
                if (border_style) {
                    append_ansi_reset(buf);
                }

                c += span;
            }

            buffer_append_str(buf, "\n");

            /* Header separator after first row */
            if (is_header) {
                indent_spaces(buf, indent_level);
                if (border_style) {
                    apply_style_string(buf, border_style, use_256_color);
                }
                buffer_append_str(buf, mid_left);
                for (int cc = 0; cc < visual_cols; cc++) {
                    int inner = col_widths[cc] > 0 ? col_widths[cc] : 1;
                    for (int i = 0; i < inner + 2; i++) {
                        buffer_append_str(buf, h_line);
                    }
                    if (cc == visual_cols - 1) {
                        buffer_append_str(buf, mid_right);
                    } else {
                        buffer_append_str(buf, mid_sep);
                    }
                }
                buffer_append_str(buf, "\n");
                if (border_style) {
                    append_ansi_reset(buf);
                }
            }

            row_index++;
        }

        /* Bottom border */
        indent_spaces(buf, indent_level);
        if (border_style) {
            apply_style_string(buf, border_style, use_256_color);
        }
        buffer_append_str(buf, bot_left);
        for (int c = 0; c < visual_cols; c++) {
            int inner = col_widths[c] > 0 ? col_widths[c] : 1;
            for (int i = 0; i < inner + 2; i++) {
                buffer_append_str(buf, h_line);
            }
            if (c == visual_cols - 1) {
                buffer_append_str(buf, bot_right);
            } else {
                buffer_append_str(buf, bot_sep);
            }
        }
        buffer_append_str(buf, "\n\n");
        if (border_style) {
            append_ansi_reset(buf);
        }

        free(col_align);
        free(col_widths);
        free(grid.cells);
        return;
    }

    switch (type) {
        case CMARK_NODE_DOCUMENT: {
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_block(buf, child, options, theme, use_256_color, indent_level);
            }
            break;
        }

        case CMARK_NODE_PARAGRAPH: {
            indent_spaces(buf, indent_level);
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_inline(buf, child, options, theme, use_256_color);
            }
            /* Compact paragraphs inside list items to a single newline */
            cmark_node *parent = cmark_node_parent(node);
            cmark_node_type ptype = parent ? cmark_node_get_type(parent) : 0;
            if (ptype == CMARK_NODE_ITEM || ptype == CMARK_NODE_LIST) {
                buffer_append_str(buf, "\n");
            } else {
                buffer_append_str(buf, "\n\n");
            }
            break;
        }

        case CMARK_NODE_HEADING: {
            int level = cmark_node_get_heading_level(node);
            const char *style = NULL;
            if (theme) {
                switch (level) {
                    case 1: style = theme->h1_color; break;
                    case 2: style = theme->h2_color; break;
                    case 3: style = theme->h3_color; break;
                    case 4: style = theme->h4_color; break;
                    case 5: style = theme->h5_color; break;
                    case 6: style = theme->h6_color; break;
                }
            }
            if (!style) {
                if (level == 1) style = "b intense_black on_white";
                else if (level == 2) style = "b white on_intense_black";
                else if (level == 3) style = "u b yellow";
                else style = "b white";
            }

            indent_spaces(buf, indent_level);
            apply_style_string(buf, style, use_256_color);
            const char *text_start = buf->buf ? buf->buf + buf->len : NULL;
            (void)text_start;
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_inline(buf, child, options, theme, use_256_color);
            }
            append_ansi_reset(buf);
            buffer_append_str(buf, "\n\n");
            break;
        }

        case CMARK_NODE_LIST: {
            cmark_list_type list_type = cmark_node_get_list_type(node);
            int start = cmark_node_get_list_start(node);
            if (start <= 0) start = 1;

            int index = 0;
            for (cmark_node *item = cmark_node_first_child(node); item; item = cmark_node_next(item)) {
                indent_spaces(buf, indent_level);
                if (list_type == CMARK_BULLET_LIST) {
                    apply_style_string(buf, "b intense_red", use_256_color);
                    buffer_append_str(buf, "* ");
                    append_ansi_reset(buf);
                } else {
                    char num[32];
                    snprintf(num, sizeof(num), "%d.", start + index);
                    apply_style_string(buf, "b intense_blue", use_256_color);
                    buffer_append_str(buf, num);
                    buffer_append_str(buf, " ");
                    append_ansi_reset(buf);
                }
                serialize_block(buf, item, options, theme, use_256_color, indent_level + 1);
                index++;
            }
            buffer_append_str(buf, "\n");
            break;
        }

        case CMARK_NODE_ITEM: {
            /* List item contents without additional marker (already printed) */
            cmark_node *child = cmark_node_first_child(node);
            while (child) {
                serialize_block(buf, child, options, theme, use_256_color, indent_level);
                child = cmark_node_next(child);
            }
            break;
        }

        case CMARK_NODE_BLOCK_QUOTE: {
            const char *marker = theme && theme->blockquote_marker ? theme->blockquote_marker : ">";
            const char *marker_style = "yellow";
            /* Text style: theme override, otherwise italic + light gray */
            const char *text_style = NULL;
            if (theme && theme->blockquote_color) {
                text_style = theme->blockquote_color;
            } else {
                /* Default: italic + light gray; fall back to plain white in 8-color mode */
                text_style = use_256_color ? "i 38;5;250" : "i white";
            }
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                cmark_node_type child_type = cmark_node_get_type(child);
                
                if (child_type == CMARK_NODE_PARAGRAPH) {
                    /* For paragraphs, serialize inline content directly on same line as > */
                    indent_spaces(buf, indent_level);
                    apply_style_string(buf, marker_style, use_256_color);
                    buffer_append_str(buf, marker);
                    buffer_append_str(buf, " ");
                    append_ansi_reset(buf);
                    
                    cmark_node *inline_child = cmark_node_first_child(child);

                    while (inline_child) {
                        cmark_node_type inline_type = cmark_node_get_type(inline_child);
                        
                        if (inline_type == CMARK_NODE_SOFTBREAK) {
                            /* For soft breaks in blockquotes, continue on next line with > */
                            buffer_append_str(buf, "\n");
                            indent_spaces(buf, indent_level);
                            apply_style_string(buf, marker_style, use_256_color);
                            buffer_append_str(buf, marker);
                            buffer_append_str(buf, " ");
                            append_ansi_reset(buf);
                        } else {
                            /* Apply text style (italic + light gray by default) */
                            apply_style_string(buf, text_style, use_256_color);
                            serialize_inline(buf, inline_child, options, theme, use_256_color);
                            append_ansi_reset(buf);
                        }
                        inline_child = cmark_node_next(inline_child);
                    }
                    
                    if (cmark_node_next(child)) {
                        buffer_append_str(buf, "\n");
                    } else {
                        buffer_append_str(buf, "\n\n");
                    }
                } else {
                    /* For other block types, prefix with > */
                    indent_spaces(buf, indent_level);
                    apply_style_string(buf, marker_style, use_256_color);
                    buffer_append_str(buf, marker);
                    buffer_append_str(buf, " ");
                    append_ansi_reset(buf);
                    serialize_block(buf, child, options, theme, use_256_color, indent_level);
                }
            }
            break;
        }


        case CMARK_NODE_CODE_BLOCK: {
            const char *info = cmark_node_get_fence_info(node);
            const char *literal = cmark_node_get_literal(node);

            /* Try external syntax highlighter when configured */
            bool highlighted = false;
            if (options && options->code_highlighter && literal && *literal) {
                const char *tool = options->code_highlighter;
                const char *binary = NULL;
                if (strcmp(tool, "pygments") == 0) {
                    binary = "pygmentize";
                }

                if (binary) {
                    /* Extract language from info string (up to first space) */
                    char lang[64] = {0};
                    if (info && *info) {
                        const char *p = info;
                        char *w = lang;
                        while (*p && !isspace((unsigned char)*p) && (size_t)(w - lang) < sizeof(lang) - 1) {
                            *w++ = *p++;
                        }
                        *w = '\0';
                    }

                    char cmd[512];
                    if (strcmp(tool, "pygments") == 0) {
                        /* Pygments: terminal / terminal256 output with pastel style */
                        const char *format = use_256_color ? "terminal256" : "terminal";
                        const char *style  = use_256_color ? "paraiso-dark" : "pastie";
                        if (lang[0]) {
                            snprintf(cmd, sizeof(cmd), "%s -l %s -f %s -O style=%s",
                                     binary, lang, format, style);
                        } else {
                            snprintf(cmd, sizeof(cmd), "%s -g -f %s -O style=%s",
                                     binary, format, style);
                        }
                    } else {
                        cmd[0] = '\0';
                    }

                    if (cmd[0]) {
                        /* Reuse run_command-style helper from syntax_highlight.c (local copy) */
                        int in_pipe[2];
                        int out_pipe[2];
                        if (pipe(in_pipe) == 0 && pipe(out_pipe) == 0) {
                            pid_t pid = fork();
                            if (pid == 0) {
                                /* Child */
                                dup2(in_pipe[0], STDIN_FILENO);
                                dup2(out_pipe[1], STDOUT_FILENO);
                                int devnull = open("/dev/null", O_WRONLY);
                                if (devnull != -1) {
                                    dup2(devnull, STDERR_FILENO);
                                    close(devnull);
                                }
                                close(in_pipe[0]); close(in_pipe[1]);
                                close(out_pipe[0]); close(out_pipe[1]);
                                execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
                                _exit(127);
                            } else if (pid > 0) {
                                /* Parent */
                                close(in_pipe[0]);
                                close(out_pipe[1]);

                                /* Write code to child stdin */
                                size_t input_len = strlen(literal);
                                ssize_t to_write = (ssize_t)input_len;
                                const char *p = literal;
                                while (to_write > 0) {
                                    ssize_t written = write(in_pipe[1], p, (size_t)to_write);
                                    if (written <= 0) break;
                                    p += written;
                                    to_write -= written;
                                }
                                close(in_pipe[1]);

                                /* Read all of child's stdout */
                                size_t cap = 8192;
                                size_t size = 0;
                                char *out = malloc(cap);
                                if (out) {
                                    for (;;) {
                                        if (size + 4096 > cap) {
                                            cap *= 2;
                                            char *nb = realloc(out, cap);
                                            if (!nb) {
                                                free(out);
                                                out = NULL;
                                                break;
                                            }
                                            out = nb;
                                        }
                                        ssize_t n = read(out_pipe[0], out + size, 4096);
                                        if (n < 0) {
                                            if (errno == EINTR) continue;
                                            free(out);
                                            out = NULL;
                                            break;
                                        }
                                        if (n == 0) break;
                                        size += (size_t)n;
                                    }
                                }
                                close(out_pipe[0]);

                                int status;
                                waitpid(pid, &status, 0);

                                if (out && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                                    out[size] = '\0';
                                    /* Emit highlighted ANSI directly */
                                    buffer_append_str(buf, out);
                                    /* Ensure a blank line after block */
                                    if (size == 0 || out[size - 1] != '\n') {
                                        buffer_append_str(buf, "\n");
                                    }
                                    buffer_append_str(buf, "\n");
                                    free(out);
                                    highlighted = true;
                                } else if (out) {
                                    free(out);
                                }
                            } else {
                                /* fork failed */
                                close(in_pipe[0]); close(in_pipe[1]);
                                close(out_pipe[0]); close(out_pipe[1]);
                            }
                        }
                    }
                }
            }

            if (!highlighted) {
                /* Fallback: render simple fenced block with theme colors */
                indent_spaces(buf, indent_level);
                if (theme && theme->code_block) {
                    apply_style_string(buf, theme->code_block, use_256_color);
                } else {
                    apply_style_string(buf, "white on_black", use_256_color);
                }
                buffer_append_str(buf, "```");
                if (info && *info) {
                    buffer_append_str(buf, info);
                }
                buffer_append_str(buf, "\n");
                if (literal) {
                    buffer_append_str(buf, literal);
                }
                buffer_append_str(buf, "\n```");
                append_ansi_reset(buf);
                buffer_append_str(buf, "\n\n");
            }
            break;
        }

        case CMARK_NODE_HTML_BLOCK: {
            const char *literal = cmark_node_get_literal(node);
            if (literal) {
                /* Handle definition lists: <dl><dt>Term</dt><dd>Definition</dd></dl> */
                if (strstr(literal, "<dl>") || strstr(literal, "<dt>") || strstr(literal, "<dd>")) {
                    const char *p = literal;
                    bool in_dt = false;
                    bool in_dd = false;
                    bool first_dd = true;
                    
                    while (*p) {
                        if (strncmp(p, "<dt>", 4) == 0) {
                            in_dt = true;
                            p += 4;
                            if (!first_dd) {
                                buffer_append_str(buf, "\n");
                            }
                            first_dd = false;
                            continue;
                        }
                        if (strncmp(p, "</dt>", 5) == 0) {
                            in_dt = false;
                            p += 5;
                            buffer_append_str(buf, "\n");
                            continue;
                        }
                        if (strncmp(p, "<dd>", 4) == 0) {
                            in_dd = true;
                            p += 4;
                            indent_spaces(buf, indent_level + 1);
                            continue;
                        }
                        if (strncmp(p, "</dd>", 5) == 0) {
                            in_dd = false;
                            p += 5;
                            buffer_append_str(buf, "\n");
                            continue;
                        }
                        if (strncmp(p, "<dl>", 4) == 0 || strncmp(p, "</dl>", 5) == 0) {
                            p += (p[1] == 'd' && p[2] == 'l') ? 4 : 5;
                            continue;
                        }
                        
                        if (in_dt || in_dd) {
                            if (*p == '\n' || *p == '\r') {
                                buffer_append_str(buf, " ");
                            } else if (*p != '<') {
                                buffer_append(buf, p, 1);
                            }
                        }
                        p++;
                    }
                    buffer_append_str(buf, "\n");
                }
                /* Handle callouts: <div class="callout callout-TYPE">...</div> */
                else if (strstr(literal, "callout")) {
                    const char *p = literal;
                    bool found_title = false;
                    
                    /* Extract callout type from class */
                    const char *type_start = strstr(p, "callout-");
                    if (type_start) {
                        type_start += 8; /* Skip "callout-" */
                        const char *type_end = type_start;
                        while (*type_end && *type_end != ' ' && *type_end != '"' && *type_end != '>') {
                            type_end++;
                        }
                        if (type_end > type_start) {
                            indent_spaces(buf, indent_level);
                            apply_style_string(buf, "b yellow", use_256_color);
                            buffer_append_str(buf, "[");
                            buffer_append(buf, type_start, (size_t)(type_end - type_start));
                            buffer_append_str(buf, "]");
                            append_ansi_reset(buf);
                            found_title = true;
                        }
                    }
                    
                    /* Extract title */
                    const char *title_start = strstr(p, "<summary>");
                    if (!title_start) title_start = strstr(p, "callout-title");
                    if (title_start) {
                        if (strstr(title_start, "<summary>")) {
                            title_start = strstr(title_start, "<summary>") + 9;
                        } else {
                            title_start = strstr(title_start, ">") + 1;
                        }
                        const char *title_end = strstr(title_start, "</");
                        if (title_end && title_end > title_start) {
                            if (found_title) {
                                buffer_append_str(buf, " ");
                            }
                            /* Strip HTML from title */
                            const char *t = title_start;
                            while (t < title_end) {
                                if (*t == '<') {
                                    while (t < title_end && *t != '>') t++;
                                    if (t < title_end) t++;
                                    continue;
                                }
                                buffer_append(buf, t, 1);
                                t++;
                            }
                            buffer_append_str(buf, "\n");
                        }
                    }
                    
                    /* Extract content */
                    const char *content_start = strstr(p, "callout-content");
                    if (content_start) {
                        content_start = strstr(content_start, ">") + 1;
                        const char *content_end = strstr(content_start, "</div>");
                        if (!content_end) content_end = strstr(content_start, "</details>");
                        if (content_end && content_end > content_start) {
                            /* Strip HTML and render content */
                            const char *c = content_start;
                            while (c < content_end) {
                                if (*c == '<') {
                                    const char *tag_end = strchr(c, '>');
                                    if (tag_end && tag_end < content_end) {
                                        c = tag_end + 1;
                                        continue;
                                    }
                                }
                                if (*c == '\n' || *c == '\r') {
                                    buffer_append_str(buf, " ");
                                } else {
                                    buffer_append(buf, c, 1);
                                }
                                c++;
                            }
                            buffer_append_str(buf, "\n\n");
                        }
                    }
                }
                /* Generic HTML: strip tags and render text */
                else {
                    const char *p = literal;
                    bool in_tag = false;
                    bool skip_whitespace = true;
                    
                    while (*p) {
                        if (*p == '<') {
                            in_tag = true;
                            p++;
                            continue;
                        }
                        if (in_tag) {
                            if (*p == '>') {
                                in_tag = false;
                                skip_whitespace = true;
                                p++;
                                continue;
                            }
                            p++;
                            continue;
                        }
                        
                        /* Skip leading whitespace after tags */
                        if (skip_whitespace && (*p == ' ' || *p == '\t' || *p == '\n')) {
                            p++;
                            continue;
                        }
                        skip_whitespace = false;
                        
                        /* Convert newlines to spaces, except for double newlines */
                        if (*p == '\n') {
                            if (p[1] == '\n' || p[1] == '\0') {
                                buffer_append_str(buf, "\n\n");
                                skip_whitespace = true;
                                p++;
                                if (*p == '\n') p++;
                                continue;
                            } else {
                                buffer_append_str(buf, " ");
                                p++;
                                continue;
                            }
                        }
                        
                        buffer_append(buf, p, 1);
                        p++;
                    }
                    
                    /* Ensure trailing newline */
                    if (buf->len > 0 && buf->buf[buf->len - 1] != '\n') {
                        buffer_append_str(buf, "\n\n");
                    } else if (buf->len > 1 && buf->buf[buf->len - 2] != '\n') {
                        buffer_append_str(buf, "\n");
                    }
                }
            }
            break;
        }

        case CMARK_NODE_THEMATIC_BREAK:
            indent_spaces(buf, indent_level);
            buffer_append_str(buf, "----------------------------------------\n\n");
            break;

        default: {
            /* Generic container: recurse into children */
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_block(buf, child, options, theme, use_256_color, indent_level);
            }
            break;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

char *apex_cmark_to_terminal(cmark_node *document,
                             const apex_options *options,
                             bool use_256) {
    if (!document || cmark_node_get_type(document) != CMARK_NODE_DOCUMENT) {
        return NULL;
    }

    terminal_buffer buf;
    buffer_init(&buf);

    terminal_theme *theme = load_theme(options);

    serialize_block(&buf, document, options, theme, use_256, 0);

    free_theme(theme);

    if (!buf.buf) {
        /* Fallback: empty string */
        char *empty = (char *)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    return buf.buf;
}

