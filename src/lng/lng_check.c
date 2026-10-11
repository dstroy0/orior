// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lng_check.c: the three language tables held against the tree and against each other (lng.h), run from the top of
// the tree by lng_check.sh.
//
//     src/lng/lng_check.sh
//
// Each defect is printed and the run exits 1. A finding does not fail the run. Exit 2 where a table, the schema or
// gnascor could not be read
#include "lng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LNG_SCHEMA "src/cu/engine/rmc/machine_ir_types.h"
#define LNG_GNASCOR "src/cu/transpiler/gnascor.md"
#define LNG_FORMS "src/cu/transpiler/lstar/protocol/table/cu.krs"

static KtextFile s_file;

// the file at `path` read into s_file, or the run ended where it holds no line
static void lng_check_read(const char *path)
{
    if (!ktext_read(&s_file, path) || (s_file.lines == 0u))
    {
        fprintf(stderr, "lng_check: %s could not be read\n", path);
        exit(2);
    }
}

// the table at `path` read into `table`, the defects of its form kept after `defects`
static void lng_check_table(const char *path, LngTable *table, LngList *defects)
{
    lng_check_read(path);
    lng_table_read((const char *const *)s_file.line, s_file.lines, table, defects);
}

int main(void)
{
    LngList defects = {NULL, 0u, 0u};
    LngList findings = {NULL, 0u, 0u};
    LngTable table = {{NULL, 0u, 0u}, NULL, 0u, 0u};
    LngTable hol = {{NULL, 0u, 0u}, NULL, 0u, 0u};
    LngTable asm_table = {{NULL, 0u, 0u}, NULL, 0u, 0u};
    lng_check_table("src/lng/transpiler_lng.tsv", &table, &defects);
    lng_table_check(&table, g_lng_kinds, LNG_KINDS, &defects);
    lng_table_findings(&table, &findings);
    lng_check_table("src/lng/gnascor_hol_lng.tsv", &hol, &defects);
    lng_table_check(&hol, g_lng_hol_kinds, LNG_HOL_KINDS, &defects);
    lng_table_findings(&hol, &findings);
    lng_check_table("src/lng/gnascor_asm_lng.tsv", &asm_table, &defects);

    LngList held = {NULL, 0u, 0u};
    for (unsigned int row = 0u; row < table.rows; row += 1u)
    {
        lng_set_add(&held, lng_cell(&table, row, "word"));
    }
    LngMap used = {{NULL, 0u, 0u}, {NULL, 0u, 0u}};
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
            lng_map_put(&used, words.text[word], where.text[0]);
        }
        lng_list_free(&where);
        lng_list_free(&words);
    }
    static const char *const s_suffixes[3] = {".krs", ".ksc", ".kdm"};
    LngList found[3] = {{NULL, 0u, 0u}, {NULL, 0u, 0u}, {NULL, 0u, 0u}};
    lng_files_ending(s_suffixes, 3u, found);
    const LngList rulesets = found[0];
    const LngList kscs = found[1];
    const LngList kdms = found[2];
    for (unsigned int at = 0u; at < rulesets.count; at += 1u)
    {
        ktext_read(&s_file, rulesets.text[at]);
        lng_ruleset_words(rulesets.text[at], (const char *const *)s_file.line, s_file.lines, &used);
    }
    for (unsigned int at = 0u; at < kscs.count; at += 1u)
    {
        ktext_read(&s_file, kscs.text[at]);
        lng_ksc_words(kscs.text[at], &s_file, &used);
    }
    for (unsigned int at = 0u; at < (kscs.count + kdms.count); at += 1u)
    {
        const char *const path = (at < kscs.count) ? kscs.text[at] : kdms.text[at - kscs.count];
        LngList kept = {NULL, 0u, 0u};
        ktext_read(&s_file, path);
        lng_ruleset_lines(&s_file, &kept);
        lng_ruleset_words(path, (const char *const *)kept.text, kept.count, &used);
        lng_list_free(&kept);
    }
    for (unsigned int at = 0u; at < used.key.count; at += 1u)
    {
        unsigned int place = 0u;
        if (!lng_set_find(&held, used.key.text[at], &place))
        {
            lng_list_printed(&defects, "%s is used at %s and no table holds it", used.key.text[at],
                             used.value.text[at]);
        }
    }

    // gnascor's word map, read whole
    lng_check_read(LNG_GNASCOR);
    QryText text = {NULL, 0ull, 0ull, 0};
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        qry_text_printed(&text, "%s\n", s_file.line[line]);
    }
    LngList listed[3] = {{NULL, 0u, 0u}, {NULL, 0u, 0u}, {NULL, 0u, 0u}};
    static const char *const s_listed_kind[3] = {"doing", "gluing", "thing"};
    lng_gnascor_lists((text.bytes != NULL) ? text.bytes : "", &listed[2], &listed[0], &listed[1]);
    qry_text_freed(&text);
    LngList listed_words = {NULL, 0u, 0u};
    for (unsigned int kind = 0u; kind < 3u; kind += 1u)
    {
        for (unsigned int at = 0u; at < listed[kind].count; at += 1u)
        {
            lng_set_add(&listed_words, listed[kind].text[at]);
        }
    }
    for (unsigned int at = 0u; at < listed_words.count; at += 1u)
    {
        for (unsigned int kind = 0u; kind < 3u; kind += 1u)
        {
            unsigned int place = 0u;
            if (!lng_set_find(&listed[kind], listed_words.text[at], &place))
            {
                continue;
            }
            int found = 0;
            for (unsigned int row = 0u; (row < table.rows) && !found; row += 1u)
            {
                found = (strcmp(lng_cell(&table, row, "word"), listed_words.text[at]) == 0) &&
                        (strcmp(lng_cell(&table, row, "kind"), s_listed_kind[kind]) == 0);
            }
            if (!found)
            {
                lng_list_printed(&defects, "gnascor lists %s as a %s word and the table does not",
                                 listed_words.text[at], s_listed_kind[kind]);
            }
        }
    }

    // each name the schema lists, between two underscores, for a word to be found in it whole
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
        const char *const kind = lng_cell(&table, row, "kind");
        const int naming =
            (strcmp(kind, "thing") == 0) || (strcmp(kind, "doing") == 0) || (strcmp(kind, "gluing") == 0);
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
            lng_list_printed(&findings, "%s is a %s word nothing in the tree uses", word, kind);
        }
    }

    // the forms of cu.krs, each its name and its text
    LngList form_names = {NULL, 0u, 0u};
    LngList form_texts = {NULL, 0u, 0u};
    ktext_read(&s_file, LNG_FORMS);
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
    for (unsigned int kind = 0u; kind < 3u; kind += 1u)
    {
        lng_list_free(&listed[kind]);
    }
    for (unsigned int suffix = 0u; suffix < 3u; suffix += 1u)
    {
        lng_list_free(&found[suffix]);
    }
    lng_list_free(&used_names);
    lng_map_free(&used);
    lng_list_free(&held);
    lng_table_free(&asm_table);
    lng_table_free(&hol);
    lng_table_free(&table);
    lng_list_free(&findings);
    lng_list_free(&defects);
    ktext_free(&s_file);
    return failed ? 1 : 0;
}
