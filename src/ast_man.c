/*
 * Man page output: roff (man page source) and styled HTML.
 * Renders cmark AST to .TH/.SH-style roff or a self-contained man-style HTML document.
 */

#include "apex/ast_man.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ------------------------------------------------------------------------- */
/* Buffer and roff escape                                                    */
/* ------------------------------------------------------------------------- */

typedef struct {
    char *buf;
    size_t len;
    size_t capacity;
} man_buffer;

static void man_buf_init(man_buffer *b) {
    b->buf = NULL;
    b->len = 0;
    b->capacity = 0;
}

static void man_buf_append(man_buffer *b, const char *str, size_t len) {
    if (!str || len == 0) return;
    if (b->len + len + 1 > b->capacity) {
        size_t new_cap = b->capacity ? b->capacity * 2 : 512;
        if (new_cap < b->len + len + 1) new_cap = b->len + len + 1;
        char *new_buf = (char *)realloc(b->buf, new_cap);
        if (!new_buf) return;
        b->buf = new_buf;
        b->capacity = new_cap;
    }
    memcpy(b->buf + b->len, str, len);
    b->len += len;
    b->buf[b->len] = '\0';
}

static void man_buf_append_str(man_buffer *b, const char *str) {
    if (str) man_buf_append(b, str, strlen(str));
}

/* Append text escaped for roff: \ -> \e, leading . or ' on a line -> \&. or \&' */
static void man_buf_append_roff_safe(man_buffer *b, const char *str, size_t len) {
    if (!str || len == 0) return;
    bool at_line_start = (b->len == 0 || (b->len > 0 && b->buf[b->len - 1] == '\n'));
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c == '\\') {
            man_buf_append_str(b, "\\e");
            at_line_start = false;
        } else if (c == '\n') {
            man_buf_append(b, "\n", 1);
            at_line_start = true;
        } else if (at_line_start && (c == '.' || c == '\'')) {
            man_buf_append_str(b, "\\&");
            man_buf_append(b, (const char *)&c, 1);
            at_line_start = false;
        } else {
            man_buf_append(b, (const char *)&c, 1);
            at_line_start = false;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Helpers: get plain text from a node (for .TH title)                       */
/* ------------------------------------------------------------------------- */

static void collect_plain_text(cmark_node *node, man_buffer *out) {
    if (!node) return;
    cmark_node_type t = cmark_node_get_type(node);
    if (t == CMARK_NODE_TEXT || t == CMARK_NODE_CODE) {
        const char *lit = cmark_node_get_literal(node);
        if (lit) man_buf_append(out, lit, strlen(lit));
        return;
    }
    for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur)) {
        collect_plain_text(cur, out);
    }
}

/* Caller frees. Returns first H1 heading text or NULL. */
static char *get_first_h1_text(cmark_node *document) {
    if (!document || cmark_node_get_type(document) != CMARK_NODE_DOCUMENT) return NULL;
    for (cmark_node *cur = cmark_node_first_child(document); cur; cur = cmark_node_next(cur)) {
        if (cmark_node_get_type(cur) == CMARK_NODE_HEADING && cmark_node_get_heading_level(cur) == 1) {
            man_buffer b;
            man_buf_init(&b);
            collect_plain_text(cur, &b);
            if (b.len > 0 && b.buf) {
                char *s = strdup(b.buf);
                free(b.buf);
                return s;
            }
            if (b.buf) free(b.buf);
            return NULL;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------------- */
/* Inline roff rendering                                                     */
/* ------------------------------------------------------------------------- */

static void render_inline_roff(man_buffer *buf, cmark_node *node);

static void render_inline_roff(man_buffer *buf, cmark_node *node) {
    if (!node) return;
    cmark_node_type t = cmark_node_get_type(node);
    switch (t) {
        case CMARK_NODE_TEXT: {
            const char *lit = cmark_node_get_literal(node);
            if (lit) man_buf_append_roff_safe(buf, lit, strlen(lit));
            break;
        }
        case CMARK_NODE_CODE: {
            man_buf_append_str(buf, "\\f[C]");
            const char *lit = cmark_node_get_literal(node);
            if (lit) man_buf_append_roff_safe(buf, lit, strlen(lit));
            man_buf_append_str(buf, "\\f[]");
            break;
        }
        case CMARK_NODE_LINEBREAK:
            man_buf_append_str(buf, "\n.br\n");
            break;
        case CMARK_NODE_SOFTBREAK:
            man_buf_append_str(buf, " ");
            break;
        case CMARK_NODE_STRONG:
            man_buf_append_str(buf, "\\f[B]");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_roff(buf, c);
            man_buf_append_str(buf, "\\f[]");
            break;
        case CMARK_NODE_EMPH:
            man_buf_append_str(buf, "\\f[I]");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_roff(buf, c);
            man_buf_append_str(buf, "\\f[]");
            break;
        case CMARK_NODE_LINK: {
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_roff(buf, c);
            const char *url = cmark_node_get_url(node);
            if (url && url[0]) {
                man_buf_append_str(buf, " (");
                man_buf_append_roff_safe(buf, url, strlen(url));
                man_buf_append_str(buf, ")");
            }
            break;
        }
        case CMARK_NODE_HTML_INLINE:
            /* skip */
            break;
        default:
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_roff(buf, c);
            break;
    }
}

/* ------------------------------------------------------------------------- */
/* Block roff rendering                                                     */
/* ------------------------------------------------------------------------- */

static void render_block_roff(man_buffer *buf, cmark_node *node);

static void render_block_roff(man_buffer *buf, cmark_node *node) {
    if (!node) return;
    cmark_node_type t = cmark_node_get_type(node);
    switch (t) {
        case CMARK_NODE_DOCUMENT:
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_roff(buf, cur);
            break;
        case CMARK_NODE_HEADING: {
            int level = cmark_node_get_heading_level(node);
            man_buf_append_str(buf, level == 1 ? "\n.SH " : "\n.SS ");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_roff(buf, c);
            man_buf_append_str(buf, "\n");
            break;
        }
        case CMARK_NODE_PARAGRAPH: {
            cmark_node *parent = cmark_node_parent(node);
            bool in_item = (parent && cmark_node_get_type(parent) == CMARK_NODE_ITEM && !cmark_node_previous(node));
            if (!in_item) {
                man_buf_append_str(buf, "\n.PP\n");
            }
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_roff(buf, c);
            man_buf_append_str(buf, "\n");
            break;
        }
        case CMARK_NODE_LIST:
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_roff(buf, cur);
            break;
        case CMARK_NODE_ITEM: {
            cmark_node *list = cmark_node_parent(node);
            if (list && cmark_node_get_list_type(list) == CMARK_BULLET_LIST) {
                man_buf_append_str(buf, "\n.IP \\(bu 2\n");
            } else {
                int idx = cmark_node_get_item_index(node);
                char num[32];
                snprintf(num, sizeof(num), "\n.IP \"%d.\" 4\n", idx);
                man_buf_append_str(buf, num);
            }
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_roff(buf, cur);
            break;
        }
        case CMARK_NODE_CODE_BLOCK: {
            man_buf_append_str(buf, "\n.PP\n.nf\n\\f[C]\n");
            const char *lit = cmark_node_get_literal(node);
            if (lit) man_buf_append_roff_safe(buf, lit, strlen(lit));
            man_buf_append_str(buf, "\n\\f[]\n.fi\n");
            break;
        }
        case CMARK_NODE_BLOCK_QUOTE:
            man_buf_append_str(buf, "\n.RS\n");
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_roff(buf, cur);
            man_buf_append_str(buf, "\n.RE\n");
            break;
        case CMARK_NODE_THEMATIC_BREAK:
            man_buf_append_str(buf, "\n.PP\n  *  *  *  *  *\n");
            break;
        case CMARK_NODE_HTML_BLOCK:
            /* skip */
            break;
        default:
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_roff(buf, cur);
            break;
    }
}

/* ------------------------------------------------------------------------- */
/* Public API: roff                                                          */
/* ------------------------------------------------------------------------- */

char *apex_cmark_to_man_roff(cmark_node *document, const apex_options *options)
{
    (void)options;
    if (!document) return strdup(".TH stub 1 \"\" \"\"\n");

    const char *title = "Document";
    char *first_h1 = get_first_h1_text(document);
    if (first_h1 && first_h1[0]) {
        title = first_h1;
    }

    const char *section = "1";
    const char *date = "1 January 1970";
    const char *source = "";

    man_buffer buf;
    man_buf_init(&buf);
    /* .TH title section date source - all args quoted if they contain spaces */
    man_buf_append_str(&buf, ".TH \"");
    man_buf_append_roff_safe(&buf, title, strlen(title));
    man_buf_append_str(&buf, "\" \"");
    man_buf_append_str(&buf, section);
    man_buf_append_str(&buf, "\" \"");
    man_buf_append_str(&buf, date);
    man_buf_append_str(&buf, "\" \"");
    man_buf_append_str(&buf, source);
    man_buf_append_str(&buf, "\"\n");

    render_block_roff(&buf, document);

    if (first_h1) free(first_h1);

    if (!buf.buf) return strdup(".TH stub 1 \"\" \"\"\n");
    return buf.buf;
}

/* ------------------------------------------------------------------------- */
/* Man-HTML: styled HTML man page                                            */
/* ------------------------------------------------------------------------- */

/* Append string with HTML entities escaped: & < > " ' */
static void man_buf_append_html_escaped(man_buffer *b, const char *str, size_t len) {
    if (!str || len == 0) return;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c == '&') man_buf_append_str(b, "&amp;");
        else if (c == '<') man_buf_append_str(b, "&lt;");
        else if (c == '>') man_buf_append_str(b, "&gt;");
        else if (c == '"') man_buf_append_str(b, "&quot;");
        else if (c == '\'') man_buf_append_str(b, "&#39;");
        else man_buf_append(b, (const char *)&c, 1);
    }
}

static const char *man_html_css =
    "body { font-family: 'Helvetica Neue', Helvetica, Arial, sans-serif; max-width: 65em; margin: 1em auto; padding: 0 1em; line-height: 1.4; }\n"
    ".man-title { font-weight: bold; margin-bottom: 0.5em; }\n"
    ".man-section { font-weight: bold; margin-top: 1em; margin-bottom: 0.25em; }\n"
    ".man-section h2 { font-size: 1em; margin: 0; }\n"
    "p { margin: 0.5em 0; }\n"
    "code, .man-option { font-family: monospace; background: #f5f5f5; padding: 0 0.2em; }\n"
    "pre { background: #f5f5f5; padding: 0.75em; overflow-x: auto; }\n"
    "ul, ol { margin: 0.5em 0; padding-left: 1.5em; }\n"
    "a { color: #0066cc; }\n";

static void render_inline_man_html(man_buffer *buf, cmark_node *node);
static void render_block_man_html(man_buffer *buf, cmark_node *node);

static void render_inline_man_html(man_buffer *buf, cmark_node *node) {
    if (!node) return;
    cmark_node_type t = cmark_node_get_type(node);
    switch (t) {
        case CMARK_NODE_TEXT: {
            const char *lit = cmark_node_get_literal(node);
            if (lit) man_buf_append_html_escaped(buf, lit, strlen(lit));
            break;
        }
        case CMARK_NODE_CODE:
            man_buf_append_str(buf, "<code>");
            if (cmark_node_get_literal(node))
                man_buf_append_html_escaped(buf, cmark_node_get_literal(node), strlen(cmark_node_get_literal(node)));
            man_buf_append_str(buf, "</code>");
            break;
        case CMARK_NODE_LINEBREAK:
            man_buf_append_str(buf, "<br>\n");
            break;
        case CMARK_NODE_SOFTBREAK:
            man_buf_append_str(buf, " ");
            break;
        case CMARK_NODE_STRONG:
            man_buf_append_str(buf, "<strong>");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_man_html(buf, c);
            man_buf_append_str(buf, "</strong>");
            break;
        case CMARK_NODE_EMPH:
            man_buf_append_str(buf, "<em>");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_man_html(buf, c);
            man_buf_append_str(buf, "</em>");
            break;
        case CMARK_NODE_LINK: {
            const char *url = cmark_node_get_url(node);
            if (url && url[0]) {
                man_buf_append_str(buf, "<a href=\"");
                man_buf_append_html_escaped(buf, url, strlen(url));
                man_buf_append_str(buf, "\">");
            }
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_man_html(buf, c);
            if (url && url[0]) man_buf_append_str(buf, "</a>");
            break;
        }
        case CMARK_NODE_HTML_INLINE:
            break;
        default:
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_man_html(buf, c);
            break;
    }
}

static void section_id_from_heading(cmark_node *node, man_buffer *buf) {
    for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c)) {
        if (cmark_node_get_type(c) == CMARK_NODE_TEXT) {
            const char *lit = cmark_node_get_literal(c);
            if (lit) {
                for (const char *p = lit; *p; p++) {
                    unsigned char ch = (unsigned char)*p;
                    if (ch == ' ' || ch == '\t') man_buf_append(buf, "-", 1);
                    else if (isalnum(ch) || ch == '-') man_buf_append(buf, (const char *)&ch, 1);
                }
            }
            break;
        }
        section_id_from_heading(c, buf);
    }
}

static void render_block_man_html(man_buffer *buf, cmark_node *node) {
    if (!node) return;
    cmark_node_type t = cmark_node_get_type(node);
    switch (t) {
        case CMARK_NODE_DOCUMENT:
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_man_html(buf, cur);
            break;
        case CMARK_NODE_HEADING: {
            int level = cmark_node_get_heading_level(node);
            int h = level + 1; /* h2 for level 1, h3 for level 2 */
            if (h > 4) h = 4;
            char tag[8];
            snprintf(tag, sizeof(tag), "h%d", h);
            man_buf_append_str(buf, "\n<div class=\"man-section\"><");
            man_buf_append(buf, tag, strlen(tag));
            man_buf_append_str(buf, " id=\"");
            section_id_from_heading(node, buf);
            man_buf_append_str(buf, "\">");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_man_html(buf, c);
            man_buf_append_str(buf, "</");
            man_buf_append(buf, tag, strlen(tag));
            man_buf_append_str(buf, "></div>\n");
            break;
        }
        case CMARK_NODE_PARAGRAPH: {
            cmark_node *parent = cmark_node_parent(node);
            bool in_item = (parent && cmark_node_get_type(parent) == CMARK_NODE_ITEM && !cmark_node_previous(node));
            if (!in_item) man_buf_append_str(buf, "<p>");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_man_html(buf, c);
            if (!in_item) man_buf_append_str(buf, "</p>\n");
            else man_buf_append_str(buf, "\n");
            break;
        }
        case CMARK_NODE_LIST: {
            cmark_list_type list_type = cmark_node_get_list_type(node);
            man_buf_append_str(buf, list_type == CMARK_ORDERED_LIST ? "\n<ol>\n" : "\n<ul>\n");
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_man_html(buf, cur);
            man_buf_append_str(buf, list_type == CMARK_ORDERED_LIST ? "</ol>\n" : "</ul>\n");
            break;
        }
        case CMARK_NODE_ITEM:
            man_buf_append_str(buf, "<li>");
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_man_html(buf, cur);
            man_buf_append_str(buf, "</li>\n");
            break;
        case CMARK_NODE_CODE_BLOCK:
            man_buf_append_str(buf, "\n<pre><code>");
            if (cmark_node_get_literal(node))
                man_buf_append_html_escaped(buf, cmark_node_get_literal(node), strlen(cmark_node_get_literal(node)));
            man_buf_append_str(buf, "</code></pre>\n");
            break;
        case CMARK_NODE_BLOCK_QUOTE:
            man_buf_append_str(buf, "\n<blockquote>\n");
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_man_html(buf, cur);
            man_buf_append_str(buf, "</blockquote>\n");
            break;
        case CMARK_NODE_THEMATIC_BREAK:
            man_buf_append_str(buf, "\n<hr>\n");
            break;
        case CMARK_NODE_HTML_BLOCK:
            break;
        default:
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_man_html(buf, cur);
            break;
    }
}

char *apex_cmark_to_man_html(cmark_node *document, const apex_options *options)
{
    (void)options;
    if (!document) return strdup("<!DOCTYPE html><html><body><p>stub</p></body></html>");

    const char *title = "Document";
    char *first_h1 = get_first_h1_text(document);
    if (first_h1 && first_h1[0]) title = first_h1;

    man_buffer buf;
    man_buf_init(&buf);
    man_buf_append_str(&buf, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n<title>");
    man_buf_append_html_escaped(&buf, title, strlen(title));
    man_buf_append_str(&buf, "</title>\n<style>\n");
    man_buf_append_str(&buf, man_html_css);
    man_buf_append_str(&buf, "</style>\n</head>\n<body>\n<div class=\"man-title\">");
    man_buf_append_html_escaped(&buf, title, strlen(title));
    man_buf_append_str(&buf, "</div>\n");
    render_block_man_html(&buf, document);
    man_buf_append_str(&buf, "\n</body>\n</html>");

    if (first_h1) free(first_h1);
    if (!buf.buf) return strdup("<!DOCTYPE html><html><body><p>stub</p></body></html>");
    return buf.buf;
}
