// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lng_check.c: the check of the three language tables against the tree and against each other (lng.h). lng_check.sh
// runs it from the top of the tree.
//
//     src/lng/lng_check.sh
//
// The check prints each defect, and the run exits 1 where it finds one. A finding does not fail the run. The run exits
// 2 where it cannot read a table, the schema or gnascor
#include "lng.h"
#include "verba_scribo/verba_scribo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LNG_SCHEMA "src/cu/engine/rmc/machine_ir_types.h"
#define LNG_GNASCOR "src/cu/transpiler/gnascor.md"
#define LNG_FORMS "src/cu/transpiler/lstar/protocol/table/cu.krs"

static KfileLines s_file;

// the file at `path` read into s_file, or the run ended where it contains no line
static void lng_check_read(const char *path)
{
    if (!kfile_lines_read(&s_file, path) || (s_file.lines == 0u))
    {
        fprintf(stderr, "lng_check: %s could not be read\n", path);
        exit(2);
    }
}

// the table at `path` read into `table`, the defects of its form appended to `defects`
static void lng_check_table(const char *path, LngTable *table, LngList *defects)
{
    lng_check_read(path);
    lng_table_read((const char *const *)s_file.line, s_file.lines, table, defects);
}

// The words of one file, `file_used`, against `table` for the file at `path`, each defect appended to `defects`.
// Then each word goes into `used`, the tree's words, where the tree's map has none. Last, `file_used` empties for the
// next file
static void lng_check_file_words(const LngTable *table, const char *path, LngMap *file_used, LngMap *used,
                                 LngList *defects)
{
    lng_used_check(table, path, file_used, defects);
    for (unsigned int at = 0u; at < file_used->key.count; at += 1u)
    {
        lng_map_put(used, file_used->key.text[at], file_used->value.text[at]);
    }
    lng_map_free(file_used);
}

int main(void)
{
    LngList defects = {NULL, 0u, 0u};
    LngList findings = {NULL, 0u, 0u};
    LngTable table = {{NULL, 0u, 0u}, NULL, 0u, 0u};
    LngTable hol = {{NULL, 0u, 0u}, NULL, 0u, 0u};
    LngTable asm_table = {{NULL, 0u, 0u}, NULL, 0u, 0u};
    lng_check_table("src/lng/transpiler_lng.tsv", &table, &defects);
    lng_table_check(&table, g_lng_types, LNG_KINDS, &defects);
    lng_table_findings(&table, &findings);
    lng_check_table("src/lng/gnascor_hol_lng.tsv", &hol, &defects);
    lng_table_check(&hol, g_lng_hol_types, LNG_HOL_KINDS, &defects);
    lng_table_findings(&hol, &findings);
    lng_check_table("src/lng/gnascor_asm_lng.tsv", &asm_table, &defects);

    // every word of the tree. The check reads the words of each file against the rows with no target and the rows
    // of the target of that file
    LngMap used = {{NULL, 0u, 0u}, {NULL, 0u, 0u}};
    LngMap file_used = {{NULL, 0u, 0u}, {NULL, 0u, 0u}};
    LngList used_names = {NULL, 0u, 0u};
    lng_check_read(LNG_SCHEMA);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        lng_schema_forms(s_file.line[line], &used_names);
    }
    for (unsigned int name = 0u; name < used_names.count; name += 1u)
    {
        LngList words = {NULL, 0u, 0u};
        LngList where = {NULL, 0u, 0u};
        lng_words_of(used_names.text[name], &words);
        lng_list_printed(&where, "%s %s", LNG_SCHEMA, used_names.text[name]);
        for (unsigned int word = 0u; word < words.count; word += 1u)
        {
            lng_map_put(&file_used, words.text[word], where.text[0]);
        }
        lng_list_free(&where);
        lng_list_free(&words);
    }
    lng_check_file_words(&table, "", &file_used, &used, &defects);
    static const char *const s_suffixes[3] = {".krs", ".ksc", ".kdm"};
    LngList found[3] = {{NULL, 0u, 0u}, {NULL, 0u, 0u}, {NULL, 0u, 0u}};
    lng_files_ending(s_suffixes, 3u, found);
    const LngList rulesets = found[0];
    const LngList kscs = found[1];
    const LngList kdms = found[2];
    for (unsigned int at = 0u; at < rulesets.count; at += 1u)
    {
        kfile_lines_read(&s_file, rulesets.text[at]);
        lng_ruleset_words(rulesets.text[at], (const char *const *)s_file.line, s_file.lines, &file_used);
        lng_check_file_words(&table, rulesets.text[at], &file_used, &used, &defects);
    }
    for (unsigned int at = 0u; at < kscs.count; at += 1u)
    {
        kfile_lines_read(&s_file, kscs.text[at]);
        lng_ksc_words(kscs.text[at], &s_file, &file_used);
        lng_check_file_words(&table, kscs.text[at], &file_used, &used, &defects);
    }
    for (unsigned int at = 0u; at < (kscs.count + kdms.count); at += 1u)
    {
        const char *const path = (at < kscs.count) ? kscs.text[at] : kdms.text[at - kscs.count];
        LngList lines = {NULL, 0u, 0u};
        kfile_lines_read(&s_file, path);
        lng_ruleset_lines(&s_file, &lines);
        lng_ruleset_words(path, (const char *const *)lines.text, lines.count, &file_used);
        lng_check_file_words(&table, path, &file_used, &used, &defects);
        lng_list_free(&lines);
    }

    // gnascor's word map, read whole
    lng_check_read(LNG_GNASCOR);
    static char s_text_bytes[QRY_WRITER_RING_BYTES / 2ULL];
    size_t joined = 0u;
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        joined = verba_textus.put(&(VerbaTextusCfg){
            .out = s_text_bytes, .cap = sizeof(s_text_bytes), .at = joined, .text = s_file.line[line]});
        joined = verba_littera.ch(
            &(VerbaLitteraCfg){.out = s_text_bytes, .cap = sizeof(s_text_bytes), .at = joined, .ch = '\n'});
    }
    (void)verba_finis.finish(&(VerbaFinisCfg){.out = s_text_bytes, .cap = sizeof(s_text_bytes), .at = joined});
    LngList listed[3] = {{NULL, 0u, 0u}, {NULL, 0u, 0u}, {NULL, 0u, 0u}};
    static const char *const s_listed_type[3] = {"doing", "gluing", "thing"};
    lng_gnascor_lists(s_text_bytes, &listed[2], &listed[0], &listed[1]);
    LngList listed_words = {NULL, 0u, 0u};
    for (unsigned int type = 0u; type < 3u; type += 1u)
    {
        for (unsigned int at = 0u; at < listed[type].count; at += 1u)
        {
            lng_set_add(&listed_words, listed[type].text[at]);
        }
    }
    for (unsigned int at = 0u; at < listed_words.count; at += 1u)
    {
        for (unsigned int type = 0u; type < 3u; type += 1u)
        {
            unsigned int place = 0u;
            if (!lng_set_find(&listed[type], listed_words.text[at], &place))
            {
                continue;
            }
            int found = 0;
            for (unsigned int row = 0u; (row < table.rows) && !found; row += 1u)
            {
                // gnascor lists the transpiler's words, and no target's row answers for one
                found = (lng_cell(&table, row, "target")[0] == '\0') &&
                        (strcmp(lng_cell(&table, row, "word"), listed_words.text[at]) == 0) &&
                        (strcmp(lng_cell(&table, row, "type"), s_listed_type[type]) == 0);
            }
            if (!found)
            {
                lng_list_printed(&defects, "gnascor lists %s as a %s word and the table does not",
                                 listed_words.text[at], s_listed_type[type]);
            }
        }
    }

    // each name the schema lists, between two underscores, for a search that finds only a whole word in it
    LngList names = {NULL, 0u, 0u};
    for (unsigned int at = 0u; at < used_names.count; at += 1u)
    {
        LngList marked = {NULL, 0u, 0u};
        lng_list_printed(&marked, "_%s_", used_names.text[at]);
        lng_set_add(&names, marked.text[0]);
        lng_list_free(&marked);
    }
    for (unsigned int row = 0u; row < table.rows; row += 1u)
    {
        const char *const word = lng_cell(&table, row, "word");
        const char *const type = lng_cell(&table, row, "type");
        const int naming =
            (strcmp(type, "thing") == 0) || (strcmp(type, "doing") == 0) || (strcmp(type, "gluing") == 0);
        int named = (lng_map_get(&used, word) != NULL);
        if (!naming || named)
        {
            continue;
        }
        LngList marked = {NULL, 0u, 0u};
        lng_list_printed(&marked, "_%s_", word);
        for (unsigned int at = 0u; (at < names.count) && !named; at += 1u)
        {
            named = (strstr(names.text[at], marked.text[0]) != NULL);
        }
        lng_list_free(&marked);
        if (!named)
        {
            lng_list_printed(&findings, "%s is a %s word nothing in the tree uses", word, type);
        }
    }

    // the forms of cu.krs, each its name and its text
    LngList form_names = {NULL, 0u, 0u};
    LngList form_texts = {NULL, 0u, 0u};
    kfile_lines_read(&s_file, LNG_FORMS);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        const char *const text_line = s_file.line[line];
        const char *const equals = strchr(text_line, '=');
        if ((strncmp(text_line, "form ", 5u) != 0) || (equals == NULL))
        {
            continue;
        }
        const char *const name = text_line + 5;
        const size_t space = strspn(name, " \t\v\f\r\x1c\x1d\x1e\x1f");
        const size_t length = strcspn(&name[space], " \t\v\f\r\x1c\x1d\x1e\x1f=");
        if ((&name[space] >= equals) || (length == 0u))
        {
            continue;
        }
        LngList cut = {NULL, 0u, 0u};
        lng_list_printed(&cut, "%.*s", (int)length, &name[space]);
        lng_list_add(&form_names, cut.text[0]);
        lng_list_free(&cut);
        lng_list_add(&form_texts, equals + 1);
    }
    lng_form_findings(&table, &form_names, &form_texts, &findings);
    lng_hol_check(&table, &hol, &defects, &findings);
    lng_asm_check(&asm_table, &defects, &findings);

    for (unsigned int at = 0u; at < defects.count; at += 1u)
    {
        printf("defect: %s\n", defects.text[at]);
    }
    for (unsigned int at = 0u; at < findings.count; at += 1u)
    {
        printf("finding: %s\n", findings.text[at]);
    }
    printf("%u defects, %u findings, %u words, %u high order words, %u transitions\n", defects.count, findings.count,
           table.rows, hol.rows, asm_table.rows);
    const int failed = (defects.count != 0u);
    lng_list_free(&form_texts);
    lng_list_free(&form_names);
    lng_list_free(&names);
    lng_list_free(&listed_words);
    for (unsigned int type = 0u; type < 3u; type += 1u)
    {
        lng_list_free(&listed[type]);
    }
    for (unsigned int suffix = 0u; suffix < 3u; suffix += 1u)
    {
        lng_list_free(&found[suffix]);
    }
    lng_list_free(&used_names);
    lng_map_free(&used);
    lng_table_free(&asm_table);
    lng_table_free(&hol);
    lng_table_free(&table);
    lng_list_free(&findings);
    lng_list_free(&defects);
    kfile_lines_free(&s_file);
    return failed ? 1 : 0;
}
