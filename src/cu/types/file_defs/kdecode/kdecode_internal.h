// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kdecode_internal.h: what the modules of kdecode share: its includes, the bounds of a line, a word and the names, the record and file read, the code, column, oracle, blob, text, file, row and run structures, and the functions one module reads from another
#ifndef KDECODE_INTERNAL_H
#define KDECODE_INTERNAL_H

#include "../kqr/kqr.h"
#include "../krep_head.h"
#include "../krep_records.h"
#include "../qry/qry.h"
#include "../qry/qry_buffer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

// the longest line of an oracle, a word of one of its fields, and a who named
#define KDECODE_LINE 8192u

#define KDECODE_WORD 64u

#define KDECODE_NAMED 2400u

// A code an oracle gives: the family its `where` names, its name and its value
typedef struct
{
    char family[KDECODE_WORD];
    char name[KDECODE_WORD];
    unsigned long long value;
} KdecodeCode;

// A column of a table's row as its oracle gives it: its name, its form, and the oracle its gloss names, whose codes
// stand for it where its own oracle gives none
typedef struct
{
    char name[KDECODE_WORD];
    char form[KDECODE_WORD];
    char codes_from[KDECODE_WORD];
} KdecodeColumn;

// An oracle read: the kind of its row, its columns in their order, and every code it gives
typedef struct
{
    char kind[KDECODE_WORD];
    KdecodeColumn column[QRY_ROW_COLUMNS];
    unsigned int columns;
    KdecodeCode *code;
    unsigned int codes;
    unsigned int code_room;
} KdecodeOracle;

// A blob of the run, its sequence and where its name stands in the run's bytes
typedef struct
{
    unsigned long long sequence;
    size_t name_at;
} KdecodeBlob;

// A text of the run or of a k-file of it: the file's key, 0 for the run's table of texts, its number, and where its
// bytes stand in the run's bytes and their count
typedef struct
{
    unsigned long long file;
    unsigned long long number;
    size_t bytes_at;
    unsigned long long length;
} KdecodeText;

// A k-file of a table, its key and the four bytes of its kind
typedef struct
{
    unsigned long long key;
    char kind[5];
} KdecodeFile;

// A row of the table read, its key and where its words stand in the run's bytes and their count
typedef struct
{
    unsigned long long key;
    size_t words_at;
    size_t words;
} KdecodeRow;

// An oracle the run holds, the latest blob of its name: where its name and its bytes stand in the run's bytes, and
// their count
typedef struct
{
    size_t name_at;
    size_t bytes_at;
    unsigned long long size;
} KdecodeHeld;

// What one walk of a run's blobs keeps: the table asked, its range of keys and the most rows; every blob's sequence
// and name, the run's texts and the texts and heads of the table's k-files; the rows of the range; the oracles the run
// holds; the bytes they name, and 1 in `failed` where the system gave no memory for some of it
typedef struct
{
    const char *table;
    unsigned long long from;
    unsigned long long to;
    unsigned long long most;
    KdecodeBlob *blob;
    size_t blobs;
    size_t blob_room;
    KdecodeText *text;
    size_t texts;
    size_t text_room;
    KdecodeFile *file;
    size_t files;
    size_t file_room;
    KdecodeRow *row;
    size_t rows;
    size_t row_room;
    KdecodeHeld *held;
    size_t helds;
    size_t held_room;
    unsigned char *bytes;
    size_t size;
    size_t room;
    int failed;
} KdecodeRun;

extern KqrRecord s_record;
extern KrepFile s_file;

void kdecode_records(const KrepFile *file, int raw, unsigned int from, unsigned int count);
int kdecode_number(const char *word, unsigned long long *value);
void kdecode_oracle_free(KdecodeOracle *oracle);
const char *kdecode_code_named(const KdecodeOracle *oracle, const char *family, unsigned long long value);
int kdecode_family_given(const KdecodeOracle *oracle, const char *family);
void kdecode_blob(void *context, const char *name, unsigned long long sequence, const unsigned char *bytes, unsigned long long size);
int kdecode_oracle_read(const KdecodeRun *run, const char *path, KdecodeOracle *oracle, int codes_only);
int kdecode_table(const char *path, const char *table, int raw, unsigned long long from, unsigned long long to, unsigned long long most);

#endif
