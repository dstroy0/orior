// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lng.h: the check of the three language tables against the tree and against each other (lng_check.c). The check reads
// a table whole as a tsv, its head and its rows. A list is texts in order, and a set is a list in order with none
// twice. The check reads every file as the run stores it, each line without its end (kfile_lines.h). It reads the text
// of a block of the ruleset's language, `__krs__(` to `)__krs__` (klang.h), in that language.
//
// A row of the transpiler's table with a `target` gives a word of the language of that target, as a gate of
// openqasm2_0. That word is a word only for the files of that target, as openqasm2_0.krs and openqasm2_0.ksc. A row
// with an empty `target` gives a word of the transpiler for every file.
//
// The tables are the oracle. A word of the tree that no table contains is a defect. A table that contradicts itself is
// a defect too. A complement that does not point back contradicts the table. So does a cross into the transpiler that
// disagrees on width or sign, or one transition with two mnemonics in one matrix.
//
// A finding is a cell for a reader to look at. Two words of one type for the same operators are a finding, and so is
// one word of two types. A `doing` word whose form writes none of the operators it claims is a finding. So is an
// operator the form writes that no word of its name claims. An operator with no word of the high order language is a
// finding. One mnemonic over transitions that do different things to the pair is a finding.
#ifndef LNG_H
#define LNG_H

#include "../../includes/file_defs/kfile_lines/kfile_lines.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // texts in order, each its own allocation; a set stores them in the order strcmp gives, none twice. A list starts
    // as zeros
    typedef struct
    {
        char **text;
        unsigned int count;
        unsigned int room;
    } LngList;

    // a set of keys and the value each maps to, at the same place in `value` as its key in `key`
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

    // the types a word of the transpiler's table is, and a word of the high order language's
    extern const char *const g_lng_types[];
#define LNG_KINDS 6u
    extern const char *const g_lng_hol_types[];
#define LNG_HOL_KINDS 3u

    // `text` appended after every text `list` contains
    void lng_list_add(LngList *list, const char *text);

    // `format` and what follows it printed, as printf prints them, and appended after every text `list` contains
    void lng_list_printed(LngList *list, const char *format, ...);

    // the cell of the row `row` of `table` under `column`, or "" where the head names no such column
    const char *lng_cell_or_none(const LngTable *table, unsigned int row, const char *column);

    // 1 where `set` contains `text`, with its place in `*at`; 0 where it does not, with the place it would take
    int lng_set_find(const LngList *set, const char *text, unsigned int *at);

    // `text` put in its place in `set`, where the set does not contain it
    void lng_set_add(LngList *set, const char *text);

    // 1 where `left` and `right` contain the same texts in the same order
    int lng_list_same(const LngList *left, const LngList *right);

    // every text of `list` joined, `between` between each two, into an allocation of the caller's to free
    char *lng_list_joined(const LngList *list, const char *between);

    // every text `list` contains let go, and the list zeros again
    void lng_list_free(LngList *list);

    // `value` stored for `key` where `map` has no value for it. The result is 1 where the map gets the value, and 0
    // where the map has the key already
    int lng_map_put(LngMap *map, const char *key, const char *value);

    // the value `map` has for `key`, or NULL where it has none
    const char *lng_map_get(const LngMap *map, const char *key);

    // everything `map` contains let go
    void lng_map_free(LngMap *map);

    // The head and the rows of a tsv into `table`, from `lines` of its lines at `line`, every empty line left out.
    // Each defect of its form goes into `defects`: a row whose count of cells is not the head's
    void lng_table_read(const char *const *line, unsigned int lines, LngTable *table, LngList *defects);

    // every cell `table` contains let go, and the table zeros again
    void lng_table_free(LngTable *table);

    // the cell of the row `row` under the last column of the head named `column`, or NULL where the head names none
    const char *lng_cell(const LngTable *table, unsigned int row, const char *column);

    // The operator columns the row `row` marks, into the set `operators`
    void lng_operators_of(const LngTable *table, unsigned int row, LngList *operators);

    // The words that make up an identifier, appended to `words`
    void lng_words_of(const char *name, LngList *words);

    // Defects of one table alone, appended to `defects`. A defect is a cell outside the values of its column, or two
    // rows with one word, type and target. A complement that does not point back or does not negate is a defect too.
    // `types`, `count` of them, are the types a row can have
    void lng_table_check(const LngTable *table, const char *const *types, unsigned int count, LngList *defects);

    // Findings of one table alone, appended to `findings`. Two words of one type that mark the same operators, width,
    // sign and C type are a finding. One word of two types in one target is a finding too
    void lng_table_findings(const LngTable *table, LngList *findings);

    // The form names the schema `text` lists, appended to `names`
    void lng_schema_forms(const char *text, LngList *names);

    // Every word of a ruleset at `path`, from `lines` of its lines at `line`, into `used` with the place of its first
    // use. The words are the record a line opens with, the name it gives and its parameter names. The forms a construct
    // body calls are words too
    void lng_ruleset_words(const char *path, const char *const *line, unsigned int lines, LngMap *used);

    // The lines of a .kdm or a .ksc that a ruleset reads, appended to `lines` with one text for each line of `file`.
    // The text of a line is the text of the blocks of the ruleset's language on it, and a block in a block counts. For
    // every other line the text is "", and each line keeps its number
    void lng_ruleset_lines(const KfileLines *file, LngList *lines);

    // Every word of a ksc at `path`, `file`, into `used` with the place of its first use. The words are the record a
    // line opens with and the channel and class names. The folds of a compile line and the parameter of each fold are
    // words too. A who line is not the ksc's. The lines of a block of the ruleset's language, its opener and closer
    // too, are not the ksc's
    void lng_ksc_words(const char *path, const KfileLines *file, LngMap *used);

    // The words gnascor's word map, `text`, lists, by the type it lists them under, into the sets `thing`, `doing` and
    // `gluing`
    void lng_gnascor_lists(const char *text, LngList *thing, LngList *doing, LngList *gluing);

    // The checked operators a C text writes, its literals and names stripped, into the set `operators`
    void lng_c_operators(const char *text, LngList *operators);

    // Each form of cu.krs, its name in `names` and its text in `texts` at the same place, against the operators the
    // words of its name claim. A `doing` word is a finding where the form writes none of its checked operators. A
    // checked operator that the form writes and no word of its name claims is a finding too. Each finding goes into
    // `findings`. A row with a target claims no operator for a form
    void lng_form_findings(const LngTable *table, const LngList *names, const LngList *texts, LngList *findings);

    // Defects of the cross, appended to `defects`. A high order type is a defect where the transpiler has no row for
    // its word, or where the row disagrees on width or sign. A register class of the transpiler is a defect where its
    // width is a power of two and no high order type has that width. The findings, appended to `findings`, are the
    // operators of the transpiler with no high order word. The cross reads the rows with an empty `target` only
    void lng_hol_check(const LngTable *transpiler, const LngTable *hol, LngList *defects, LngList *findings);

    // The arrow the bits of a transition give. It is *> where a side has no pair bit, the unknown or a superstate. It
    // is x> where both sides change, a cross, and -> where they do not cross, a straight line. A branch pair is no
    // transition and has no arrow
    const char *lng_arrow_of(const LngTable *table, unsigned int row);

    // Defects, appended to `defects`: one transition with two mnemonics in one table, and an arrow its bits do not
    // give. Findings, appended to `findings`: one transition with two mnemonics in two tables, and one mnemonic over
    // transitions that do different things to the pair. The mirror of a transition has lead and rite in each other's
    // places. A transition with another name for its mirror in the same table is a finding. A mnemonic that is also
    // the name of a state is a finding
    void lng_asm_check(const LngTable *table, LngList *defects, LngList *findings);

    // The ladder anchor of what a transition does to one side, `operation`. It is copy where the side keeps its bit
    // (`=`), and invert where the side reaches the other bit (`0` or `1`). NULL where the side holds no pair bit (`-`)
    const char *lng_side_anchor(const char *operation);

    // The transpiler form of the transition at `row` of the gsm table `table`: the transpiler word of each side's
    // anchor, into `left` and `right`. The result is 1, or 0 where a side holds no pair bit. It is 0 too where no
    // word of `transpiler`, or more than one, has the anchor
    int lng_asm_form(const LngTable *transpiler, const LngTable *table, unsigned int row, const char **left,
                     const char **right);

    // The transpiler forms of the gsm table `table`, and the count of its mnemonics with a form. A side anchor that
    // no one transpiler word has goes into `defects`. A mnemonic with no transition that gives each side a bit has
    // no form, and that hole goes into `findings`
    unsigned int lng_asm_forms(const LngTable *transpiler, const LngTable *table, LngList *defects, LngList *findings);

    // The ladder anchor that the .g operator `symbol` compiles to, and so its gsm: the cases of that anchor. The core
    // op of the high order table `hol` that marks the operator goes into `word`. That word crosses to the transpiler
    // word that its `transpiler` cell names, or else to the transpiler word of its own name. The result is the anchor
    // of that transpiler word. It is NULL where no core op marks the operator, and then `word` is NULL too. It is NULL
    // where the transpiler word has no anchor
    const char *lng_g_anchor(const LngTable *hol, const LngTable *transpiler, const char *symbol, const char **word);

    // The .g operators of `hol` that compile to gsm, and their count. Each operator that a row marks and that has no
    // gsm cases goes into `findings`
    unsigned int lng_g_bindings(const LngTable *hol, const LngTable *transpiler, LngList *findings);

    // Every file under src and utils, from the folder the run is in, that ends in one of `suffixes`, `count` of them.
    // Each path starts at that folder and goes into the set of `found` at the place of its suffix. The walk goes over
    // the two trees once
    void lng_files_ending(const char *const *suffixes, unsigned int count, LngList *found);

    // The target of the file at `path`: its name without its folders and its extension, into `target`. The target of
    // src/cu/transpiler/lstar/protocol/table/openqasm2_0.krs is `openqasm2_0`. `capacity` bytes include its end
    void lng_target_of(const char *path, char *target, size_t capacity);

    // Defects of the words of one file, appended to `defects`. Each word of `used` for the file at `path` is a defect
    // where no row of `table` contains it for that file. A row with an empty `target` contains its word for every file.
    // A row with a target contains its word only for a file of that target
    void lng_used_check(const LngTable *table, const char *path, const LngMap *used, LngList *defects);

    // A target table, src/lng/<target>_lng.tsv, gives the symbols of one language. Each row names the transpiler word
    // its symbol bridges to. The `anchor` column of the transpiler table gives the bridge word's ladder anchor. The
    // anchor takes a relation to gsm, and its cases in ladder.h are the relation's gsm.
    //
    // A relation's probe is a template that the witness run puts on each of those cases. A type's probe prints its
    // expected output. The witness cell names the run that gave every answer (harness.py lng witness). A row with no
    // witness is a hole, and so is a relation whose bridge word has no anchor. No lift goes through a hole.
    //
    // Defects of the target table `target`, named `name`, against `transpiler`, appended to `defects`. A type outside
    // the types of .g is a defect, and so is a type's probe with no expected output. A relation with an expected
    // output is a defect, and so is an anchored relation with no probe. A bridge word the transpiler has no word for
    // is a defect, and so are two rows with one bridge word.
    //
    // A type that disagrees with its bridge on width or sign is a defect. A relation the row marks and its bridge does
    // not is a defect. Each hole goes into `findings`
    void lng_target_check(const LngTable *transpiler, const LngTable *target, const char *name, LngList *defects,
                          LngList *findings);

    // The holes between the target tables `targets`, `count` of them, with their names in `names`. A finding goes into
    // `findings` for each bridge word one table gives and another does not. Two tables whose expected outputs for one
    // bridge word are not the same are a defect in `defects`
    void lng_targets_holes(const LngTable *const *targets, const char *const *names, unsigned int count,
                           LngList *defects, LngList *findings);

    // The lift of one bridge word, `bridge`, from the table `from`, named `from_name`, to the table `to`, named
    // `to_name`. The result is the word of `to` for it. The lift refuses with NULL where the transpiler has no word
    // `bridge`. It refuses where `bridge` is no thing and has no ladder anchor, and so no gsm cases. It refuses too
    // where a side has no witnessed row for it. Each refusal names the row and goes into `refusals`
    //
    // The code of the lift is LngLift.lean in theory/lng, compiled to C, and lng_lift.c passes it the columns of the
    // tables. A program that calls lng_lift links Lng_LngLift.a and the Lean runtime
    const char *lng_lift(const LngTable *transpiler, const LngTable *from, const char *from_name, const LngTable *to,
                         const char *to_name, const char *bridge, LngList *refusals);

    // The name of each theorem a Lean file states, `lines` of its lines at `line`, into the set `names`. A theorem
    // starts a line with `theorem `, and a name in guillemets, as «not», goes in without them
    void lng_theorem_names(const char *const *line, unsigned int lines, LngList *names);

    // The proofs of the target table `target`, named `name`, against the theorems of its Lean file at `path`. Each
    // witnessed relation row needs a theorem with the name of its bridge word, and a finding goes into `findings`
    // for each row with none. Where `theorems` is NULL, the target has no Lean file, and that is one finding
    void lng_target_proofs(const LngTable *target, const char *name, const char *path, const LngList *theorems,
                           LngList *findings);

#ifdef __cplusplus
}
#endif

#endif
