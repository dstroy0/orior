// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lng.c: the three language tables read, and each held against the tree and against the others (lng.h)
#include "lng.h"

#include "../cu/includes/file_defs/klang/klang.h"
#include "../cu/includes/parsers/who.h"
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

const char *const g_lng_kinds[LNG_KINDS] = {"thing", "doing", "gluing", "record", "ksc", "selection"};
const char *const g_lng_hol_kinds[LNG_HOL_KINDS] = {"type", "plain", "core op"};

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

// the operators a form is held to
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

// the tables whose transitions are held across
static const char s_matrix[] = "the state-transition matrix";
static const char s_high_energy_matrix[] = "the high-energy transition matrix";

// the letters Python's split takes as space
static const char s_space[] = " \t\n\r\v\f\x1c\x1d\x1e\x1f";

// the letters of a name the rulesets write
static const char s_lower[] = "abcdefghijklmnopqrstuvwxyz_";

// a set of texts for each key of a set: the set of `key` holds the keys, and `set` the set of each at its place
typedef struct
{
    LngList key;
    LngList *set;
    unsigned int room;
} LngSets;

// `pointer`, or the run ended where the system gave no memory
static void *lng_held(void *pointer)
{
    if (pointer == NULL)
    {
        fputs("lng: the system gave no memory\n", stderr);
        exit(2);
    }
    return pointer;
}

// `length` letters at `text` copied into an allocation of their own, ended
static char *lng_copy(const char *text, size_t length)
{
    char *const copy = (char *)lng_held(malloc(length + 1u));
    memcpy(copy, text, length);
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
    list->text = (char **)lng_held(realloc(list->text, (size_t)room * sizeof(char *)));
    list->room = room;
}

// `length` letters at `text` put at `at` in `list`, every text from there one later
static void lng_list_insert(LngList *list, unsigned int at, const char *text, size_t length)
{
    lng_list_room(list);
    memmove(&list->text[at + 1u], &list->text[at], (size_t)(list->count - at) * sizeof(char *));
    list->text[at] = lng_copy(text, length);
    list->count += 1u;
}

void lng_list_add(LngList *list, const char *text)
{
    lng_list_insert(list, list->count, text, strlen(text));
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
        lng_held(NULL);
        return;
    }
    // a printed length is not negative
    char *const text = (char *)lng_held(malloc((size_t)length + 1u));
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
        lng_list_insert(set, at, text, strlen(text));
    }
}

// 1 where `set` holds `text`
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
        same = (strcmp(left->text[at], right->text[at]) == 0);
    }
    return same;
}

// <0, 0 or >0 as `left` stands before `right`, with them or after them, text by text, the shorter first where one
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
    const size_t gap = strlen(between);
    size_t length = 0u;
    for (unsigned int at = 0u; at < list->count; at += 1u)
    {
        length += strlen(list->text[at]) + ((at == 0u) ? 0u : gap);
    }
    char *const joined = (char *)lng_held(malloc(length + 1u));
    size_t written = 0u;
    for (unsigned int at = 0u; at < list->count; at += 1u)
    {
        if (at != 0u)
        {
            memcpy(&joined[written], between, gap);
            written += gap;
        }
        const size_t size = strlen(list->text[at]);
        memcpy(&joined[written], list->text[at], size);
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
    lng_list_insert(&map->key, at, key, strlen(key));
    lng_list_insert(&map->value, at, value, strlen(value));
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

// the set `sets` holds for `key`, a set of none put for it where it holds none
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
        sets->set = (LngList *)lng_held(realloc(sets->set, (size_t)sets->room * sizeof(LngList)));
    }
    memmove(&sets->set[at + 1u], &sets->set[at], (size_t)(sets->key.count - at) * sizeof(LngList));
    memset(&sets->set[at], 0, sizeof(LngList));
    lng_list_insert(&sets->key, at, key, strlen(key));
    return &sets->set[at];
}

// the set `sets` holds for `key`, or NULL where it holds none
static const LngList *lng_sets_get(const LngSets *sets, const char *key)
{
    unsigned int at = 0u;
    return lng_set_find(&sets->key, key, &at) ? &sets->set[at] : NULL;
}

// everything `sets` holds let go
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
        found = (strcmp(text, texts[at]) == 0);
    }
    return found;
}

// the cells of `line`, each a text between two tabs, kept after `cells`
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

// the words of `length` letters at `text` apart by space, kept after `tokens`
static void lng_tokens(const char *text, size_t length, LngList *tokens)
{
    size_t at = 0u;
    while (at < length)
    {
        while ((at < length) && (strchr(s_space, text[at]) != NULL) && (text[at] != '\0'))
        {
            at += 1u;
        }
        const size_t start = at;
        while ((at < length) && (strchr(s_space, text[at]) == NULL))
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
            lng_list_printed(defects, "line %u holds %u cells where the head holds %u", number, cells.count,
                             table->head.count);
            lng_list_free(&cells);
            continue;
        }
        if (table->rows == table->room)
        {
            table->room = (table->room == 0u) ? 64u : (table->room * 2u);
            table->row = (char ***)lng_held(realloc(table->row, (size_t)table->room * sizeof(char **)));
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
        if (strcmp(table->head.text[at], column) == 0)
        {
            return table->row[row][at];
        }
    }
    return NULL;
}

// the cell of the row `row` under `column`, or "" where the head names none
static const char *lng_cell_or_none(const LngTable *table, unsigned int row, const char *column)
{
    const char *const cell = lng_cell(table, row, column);
    return (cell != NULL) ? cell : "";
}

// the first operator column of `table`, the one after the last selected column, or the head's count where it has none
static unsigned int lng_operators_start(const LngTable *table)
{
    for (unsigned int at = 0u; at < table->head.count; at += 1u)
    {
        if (strcmp(table->head.text[at], s_selected_last) == 0)
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
            (strcmp(lng_cell_or_none(table, row, column), "1") == 0))
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

void lng_table_check(const LngTable *table, const char *const *kinds, unsigned int count, LngList *defects)
{
    const unsigned int start = lng_operators_start(table);
    LngList seen = {NULL, 0u, 0u};
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        const char *const word = lng_cell_or_none(table, row, "word");
        const char *const kind = lng_cell_or_none(table, row, "kind");
        LngList held = {NULL, 0u, 0u};
        lng_list_printed(&held, "%s\x1f%s", word, kind);
        if (lng_set_has(&seen, held.text[0]))
        {
            lng_list_printed(defects, "%s %s is held twice", word, kind);
        }
        lng_set_add(&seen, held.text[0]);
        lng_list_free(&held);
        if (!lng_one_of(kind, kinds, count))
        {
            lng_list_printed(defects, "%s has the kind %s, which no table names", word, kind);
        }
        for (unsigned int at = start; at < table->head.count; at += 1u)
        {
            const char *const column = table->head.text[at];
            const char *const cell = lng_cell_or_none(table, row, column);
            if (!lng_one_of(column, s_not_operators, LNG_NOT_OPERATORS) && (cell[0] != '\0') &&
                (strcmp(cell, "1") != 0))
            {
                lng_list_printed(defects, "%s holds %s under %s, where an operator cell holds 1 or nothing", word, cell,
                                 column);
            }
        }
    }
    lng_list_free(&seen);

    // each word whose row holds a complement, in the order the words first stand, and the last row of it
    LngList order = {NULL, 0u, 0u};
    unsigned int *const last = (unsigned int *)lng_held(calloc((size_t)table->rows + 1u, sizeof(unsigned int)));
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        if (lng_cell_or_none(table, row, "complement")[0] == '\0')
        {
            continue;
        }
        const char *const word = lng_cell_or_none(table, row, "word");
        unsigned int at = 0u;
        while ((at < order.count) && (strcmp(order.text[at], word) != 0))
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
        while ((other < order.count) && (strcmp(order.text[other], complement) != 0))
        {
            other += 1u;
        }
        if ((other == order.count) || (strcmp(lng_cell_or_none(table, last[other], "complement"), word) != 0))
        {
            lng_list_printed(defects, "%s complements %s, which does not complement it back", word, complement);
            continue;
        }
        LngList mine = {NULL, 0u, 0u};
        LngList theirs = {NULL, 0u, 0u};
        LngList negated = {NULL, 0u, 0u};
        lng_operators_of(table, last[at], &mine);
        lng_operators_of(table, last[other], &theirs);
        // an operator with no complement negates to nothing any set holds
        int negates = 1;
        for (unsigned int one = 0u; one < mine.count; one += 1u)
        {
            unsigned int pair = 0u;
            while ((pair < LNG_COMPLEMENTS) && (strcmp(s_complement[pair][0], mine.text[one]) != 0))
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

// rows of one kind marking the same operators and the same width, sign and type: the kind, the operators, and the
// words, in the order the rows stand
typedef struct
{
    char *key;
    const char *kind;
    LngList operators;
    LngList words;
} LngGroup;

void lng_table_findings(const LngTable *table, LngList *findings)
{
    LngGroup *const group = (LngGroup *)lng_held(calloc((size_t)table->rows + 1u, sizeof(LngGroup)));
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
        lng_list_printed(&key, "%s\x1f%s\x1f%s\x1f%s\x1f%s", lng_cell_or_none(table, row, "kind"), marked,
                         lng_cell_or_none(table, row, "bits"), lng_cell_or_none(table, row, "signed"),
                         lng_cell_or_none(table, row, "c_type"));
        free(marked);
        unsigned int at = 0u;
        while ((at < groups) && (strcmp(group[at].key, key.text[0]) != 0))
        {
            at += 1u;
        }
        if (at == groups)
        {
            group[at].key = lng_copy(key.text[0], strlen(key.text[0]));
            group[at].kind = lng_cell_or_none(table, row, "kind");
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
    // the groups in the order of their words, a group whose words stand with another's kept after it
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
            lng_list_printed(findings, "%s are %s words that all mean %s", words, group[at].kind, operators);
            free(words);
            free(operators);
        }
        free(group[at].key);
        lng_list_free(&group[at].operators);
        lng_list_free(&group[at].words);
    }
    free(group);

    LngList words = {NULL, 0u, 0u};
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        lng_set_add(&words, lng_cell_or_none(table, row, "word"));
    }
    for (unsigned int at = 0u; at < words.count; at += 1u)
    {
        LngList meanings = {NULL, 0u, 0u};
        for (unsigned int row = 0u; row < table->rows; row += 1u)
        {
            if (strcmp(lng_cell_or_none(table, row, "word"), words.text[at]) != 0)
            {
                continue;
            }
            LngList operators = {NULL, 0u, 0u};
            lng_operators_of(table, row, &operators);
            char *const meant = lng_list_joined(&operators, " ");
            lng_list_printed(&meanings, "%s meaning %s", lng_cell_or_none(table, row, "kind"),
                             (meant[0] != '\0') ? meant : "no operator");
            free(meant);
            lng_list_free(&operators);
        }
        if (meanings.count > 1u)
        {
            char *const meaning = lng_list_joined(&meanings, " and ");
            lng_list_printed(findings, "%s is %s", words.text[at], meaning);
            free(meaning);
        }
        lng_list_free(&meanings);
    }
    lng_list_free(&words);
}

void lng_schema_forms(const char *text, LngList *names)
{
    static const char s_form[] = "form_(";
    const char *at = strstr(text, s_form);
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
        at = strstr(next, s_form);
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
    const size_t letters = strspn(&text[at + 1u], s_lower);
    if (letters == 0u)
    {
        return 0;
    }
    size_t end = at + 1u + letters;
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
    *length = letters;
    *past = end + 1u;
    return 1;
}

// the names a token of a construct body uses, kept after `names`: the token where it is a name, or each name in
// braces in it
static void lng_braced_names(const char *token, LngList *names)
{
    const size_t whole = strlen(token);
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

// every word of `name` put in `used` at `where`, where it holds none
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
        lng_tokens(text, strlen(text), &tokens);
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
        if (inside && (strcmp(first, "end") != 0))
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
        inside = (strcmp(first, "construct") == 0) || (inside && (strcmp(first, "end") != 0));
        lng_list_free(&where);
        lng_list_free(&tokens);
    }
}

// Each line of `file` cut by the blocks of the ruleset's language (klang.h): what of the line stands inside a block's
// text kept after `inside`, and what stands outside every block, its opener and its closer among the block, kept after
// `outside`, a text each for every line, "" where the line holds none
static void lng_block_parts(const KfileLines *file, LngList *inside, LngList *outside)
{
    // the lines joined, each ended by a newline, and where each starts
    static char s_text_bytes[QRY_WRITER_RING_BYTES / 2ull];
    size_t length = 0u;
    size_t *const starts = (size_t *)lng_held(calloc((size_t)file->lines + 1u, sizeof(size_t)));
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
        lng_held(NULL);
    }
    KlangBlock *block = NULL;
    unsigned int blocks = 0u;
    size_t from = 0u;
    KlangBlock found;
    while (klang_block_find(s_text_bytes, length, from, "krs", &found))
    {
        block = (KlangBlock *)lng_held(realloc(block, ((size_t)blocks + 1u) * sizeof(KlangBlock)));
        block[blocks] = found;
        blocks += 1u;
        from = found.closed;
    }
    for (unsigned int at = 0u; at < file->lines; at += 1u)
    {
        const size_t start = starts[at];
        const size_t end = start + strlen(file->line[at]);
        static char s_in_bytes[QRY_WRITER_RING_BYTES / 2ull];
        size_t in = 0u;
        static char s_out_bytes[QRY_WRITER_RING_BYTES / 2ull];
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
            lng_held(NULL);
        }
        lng_list_insert(inside, inside->count, s_in_bytes, in);
        lng_list_insert(outside, outside->count, s_out_bytes, out);
    }
    free(block);
    free(starts);
}

void lng_ruleset_lines(const KfileLines *file, LngList *kept)
{
    LngList outside = {NULL, 0u, 0u};
    lng_block_parts(file, kept, &outside);
    lng_list_free(&outside);
}

// every match of "folds the <fold> put for <put>" in `line`, the two kept after `folds` and `puts`
static void lng_folds(const char *line, LngList *folds, LngList *puts)
{
    static const char s_folds[] = "folds the ";
    static const char s_put[] = " put for ";
    const char *at = strstr(line, s_folds);
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
        at = strstr(next, s_folds);
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
        int held = 0;
        if ((line[0] == '\0') || (line[0] == '#') || kfile_lines_who_line(line, &who, &held))
        {
            continue;
        }
        LngList tokens = {NULL, 0u, 0u};
        lng_tokens(line, strlen(line), &tokens);
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
        if ((strcmp(tokens.text[0], "count") == 0) && (tokens.count > 2u))
        {
            lng_map_put(used, tokens.text[2], where.text[0]);
        }
        if ((strcmp(tokens.text[0], "compile") == 0) && (tokens.count > 3u))
        {
            LngList folds = {NULL, 0u, 0u};
            LngList puts = {NULL, 0u, 0u};
            lng_folds(line, &folds, &puts);
            // the fourth word, its colon cut
            const size_t named = strlen(tokens.text[3]);
            size_t kept = named;
            while ((kept != 0u) && (tokens.text[3][kept - 1u] == ':'))
            {
                kept -= 1u;
            }
            tokens.text[3][kept] = '\0';
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

// every name in backticks, `[a-z_]+`, of the `length` letters at `text` put in `set`
static void lng_ticked(const char *text, size_t length, LngList *set)
{
    size_t at = 0u;
    while (at < length)
    {
        size_t letters = 0u;
        while (((at + 1u + letters) < length) && (strchr(s_lower, text[at + 1u + letters]) != NULL) &&
               (text[at + 1u + letters] != '\0'))
        {
            letters += 1u;
        }
        if ((text[at] == '`') && (letters != 0u) && ((at + 1u + letters) < length) && (text[at + 1u + letters] == '`'))
        {
            char *const name = lng_copy(&text[at + 1u], letters);
            lng_set_add(set, name);
            free(name);
            at += letters + 2u;
        }
        else
        {
            at += 1u;
        }
    }
}

// the part of the `length` letters at `text` from past its `field`th `mark` to the next, or to their end: 1 with its
// start and length, or 0 where they hold fewer marks
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
    const char *const things = strstr(text, "**Thing words.**");
    const char *const doings = strstr(text, "**Doing words.**");
    const char *const gluings = strstr(text, "**Gluing words.**");
    if ((things == NULL) || (doings == NULL) || (gluings == NULL))
    {
        return;
    }
    const char *line = things;
    while (line < doings)
    {
        const char *const newline = memchr(line, '\n', (size_t)(doings - line));
        const size_t length = (newline != NULL) ? (size_t)(newline - line) : (size_t)(doings - line);
        char *const held = lng_copy(line, length);
        size_t start = 0u;
        size_t size = 0u;
        if ((strncmp(held, "| ", 2u) == 0) && (strstr(held, "---") == NULL) && (strstr(held, "| kind |") == NULL) &&
            lng_field(held, length, '|', 2u, &start, &size))
        {
            lng_ticked(&held[start], size, thing);
        }
        free(held);
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
static void lng_punctuator_kept(const char *punctuator, LngList *found)
{
    const size_t length = strlen(punctuator);
    static const char *const s_compared[4] = {"<=", ">=", "==", "!="};
    if ((length > 1u) && (punctuator[length - 1u] == '=') && !lng_one_of(punctuator, s_compared, 4u))
    {
        size_t kept = length;
        while ((kept != 0u) && (punctuator[kept - 1u] == '='))
        {
            kept -= 1u;
        }
        char *const operator_text = lng_copy(punctuator, kept);
        lng_set_add(found, operator_text);
        free(operator_text);
        return;
    }
    lng_set_add(found, punctuator);
}

// the length of a literal or a name at `text`, or 0 where none starts there: a name in braces, a C name, a hex number
// with its u, or a decimal number with its suffix
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
    if ((text[0] != '\0') && (strchr(s_name_start, text[0]) != NULL))
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
        const size_t suffix = strlen(s_suffix[at]);
        if (strncmp(&text[digits], s_suffix[at], suffix) == 0)
        {
            return digits + suffix;
        }
    }
    return digits;
}

void lng_c_operators(const char *text, LngList *operators)
{
    const size_t length = strlen(text);
    // the text with each written newline a newline, then each note from `//` to the next backslash cut, then each
    // literal and name a space
    char *const unescaped = (char *)lng_held(malloc(length + 1u));
    size_t size = 0u;
    for (size_t at = 0u; at < length; at += 1u)
    {
        const int newline = (text[at] == '\\') && (text[at + 1u] == 'n');
        unescaped[size] = newline ? '\n' : text[at];
        size += 1u;
        at += newline ? 1u : 0u;
    }
    unescaped[size] = '\0';
    char *const unnoted = (char *)lng_held(malloc(size + 1u));
    size_t kept = 0u;
    for (size_t at = 0u; at < size;)
    {
        if ((unescaped[at] == '/') && (unescaped[at + 1u] == '/'))
        {
            at += 2u + strcspn(&unescaped[at + 2u], "\\");
            continue;
        }
        unnoted[kept] = unescaped[at];
        kept += 1u;
        at += 1u;
    }
    unnoted[kept] = '\0';
    char *const stripped = (char *)lng_held(malloc(kept + 1u));
    size_t left = 0u;
    for (size_t at = 0u; at < kept;)
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
        if (strchr(s_space, stripped[at]) != NULL)
        {
            at += 1u;
            continue;
        }
        unsigned int which = 0u;
        while ((which < LNG_PUNCTUATORS) &&
               (strncmp(&stripped[at], s_punctuators[which], strlen(s_punctuators[which])) != 0))
        {
            which += 1u;
        }
        if (which == LNG_PUNCTUATORS)
        {
            at += 1u;
            continue;
        }
        lng_punctuator_kept(s_punctuators[which], &found);
        at += strlen(s_punctuators[which]);
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
        if (strcmp(s_family[at][0], operator_text) == 0)
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
        const char *const word = lng_cell_or_none(table, row, "word");
        LngList *const claimed = lng_sets_at(&claims, word);
        LngList operators = {NULL, 0u, 0u};
        lng_operators_of(table, row, &operators);
        for (unsigned int at = 0u; at < operators.count; at += 1u)
        {
            lng_set_add(claimed, operators.text[at]);
        }
        lng_list_free(&operators);
        if (strcmp(lng_cell_or_none(table, row, "kind"), "doing") == 0)
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

// the last row of `table` whose word is `word` and whose kind is `kind`, or the table's count of rows where none is
static unsigned int lng_row_of(const LngTable *table, const char *word, const char *kind)
{
    unsigned int found = table->rows;
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        found = ((strcmp(lng_cell_or_none(table, row, "word"), word) == 0) &&
                 (strcmp(lng_cell_or_none(table, row, "kind"), kind) == 0))
                    ? row
                    : found;
    }
    return found;
}

void lng_hol_check(const LngTable *transpiler, const LngTable *hol, LngList *defects, LngList *findings)
{
    LngList things = {NULL, 0u, 0u};
    LngList words = {NULL, 0u, 0u};
    for (unsigned int row = 0u; row < transpiler->rows; row += 1u)
    {
        lng_set_add(&words, lng_cell_or_none(transpiler, row, "word"));
        if (strcmp(lng_cell_or_none(transpiler, row, "kind"), "thing") == 0)
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
            lng_list_printed(defects, "%s crosses to %s, which the transpiler holds no word for", word, crossed);
        }
        else if ((other != transpiler->rows) && ((strcmp(lng_cell_or_none(transpiler, other, "bits"), bits) != 0) ||
                                                 (strcmp(lng_cell_or_none(transpiler, other, "signed"), sign) != 0)))
        {
            lng_list_printed(defects, "%s is %s bits signed %s and crosses to %s, which is %s bits signed %s", word,
                             bits, sign, lng_cell_or_none(transpiler, other, "word"),
                             lng_cell_or_none(transpiler, other, "bits"),
                             lng_cell_or_none(transpiler, other, "signed"));
        }
    }
    LngList held = {NULL, 0u, 0u};
    for (unsigned int row = 0u; row < hol->rows; row += 1u)
    {
        if (strcmp(lng_cell_or_none(hol, row, "kind"), "type") == 0)
        {
            LngList width = {NULL, 0u, 0u};
            lng_list_printed(&width, "%s\x1f%s", lng_cell_or_none(hol, row, "bits"),
                             lng_cell_or_none(hol, row, "signed"));
            lng_set_add(&held, width.text[0]);
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
        if (lng_one_of(bits, s_hol_bits, LNG_HOL_BITS) && !lng_set_has(&held, width.text[0]))
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
        const int core = (strcmp(lng_cell_or_none(hol, row, "kind"), "core op") == 0);
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
        if (strcmp(lng_cell_or_none(transpiler, row, "kind"), "doing") != 0)
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
    lng_list_free(&held);
    lng_list_free(&words);
    lng_list_free(&things);
}

const char *lng_arrow_of(const LngTable *table, unsigned int row)
{
    const char *const left = lng_cell_or_none(table, row, "left");
    const char *const right = lng_cell_or_none(table, row, "right");
    if (strcmp(lng_cell_or_none(table, row, "table"), "the branch pair") == 0)
    {
        return "";
    }
    if ((strcmp(left, "-") == 0) || (strcmp(right, "-") == 0))
    {
        return "*>";
    }
    if ((strcmp(left, "=") != 0) && (strcmp(right, "=") != 0))
    {
        return "x>";
    }
    return "->";
}

// `side`, lead and rite trading places
static const char *lng_mirror(const char *side)
{
    return (strcmp(side, "lead") == 0) ? "rite" : ((strcmp(side, "rite") == 0) ? "lead" : side);
}

// the `field`th field of `key`, its fields apart by \x1f, copied into an allocation of the caller's to free
static char *lng_key_field(const char *key, unsigned int field)
{
    size_t start = 0u;
    size_t size = 0u;
    if (!lng_field(key, strlen(key), '\x1f', field, &start, &size))
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
        if (strcmp(given, arrow) != 0)
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
            if (strcmp(lng_cell_or_none(table, row, "mnemonic"), mnemonics.text[at]) == 0)
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

    // each transition, its table, its past and its present apart by \x1f, which stands before every letter a cell
    // holds: the keys stand in the order of the three; and the mnemonics each table names it by
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
        if ((strcmp(left, "-") != 0) && (strcmp(right, "-") != 0))
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
        if ((strcmp(table_name, s_matrix) == 0) || (strcmp(table_name, s_high_energy_matrix) == 0))
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
        if ((strcmp(key.text[0], named.key.text[at]) != 0) && (mirror != NULL) &&
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

// the entry `name` of the folder `folder`, a folder where `folder_held`: each file under it the walk seeks put in its
// set where it is a folder, or it where it is a file that ends in a suffix the walk seeks, as a path from the folder
// the run stands in
static void lng_files_under(const char *folder, const LngWalk *walk);
static void lng_entry_taken(const char *folder, const char *name, int folder_held, const LngWalk *walk)
{
    if ((strcmp(name, ".") == 0) || (strcmp(name, "..") == 0))
    {
        return;
    }
    LngList path = {NULL, 0u, 0u};
    lng_list_printed(&path, "%s/%s", folder, name);
    const size_t length = strlen(name);
    if (folder_held)
    {
        lng_files_under(path.text[0], walk);
    }
    for (unsigned int at = 0u; !folder_held && (at < walk->count); at += 1u)
    {
        const size_t ending = strlen(walk->suffixes[at]);
        if ((length >= ending) && (strcmp(&name[length - ending], walk->suffixes[at]) == 0))
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
    // the entries asked for in large reads, without the short names nothing here reads
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
        struct stat held;
        const int folder_held = (stat(path.text[0], &held) == 0) && S_ISDIR(held.st_mode);
        lng_list_free(&path);
        lng_entry_taken(folder, entry->d_name, folder_held, walk);
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
