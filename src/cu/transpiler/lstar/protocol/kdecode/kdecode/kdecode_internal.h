// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kdecode_internal.h: what the modules of orior_kdecode share: its includes, the bounds of a line, a word, the names
// and the run's records read, the text it passes the run, the record and file read, the code, column, oracle, blob,
// text, file, row and run structures, and the functions one module reads from another
#ifndef KDECODE_INTERNAL_H
#define KDECODE_INTERNAL_H

#include "../../../../../../../includes/file_defs/kqr/kqr_record.h"
#include "../../../../../../../includes/file_defs/krep/krep_head.h"
#include "../../../../../../../includes/file_defs/krep/krep_records.h"
#include "../../../../../../../includes/file_defs/qry/qry_read.h"
#include "../../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../table_text.h"
#include "verba_scribo/verba_linea.h"
#include <stdio.h>

#include <ctype.h>

// the longest line of an oracle, a word of one of its fields, and a who named
#define KDECODE_LINE 8192u

#define KDECODE_WORD 64u

#define KDECODE_NAMED 2400u

// the max records of a run that a walk keeps, for blobs, texts and rows each. Then the max k-files of a table, oracles
// that a run holds, and codes that an oracle gives
#define KDECODE_RECORDS_MAX (1u << 21u)

#define KDECODE_FILES_MAX 4096u

#define KDECODE_CONTENTS_MAX 1024u

#define KDECODE_CODES_MAX 16384u

// the record that the decoder passes its text to the run in, gathered in s_displayed. The decoder passes it whenever it
// holds past half its room, and when the decoder ends (kdecode_displayed_passed). It passes a failure at once
// (kdecode_failure_passed). A decoder that is a member of no run writes its text to the standard output, and its
// failures to the standard error
#define KDECODE_DISPLAYED "orior_kdecode"

// A code an oracle gives: the family its `where` names, its name and its value
typedef struct
{
    char family[KDECODE_WORD];
    char name[KDECODE_WORD];
    unsigned long long value;
} KdecodeCode;

// A column of the row of a table as its oracle gives it: its name, its form, and the oracle that its gloss names. The
// codes of that oracle are for it where its own oracle gives none
typedef struct
{
    char name[KDECODE_WORD];
    char form[KDECODE_WORD];
    char codes_from[KDECODE_WORD];
} KdecodeColumn;

// An oracle read: the type of its row, its columns in their order, and every code it gives
typedef struct
{
    char type[KDECODE_WORD];
    KdecodeColumn column[TABLE_ROW_COLUMNS];
    unsigned int columns;
    KdecodeCode code[KDECODE_CODES_MAX];
    unsigned int codes;
} KdecodeOracle;

// A blob of the run, its sequence and its name where it is in the mapped run
typedef struct
{
    unsigned long long sequence;
    const char *name;
    unsigned int name_length;
} KdecodeBlob;

// A text of the run or of a k-file of it. It holds the key of the file, 0 for the table of texts of the run, and its
// number. Then its bytes where they are in the mapped run, and their count
typedef struct
{
    unsigned long long file;
    unsigned long long number;
    const unsigned char *bytes;
    unsigned long long length;
} KdecodeText;

// A k-file of a table, its key and the four bytes of its type
typedef struct
{
    unsigned long long key;
    char type[5];
} KdecodeFile;

// A row of the table read, its key and its words where they are in the mapped run and their count
typedef struct
{
    unsigned long long key;
    const unsigned char *words_at;
    size_t words;
} KdecodeRow;

// An oracle that the run holds, the latest blob of its name. It holds its name and its bytes where they are in the
// mapped run, and their counts
typedef struct
{
    const char *name;
    unsigned int name_length;
    const unsigned char *bytes;
    unsigned long long size;
} KdecodeContent;

// What one walk of the blobs of a run keeps, each where it is in the mapped run:
//
// - the table requested, its range of keys and the max rows
// - the sequence and name of every blob, the texts of the run, and the texts and heads of the k-files of the table
// - the rows of the range
// - the oracles that the run holds
//
// and 1 in `failed` where the run holds more than a walk keeps
typedef struct
{
    const char *table;
    unsigned long long from;
    unsigned long long to;
    unsigned long long max;
    KdecodeBlob *blob;
    size_t blobs;
    KdecodeText *text;
    size_t texts;
    KdecodeFile *file;
    size_t files;
    KdecodeRow *row;
    size_t rows;
    KdecodeContent *contents;
    size_t content_count;
    int failed;
} KdecodeRun;

extern KqrRecord s_record;
extern KrepFile s_file;
extern VerbaLinea s_displayed;

void kdecode_displayed_passed(int whole);
void kdecode_failure_passed(void);
void kdecode_decimal_displayed(unsigned long long number);
void kdecode_line_ended(void);
int kdecode_records_each(const unsigned char *bytes, unsigned long long size,
                         void (*each)(void *context, const char *name, unsigned int name_length,
                                      unsigned long long time, const unsigned char *blob, unsigned long long blob_size),
                         void *context);
void kdecode_records(const KrepFile *file, int raw, unsigned int from, unsigned int count);
int kdecode_number(const char *word, unsigned long long *value);
void kdecode_oracle_free(KdecodeOracle *oracle);
const char *kdecode_code_named(const KdecodeOracle *oracle, const char *family, unsigned long long value);
int kdecode_family_given(const KdecodeOracle *oracle, const char *family);
void kdecode_blob(void *context, const char *name, unsigned int name_length, unsigned long long sequence,
                  const unsigned char *bytes, unsigned long long size);
int kdecode_oracle_read(const KdecodeRun *run, const char *path, KdecodeOracle *oracle, int codes_only);
int kdecode_table(const char *path, const char *table, int raw, unsigned long long from, unsigned long long to,
                  unsigned long long max);
// every blob of the run at `path` named `name`, each written whole after a line of its sequence, its name and its size.
// A NUL byte shows as a space. The latest alone where `latest` is 1. The return is 1, or 0 where the read of the run
// failed
int kdecode_blobs(const char *path, const char *name, int latest);
// whether the `size` bytes at `bytes` are the records of a run whole, as a .qry stores them with no head of its own.
// The return is 1, or 0 where they are none, or not records to their last byte
int kdecode_run_whole(const unsigned char *bytes, unsigned long long size);

#endif
