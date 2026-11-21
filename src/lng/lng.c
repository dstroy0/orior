// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lng.c: the three language tables read, and each checked against the tree and against the others (lng.h)
#include "lng.h"
#include "../platform/platform.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "memoria_operor/memoria_linea.h"

#include "../../includes/file_defs/klang/klang.h"
#include "../../includes/parsers/who.h"
#include "verba_scribo/verba_scribo.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

const char *const g_lng_types[LNG_KINDS] = {"thing", "doing", "gluing", "record", "ksc", "selection"};
const char *const g_lng_hol_types[LNG_HOL_KINDS] = {"type", "plain", "core op"};

// the last of the selected columns; the operator columns are the ones after it
static const char s_selected_last[] = "selected_by_fulltrace";

// the columns after it that are no operator
#define LNG_NOT_OPERATORS 5u
static const char *const s_not_operators[LNG_NOT_OPERATORS] = {"bits", "signed", "c_type", "complement", "transpiler"};

// each comparison and the one it complements
#define LNG_COMPLEMENTS 8u
static const char *const s_complement[LNG_COMPLEMENTS][2] = {
    {"<", ">="}, {">=", "<"}, {"<=", ">"}, {">", "<="}, {"==", "!="}, {"!=", "=="}, {"==0", "!=0"}, {"!=0", "==0"}};

// C's punctuators, the longest first where one begins another
#define LNG_PUNCTUATORS 46u
static const char *const s_punctuators[LNG_PUNCTUATORS] = {
    "<<=", ">>=", "&&", "||", "<<", ">>", "<=", ">=", "==", "!=", "+=", "-=", "*=", "/=", "%=",
    "&=",  "|=",  "^=", "->", "++", "--", "+",  "-",  "*",  "/",  "%",  "&",  "|",  "^",  "<",
    ">",   "=",   "!",  "~",  "?",  ":",  "(",  ")",  "[",  "]",  "{",  "}",  ";",  ",",  "."};

// each operator a table names apart and the operator C writes for it
#define LNG_FAMILY 9u
static const char *const s_family[LNG_FAMILY][2] = {{"==0", "=="}, {"!=0", "!="}, {"<0", "<"}, {"-x", "-"}, {"~x", "~"},
                                                    {"!x", "!"},   {"?:", "?"},   {"*p", "*"}, {"*p=", "="}};

// the operators the check reads in the text of a form
#define LNG_CHECKED 21u
static const char *const s_checked[LNG_CHECKED] = {"+",  "-", "*",  "/",  "%",  "&",  "|",  "^", "<<", ">>", "<",
                                                   "<=", ">", ">=", "==", "!=", "&&", "||", "~", "!",  "?"};

// the widths a high order type is
#define LNG_HOL_BITS 4u
static const char *const s_hol_bits[LNG_HOL_BITS] = {"8", "16", "32", "64"};

// the operators the high order language writes as C writes them, with no word of its own
#define LNG_HOL_AS_C 21u
static const char *const s_hol_as_c[LNG_HOL_AS_C] = {"f()", "(T)", "&&",    "||",       "*p",   "*p=",    ".",
                                                     "//",  "T x", "const", "atomic()", "{}",   "==0",    "!=0",
                                                     "<0",  "?:",  "if",    "switch",   "goto", "return", "label:"};

// the tables whose transitions the check compares with each other
static const char s_matrix[] = "the state-transition matrix";
static const char s_high_energy_matrix[] = "the high-energy transition matrix";

// the characters Python's split takes as space
static const char s_space[] = " \t\n\r\v\f\x1c\x1d\x1e\x1f";

// the characters of a name the rulesets write
static const char s_lower[] = "abcdefghijklmnopqrstuvwxyz_";

// a set of texts for each key of a set: the set of `key` contains the keys, and `set` the set of each at its place
typedef struct
{
    LngList key;
    LngList *set;
    unsigned int room;
} LngSets;

// `pointer`, or the run ended where the system gave no memory
static void *lng_allocated(void *pointer)
{
    if (pointer == NULL)
    {
        fputs("lng: the system gave no memory\n", stderr);
        exit(2);
    }
    return pointer;
}

// `length` characters at `text` copied into an allocation of their own, ended
static char *lng_copy(const char *text, size_t length)
{
    char *const copy = (char *)lng_allocated(malloc(length + 1u));
    mmgr_bytes_copy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

// `list` given room for one more text
static void lng_list_room(LngList *list)
{
    if (list->count < list->room)
    {
        return;
    }
    const unsigned int room = (list->room == 0u) ? 16u : (list->room * 2u);
    list->text = (char **)lng_allocated(realloc(list->text, (size_t)room * sizeof(char *)));
    list->room = room;
}

// `length` characters at `text` put at `at` in `list`, every text from there one later
static void lng_list_insert(LngList *list, unsigned int at, const char *text, size_t length)
{
    lng_list_room(list);
    mmgr_bytes_move(&list->text[at + 1u], &list->text[at], (size_t)(list->count - at) * sizeof(char *));
    list->text[at] = lng_copy(text, length);
    list->count += 1u;
}

void lng_list_add(LngList *list, const char *text)
{
    lng_list_insert(list, list->count, text, mmgr_text_length(text, PLATFORM_TEXT_TO_TERMINATOR));
}

void lng_list_printed(LngList *list, const char *format, ...)
{
    va_list values;
    va_start(values, format);
    va_list measured;
    va_copy(measured, values);
    const int length = vsnprintf(NULL, 0u, format, measured);
    va_end(measured);
    if (length < 0)
    {
        va_end(values);
        lng_allocated(NULL);
        return;
    }
    // a printed length is not negative
    char *const text = (char *)lng_allocated(malloc((size_t)length + 1u));
    vsnprintf(text, (size_t)length + 1u, format, values);
    va_end(values);
    lng_list_room(list);
    list->text[list->count] = text;
    list->count += 1u;
}

int lng_set_find(const LngList *set, const char *text, unsigned int *at)
{
    unsigned int low = 0u;
    unsigned int high = set->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        const int order = strcmp(set->text[middle], text);
        if (order == 0)
        {
            *at = middle;
            return 1;
        }
        low = (order < 0) ? (middle + 1u) : low;
        high = (order < 0) ? high : middle;
    }
    *at = low;
    return 0;
}

void lng_set_add(LngList *set, const char *text)
{
    unsigned int at = 0u;
    if (!lng_set_find(set, text, &at))
    {
        lng_list_insert(set, at, text, mmgr_text_length(text, PLATFORM_TEXT_TO_TERMINATOR));
    }
}

// 1 where `set` contains `text`
static int lng_set_has(const LngList *set, const char *text)
{
    unsigned int at = 0u;
    return lng_set_find(set, text, &at);
}

int lng_list_same(const LngList *left, const LngList *right)
{
    int same = (left->count == right->count);
    for (unsigned int at = 0u; same && (at < left->count); at += 1u)
    {
        same = (mmgr_text_equal(left->text[at], right->text[at], PLATFORM_TEXT_TO_TERMINATOR));
    }
    return same;
}

// <0, 0 or >0 as `left` is before `right`, with them or after them, text by text, the shorter first where one
// begins the other
static int lng_list_order(const LngList *left, const LngList *right)
{
    for (unsigned int at = 0u; (at < left->count) && (at < right->count); at += 1u)
    {
        const int order = strcmp(left->text[at], right->text[at]);
        if (order != 0)
        {
            return order;
        }
    }
    return (left->count < right->count) ? -1 : ((left->count > right->count) ? 1 : 0);
}

char *lng_list_joined(const LngList *list, const char *between)
{
    const size_t gap = mmgr_text_length(between, PLATFORM_TEXT_TO_TERMINATOR);
    size_t length = 0u;
    for (unsigned int at = 0u; at < list->count; at += 1u)
    {
        length += mmgr_text_length(list->text[at], PLATFORM_TEXT_TO_TERMINATOR) + ((at == 0u) ? 0u : gap);
    }
    char *const joined = (char *)lng_allocated(malloc(length + 1u));
    size_t written = 0u;
    for (unsigned int at = 0u; at < list->count; at += 1u)
    {
        if (at != 0u)
        {
            mmgr_bytes_copy(&joined[written], between, gap);
            written += gap;
        }
        const size_t size = mmgr_text_length(list->text[at], PLATFORM_TEXT_TO_TERMINATOR);
        mmgr_bytes_copy(&joined[written], list->text[at], size);
        written += size;
    }
    joined[written] = '\0';
    return joined;
}

void lng_list_free(LngList *list)
{
    for (unsigned int at = 0u; at < list->count; at += 1u)
    {
        free(list->text[at]);
    }
    free(list->text);
    list->text = NULL;
    list->count = 0u;
    list->room = 0u;
}

int lng_map_put(LngMap *map, const char *key, const char *value)
{
    unsigned int at = 0u;
    if (lng_set_find(&map->key, key, &at))
    {
        return 0;
    }
    lng_list_insert(&map->key, at, key, mmgr_text_length(key, PLATFORM_TEXT_TO_TERMINATOR));
    lng_list_insert(&map->value, at, value, mmgr_text_length(value, PLATFORM_TEXT_TO_TERMINATOR));
    return 1;
}

const char *lng_map_get(const LngMap *map, const char *key)
{
    unsigned int at = 0u;
    return lng_set_find(&map->key, key, &at) ? map->value.text[at] : NULL;
}

void lng_map_free(LngMap *map)
{
    lng_list_free(&map->key);
    lng_list_free(&map->value);
}

// the set `sets` stores for `key`, a set of none put for it where it contains none
static LngList *lng_sets_at(LngSets *sets, const char *key)
{
    unsigned int at = 0u;
    if (lng_set_find(&sets->key, key, &at))
    {
        return &sets->set[at];
    }
    if (sets->key.count == sets->room)
    {
        sets->room = (sets->room == 0u) ? 16u : (sets->room * 2u);
        sets->set = (LngList *)lng_allocated(realloc(sets->set, (size_t)sets->room * sizeof(LngList)));
    }
    mmgr_bytes_move(&sets->set[at + 1u], &sets->set[at], (size_t)(sets->key.count - at) * sizeof(LngList));
    mmgr_bytes_set(&sets->set[at], 0, sizeof(LngList));
    lng_list_insert(&sets->key, at, key, mmgr_text_length(key, PLATFORM_TEXT_TO_TERMINATOR));
    return &sets->set[at];
}

// the set `sets` stores for `key`, or NULL where it contains none
static const LngList *lng_sets_get(const LngSets *sets, const char *key)
{
    unsigned int at = 0u;
    return lng_set_find(&sets->key, key, &at) ? &sets->set[at] : NULL;
}

// everything `sets` contains let go
static void lng_sets_free(LngSets *sets)
{
    for (unsigned int at = 0u; at < sets->key.count; at += 1u)
    {
        lng_list_free(&sets->set[at]);
    }
    free(sets->set);
    lng_list_free(&sets->key);
    sets->set = NULL;
    sets->room = 0u;
}

// 1 where `text` is one of the `count` texts at `texts`
static int lng_one_of(const char *text, const char *const *texts, unsigned int count)
{
    int found = 0;
    for (unsigned int at = 0u; (at < count) && !found; at += 1u)
    {
        found = (mmgr_text_equal(text, texts[at], PLATFORM_TEXT_TO_TERMINATOR));
    }
    return found;
}

// the cells of `line`, each a text between two tabs, appended to `cells`
static void lng_cells(const char *line, LngList *cells)
{
    const char *at = line;
    for (;;)
    {
        const size_t length = strcspn(at, "\t");
        lng_list_insert(cells, cells->count, at, length);
        if (at[length] == '\0')
        {
            return;
        }
        at += length + 1u;
    }
}

// the words of `length` characters at `text` apart by space, appended to `tokens`
static void lng_tokens(const char *text, size_t length, LngList *tokens)
{
    size_t at = 0u;
    while (at < length)
    {
        while ((at < length) && (mmgr_text_char(s_space, text[at], PLATFORM_TEXT_TO_TERMINATOR) != NULL) &&
               (text[at] != '\0'))
        {
            at += 1u;
        }
        const size_t start = at;
        while ((at < length) && (mmgr_text_char(s_space, text[at], PLATFORM_TEXT_TO_TERMINATOR) == NULL))
        {
            at += 1u;
        }
        if (at > start)
        {
            lng_list_insert(tokens, tokens->count, &text[start], at - start);
        }
    }
}

void lng_table_read(const char *const *line, unsigned int lines, LngTable *table, LngList *defects)
{
    unsigned int at = 0u;
    while ((at < lines) && (line[at][0] == '\0'))
    {
        at += 1u;
    }
    if (at == lines)
    {
        return;
    }
    lng_cells(line[at], &table->head);
    // the number of the line among the lines that are not empty, the head's 1
    unsigned int number = 1u;
    for (at += 1u; at < lines; at += 1u)
    {
        if (line[at][0] == '\0')
        {
            continue;
        }
        number += 1u;
        LngList cells = {NULL, 0u, 0u};
        lng_cells(line[at], &cells);
        if (cells.count != table->head.count)
        {
            lng_list_printed(defects, "line %u has %u cells where the head has %u", number, cells.count,
                             table->head.count);
            lng_list_free(&cells);
            continue;
        }
        if (table->rows == table->room)
        {
            table->room = (table->room == 0u) ? 64u : (table->room * 2u);
            table->row = (char ***)lng_allocated(realloc(table->row, (size_t)table->room * sizeof(char **)));
        }
        table->row[table->rows] = cells.text;
        table->rows += 1u;
    }
}

void lng_table_free(LngTable *table)
{
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        for (unsigned int cell = 0u; cell < table->head.count; cell += 1u)
        {
            free(table->row[row][cell]);
        }
        free(table->row[row]);
    }
    free(table->row);
    lng_list_free(&table->head);
    table->row = NULL;
    table->rows = 0u;
    table->room = 0u;
}

const char *lng_cell(const LngTable *table, unsigned int row, const char *column)
{
    unsigned int at = table->head.count;
    while (at > 0u)
    {
        at -= 1u;
        if (mmgr_text_equal(table->head.text[at], column, PLATFORM_TEXT_TO_TERMINATOR))
        {
            return table->row[row][at];
        }
    }
    return NULL;
}

// the cell of the row `row` under `column`, or "" where the head names none
const char *lng_cell_or_none(const LngTable *table, unsigned int row, const char *column)
{
    const char *const cell = lng_cell(table, row, column);
    return (cell != NULL) ? cell : "";
}

// 1 where the row `row` names a target, a ruleset's own word, and 0 where the word is the transpiler's
static int lng_row_targeted(const LngTable *table, unsigned int row)
{
    return lng_cell_or_none(table, row, "target")[0] != '\0';
}

// the first operator column of `table`, the one after the last selected column, or the head's count where it has none
static unsigned int lng_operators_start(const LngTable *table)
{
    for (unsigned int at = 0u; at < table->head.count; at += 1u)
    {
        if (mmgr_text_equal(table->head.text[at], s_selected_last, PLATFORM_TEXT_TO_TERMINATOR))
        {
            return at + 1u;
        }
    }
    return table->head.count;
}

void lng_operators_of(const LngTable *table, unsigned int row, LngList *operators)
{
    for (unsigned int at = lng_operators_start(table); at < table->head.count; at += 1u)
    {
        const char *const column = table->head.text[at];
        if (!lng_one_of(column, s_not_operators, LNG_NOT_OPERATORS) &&
            (mmgr_text_equal(lng_cell_or_none(table, row, column), "1", PLATFORM_TEXT_TO_TERMINATOR)))
        {
            lng_set_add(operators, column);
        }
    }
}

void lng_words_of(const char *name, LngList *words)
{
    const char *at = name;
    for (;;)
    {
        const size_t length = strcspn(at, "_");
        if ((length != 0u) && (strspn(at, "0123456789") < length))
        {
            lng_list_insert(words, words->count, at, length);
        }
        if (at[length] == '\0')
        {
            return;
        }
        at += length + 1u;
    }
}

void lng_table_check(const LngTable *table, const char *const *types, unsigned int count, LngList *defects)
{
    const unsigned int start = lng_operators_start(table);
    LngList seen = {NULL, 0u, 0u};
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        const char *const word = lng_cell_or_none(table, row, "word");
        const char *const type = lng_cell_or_none(table, row, "type");
        const char *const target = lng_cell_or_none(table, row, "target");
        LngList found = {NULL, 0u, 0u};
        lng_list_printed(&found, "%s\x1f%s\x1f%s", word, type, target);
        if (lng_set_has(&seen, found.text[0]))
        {
            lng_list_printed(defects, "%s %s%s%s is listed twice", word, type, (target[0] != '\0') ? " of " : "",
                             target);
        }
        lng_set_add(&seen, found.text[0]);
        lng_list_free(&found);
        if (!lng_one_of(type, types, count))
        {
            lng_list_printed(defects, "%s has the type %s, which no table names", word, type);
        }
        for (unsigned int at = start; at < table->head.count; at += 1u)
        {
            const char *const column = table->head.text[at];
            const char *const cell = lng_cell_or_none(table, row, column);
            if (!lng_one_of(column, s_not_operators, LNG_NOT_OPERATORS) && (cell[0] != '\0') &&
                (!mmgr_text_equal(cell, "1", PLATFORM_TEXT_TO_TERMINATOR)))
            {
                lng_list_printed(defects, "%s has %s under %s, where an operator cell has 1 or nothing", word, cell,
                                 column);
            }
        }
    }
    lng_list_free(&seen);

    // each word whose row has a complement, in the order the words first are, and the last row of it
    LngList order = {NULL, 0u, 0u};
    unsigned int *const last = (unsigned int *)lng_allocated(calloc((size_t)table->rows + 1u, sizeof(unsigned int)));
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        if (lng_cell_or_none(table, row, "complement")[0] == '\0')
        {
            continue;
        }
        const char *const word = lng_cell_or_none(table, row, "word");
        unsigned int at = 0u;
        while ((at < order.count) && (!mmgr_text_equal(order.text[at], word, PLATFORM_TEXT_TO_TERMINATOR)))
        {
            at += 1u;
        }
        if (at == order.count)
        {
            lng_list_add(&order, word);
        }
        last[at] = row;
    }
    for (unsigned int at = 0u; at < order.count; at += 1u)
    {
        const char *const word = order.text[at];
        const char *const complement = lng_cell_or_none(table, last[at], "complement");
        unsigned int other = 0u;
        while ((other < order.count) && (!mmgr_text_equal(order.text[other], complement, PLATFORM_TEXT_TO_TERMINATOR)))
        {
            other += 1u;
        }
        if ((other == order.count) ||
            (!mmgr_text_equal(lng_cell_or_none(table, last[other], "complement"), word, PLATFORM_TEXT_TO_TERMINATOR)))
        {
            lng_list_printed(defects, "%s complements %s, which does not complement it back", word, complement);
            continue;
        }
        LngList mine = {NULL, 0u, 0u};
        LngList theirs = {NULL, 0u, 0u};
        LngList negated = {NULL, 0u, 0u};
        lng_operators_of(table, last[at], &mine);
        lng_operators_of(table, last[other], &theirs);
        // an operator with no complement negates to nothing any set contains
        int negates = 1;
        for (unsigned int one = 0u; one < mine.count; one += 1u)
        {
            unsigned int pair = 0u;
            while ((pair < LNG_COMPLEMENTS) &&
                   (!mmgr_text_equal(s_complement[pair][0], mine.text[one], PLATFORM_TEXT_TO_TERMINATOR)))
            {
                pair += 1u;
            }
            negates = negates && (pair < LNG_COMPLEMENTS);
            if (pair < LNG_COMPLEMENTS)
            {
                lng_set_add(&negated, s_complement[pair][1]);
            }
        }
        if (!negates || !lng_list_same(&negated, &theirs))
        {
            char *const their_text = lng_list_joined(&theirs, " ");
            char *const my_text = lng_list_joined(&mine, " ");
            lng_list_printed(defects, "%s complements %s, and %s does not negate %s", word,
                             lng_cell_or_none(table, last[other], "word"), their_text, my_text);
            free(their_text);
            free(my_text);
        }
        lng_list_free(&mine);
        lng_list_free(&theirs);
        lng_list_free(&negated);
    }
    free(last);
    lng_list_free(&order);
}

// a group of rows of one type that mark the same operators, width, sign and C type. It keeps the type, the operators
// and the words in the order of the rows
typedef struct
{
    char *key;
    const char *type;
    LngList operators;
    LngList words;
} LngGroup;

void lng_table_findings(const LngTable *table, LngList *findings)
{
    LngGroup *const group = (LngGroup *)lng_allocated(calloc((size_t)table->rows + 1u, sizeof(LngGroup)));
    unsigned int groups = 0u;
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        LngList operators = {NULL, 0u, 0u};
        lng_operators_of(table, row, &operators);
        if (operators.count == 0u)
        {
            continue;
        }
        char *const marked = lng_list_joined(&operators, "\x1e");
        LngList key = {NULL, 0u, 0u};
        lng_list_printed(&key, "%s\x1f%s\x1f%s\x1f%s\x1f%s", lng_cell_or_none(table, row, "type"), marked,
                         lng_cell_or_none(table, row, "bits"), lng_cell_or_none(table, row, "signed"),
                         lng_cell_or_none(table, row, "c_type"));
        free(marked);
        unsigned int at = 0u;
        while ((at < groups) && (!mmgr_text_equal(group[at].key, key.text[0], PLATFORM_TEXT_TO_TERMINATOR)))
        {
            at += 1u;
        }
        if (at == groups)
        {
            group[at].key = lng_copy(key.text[0], mmgr_text_length(key.text[0], PLATFORM_TEXT_TO_TERMINATOR));
            group[at].type = lng_cell_or_none(table, row, "type");
            group[at].operators = operators;
            groups += 1u;
        }
        else
        {
            lng_list_free(&operators);
        }
        lng_list_free(&key);
        lng_list_add(&group[at].words, lng_cell_or_none(table, row, "word"));
    }
    // the groups in the order of their words, a group whose words are with another's placed after it
    for (unsigned int at = 1u; at < groups; at += 1u)
    {
        unsigned int place = at;
        while ((place > 0u) && (lng_list_order(&group[place - 1u].words, &group[place].words) > 0))
        {
            const LngGroup swapped = group[place - 1u];
            group[place - 1u] = group[place];
            group[place] = swapped;
            place -= 1u;
        }
    }
    for (unsigned int at = 0u; at < groups; at += 1u)
    {
        if (group[at].words.count > 1u)
        {
            char *const words = lng_list_joined(&group[at].words, ", ");
            char *const operators = lng_list_joined(&group[at].operators, " ");
            lng_list_printed(findings, "%s are %s words that all mean %s", words, group[at].type, operators);
            free(words);
            free(operators);
        }
        free(group[at].key);
        lng_list_free(&group[at].operators);
        lng_list_free(&group[at].words);
    }
    free(group);

    // a word of the transpiler and a target's word of the same letters are two words. The check reads each word in
    // its target
    LngList words = {NULL, 0u, 0u};
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        LngList keyed = {NULL, 0u, 0u};
        lng_list_printed(&keyed, "%s\x1f%s", lng_cell_or_none(table, row, "word"),
                         lng_cell_or_none(table, row, "target"));
        lng_set_add(&words, keyed.text[0]);
        lng_list_free(&keyed);
    }
    for (unsigned int at = 0u; at < words.count; at += 1u)
    {
        const char *const unit = strchr(words.text[at], '\x1f');
        const size_t word_length = (size_t)(unit - words.text[at]);
        const char *const target = unit + 1;
        LngList meanings = {NULL, 0u, 0u};
        for (unsigned int row = 0u; row < table->rows; row += 1u)
        {
            const char *const word = lng_cell_or_none(table, row, "word");
            if ((mmgr_text_length(word, PLATFORM_TEXT_TO_TERMINATOR) != word_length) ||
                !mmgr_text_starts(words.text[at], word, PLATFORM_TEXT_TO_TERMINATOR) ||
                !mmgr_text_equal(lng_cell_or_none(table, row, "target"), target, PLATFORM_TEXT_TO_TERMINATOR))
            {
                continue;
            }
            LngList operators = {NULL, 0u, 0u};
            lng_operators_of(table, row, &operators);
            char *const meant = lng_list_joined(&operators, " ");
            lng_list_printed(&meanings, "%s meaning %s", lng_cell_or_none(table, row, "type"),
                             (meant[0] != '\0') ? meant : "no operator");
            free(meant);
            lng_list_free(&operators);
        }
        if (meanings.count > 1u)
        {
            char *const meaning = lng_list_joined(&meanings, " and ");
            lng_list_printed(findings, "%.*s%s%s is %s", (int)word_length, words.text[at],
                             (target[0] != '\0') ? " of " : "", target, meaning);
            free(meaning);
        }
        lng_list_free(&meanings);
    }
    lng_list_free(&words);
}

void lng_schema_forms(const char *text, LngList *names)
{
    static const char s_form[] = "form_(";
    const char *at = mmgr_text_find(text, s_form, PLATFORM_TEXT_TO_TERMINATOR);
    while (at != NULL)
    {
        const char *const upper = at + (sizeof(s_form) - 1u);
        const size_t uppers = strspn(upper, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
        const char *next = at + 1;
        if ((uppers != 0u) && (strncmp(&upper[uppers], ", \"", 3u) == 0))
        {
            const char *const name = &upper[uppers + 3u];
            const size_t length = strspn(name, "abcdefghijklmnopqrstuvwxyz0123456789_");
            if ((length != 0u) && (name[length] == '"'))
            {
                lng_list_insert(names, names->count, name, length);
                next = &name[length + 1u];
            }
        }
        at = mmgr_text_find(next, s_form, PLATFORM_TEXT_TO_TERMINATOR);
    }
}

// 1 where a name in braces, `{name}` or `{name:<digits>}`, starts at `at` of `text`, with the name's start and length
// and the place past the braces
static int lng_braced_at(const char *text, size_t at, size_t *name, size_t *length, size_t *past)
{
    if (text[at] != '{')
    {
        return 0;
    }
    const size_t characters = strspn(&text[at + 1u], s_lower);
    if (characters == 0u)
    {
        return 0;
    }
    size_t end = at + 1u + characters;
    if (text[end] == ':')
    {
        const size_t digits = strspn(&text[end + 1u], "0123456789");
        end = ((digits != 0u) && (text[end + 1u + digits] == '}')) ? (end + 1u + digits) : end;
    }
    if (text[end] != '}')
    {
        return 0;
    }
    *name = at + 1u;
    *length = characters;
    *past = end + 1u;
    return 1;
}

// the names a token of a construct body uses, appended to `names`: the token where it is a name, or each name in
// braces in it
static void lng_braced_names(const char *token, LngList *names)
{
    const size_t whole = mmgr_text_length(token, PLATFORM_TEXT_TO_TERMINATOR);
    if ((whole != 0u) && (strspn(token, s_lower) == whole))
    {
        lng_list_add(names, token);
        return;
    }
    size_t at = 0u;
    while (at < whole)
    {
        size_t name = 0u;
        size_t length = 0u;
        size_t past = 0u;
        if (lng_braced_at(token, at, &name, &length, &past))
        {
            lng_list_insert(names, names->count, &token[name], length);
            at = past;
        }
        else
        {
            at += 1u;
        }
    }
}

// every word of `name` put in `used` at `where`, where it contains none
static void lng_use(LngMap *used, const char *name, const char *where)
{
    LngList words = {NULL, 0u, 0u};
    lng_words_of(name, &words);
    for (unsigned int at = 0u; at < words.count; at += 1u)
    {
        lng_map_put(used, words.text[at], where);
    }
    lng_list_free(&words);
}

void lng_ruleset_words(const char *path, const char *const *line, unsigned int lines, LngMap *used)
{
    int inside = 0;
    for (unsigned int number = 1u; number <= lines; number += 1u)
    {
        const char *const text = line[number - 1u];
        if ((text[0] == '\0') || (text[0] == '#'))
        {
            continue;
        }
        LngList tokens = {NULL, 0u, 0u};
        lng_tokens(text, mmgr_text_length(text, PLATFORM_TEXT_TO_TERMINATOR), &tokens);
        if (tokens.count == 0u)
        {
            continue;
        }
        static const char *const s_written[3] = {"form", "err", "nop"};
        if (lng_one_of(tokens.text[0], s_written, 3u))
        {
            // a form's text, after its first `=`, is no word of the ruleset
            lng_list_free(&tokens);
            lng_tokens(text, strcspn(text, "="), &tokens);
        }
        LngList where = {NULL, 0u, 0u};
        lng_list_printed(&where, "%s:%u", path, number);
        const char *const first = tokens.text[0];
        if (inside && (!mmgr_text_equal(first, "end", PLATFORM_TEXT_TO_TERMINATOR)))
        {
            lng_use(used, first, where.text[0]);
            for (unsigned int at = 1u; at < tokens.count; at += 1u)
            {
                LngList names = {NULL, 0u, 0u};
                lng_braced_names(tokens.text[at], &names);
                for (unsigned int name = 0u; name < names.count; name += 1u)
                {
                    lng_use(used, names.text[name], where.text[0]);
                }
                lng_list_free(&names);
            }
            lng_list_free(&where);
            lng_list_free(&tokens);
            continue;
        }
        lng_use(used, first, where.text[0]);
        static const char *const s_naming[4] = {"form", "err", "nop", "construct"};
        static const char *const s_named[3] = {"bank", "fixed", "header"};
        if (lng_one_of(first, s_naming, 4u))
        {
            for (unsigned int at = 1u; at < tokens.count; at += 1u)
            {
                lng_use(used, tokens.text[at], where.text[0]);
            }
        }
        else if (lng_one_of(first, s_named, 3u) && (tokens.count > 1u))
        {
            lng_use(used, tokens.text[1], where.text[0]);
        }
        inside = (mmgr_text_equal(first, "construct", PLATFORM_TEXT_TO_TERMINATOR)) ||
                 (inside && (!mmgr_text_equal(first, "end", PLATFORM_TEXT_TO_TERMINATOR)));
        lng_list_free(&where);
        lng_list_free(&tokens);
    }
}

// Each line of `file` cut at the blocks of the ruleset's language (klang.h). The part of a line in the text of a
// block goes into `inside`. The part outside every block goes into `outside`, and a block's opener and closer are
// part of the block. Each list gets one text for each line, and "" where the line has no such part
static void lng_block_parts(const KfileLines *file, LngList *inside, LngList *outside)
{
    // the lines joined, each ended by a newline, and where each starts
    static char s_text_bytes[QRY_WRITER_RING_BYTES / 2ULL];
    size_t length = 0u;
    size_t *const starts = (size_t *)lng_allocated(calloc((size_t)file->lines + 1u, sizeof(size_t)));
    for (unsigned int at = 0u; at < file->lines; at += 1u)
    {
        starts[at] = length;
        length = verba_textus.put(
            &(VerbaTextusCfg){.out = s_text_bytes, .cap = sizeof(s_text_bytes), .at = length, .text = file->line[at]});
        length = verba_littera.ch(
            &(VerbaLitteraCfg){.out = s_text_bytes, .cap = sizeof(s_text_bytes), .at = length, .ch = '\n'});
    }
    if (!verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_text_bytes), .at = length}))
    {
        lng_allocated(NULL);
    }
    KlangBlock *block = NULL;
    unsigned int blocks = 0u;
    size_t from = 0u;
    KlangBlock found;
    while (klang_block_find(s_text_bytes, length, from, "krs", &found))
    {
        block = (KlangBlock *)lng_allocated(realloc(block, ((size_t)blocks + 1u) * sizeof(KlangBlock)));
        block[blocks] = found;
        blocks += 1u;
        from = found.closed;
    }
    for (unsigned int at = 0u; at < file->lines; at += 1u)
    {
        const size_t start = starts[at];
        const size_t end = start + mmgr_text_length(file->line[at], PLATFORM_TEXT_TO_TERMINATOR);
        static char s_in_bytes[QRY_WRITER_RING_BYTES / 2ULL];
        size_t in = 0u;
        static char s_out_bytes[QRY_WRITER_RING_BYTES / 2ULL];
        size_t out = 0u;
        size_t cursor = start;
        for (unsigned int one = 0u; one < blocks; one += 1u)
        {
            const size_t low = (block[one].text > start) ? block[one].text : start;
            const size_t high = (block[one].text_end < end) ? block[one].text_end : end;
            if (low < high)
            {
                in = verba_textus.put_n(&(VerbaTextusCfg){.out = s_in_bytes,
                                                          .cap = sizeof(s_in_bytes),
                                                          .at = in,
                                                          .text = &s_text_bytes[low],
                                                          .text_len = high - low});
            }
            if ((block[one].closed <= cursor) || (block[one].opened >= end))
            {
                continue;
            }
            if (block[one].opened > cursor)
            {
                out = verba_textus.put_n(&(VerbaTextusCfg){.out = s_out_bytes,
                                                           .cap = sizeof(s_out_bytes),
                                                           .at = out,
                                                           .text = &s_text_bytes[cursor],
                                                           .text_len = block[one].opened - cursor});
            }
            cursor = (block[one].closed < end) ? block[one].closed : end;
        }
        if (cursor < end)
        {
            out = verba_textus.put_n(&(VerbaTextusCfg){.out = s_out_bytes,
                                                       .cap = sizeof(s_out_bytes),
                                                       .at = out,
                                                       .text = &s_text_bytes[cursor],
                                                       .text_len = end - cursor});
        }
        if (!verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_in_bytes), .at = in}) ||
            !verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_out_bytes), .at = out}))
        {
            lng_allocated(NULL);
        }
        lng_list_insert(inside, inside->count, s_in_bytes, in);
        lng_list_insert(outside, outside->count, s_out_bytes, out);
    }
    free(block);
    free(starts);
}

void lng_ruleset_lines(const KfileLines *file, LngList *lines)
{
    LngList outside = {NULL, 0u, 0u};
    lng_block_parts(file, lines, &outside);
    lng_list_free(&outside);
}

// every match of "folds the <fold> put for <put>" in `line`, the two appended to `folds` and `puts`
static void lng_folds(const char *line, LngList *folds, LngList *puts)
{
    static const char s_folds[] = "folds the ";
    static const char s_put[] = " put for ";
    const char *at = mmgr_text_find(line, s_folds, PLATFORM_TEXT_TO_TERMINATOR);
    while (at != NULL)
    {
        const char *const fold = at + (sizeof(s_folds) - 1u);
        const size_t fold_length = strcspn(fold, s_space);
        const char *next = at + 1;
        if ((fold_length != 0u) && (strncmp(&fold[fold_length], s_put, sizeof(s_put) - 1u) == 0))
        {
            const char *const put = &fold[fold_length + (sizeof(s_put) - 1u)];
            const size_t put_length = strcspn(put, s_space);
            if (put_length != 0u)
            {
                lng_list_insert(folds, folds->count, fold, fold_length);
                lng_list_insert(puts, puts->count, put, put_length);
                next = &put[put_length];
            }
        }
        at = mmgr_text_find(next, s_folds, PLATFORM_TEXT_TO_TERMINATOR);
    }
}

void lng_ksc_words(const char *path, const KfileLines *file, LngMap *used)
{
    LngList inside = {NULL, 0u, 0u};
    LngList outside = {NULL, 0u, 0u};
    lng_block_parts(file, &inside, &outside);
    for (unsigned int at = 0u; at < outside.count; at += 1u)
    {
        const char *const line = outside.text[at];
        Who who;
        int named_found = 0;
        if ((line[0] == '\0') || (line[0] == '#') || kfile_lines_who_line(line, &who, &named_found))
        {
            continue;
        }
        LngList tokens = {NULL, 0u, 0u};
        lng_tokens(line, mmgr_text_length(line, PLATFORM_TEXT_TO_TERMINATOR), &tokens);
        if (tokens.count == 0u)
        {
            continue;
        }
        LngList where = {NULL, 0u, 0u};
        lng_list_printed(&where, "%s:%u", path, at + 1u);
        lng_use(used, tokens.text[0], where.text[0]);
        static const char *const s_naming[4] = {"channel", "class", "count", "compile"};
        if (lng_one_of(tokens.text[0], s_naming, 4u) && (tokens.count > 1u))
        {
            lng_use(used, tokens.text[1], where.text[0]);
        }
        if ((mmgr_text_equal(tokens.text[0], "count", PLATFORM_TEXT_TO_TERMINATOR)) && (tokens.count > 2u))
        {
            lng_map_put(used, tokens.text[2], where.text[0]);
        }
        if ((mmgr_text_equal(tokens.text[0], "compile", PLATFORM_TEXT_TO_TERMINATOR)) && (tokens.count > 3u))
        {
            LngList folds = {NULL, 0u, 0u};
            LngList puts = {NULL, 0u, 0u};
            lng_folds(line, &folds, &puts);
            // the fourth word, its colon cut
            const size_t named = mmgr_text_length(tokens.text[3], PLATFORM_TEXT_TO_TERMINATOR);
            size_t count = named;
            while ((count != 0u) && (tokens.text[3][count - 1u] == ':'))
            {
                count -= 1u;
            }
            tokens.text[3][count] = '\0';
            for (unsigned int fold = 0u; fold < folds.count; fold += 1u)
            {
                lng_use(used, folds.text[fold], where.text[0]);
                lng_use(used, puts.text[fold], where.text[0]);
                lng_use(used, tokens.text[3], where.text[0]);
            }
            lng_list_free(&folds);
            lng_list_free(&puts);
        }
        lng_list_free(&where);
        lng_list_free(&tokens);
    }
    lng_list_free(&inside);
    lng_list_free(&outside);
}

// every name in backticks, `[a-z_]+`, of the `length` characters at `text` put in `set`
static void lng_ticked(const char *text, size_t length, LngList *set)
{
    size_t at = 0u;
    while (at < length)
    {
        size_t characters = 0u;
        while (((at + 1u + characters) < length) &&
               (mmgr_text_char(s_lower, text[at + 1u + characters], PLATFORM_TEXT_TO_TERMINATOR) != NULL) &&
               (text[at + 1u + characters] != '\0'))
        {
            characters += 1u;
        }
        if ((text[at] == '`') && (characters != 0u) && ((at + 1u + characters) < length) &&
            (text[at + 1u + characters] == '`'))
        {
            char *const name = lng_copy(&text[at + 1u], characters);
            lng_set_add(set, name);
            free(name);
            at += characters + 2u;
        }
        else
        {
            at += 1u;
        }
    }
}

// The part of the `length` characters at `text` after their `field`th `mark`, to the next mark or to their end. The
// result is 1 with its start and length, or 0 where they contain fewer marks
static int lng_field(const char *text, size_t length, char mark, unsigned int field, size_t *start, size_t *size)
{
    size_t at = 0u;
    for (unsigned int passed = 0u; passed < field; passed += 1u)
    {
        while ((at < length) && (text[at] != mark))
        {
            at += 1u;
        }
        if (at == length)
        {
            return 0;
        }
        at += 1u;
    }
    size_t end = at;
    while ((end < length) && (text[end] != mark))
    {
        end += 1u;
    }
    *start = at;
    *size = end - at;
    return 1;
}

void lng_gnascor_lists(const char *text, LngList *thing, LngList *doing, LngList *gluing)
{
    const char *const things = mmgr_text_find(text, "**Thing words.**", PLATFORM_TEXT_TO_TERMINATOR);
    const char *const doings = mmgr_text_find(text, "**Doing words.**", PLATFORM_TEXT_TO_TERMINATOR);
    const char *const gluings = mmgr_text_find(text, "**Gluing words.**", PLATFORM_TEXT_TO_TERMINATOR);
    if ((things == NULL) || (doings == NULL) || (gluings == NULL))
    {
        return;
    }
    const char *line = things;
    while (line < doings)
    {
        const char *const newline = memchr(line, '\n', (size_t)(doings - line));
        const size_t length = (newline != NULL) ? (size_t)(newline - line) : (size_t)(doings - line);
        char *const line_copy = lng_copy(line, length);
        size_t start = 0u;
        size_t size = 0u;
        if ((mmgr_text_starts(line_copy, "| ", PLATFORM_TEXT_TO_TERMINATOR)) &&
            (mmgr_text_find(line_copy, "---", PLATFORM_TEXT_TO_TERMINATOR) == NULL) &&
            (mmgr_text_find(line_copy, "| type |", PLATFORM_TEXT_TO_TERMINATOR) == NULL) &&
            lng_field(line_copy, length, '|', 2u, &start, &size))
        {
            lng_ticked(&line_copy[start], size, thing);
        }
        free(line_copy);
        line += length + 1u;
    }
    if (gluings > doings)
    {
        const char *const newline = memchr(doings, '\n', (size_t)(gluings - doings));
        lng_ticked(doings, (newline != NULL) ? (size_t)(newline - doings) : (size_t)(gluings - doings), doing);
    }
    const size_t length = strcspn(gluings, "\n");
    size_t start = 0u;
    size_t size = 0u;
    if (lng_field(gluings, length, '.', 1u, &start, &size))
    {
        lng_ticked(&gluings[start], size, gluing);
    }
}

// the operator a punctuator writes: a compound assignment its operator, every other punctuator itself
static void lng_punctuator_added(const char *punctuator, LngList *found)
{
    const size_t length = mmgr_text_length(punctuator, PLATFORM_TEXT_TO_TERMINATOR);
    static const char *const s_compared[4] = {"<=", ">=", "==", "!="};
    if ((length > 1u) && (punctuator[length - 1u] == '=') && !lng_one_of(punctuator, s_compared, 4u))
    {
        size_t length_used = length;
        while ((length_used != 0u) && (punctuator[length_used - 1u] == '='))
        {
            length_used -= 1u;
        }
        char *const operator_text = lng_copy(punctuator, length_used);
        lng_set_add(found, operator_text);
        free(operator_text);
        return;
    }
    lng_set_add(found, punctuator);
}

// The length of a literal or a name at `text`, or 0 where none starts there. It reads a name in braces, a C name, a
// hex number with its u, or a decimal number with its suffix
static size_t lng_literal_at(const char *text)
{
    size_t name = 0u;
    size_t length = 0u;
    size_t past = 0u;
    if (lng_braced_at(text, 0u, &name, &length, &past))
    {
        return past;
    }
    static const char s_name_start[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";
    if ((text[0] != '\0') && (mmgr_text_char(s_name_start, text[0], PLATFORM_TEXT_TO_TERMINATOR) != NULL))
    {
        return 1u + strspn(&text[1], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789");
    }
    if ((text[0] == '0') && (text[1] == 'x'))
    {
        const size_t digits = strspn(&text[2], "0123456789abcdefABCDEF");
        if (digits != 0u)
        {
            return 2u + digits + ((text[2u + digits] == 'u') ? 1u : 0u);
        }
    }
    const size_t digits = strspn(text, "0123456789");
    if (digits == 0u)
    {
        return 0u;
    }
    static const char *const s_suffix[5] = {"ull", "ul", "u", "ll", "l"};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        const size_t suffix = mmgr_text_length(s_suffix[at], PLATFORM_TEXT_TO_TERMINATOR);
        if (strncmp(&text[digits], s_suffix[at], suffix) == 0)
        {
            return digits + suffix;
        }
    }
    return digits;
}

void lng_c_operators(const char *text, LngList *operators)
{
    const size_t length = mmgr_text_length(text, PLATFORM_TEXT_TO_TERMINATOR);
    // the text with each written newline a newline, then each note from `//` to the next backslash cut, then each
    // literal and name a space
    char *const unescaped = (char *)lng_allocated(malloc(length + 1u));
    size_t size = 0u;
    for (size_t at = 0u; at < length; at += 1u)
    {
        const int newline = (text[at] == '\\') && (text[at + 1u] == 'n');
        unescaped[size] = newline ? '\n' : text[at];
        size += 1u;
        at += newline ? 1u : 0u;
    }
    unescaped[size] = '\0';
    char *const unnoted = (char *)lng_allocated(malloc(size + 1u));
    size_t copied = 0u;
    for (size_t at = 0u; at < size;)
    {
        if ((unescaped[at] == '/') && (unescaped[at + 1u] == '/'))
        {
            at += 2u + strcspn(&unescaped[at + 2u], "\\");
            continue;
        }
        unnoted[copied] = unescaped[at];
        copied += 1u;
        at += 1u;
    }
    unnoted[copied] = '\0';
    char *const stripped = (char *)lng_allocated(malloc(copied + 1u));
    size_t left = 0u;
    for (size_t at = 0u; at < copied;)
    {
        const size_t literal = lng_literal_at(&unnoted[at]);
        stripped[left] = (literal != 0u) ? ' ' : unnoted[at];
        left += 1u;
        at += (literal != 0u) ? literal : 1u;
    }
    stripped[left] = '\0';

    LngList found = {NULL, 0u, 0u};
    for (size_t at = 0u; at < left;)
    {
        if (mmgr_text_char(s_space, stripped[at], PLATFORM_TEXT_TO_TERMINATOR) != NULL)
        {
            at += 1u;
            continue;
        }
        unsigned int which = 0u;
        while ((which < LNG_PUNCTUATORS) &&
               (strncmp(&stripped[at], s_punctuators[which],
                        mmgr_text_length(s_punctuators[which], PLATFORM_TEXT_TO_TERMINATOR)) != 0))
        {
            which += 1u;
        }
        if (which == LNG_PUNCTUATORS)
        {
            at += 1u;
            continue;
        }
        lng_punctuator_added(s_punctuators[which], &found);
        at += mmgr_text_length(s_punctuators[which], PLATFORM_TEXT_TO_TERMINATOR);
    }
    for (unsigned int at = 0u; at < found.count; at += 1u)
    {
        if (lng_one_of(found.text[at], s_checked, LNG_CHECKED))
        {
            lng_set_add(operators, found.text[at]);
        }
    }
    lng_list_free(&found);
    free(stripped);
    free(unnoted);
    free(unescaped);
}

// the operator C writes for `operator_text`
static const char *lng_family(const char *operator_text)
{
    for (unsigned int at = 0u; at < LNG_FAMILY; at += 1u)
    {
        if (mmgr_text_equal(s_family[at][0], operator_text, PLATFORM_TEXT_TO_TERMINATOR))
        {
            return s_family[at][1];
        }
    }
    return operator_text;
}

void lng_form_findings(const LngTable *table, const LngList *names, const LngList *texts, LngList *findings)
{
    // the operators each word claims, every row of it taken together, C's operator for each
    LngSets claims = {{NULL, 0u, 0u}, NULL, 0u};
    LngList doing = {NULL, 0u, 0u};
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        // a target's own word names no form of the transpiler's
        if (lng_row_targeted(table, row))
        {
            continue;
        }
        const char *const word = lng_cell_or_none(table, row, "word");
        LngList *const claimed = lng_sets_at(&claims, word);
        LngList operators = {NULL, 0u, 0u};
        lng_operators_of(table, row, &operators);
        for (unsigned int at = 0u; at < operators.count; at += 1u)
        {
            lng_set_add(claimed, operators.text[at]);
        }
        lng_list_free(&operators);
        if (mmgr_text_equal(lng_cell_or_none(table, row, "type"), "doing", PLATFORM_TEXT_TO_TERMINATOR))
        {
            lng_set_add(&doing, word);
        }
    }
    for (unsigned int form = 0u; form < names->count; form += 1u)
    {
        const char *const name = names->text[form];
        LngList words = {NULL, 0u, 0u};
        lng_words_of(name, &words);
        LngList claimed = {NULL, 0u, 0u};
        for (unsigned int at = 0u; at < words.count; at += 1u)
        {
            const LngList *const its = lng_sets_get(&claims, words.text[at]);
            for (unsigned int one = 0u; (its != NULL) && (one < its->count); one += 1u)
            {
                lng_set_add(&claimed, lng_family(its->text[one]));
            }
        }
        LngList written = {NULL, 0u, 0u};
        lng_c_operators(texts->text[form], &written);
        for (unsigned int at = 0u; at < words.count; at += 1u)
        {
            const LngList *const its = lng_sets_get(&claims, words.text[at]);
            LngList checked = {NULL, 0u, 0u};
            for (unsigned int one = 0u; (its != NULL) && (one < its->count); one += 1u)
            {
                const char *const family = lng_family(its->text[one]);
                if (lng_one_of(family, s_checked, LNG_CHECKED))
                {
                    lng_set_add(&checked, family);
                }
            }
            int writes = 0;
            for (unsigned int one = 0u; (one < checked.count) && !writes; one += 1u)
            {
                writes = lng_set_has(&written, checked.text[one]);
            }
            if (lng_set_has(&doing, words.text[at]) && (checked.count != 0u) && !writes)
            {
                char *const claim = lng_list_joined(&checked, " ");
                lng_list_printed(findings, "%s claims %s through %s, and its form writes none of it", name, claim,
                                 words.text[at]);
                free(claim);
            }
            lng_list_free(&checked);
        }
        LngList unclaimed = {NULL, 0u, 0u};
        for (unsigned int at = 0u; at < written.count; at += 1u)
        {
            if (!lng_set_has(&claimed, written.text[at]))
            {
                lng_set_add(&unclaimed, written.text[at]);
            }
        }
        if (unclaimed.count != 0u)
        {
            char *const unclaimed_text = lng_list_joined(&unclaimed, " ");
            lng_list_printed(findings, "%s writes %s, which no word of its name claims", name, unclaimed_text);
            free(unclaimed_text);
        }
        lng_list_free(&unclaimed);
        lng_list_free(&written);
        lng_list_free(&claimed);
        lng_list_free(&words);
    }
    lng_list_free(&doing);
    lng_sets_free(&claims);
}

// The last row of `table` with the word `word` and the type `type`, or the table's count of rows where none is. A row
// with a target is not a row of the transpiler, and this skips it
static unsigned int lng_row_of(const LngTable *table, const char *word, const char *type)
{
    unsigned int found = table->rows;
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        found = (!lng_row_targeted(table, row) &&
                 mmgr_text_equal(lng_cell_or_none(table, row, "word"), word, PLATFORM_TEXT_TO_TERMINATOR) &&
                 mmgr_text_equal(lng_cell_or_none(table, row, "type"), type, PLATFORM_TEXT_TO_TERMINATOR))
                    ? row
                    : found;
    }
    return found;
}

void lng_hol_check(const LngTable *transpiler, const LngTable *hol, LngList *defects, LngList *findings)
{
    LngList things = {NULL, 0u, 0u};
    LngList words = {NULL, 0u, 0u};
    // the high order language crosses into the transpiler's own words, and no target's word
    for (unsigned int row = 0u; row < transpiler->rows; row += 1u)
    {
        if (lng_row_targeted(transpiler, row))
        {
            continue;
        }
        lng_set_add(&words, lng_cell_or_none(transpiler, row, "word"));
        if (mmgr_text_equal(lng_cell_or_none(transpiler, row, "type"), "thing", PLATFORM_TEXT_TO_TERMINATOR))
        {
            lng_set_add(&things, lng_cell_or_none(transpiler, row, "word"));
        }
    }
    for (unsigned int row = 0u; row < hol->rows; row += 1u)
    {
        const char *const crossed = lng_cell_or_none(hol, row, "transpiler");
        if (crossed[0] == '\0')
        {
            continue;
        }
        const char *const word = lng_cell_or_none(hol, row, "word");
        const char *const bits = lng_cell_or_none(hol, row, "bits");
        const char *const sign = lng_cell_or_none(hol, row, "signed");
        const unsigned int other = lng_row_of(transpiler, crossed, "thing");
        if (!lng_set_has(&words, crossed))
        {
            lng_list_printed(defects, "%s crosses to %s, which the transpiler has no word for", word, crossed);
        }
        else if ((other != transpiler->rows) &&
                 ((!mmgr_text_equal(lng_cell_or_none(transpiler, other, "bits"), bits, PLATFORM_TEXT_TO_TERMINATOR)) ||
                  (!mmgr_text_equal(lng_cell_or_none(transpiler, other, "signed"), sign, PLATFORM_TEXT_TO_TERMINATOR))))
        {
            lng_list_printed(defects, "%s is %s bits signed %s and crosses to %s, which is %s bits signed %s", word,
                             bits, sign, lng_cell_or_none(transpiler, other, "word"),
                             lng_cell_or_none(transpiler, other, "bits"),
                             lng_cell_or_none(transpiler, other, "signed"));
        }
    }
    LngList found = {NULL, 0u, 0u};
    for (unsigned int row = 0u; row < hol->rows; row += 1u)
    {
        if (mmgr_text_equal(lng_cell_or_none(hol, row, "type"), "type", PLATFORM_TEXT_TO_TERMINATOR))
        {
            LngList width = {NULL, 0u, 0u};
            lng_list_printed(&width, "%s\x1f%s", lng_cell_or_none(hol, row, "bits"),
                             lng_cell_or_none(hol, row, "signed"));
            lng_set_add(&found, width.text[0]);
            lng_list_free(&width);
        }
    }
    for (unsigned int at = 0u; at < things.count; at += 1u)
    {
        const unsigned int row = lng_row_of(transpiler, things.text[at], "thing");
        const char *const bits = lng_cell_or_none(transpiler, row, "bits");
        const char *const sign = lng_cell_or_none(transpiler, row, "signed");
        LngList width = {NULL, 0u, 0u};
        lng_list_printed(&width, "%s\x1f%s", bits, sign);
        if (lng_one_of(bits, s_hol_bits, LNG_HOL_BITS) && !lng_set_has(&found, width.text[0]))
        {
            lng_list_printed(findings, "the transpiler's %s is %s bits signed %s, and no high order type is",
                             things.text[at], bits, sign);
        }
        lng_list_free(&width);
    }

    LngList hol_means = {NULL, 0u, 0u};
    LngSets ops = {{NULL, 0u, 0u}, NULL, 0u};
    for (unsigned int row = 0u; row < hol->rows; row += 1u)
    {
        LngList operators = {NULL, 0u, 0u};
        lng_operators_of(hol, row, &operators);
        const int core = (mmgr_text_equal(lng_cell_or_none(hol, row, "type"), "core op", PLATFORM_TEXT_TO_TERMINATOR));
        for (unsigned int at = 0u; at < operators.count; at += 1u)
        {
            lng_set_add(&hol_means, operators.text[at]);
            if (core)
            {
                lng_set_add(lng_sets_at(&ops, operators.text[at]), lng_cell_or_none(hol, row, "word"));
            }
        }
        lng_list_free(&operators);
    }
    LngSets transpiler_means = {{NULL, 0u, 0u}, NULL, 0u};
    for (unsigned int row = 0u; row < transpiler->rows; row += 1u)
    {
        LngList operators = {NULL, 0u, 0u};
        lng_operators_of(transpiler, row, &operators);
        for (unsigned int at = 0u; at < operators.count; at += 1u)
        {
            lng_set_add(lng_sets_at(&transpiler_means, operators.text[at]), lng_cell_or_none(transpiler, row, "word"));
        }
        lng_list_free(&operators);
    }
    for (unsigned int at = 0u; at < transpiler_means.key.count; at += 1u)
    {
        const char *const operator_text = transpiler_means.key.text[at];
        if (!lng_set_has(&hol_means, operator_text) && !lng_one_of(operator_text, s_hol_as_c, LNG_HOL_AS_C))
        {
            char *const meant = lng_list_joined(&transpiler_means.set[at], ", ");
            lng_list_printed(findings, "the high order language has no word for %s, which the transpiler means by %s",
                             operator_text, meant);
            free(meant);
        }
    }
    for (unsigned int row = 0u; row < transpiler->rows; row += 1u)
    {
        if (!mmgr_text_equal(lng_cell_or_none(transpiler, row, "type"), "doing", PLATFORM_TEXT_TO_TERMINATOR))
        {
            continue;
        }
        const char *const word = lng_cell_or_none(transpiler, row, "word");
        LngList operators = {NULL, 0u, 0u};
        lng_operators_of(transpiler, row, &operators);
        for (unsigned int at = 0u; at < operators.count; at += 1u)
        {
            const LngList *const core = lng_sets_get(&ops, operators.text[at]);
            if ((core != NULL) && !lng_set_has(core, word))
            {
                char *const named = lng_list_joined(core, ", ");
                lng_list_printed(findings, "the transpiler names %s %s where the core op is %s", operators.text[at],
                                 word, named);
                free(named);
            }
        }
        lng_list_free(&operators);
    }
    lng_sets_free(&transpiler_means);
    lng_sets_free(&ops);
    lng_list_free(&hol_means);
    lng_list_free(&found);
    lng_list_free(&words);
    lng_list_free(&things);
}

const char *lng_arrow_of(const LngTable *table, unsigned int row)
{
    const char *const left = lng_cell_or_none(table, row, "left");
    const char *const right = lng_cell_or_none(table, row, "right");
    if (mmgr_text_equal(lng_cell_or_none(table, row, "table"), "the branch pair", PLATFORM_TEXT_TO_TERMINATOR))
    {
        return "";
    }
    if ((mmgr_text_equal(left, "-", PLATFORM_TEXT_TO_TERMINATOR)) ||
        (mmgr_text_equal(right, "-", PLATFORM_TEXT_TO_TERMINATOR)))
    {
        return "*>";
    }
    if ((!mmgr_text_equal(left, "=", PLATFORM_TEXT_TO_TERMINATOR)) &&
        (!mmgr_text_equal(right, "=", PLATFORM_TEXT_TO_TERMINATOR)))
    {
        return "x>";
    }
    return "->";
}

// the mirror of `side`, with lead and rite in each other's places
static const char *lng_mirror(const char *side)
{
    return (mmgr_text_equal(side, "lead", PLATFORM_TEXT_TO_TERMINATOR))
               ? "rite"
               : ((mmgr_text_equal(side, "rite", PLATFORM_TEXT_TO_TERMINATOR)) ? "lead" : side);
}

// the `field`th field of `key`, its fields apart by \x1f, copied into an allocation of the caller's to free
static char *lng_key_field(const char *key, unsigned int field)
{
    size_t start = 0u;
    size_t size = 0u;
    if (!lng_field(key, mmgr_text_length(key, PLATFORM_TEXT_TO_TERMINATOR), '\x1f', field, &start, &size))
    {
        return lng_copy("", 0u);
    }
    return lng_copy(&key[start], size);
}

void lng_asm_check(const LngTable *table, LngList *defects, LngList *findings)
{
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        const char *const arrow = lng_arrow_of(table, row);
        const char *const written = lng_cell(table, row, "arrow");
        const char *const given = (written != NULL) ? written : arrow;
        if (!mmgr_text_equal(given, arrow, PLATFORM_TEXT_TO_TERMINATOR))
        {
            lng_list_printed(defects, "%s to %s in %s is written %s and its bits give %s",
                             lng_cell_or_none(table, row, "past"), lng_cell_or_none(table, row, "present"),
                             lng_cell_or_none(table, row, "table"), (given[0] != '\0') ? given : "no arrow",
                             (arrow[0] != '\0') ? arrow : "none");
        }
    }
    LngList states = {NULL, 0u, 0u};
    LngList mnemonics = {NULL, 0u, 0u};
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        lng_set_add(&states, lng_cell_or_none(table, row, "past"));
        lng_set_add(&states, lng_cell_or_none(table, row, "present"));
        lng_set_add(&mnemonics, lng_cell_or_none(table, row, "mnemonic"));
    }
    for (unsigned int at = 0u; at < mnemonics.count; at += 1u)
    {
        if (!lng_set_has(&states, mnemonics.text[at]))
        {
            continue;
        }
        LngList transitions = {NULL, 0u, 0u};
        for (unsigned int row = 0u; row < table->rows; row += 1u)
        {
            if (mmgr_text_equal(lng_cell_or_none(table, row, "mnemonic"), mnemonics.text[at],
                                PLATFORM_TEXT_TO_TERMINATOR))
            {
                lng_list_printed(&transitions, "%s to %s in %s", lng_cell_or_none(table, row, "past"),
                                 lng_cell_or_none(table, row, "present"), lng_cell_or_none(table, row, "table"));
            }
        }
        char *const joined = lng_list_joined(&transitions, ", ");
        lng_list_printed(findings, "%s is a state and also the mnemonic of %s", mnemonics.text[at], joined);
        free(joined);
        lng_list_free(&transitions);
    }

    // A key for each transition: its table, its past and its present, with \x1f between them. The \x1f sorts before
    // every character of a cell, and the keys sort in the order of the three. Each key maps to the mnemonics each
    // table gives the transition
    LngSets named = {{NULL, 0u, 0u}, NULL, 0u};
    LngSets doing = {{NULL, 0u, 0u}, NULL, 0u};
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        LngList key = {NULL, 0u, 0u};
        lng_list_printed(&key, "%s\x1f%s\x1f%s", lng_cell_or_none(table, row, "table"),
                         lng_cell_or_none(table, row, "past"), lng_cell_or_none(table, row, "present"));
        lng_set_add(lng_sets_at(&named, key.text[0]), lng_cell_or_none(table, row, "mnemonic"));
        lng_list_free(&key);
        const char *const left = lng_cell_or_none(table, row, "left");
        const char *const right = lng_cell_or_none(table, row, "right");
        if ((!mmgr_text_equal(left, "-", PLATFORM_TEXT_TO_TERMINATOR)) &&
            (!mmgr_text_equal(right, "-", PLATFORM_TEXT_TO_TERMINATOR)))
        {
            LngList pair = {NULL, 0u, 0u};
            lng_list_printed(&pair, "%s\x1f%s", left, right);
            lng_set_add(lng_sets_at(&doing, lng_cell_or_none(table, row, "mnemonic")), pair.text[0]);
            lng_list_free(&pair);
        }
    }
    LngSets across = {{NULL, 0u, 0u}, NULL, 0u};
    for (unsigned int at = 0u; at < named.key.count; at += 1u)
    {
        char *const table_name = lng_key_field(named.key.text[at], 0u);
        char *const past = lng_key_field(named.key.text[at], 1u);
        char *const present = lng_key_field(named.key.text[at], 2u);
        if (named.set[at].count > 1u)
        {
            char *const both = lng_list_joined(&named.set[at], " and ");
            lng_list_printed(defects, "%s names %s to %s both %s", table_name, past, present, both);
            free(both);
        }
        if ((mmgr_text_equal(table_name, s_matrix, PLATFORM_TEXT_TO_TERMINATOR)) ||
            (mmgr_text_equal(table_name, s_high_energy_matrix, PLATFORM_TEXT_TO_TERMINATOR)))
        {
            LngList key = {NULL, 0u, 0u};
            lng_list_printed(&key, "%s\x1f%s", past, present);
            lng_set_add(lng_sets_at(&across, key.text[0]), table_name);
            lng_list_free(&key);
        }
        free(table_name);
        free(past);
        free(present);
    }
    for (unsigned int at = 0u; at < across.key.count; at += 1u)
    {
        char *const past = lng_key_field(across.key.text[at], 0u);
        char *const present = lng_key_field(across.key.text[at], 1u);
        LngList names = {NULL, 0u, 0u};
        LngList each = {NULL, 0u, 0u};
        for (unsigned int one = 0u; one < across.set[at].count; one += 1u)
        {
            LngList key = {NULL, 0u, 0u};
            lng_list_printed(&key, "%s\x1f%s\x1f%s", across.set[at].text[one], past, present);
            const LngList *const its = lng_sets_get(&named, key.text[0]);
            for (unsigned int name = 0u; name < its->count; name += 1u)
            {
                lng_set_add(&names, its->text[name]);
            }
            char *const joined = lng_list_joined(its, " ");
            lng_list_add(&each, joined);
            free(joined);
            lng_list_free(&key);
        }
        if ((across.set[at].count > 1u) && (names.count > 1u) && !lng_set_has(&names, "sync"))
        {
            char *const joined = lng_list_joined(&each, " in one matrix and ");
            lng_list_printed(findings, "%s to %s is %s", past, present, joined);
            free(joined);
        }
        lng_list_free(&each);
        lng_list_free(&names);
        free(past);
        free(present);
    }
    for (unsigned int at = 0u; at < doing.key.count; at += 1u)
    {
        if (doing.set[at].count <= 1u)
        {
            continue;
        }
        LngList pairs = {NULL, 0u, 0u};
        for (unsigned int one = 0u; one < doing.set[at].count; one += 1u)
        {
            char *const left = lng_key_field(doing.set[at].text[one], 0u);
            char *const right = lng_key_field(doing.set[at].text[one], 1u);
            lng_list_printed(&pairs, "left %s right %s", left, right);
            free(left);
            free(right);
        }
        char *const joined = lng_list_joined(&pairs, ", ");
        lng_list_printed(findings, "%s does %s to the pair", doing.key.text[at], joined);
        free(joined);
        lng_list_free(&pairs);
    }
    for (unsigned int at = 0u; at < named.key.count; at += 1u)
    {
        char *const table_name = lng_key_field(named.key.text[at], 0u);
        char *const past = lng_key_field(named.key.text[at], 1u);
        char *const present = lng_key_field(named.key.text[at], 2u);
        const char *const mirror_past = lng_mirror(past);
        const char *const mirror_present = lng_mirror(present);
        LngList key = {NULL, 0u, 0u};
        lng_list_printed(&key, "%s\x1f%s\x1f%s", table_name, mirror_past, mirror_present);
        const LngList *const mirror = lng_sets_get(&named, key.text[0]);
        const int order = strcmp(past, mirror_past);
        const int sooner = (order < 0) || ((order == 0) && (strcmp(present, mirror_present) < 0));
        if ((!mmgr_text_equal(key.text[0], named.key.text[at], PLATFORM_TEXT_TO_TERMINATOR)) && (mirror != NULL) &&
            !lng_list_same(mirror, &named.set[at]) && sooner)
        {
            char *const mine = lng_list_joined(&named.set[at], " ");
            char *const theirs = lng_list_joined(mirror, " ");
            lng_list_printed(findings, "%s names %s to %s %s and its mirror %s to %s %s", table_name, past, present,
                             mine, mirror_past, mirror_present, theirs);
            free(mine);
            free(theirs);
        }
        lng_list_free(&key);
        free(table_name);
        free(past);
        free(present);
    }
    lng_sets_free(&across);
    lng_sets_free(&doing);
    lng_sets_free(&named);
    lng_list_free(&mnemonics);
    lng_list_free(&states);
}

// the suffixes a walk seeks, `count` of them, and the set of each, at the suffix's place
typedef struct
{
    const char *const *suffixes;
    unsigned int count;
    LngList *found;
} LngWalk;

// The entry `name` of the folder `folder`, a folder where `folder_exists`. For a folder, each file under it that the
// walk seeks goes into its set. For a file that ends in a suffix the walk seeks, the file goes into its set. Each
// path starts at the folder the run is in
static void lng_files_under(const char *folder, const LngWalk *walk);
static void lng_entry_taken(const char *folder, const char *name, int folder_exists, const LngWalk *walk)
{
    if ((mmgr_text_equal(name, ".", PLATFORM_TEXT_TO_TERMINATOR)) ||
        (mmgr_text_equal(name, "..", PLATFORM_TEXT_TO_TERMINATOR)))
    {
        return;
    }
    LngList path = {NULL, 0u, 0u};
    lng_list_printed(&path, "%s/%s", folder, name);
    const size_t length = mmgr_text_length(name, PLATFORM_TEXT_TO_TERMINATOR);
    if (folder_exists)
    {
        lng_files_under(path.text[0], walk);
    }
    for (unsigned int at = 0u; !folder_exists && (at < walk->count); at += 1u)
    {
        const size_t ending = mmgr_text_length(walk->suffixes[at], PLATFORM_TEXT_TO_TERMINATOR);
        if ((length >= ending) &&
            (mmgr_text_equal(&name[length - ending], walk->suffixes[at], PLATFORM_TEXT_TO_TERMINATOR)))
        {
            lng_set_add(&walk->found[at], path.text[0]);
        }
    }
    lng_list_free(&path);
}

#if defined(_WIN32)
static void lng_files_under(const char *folder, const LngWalk *walk)
{
    LngList pattern = {NULL, 0u, 0u};
    lng_list_printed(&pattern, "%s/*", folder);
    // the entries requested in large reads, without the short names nothing here reads
    WIN32_FIND_DATAA entry;
    const HANDLE search = FindFirstFileExA(pattern.text[0], FindExInfoBasic, &entry, FindExSearchNameMatch, NULL,
                                           FIND_FIRST_EX_LARGE_FETCH);
    lng_list_free(&pattern);
    if (search == INVALID_HANDLE_VALUE)
    {
        return;
    }
    int more = 0;
    do
    {
        lng_entry_taken(folder, entry.cFileName, (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u, walk);
        more = (FindNextFileA(search, &entry) != 0);
    } while (more);
    FindClose(search);
}
#else
static void lng_files_under(const char *folder, const LngWalk *walk)
{
    DIR *const directory = opendir(folder);
    if (directory == NULL)
    {
        return;
    }
    for (const struct dirent *entry = readdir(directory); entry != NULL; entry = readdir(directory))
    {
        LngList path = {NULL, 0u, 0u};
        lng_list_printed(&path, "%s/%s", folder, entry->d_name);
        struct stat status;
        const int folder_exists = (stat(path.text[0], &status) == 0) && S_ISDIR(status.st_mode);
        lng_list_free(&path);
        lng_entry_taken(folder, entry->d_name, folder_exists, walk);
    }
    closedir(directory);
}
#endif

void lng_files_ending(const char *const *suffixes, unsigned int count, LngList *found)
{
    static const char *const s_searched[2] = {"src", "utils"};
    const LngWalk walk = {suffixes, count, found};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        lng_files_under(s_searched[at], &walk);
    }
}

void lng_target_of(const char *path, char *target, size_t capacity)
{
    const char *name = path;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        name = ((*at == '/') || (*at == '\\')) ? (at + 1) : name;
    }
    const char *const dot = strchr(name, '.');
    const size_t length = (dot != NULL) ? (size_t)(dot - name) : strlen(name);
    const size_t kept = (length < capacity) ? length : (capacity - 1u);
    memcpy(target, name, kept);
    target[kept] = '\0';
}

void lng_used_check(const LngTable *table, const char *path, const LngMap *used, LngList *defects)
{
    char target[256];
    lng_target_of(path, target, sizeof(target));
    for (unsigned int at = 0u; at < used->key.count; at += 1u)
    {
        int contained = 0;
        for (unsigned int row = 0u; (row < table->rows) && !contained; row += 1u)
        {
            const char *const row_target = lng_cell_or_none(table, row, "target");
            contained = (strcmp(lng_cell_or_none(table, row, "word"), used->key.text[at]) == 0) &&
                        ((row_target[0] == '\0') || (strcmp(row_target, target) == 0));
        }
        if (!contained)
        {
            lng_list_printed(defects, "%s is used at %s and no table contains it", used->key.text[at],
                             used->value.text[at]);
        }
    }
}

// the row of the transpiler's own words in `transpiler` whose word is `word`, or the table's count of rows where none
// is
static unsigned int lng_bridge_word_row(const LngTable *transpiler, const char *word)
{
    for (unsigned int row = 0u; row < transpiler->rows; row += 1u)
    {
        if (!lng_row_targeted(transpiler, row) && (strcmp(lng_cell_or_none(transpiler, row, "word"), word) == 0))
        {
            return row;
        }
    }
    return transpiler->rows;
}

// the first row of the target table `target` that bridges to the transpiler word `bridge`, or the table's count of
// rows where none does
static unsigned int lng_bridged_row(const LngTable *target, const char *bridge)
{
    for (unsigned int row = 0u; row < target->rows; row += 1u)
    {
        if (strcmp(lng_cell_or_none(target, row, "transpiler"), bridge) == 0)
        {
            return row;
        }
    }
    return target->rows;
}

void lng_target_check(const LngTable *transpiler, const LngTable *target, const char *name, LngList *defects,
                      LngList *findings)
{
    LngList bridged = {NULL, 0u, 0u};
    for (unsigned int row = 0u; row < target->rows; row += 1u)
    {
        const char *const word = lng_cell_or_none(target, row, "word");
        const char *const type = lng_cell_or_none(target, row, "type");
        const char *const bridge = lng_cell_or_none(target, row, "transpiler");
        const int probed = (lng_cell_or_none(target, row, "probe")[0] != '\0');
        const int expected = (lng_cell_or_none(target, row, "expected")[0] != '\0');
        const int typed = (strcmp(type, "type") == 0);
        if (!lng_one_of(type, g_lng_hol_types, LNG_HOL_KINDS))
        {
            lng_list_printed(defects, "%s's %s has the type %s, which no table names", name, word, type);
        }
        // a type's probe prints its expected output; a relation's probe runs on the ladder's cases, which give the
        // expected outputs
        if (typed && (probed != expected))
        {
            lng_list_printed(defects, "%s's %s has a %s and no %s", name, word, probed ? "probe" : "expected output",
                             probed ? "expected output" : "probe");
        }
        if (!typed && expected)
        {
            lng_list_printed(defects,
                             "%s's %s has an expected output, and the ladder's cases give a relation's outputs", name,
                             word);
        }
        if (bridge[0] == '\0')
        {
            lng_list_printed(defects, "%s's %s bridges to no transpiler word, and no lift takes it to gsm", name, word);
            continue;
        }
        const unsigned int crossed = lng_bridge_word_row(transpiler, bridge);
        if (crossed == transpiler->rows)
        {
            lng_list_printed(defects, "%s's %s bridges to %s, which the transpiler has no word for", name, word,
                             bridge);
            continue;
        }
        if (lng_set_has(&bridged, bridge))
        {
            lng_list_printed(defects, "%s bridges to %s twice", name, bridge);
        }
        lng_set_add(&bridged, bridge);
        // a type and its bridge agree on width and sign
        if ((strcmp(type, "type") == 0) &&
            ((strcmp(lng_cell_or_none(target, row, "bits"), lng_cell_or_none(transpiler, crossed, "bits")) != 0) ||
             (strcmp(lng_cell_or_none(target, row, "signed"), lng_cell_or_none(transpiler, crossed, "signed")) != 0)))
        {
            lng_list_printed(defects, "%s's %s is %s bits signed %s and bridges to %s, which is %s bits signed %s",
                             name, word, lng_cell_or_none(target, row, "bits"), lng_cell_or_none(target, row, "signed"),
                             bridge, lng_cell_or_none(transpiler, crossed, "bits"),
                             lng_cell_or_none(transpiler, crossed, "signed"));
        }
        // each relation the row marks, its bridge marks too
        LngList marked = {NULL, 0u, 0u};
        LngList bridge_marked = {NULL, 0u, 0u};
        lng_operators_of(target, row, &marked);
        lng_operators_of(transpiler, crossed, &bridge_marked);
        for (unsigned int at = 0u; at < marked.count; at += 1u)
        {
            if (!lng_set_has(&bridge_marked, marked.text[at]))
            {
                lng_list_printed(defects, "%s's %s marks %s, and its bridge %s does not", name, word, marked.text[at],
                                 bridge);
            }
        }
        lng_list_free(&bridge_marked);
        lng_list_free(&marked);
        // a relation goes to gsm through the ladder anchor of its bridge word
        const char *const anchor = lng_cell_or_none(transpiler, crossed, "anchor");
        if (!typed && (anchor[0] == '\0'))
        {
            lng_list_printed(findings, "%s's %s bridges to %s, which has no gsm cases, and no lift goes through it",
                             name, word, bridge);
            continue;
        }
        if (!typed && !probed)
        {
            lng_list_printed(defects, "%s's %s bridges to %s, which gsm reads as %s, and has no probe", name, word,
                             bridge, anchor);
        }
        if (lng_cell_or_none(target, row, "witness")[0] == '\0')
        {
            lng_list_printed(findings, "%s's %s for %s has no witness, and no lift goes through it", name, word,
                             bridge);
        }
    }
    lng_list_free(&bridged);
}

void lng_targets_holes(const LngTable *const *targets, const char *const *names, unsigned int count, LngList *defects,
                       LngList *findings)
{
    // every bridge word a target table gives, with the first table that gives it
    LngMap given = {{NULL, 0u, 0u}, {NULL, 0u, 0u}};
    for (unsigned int table = 0u; table < count; table += 1u)
    {
        for (unsigned int row = 0u; row < targets[table]->rows; row += 1u)
        {
            const char *const bridge = lng_cell_or_none(targets[table], row, "transpiler");
            if (bridge[0] != '\0')
            {
                lng_map_put(&given, bridge, names[table]);
            }
        }
    }
    for (unsigned int at = 0u; at < given.key.count; at += 1u)
    {
        const char *const bridge = given.key.text[at];
        const char *first_expected = NULL;
        const char *first_name = NULL;
        for (unsigned int table = 0u; table < count; table += 1u)
        {
            const unsigned int row = lng_bridged_row(targets[table], bridge);
            if (row == targets[table]->rows)
            {
                lng_list_printed(findings, "%s has no row for %s, which %s bridges to", names[table], bridge,
                                 given.value.text[at]);
                continue;
            }
            // one bridge word, one output for every target's probe
            const char *const expected = lng_cell_or_none(targets[table], row, "expected");
            if ((first_expected != NULL) && (strcmp(first_expected, expected) != 0))
            {
                lng_list_printed(defects, "%s expects %s for %s, and %s expects %s", first_name, first_expected, bridge,
                                 names[table], expected);
            }
            first_expected = (first_expected == NULL) ? expected : first_expected;
            first_name = (first_name == NULL) ? names[table] : first_name;
        }
    }
    lng_map_free(&given);
}

void lng_theorem_names(const char *const *line, unsigned int lines, LngList *names)
{
    static const char s_theorem[] = "theorem ";
    for (unsigned int at = 0u; at < lines; at += 1u)
    {
        if (strncmp(line[at], s_theorem, sizeof(s_theorem) - 1u) != 0)
        {
            continue;
        }
        // a name that Lean keeps for itself goes in guillemets, as «not»
        const char *start = line[at] + sizeof(s_theorem) - 1u;
        const int guarded = (strncmp(start, "\xc2\xab", 2u) == 0);
        start += guarded ? 2u : 0u;
        size_t length = 0u;
        while ((start[length] != '\0') && (start[length] != ' ') && (start[length] != ':') &&
               (strncmp(&start[length], "\xc2\xbb", 2u) != 0))
        {
            length += 1u;
        }
        LngList name = {NULL, 0u, 0u};
        lng_list_printed(&name, "%.*s", (int)length, start);
        lng_set_add(names, name.text[0]);
        lng_list_free(&name);
    }
}

void lng_target_proofs(const LngTable *target, const char *name, const char *path, const LngList *theorems,
                       LngList *findings)
{
    if (theorems == NULL)
    {
        lng_list_printed(findings, "%s has no proof file, %s", name, path);
        return;
    }
    for (unsigned int row = 0u; row < target->rows; row += 1u)
    {
        const char *const bridge = lng_cell_or_none(target, row, "transpiler");
        // a type and a hole have no proof to give: a type is a width, and a hole is no symbol of the relation
        if ((strcmp(lng_cell_or_none(target, row, "type"), "type") == 0) || (bridge[0] == '\0') ||
            (lng_cell_or_none(target, row, "witness")[0] == '\0'))
        {
            continue;
        }
        if (!lng_set_has(theorems, bridge))
        {
            lng_list_printed(findings, "%s's %s for %s has no proof in %s", name, lng_cell_or_none(target, row, "word"),
                             bridge, path);
        }
    }
}

const char *lng_side_anchor(const char *operation)
{
    if ((operation[0] == '\0') || (strcmp(operation, "-") == 0))
    {
        return NULL;
    }
    // a side that keeps its bit copies it, and a side that reaches the other bit inverts it
    return (strcmp(operation, "=") == 0) ? "copy" : "invert";
}

// the transpiler's own word whose anchor is `anchor`, or NULL where no word or two words have it
static const char *lng_anchor_word(const LngTable *transpiler, const char *anchor)
{
    const char *found = NULL;
    for (unsigned int row = 0u; row < transpiler->rows; row += 1u)
    {
        if (lng_row_targeted(transpiler, row) || (strcmp(lng_cell_or_none(transpiler, row, "anchor"), anchor) != 0))
        {
            continue;
        }
        if (found != NULL)
        {
            return NULL;
        }
        found = lng_cell_or_none(transpiler, row, "word");
    }
    return found;
}

int lng_asm_form(const LngTable *transpiler, const LngTable *table, unsigned int row, const char **left,
                 const char **right)
{
    const char *const left_anchor = lng_side_anchor(lng_cell_or_none(table, row, "left"));
    const char *const right_anchor = lng_side_anchor(lng_cell_or_none(table, row, "right"));
    if ((left_anchor == NULL) || (right_anchor == NULL))
    {
        return 0;
    }
    *left = lng_anchor_word(transpiler, left_anchor);
    *right = lng_anchor_word(transpiler, right_anchor);
    return (*left != NULL) && (*right != NULL);
}

unsigned int lng_asm_forms(const LngTable *transpiler, const LngTable *table, LngList *defects, LngList *findings)
{
    LngList mnemonics = {NULL, 0u, 0u};
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        lng_set_add(&mnemonics, lng_cell_or_none(table, row, "mnemonic"));
        const char *const left = lng_side_anchor(lng_cell_or_none(table, row, "left"));
        const char *const right = lng_side_anchor(lng_cell_or_none(table, row, "right"));
        const char *const sides[2] = {left, right};
        for (unsigned int side = 0u; side < 2u; side += 1u)
        {
            if ((sides[side] != NULL) && (lng_anchor_word(transpiler, sides[side]) == NULL))
            {
                lng_list_printed(defects, "%s to %s in %s takes the anchor %s, and no one transpiler word has it",
                                 lng_cell_or_none(table, row, "past"), lng_cell_or_none(table, row, "present"),
                                 lng_cell_or_none(table, row, "table"), sides[side]);
            }
        }
    }
    unsigned int formed = 0u;
    for (unsigned int at = 0u; at < mnemonics.count; at += 1u)
    {
        unsigned int given = 0u;
        for (unsigned int row = 0u; row < table->rows; row += 1u)
        {
            const char *left = NULL;
            const char *right = NULL;
            if ((strcmp(lng_cell_or_none(table, row, "mnemonic"), mnemonics.text[at]) == 0) &&
                lng_asm_form(transpiler, table, row, &left, &right))
            {
                given += 1u;
            }
        }
        if (given == 0u)
        {
            lng_list_printed(findings, "%s has no transpiler form: no transition of it gives each side a bit",
                             mnemonics.text[at]);
        }
        formed += (given != 0u) ? 1u : 0u;
    }
    lng_list_free(&mnemonics);
    return formed;
}

const char *lng_g_anchor(const LngTable *hol, const LngTable *transpiler, const char *symbol, const char **word)
{
    for (unsigned int row = 0u; row < hol->rows; row += 1u)
    {
        if (!mmgr_text_equal(lng_cell_or_none(hol, row, "type"), "core op", PLATFORM_TEXT_TO_TERMINATOR) ||
            !mmgr_text_equal(lng_cell_or_none(hol, row, symbol), "1", PLATFORM_TEXT_TO_TERMINATOR))
        {
            continue;
        }
        *word = lng_cell_or_none(hol, row, "word");
        // a core op crosses to the transpiler's word of its own name, where its row names no other
        const char *const crossed = lng_cell_or_none(hol, row, "transpiler");
        const unsigned int bridge = lng_bridge_word_row(transpiler, (crossed[0] != '\0') ? crossed : *word);
        if (bridge == transpiler->rows)
        {
            return NULL;
        }
        const char *const anchor = lng_cell_or_none(transpiler, bridge, "anchor");
        return (anchor[0] != '\0') ? anchor : NULL;
    }
    *word = NULL;
    return NULL;
}

unsigned int lng_g_bindings(const LngTable *hol, const LngTable *transpiler, LngList *findings)
{
    LngList symbols = {NULL, 0u, 0u};
    for (unsigned int row = 0u; row < hol->rows; row += 1u)
    {
        if (!mmgr_text_equal(lng_cell_or_none(hol, row, "type"), "type", PLATFORM_TEXT_TO_TERMINATOR))
        {
            lng_operators_of(hol, row, &symbols);
        }
    }
    unsigned int bound = 0u;
    for (unsigned int at = 0u; at < symbols.count; at += 1u)
    {
        const char *word = NULL;
        if (lng_g_anchor(hol, transpiler, symbols.text[at], &word) != NULL)
        {
            bound += 1u;
            continue;
        }
        if (word == NULL)
        {
            lng_list_printed(findings, "the .g operator %s has no core op, and no gsm cases", symbols.text[at]);
        }
        else
        {
            lng_list_printed(findings, "the .g operator %s reads %s, which has no gsm cases", symbols.text[at], word);
        }
    }
    lng_list_free(&symbols);
    return bound;
}
