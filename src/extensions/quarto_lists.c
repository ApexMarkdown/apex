/**
 * Quarto/Pandoc list extensions: (@) continuation, line blocks, roman markers
 */

#include "quarto_lists.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char LIST_CONTINUE_MARKER[] = "<!-- apex-list-continue -->";
static const char ROMAN_COMMENT_LOWER[] = "<!-- apex-alpha-list-lower-roman -->";
static const char ROMAN_COMMENT_UPPER[] = "<!-- apex-alpha-list-upper-roman -->";

static const char *line_end_ptr(const char *p) {
    const char *e = strchr(p, '\n');
    return e ? e : p + strlen(p);
}

static bool append_chunk(char **out, size_t *len, size_t *cap, const char *chunk, size_t chunk_len) {
    if (chunk_len == 0) {
        return true;
    }
    if (*len + chunk_len + 1 > *cap) {
        size_t new_cap = (*cap == 0 ? 256 : *cap * 2);
        while (*len + chunk_len + 1 > new_cap) {
            new_cap *= 2;
        }
        char *grown = realloc(*out, new_cap);
        if (!grown) {
            return false;
        }
        *out = grown;
        *cap = new_cap;
    }
    memcpy(*out + *len, chunk, chunk_len);
    *len += chunk_len;
    (*out)[*len] = '\0';
    return true;
}

static bool append_str(char **out, size_t *len, size_t *cap, const char *str) {
    if (!str) {
        return true;
    }
    return append_chunk(out, len, cap, str, strlen(str));
}

static const char *skip_indent(const char *line_start, const char *line_end, size_t *indent_out) {
    const char *p = line_start;
    while (p < line_end && (*p == ' ' || *p == '\t')) {
        p++;
    }
    if (indent_out) {
        *indent_out = (size_t)(p - line_start);
    }
    return p;
}

static bool line_is_blank(const char *line_start, const char *line_end) {
    const char *p = skip_indent(line_start, line_end, NULL);
    return p >= line_end;
}

static bool parse_ordered_list_marker(const char *p, const char *line_end, size_t *indent_out) {
    const char *content = skip_indent(p, line_end, indent_out);
    if (content >= line_end || *content < '0' || *content > '9') {
        return false;
    }
    const char *num_p = content;
    while (num_p < line_end && *num_p >= '0' && *num_p <= '9') {
        num_p++;
    }
    if (num_p >= line_end || *num_p != '.') {
        return false;
    }
    num_p++;
    return num_p >= line_end || *num_p == ' ' || *num_p == '\t';
}

static bool is_list_continuation_marker(const char *line_start, const char *line_end) {
    const char *p = skip_indent(line_start, line_end, NULL);
    if (p + 3 > line_end) {
        return false;
    }
    if (p[0] != '(' || p[1] != '@' || p[2] != ')') {
        return false;
    }
    p += 3;
    while (p < line_end && (*p == ' ' || *p == '\t')) {
        p++;
    }
    return p >= line_end;
}

static const char *next_non_blank_line(const char *read) {
    while (*read) {
        const char *line_start = read;
        const char *line_end = line_end_ptr(read);
        if (!line_is_blank(line_start, line_end)) {
            return line_start;
        }
        read = (*line_end == '\n') ? line_end + 1 : line_end;
    }
    return NULL;
}

static bool is_line_block_line(const char *line_start, const char *line_end) {
    const char *p = skip_indent(line_start, line_end, NULL);
    if (p >= line_end || *p != '|') {
        return false;
    }
    p++;
    for (const char *q = p; q < line_end; q++) {
        if (*q == '|') {
            return false;
        }
    }
    return true;
}

static bool extract_line_block_content(const char *line_start, const char *line_end,
                                       const char **content_start, size_t *content_len) {
    const char *p = skip_indent(line_start, line_end, NULL);
    if (p >= line_end || *p != '|') {
        return false;
    }
    p++;
    if (p < line_end && *p == ' ') {
        p++;
    }
    const char *end = line_end;
    while (end > p && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    *content_start = p;
    *content_len = (size_t)(end - p);
    return true;
}

static int roman_digit_value(char c) {
    switch (tolower((unsigned char)c)) {
        case 'i': return 1;
        case 'v': return 5;
        case 'x': return 10;
        case 'l': return 50;
        case 'c': return 100;
        case 'd': return 500;
        case 'm': return 1000;
        default: return 0;
    }
}

static bool roman_to_int(const char *start, const char *end, bool *is_upper, int *value_out) {
    if (!start || !end || start >= end || !value_out) {
        return false;
    }

    bool upper = true;
    bool lower = true;
    for (const char *p = start; p < end; p++) {
        if (*p >= 'a' && *p <= 'z') {
            upper = false;
        } else if (*p >= 'A' && *p <= 'Z') {
            lower = false;
        } else {
            return false;
        }
        if (roman_digit_value(*p) == 0) {
            return false;
        }
    }
    if (!upper && !lower) {
        return false;
    }
    if (is_upper) {
        *is_upper = upper;
    }

    int total = 0;
    int prev = 0;
    for (const char *p = end - 1; p >= start; p--) {
        int val = roman_digit_value(*p);
        if (val < prev) {
            total -= val;
        } else {
            total += val;
        }
        prev = val;
    }
    if (total <= 0) {
        return false;
    }
    *value_out = total;
    return true;
}

static bool parse_roman_list_marker(const char *line_start, const char *line_end,
                                    size_t *indent_out, int *value_out, bool *is_upper) {
    const char *p = skip_indent(line_start, line_end, indent_out);
    if (p >= line_end) {
        return false;
    }
    const char *digits_start = p;
    while (p < line_end && (tolower((unsigned char)*p) == 'i' ||
                            tolower((unsigned char)*p) == 'v' ||
                            tolower((unsigned char)*p) == 'x' ||
                            tolower((unsigned char)*p) == 'l' ||
                            tolower((unsigned char)*p) == 'c' ||
                            tolower((unsigned char)*p) == 'd' ||
                            tolower((unsigned char)*p) == 'm')) {
        p++;
    }
    if (p == digits_start || p >= line_end || *p != ')') {
        return false;
    }
    p++;
    if (p < line_end && *p != ' ' && *p != '\t') {
        return false;
    }
    return roman_to_int(digits_start, p - 1, is_upper, value_out);
}

static const char *prev_line_start(const char *text, const char *line_start) {
    if (line_start <= text) {
        return NULL;
    }
    const char *p = line_start - 1;
    while (p >= text && *p == '\n') {
        p--;
    }
    while (p >= text && *p != '\n') {
        p--;
    }
    return (p < text) ? text : p + 1;
}

static const char *find_prev_ordered_list(const char *text, const char *before,
                                          size_t target_indent, size_t *indent_out) {
    const char *line_start = before;
    while (line_start && line_start >= text) {
        const char *line_end = line_end_ptr(line_start);
        if (is_list_continuation_marker(line_start, line_end)) {
            line_start = prev_line_start(text, line_start);
            continue;
        }
        if (line_is_blank(line_start, line_end)) {
            line_start = prev_line_start(text, line_start);
            continue;
        }
        size_t indent = 0;
        if (parse_ordered_list_marker(line_start, line_end, &indent) && indent == target_indent) {
            if (indent_out) {
                *indent_out = indent;
            }
            return line_start;
        }
        size_t line_indent = 0;
        skip_indent(line_start, line_end, &line_indent);
        if (line_indent <= target_indent) {
            /* Paragraph or other block between list items; keep scanning backward. */
            line_start = prev_line_start(text, line_start);
            continue;
        }
        line_start = prev_line_start(text, line_start);
    }
    return NULL;
}

static const char *find_next_ordered_list(const char *text, size_t target_indent, size_t *indent_out) {
    const char *read = text;
    while (*read) {
        const char *line_start = read;
        const char *line_end = line_end_ptr(read);
        if (!line_is_blank(line_start, line_end) &&
            !is_list_continuation_marker(line_start, line_end)) {
            size_t indent = 0;
            if (parse_ordered_list_marker(line_start, line_end, &indent) && indent == target_indent) {
                if (indent_out) {
                    *indent_out = indent;
                }
                return line_start;
            }
            size_t line_indent = 0;
            skip_indent(line_start, line_end, &line_indent);
            if (line_indent <= target_indent) {
                return NULL;
            }
        }
        read = (*line_end == '\n') ? line_end + 1 : line_end;
    }
    return NULL;
}

static bool only_blanks_between(const char *start, const char *end) {
    const char *read = start;
    while (read < end) {
        const char *line_start = read;
        const char *line_end = line_end_ptr(read);
        if (!line_is_blank(line_start, line_end)) {
            return false;
        }
        read = (*line_end == '\n') ? line_end + 1 : line_end;
    }
    return true;
}

char *apex_preprocess_list_continuation(const char *text) {
    if (!text) {
        return NULL;
    }

    size_t cap = strlen(text) * 2 + 256;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t len = 0;
    out[0] = '\0';
    bool changed = false;

    const char *read = text;
    char pending_blanks[512];
    size_t pending_blanks_len = 0;

    while (*read) {
        const char *line_start = read;
        const char *line_end = line_end_ptr(read);
        bool has_newline = (*line_end == '\n');

        if (is_list_continuation_marker(line_start, line_end)) {
            changed = true;
            size_t marker_indent = 0;
            skip_indent(line_start, line_end, &marker_indent);

            const char *prev_line = prev_line_start(text, line_start);
            size_t prev_indent = 0;
            const char *prev_ordered = prev_line
                ? find_prev_ordered_list(text, prev_line, marker_indent, &prev_indent)
                : NULL;

            const char *next_line = has_newline ? line_end + 1 : line_end;
            size_t next_indent = 0;
            const char *next_ordered = find_next_ordered_list(next_line, marker_indent, &next_indent);

            if (prev_ordered && next_ordered && prev_indent == next_indent) {
                const char *after_prev = line_end_ptr(prev_ordered);
                after_prev = (*after_prev == '\n') ? after_prev + 1 : after_prev;
                bool collapse = only_blanks_between(after_prev, line_start);
                if (!collapse) {
                    for (const char *scan = after_prev; scan < line_start; ) {
                        const char *ls = scan;
                        const char *le = line_end_ptr(scan);
                        if (!line_is_blank(ls, le) && !is_list_continuation_marker(ls, le)) {
                            size_t indent = 0;
                            if (parse_ordered_list_marker(ls, le, &indent) && indent == marker_indent) {
                                collapse = true;
                                break;
                            }
                            collapse = false;
                            break;
                        }
                        scan = (*le == '\n') ? le + 1 : le;
                    }
                }

                if (collapse) {
                    pending_blanks_len = 0;
                } else {
                    pending_blanks_len = 0;
                    if (!append_str(&out, &len, &cap, LIST_CONTINUE_MARKER) ||
                        !append_str(&out, &len, &cap, "\n")) {
                        free(out);
                        return NULL;
                    }
                }
            }

            read = has_newline ? line_end + 1 : line_end;
            continue;
        }

        if (line_is_blank(line_start, line_end)) {
            if (pending_blanks_len + (has_newline ? 1 : 0) < sizeof(pending_blanks) && has_newline) {
                pending_blanks[pending_blanks_len++] = '\n';
            }
            read = has_newline ? line_end + 1 : line_end;
            continue;
        }

        if (pending_blanks_len > 0) {
            if (!append_chunk(&out, &len, &cap, pending_blanks, pending_blanks_len)) {
                free(out);
                return NULL;
            }
            pending_blanks_len = 0;
        }

        size_t line_len = (size_t)(line_end - line_start) + (has_newline ? 1 : 0);
        if (!append_chunk(&out, &len, &cap, line_start, line_len)) {
            free(out);
            return NULL;
        }

        read = has_newline ? line_end + 1 : line_end;
    }

    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}

char *apex_preprocess_line_blocks(const char *text, bool unsafe) {
    if (!text || !unsafe) {
        return NULL;
    }

    size_t cap = strlen(text) * 3 + 256;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t len = 0;
    out[0] = '\0';
    bool changed = false;
    bool in_fenced_code = false;

    const char *read = text;
    while (*read) {
        const char *line_start = read;
        const char *line_end = line_end_ptr(read);
        bool has_newline = (*line_end == '\n');
        bool at_line_start = (read == text || read[-1] == '\n');

        if (at_line_start) {
            const char *content = skip_indent(line_start, line_end, NULL);
            if ((size_t)(line_end - content) >= 3 && strncmp(content, "```", 3) == 0) {
                in_fenced_code = !in_fenced_code;
            }
        }

        if (!in_fenced_code && at_line_start && is_line_block_line(line_start, line_end)) {
            const char *block_start = line_start;
            const char *scan = has_newline ? line_end + 1 : line_end;
            while (*scan) {
                const char *next_start = scan;
                const char *next_end = line_end_ptr(scan);
                if (!is_line_block_line(next_start, next_end)) {
                    break;
                }
                bool next_has_newline = (*next_end == '\n');
                scan = next_has_newline ? next_end + 1 : next_end;
            }

            if (block_start > text && block_start[-1] != '\n') {
                if (!append_str(&out, &len, &cap, "\n")) {
                    free(out);
                    return NULL;
                }
            }
            if (!append_str(&out, &len, &cap, "<div class=\"line-block\">\n")) {
                free(out);
                return NULL;
            }

            const char *line = block_start;
            while (line < scan) {
                const char *le = line_end_ptr(line);
                const char *content = NULL;
                size_t content_len = 0;
                extract_line_block_content(line, le, &content, &content_len);
                if (!append_str(&out, &len, &cap, "<div class=\"line\">")) {
                    free(out);
                    return NULL;
                }
                if (content_len > 0 && !append_chunk(&out, &len, &cap, content, content_len)) {
                    free(out);
                    return NULL;
                }
                if (!append_str(&out, &len, &cap, "</div>\n")) {
                    free(out);
                    return NULL;
                }
                line = (*le == '\n') ? le + 1 : le;
            }

            if (!append_str(&out, &len, &cap, "</div>\n")) {
                free(out);
                return NULL;
            }

            changed = true;
            read = scan;
            continue;
        }

        size_t line_len = (size_t)(line_end - line_start) + (has_newline ? 1 : 0);
        if (!append_chunk(&out, &len, &cap, line_start, line_len)) {
            free(out);
            return NULL;
        }
        read = has_newline ? line_end + 1 : line_end;
    }

    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}

char *apex_preprocess_roman_lists(const char *text) {
    if (!text) {
        return NULL;
    }

    size_t cap = strlen(text) * 3 + 256;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t len = 0;
    out[0] = '\0';

    bool in_roman_list = false;
    size_t roman_list_indent = 0;
    bool is_upper = false;
    int item_number = 1;
    int expected_value = 1;
    int blank_lines_since_roman = 0;
    bool changed = false;

    const char *read = text;
    while (*read) {
        const char *line_start = read;
        const char *line_end = line_end_ptr(read);
        bool has_newline = (*line_end == '\n');

        size_t current_indent = 0;
        int roman_value = 0;
        bool roman_upper = false;
        bool is_roman_marker = parse_roman_list_marker(line_start, line_end, &current_indent,
                                                       &roman_value, &roman_upper);

        if (is_roman_marker) {
            bool continues_list = false;
            if (in_roman_list && roman_upper == is_upper && current_indent == roman_list_indent &&
                roman_value == expected_value) {
                continues_list = true;
            }

            if (!continues_list) {
                in_roman_list = true;
                is_upper = roman_upper;
                roman_list_indent = current_indent;
                item_number = 1;
                expected_value = roman_value;
                blank_lines_since_roman = 0;

                const char *indent_end = skip_indent(line_start, line_end, NULL);
                if (!append_chunk(&out, &len, &cap, line_start, (size_t)(indent_end - line_start))) {
                    free(out);
                    return NULL;
                }
                const char *comment = is_upper ? ROMAN_COMMENT_UPPER : ROMAN_COMMENT_LOWER;
                if (!append_str(&out, &len, &cap, comment) ||
                    !append_str(&out, &len, &cap, "\n\n")) {
                    free(out);
                    return NULL;
                }
            } else {
                blank_lines_since_roman = 0;
            }

            const char *indent_end = skip_indent(line_start, line_end, NULL);
            char num_buf[32];
            int num_len = snprintf(num_buf, sizeof(num_buf), "%d. ", item_number);
            if (num_len <= 0 || num_len >= (int)sizeof(num_buf)) {
                free(out);
                return NULL;
            }
            if (!append_chunk(&out, &len, &cap, line_start, (size_t)(indent_end - line_start)) ||
                !append_chunk(&out, &len, &cap, num_buf, (size_t)num_len)) {
                free(out);
                return NULL;
            }

            const char *marker_end = indent_end;
            while (marker_end < line_end && (tolower((unsigned char)*marker_end) == 'i' ||
                                            tolower((unsigned char)*marker_end) == 'v' ||
                                            tolower((unsigned char)*marker_end) == 'x' ||
                                            tolower((unsigned char)*marker_end) == 'l' ||
                                            tolower((unsigned char)*marker_end) == 'c' ||
                                            tolower((unsigned char)*marker_end) == 'd' ||
                                            tolower((unsigned char)*marker_end) == 'm')) {
                marker_end++;
            }
            if (marker_end < line_end && *marker_end == ')') {
                marker_end++;
            }
            while (marker_end < line_end && (*marker_end == ' ' || *marker_end == '\t')) {
                marker_end++;
            }

            size_t rest_len = (size_t)(line_end - marker_end) + (has_newline ? 1 : 0);
            if (!append_chunk(&out, &len, &cap, marker_end, rest_len)) {
                free(out);
                return NULL;
            }

            item_number++;
            expected_value = roman_value + 1;
            changed = true;
            read = has_newline ? line_end + 1 : line_end;
            continue;
        }

        if (in_roman_list) {
            if (line_is_blank(line_start, line_end)) {
                blank_lines_since_roman++;
                if (blank_lines_since_roman >= 2) {
                    in_roman_list = false;
                }
            } else if (current_indent > roman_list_indent) {
                blank_lines_since_roman = 0;
            } else {
                in_roman_list = false;
                blank_lines_since_roman = 0;
            }
        }

        size_t line_len = (size_t)(line_end - line_start) + (has_newline ? 1 : 0);
        if (!append_chunk(&out, &len, &cap, line_start, line_len)) {
            free(out);
            return NULL;
        }
        read = has_newline ? line_end + 1 : line_end;
    }

    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}

static char *apply_list_style_to_following_ol(const char *html, const char *comment,
                                              const char *style_attr) {
    if (!html || !comment || !style_attr) {
        return NULL;
    }
    if (!strstr(html, comment)) {
        return NULL;
    }

    size_t html_len = strlen(html);
    size_t cap = html_len + 1024;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t len = 0;
    out[0] = '\0';
    bool changed = false;

    const char *read = html;
    const char *read_start = read;
    const size_t comment_len = strlen(comment);
    const size_t style_len = strlen(style_attr);

    while (*read) {
        if (strncmp(read, comment, comment_len) == 0) {
            size_t copy_len = (size_t)(read - read_start);
            if (len + copy_len + style_len + 64 > cap) {
                cap = (len + copy_len + style_len + 64) * 2;
                char *grown = realloc(out, cap);
                if (!grown) {
                    free(out);
                    return NULL;
                }
                out = grown;
            }
            if (copy_len > 0) {
                memcpy(out + len, read_start, copy_len);
                len += copy_len;
            }
            read += comment_len;
            read_start = read;
            while (*read == ' ' || *read == '\t' || *read == '\n' || *read == '\r') {
                read++;
            }
            read_start = read;

            if (read[0] == '<' && read[1] == 'o' && read[2] == 'l' &&
                (read[3] == '>' || read[3] == ' ' || read[3] == '\t' || read[3] == '\n')) {
                const char *ol_start = read;
                const char *tag_end = strchr(ol_start, '>');
                if (tag_end) {
                    bool has_style = false;
                    for (const char *p = ol_start; p < tag_end; p++) {
                        if (strncmp(p, "style=", 6) == 0) {
                            has_style = true;
                            break;
                        }
                    }
                    size_t tag_len = (size_t)(tag_end - ol_start);
                    memcpy(out + len, ol_start, tag_len);
                    len += tag_len;
                    if (!has_style) {
                        memcpy(out + len, style_attr, style_len);
                        len += style_len;
                    } else {
                        out[len++] = '>';
                    }
                    read = tag_end + 1;
                    read_start = read;
                    changed = true;
                    continue;
                }
            }
            continue;
        }
        read++;
    }

    if (!changed) {
        free(out);
        return NULL;
    }

    size_t tail_len = strlen(read_start);
    if (len + tail_len + 1 > cap) {
        cap = len + tail_len + 2;
        char *grown = realloc(out, cap);
        if (!grown) {
            free(out);
            return NULL;
        }
        out = grown;
    }
    memcpy(out + len, read_start, tail_len);
    len += tail_len;
    out[len] = '\0';
    return out;
}

char *apex_postprocess_roman_lists_html(const char *html) {
    if (!html) {
        return NULL;
    }

    char *lower = apply_list_style_to_following_ol(html, ROMAN_COMMENT_LOWER,
                                                   " style=\"list-style-type: lower-roman\">");
    const char *src = lower ? lower : html;
    char *upper = apply_list_style_to_following_ol(src, ROMAN_COMMENT_UPPER,
                                                   " style=\"list-style-type: upper-roman\">");
    if (lower && upper) {
        free(lower);
        return upper;
    }
    return upper ? upper : lower;
}

static bool extract_ol_inner(const char *ol_open, const char *html_end,
                             const char **inner_start, size_t *inner_len,
                             const char **after_ol) {
    const char *close = strstr(ol_open, "</ol>");
    if (!close || close >= html_end) {
        return false;
    }
    const char *gt = strchr(ol_open, '>');
    if (!gt || gt >= close) {
        return false;
    }
    *inner_start = gt + 1;
    *inner_len = (size_t)(close - *inner_start);
    *after_ol = close + 5;
    return true;
}

char *apex_postprocess_list_continuation_html(const char *html) {
    if (!html || !strstr(html, LIST_CONTINUE_MARKER)) {
        return NULL;
    }

    size_t cap = strlen(html) + 256;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t len = 0;
    out[0] = '\0';
    bool changed = false;

    const char *read = html;
    const char *html_end = html + strlen(html);
    const size_t marker_len = sizeof(LIST_CONTINUE_MARKER) - 1;

    while (read < html_end) {
        const char *marker = strstr(read, LIST_CONTINUE_MARKER);
        if (!marker) {
            break;
        }

        const char *prev_close = NULL;
        for (const char *p = marker - 1; p >= html + 4; p--) {
            if (strncmp(p, "</ol>", 5) == 0) {
                prev_close = p;
                break;
            }
        }

        const char *after_marker = marker + marker_len;
        while (after_marker < html_end &&
               (*after_marker == ' ' || *after_marker == '\t' ||
                *after_marker == '\n' || *after_marker == '\r')) {
            after_marker++;
        }

        const char *next_ol = NULL;
        if (after_marker + 3 <= html_end && strncmp(after_marker, "<ol", 3) == 0) {
            next_ol = after_marker;
        }

        if (prev_close && next_ol) {
            const char *between = prev_close + 5;
            while (between < next_ol &&
                   (*between == ' ' || *between == '\t' ||
                    *between == '\n' || *between == '\r')) {
                between++;
            }
            if ((size_t)(next_ol - between) >= marker_len &&
                strncmp(between, LIST_CONTINUE_MARKER, marker_len) == 0) {
                between += marker_len;
                while (between < next_ol &&
                       (*between == ' ' || *between == '\t' ||
                        *between == '\n' || *between == '\r')) {
                    between++;
                }
            }

            const char *inner = NULL;
            size_t inner_len = 0;
            const char *after_next_ol = NULL;
            if (between >= next_ol &&
                extract_ol_inner(next_ol, html_end, &inner, &inner_len, &after_next_ol)) {
                size_t segment_len = (size_t)(prev_close - read);
                if (len + segment_len + inner_len + 1 > cap) {
                    cap = (len + segment_len + inner_len + 1) * 2;
                    char *grown = realloc(out, cap);
                    if (!grown) {
                        free(out);
                        return NULL;
                    }
                    out = grown;
                }
                if (segment_len > 0) {
                    memcpy(out + len, read, segment_len);
                    len += segment_len;
                }
                if (inner_len > 0) {
                    memcpy(out + len, inner, inner_len);
                    len += inner_len;
                }
                read = after_next_ol;
                changed = true;
                continue;
            }
        }

        size_t skip_len = (size_t)(marker - read);
        if (len + skip_len + 1 > cap) {
            cap = (len + skip_len + 1) * 2;
            char *grown = realloc(out, cap);
            if (!grown) {
                free(out);
                return NULL;
            }
            out = grown;
        }
        if (skip_len > 0) {
            memcpy(out + len, read, skip_len);
            len += skip_len;
        }
        read = marker + marker_len;
        changed = true;
    }

    if (!changed) {
        free(out);
        return NULL;
    }

    size_t tail_len = (size_t)(html_end - read);
    if (len + tail_len + 1 > cap) {
        cap = len + tail_len + 2;
        char *grown = realloc(out, cap);
        if (!grown) {
            free(out);
            return NULL;
        }
        out = grown;
    }
    memcpy(out + len, read, tail_len);
    len += tail_len;
    out[len] = '\0';
    return out;
}
