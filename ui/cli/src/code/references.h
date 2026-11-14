// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// references.h: the definition, the references and the rename of the name at a place in the editor, each answered from
// the tree the CLI has read, a file's text the editor's where it holds one. The table of references is one, the view's
// thread's alone, and holds the last references found until the next are or it ends (references.c)
#ifndef CLI_REFERENCES_H
#define CLI_REFERENCES_H
#include "../command_line/cli_internal.h"

// the max bytes of a reference row's text with its end, and the rows: a file, and a reference under it
#define CLI_REFERENCES_ROW_TEXT 1024u
#define CLI_REFERENCES_ROW_FILE 0u
#define CLI_REFERENCES_ROW_PLACE 1u
// A row of the table: its type; the file's path from the top of the tree; a file row's name with its bytes and the
// bytes of its folder before the name, its count of references and whether they show; a reference row's line from 0,
// its span `col` to `to` in UTF-16 units, its line's text with the text's bytes, and the name's place and bytes in the
// text; the text and the path copied into the row's own bytes
typedef struct
{
    unsigned int type;
    const char *path;
    const char *name;
    unsigned long long name_length;
    unsigned long long folder_length;
    unsigned int count;
    int open;
    unsigned long long line;
    unsigned long long col;
    unsigned long long to;
    const char *text;
    unsigned long long text_length;
    unsigned long long match_at;
    unsigned long long match_length;
    char text_copied[CLI_REFERENCES_ROW_TEXT];
    char path_copied[CLI_PATH_MAX_LEN];
} CliReferenceRow;
typedef struct CliReferencesPlaceCfg CliReferencesPlaceCfg;
// the declaration the hover shows first for the word `name` at `line` of the view `key` on the tree `root`'s file at
// `path`, a parameter or a local of the function the line is in, else the tree's declaration of the word in the file's
// language (servers_panels.c): 1 with its file's path into `path_out`, sized for CLI_PATH_MAX_LEN, and its place into
// `line_out` and `col_out`, or 0 where none is
int cli_references_definition(const CliReferencesPlaceCfg *args);
// every reference to the name at `line`, `col` in the tree's files found into the table: 1, 0 where the tree knows no
// name there, or -1 with why given; and the name renamed to `name` at every reference, each file's view edited, a file
// no view holds opened as one in the tree `root` at `rows` and `columns` first, the references of a file written from
// its last to its first: how many were written, with the files into `files_out`, 0 where the tree knows no name there
// or `name` is no name, or -1 with why given
int cli_references_find(const CliReferencesPlaceCfg *args);
int cli_references_rename(const CliReferencesPlaceCfg *args);
// the table: how many rows; the row `at` into `row`: 1, or 0 past them; a press on the row `at`, a file's rows closed
// or opened, or a reference's row into `row`: 1 where it was a reference's, its file to be opened at its place; the
// report, the references and files counted with the name, or that none were found, with its bytes into `length_out`;
// and the table released
unsigned int cli_references_rows(void);
int cli_references_row(unsigned int at, CliReferenceRow *row);
int cli_references_press(unsigned int at, CliReferenceRow *row);
const char *cli_references_report(unsigned long long *length_out);
void cli_references_end(void);
struct CliReferencesPlaceCfg
{
    const char *path;
    const char *key;
    unsigned int line;
    unsigned int col;
    const char *name;
    const char *root;
    unsigned int rows;
    unsigned int columns;
    char *path_out;
    unsigned long long *line_out;
    unsigned long long *col_out;
    unsigned int *files_out;
    VerbaLinea *err_msg;
};

#endif
