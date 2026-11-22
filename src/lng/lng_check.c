// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lng_check.c: the check of the three language tables against the tree and against each other (lng.h). lng_check.sh
// runs it from the top of the tree.
//
//     src/lng/lng_check.sh
//
// The check prints each defect, and the run exits 1 where it finds one. A finding does not fail the run. The run exits
// 2 where it cannot read a table, the schema or gnascor
#include "../cu/engine/engine_note.h"
#include "../platform/platform.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "lng.h"
#include "verba_scribo/verba_scribo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LNG_SCHEMA "src/cu/engine/rmc/machine_ir_types.h"
#define LNG_GNASCOR "src/cu/transpiler/gnascor.md"
#define LNG_FORMS "src/cu/transpiler/lstar/protocol/table/cu.krs"
#define LNG_LADDER "src/cu/transpiler/lstar/protocol/counterexample/ladder.h"

static KfileLines s_file;

// the file at `path` read into s_file, or the run ended where it contains no line
static void lng_check_read(const char *path)
{
    if (!kfile_lines_read(&s_file, path) || (s_file.lines == 0u))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "lng_check: ");
            mmgr_line_put(&engine_note.line, path);
            mmgr_line_put(&engine_note.line, " could not be read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
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

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LngList defects = {NULL, 0u, 0u};
    LngList findings = {NULL, 0u, 0u};
    LngTable table = {{NULL, 0u, 0u}, NULL, 0u, 0u};
    LngTable hol = {{NULL, 0u, 0u}, NULL, 0u, 0u};
    LngTable asm_table = {{NULL, 0u, 0u}, NULL, 0u, 0u};
    lng_check_table("src/lng/transpiler_lng.tsv", &table, &defects);
    lng_table_check(&table, g_lng_types, LNG_TYPES, &defects);
    // each anchor a row names is an anchor of the ladder, the text of an `anchor_(NAME, "text"` of ladder.h
    LngList ladder_anchors = {NULL, 0u, 0u};
    lng_check_read(LNG_LADDER);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        const char *const opened = mmgr_text_find(s_file.line[line], "anchor_(", PLATFORM_TEXT_TO_TERMINATOR);
        const char *const quote = (opened != NULL) ? mmgr_text_char(opened, '"', PLATFORM_TEXT_TO_TERMINATOR) : NULL;
        const char *const closed = (quote != NULL) ? mmgr_text_char(quote + 1, '"', PLATFORM_TEXT_TO_TERMINATOR) : NULL;
        if (closed != NULL)
        {
            LngList text = {NULL, 0u, 0u};
            lng_list_printed(&text, "%.*s", (int)(closed - quote - 1), quote + 1);
            lng_set_add(&ladder_anchors, text.text[0]);
            lng_list_free(&text);
        }
    }
    for (unsigned int row = 0u; row < table.rows; row += 1u)
    {
        const char *const anchor = lng_cell(&table, row, "anchor");
        unsigned int place = 0u;
        if ((anchor != NULL) && (anchor[0] != '\0') && !lng_set_find(&ladder_anchors, anchor, &place))
        {
            lng_list_printed(&defects, "%s has the anchor %s, which the ladder has no anchor for",
                             lng_cell(&table, row, "word"), anchor);
        }
    }
    lng_list_free(&ladder_anchors);
    lng_table_findings(&table, &findings);
    lng_check_table("src/lng/gnascor_hol_lng.tsv", &hol, &defects);
    lng_table_check(&hol, g_lng_hol_types, LNG_HOL_TYPES, &defects);
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
    static const char *const s_suffixes[4] = {".krs", ".ksc", ".kdm", "_lng.tsv"};
    LngList found[4] = {{NULL, 0u, 0u}, {NULL, 0u, 0u}, {NULL, 0u, 0u}, {NULL, 0u, 0u}};
    lng_files_ending(s_suffixes, 4u, found);
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
                found =
                    (lng_cell(&table, row, "target")[0] == '\0') &&
                    (mmgr_text_equal(lng_cell(&table, row, "word"), listed_words.text[at],
                                     PLATFORM_TEXT_TO_TERMINATOR)) &&
                    (mmgr_text_equal(lng_cell(&table, row, "type"), s_listed_type[type], PLATFORM_TEXT_TO_TERMINATOR));
            }
            if (!found)
            {
                lng_list_printed(&defects, "gnascor lists %s as a %s word and the table does not",
                                 listed_words.text[at], s_listed_type[type]);
            }
        }
    }

    // each target table: a file of the walk that ends in _lng.tsv and has a witness column
    const LngList tables = found[3];
    LngTable *const targets = (LngTable *)calloc((size_t)tables.count + 1u, sizeof(LngTable));
    const LngTable **const checked = (const LngTable **)calloc((size_t)tables.count + 1u, sizeof(LngTable *));
    LngList target_names = {NULL, 0u, 0u};
    if ((targets == NULL) || (checked == NULL))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "lng_check: no memory for the target tables");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    unsigned int target_count = 0u;
    for (unsigned int at = 0u; at < tables.count; at += 1u)
    {
        LngTable *const read = &targets[target_count];
        lng_check_table(tables.text[at], read, &defects);
        int witnessed = 0;
        for (unsigned int column = 0u; column < read->head.count; column += 1u)
        {
            witnessed = witnessed || (mmgr_text_equal(read->head.text[column], "witness", PLATFORM_TEXT_TO_TERMINATOR));
        }
        if (!witnessed)
        {
            lng_table_free(read);
            continue;
        }
        char name[256];
        lng_target_of(tables.text[at], name, sizeof(name));
        const size_t length = mmgr_text_length(name, PLATFORM_TEXT_TO_TERMINATOR);
        name[(length > 4u) ? (length - 4u) : length] = '\0';
        lng_list_add(&target_names, name);
        lng_target_check(&table, read, name, &defects, &findings);
        // each witnessed relation of the table has a theorem of its bridge word in its Lean file
        LngList proof_path = {NULL, 0u, 0u};
        LngList theorems = {NULL, 0u, 0u};
        lng_list_printed(&proof_path, "theory/lng/Lng/%s_lng.lean", name);
        static KfileLines s_proof;
        const int proven = kfile_lines_read(&s_proof, proof_path.text[0]);
        if (proven)
        {
            lng_theorem_names((const char *const *)s_proof.line, s_proof.lines, &theorems);
        }
        lng_target_proofs(read, name, proof_path.text[0], proven ? &theorems : NULL, &findings);
        lng_list_free(&theorems);
        lng_list_free(&proof_path);
        kfile_lines_free(&s_proof);
        // each bridge word a row gives is a use of that transpiler word
        for (unsigned int row = 0u; row < read->rows; row += 1u)
        {
            const char *const bridge = lng_cell(read, row, "transpiler");
            if ((bridge != NULL) && (bridge[0] != '\0'))
            {
                LngList where = {NULL, 0u, 0u};
                lng_list_printed(&where, "%s:%u", tables.text[at], row + 2u);
                lng_map_put(&used, bridge, where.text[0]);
                lng_list_free(&where);
            }
        }
        checked[target_count] = read;
        target_count += 1u;
    }
    lng_targets_holes(checked, (const char *const *)target_names.text, target_count, &defects, &findings);

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
        const int naming = (mmgr_text_equal(type, "thing", PLATFORM_TEXT_TO_TERMINATOR)) ||
                           (mmgr_text_equal(type, "doing", PLATFORM_TEXT_TO_TERMINATOR)) ||
                           (mmgr_text_equal(type, "gluing", PLATFORM_TEXT_TO_TERMINATOR));
        int named = (lng_map_get(&used, word) != NULL);
        if (!naming || named)
        {
            continue;
        }
        LngList marked = {NULL, 0u, 0u};
        lng_list_printed(&marked, "_%s_", word);
        for (unsigned int at = 0u; (at < names.count) && !named; at += 1u)
        {
            named = (mmgr_text_find(names.text[at], marked.text[0], PLATFORM_TEXT_TO_TERMINATOR) != NULL);
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
        const char *const equals = mmgr_text_char(text_line, '=', PLATFORM_TEXT_TO_TERMINATOR);
        if ((!mmgr_text_starts(text_line, "form ", PLATFORM_TEXT_TO_TERMINATOR)) || (equals == NULL))
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
    // each mnemonic's transpiler form, from the anchor of what its transitions do to each side
    const unsigned int formed = lng_asm_forms(&table, &asm_table, &defects, &findings);
    // each .g operator's gsm, through its core op, the transpiler word it crosses to and that word's anchor
    const unsigned int bound = lng_g_bindings(&hol, &table, &findings);

    for (unsigned int at = 0u; at < defects.count; at += 1u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "defect: ");
        mmgr_line_put(output_line, defects.text[at]);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (unsigned int at = 0u; at < findings.count; at += 1u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "finding: ");
        mmgr_line_put(output_line, findings.text[at]);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_decimal(output_line, defects.count, 1u);
        mmgr_line_put(output_line, " defects, ");
        mmgr_line_decimal(output_line, findings.count, 1u);
        mmgr_line_put(output_line, " findings, ");
        mmgr_line_decimal(output_line, table.rows, 1u);
        mmgr_line_put(output_line, " words, ");
        mmgr_line_decimal(output_line, hol.rows, 1u);
        mmgr_line_put(output_line, " high order words, ");
        mmgr_line_decimal(output_line, asm_table.rows, 1u);
        mmgr_line_put(output_line, " transitions, ");
        mmgr_line_decimal(output_line, formed, 1u);
        mmgr_line_put(output_line, " mnemonics with a transpiler form, ");
        mmgr_line_decimal(output_line, bound, 1u);
        mmgr_line_put(output_line, " .g operators with gsm");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const int failed = (defects.count != 0u);
    lng_list_free(&form_texts);
    lng_list_free(&form_names);
    lng_list_free(&names);
    lng_list_free(&listed_words);
    for (unsigned int type = 0u; type < 3u; type += 1u)
    {
        lng_list_free(&listed[type]);
    }
    for (unsigned int suffix = 0u; suffix < 4u; suffix += 1u)
    {
        lng_list_free(&found[suffix]);
    }
    lng_list_free(&used_names);
    lng_map_free(&used);
    for (unsigned int at = 0u; at < target_count; at += 1u)
    {
        lng_table_free(&targets[at]);
    }
    free(targets);
    free((void *)checked);
    lng_list_free(&target_names);
    lng_table_free(&asm_table);
    lng_table_free(&hol);
    lng_table_free(&table);
    lng_list_free(&findings);
    lng_list_free(&defects);
    kfile_lines_free(&s_file);
    return failed ? 1 : 0;
}
