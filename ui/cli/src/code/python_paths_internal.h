// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths_internal.h: what the Python paths sources share: the expressions read from one script, their texts and
// the values they may be, held while the script is read under the module's lock
#ifndef PYTHON_PATHS_INTERNAL_H
#define PYTHON_PATHS_INTERNAL_H

#include "python_paths.h"

// the most expressions, list items, letters of one expression, bytes of texts, values and names one script's reading
// holds; no expression; and the most values a pairing of two lists gives
#define CLI_PY_EXPRS 16384u
#define CLI_PY_ITEMS 16384u
#define CLI_PY_LETTERS (1u << 20u)
#define CLI_PY_TEXT (1u << 22u)
#define CLI_PY_VALUES 65536u
#define CLI_PY_NAMES 1024u
#define CLI_PY_NONE 0xffffffffu
#define CLI_PY_PAIRS 256u

// the kinds of an expression: a name, a string, a whole number, a call, an attribute, an index, two joined by `/`,
// two added by `+`, and a tuple or a list
enum
{
    CLI_EXPR_NAME,
    CLI_EXPR_TEXT,
    CLI_EXPR_NUMBER,
    CLI_EXPR_CALL,
    CLI_EXPR_ATTR,
    CLI_EXPR_INDEX,
    CLI_EXPR_JOIN,
    CLI_EXPR_ADD,
    CLI_EXPR_MANY
};

// An expression: its kind; its name or string in the texts; its number; the expression it is of, or the left of two;
// the index, or the right of two; and its arguments or items, `count` of them from `first` in the items
typedef struct
{
    unsigned int kind;
    unsigned int text;
    long long number;
    unsigned int left;
    unsigned int right;
    unsigned int first;
    unsigned int count;
} CliExpr;

// What an expression may be: `count` texts, their places in the texts listed from `first` in the values
typedef struct
{
    unsigned int first;
    unsigned int count;
} CliValues;

// A name the script set, its place in the texts, and what it may be
typedef struct
{
    unsigned int name;
    CliValues values;
} CliPyName;

// One script's reading: its expressions, the items of their lists and the items of lists being read, the letters of
// the expression being read and where the reading stands, the texts, the values, the lists being joined into one, the
// names set, the script's path, and what it puts on `sys.path`, as places in the texts
typedef struct
{
    CliExpr expr[CLI_PY_EXPRS];
    unsigned int exprs;
    unsigned int item[CLI_PY_ITEMS];
    unsigned int items;
    unsigned int stack[CLI_PY_ITEMS];
    unsigned int stacked;
    unsigned int letter[CLI_PY_LETTERS];
    unsigned int letters;
    unsigned int at;
    char text[CLI_PY_TEXT];
    unsigned int used;
    unsigned int value[CLI_PY_VALUES];
    unsigned int values;
    CliValues range[CLI_PY_ITEMS];
    unsigned int ranged;
    CliPyName name[CLI_PY_NAMES];
    unsigned int names;
    char file[CLI_PATH_LONGEST];
    unsigned int found[CLI_PY_VALUES];
    unsigned int founds;
} CliPy;

extern CliPy g_cli_py;

// `length` bytes of `text` put in the texts, a 0 after them: their place, or CLI_PY_NONE where they do not fit; the
// text at a place; and the expression `length` bytes of `text` say, read whole: its place, or CLI_PY_NONE where it
// is not one the reading takes (python_paths_parse.c)
unsigned int cli_py_text_put(const char *text, unsigned long long length);
const char *cli_py_text(unsigned int at);
unsigned int cli_py_parse(const char *text, unsigned long long length);

// a text put after the values; the values from `first` to the end as a list; the dotted name an expression of names
// and attributes writes, as `os.path.join`, into `out`: 1, or 0 where it writes none; what an expression may be, by
// the names set, in the script; the folders in a folder, each a value a loop over its entries takes; and the folders
// above a path, itself first, up to the top of its drive (python_paths_eval.c)
void cli_py_value_put(unsigned int text);
CliValues cli_py_values_from(unsigned int first);
int cli_py_dotted(unsigned int expr, CliLine *out);
CliValues cli_py_eval(unsigned int expr);
CliValues cli_py_folders_in(const char *folder);
CliValues cli_py_upward(const char *path);
// the folder that holds `path`, as `os.path.dirname` gives it, written into `out` (python_paths_eval.c)
void cli_py_parent(const char *path, CliLine *out);

// what the script at `file`, `size` bytes of `text`, puts on `sys.path`, as it may write them, read into the found
// (python_paths.c)
void cli_py_paths_in(const char *file, const char *text, unsigned long long size);

#endif
