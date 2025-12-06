/**
 * Relaxed Tables Extension for Apex
 *
 * Supports tables without separator rows (Kramdown-style):
 * A | B
 * 1 | 2
 *
 * This preprocessing step detects such tables and inserts separator rows
 * so the existing table parser can handle them.
 */

#ifndef APEX_RELAXED_TABLES_H
#define APEX_RELAXED_TABLES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Process relaxed tables - detect tables without separator rows and insert them
 * @param text Input markdown text
 * @return Newly allocated text with separator rows inserted (must be freed), or NULL if no changes
 */
char *apex_process_relaxed_tables(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* APEX_RELAXED_TABLES_H */

