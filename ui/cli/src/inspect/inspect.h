// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect.h: inspections: checks of orior's own over the tree's Python and JavaScript, each finding given to the
// editor as a diagnostic beside the language servers', with the edits of its quick fix where one applies.
//
// What a file says alone, the Python and the JavaScript readers find: a name set and never read, code no path reaches,
// and a Python docstring out of PEP 257's form. What the tree says, the tree's index finds, from each file's imports
// and classes:
//
// - an import that leads back round to the file that makes it, through the imports the files it reaches make as they
//   run, named with the files it passes through;
// - an attribute a method reads of `self` that neither its class, a class it comes from, nor a class that comes from it
//   sets, as a mixin's methods are found in the classes it is mixed into and a class's in the mixins it takes in. A
//   class whose family holds a class from outside the tree, or that sets its attributes by name, is left alone. What
//   each class's family sets is kept, and a language server's report that a class has no such attribute, as pyright
//   gives of a mixin, is not passed on where the family sets it.
//
// The index also holds each function a file declares and the calls in it, which the call hierarchy reads where no
// language server answers: a call of a name reaches the functions of that name in the caller's file, or, where its
// file declares none, those of the tree in its language.
//
// The same reading parses a file whole for the editor: each column's class, a name's by what it is, a function's, a
// class's, a parameter's, an attribute's or a module's; the regions that fold; and the spans Expand Selection steps
// through, its strings, its brackets, its statements and its blocks.
//
// A Python import is found from the importing file's folder, from the folder above its outermost package, and from the
// tree's top folder, in that order; a relative one from its package. A JavaScript import is a relative path, with
// `.js`, `.mjs` or `/index.js` after it where the path names no file (inspect.c, inspect_*.c)
#ifndef INSPECT_H
#define INSPECT_H

#include "../command_line/cli_internal.h"

// the classes a parse gives a column, by their index: none, then each the page's own, as its colors name them
#define CLI_INSPECT_NONE 0u
#define CLI_INSPECT_KEYWORD 1u
#define CLI_INSPECT_STRING 2u
#define CLI_INSPECT_NUMBER 3u
#define CLI_INSPECT_COMMENT 4u
#define CLI_INSPECT_OPERATOR 5u
#define CLI_INSPECT_DELIMITER 6u
#define CLI_INSPECT_FUNCTION 7u
#define CLI_INSPECT_TYPE 8u
#define CLI_INSPECT_PARAMETER 9u
#define CLI_INSPECT_PROPERTY 10u
#define CLI_INSPECT_MODULE 11u
#define CLI_INSPECT_PREDEFINED 12u
#define CLI_INSPECT_ATTRIBUTE 13u
#define CLI_INSPECT_CLASSES 14u

// how a finding is shown
#define CLI_INSPECT_WARNING 0
#define CLI_INSPECT_NOTE 1

// A place in a text: its line and its column in UTF-16 units, as the editor and the protocol count columns, each from 0
typedef struct
{
    unsigned int line;
    unsigned int col;
} CliInspectPlace;

// A text kept in a pool: where it starts and its length, a 0 after it
typedef struct
{
    unsigned int at;
    unsigned int size;
} CliInspectText;

// An edit: the span it takes out and the text it writes there
typedef struct
{
    CliInspectPlace from;
    CliInspectPlace to;
    CliInspectText text;
} CliInspectEdit;

// A quick fix: what it says it does, and its edits, `edits` of them from `edit_first`
typedef struct
{
    CliInspectText title;
    unsigned int edit_first;
    unsigned int edits;
} CliInspectFix;

// What an inspection found: its span, how it is shown, what it says in Markdown, its name, the page that tells of what
// it checks, NULL where none does, and its fixes
typedef struct
{
    CliInspectPlace from;
    CliInspectPlace to;
    int severity;
    CliInspectText message;
    const char *code;
    const char *href;
    unsigned int fix_first;
    unsigned int fixes;
} CliInspectFinding;

// A name and its span, where it has one: a name a Python `from` import takes with its alias, a base a class names, an
// attribute its body and methods set, an attribute its methods read of `self`, and a call a function makes
typedef struct
{
    CliInspectText name;
    CliInspectText alias;
    int has_alias;
    CliInspectPlace from;
    CliInspectPlace to;
} CliInspectName;

// An import a file makes as it runs: the module or the path it names, the names a Python `from` import takes, the
// alias of a Python `import ... as`, and its span
typedef struct
{
    CliInspectText spec;
    unsigned int name_first;
    unsigned int names;
    CliInspectText alias;
    int has_alias;
    CliInspectPlace from;
    CliInspectPlace to;
} CliInspectImport;

// A Python class declared at a file's top level: its name and its span, the bases it names, what its body and its
// methods set, each attribute its methods read of `self` with its span, and whether it sets attributes by name
typedef struct
{
    CliInspectText name;
    CliInspectPlace from;
    CliInspectPlace to;
    unsigned int base_first;
    unsigned int bases;
    unsigned int has_first;
    unsigned int has;
    unsigned int read_first;
    unsigned int reads;
    int dynamic;
} CliInspectClass;

// A function a file declares: its name and the span of its name, its first and last lines, and each call in it, of a
// bare name or of a method of `self` or `this`, outside the functions it holds, with the call's span
typedef struct
{
    CliInspectText name;
    CliInspectPlace from;
    CliInspectPlace to;
    unsigned int first;
    unsigned int last;
    unsigned int call_first;
    unsigned int calls;
} CliInspectFunction;

// What a file holds for the inspections, in the storage its holder hands it: its imports, its classes, its functions,
// and what it says alone, the findings, their fixes and edits, the names the others list, and the texts of all; 1 in
// `full` where one did not fit
typedef struct
{
    CliInspectImport *import;
    unsigned int imports;
    unsigned int imports_most;
    CliInspectClass *class_of;
    unsigned int classes;
    unsigned int classes_most;
    CliInspectFunction *function;
    unsigned int functions;
    unsigned int functions_most;
    CliInspectFinding *finding;
    unsigned int findings;
    unsigned int findings_most;
    CliInspectFix *fix;
    unsigned int fixes;
    unsigned int fixes_most;
    CliInspectEdit *edit;
    unsigned int edits;
    unsigned int edits_most;
    CliInspectName *name;
    unsigned int names;
    unsigned int names_most;
    char *text;
    unsigned int used;
    unsigned int text_most;
    int full;
    // counting: each put counts what it would keep and keeps nothing, so a walk sizes the room the next walk fills
    int counting;
} CliFacts;

// A span: from where to where
typedef struct
{
    CliInspectPlace from;
    CliInspectPlace to;
} CliInspectSpan;

// A run of one class on a line: its first column, the column past its last, and its class
typedef struct
{
    unsigned int from;
    unsigned int to;
    unsigned char class_of;
} CliInspectRun;

// A file as its parse reads it, in the storage its holder hands it: each line's runs, `count` of them from `first`,
// every column of the line in one; the regions that fold, by their first and last lines; and the spans Expand
// Selection steps through; 1 in `full` where one did not fit. A parse handed no storage counts the runs, lines, folds
// and spans it would hold, for its holder to take that storage and parse again
typedef struct
{
    CliInspectRun *run;
    unsigned int runs;
    unsigned int runs_most;
    unsigned int *line_first;
    unsigned int *line_runs;
    unsigned int lines;
    unsigned int lines_most;
    unsigned int *fold_first;
    unsigned int *fold_last;
    unsigned int folds;
    unsigned int folds_most;
    CliInspectSpan *range;
    unsigned int ranges;
    unsigned int ranges_most;
    int full;
} CliParsed;

// the class names a parse gives columns, by index, as the page's colors name them
extern const char *const g_cli_inspect_classes[CLI_INSPECT_CLASSES];
// the forms a docstring is drawn up in: Google's, NumPy's, reStructuredText's fields, and a summary line alone
extern const char *const g_cli_inspect_docstring_forms[4];

// the language of a path by its extension, where the inspections read it: "python" or "javascript", NULL where none;
// and a text of a pool
const char *cli_inspect_language_of(const char *path);
const char *cli_inspect_text(const char *pool, CliInspectText text);

// facts made empty over the storage handed them; facts that count what a walk would keep and keep nothing, so the
// walk sizes the room the next one fills; and the facts of `size` bytes of a file's text in `language`, where the
// inspections read the language: 1, or 0 where they do not
void cli_facts_counting(CliFacts *facts);
void cli_facts_begun(CliFacts *facts, CliInspectImport *imports, unsigned int imports_most, CliInspectClass *classes,
                     unsigned int classes_most, CliInspectFunction *functions, unsigned int functions_most,
                     CliInspectFinding *findings, unsigned int findings_most, CliInspectFix *fixes,
                     unsigned int fixes_most, CliInspectEdit *edits, unsigned int edits_most, CliInspectName *names,
                     unsigned int names_most, char *text, unsigned int text_most);
int cli_inspect_facts(const char *language, const char *text, unsigned long long size, CliFacts *facts);

// a finding of `facts` written as a diagnostic of the protocol's form, its message Markdown and its fixes kept in its
// data: `{"range", "severity", "message": {"kind", "value"}, "source": "orior", "code", "data": {"orior": fixes},
// "codeDescription"}`
void cli_inspect_finding_written(const CliFacts *facts, unsigned int finding, CliLine *out);

// a parse made empty over the storage handed it; and the parse of `size` bytes of a file's text in `language`: 1, or 0
// where the inspections do not read the language
void cli_parsed_begun(CliParsed *parsed, CliInspectRun *runs, unsigned int runs_most, unsigned int *line_first,
                      unsigned int *line_runs, unsigned int lines_most, unsigned int *fold_first,
                      unsigned int *fold_last, unsigned int folds_most, CliInspectSpan *ranges,
                      unsigned int ranges_most);
int cli_inspect_parse(const char *language, const char *text, unsigned long long size, CliParsed *parsed);
// the spans of `ranges`, `count` of them, that hold `from` to `to` and are more than it, the least first, each once,
// into `out`, which holds `most`: their count
unsigned int cli_inspect_ranges_at(const CliInspectSpan *ranges, unsigned int count, CliInspectPlace from,
                                   CliInspectPlace to, CliInspectSpan *out, unsigned int most);
// folds by their first line, each the widest that starts there, one line or more below it, sorted: `count` pairs of
// `first` and `last` taken in place, their count after
unsigned int cli_inspect_folds_of(unsigned int *first, unsigned int *last, unsigned int count);

// a docstring drawn up for the function at or around `line` of `size` bytes of a file's text in `language`, in
// `form`: the edit that writes it, its text into `text`, and the place to write its summary: 1, or 0 with why said;
// and the methods of the class at or around `line` sorted by name, each with the comments and decorators over it, the
// class's other members and its blank lines staying where they stand: the edit that writes the class's body again, its
// text into `text`: 1, or 0 with why said
int cli_inspect_docstring(const char *language, const char *source, unsigned long long size, unsigned int line,
                          const char *form, CliInspectEdit *edit, CliLine *text, CliInspectPlace *summary,
                          CliLine *said);
int cli_inspect_sort_methods(const char *language, const char *source, unsigned long long size, unsigned int line,
                             CliInspectEdit *edit, CliLine *text, CliLine *said);

#endif
