/**
 * File Includes Extension for Apex
 * Implementation
 */

#include "includes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <strings.h>

/**
 * Read file contents
 */
static char *read_file_contents(const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return NULL;

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 0 || size > 10 * 1024 * 1024) {  /* Limit to 10MB */
        fclose(fp);
        return NULL;
    }

    /* Read content */
    char *content = malloc(size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }

    size_t read = fread(content, 1, size, fp);
    content[read] = '\0';
    fclose(fp);

    return content;
}

/**
 * Resolve relative path from base directory
 */
static char *resolve_path(const char *filepath, const char *base_dir) {
    if (!filepath) return NULL;

    /* If absolute path, return as-is */
    if (filepath[0] == '/') {
        return strdup(filepath);
    }

    /* Relative path - combine with base_dir */
    if (!base_dir || !*base_dir) {
        return strdup(filepath);
    }

    size_t len = strlen(base_dir) + strlen(filepath) + 2;
    char *resolved = malloc(len);
    if (!resolved) return NULL;

    snprintf(resolved, len, "%s/%s", base_dir, filepath);
    return resolved;
}

/**
 * Get directory of a file path
 */
static char *get_directory(const char *filepath) {
    if (!filepath) return strdup(".");

    char *path_copy = strdup(filepath);
    if (!path_copy) return strdup(".");

    char *dir = dirname(path_copy);
    char *result = strdup(dir);
    free(path_copy);

    return result ? result : strdup(".");
}

/**
 * Check if a file exists
 */
bool apex_file_exists(const char *filepath) {
    if (!filepath) return false;
    struct stat st;
    return (stat(filepath, &st) == 0);
}

/**
 * File type enum
 */
typedef enum {
    FILE_TYPE_MARKDOWN,
    FILE_TYPE_IMAGE,
    FILE_TYPE_CODE,
    FILE_TYPE_HTML,
    FILE_TYPE_CSV,
    FILE_TYPE_TSV,
    FILE_TYPE_TEXT
} apex_file_type_t;

/**
 * Detect file type from extension
 */
static apex_file_type_t apex_detect_file_type(const char *filepath) {
    if (!filepath) return FILE_TYPE_TEXT;

    const char *ext = strrchr(filepath, '.');
    if (!ext) return FILE_TYPE_TEXT;
    ext++;

    /* Images */
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
        strcasecmp(ext, "png") == 0 || strcasecmp(ext, "gif") == 0 ||
        strcasecmp(ext, "webp") == 0 || strcasecmp(ext, "svg") == 0) {
        return FILE_TYPE_IMAGE;
    }

    /* CSV/TSV */
    if (strcasecmp(ext, "csv") == 0) return FILE_TYPE_CSV;
    if (strcasecmp(ext, "tsv") == 0) return FILE_TYPE_TSV;

    /* HTML */
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
        return FILE_TYPE_HTML;
    }

    /* Markdown */
    if (strcasecmp(ext, "md") == 0 || strcasecmp(ext, "markdown") == 0 ||
        strcasecmp(ext, "mmd") == 0) {
        return FILE_TYPE_MARKDOWN;
    }

    /* Code files */
    if (strcasecmp(ext, "c") == 0 || strcasecmp(ext, "h") == 0 ||
        strcasecmp(ext, "cpp") == 0 || strcasecmp(ext, "py") == 0 ||
        strcasecmp(ext, "js") == 0 || strcasecmp(ext, "java") == 0 ||
        strcasecmp(ext, "swift") == 0 || strcasecmp(ext, "go") == 0 ||
        strcasecmp(ext, "rs") == 0 || strcasecmp(ext, "sh") == 0) {
        return FILE_TYPE_CODE;
    }

    return FILE_TYPE_TEXT;
}

/**
 * Convert CSV/TSV to Markdown table
 */
static char *apex_csv_to_table(const char *csv_content, bool is_tsv) {
    if (!csv_content) return NULL;

    char delim = is_tsv ? '\t' : ',';
    size_t len = strlen(csv_content);
    char *output = malloc(len * 3);
    if (!output) return NULL;

    char *write = output;
    const char *line_start = csv_content;
    bool first_row = true;
    int col_count = 0;

    while (*line_start) {
        const char *line_end = strchr(line_start, '\n');
        if (!line_end) line_end = line_start + strlen(line_start);

        *write++ = '|';
        const char *cell_start = line_start;
        col_count = 0;

        while (cell_start < line_end) {
            const char *cell_end = cell_start;
            while (cell_end < line_end && *cell_end != delim) cell_end++;

            *write++ = ' ';
            while (cell_start < cell_end) {
                *write++ = *cell_start++;
            }
            *write++ = ' ';
            *write++ = '|';
            col_count++;

            if (cell_end < line_end) cell_start = cell_end + 1;
            else break;
        }
        *write++ = '\n';

        /* Add separator after first row */
        if (first_row) {
            *write++ = '|';
            for (int i = 0; i < col_count; i++) {
                strcpy(write, " --- |");
                write += 6;
            }
            *write++ = '\n';
            first_row = false;
        }

        line_start = line_end;
        if (*line_start == '\n') line_start++;
    }

    *write = '\0';
    return output;
}

/**
 * Resolve wildcard path (file.* -> file.html, file.md, etc.)
 * Tries extensions in order: .html, .md, .txt, .tex
 */
char *apex_resolve_wildcard(const char *filepath, const char *base_dir) {
    if (!filepath) return NULL;

    /* Check if this is a wildcard */
    const char *wildcard = strstr(filepath, ".*");
    if (!wildcard) {
        /* Not a wildcard, return as-is */
        return resolve_path(filepath, base_dir);
    }

    /* Extract base filename (before .*) */
    size_t base_len = wildcard - filepath;
    char base_filename[1024];
    if (base_len >= sizeof(base_filename)) return NULL;

    memcpy(base_filename, filepath, base_len);
    base_filename[base_len] = '\0';

    /* Try common extensions */
    const char *extensions[] = {".html", ".md", ".txt", ".tex", NULL};

    for (int i = 0; extensions[i]; i++) {
        char test_path[1024];
        snprintf(test_path, sizeof(test_path), "%s%s", base_filename, extensions[i]);

        char *resolved = resolve_path(test_path, base_dir);
        if (resolved && apex_file_exists(resolved)) {
            return resolved;
        }
        free(resolved);
    }

    /* No match found, return NULL */
    return NULL;
}

/**
 * Process file includes in text
 */
char *apex_process_includes(const char *text, const char *base_dir, int depth) {
    if (!text) return NULL;
    if (depth > MAX_INCLUDE_DEPTH) {
        return strdup(text);  /* Silently return original text */
    }

    size_t text_len = strlen(text);
    size_t output_capacity = text_len * 10;  /* Generous for includes */
    if (output_capacity < 1024 * 1024) output_capacity = 1024 * 1024;  /* At least 1MB */
    char *output = malloc(output_capacity);
    if (!output) return NULL;

    const char *read_pos = text;
    char *write_pos = output;
    size_t remaining = output_capacity;

    while (*read_pos) {
        bool processed_include = false;

        /* Look for iA Writer transclusion /filename (at start of line only) */
        if (read_pos[0] == '/' && (read_pos == text || read_pos[-1] == '\n')) {
            const char *filepath_start = read_pos + 1;
            const char *filepath_end = filepath_start;

            /* Find end of filename */
            while (*filepath_end && *filepath_end != ' ' && *filepath_end != '\t' &&
                   *filepath_end != '\n' && *filepath_end != '\r') {
                filepath_end++;
            }

            if (filepath_end > filepath_start && (filepath_end - filepath_start) < 1024) {
                char filepath[1024];
                size_t filepath_len = filepath_end - filepath_start;
                memcpy(filepath, filepath_start, filepath_len);
                filepath[filepath_len] = '\0';

                /* Resolve and check file exists */
                char *resolved_path = resolve_path(filepath, base_dir);
                if (resolved_path && apex_file_exists(resolved_path)) {
                    apex_file_type_t file_type = apex_detect_file_type(resolved_path);
                    char *content = read_file_contents(resolved_path);

                    if (content) {
                        char *to_insert = NULL;

                        if (file_type == FILE_TYPE_IMAGE) {
                            /* Image: create ![](path) */
                            to_insert = malloc(strlen(filepath) + 10);
                            if (to_insert) sprintf(to_insert, "![](%s)\n", filepath);
                        } else if (file_type == FILE_TYPE_CSV || file_type == FILE_TYPE_TSV) {
                            /* CSV/TSV: convert to table */
                            to_insert = apex_csv_to_table(content, file_type == FILE_TYPE_TSV);
                        } else if (file_type == FILE_TYPE_CODE) {
                            /* Code: wrap in fenced code block */
                            const char *ext = strrchr(filepath, '.');
                            const char *lang = ext ? ext + 1 : "";
                            to_insert = malloc(strlen(content) + strlen(lang) + 20);
                            if (to_insert) sprintf(to_insert, "\n```%s\n%s\n```\n", lang, content);
                        } else {
                            /* Text/Markdown: process and include */
                            char *file_dir = get_directory(resolved_path);
                            to_insert = apex_process_includes(content, file_dir, depth + 1);
                            free(file_dir);
                        }

                        if (to_insert) {
                            size_t insert_len = strlen(to_insert);
                            if (insert_len < remaining) {
                                memcpy(write_pos, to_insert, insert_len);
                                write_pos += insert_len;
                                remaining -= insert_len;
                            }
                            if (to_insert != content) free(to_insert);
                        }

                        free(content);
                        free(resolved_path);
                        read_pos = filepath_end;
                        processed_include = true;
                    } else {
                        free(resolved_path);
                    }
                } else if (resolved_path) {
                    free(resolved_path);
                }
            }
        }

        /* Look for MMD transclusion {{file}} */
        if (!processed_include && read_pos[0] == '{' && read_pos[1] == '{') {
            const char *filepath_start = read_pos + 2;
            const char *filepath_end = strstr(filepath_start, "}}");

            if (filepath_end && (filepath_end - filepath_start) > 0 && (filepath_end - filepath_start) < 1024) {
                /* Extract filepath */
                int filepath_len = filepath_end - filepath_start;
                char filepath[1024];
                memcpy(filepath, filepath_start, filepath_len);
                filepath[filepath_len] = '\0';

                /* Resolve path (handle wildcards) */
                char *resolved_path = apex_resolve_wildcard(filepath, base_dir);
                if (!resolved_path) {
                    /* Try without wildcard resolution */
                    resolved_path = resolve_path(filepath, base_dir);
                }

                if (resolved_path) {
                    apex_file_type_t file_type = apex_detect_file_type(resolved_path);
                    char *content = read_file_contents(resolved_path);
                    if (content) {
                        char *to_process = content;
                        bool free_to_process = false;

                        /* Convert CSV/TSV to table */
                        if (file_type == FILE_TYPE_CSV || file_type == FILE_TYPE_TSV) {
                            char *table = apex_csv_to_table(content, file_type == FILE_TYPE_TSV);
                            if (table) {
                                to_process = table;
                                free_to_process = true;
                            }
                        }

                        /* Recursively process */
                        char *file_dir = get_directory(resolved_path);
                        char *processed = apex_process_includes(to_process, file_dir, depth + 1);
                        free(file_dir);

                        if (processed) {
                            size_t proc_len = strlen(processed);
                            if (proc_len < remaining) {
                                memcpy(write_pos, processed, proc_len);
                                write_pos += proc_len;
                                remaining -= proc_len;
                            }
                            free(processed);
                        }

                        if (free_to_process) free(to_process);
                        free(content);

                        read_pos = filepath_end + 2;
                        free(resolved_path);
                        processed_include = true;
                    } else {
                        free(resolved_path);
                    }
                }
            }
        }

        /* Look for << (Marked syntax) */
        if (!processed_include && read_pos[0] == '<' && read_pos[1] == '<') {
            char bracket_type = 0;
            const char *filepath_start = NULL;
            const char *filepath_end = NULL;

            /* Determine include type */
            if (read_pos[2] == '[') {
                /* <<[file.md] - Markdown include */
                bracket_type = '[';
                filepath_start = read_pos + 3;
                filepath_end = strchr(filepath_start, ']');
            } else if (read_pos[2] == '(') {
                /* <<(file.ext) - Code block include */
                bracket_type = '(';
                filepath_start = read_pos + 3;
                filepath_end = strchr(filepath_start, ')');
            } else if (read_pos[2] == '{') {
                /* <<{file.html} - Raw HTML include */
                bracket_type = '{';
                filepath_start = read_pos + 3;
                filepath_end = strchr(filepath_start, '}');
            }

            if (bracket_type && filepath_start && filepath_end) {
                /* Extract filepath */
                int filepath_len = filepath_end - filepath_start;
                char filepath[1024];
                if (filepath_len > 0 && filepath_len < sizeof(filepath)) {
                    memcpy(filepath, filepath_start, filepath_len);
                    filepath[filepath_len] = '\0';

                    /* Resolve path */
                    char *resolved_path = resolve_path(filepath, base_dir);
                    if (resolved_path) {
                        apex_file_type_t file_type = apex_detect_file_type(resolved_path);
                        char *content = read_file_contents(resolved_path);
                        if (content) {
                            /* Process based on include type */
                            if (bracket_type == '[') {
                                char *to_process = content;
                                bool free_to_process = false;

                                /* Convert CSV/TSV to table */
                                if (file_type == FILE_TYPE_CSV || file_type == FILE_TYPE_TSV) {
                                    char *table = apex_csv_to_table(content, file_type == FILE_TYPE_TSV);
                                    if (table) {
                                        to_process = table;
                                        free_to_process = true;
                                    }
                                }

                                /* Markdown include - recursively process */
                                char *file_dir = get_directory(resolved_path);
                                char *processed = apex_process_includes(to_process, file_dir, depth + 1);
                                free(file_dir);

                                if (processed) {
                                    size_t proc_len = strlen(processed);
                                    if (proc_len < remaining) {
                                        memcpy(write_pos, processed, proc_len);
                                        write_pos += proc_len;
                                        remaining -= proc_len;
                                    }
                                    free(processed);
                                }

                                if (free_to_process) free(to_process);
                            } else if (bracket_type == '(') {
                                /* Code block include - wrap in code fence */
                                /* Try to detect language from extension */
                                const char *ext = strrchr(filepath, '.');
                                const char *lang = "";
                                if (ext) {
                                    ext++;
                                    if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0) lang = "c";
                                    else if (strcmp(ext, "cpp") == 0 || strcmp(ext, "cc") == 0) lang = "cpp";
                                    else if (strcmp(ext, "py") == 0) lang = "python";
                                    else if (strcmp(ext, "js") == 0) lang = "javascript";
                                    else if (strcmp(ext, "rb") == 0) lang = "ruby";
                                    else if (strcmp(ext, "sh") == 0) lang = "bash";
                                    else lang = ext;
                                }

                                char code_header[128];
                                snprintf(code_header, sizeof(code_header), "\n```%s\n", lang);
                                const char *code_footer = "\n```\n";

                                size_t total_len = strlen(code_header) + strlen(content) + strlen(code_footer);
                                if (total_len < remaining) {
                                    strcpy(write_pos, code_header);
                                    write_pos += strlen(code_header);
                                    strcpy(write_pos, content);
                                    write_pos += strlen(content);
                                    strcpy(write_pos, code_footer);
                                    write_pos += strlen(code_footer);
                                    remaining -= total_len;
                                }
                            } else if (bracket_type == '{') {
                                /* Raw HTML - will be inserted after processing */
                                /* For now, insert a placeholder marker */
                                char marker[1024];
                                snprintf(marker, sizeof(marker), "<!--APEX_RAW_INCLUDE:%s-->", resolved_path);
                                size_t marker_len = strlen(marker);
                                if (marker_len < remaining) {
                                    memcpy(write_pos, marker, marker_len);
                                    write_pos += marker_len;
                                    remaining -= marker_len;
                                }
                            }

                            free(content);
                        }
                        free(resolved_path);
                    }

                    /* Skip past the include syntax */
                    read_pos = filepath_end + 1;
                    processed_include = true;
                }
            }
        }

        /* Not an include, copy character */
        if (!processed_include) {
            if (remaining > 0) {
                *write_pos++ = *read_pos;
                remaining--;
            }
            read_pos++;
        }
    }

    *write_pos = '\0';
    return output;
}

