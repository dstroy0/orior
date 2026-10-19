// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kdecode_internal.h: what the modules of kdecode share: its includes, the bounds of a line, a word, the names and the
// run's records read, the text it hands the run, the record and file read, the code, column, oracle, blob, text, file,
// row and run structures, and the functions one module reads from another
#ifndef KDECODE_INTERNAL_H
#define KDECODE_INTERNAL_H

#include "../../../../../includes/file_defs/kqr/kqr_record.h"
#include "../../../../../includes/file_defs/krep/krep_head.h"
#include "../../../../../includes/file_defs/krep/krep_records.h"
#include "../../../../../includes/file_defs/qry/qry_read.h"
#include "../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../table_text.h"
#include "../../../../../engine/runtime/scriptura/scriptura.h"

#include <ctype.h>
#include <string.h>

// the longest line of an oracle, a word of one of its fields, and a who named
#define KDECODE_LINE 8192u

#define KDECODE_WORD 64u

#define KDECODE_NAMED 2400u

// the most records of a run a walk keeps, blobs, texts and rows each, and the most k-files of a table, oracles a run
// holds and codes an oracle gives
#define KDECODE_RECORDS_MOST (1u << 21u)

#define KDECODE_FILES_MOST 4096u

#define KDECODE_HELDS_MOST 1024u

#define KDECODE_CODES_MOST 16384u

// the record the decoder hands the run its text in, gathered in s_shown and handed whenever it holds past half its
// room and once the decoder is done (kdecode_shown_handed)
#define KDECODE_SHOWN "kdecode"

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
    KdecodeColumn column[TABLE_ROW_COLUMNS];
    unsigned int columns;
    KdecodeCode code[KDECODE_CODES_MOST];
    unsigned int codes;
} KdecodeOracle;

// A blob of the run, its sequence and its name where it stands in the mapped run
typedef struct
{
    unsigned long long sequence;
    const char *name;
    unsigned int name_length;
} KdecodeBlob;

// A text of the run or of a k-file of it: the file's key, 0 for the run's table of texts, its number, and its bytes
// where they stand in the mapped run and their count
typedef struct
{
    unsigned long long file;
    unsigned long long number;
    const unsigned char *bytes;
    unsigned long long length;
} KdecodeText;

// A k-file of a table, its key and the four bytes of its kind
typedef struct
{
    unsigned long long key;
    char kind[5];
} KdecodeFile;

// A row of the table read, its key and its words where they stand in the mapped run and their count
typedef struct
{
    unsigned long long key;
    const unsigned char *words_at;
    size_t words;
} KdecodeRow;

// An oracle the run holds, the latest blob of its name: its name and its bytes where they stand in the mapped run,
// and their counts
typedef struct
{
    const char *name;
    unsigned int name_length;
    const unsigned char *bytes;
    unsigned long long size;
} KdecodeHeld;

// What one walk of a run's blobs keeps: the table asked, its range of keys and the most rows; every blob's sequence
// and name, the run's texts and the texts and heads of the table's k-files; the rows of the range; the oracles the run
// holds; each where it stands in the mapped run, and 1 in `failed` where the run held more than a walk keeps
typedef struct
{
    const char *table;
    unsigned long long from;
    unsigned long long to;
    unsigned long long most;
    KdecodeBlob *blob;
    size_t blobs;
    KdecodeText *text;
    size_t texts;
    KdecodeFile *file;
    size_t files;
    KdecodeRow *row;
    size_t rows;
    KdecodeHeld *held;
    size_t helds;
    int failed;
} KdecodeRun;

extern KqrRecord s_record;
extern KrepFile s_file;
extern ScripturaLine s_shown;

void kdecode_shown_handed(int whole);
void kdecode_records(const KrepFile *file, int raw, unsigned int from, unsigned int count);
int kdecode_number(const char *word, unsigned long long *value);
void kdecode_oracle_free(KdecodeOracle *oracle);
const char *kdecode_code_named(const KdecodeOracle *oracle, const char *family, unsigned long long value);
int kdecode_family_given(const KdecodeOracle *oracle, const char *family);
void kdecode_blob(void *context, const char *name, unsigned int name_length, unsigned long long sequence,
                  const unsigned char *bytes, unsigned long long size);
int kdecode_oracle_read(const KdecodeRun *run, const char *path, KdecodeOracle *oracle, int codes_only);
int kdecode_table(const char *path, const char *table, int raw, unsigned long long from, unsigned long long to,
                  unsigned long long most);
// every blob of the run at `path` named `name`, each written whole after a line of its sequence, its name and its
// size, a NUL byte shown as a space; the latest alone where `latest` is 1. 1, or 0 where the run could not be read
int kdecode_blobs(const char *path, const char *name, int latest);

#endif
