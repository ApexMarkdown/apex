/**
 * Apex CLI - Command-line interface for the Apex Markdown processor
 */

#include "apex/apex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define BUFFER_SIZE 4096

static void print_usage(const char *program_name) {
    fprintf(stderr, "Apex Markdown Processor v%s\n", apex_version_string());
    fprintf(stderr, "One Markdown processor to rule them all\n\n");
    fprintf(stderr, "Usage: %s [options] [file]\n\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --accept               Accept all Critic Markup changes (apply edits)\n");
    fprintf(stderr, "  --enable-includes      Enable file inclusion\n");
    fprintf(stderr, "  --hardbreaks           Treat newlines as hard breaks\n");
    fprintf(stderr, "  -h, --help             Show this help message\n");
    fprintf(stderr, "  --header-anchors        Generate <a> anchor tags instead of header IDs\n");
    fprintf(stderr, "  --id-format FORMAT      Header ID format: gfm (default), mmd, or kramdown\n");
    fprintf(stderr, "                          (modes auto-set format; use this to override in unified mode)\n");
    fprintf(stderr, "  --[no-]alpha-lists     Support alpha list markers (a., b., c. and A., B., C.)\n");
    fprintf(stderr, "  --[no-]mixed-lists     Allow mixed list markers at same level (inherit type from first item)\n");
    fprintf(stderr, "  -m, --mode MODE        Processor mode: commonmark, gfm, mmd, kramdown, unified (default)\n");
    fprintf(stderr, "  --no-footnotes         Disable footnote support\n");
    fprintf(stderr, "  --no-ids                Disable automatic header ID generation\n");
    fprintf(stderr, "  --no-math              Disable math support\n");
    fprintf(stderr, "  --no-smart             Disable smart typography\n");
    fprintf(stderr, "  --no-tables            Disable table support\n");
    fprintf(stderr, "  -o, --output FILE      Write output to FILE instead of stdout\n");
    fprintf(stderr, "  --pretty               Pretty-print HTML with indentation and whitespace\n");
    fprintf(stderr, "  --[no-]relaxed-tables  Enable relaxed table parsing (no separator rows required)\n");
    fprintf(stderr, "  --[no-]sup-sub         Enable MultiMarkdown-style superscript (^text^) and subscript (~text~) syntax\n");
    fprintf(stderr, "  --[no-]unsafe          Allow raw HTML in output (default: true for unified/mmd/kramdown, false for commonmark/gfm)\n");
    fprintf(stderr, "  --reject               Reject all Critic Markup changes (revert edits)\n");
    fprintf(stderr, "  -s, --standalone       Generate complete HTML document (with <html>, <head>, <body>)\n");
    fprintf(stderr, "  --style FILE           Link to CSS file in document head (requires --standalone)\n");
    fprintf(stderr, "  --title TITLE          Document title (requires --standalone, default: \"Document\")\n");
    fprintf(stderr, "  -v, --version          Show version information\n\n");
    fprintf(stderr, "If no file is specified, reads from stdin.\n");
}

static void print_version(void) {
    printf("Apex %s\n", apex_version_string());
    printf("Copyright (c) 2025 Brett Terpstra\n");
    printf("Licensed under MIT License\n");
}

static char *read_file(const char *filename, size_t *len) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return NULL;
    }

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(fp);
        fprintf(stderr, "Error: Cannot determine file size\n");
        return NULL;
    }

    /* Allocate buffer */
    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    /* Read file */
    size_t bytes_read = fread(buffer, 1, file_size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    if (len) *len = bytes_read;
    return buffer;
}

static char *read_stdin(size_t *len) {
    size_t capacity = BUFFER_SIZE;
    size_t size = 0;
    char *buffer = malloc(capacity);

    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    size_t bytes_read;
    while ((bytes_read = fread(buffer + size, 1, BUFFER_SIZE, stdin)) > 0) {
        size += bytes_read;
        if (size + BUFFER_SIZE > capacity) {
            capacity *= 2;
            char *new_buffer = realloc(buffer, capacity);
            if (!new_buffer) {
                free(buffer);
                fprintf(stderr, "Error: Memory allocation failed\n");
                return NULL;
            }
            buffer = new_buffer;
        }
    }

    buffer[size] = '\0';
    if (len) *len = size;
    return buffer;
}

int main(int argc, char *argv[]) {
    apex_options options = apex_options_default();
    const char *input_file = NULL;
    const char *output_file = NULL;

    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mode") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --mode requires an argument\n");
                return 1;
            }
            if (strcmp(argv[i], "commonmark") == 0) {
                options = apex_options_for_mode(APEX_MODE_COMMONMARK);
            } else if (strcmp(argv[i], "gfm") == 0) {
                options = apex_options_for_mode(APEX_MODE_GFM);
            } else if (strcmp(argv[i], "mmd") == 0 || strcmp(argv[i], "multimarkdown") == 0) {
                options = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
            } else if (strcmp(argv[i], "kramdown") == 0) {
                options = apex_options_for_mode(APEX_MODE_KRAMDOWN);
            } else if (strcmp(argv[i], "unified") == 0) {
                options = apex_options_for_mode(APEX_MODE_UNIFIED);
            } else {
                fprintf(stderr, "Error: Unknown mode '%s'\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --output requires an argument\n");
                return 1;
            }
            output_file = argv[i];
        } else if (strcmp(argv[i], "--no-tables") == 0) {
            options.enable_tables = false;
        } else if (strcmp(argv[i], "--no-footnotes") == 0) {
            options.enable_footnotes = false;
        } else if (strcmp(argv[i], "--no-smart") == 0) {
            options.enable_smart_typography = false;
        } else if (strcmp(argv[i], "--no-math") == 0) {
            options.enable_math = false;
        } else if (strcmp(argv[i], "--enable-includes") == 0) {
            options.enable_file_includes = true;
        } else if (strcmp(argv[i], "--hardbreaks") == 0) {
            options.hardbreaks = true;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--standalone") == 0) {
            options.standalone = true;
        } else if (strcmp(argv[i], "--style") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --style requires an argument\n");
                return 1;
            }
            options.stylesheet_path = argv[i];
            options.standalone = true;  /* Imply standalone if style is specified */
        } else if (strcmp(argv[i], "--title") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --title requires an argument\n");
                return 1;
            }
            options.document_title = argv[i];
        } else if (strcmp(argv[i], "--pretty") == 0) {
            options.pretty = true;
        } else if (strcmp(argv[i], "--accept") == 0) {
            options.enable_critic_markup = true;
            options.critic_mode = 0;  /* CRITIC_ACCEPT */
        } else if (strcmp(argv[i], "--reject") == 0) {
            options.enable_critic_markup = true;
            options.critic_mode = 1;  /* CRITIC_REJECT */
        } else if (strcmp(argv[i], "--id-format") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --id-format requires an argument (gfm, mmd, or kramdown)\n");
                return 1;
            }
            if (strcmp(argv[i], "gfm") == 0) {
                options.id_format = 0;  /* GFM format */
            } else if (strcmp(argv[i], "mmd") == 0) {
                options.id_format = 1;  /* MMD format */
            } else if (strcmp(argv[i], "kramdown") == 0) {
                options.id_format = 2;  /* Kramdown format */
            } else {
                fprintf(stderr, "Error: --id-format must be 'gfm', 'mmd', or 'kramdown'\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--no-ids") == 0) {
            options.generate_header_ids = false;
        } else if (strcmp(argv[i], "--header-anchors") == 0) {
            options.header_anchors = true;
        } else if (strcmp(argv[i], "--relaxed-tables") == 0) {
            options.relaxed_tables = true;
        } else if (strcmp(argv[i], "--no-relaxed-tables") == 0) {
            options.relaxed_tables = false;
        } else if (strcmp(argv[i], "--alpha-lists") == 0) {
            options.allow_alpha_lists = true;
        } else if (strcmp(argv[i], "--no-alpha-lists") == 0) {
            options.allow_alpha_lists = false;
        } else if (strcmp(argv[i], "--mixed-lists") == 0) {
            options.allow_mixed_list_markers = true;
        } else if (strcmp(argv[i], "--no-mixed-lists") == 0) {
            options.allow_mixed_list_markers = false;
        } else if (strcmp(argv[i], "--unsafe") == 0) {
            options.unsafe = true;
        } else if (strcmp(argv[i], "--no-unsafe") == 0) {
            options.unsafe = false;
        } else if (strcmp(argv[i], "--sup-sub") == 0) {
            options.enable_sup_sub = true;
        } else if (strcmp(argv[i], "--no-sup-sub") == 0) {
            options.enable_sup_sub = false;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            /* Assume it's the input file */
            input_file = argv[i];
        }
    }

    /* Read input */
    size_t input_len;
    char *markdown;

    if (input_file) {
        markdown = read_file(input_file, &input_len);
    } else {
        markdown = read_stdin(&input_len);
    }

    if (!markdown) {
        return 1;
    }

    /* Convert to HTML */
    char *html = apex_markdown_to_html(markdown, input_len, &options);
    free(markdown);

    if (!html) {
        fprintf(stderr, "Error: Conversion failed\n");
        return 1;
    }

    /* Write output */
    if (output_file) {
        FILE *fp = fopen(output_file, "w");
        if (!fp) {
            fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
            apex_free_string(html);
            return 1;
        }
        fputs(html, fp);
        fclose(fp);
    } else {
        fputs(html, stdout);
    }

    apex_free_string(html);
    return 0;
}
