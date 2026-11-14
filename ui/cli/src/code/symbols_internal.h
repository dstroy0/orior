// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_internal.h: the languages a reading reads its files in: the language each file opens as, by the plugins'
// extensions, and of those, each whose plugin has a grammar, its grammar stored for the colorer as it is first read
// (symbols_read.c)
#ifndef SYMBOLS_INTERNAL_H
#define SYMBOLS_INTERNAL_H
#include "../editor/highlight.h"
#include "../plugins/plugins.h"
#include "embed_dispatch_layout.h"
#include "symbols.h"

typedef struct CliSymbolsPathCtx CliSymbolsPathCtx;
typedef struct CliSymbolsReadLanguagesTextCtx CliSymbolsReadLanguagesTextCtx;
typedef struct CliSymbolsPathReadLanguagesTextCtx CliSymbolsPathReadLanguagesTextCtx;
typedef struct CliSymbolsPathReadLanguagesCtx CliSymbolsPathReadLanguagesCtx;
typedef struct CliSymbolsSizeCtx CliSymbolsSizeCtx;
typedef struct CliSymbolsLanguagesCtx CliSymbolsLanguagesCtx;
typedef struct CliSymbolsTextCtx CliSymbolsTextCtx;
typedef struct CliSymbolsPathNameCtx CliSymbolsPathNameCtx;
typedef struct CliSymbolsEffectCtx CliSymbolsEffectCtx;
typedef struct CliSymbolsNameCtx CliSymbolsNameCtx;
typedef struct CliSymbolsTextLengthCtx CliSymbolsTextLengthCtx;
typedef struct CliSymbolsPathEffectCtx CliSymbolsPathEffectCtx;
typedef struct CliSymbolsReadCtx CliSymbolsReadCtx;
typedef struct CliSymbolsReadLeafCtx CliSymbolsReadLeafCtx;
typedef struct CliSymbolsReadLeafTypeCtx CliSymbolsReadLeafTypeCtx;
typedef struct CliSymbolsCtx CliSymbolsCtx;
typedef struct CliSymbolsReadLanguagesCtx CliSymbolsReadLanguagesCtx;
typedef struct CliSymbolsArgumentCtx CliSymbolsArgumentCtx;

// the most languages a reading holds whose plugins have a grammar
#define CLI_SYMBOLS_GRAMMARS 64u

// the most bytes of a language's directive mark
#define CLI_SYMBOLS_MARK_MAX 16u

// One language whose plugin has a grammar: its id, the key its grammar is stored under, whether it is stored, 1,
// refused, -1, or not yet sought, 0, whether its grammar names a declaration, the mark its plugin names a directive's
// line by, and the mark that ends a directive's line it carries on past, each empty for none
typedef struct
{
    char id[CLI_NAME_MAX_LEN];
    char key[CLI_NAME_MAX_LEN];
    int stored;
    int declares;
    char directive_mark[CLI_SYMBOLS_MARK_MAX];
    char directive_continued[CLI_SYMBOLS_MARK_MAX];
} CliSymbolsGrammar;

// The languages a reading reads its files in: the language each file opens as, and those whose plugins have a grammar
struct CliSymbolsLanguages
{
    CliLanguages languages;
    CliSymbolsGrammar grammar[CLI_SYMBOLS_GRAMMARS];
    unsigned int grammars;
};

// whether the file at `path` is read by the languages for its declarations, its grammar naming one; its declarations
// read by them, each handed to `each` with its type, a text that stands while `each` runs; the leaves of `size` bytes
// of `text` read by them, in the language of the file at `path` or in the language `id`, the first line starting in
// `state`, each line's handed to `each` from a copy of the text in a cell whose lines stand at the text's own bytes,
// each line ended by a NUL: 1, or 0 where the language is none read here or the cells cannot meet the copy; and the
// byte of `line`, `size` bytes, the column `col` stands at in UTF-16 units, walked on from `*byte` at column `*at`
// (symbols_read.c)
int cli_symbols_reads_in(const CliSymbolsReadLanguagesCtx *args);
// the grammar of the file at `path`: the mark its plugin names a directive's line by, and the mark that ends a
// directive's line it carries on past, each empty where it names none (symbols_read.c)
const char *cli_symbols_directive_mark(const CliSymbolsReadLanguagesCtx *args);
const char *cli_symbols_directive_continued(const CliSymbolsReadLanguagesCtx *args);
void cli_symbols_declared_in(const CliSymbolsPathReadLanguagesCtx *args);
int cli_symbols_leaves_in(const CliSymbolsPathReadLanguagesTextCtx *args);
int cli_symbols_language_leaves_in(const CliSymbolsReadLanguagesTextCtx *args);
unsigned long long cli_symbols_byte_at(const CliSymbolsSizeCtx *args);

// what a leaf is, read off its token's name: the empty name; a name whose first segment is `string`, `comment`,
// `number`, `identifier` or `predefined`, `keyword` or `type`; a name holding the segment `memory`, `parameter`,
// `local` or `declaration`; a type's name holding the segment `number` or `string`
#define CLI_LEAF_BLANK 0x0001ull
#define CLI_LEAF_STRING 0x0002ull
#define CLI_LEAF_COMMENT 0x0004ull
#define CLI_LEAF_NUMBER 0x0008ull
#define CLI_LEAF_NAME 0x0010ull
#define CLI_LEAF_KEYWORD 0x0020ull
#define CLI_LEAF_TYPE 0x0040ull
#define CLI_LEAF_MEMORY 0x0080ull
#define CLI_LEAF_PARAMETER 0x0100ull
#define CLI_LEAF_LOCAL 0x0200ull
#define CLI_LEAF_DECLARES 0x0400ull
#define CLI_LEAF_OF_NUMBER 0x0800ull
#define CLI_LEAF_OF_STRING 0x1000ull
// and a directive's role, a segment of a name whose second segment is `directive`: `include`, `define`, `undef`, the
// conditions `if`, `ifdef`, `ifndef`, `elif`, `else` and `endif`, `defined` in a condition, and `continued`, the mark
// that carries a directive on to the next line
#define CLI_LEAF_INCLUDE 0x2000ull
#define CLI_LEAF_DEFINE 0x4000ull
#define CLI_LEAF_UNDEF 0x8000ull
#define CLI_LEAF_IF 0x10000ull
#define CLI_LEAF_IFDEF 0x20000ull
#define CLI_LEAF_IFNDEF 0x40000ull
#define CLI_LEAF_ELIF 0x80000ull
#define CLI_LEAF_ELSE 0x100000ull
#define CLI_LEAF_ENDIF 0x200000ull
#define CLI_LEAF_DEFINED 0x400000ull
#define CLI_LEAF_CONTINUED 0x80000000ull
#define CLI_LEAF_DIRECTIVE                                                                                             \
    (CLI_LEAF_INCLUDE | CLI_LEAF_DEFINE | CLI_LEAF_UNDEF | CLI_LEAF_IF | CLI_LEAF_IFDEF | CLI_LEAF_IFNDEF |            \
     CLI_LEAF_ELIF | CLI_LEAF_ELSE | CLI_LEAF_ENDIF)
// and a role a segment of any other name names: `rest`, a parameter or mark taking every argument past those before
// it; `join`, an operator joining the leaves about it into one; `quote`, one making the leaf after it a string;
// `unary`, an operator before a value; `choice`, one of the two choosing between two values, the second naming
// `otherwise` as well; `prefix`, a number's prefix; `memory`, an operator on what a name holds or where; and in a
// string's name `include`, the file an include reaches, looked for beside the file including it first where it names
// `adjacent` as well
#define CLI_LEAF_REMAINDER 0x800000ull
#define CLI_LEAF_JOIN 0x1000000ull
#define CLI_LEAF_QUOTE 0x2000000ull
#define CLI_LEAF_UNARY 0x4000000ull
#define CLI_LEAF_CHOICE 0x8000000ull
#define CLI_LEAF_PREFIX 0x10000000ull
#define CLI_LEAF_INCLUDED 0x20000000ull
#define CLI_LEAF_ADJACENT 0x40000000ull
#define CLI_LEAF_OTHERWISE 0x100000000ull
// and the structure a grammar names: `open` and `close`, a bracket that opens or closes a pair; `region`, a bracket
// whose pair encloses a region of statements; `statement`, the mark that ends a statement; `separator`, the mark
// between the items of a list; `call`, a name a call names; `assign`, the operator that gives a name its value;
// `function`, with `declaration`, the name a function is declared by; and `depth`, a token whose `open` or `close`
// counts the depth its pair stands at
#define CLI_LEAF_OPEN 0x200000000ull
#define CLI_LEAF_CLOSE 0x400000000ull
#define CLI_LEAF_REGION 0x800000000ull
#define CLI_LEAF_STATEMENT 0x1000000000ull
#define CLI_LEAF_SEPARATOR 0x2000000000ull
#define CLI_LEAF_CALL 0x4000000000ull
#define CLI_LEAF_ASSIGN 0x8000000000ull
#define CLI_LEAF_FUNCTION 0x10000000000ull
#define CLI_LEAF_DEPTH 0x20000000000ull

// the operation an operator's name names in a segment past its first, 0 for none: `not`, `complement`, `multiply`,
// `divide`, `remainder`, `add`, `subtract`, `shiftleft`, `shiftright`, `less`, `atmost`, `greater`, `atleast`, `equal`,
// `unequal`, `and`, `or`, `bitand`, `bitxor` and `bitor`, numbered from 1 in that order
#define CLI_DOES_NOT 1u
#define CLI_DOES_COMPLEMENT 2u
#define CLI_DOES_MULTIPLY 3u
#define CLI_DOES_DIVIDE 4u
#define CLI_DOES_REMAINDER 5u
#define CLI_DOES_ADD 6u
#define CLI_DOES_SUBTRACT 7u
#define CLI_DOES_SHIFTLEFT 8u
#define CLI_DOES_SHIFTRIGHT 9u
#define CLI_DOES_LESS 10u
#define CLI_DOES_ATMOST 11u
#define CLI_DOES_GREATER 12u
#define CLI_DOES_ATLEAST 13u
#define CLI_DOES_EQUAL 14u
#define CLI_DOES_UNEQUAL 15u
#define CLI_DOES_AND 16u
#define CLI_DOES_OR 17u
#define CLI_DOES_BITAND 18u
#define CLI_DOES_BITXOR 19u
#define CLI_DOES_BITOR 20u
#define CLI_DOES_COUNT 20u

// One leaf in a text's cell of leaves: its bytes in the text, from and to, what it is, its line among the text's, how
// tightly it binds as an operator between two values, the number after a segment `binds`, 0 for none, the base its
// digits are written in, the number after a segment `base`, 10 where it names none, and the operation it does, a
// CLI_DOES_ one, 0 for none
typedef struct
{
    unsigned long long from;
    unsigned long long to;
    unsigned long long is;
    unsigned int line;
    unsigned char binds;
    unsigned char base;
    unsigned char does;
} CliLeaf;

// A text's leaves saved: the text, its leaves in a cell at their most, how many, the capacity, the lines read, whether
// the cells refused a leaf, and where each declaration a leaf names goes as it is saved, NULL for nowhere; and whether
// the leaves' first cell is taken on the lane the thread is on, for a read given back before the lane is rewound
typedef struct
{
    const char *text;
    CliLeaf *leaf;
    unsigned int count;
    unsigned int capacity;
    unsigned int lines;
    const char *first;
    int refused;
    CliDeclared declared;
    void *declared_context;
    int temporary;
    unsigned long long size;
} CliLeafRead;

// the leaves of `size` bytes of `text` read in the language of the file at `path`, the first line starting in
// `state`, saved in `read`: 1, or 0 where the language is none read here or the cells refused a leaf; the same read in
// the language `id`, the first line starting at the grammar's root; the same with each declaration a leaf names handed
// to `each` as it is saved; a saved leaf's text and its length; whether a leaf plays every role of `roles`, CLI_LEAF_
// bits its grammar names, and is neither a string nor a comment; and the leaves' cell given back (symbols_leaves.c)
int cli_symbols_leaves_save(const CliSymbolsPathCtx *args);
int cli_symbols_language_leaves_save(const CliSymbolsTextCtx *args);
int cli_symbols_leaves_declare(const CliSymbolsPathCtx *args);
// the leaves read as cli_symbols_leaves_save reads them, their cell on the lane the thread is on, for a read given back
// before the lane is rewound
int cli_symbols_leaves_temporary(const CliSymbolsPathCtx *args);
// the type a token's name declares: the rest of the name past its segment `declaration`, or NULL where it has none
// (symbols_read.c)
const char *cli_symbols_declares(const CliSymbolsNameCtx *args);
const char *cli_leaf_text(const CliSymbolsReadLeafCtx *args);
int cli_leaf_has(const CliSymbolsReadCtx *args);

// the most bytes of a type's text, of a head's label, the most parameters a head is read with, the most declarations
// of a name read, and the most lines a head is read across
#define CLI_SYMBOLS_TYPE 96u
#define CLI_SYMBOLS_LABEL 512u
#define CLI_SYMBOLS_PARAMS 32u
#define CLI_SYMBOLS_HEADS 16u
#define CLI_SYMBOLS_HEAD_LINES 16u

// A type read off leaves: its leaves' texts joined, a space between two names of types, and what its leaves are,
// CLI_LEAF_OF_NUMBER, CLI_LEAF_OF_STRING and CLI_LEAF_MEMORY among them; or a literal's, its text `number` or `string`
// and CLI_LEAF_NUMBER or CLI_LEAF_STRING; or none, its text empty
typedef struct
{
    char text[CLI_SYMBOLS_TYPE];
    unsigned long long is;
} CliSymbolType;

// One declaration's head: where it stands in the tree, its label from its first letter to the bracket that closes its
// parameters, each parameter's span in the label in UTF-16 units, its name's span there, empty where it has no name,
// and its type, how many, whether more are taken after them, and how the call's arguments fit it: the arguments that
// differ, how many, and whether their count fits
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    unsigned int line;
    char label[CLI_SYMBOLS_LABEL];
    unsigned int params;
    unsigned int span[CLI_SYMBOLS_PARAMS][2];
    unsigned int named[CLI_SYMBOLS_PARAMS][2];
    CliSymbolType type[CLI_SYMBOLS_PARAMS];
    int variadic;
    unsigned int differs[CLI_SYMBOLS_PARAMS];
    unsigned int differing;
    int counts;
} CliSymbolHead;

// the type of the declaration whose name is the leaf `leaf`: its type's leaves on its line between the brackets,
// commas, semicolons and `=` about it, else the literal after it and its `=`; whether an argument's type fits a
// parameter's, 1, 0 where it differs, or -1 where either has none; and the head of the declaration of `name`, `length`
// bytes, at
// `head`'s line of its file under `root` read into `head`: 1, or 0 where the file is not read or has no head there
// (symbols_heads.c)
void cli_symbols_type_get(const CliSymbolsReadLeafTypeCtx *args);
int cli_symbols_type_fits(const CliSymbolsArgumentCtx *args);
int cli_symbols_head_read(const CliSymbolsLanguagesCtx *args);

// the most conditions open at once and the most includes deep a walk goes, the most lines one directive runs across,
// the most declarations of the name sought read from one branch, the most macros deep a condition's value is read
// through, and the line of a define the compilation database gives
#define CLI_EFFECT_DEPTH 64u
#define CLI_EFFECT_DIRECTIVE_LINES 64u
#define CLI_EFFECT_NEEDLE 64u
#define CLI_EFFECT_THROUGH 16u
#define CLI_EFFECT_INPUT 0xffffffffu

// One leaf as the condition reader reads it: its text and length, and what it is, how tightly it binds, its digits'
// base and the operation it does, as its leaf was
typedef struct
{
    const char *text;
    unsigned long long length;
    unsigned long long is;
    unsigned char binds;
    unsigned char base;
    unsigned char does;
} CliEffectToken;

// One leaf of a macro's value, in a cell of the walk's: what it is, its text, a place among the walk's texts, and its
// length, how tightly it binds, its digits' base and the operation it does
typedef struct
{
    unsigned long long is;
    unsigned int text;
    unsigned int length;
    unsigned char binds;
    unsigned char base;
    unsigned char does;
} CliEffectValueLeaf;

// One macro met: its name and value's text, places among the walk's texts, and where it was met, its branch a place
// among the texts and its line, CLI_EFFECT_INPUT for a define the database gives; whether it was undefined there;
// whether it is called with arguments, and if so its parameters' names, each ended by a zero, a place among the texts,
// how many, and whether it takes the arguments past them; its value's leaves, a run among the walk's value leaves;
// and its directive among what a slice's walk met, CLI_JSON_NONE where the walk is no slice's or the database gives it
typedef struct
{
    unsigned int name;
    unsigned int value;
    unsigned int path;
    unsigned int line;
    int undefined;
    int called;
    unsigned int params;
    unsigned int param_count;
    int rest;
    unsigned int first;
    unsigned int count;
    unsigned int seen;
} CliEffectMacro;

// what a slice's walk meets in an active region: a define or undefine, a declaration a branch's leaves name, and an
// include of no file of the tree
#define CLI_SEEN_MACRO 0u
#define CLI_SEEN_DECLARED 1u
#define CLI_SEEN_INCLUDE 2u

// One branch a slice's walk read whole, its bytes and leaves in cells until the walk is given back: its path, a place
// among the walk's texts, its bytes and how many, whether they are the file's as the disk holds it, let go with the
// walk, and its leaves
typedef struct
{
    unsigned int path;
    const unsigned char *bytes;
    unsigned long long size;
    int from_disk;
    CliLeafRead read;
} CliEffectBranchRead;

// One thing a slice's walk met in an active region, in the order it met them: what it is, a CLI_SEEN_ one; the branch
// it stands in, a place among the branches read whole; its line; its first leaf, its line's first, and for a directive
// the leaf after its last; and for a declaration, the name it declares and its type, places among the walk's texts, and
// the name's length
typedef struct
{
    unsigned int is;
    unsigned int branch;
    unsigned int line;
    unsigned int first;
    unsigned int end;
    unsigned int name;
    unsigned int length;
    unsigned int type;
} CliEffectSeen;

typedef struct
{
    void *context;
    const CliLeafRead *read;
    unsigned int leaf;
    const char *path;
    unsigned int line;
} CliEffectNotedCfg;

// one leaf of the branch a walk starts from that names the name sought, in an active region, handed on as the walk
// passes its line: the branch's leaves and the leaf, and the definition of the name in effect there as a usage knows
// it, the define in effect, else the first declaration of it the walk met: its branch's path as the tree holds it and
// its line, CLI_EFFECT_INPUT for a define the database gives, the path NULL for none
typedef void (*CliEffectNoted)(const CliEffectNotedCfg *args);
// the most names a sifted walk sifts for, and the most lines a statement the sift found is read across
#define CLI_EFFECT_SIFT_NAMES 4096u
#define CLI_EFFECT_STATEMENT_LINES 4096u
typedef struct
{
    void *context;
    const CliLeafRead *read;
    unsigned int leaf;
    int active;
} CliEffectReachedCfg;

// a line of the branch a walk starts from reached, before the walk reads it: the branch's leaves, the line's first leaf
// and whether the line stands in an active region: 1 to read on, 0 to end the walk there
typedef int (*CliEffectReached)(const CliEffectReachedCfg *args);

// A walk from a branch's first line to a line of it: the languages, the tree's top, the branch it starts from and its
// text where the editor's stands in for the disk's, NULL where none does, and how many bytes, its texts a line, the
// macros met with the leaves of their values and the branches walked, each in a cell at their most, the name sought,
// the last declaration of it the walk reached in an active region, its branch and its line's text places among the
// texts, its line, and whether one was reached, and the first, its branch and its line, and whether one was; whether it
// is a slice's walk, and if so each branch it read whole and what it met, each in a cell at their most; where each leaf
// of the name in the branch it starts from goes, NULL for nowhere; where each line of that branch goes as the walk
// reaches it, NULL for nowhere; and whether the cells refused it
typedef struct
{
    CliSymbolsLanguages *languages;
    char root[CLI_PATH_MAX_LEN];
    const char *start;
    const char *start_text;
    unsigned long long start_size;
    VerbaLinea text;
    CliEffectMacro *macro;
    unsigned int macros;
    unsigned int macro_capacity;
    CliEffectValueLeaf *value_leaf;
    unsigned int value_leaves;
    unsigned int value_leaf_capacity;
    unsigned int *walked;
    unsigned int walks;
    unsigned int walk_capacity;
    const char *name;
    unsigned long long length;
    unsigned int found_path;
    unsigned int found_text;
    unsigned int found_line;
    int found;
    unsigned int first_path;
    unsigned int first_line;
    int first_found;
    int slicing;
    CliEffectBranchRead *branch_read;
    unsigned int branches_read;
    unsigned int branch_read_capacity;
    CliEffectSeen *seen;
    unsigned int seens;
    unsigned int seen_capacity;
    CliEffectNoted noted;
    void *noted_context;
    CliEffectReached reached;
    void *reached_context;
    int refused;
    int sifting;
    unsigned int view_from;
    unsigned int view_to;
    const char **sift_name;
    unsigned long long *sift_length;
    unsigned int sift_names;
    unsigned int sift_capacity;
} CliEffect;

// the text at `at` among the walk's texts; the macro of `name`, `length` bytes, the walk met last, its place, or
// CLI_JSON_NONE where it met none or met it undefined; and `length` bytes of `text` put after the walk's texts with a
// zero after them, their place, or CLI_JSON_NONE with the walk refused where the line is full (symbols_effect.c)
const char *cli_effect_text(const CliSymbolsCtx *args);
unsigned int cli_effect_macro(const CliSymbolsNameCtx *args);
unsigned int cli_effect_append(const CliSymbolsTextLengthCtx *args);
// the most entries a walk's list takes, and its values' and one branch's declarations'
#define CLI_EFFECT_LIST_MAX (1u << 15u)
#define CLI_EFFECT_VALUES_MAX (1u << 17u)
#define CLI_EFFECT_DECLARED_MAX 4096u
// a walk made for the tree's file at `path` and the name `name`, a slice's walk where `slicing` is 1: the walk, or NULL
// with why given where the cells cannot meet it; the walk taken from the file's first line to before line `line`; and
// the walk's cells given back (symbols_effect.c)
CliEffect *cli_effect_init(const CliSymbolsPathNameCtx *args);
void cli_effect_alloc(const CliSymbolsPathEffectCtx *args);
void cli_effect_input_back(const CliSymbolsTextLengthCtx *args);
// the integer the `count` leaves `token` of a condition give, read by the roles their grammar names: a number in the
// base its leaf names, a prefix passed over; an operator by how tightly it binds, one named `unary` or any before a
// value taken as one before it, and one named `choice` choosing between the value after it, to the one named
// `otherwise` as well, and the value after that one; `defined` asking whether a
// name is defined; and a name as the value of the macro it names, the leaves of the value its definition read, a macro
// called with arguments its value with each parameter's name made the leaves of the argument at its place, a leaf
// named `rest` the arguments past its parameters, one named `join` joining the leaves about it and one named `quote`
// making the leaf after it a string; CLI_EFFECT_THROUGH macros deep at most, and a name of no macro 0; 0 where the
// leaves give no integer. A bracket and a comma are known by their text (symbols_effect_value.c)
long long cli_effect_value(const CliSymbolsEffectCtx *args);

struct CliSymbolsPathCtx
{
    const char *path;
    const char *text;
    unsigned long long size;
    CliDeclared each;
    void *context;
    const char *state;
    CliSymbolsLanguages *languages;
    CliLeafRead *read;
};

struct CliSymbolsReadLanguagesTextCtx
{
    CliSymbolsLanguages *read_symbols_languages_pointer_other;
    const char *text;
    unsigned long long size;
    void *context;
    const char *state;
    CliLeavesEach each_leaves_each;
    const char *id;
};

struct CliSymbolsPathReadLanguagesTextCtx
{
    const char *path;
    CliSymbolsLanguages *read_symbols_languages_pointer_other;
    const char *text;
    unsigned long long size;
    void *context;
    const char *state;
    CliLeavesEach each_leaves_each;
};

struct CliSymbolsPathReadLanguagesCtx
{
    const char *path;
    CliSymbolsLanguages *read_symbols_languages_pointer_other;
    const char *text;
    unsigned long long size;
    CliDeclared each;
    void *context;
};

struct CliSymbolsSizeCtx
{
    unsigned long long size;
    const char *line;
    unsigned int col;
    unsigned long long *byte;
    unsigned int *at;
};

struct CliSymbolsLanguagesCtx
{
    CliSymbolsLanguages *languages;
    const char *name;
    const char *root;
    unsigned long long length;
    CliSymbolHead *head;
};

struct CliSymbolsTextCtx
{
    const char *text;
    unsigned long long size;
    const char *id;
    CliSymbolsLanguages *languages;
    CliLeafRead *read;
};

struct CliSymbolsPathNameCtx
{
    const char *path;
    const char *name;
    int slicing;
    VerbaLinea *err_msg;
};

struct CliSymbolsEffectCtx
{
    CliEffect *effect;
    const CliEffectToken *token;
    unsigned int count;
    unsigned int through;
};

struct CliSymbolsNameCtx
{
    const char *name;
    unsigned long long length;
    const CliEffect *effect_read;
};

struct CliSymbolsTextLengthCtx
{
    const char *text;
    unsigned long long length;
    CliEffect *effect;
};

struct CliSymbolsPathEffectCtx
{
    const char *path;
    CliEffect *effect;
    unsigned int line_u32;
};

struct CliSymbolsReadCtx
{
    const CliLeafRead *read_read;
    unsigned int leaf;
    unsigned long long roles;
};

struct CliSymbolsReadLeafCtx
{
    const CliLeafRead *read_read;
    unsigned int leaf;
    unsigned long long *length_pointer;
};

struct CliSymbolsReadLeafTypeCtx
{
    const CliLeafRead *read_read;
    unsigned int leaf;
    CliSymbolType *type;
};

struct CliSymbolsCtx
{
    const CliEffect *effect_read;
    unsigned int at_value;
};

struct CliSymbolsReadLanguagesCtx
{
    const CliSymbolsLanguages *read_symbols_languages_pointer;
    const char *path;
};

struct CliSymbolsArgumentCtx
{
    const CliSymbolType *argument;
    const CliSymbolType *parameter;
};

typedef struct
{
    int (*reads_in)(const CliSymbolsReadLanguagesCtx *args);
} CliSymbolsReadLanguagesNs;
EMBED_TABLE_LAYOUT(CliSymbolsReadLanguagesNs, reads_in);
EMBED_TABLE_STORAGE CliSymbolsReadLanguagesNs cli_symbols_read_languages_internal EMBED_UNUSED = {
    .reads_in = cli_symbols_reads_in,
};

typedef struct
{
    void (*declared_in)(const CliSymbolsPathReadLanguagesCtx *args);
} CliSymbolsPathReadLanguagesNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathReadLanguagesNs, declared_in);
EMBED_TABLE_STORAGE CliSymbolsPathReadLanguagesNs cli_symbols_path_read_languages_internal EMBED_UNUSED = {
    .declared_in = cli_symbols_declared_in,
};

typedef struct
{
    int (*leaves_in)(const CliSymbolsPathReadLanguagesTextCtx *args);
} CliSymbolsPathReadLanguagesTextNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathReadLanguagesTextNs, leaves_in);
EMBED_TABLE_STORAGE CliSymbolsPathReadLanguagesTextNs cli_symbols_path_read_languages_text_internal EMBED_UNUSED = {
    .leaves_in = cli_symbols_leaves_in,
};

typedef struct
{
    int (*language_leaves_in)(const CliSymbolsReadLanguagesTextCtx *args);
} CliSymbolsReadLanguagesTextNs;
EMBED_TABLE_LAYOUT(CliSymbolsReadLanguagesTextNs, language_leaves_in);
EMBED_TABLE_STORAGE CliSymbolsReadLanguagesTextNs cli_symbols_read_languages_text_internal EMBED_UNUSED = {
    .language_leaves_in = cli_symbols_language_leaves_in,
};

typedef struct
{
    unsigned long long (*byte_at)(const CliSymbolsSizeCtx *args);
} CliSymbolsSizeNs;
EMBED_TABLE_LAYOUT(CliSymbolsSizeNs, byte_at);
EMBED_TABLE_STORAGE CliSymbolsSizeNs cli_symbols_size_internal EMBED_UNUSED = {
    .byte_at = cli_symbols_byte_at,
};

typedef struct
{
    int (*leaves_save)(const CliSymbolsPathCtx *args);
    int (*leaves_declare)(const CliSymbolsPathCtx *args);
    int (*leaves_temporary)(const CliSymbolsPathCtx *args);
} CliSymbolsPathNsTable;
EMBED_TABLE_LAYOUT(CliSymbolsPathNsTable, leaves_save, leaves_declare, leaves_temporary);
EMBED_TABLE_STORAGE CliSymbolsPathNsTable cli_symbols_path_internal EMBED_UNUSED = {
    .leaves_save = cli_symbols_leaves_save,
    .leaves_declare = cli_symbols_leaves_declare,
    .leaves_temporary = cli_symbols_leaves_temporary,
};

typedef struct
{
    int (*language_leaves_save)(const CliSymbolsTextCtx *args);
} CliSymbolsTextNs;
EMBED_TABLE_LAYOUT(CliSymbolsTextNs, language_leaves_save);
EMBED_TABLE_STORAGE CliSymbolsTextNs cli_symbols_text_internal EMBED_UNUSED = {
    .language_leaves_save = cli_symbols_language_leaves_save,
};

typedef struct
{
    const char *(*declares)(const CliSymbolsNameCtx *args);
    unsigned int (*effect_macro)(const CliSymbolsNameCtx *args);
} CliSymbolsNameNs;
EMBED_TABLE_LAYOUT(CliSymbolsNameNs, declares, effect_macro);
EMBED_TABLE_STORAGE CliSymbolsNameNs cli_symbols_name_internal EMBED_UNUSED = {
    .declares = cli_symbols_declares,
    .effect_macro = cli_effect_macro,
};

typedef struct
{
    const char *(*leaf_text)(const CliSymbolsReadLeafCtx *args);
} CliSymbolsReadLeafNs;
EMBED_TABLE_LAYOUT(CliSymbolsReadLeafNs, leaf_text);
EMBED_TABLE_STORAGE CliSymbolsReadLeafNs cli_symbols_read_leaf_internal EMBED_UNUSED = {
    .leaf_text = cli_leaf_text,
};

typedef struct
{
    int (*leaf_has)(const CliSymbolsReadCtx *args);
} CliSymbolsReadNsTable;
EMBED_TABLE_LAYOUT(CliSymbolsReadNsTable, leaf_has);
EMBED_TABLE_STORAGE CliSymbolsReadNsTable cli_symbols_read_internal EMBED_UNUSED = {
    .leaf_has = cli_leaf_has,
};

typedef struct
{
    void (*type_get)(const CliSymbolsReadLeafTypeCtx *args);
} CliSymbolsReadLeafTypeNs;
EMBED_TABLE_LAYOUT(CliSymbolsReadLeafTypeNs, type_get);
EMBED_TABLE_STORAGE CliSymbolsReadLeafTypeNs cli_symbols_read_leaf_type_internal EMBED_UNUSED = {
    .type_get = cli_symbols_type_get,
};

typedef struct
{
    int (*type_fits)(const CliSymbolsArgumentCtx *args);
} CliSymbolsArgumentNs;
EMBED_TABLE_LAYOUT(CliSymbolsArgumentNs, type_fits);
EMBED_TABLE_STORAGE CliSymbolsArgumentNs cli_symbols_argument_internal EMBED_UNUSED = {
    .type_fits = cli_symbols_type_fits,
};

typedef struct
{
    int (*head_read)(const CliSymbolsLanguagesCtx *args);
} CliSymbolsLanguagesNs;
EMBED_TABLE_LAYOUT(CliSymbolsLanguagesNs, head_read);
EMBED_TABLE_STORAGE CliSymbolsLanguagesNs cli_symbols_languages_internal EMBED_UNUSED = {
    .head_read = cli_symbols_head_read,
};

typedef struct
{
    const char *(*effect_text)(const CliSymbolsCtx *args);
} CliSymbolsNsTable;
EMBED_TABLE_LAYOUT(CliSymbolsNsTable, effect_text);
EMBED_TABLE_STORAGE CliSymbolsNsTable cli_symbols_internal EMBED_UNUSED = {
    .effect_text = cli_effect_text,
};

typedef struct
{
    unsigned int (*effect_append)(const CliSymbolsTextLengthCtx *args);
    void (*effect_input_back)(const CliSymbolsTextLengthCtx *args);
} CliSymbolsTextLengthNs;
EMBED_TABLE_LAYOUT(CliSymbolsTextLengthNs, effect_append, effect_input_back);
EMBED_TABLE_STORAGE CliSymbolsTextLengthNs cli_symbols_text_length_internal EMBED_UNUSED = {
    .effect_append = cli_effect_append,
    .effect_input_back = cli_effect_input_back,
};

typedef struct
{
    CliEffect *(*effect_init)(const CliSymbolsPathNameCtx *args);
} CliSymbolsPathNameNsTable;
EMBED_TABLE_LAYOUT(CliSymbolsPathNameNsTable, effect_init);
EMBED_TABLE_STORAGE CliSymbolsPathNameNsTable cli_symbols_path_name_internal EMBED_UNUSED = {
    .effect_init = cli_effect_init,
};

typedef struct
{
    void (*effect_alloc)(const CliSymbolsPathEffectCtx *args);
} CliSymbolsPathEffectNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathEffectNs, effect_alloc);
EMBED_TABLE_STORAGE CliSymbolsPathEffectNs cli_symbols_path_effect_internal EMBED_UNUSED = {
    .effect_alloc = cli_effect_alloc,
};

typedef struct
{
    long long (*effect_value)(const CliSymbolsEffectCtx *args);
} CliSymbolsEffectNs;
EMBED_TABLE_LAYOUT(CliSymbolsEffectNs, effect_value);
EMBED_TABLE_STORAGE CliSymbolsEffectNs cli_symbols_effect_internal EMBED_UNUSED = {
    .effect_value = cli_effect_value,
};

#endif
