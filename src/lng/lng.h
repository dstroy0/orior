// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lng.h: the three language tables held against the tree and against each other (lng_check.c). A table is a tsv read
// whole, its head and its rows; a list is texts in order, and a set is a list in order with none twice. Every file is
// read as the run holds it, each line without its end (kfile_lines.h), and the text of a block of the ruleset's language,
// `__krs__(` to `)__krs__` (klang.h), is read in that language.
//
// The tables are the oracle. A word the tree uses that no table holds is a defect, and so is a table that contradicts
// itself: a complement that does not point back, a cross into the transpiler that disagrees on width or sign, one
// transition given two mnemonics in one matrix. A finding is a cell worth a reader's eye: two words of one kind meaning
// the same operators, one word of two kinds, a doing word whose form writes none of the operators it claims, an
// operator the form writes that no word of its name claims, an operator the high order language has no word for, one
// mnemonic over transitions that do different things to the pair.
#ifndef LNG_H
#define LNG_H

#include "../cu/includes/file_defs/kfile_lines/kfile_lines.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // texts in order, each its own allocation; a set holds them in the order strcmp gives, none twice. A list starts
    // as zeros
    typedef struct
    {
        char **text;
        unsigned int count;
        unsigned int room;
    } LngList;

    // a set of keys and the value each holds, at the same place in `value` as its key in `key`
    typedef struct
    {
        LngList key;
        LngList value;
    } LngMap;

    // a tsv: the cells of its head, and each row's cells, as many as the head's. A table starts as zeros
    typedef struct
    {
        LngList head;
        char ***row;
        unsigned int rows;
        unsigned int room;
    } LngTable;

    // the kinds a word of the transpiler's table is, and a word of the high order language's
    extern const char *const g_lng_kinds[];
#define LNG_KINDS 6u
    extern const char *const g_lng_hol_kinds[];
#define LNG_HOL_KINDS 3u

    // `text` kept after every text `list` holds
    void lng_list_add(LngList *list, const char *text);

    // `format` and what follows it printed, as printf prints them, and kept after every text `list` holds
    void lng_list_printed(LngList *list, const char *format, ...);

    // 1 where `set` holds `text`, with its place in `*at`; 0 where it does not, with the place it would take
    int lng_set_find(const LngList *set, const char *text, unsigned int *at);

    // `text` put in its place in `set`, where the set does not hold it
    void lng_set_add(LngList *set, const char *text);

    // 1 where `left` and `right` hold the same texts in the same order
    int lng_list_same(const LngList *left, const LngList *right);

    // every text of `list` joined, `between` between each two, into an allocation of the caller's to free
    char *lng_list_joined(const LngList *list, const char *between);

    // every text `list` holds let go, and the list zeros again
    void lng_list_free(LngList *list);

    // `value` held for `key` where `map` holds no value for it: 1 where it was put, 0 where the key was held
    int lng_map_put(LngMap *map, const char *key, const char *value);

    // the value `map` holds for `key`, or NULL where it holds none
    const char *lng_map_get(const LngMap *map, const char *key);

    // everything `map` holds let go
    void lng_map_free(LngMap *map);

    // The head and the rows of a tsv, `lines` of its lines at `line`, every empty line left out, into `table`; each
    // defect of its form, a row whose count of cells is not the head's, kept after `defects`
    void lng_table_read(const char *const *line, unsigned int lines, LngTable *table, LngList *defects);

    // every cell `table` holds let go, and the table zeros again
    void lng_table_free(LngTable *table);

    // the cell of the row `row` under the last column of the head named `column`, or NULL where the head names none
    const char *lng_cell(const LngTable *table, unsigned int row, const char *column);

    // The operator columns the row `row` marks, into the set `operators`
    void lng_operators_of(const LngTable *table, unsigned int row, LngList *operators);

    // The words an identifier is compounded of, kept after `words`
    void lng_words_of(const char *name, LngList *words);

    // Defects of one table taken alone, kept after `defects`: a cell outside its column's values, a row held twice, a
    // complement that does not point back or does not negate. `kinds`, `count` of them, are the kinds a row may be
    void lng_table_check(const LngTable *table, const char *const *kinds, unsigned int count, LngList *defects);

    // Findings of one table taken alone, kept after `findings`: two words of one kind marking the same operators and
    // the same width, sign and type, and one word held as two kinds
    void lng_table_findings(const LngTable *table, LngList *findings);

    // The form names the schema `text` lists, kept after `names`
    void lng_schema_forms(const char *text, LngList *names);

    // Every word a ruleset at `path` uses, `lines` of its lines at `line`, each with where it is used, put in `used`
    // where it holds none: the record a line opens with, the name it gives, its parameter names, and the forms a
    // construct body calls
    void lng_ruleset_words(const char *path, const char *const *line, unsigned int lines, LngMap *used);

    // The lines of a .kdm or a .ksc a ruleset reads, kept after `kept`, a text for each line `file` holds: the text of
    // each of its blocks of the ruleset's language a line holds, a block inside a block of the same language counted;
    // "" for every other line so each keeps its number
    void lng_ruleset_lines(const KfileLines *file, LngList *kept);

    // Every word a ksc at `path`, `file`, uses, put in `used` where it holds none: the record a line opens with, the
    // channel and class names, and the folds a compile line records with the parameter each is put for. A who line
    // and the lines of a block of the ruleset's language, its own two among them, are not the ksc's
    void lng_ksc_words(const char *path, const KfileLines *file, LngMap *used);

    // The words gnascor's word map, `text`, lists, by the kind it lists them under, into the sets `thing`, `doing` and
    // `gluing`
    void lng_gnascor_lists(const char *text, LngList *thing, LngList *doing, LngList *gluing);

    // The checked operators a C text writes, its literals and names stripped, into the set `operators`
    void lng_c_operators(const char *text, LngList *operators);

    // Each form of cu.krs, `names` and `texts` at the same places, held against the operators the words of its name
    // claim, kept after `findings`: a doing word none of whose checked operators the form writes, and a checked
    // operator the form writes that no word of its name claims
    void lng_form_findings(const LngTable *table, const LngList *names, const LngList *texts, LngList *findings);

    // Defects of the cross, kept after `defects`: a high order type whose transpiler word is missing or disagrees on
    // width or sign, and a transpiler register class of a power-of-two width with no high order type at that width.
    // Findings, kept after `findings`: the operators the transpiler means that no high order word means
    void lng_hol_check(const LngTable *transpiler, const LngTable *hol, LngList *defects, LngList *findings);

    // The arrow a transition's bits give: *> where a side holds no pair bit, the unknown or a superstate; x> where
    // both sides change, a cross; -> where they do not cross, a straight line. A branch pair is no transition and
    // takes none
    const char *lng_arrow_of(const LngTable *table, unsigned int row);

    // Defects, kept after `defects`: one transition given two mnemonics in one table, and an arrow its bits do not
    // give. Findings, kept after `findings`: one transition given two mnemonics by two tables, one mnemonic over
    // transitions that do different things to the pair, a transition whose mirror, lead and rite trading places, is
    // named otherwise in the same table, and a mnemonic that is also a state's name
    void lng_asm_check(const LngTable *table, LngList *defects, LngList *findings);

    // Every file of the searched trees, src and utils from the folder the run stands in, ending in one of `suffixes`,
    // `count` of them, as a path from there, into the set `found` holds at the place of the suffix it ends in. The
    // trees are walked once
    void lng_files_ending(const char *const *suffixes, unsigned int count, LngList *found);

#ifdef __cplusplus
}
#endif

#endif
