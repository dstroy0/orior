// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths_internal.h: what the Python paths sources share: the expressions read from one script, their texts and
// the values they may be, stored while the script is read under the module's lock
#ifndef PYTHON_PATHS_INTERNAL_H
#define PYTHON_PATHS_INTERNAL_H
#include "../command_line/pool.h"
#include "embed_dispatch_layout.h"
#include "python_paths.h"

typedef struct CliPythonPathsTextTextMaxCtx CliPythonPathsTextTextMaxCtx;
typedef struct CliPythonPathsSizeCtx CliPythonPathsSizeCtx;
typedef struct CliPythonPathsExprCtx CliPythonPathsExprCtx;
typedef struct CliPythonPathsOutCtx CliPythonPathsOutCtx;
typedef struct CliPythonPathsTextCtx CliPythonPathsTextCtx;
typedef struct CliPythonPathsCtx CliPythonPathsCtx;
typedef struct CliPythonPathsAtCtx CliPythonPathsAtCtx;
typedef struct CliPythonPathsTextKindCtx CliPythonPathsTextKindCtx;
typedef struct CliPythonPathsFirstCtx CliPythonPathsFirstCtx;

// the most expressions, list items, letters of one expression, bytes of texts, values and names one script's reading
// contains; no expression; and the most values a pairing of two lists gives. A reading's own most of each is sized to
// its script within these
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
// names set, the script's path, and what it puts on `sys.path`, as places in the texts. Its lists are one cell taken
// for the script, each sized to it: `exprs_max` expressions, `items_max` items, stacked items and ranges,
// `letters_max` letters and `text_max` bytes of texts, and CLI_PY_VALUES values and found
typedef struct
{
    CliExpr *expr;
    unsigned int exprs;
    unsigned int exprs_max;
    unsigned int *item;
    unsigned int items;
    unsigned int items_max;
    unsigned int *stack;
    unsigned int stacked;
    unsigned int *letter;
    unsigned int letters;
    unsigned int letters_max;
    unsigned int at;
    char *text;
    unsigned int used;
    unsigned int text_max;
    unsigned int *value;
    unsigned int values;
    CliValues *range;
    unsigned int ranged;
    CliPyName name[CLI_PY_NAMES];
    unsigned int names;
    char file[CLI_PATH_MAX_LEN];
    unsigned int *found;
    unsigned int founds;
} CliPy;

extern CliPy g_cli_py;

// the reading of a script of `size` bytes begun empty in a cell sized to it, the reading before given back: 1, or 0
// where the cell is refused and the reading contains nothing; and the reading's cell given back once what it found is
// read (python_paths.c)
int cli_py_read_state_alloc(const CliPythonPathsSizeCtx *args);
void cli_py_read_state_free(void);
// the logical lines of a script read into the reading's cell: `text_max` bytes of their text at `text`, and the start
// and length of `lines_max` lines at `start` and `length`, none read yet (python_paths.c)
void cli_py_lines_position(const CliPythonPathsTextTextMaxCtx *args);

// `length` bytes of `text` put in the texts, a 0 after them: their place, or CLI_PY_NONE where they do not fit; the
// text at a place; and the expression `length` bytes of `text` denote, read whole: its place, or CLI_PY_NONE where it
// is not one the reading takes (python_paths_parse.c)
unsigned int cli_py_text_append(const CliPythonPathsTextCtx *args);
const char *cli_py_text(const CliPythonPathsAtCtx *args);
unsigned int cli_py_parse(const CliPythonPathsTextCtx *args);

// a text put after the values; the values from `first` to the end as a list; the dotted name an expression of names
// and attributes writes, as `os.path.join`, into `out`: 1, or 0 where it writes none; what an expression may be, by
// the names set, in the script; the folders in a folder, each a value a loop over its entries takes; and the folders
// above a path, itself first, up to the top of its drive (python_paths_eval.c)
void cli_py_value_append(const CliPythonPathsTextKindCtx *args);
CliValues cli_py_values_from(const CliPythonPathsFirstCtx *args);
int cli_py_dot(const CliPythonPathsExprCtx *args);
CliValues cli_py_eval(const CliPythonPathsExprCtx *args);
CliValues cli_py_folders_in(const CliPythonPathsCtx *args);
CliValues cli_py_upward(const CliPythonPathsOutCtx *args);
// the folder that contains `path`, as `os.path.dirname` gives it, written into `out` (python_paths_eval.c)
void cli_py_parent(const CliPythonPathsOutCtx *args);

// what the script at `file`, `size` bytes of `text`, puts on `sys.path`, as it may write them, read into the found,
// the reading taken for it and released through persistent_buf_release once the found are read; none found where its
// cell is refused (python_paths.c)
void cli_py_paths_in(const CliPythonPathsSizeCtx *args);

struct CliPythonPathsTextTextMaxCtx
{
    char *text_out;
    unsigned int text_max;
    unsigned int *start;
    unsigned int *length_u32s;
    unsigned int lines_max;
};

struct CliPythonPathsSizeCtx
{
    unsigned long long size;
    const char *text;
    const char *file;
};

struct CliPythonPathsExprCtx
{
    unsigned int expr;
    VerbaLinea *out;
};

struct CliPythonPathsOutCtx
{
    VerbaLinea *out;
    const char *path;
};

struct CliPythonPathsTextCtx
{
    const char *text;
    unsigned long long length;
};

struct CliPythonPathsCtx
{
    const char *folder;
};

struct CliPythonPathsAtCtx
{
    unsigned int at;
};

struct CliPythonPathsTextKindCtx
{
    unsigned int text_u32;
};

struct CliPythonPathsFirstCtx
{
    unsigned int first;
};

typedef struct
{
    int (*py_read_state_alloc)(const CliPythonPathsSizeCtx *args);
    void (*py_paths_in)(const CliPythonPathsSizeCtx *args);
} CliPythonPathsSizeNs;
EMBED_TABLE_LAYOUT(CliPythonPathsSizeNs, py_read_state_alloc, py_paths_in);
EMBED_TABLE_STORAGE CliPythonPathsSizeNs cli_python_paths_size_internal EMBED_UNUSED = {
    .py_read_state_alloc = cli_py_read_state_alloc,
    .py_paths_in = cli_py_paths_in,
};

typedef struct
{
    void (*py_lines_position)(const CliPythonPathsTextTextMaxCtx *args);
} CliPythonPathsTextTextMaxNs;
EMBED_TABLE_LAYOUT(CliPythonPathsTextTextMaxNs, py_lines_position);
EMBED_TABLE_STORAGE CliPythonPathsTextTextMaxNs cli_python_paths_text_text_max_internal EMBED_UNUSED = {
    .py_lines_position = cli_py_lines_position,
};

typedef struct
{
    unsigned int (*py_text_append)(const CliPythonPathsTextCtx *args);
    unsigned int (*py_parse)(const CliPythonPathsTextCtx *args);
} CliPythonPathsTextNs;
EMBED_TABLE_LAYOUT(CliPythonPathsTextNs, py_text_append, py_parse);
EMBED_TABLE_STORAGE CliPythonPathsTextNs cli_python_paths_text_internal EMBED_UNUSED = {
    .py_text_append = cli_py_text_append,
    .py_parse = cli_py_parse,
};

typedef struct
{
    const char *(*py_text)(const CliPythonPathsAtCtx *args);
} CliPythonPathsAtNs;
EMBED_TABLE_LAYOUT(CliPythonPathsAtNs, py_text);
EMBED_TABLE_STORAGE CliPythonPathsAtNs cli_python_paths_at_internal EMBED_UNUSED = {
    .py_text = cli_py_text,
};

typedef struct
{
    void (*py_value_append)(const CliPythonPathsTextKindCtx *args);
} CliPythonPathsTextKindNs;
EMBED_TABLE_LAYOUT(CliPythonPathsTextKindNs, py_value_append);
EMBED_TABLE_STORAGE CliPythonPathsTextKindNs cli_python_paths_text_kind_internal EMBED_UNUSED = {
    .py_value_append = cli_py_value_append,
};

typedef struct
{
    CliValues (*py_values_from)(const CliPythonPathsFirstCtx *args);
} CliPythonPathsFirstNs;
EMBED_TABLE_LAYOUT(CliPythonPathsFirstNs, py_values_from);
EMBED_TABLE_STORAGE CliPythonPathsFirstNs cli_python_paths_first_internal EMBED_UNUSED = {
    .py_values_from = cli_py_values_from,
};

typedef struct
{
    int (*py_dot)(const CliPythonPathsExprCtx *args);
    CliValues (*py_eval)(const CliPythonPathsExprCtx *args);
} CliPythonPathsExprNs;
EMBED_TABLE_LAYOUT(CliPythonPathsExprNs, py_dot, py_eval);
EMBED_TABLE_STORAGE CliPythonPathsExprNs cli_python_paths_expr_internal EMBED_UNUSED = {
    .py_dot = cli_py_dot,
    .py_eval = cli_py_eval,
};

typedef struct
{
    CliValues (*py_folders_in)(const CliPythonPathsCtx *args);
} CliPythonPathsNsTable;
EMBED_TABLE_LAYOUT(CliPythonPathsNsTable, py_folders_in);
EMBED_TABLE_STORAGE CliPythonPathsNsTable cli_python_paths_internal EMBED_UNUSED = {
    .py_folders_in = cli_py_folders_in,
};

typedef struct
{
    CliValues (*py_upward)(const CliPythonPathsOutCtx *args);
    void (*py_parent)(const CliPythonPathsOutCtx *args);
} CliPythonPathsOutNs;
EMBED_TABLE_LAYOUT(CliPythonPathsOutNs, py_upward, py_parent);
EMBED_TABLE_STORAGE CliPythonPathsOutNs cli_python_paths_out_internal EMBED_UNUSED = {
    .py_upward = cli_py_upward,
    .py_parent = cli_py_parent,
};

#endif
