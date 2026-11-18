/**
 * Definition List Extension for Apex
 *
 * Supports four formats (all produce <dl><dt>term</dt><dd>definition</dd></dl>):
 *
 * 1. Kramdown single colon:
 *    term
 *    : definition
 *
 * 2. Kramdown double colon:
 *    term
 *    :: definition
 *
 * 3. One-line no space: term::definition
 *
 * 4. One-line with space: term :: definition
 *
 * For one-line format, :: must NOT be at line start (that's Kramdown).
 * Whitespace around :: is allowed in one-line format.
 *
 * Both formats enabled by default in unified mode.
 */

#include "definition_list.h"
#include "ial.h"
#include "parser.h"
#include <ctype.h>
#include "node.h"
#include "html.h"
#include "render.h"
#include "../html_renderer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/**
 * Check if a line matches the one-line definition format: Term :: Definition
 * The line must contain :: with optional whitespace around it.
 * Uses the last :: to avoid splitting URLs (e.g. http://example.com).
 * Returns the position of :: or -1 if not a match.
 */
static int find_def_separator(const unsigned char *line, int len) {
    if (!line || len < 3) return -1;

    int last_sep = -1;
    for (int i = 0; i < len - 1; i++) {
        if (line[i] == ':' && line[i + 1] == ':') {
            /* Skip :: that's part of URL (://) */
            if (i + 3 <= len && line[i + 2] == '/') continue;
            /* Skip ::: or more (div/custom element fence) */
            if (i > 0 && line[i - 1] == ':') continue;
            /* Skip Leanpub {::marker /} syntax */
            if (i > 0 && line[i - 1] == '{') continue;
            if (i + 2 < len && line[i + 2] == ':') continue;
            last_sep = i;
        }
    }
    if (last_sep < 0) return -1;

    /* Ensure we have content before (at least one non-space) */
    int before = 0;
    for (int j = 0; j < last_sep; j++) {
        if (line[j] != ' ' && line[j] != '\t') {
            before = 1;
            break;
        }
    }
    /* After: at least one character */
    int after = (last_sep + 2 < len);
    if (before && after) return last_sep;
    return -1;
}

/**
 * True for a CommonMark link reference definition (`[label]: destination`).
 * An inline link such as `[Masoc](https://example.com)` also contains a colon,
 * but the colon is inside the destination, not the `[label]:` marker.
 */
static bool line_is_link_reference_definition(const char *line, const char *line_end) {
    const char *p = line;
    while (p < line_end && (*p == ' ' || *p == '\t')) p++;
    if (p >= line_end || *p != '[') return false;

    for (const char *q = p + 1; q < line_end; q++) {
        if (*q != ']') continue;
        const char *r = q + 1;
        while (r < line_end && (*r == ' ' || *r == '\t')) r++;
        if (r < line_end && *r == '(') return false;
        if (r < line_end && *r == ':') return true;
    }
    return false;
}

/**
 * Check if a line is a Kramdown-style definition line (starts with : or :: after optional spaces).
 * Reject ::: or more - those are div/custom element fences, not definition lists.
 */
static bool is_kramdown_def_line(const char *line, size_t len) {
    if (!line || len == 0) return false;
    size_t i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
    if (i >= len) return false;
    if (line[i] != ':') return false;
    int colon_len = 1;
    if (i + 2 <= len && line[i + 1] == ':') colon_len = 2;
    /* Reject 3+ colons (::: is div fence) */
    if (i + 3 <= len && line[i + 2] == ':') return false;
    if (i + (size_t)colon_len >= len) return false;
    if (line[i + colon_len] != ' ' && line[i + colon_len] != '\t') return false;
    return true;
}

/**
 * Check if a line looks like a table row (starts with | after optional indent).
 * Used to avoid treating : Caption as a definition when it's a table caption.
 */
static bool is_table_row_line(const char *line, size_t len) {
    if (!line || len == 0) return false;
    size_t i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
    return i < len && line[i] == '|';
}

/**
 * Check if the next non-blank line after pos is a table row. Used for "caption before table".
 */
static bool next_nonblank_line_is_table(const char *pos, const char *text_end) {
    while (pos < text_end) {
        if (*pos == '\n') { pos++; continue; }
        const char *line_end = strchr(pos, '\n');
        if (!line_end) line_end = text_end;
        const char *p = pos;
        while (p < line_end && (*p == ' ' || *p == '\t')) p++;
        if (p < line_end) return *p == '|';
        pos = line_end + (line_end < text_end && *line_end == '\n' ? 1 : 0);
    }
    return false;
}

/** True if content at p looks like a list marker (- , * , + , or digit+. ) */
static bool looks_like_list_marker(const char *p) {
    if (*p == '-' || *p == '*' || *p == '+')
        return (p[1] == ' ' || p[1] == '\t');
    if (isdigit((unsigned char)*p)) {
        while (isdigit((unsigned char)*p)) p++;
        return (*p == '.' && (p[1] == ' ' || p[1] == '\t'));
    }
    return false;
}

/**
 * True if line is an indented code block (4+ spaces or tab at start, not a list line).
 * Used to skip definition list processing inside indented code blocks.
 */
static bool line_is_indented_code_block(const char *line, size_t len) {
    if (len == 0) return false;
    if (line[0] == '\t') {
        return len > 1 && !looks_like_list_marker(line + 1);
    }
    if (len < 4 || line[0] != ' ' || line[1] != ' ' || line[2] != ' ' || line[3] != ' ')
        return false;
    const char *content = line + 4;
    while (content < line + len && *content == ' ') content++;
    return (content < line + len) && !looks_like_list_marker(content);
}

/** True if :: at scan is part of Leanpub {::marker /} syntax, not a definition list. */
static bool is_leanpub_marker_colons(const char *line_start, const char *colon_pos) {
    return colon_pos > line_start && colon_pos[-1] == '{';
}

/** True if line is an ATX Markdown heading (# .. ######). */
static bool is_atx_heading_line(const char *line, size_t len) {
    size_t i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
    if (i >= len || line[i] != '#') return false;
    int hashes = 0;
    while (i < len && line[i] == '#') {
        hashes++;
        i++;
    }
    if (hashes < 1 || hashes > 6) return false;
    return i >= len || line[i] == ' ' || line[i] == '\t';
}

/**
 * Scans line for inline code backticks, updates state for next line, and returns
 * whether sep_pos is inside an inline code span. Single backticks toggle; 3+ are
 * fenced blocks (handled elsewhere). Used to skip definition processing inside
 * inline code spans, including multi-line spans like `term::def\n :more:`.
 */
static bool scan_inline_code_for_sep(const char *line, size_t len, int sep_pos,
                                     bool in_span_at_start, bool *out_in_span_at_end) {
    bool in = in_span_at_start;
    bool sep_inside = false;
    for (size_t i = 0; i < len; i++) {
        if ((int)i == sep_pos) sep_inside = in;
        if (line[i] == '`') {
            int count = 1;
            while (i + (size_t)count < len && line[i + count] == '`') count++;
            if (count == 1) in = !in;
            i += (size_t)(count - 1);
        }
    }
    *out_in_span_at_end = in;
    return (sep_pos >= 0 && (size_t)sep_pos < len) ? sep_inside : false;
}

/**
 * Check if we're inside a fenced code block (```) - don't process definition lists there
 */
static bool is_code_fence_line(const char *line, size_t len) {
    const char *p = line;
    while (p < line + len && (*p == ' ' || *p == '\t')) p++;
    if (p + 3 <= line + len && p[0] == '`' && p[1] == '`' && p[2] == '`') {
        return true;
    }
    if (p + 3 <= line + len && p[0] == '~' && p[1] == '~' && p[2] == '~') {
        return true;
    }
    return false;
}

/* CommonMark HTML block type 1. Contents are not Markdown, so `::before`
 * in CSS must not become a one-line definition. */
static bool raw_tag_name_at(const char *p, const char *end, const char *name) {
    size_t n = strlen(name);
    if ((size_t)(end - p) < n) return false;
    for (size_t i = 0; i < n; i++) {
        char c = p[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c != name[i]) return false;
    }
    if ((size_t)(end - p) == n) return true;
    char next = p[n];
    return next == '>' || next == ' ' || next == '\t' || next == '\r' ||
           next == '\n' || next == '/';
}

/* Closing tag for a style/script/pre/textarea block that starts this line,
 * or NULL. Up to three leading spaces, matching CommonMark. */
static const char *raw_html_block_close(const char *line, size_t len) {
    const char *p = line;
    const char *end = line + len;
    int spaces = 0;
    while (p < end && *p == ' ') {
        spaces++;
        if (spaces > 3) return NULL;
        p++;
    }
    if (p >= end || *p != '<') return NULL;
    p++;
    if (raw_tag_name_at(p, end, "script")) return "</script>";
    if (raw_tag_name_at(p, end, "pre")) return "</pre>";
    if (raw_tag_name_at(p, end, "style")) return "</style>";
    if (raw_tag_name_at(p, end, "textarea")) return "</textarea>";
    return NULL;
}

static bool line_contains_ci(const char *line, size_t len, const char *token) {
    size_t tlen = strlen(token);
    if (tlen == 0 || len < tlen) return false;
    for (size_t i = 0; i + tlen <= len; i++) {
        size_t j = 0;
        for (; j < tlen; j++) {
            char a = line[i + j];
            char b = token[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
        }
        if (j == tlen) return true;
    }
    return false;
}

/**
 * Lines that clearly start a new Markdown block should not be buffered as
 * potential definition-list terms (avoids eating blanks / mis-ordering fences).
 */
static bool is_block_start_line(const char *line, size_t len) {
    const char *p = line;
    const char *end = line + len;
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    if (p >= end) return false;

    if (*p == '>' || *p == '#' || *p == '|' || *p == '<') return true;

    /* Unordered list markers */
    if ((*p == '-' || *p == '*' || *p == '+') &&
        (p + 1 >= end || p[1] == ' ' || p[1] == '\t')) {
        return true;
    }

    /* Ordered list markers: 1. / 1) */
    if (isdigit((unsigned char)*p)) {
        const char *q = p;
        while (q < end && isdigit((unsigned char)*q)) q++;
        if (q < end && (*q == '.' || *q == ')') &&
            (q + 1 >= end || q[1] == ' ' || q[1] == '\t')) {
            return true;
        }
    }

    return false;
}

/**
 * Render inline content (term or definition) with full document context so cmark
 * can resolve reference links. Parses full_doc + "\n\n" + content so ref defs are
 * available. Returns HTML of the last block (our content), stripping <p></p>.
 * Caller must free the returned string.
 */
static char *render_inline_with_doc(const char *content, size_t content_len,
                                    const char *full_doc, size_t full_doc_len, bool unsafe) {
    size_t buf_len = full_doc_len + 2 + content_len + 1;
    char *buf = malloc(buf_len);
    if (!buf) return NULL;
    memcpy(buf, full_doc, full_doc_len);
    buf[full_doc_len] = '\n';
    buf[full_doc_len + 1] = '\n';
    memcpy(buf + full_doc_len + 2, content, content_len);
    buf[buf_len - 1] = '\0';

    int opts = CMARK_OPT_DEFAULT | CMARK_OPT_SMART;
    if (unsafe) opts |= CMARK_OPT_UNSAFE | CMARK_OPT_LIBERAL_HTML_TAG;
    cmark_parser *cp = cmark_parser_new(opts);
    if (!cp) { free(buf); return NULL; }
    cmark_parser_feed(cp, buf, (int)(buf_len - 1));
    free(buf);
    cmark_node *doc = cmark_parser_finish(cp);
    cmark_parser_free(cp);
    if (!doc) return NULL;

    cmark_node *last = cmark_node_last_child(doc);
    if (!last) {
        cmark_node_free(doc);
        return NULL;
    }
    /* Terms and definitions are rendered before the document IAL pass, so
     * apply span attributes here or {:.class} stays literal text in the <dt>. */
    if (memchr(content, '{', content_len) != NULL) {
        apex_process_ial_in_tree(doc, NULL);
    }
    char *html = apex_render_html_with_attributes(last, opts);
    cmark_node_free(doc);
    if (!html) return NULL;

    /* Strip <p> and </p> wrapper, return inner content */
    char *content_start = html;
    if (strncmp(html, "<p>", 3) == 0) content_start = html + 3;
    size_t html_len = strlen(content_start);
    if (html_len > 5 && strcmp(content_start + html_len - 5, "</p>\n") == 0)
        html_len -= 5;
    else if (html_len > 4 && strcmp(content_start + html_len - 4, "</p>") == 0)
        html_len -= 4;
    char *result = malloc(html_len + 1);
    if (result) {
        memcpy(result, content_start, html_len);
        result[html_len] = '\0';
    }
    free(html);
    return result;
}

/* Footnote markers inside a definition are rendered before the main parse,
 * so [^id] would stay literal and the note would be dropped. Shield the
 * marker, leave a real reference for the main parser, then splice it back. */
static bool parse_fn_token(const char *s, const char *prefix,
                           const char **id_out, size_t *id_len, size_t *token_len) {
    size_t prefix_len = strlen(prefix);
    if (strncmp(s, prefix, prefix_len) != 0) return false;
    const char *p = s + prefix_len;
    char *end = NULL;
    long n = strtol(p, &end, 10);
    if (end == p || *end != ':' || n < 0) return false;
    const char *id = end + 1;
    if ((size_t)n > strlen(id)) return false;
    if (id[n] != '%' || id[n + 1] != '%') return false;
    *id_out = id;
    *id_len = (size_t)n;
    *token_len = (size_t)((id + n + 2) - s);
    return true;
}

static void fn_buf_append(char **buf, size_t *len, size_t *cap, const char *data, size_t n) {
    if (*len + n + 1 > *cap) {
        size_t next = (*cap < 64) ? 64 : *cap * 2;
        while (*len + n + 1 > next) next *= 2;
        char *grown = realloc(*buf, next);
        if (!grown) return;
        *buf = grown;
        *cap = next;
    }
    memcpy(*buf + *len, data, n);
    *len += n;
    (*buf)[*len] = '\0';
}

static void fn_stash_add(char **stash, size_t *len, size_t *cap, const char *id, size_t id_len) {
    char prefix[64];
    int plen = snprintf(prefix, sizeof(prefix), "%%%%APEXFNSTASH:%zu:", id_len);
    if (plen < 0) return;
    fn_buf_append(stash, len, cap, prefix, (size_t)plen);
    fn_buf_append(stash, len, cap, id, id_len);
    fn_buf_append(stash, len, cap, "%%[^", 4);
    fn_buf_append(stash, len, cap, id, id_len);
    fn_buf_append(stash, len, cap, "]", 1);
}

static bool at_line_start(const char *s, size_t i) {
    return i == 0 || s[i - 1] == '\n';
}

/* A fence line. Closing fences may contain only trailing space. */
static bool fence_marker_at(const char *s, size_t len, size_t i, bool closing,
                            char *ch_out, int *n_out) {
    size_t j = i;
    int spaces = 0;
    while (j < len && s[j] == ' ' && spaces < 4) {
        j++;
        spaces++;
    }
    if (spaces > 3 || j >= len) return false;
    char ch = s[j];
    if (ch != '`' && ch != '~') return false;
    int n = 0;
    while (j + (size_t)n < len && s[j + (size_t)n] == ch) n++;
    if (n < 3) return false;
    size_t k = j + (size_t)n;
    if (closing) {
        while (k < len && s[k] != '\n') {
            if (s[k] != ' ' && s[k] != '\t' && s[k] != '\r') return false;
            k++;
        }
    } else if (ch == '`') {
        while (k < len && s[k] != '\n') {
            if (s[k] == '`') return false;
            k++;
        }
    }
    if (ch_out) *ch_out = ch;
    if (n_out) *n_out = n;
    return true;
}

static bool footnote_definition_line_at(const char *s, size_t len, size_t bracket, size_t bracket_end) {
    if (bracket_end >= len || s[bracket_end] != ':') return false;
    size_t line = bracket;
    while (line > 0 && s[line - 1] != '\n') line--;
    size_t p = line;
    int spaces = 0;
    while (p < bracket && s[p] == ' ' && spaces < 4) {
        p++;
        spaces++;
    }
    return p == bracket;
}

/* Replace [^id] outside code with a placeholder. Append a real
 * reference to stash so the main parser still builds the note. */
char *apex_shield_footnote_refs(const char *content, size_t len, bool in_code,
                                char **stash, size_t *stash_len, size_t *stash_cap) {
    if (!content || len == 0 || !memchr(content, '[', len)) return NULL;

    bool found = false;
    char *out = NULL;
    size_t out_len = 0;
    size_t out_cap = 0;
    bool code = in_code;
    bool in_fence = false;
    char fence_char = 0;
    int fence_len = 0;

    for (size_t i = 0; i < len; i++) {
        if (!code && at_line_start(content, i)) {
            char ch = 0;
            int n = 0;
            if (!in_fence && fence_marker_at(content, len, i, false, &ch, &n)) {
                in_fence = true;
                fence_char = ch;
                fence_len = n;
            } else if (in_fence && fence_marker_at(content, len, i, true, &ch, &n) &&
                       ch == fence_char && n >= fence_len) {
                in_fence = false;
            }
        }
        if (in_fence) {
            fn_buf_append(&out, &out_len, &out_cap, content + i, 1);
            continue;
        }
        if (content[i] == '`') {
            int count = 1;
            while (i + (size_t)count < len && content[i + (size_t)count] == '`') count++;
            if (count == 1) code = !code;
            fn_buf_append(&out, &out_len, &out_cap, content + i, (size_t)count);
            i += (size_t)(count - 1);
            continue;
        }
        if (!code && content[i] == '[' && i + 2 < len && content[i + 1] == '^') {
            size_t id_start = i + 2;
            size_t id_end = id_start;
            while (id_end < len && content[id_end] != ']' && content[id_end] != '\n') id_end++;
            if (id_end < len && content[id_end] == ']' && id_end > id_start &&
                (id_end + 1 >= len || (content[id_end + 1] != '(' && content[id_end + 1] != '[')) &&
                !footnote_definition_line_at(content, len, i, id_end + 1)) {
                size_t id_len = id_end - id_start;
                char prefix[64];
                int plen = snprintf(prefix, sizeof(prefix), "%%%%APEXFN:%zu:", id_len);
                if (plen > 0) {
                    fn_buf_append(&out, &out_len, &out_cap, prefix, (size_t)plen);
                    fn_buf_append(&out, &out_len, &out_cap, content + id_start, id_len);
                    fn_buf_append(&out, &out_len, &out_cap, "%%", 2);
                    fn_stash_add(stash, stash_len, stash_cap, content + id_start, id_len);
                    found = true;
                    i = id_end;
                    continue;
                }
            }
        }
        fn_buf_append(&out, &out_len, &out_cap, content + i, 1);
    }

    if (!found) {
        free(out);
        return NULL;
    }
    return out;
}

static char *render_deflist_text(const char *content, size_t content_len, bool in_code,
                                 const char *full_doc, size_t full_doc_len, bool unsafe,
                                 char **stash, size_t *stash_len, size_t *stash_cap) {
    char *shielded = apex_shield_footnote_refs(content, content_len, in_code, stash, stash_len, stash_cap);
    const char *use = shielded ? shielded : content;
    size_t use_len = shielded ? strlen(shielded) : content_len;
    char *html = render_inline_with_doc(use, use_len, full_doc, full_doc_len, unsafe);
    free(shielded);
    return html;
}

static char *href_escape(const char *s) {
    size_t len = strlen(s);
    char *out = malloc(len * 3 + 1);
    if (!out) return NULL;
    char *w = out;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.') {
            *w++ = (char)*p;
        } else {
            sprintf(w, "%%%02X", *p);
            w += 3;
        }
    }
    *w = '\0';
    return out;
}

static char *render_footnote_ref_html(cmark_node *ref) {
    if (!ref || !ref->parent_footnote_def) return NULL;
    const char *id = cmark_node_get_literal(ref->parent_footnote_def);
    const char *num = cmark_node_get_literal(ref);
    if (!id || !id[0] || !num) return NULL;
    char *id_esc = href_escape(id);
    char *num_esc = href_escape(num);
    if (!id_esc || !num_esc) {
        free(id_esc);
        free(num_esc);
        return NULL;
    }
    char suffix[32] = "";
    if (ref->footnote.ref_ix > 1)
        snprintf(suffix, sizeof(suffix), "-%d", ref->footnote.ref_ix);
    size_t n = strlen(id_esc) * 2 + strlen(num_esc) + strlen(suffix) + 96;
    char *out = malloc(n);
    if (out) {
        snprintf(out, n,
                 "<sup class=\"footnote-ref\"><a href=\"#fn-%s\" id=\"fnref-%s%s\" data-footnote-ref>%s</a></sup>",
                 id_esc, id_esc, suffix, num_esc);
    }
    free(id_esc);
    free(num_esc);
    return out;
}

static char *fallback_footnote_marker(const char *id, size_t id_len) {
    char *out = malloc(id_len + 4);
    if (!out) return NULL;
    out[0] = '[';
    out[1] = '^';
    memcpy(out + 2, id, id_len);
    out[2 + id_len] = ']';
    out[3 + id_len] = '\0';
    return out;
}

static bool text_has_stash(cmark_node *para) {
    for (cmark_node *child = cmark_node_first_child(para); child; child = cmark_node_next(child)) {
        if (cmark_node_get_type(child) != CMARK_NODE_TEXT) continue;
        const char *lit = cmark_node_get_literal(child);
        if (lit && strstr(lit, "%%APEXFNSTASH:")) return true;
    }
    return false;
}

static void collect_stash_replacements(cmark_node *para, char ***reps, size_t *count, size_t *cap) {
    size_t unfilled = *count;
    for (cmark_node *child = cmark_node_first_child(para); child; child = cmark_node_next(child)) {
        if (cmark_node_get_type(child) == CMARK_NODE_TEXT) {
            const char *lit = cmark_node_get_literal(child);
            if (!lit) continue;
            const char *p = lit;
            while ((p = strstr(p, "%%APEXFNSTASH:")) != NULL) {
                const char *id = NULL;
                size_t id_len = 0;
                size_t token_len = 0;
                if (!parse_fn_token(p, "%%APEXFNSTASH:", &id, &id_len, &token_len)) {
                    p += 14;
                    continue;
                }
                if (*count + 1 > *cap) {
                    size_t next = (*cap < 8) ? 8 : *cap * 2;
                    char **grown = realloc(*reps, next * sizeof(char *));
                    if (!grown) return;
                    *reps = grown;
                    *cap = next;
                }
                (*reps)[(*count)++] = fallback_footnote_marker(id, id_len);
                p += token_len;
            }
        } else if (cmark_node_get_type(child) == CMARK_NODE_FOOTNOTE_REFERENCE && unfilled < *count) {
            char *html = render_footnote_ref_html(child);
            if (html) {
                free((*reps)[unfilled]);
                (*reps)[unfilled] = html;
                unfilled++;
            }
        }
    }
}

static char *replace_fn_placeholders(const char *literal, char **reps, size_t rep_count, size_t *used) {
    size_t cap = strlen(literal) + 64;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t len = 0;
    const char *p = literal;
    while (*p) {
        const char *token = strstr(p, "%%APEXFN:");
        if (!token) {
            size_t n = strlen(p);
            if (len + n + 1 > cap) {
                cap = len + n + 1;
                char *grown = realloc(out, cap);
                if (!grown) { free(out); return NULL; }
                out = grown;
            }
            memcpy(out + len, p, n);
            len += n;
            break;
        }
        size_t prefix_n = (size_t)(token - p);
        const char *id = NULL;
        size_t id_len = 0;
        size_t token_len = 0;
        if (!parse_fn_token(token, "%%APEXFN:", &id, &id_len, &token_len)) {
            if (len + prefix_n + 1 + 1 > cap) {
                cap = (len + prefix_n + 16) * 2;
                char *grown = realloc(out, cap);
                if (!grown) { free(out); return NULL; }
                out = grown;
            }
            memcpy(out + len, p, prefix_n + 1);
            len += prefix_n + 1;
            p = token + 1;
            continue;
        }
        const char *repl = (*used < rep_count) ? reps[*used] : NULL;
        char *fallback = NULL;
        if (!repl) {
            fallback = fallback_footnote_marker(id, id_len);
            repl = fallback ? fallback : "";
        } else {
            (*used)++;
        }
        size_t repl_len = strlen(repl);
        if (len + prefix_n + repl_len + 1 > cap) {
            cap = (len + prefix_n + repl_len + 64) * 2;
            char *grown = realloc(out, cap);
            if (!grown) { free(fallback); free(out); return NULL; }
            out = grown;
        }
        memcpy(out + len, p, prefix_n);
        len += prefix_n;
        memcpy(out + len, repl, repl_len);
        len += repl_len;
        free(fallback);
        p = token + token_len;
    }
    out[len] = '\0';
    return out;
}

static void collect_fn_nodes(cmark_node *node, cmark_node ***blocks, size_t *block_count, size_t *block_cap,
                             cmark_node ***paras, size_t *para_count, size_t *para_cap,
                             char ***reps, size_t *rep_count, size_t *rep_cap) {
    for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
        cmark_node_type type = cmark_node_get_type(child);
        if (type == CMARK_NODE_HTML_BLOCK) {
            const char *lit = cmark_node_get_literal(child);
            if (lit && strstr(lit, "%%APEXFN:")) {
                if (*block_count + 1 > *block_cap) {
                    size_t next = (*block_cap < 8) ? 8 : *block_cap * 2;
                    cmark_node **grown = realloc(*blocks, next * sizeof(cmark_node *));
                    if (!grown) return;
                    *blocks = grown;
                    *block_cap = next;
                }
                (*blocks)[(*block_count)++] = child;
            }
        } else if (type == CMARK_NODE_PARAGRAPH && text_has_stash(child)) {
            if (*para_count + 1 > *para_cap) {
                size_t next = (*para_cap < 8) ? 8 : *para_cap * 2;
                cmark_node **grown = realloc(*paras, next * sizeof(cmark_node *));
                if (!grown) return;
                *paras = grown;
                *para_cap = next;
            }
            (*paras)[(*para_count)++] = child;
            collect_stash_replacements(child, reps, rep_count, rep_cap);
        } else {
            collect_fn_nodes(child, blocks, block_count, block_cap, paras, para_count, para_cap,
                             reps, rep_count, rep_cap);
        }
    }
}

void apex_splice_definition_footnotes(cmark_node *root) {
    if (!root) return;

    cmark_node **blocks = NULL;
    size_t block_count = 0, block_cap = 0;
    cmark_node **paras = NULL;
    size_t para_count = 0, para_cap = 0;
    char **reps = NULL;
    size_t rep_count = 0, rep_cap = 0;

    collect_fn_nodes(root, &blocks, &block_count, &block_cap, &paras, &para_count, &para_cap,
                     &reps, &rep_count, &rep_cap);
    if (block_count == 0 && para_count == 0) {
        free(blocks);
        free(paras);
        free(reps);
        return;
    }

    size_t used = 0;
    for (size_t i = 0; i < block_count; i++) {
        const char *lit = cmark_node_get_literal(blocks[i]);
        if (!lit) continue;
        char *replaced = replace_fn_placeholders(lit, reps, rep_count, &used);
        if (replaced) {
            cmark_node_set_literal(blocks[i], replaced);
            free(replaced);
        }
    }
    for (size_t i = 0; i < para_count; i++)
        cmark_node_free(paras[i]);
    for (size_t i = 0; i < rep_count; i++)
        free(reps[i]);
    free(reps);
    free(blocks);
    free(paras);
}

/**
 * Process one-line definition lists: Term :: Definition -> <dl><dt>Term</dt><dd>Definition</dd></dl>
 * Returns newly allocated string with HTML, or NULL if no changes (caller uses original).
 */
char *apex_process_definition_lists(const char *text, bool unsafe) {
    if (!text) return NULL;

    size_t text_len = strlen(text);

    /* Quick scan: check for :: or : at line start (skip reference defs [id]: url) */
    bool has_pattern = false;
    const char *scan = text;
    while (*scan) {
        if (scan[0] == ':' && scan[1] == ':') {
            /* Skip ::: or more (div/custom element fence) - only match exactly :: */
            if (scan > text && scan[-1] == ':') { scan++; continue; }
            if (scan[2] == ':') { scan++; continue; }
            const char *line_start = scan;
            while (line_start > text && line_start[-1] != '\n') line_start--;
            if (is_leanpub_marker_colons(line_start, scan)) { scan++; continue; }
            const char *p = line_start;
            while (p < scan && (*p == ' ' || *p == '\t')) p++;
            if (p >= scan || *p != '[') { has_pattern = true; break; }
        }
        if ((scan == text || scan[-1] == '\n') && *scan) {
            const char *p = scan;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == ':' && (p[1] == ' ' || p[1] == '\t' || (p[1] == ':' && (p[2] == ' ' || p[2] == '\t')))) {
                has_pattern = true;
                break;
            }
        }
        scan++;
    }
    if (!has_pattern) return NULL;

    size_t output_capacity = text_len * 3;
    char *output = malloc(output_capacity + 1);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = output_capacity;
    char *fn_stash = NULL;
    size_t fn_stash_len = 0;
    size_t fn_stash_cap = 0;

#define ENSURE_SPACE(needed) do { \
    if (remaining <= (needed)) { \
        size_t used = write - output; \
        size_t min_capacity = used + (needed) + 1; \
        output_capacity = (min_capacity < 1024) ? 2048 : min_capacity * 2; \
        char *new_output = realloc(output, output_capacity + 1); \
        if (!new_output) { free(output); free(fn_stash); return NULL; } \
        output = new_output; \
        write = output + used; \
        remaining = output_capacity - used; \
    } \
} while(0)

#define FLUSH_FN_STASH() do { \
    if (fn_stash_len > 0) { \
        ENSURE_SPACE(fn_stash_len + 2); \
        memcpy(write, fn_stash, fn_stash_len); \
        write += fn_stash_len; \
        remaining -= fn_stash_len; \
        *write++ = '\n'; \
        *write++ = '\n'; \
        remaining -= 2; \
        fn_stash_len = 0; \
    } \
} while(0)

    bool in_def_list = false;
    bool in_code_block = false;
    bool in_indented_code_block = false;
    bool in_inline_code_span = false;
    const char *raw_close = NULL;  /* inside <style>, <script>, <pre>, or <textarea> */
    char term_buffer[4096];
    int term_len = 0;
    int blanks_after_term = 0;  /* blanks skipped while a potential term is buffered */
    bool dd_open = false;  /* True when we output <dd> but not yet </dd> (for Kramdown continuation) */
    bool prev_line_was_table_row = false;

    while (*read) {
        const char *line_start = read;
        const char *line_end = strchr(read, '\n');
        if (!line_end) line_end = read + strlen(read);

        size_t line_length = (size_t)(line_end - line_start);
        int sep = -1;  /* One-line def separator pos; -1 = none (used for inline code state update) */

        /* Leave raw HTML blocks alone. `::before` in CSS is not a definition. */
        if (raw_close) {
            ENSURE_SPACE(line_length + 2);
            memcpy(write, line_start, line_length);
            write += line_length;
            remaining -= line_length;
            *write++ = '\n';
            remaining--;
            if (line_contains_ci(line_start, line_length, raw_close))
                raw_close = NULL;
            read = line_end + (line_end < text + text_len && *line_end == '\n' ? 1 : 0);
            continue;
        }

        /* Track indented code blocks (4+ spaces or tab, not list continuation) */
        if (read == text || read[-1] == '\n') {
            bool this_line_indented = line_is_indented_code_block(line_start, line_length);
            if (this_line_indented) {
                in_indented_code_block = true;
            } else {
                /* Non-blank line without indent ends the block */
                bool is_blank = (line_length == 0 || (line_length == 1 && (*line_start == '\r' || *line_start == '\n')));
                if (!is_blank) in_indented_code_block = false;
            }
        }

        /* Track code blocks */
        if (is_code_fence_line(line_start, line_length)) {
            in_code_block = !in_code_block;
            if (in_code_block) {
                /* Entering fenced code: close DL cleanly and flush buffered prose
                 * before the fence. Extra blank after </dl> so CommonMark does not
                 * treat the fence as part of an HTML block. */
                if (dd_open) {
                    ENSURE_SPACE(10);
                    memcpy(write, "</dd>\n", 6);
                    write += 6;
                    remaining -= 6;
                    dd_open = false;
                }
                if (in_def_list) {
                    ENSURE_SPACE(10);
                    memcpy(write, "</dl>\n\n", 7);
                    write += 7;
                    remaining -= 7;
                    in_def_list = false;
                    FLUSH_FN_STASH();
                }
                if (term_len > 0) {
                    ENSURE_SPACE((size_t)term_len + 2 + (size_t)blanks_after_term);
                    memcpy(write, term_buffer, (size_t)term_len);
                    write += term_len;
                    remaining -= (size_t)term_len;
                    *write++ = '\n';
                    remaining--;
                    for (int bi = 0; bi < blanks_after_term; bi++) {
                        *write++ = '\n';
                        remaining--;
                    }
                    term_len = 0;
                    blanks_after_term = 0;
                }
            }
            ENSURE_SPACE(line_length + 2);
            memcpy(write, line_start, line_length);
            write += line_length;
            remaining -= line_length;
            *write++ = '\n';
            remaining--;
            read = line_end + (line_end < text + text_len && *line_end == '\n' ? 1 : 0);
            continue;
        }

        if (in_code_block) {
            ENSURE_SPACE(line_length + 2);
            memcpy(write, line_start, line_length);
            write += line_length;
            remaining -= line_length;
            *write++ = '\n';
            remaining--;
            read = line_end + (line_end < text + text_len && *line_end == '\n' ? 1 : 0);
            continue;
        }

        /* <style>, <script>, <pre>, and <textarea> are raw until their closer.
         * Close any open list first so the block is not nested in a <dd>. */
        const char *raw_open = raw_html_block_close(line_start, line_length);
        if (raw_open) {
            if (dd_open) {
                ENSURE_SPACE(10);
                memcpy(write, "</dd>\n", 6);
                write += 6;
                remaining -= 6;
                dd_open = false;
            }
            if (in_def_list) {
                ENSURE_SPACE(10);
                memcpy(write, "</dl>\n\n", 7);
                write += 7;
                remaining -= 7;
                in_def_list = false;
                FLUSH_FN_STASH();
            }
            if (term_len > 0) {
                ENSURE_SPACE((size_t)term_len + 2 + (size_t)blanks_after_term);
                memcpy(write, term_buffer, (size_t)term_len);
                write += term_len;
                remaining -= (size_t)term_len;
                *write++ = '\n';
                remaining--;
                for (int bi = 0; bi < blanks_after_term; bi++) {
                    *write++ = '\n';
                    remaining--;
                }
                term_len = 0;
                blanks_after_term = 0;
            }
            ENSURE_SPACE(line_length + 2);
            memcpy(write, line_start, line_length);
            write += line_length;
            remaining -= line_length;
            *write++ = '\n';
            remaining--;
            if (!line_contains_ci(line_start, line_length, raw_open))
                raw_close = raw_open;
            read = line_end + (line_end < text + text_len && *line_end == '\n' ? 1 : 0);
            continue;
        }

        /* Skip definition processing inside indented code blocks */
        if (in_indented_code_block) {
            if (term_len > 0) {
                ENSURE_SPACE((size_t)term_len + 2 + (size_t)blanks_after_term);
                memcpy(write, term_buffer, (size_t)term_len);
                write += term_len;
                remaining -= (size_t)term_len;
                *write++ = '\n';
                remaining--;
                for (int bi = 0; bi < blanks_after_term; bi++) {
                    *write++ = '\n';
                    remaining--;
                }
                term_len = 0;
                blanks_after_term = 0;
            }
            ENSURE_SPACE(line_length + 2);
            memcpy(write, line_start, line_length);
            write += line_length;
            remaining -= line_length;
            *write++ = '\n';
            remaining--;
            read = line_end + (line_end < text + text_len && *line_end == '\n' ? 1 : 0);
            continue;
        }

        /* Check for Kramdown-style definition: : Definition (requires buffered term) */
        bool is_kramdown_def = !in_code_block && !in_inline_code_span && is_kramdown_def_line(line_start, line_length);
        if (is_kramdown_def) {
            /* Skip reference link definitions [id]: url */
            const char *p = line_start;
            while (p < line_end && (*p == ' ' || *p == '\t')) p++;
            if (p < line_end && *p == '[') {
                is_kramdown_def = false;  /* Reference def, not a definition line */
            }
        }
        if (is_kramdown_def && prev_line_was_table_row) {
            /* : Caption after table is a table caption, not a definition */
            is_kramdown_def = false;
        }
        if (is_kramdown_def) {
            const char *next_start = line_end;
            if (next_start < text + text_len && *next_start == '\n') next_start++;
            if (next_nonblank_line_is_table(next_start, text + text_len)) {
                /* : Caption before table is a table caption, not a definition */
                is_kramdown_def = false;
            }
        }

        if (is_kramdown_def) {
            /* Extract definition text (after : or :: and space) */
            const char *def_start = line_start;
            while (def_start < line_end && (*def_start == ' ' || *def_start == '\t')) def_start++;
            if (def_start < line_end && *def_start == ':') {
                def_start++;
                if (def_start < line_end && *def_start == ':') def_start++;
                while (def_start < line_end && (*def_start == ' ' || *def_start == '\t')) def_start++;
            }
            size_t def_len = (size_t)(line_end - def_start);

            if (term_len > 0) {
                /* We have a buffered term - output <dl><dt>term</dt><dd>def</dd> */
                if (!in_def_list) {
                    ENSURE_SPACE(20);
                    memcpy(write, "<dl>\n", 5);
                    write += 5;
                    remaining -= 5;
                    in_def_list = true;
                }
                if (dd_open) {
                    memcpy(write, "</dd>\n", 6);
                    write += 6;
                    remaining -= 6;
                    dd_open = false;
                }
                /* <dt>term</dt> */
                ENSURE_SPACE(10);
                memcpy(write, "<dt>", 4);
                write += 4;
                remaining -= 4;
                char *term_html = render_deflist_text(term_buffer, (size_t)term_len, false, text, text_len, unsafe, &fn_stash, &fn_stash_len, &fn_stash_cap);
                if (term_html) {
                    size_t html_len = strlen(term_html);
                    ENSURE_SPACE(html_len + 20);
                    memcpy(write, term_html, html_len);
                    write += html_len;
                    remaining -= html_len;
                    free(term_html);
                }
                memcpy(write, "</dt>\n", 6);
                write += 6;
                remaining -= 6;
                term_len = 0;
                blanks_after_term = 0;  /* blanks between term and : are normal DL spacing */
            } else if (in_def_list) {
                /* Another : definition for same term */
                if (dd_open) {
                    memcpy(write, "</dd>\n", 6);
                    write += 6;
                    remaining -= 6;
                }
            }

            /* <dd>definition</dd> */
            ENSURE_SPACE(20 + def_len * 2);
            memcpy(write, "<dd>", 4);
            write += 4;
            remaining -= 4;
            dd_open = true;

            if (def_len > 0) {
                char *def_html = render_deflist_text(def_start, def_len, in_inline_code_span, text, text_len, unsafe, &fn_stash, &fn_stash_len, &fn_stash_cap);
                if (def_html) {
                    size_t html_len = strlen(def_html);
                    ENSURE_SPACE(html_len + 20);
                    memcpy(write, def_html, html_len);
                    write += html_len;
                    remaining -= html_len;
                    free(def_html);
                }
            }
            /* Don't close </dd> yet - allow indented continuation lines */
        }
        /* Check for one-line definition: Term :: Definition */
        else if (!in_code_block) {
            sep = find_def_separator((const unsigned char *)line_start, (int)line_length);
            bool sep_inside_inline = false;
            if (sep >= 0) {
                sep_inside_inline = scan_inline_code_for_sep(line_start, line_length, sep, in_inline_code_span, &in_inline_code_span);
            }
            if (sep >= 0 && !sep_inside_inline) {
            /* Close any open Kramdown dd and flush unused term buffer */
            if (dd_open) {
                memcpy(write, "</dd>\n", 6);
                write += 6;
                remaining -= 6;
                dd_open = false;
            }
            if (in_def_list && term_len > 0) {
                /* Buffered term wasn't used - output as regular line, close list */
                ENSURE_SPACE(10);
                memcpy(write, "</dl>\n\n", 7);
                write += 7;
                remaining -= 7;
                in_def_list = false;
                FLUSH_FN_STASH();
            }
            if (term_len > 0) {
                ENSURE_SPACE((size_t)term_len + 2 + (size_t)blanks_after_term);
                memcpy(write, term_buffer, (size_t)term_len);
                write += term_len;
                remaining -= (size_t)term_len;
                *write++ = '\n';
                remaining--;
                for (int bi = 0; bi < blanks_after_term; bi++) {
                    *write++ = '\n';
                    remaining--;
                }
                term_len = 0;
                blanks_after_term = 0;
            }
            /* Extract term (before ::) and definition (after ::) */
            const char *term_start = line_start;
            const char *term_end = line_start + sep;
            const char *def_start = line_start + sep + 2;
            const char *def_end = line_end;

            /* Trim term */
            while (term_start < term_end && (*term_start == ' ' || *term_start == '\t')) term_start++;
            while (term_end > term_start && (term_end[-1] == ' ' || term_end[-1] == '\t')) term_end--;

            /* Trim definition */
            while (def_start < def_end && (*def_start == ' ' || *def_start == '\t')) def_start++;

            size_t term_len = (size_t)(term_end - term_start);
            size_t def_len = (size_t)(def_end - def_start);

            if (!in_def_list) {
                ENSURE_SPACE(10);
                memcpy(write, "<dl>\n", 5);
                write += 5;
                remaining -= 5;
                in_def_list = true;
            }

            /* <dt>term</dt> */
            ENSURE_SPACE(20 + term_len * 2);
            memcpy(write, "<dt>", 4);
            write += 4;
            remaining -= 4;

            /* Parse term as inline markdown */
            if (term_len > 0) {
                char *term_html = render_deflist_text(term_start, term_len, in_inline_code_span, text, text_len, unsafe, &fn_stash, &fn_stash_len, &fn_stash_cap);
                if (term_html) {
                    size_t html_len = strlen(term_html);
                    ENSURE_SPACE(html_len + 20);
                    memcpy(write, term_html, html_len);
                    write += html_len;
                    remaining -= html_len;
                    free(term_html);
                }
            }

            memcpy(write, "</dt>\n", 6);
            write += 6;
            remaining -= 6;

            /* <dd>definition</dd> */
            ENSURE_SPACE(20 + def_len * 2);
            memcpy(write, "<dd>", 4);
            write += 4;
            remaining -= 4;

            if (def_len > 0) {
                char *def_html = render_deflist_text(def_start, def_len, in_inline_code_span, text, text_len, unsafe, &fn_stash, &fn_stash_len, &fn_stash_cap);
                if (def_html) {
                    size_t html_len = strlen(def_html);
                    ENSURE_SPACE(html_len + 20);
                    memcpy(write, def_html, html_len);
                    write += html_len;
                    remaining -= html_len;
                    free(def_html);
                }
            }

            memcpy(write, "</dd>\n", 6);
            write += 6;
            remaining -= 6;
            } else {
            /* Not one-line def (sep < 0) - buffer as potential Kramdown term */
            if (dd_open) {
                memcpy(write, "</dd>\n", 6);
                write += 6;
                remaining -= 6;
                dd_open = false;
            }
            bool is_blank = (line_length == 0 || (line_length == 1 && (*line_start == '\r' || *line_start == '\n')));
            if (is_blank) {
                /* Blank line: keep def list open (next line might be : definition for same term) */
                if (term_len > 0) {
                    /* Remember blanks; restore them if this was not actually a term */
                    blanks_after_term++;
                } else if (!in_def_list) {
                    ENSURE_SPACE(2);
                    *write++ = '\n';
                    remaining--;
                }
                /* else: in_def_list, skip blank, list stays open */
            } else {
                if (in_def_list) {
                    ENSURE_SPACE(10);
                    memcpy(write, "</dl>\n\n", 7);
                    write += 7;
                    remaining -= 7;
                    in_def_list = false;
                    FLUSH_FN_STASH();
                }
                if (term_len > 0) {
                    ENSURE_SPACE((size_t)term_len + 2 + (size_t)blanks_after_term);
                    memcpy(write, term_buffer, (size_t)term_len);
                    write += term_len;
                    remaining -= (size_t)term_len;
                    *write++ = '\n';
                    remaining--;
                    for (int bi = 0; bi < blanks_after_term; bi++) {
                        *write++ = '\n';
                        remaining--;
                    }
                    term_len = 0;
                    blanks_after_term = 0;
                }
                bool is_ref_def = line_is_link_reference_definition(line_start, line_end);
                if (is_ref_def || is_atx_heading_line(line_start, line_length) ||
                    is_block_start_line(line_start, line_length) ||
                    line_length >= sizeof(term_buffer) - 1) {
                    ENSURE_SPACE(line_length + 2);
                    memcpy(write, line_start, line_length);
                    write += line_length;
                    remaining -= line_length;
                    *write++ = '\n';
                    remaining--;
                } else {
                    memcpy(term_buffer, line_start, line_length);
                    term_len = (int)line_length;
                    term_buffer[term_len] = '\0';
                    blanks_after_term = 0;
                }
            }
            }
        }

        /* Track if this line was a table row (for : Caption after table detection) */
        bool is_blank = (line_length == 0 || (line_length == 1 && (*line_start == '\r' || *line_start == '\n')));
        if (!is_blank) prev_line_was_table_row = is_table_row_line(line_start, line_length);

        /* Update inline code span state for next line (if not already updated in one-line def path) */
        if (sep < 0) {
            scan_inline_code_for_sep(line_start, line_length, -1, in_inline_code_span, &in_inline_code_span);
        }

        read = line_end;
        if (read < text + text_len && *read == '\n') read++;
    }


    if (dd_open) {
        memcpy(write, "</dd>\n", 6);
        write += 6;
    }
    if (in_def_list) {
        ENSURE_SPACE(10);
        memcpy(write, "</dl>\n\n", 7);
        write += 7;
        remaining -= 7;
        FLUSH_FN_STASH();
    }
    if (term_len > 0) {
        ENSURE_SPACE((size_t)term_len + 2 + (size_t)blanks_after_term);
        memcpy(write, term_buffer, (size_t)term_len);
        write += term_len;
        *write++ = '\n';
        for (int bi = 0; bi < blanks_after_term; bi++) {
            *write++ = '\n';
        }
    }

    *write = '\0';
#undef FLUSH_FN_STASH
#undef ENSURE_SPACE

    free(fn_stash);
    return output;
}

void apex_deflist_debug_touch(int enable_definition_lists) {
    (void)enable_definition_lists;
    /* No-op for one-line format - debug was for old Kramdown format */
}
