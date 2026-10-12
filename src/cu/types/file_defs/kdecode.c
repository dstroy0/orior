// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kdecode.c: a k-file of records (krep_records.h) written for a person to read, the way a stack trace is read
// through its symbols: each record in the text its format gives it, each who named through the who table
// (qry_buffer.h, QryWho), and a kind no format here reads given as its records' codes and numbers and its texts. A
// table of a run's .qry is read the same way, the select over its rows: the rows of the table, a range of their keys,
// raw or decoded through the table's oracle.
//
//     kdecode [--raw] [--from <record>] [--count <records>] <file>...
//     kdecode [--raw] [--from <key>] [--to <key>] [--count <rows>] --table <table> <run.qry>...
//
// A reader that shows a file grows it a slice at a time, from the record it stands at, a slice after it or before it
// as it scrolls and on both sides as it stands still. Where it moves fast it asks --raw, each record a line of its code
// and its numbers in hex as the file holds them, a text by its number and a who by its code: the fewest letters to
// draw, so that the most records fit what it shows. Where it stands still it asks without, each record decoded. The
// records are counted from 0, as the file holds them, the texts apart, and a raw line and a decoded line stand for
// the same record. Each file is read as the run holds it where a run's writer runs, and from the disk where none
// does.
//
// A table's rows are its blobs in the run (qry_buffer.h), a row's key its blob's sequence. --from and --to give the
// least key and the most, and --count the most rows, each counted from the first row at or past --from. Raw, each row
// is a line of its key and every word of it in hex, as the run holds them. Decoded, a table whose oracle stands beside
// qry_buffer.h (<table>.oracle.tsv) is a line of its key, the row's kind and each column as the oracle gives it, the
// who named last: a key with the name of its blob, a text by its number and its bytes, a code by its name in the
// oracle or the oracle its gloss names, one of two words by the word, a hash in hex and any other number in decimal.
// The table of texts is a line of its key and its text. Any other table is a k-file's (krep_tabled_by), each row
// its change as krep_records.oracle.tsv names it, the file's key and the numbers the change gives, a put record's
// code named through its format's oracle and the texts of the file a record names given after it. Exit 0 where every
// file was read, 1 where one was not
#include "kqr/kqr.h"
#include "krep_head.h"
#include "krep_records.h"
#include "qry/qry.h"
#include "qry/qry_buffer.h"

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

static KqrRecord s_record;
static KrepFile s_file;

// the records of `file` from `from`, `count` of them or every one past it where `count` is 0, a line each: raw, its
// code and its numbers in hex, where `raw`; and where not, each who named and every other record with its code, its
// numbers and the texts it names by number given after it
static void kdecode_records(const KrepFile *file, int raw, unsigned int from, unsigned int count)
{
    const unsigned int past =
        (from >= file->records)
            ? from
            : (((count == 0u) || (count > (file->records - from))) ? file->records : (from + count));
    for (unsigned int at = from; at < past; at += 1u)
    {
        const KrepRecord *const held = &file->record[at];
        if (!raw && (held->code == KREP_RECORD_WHO) && (held->count == 2u))
        {
            char code[QRY_WHO_CODE_LETTERS + 1u];
            char named[KDECODE_NAMED];
            const QryWho who = qry_who_of_words(held->word);
            qry_who_code(who, code, sizeof(code));
            printf("who %s %s\n", code, qry_who_named(who, named, sizeof(named)) ? named : "");
            continue;
        }
        printf("%x", held->code);
        for (unsigned int place = 0u; place < held->count; place += 1u)
        {
            printf(" %llx", held->word[place]);
        }
        for (unsigned int place = 0u; !raw && (place < held->count); place += 1u)
        {
            unsigned long long length = 0ull;
            const char *const text = krep_text(file, held->word[place], &length);
            // a text a record names is printed to its length, which an int counts
            if ((held->code != KREP_RECORD_WHO) && (text != NULL) && (held->word[place] != 0ull))
            {
                printf(" [%llx %.*s]", held->word[place], (int)length, text);
            }
        }
        printf("\n");
    }
}

// the number the word `word` writes into `value`: 1, or 0 where it is not one written plain in decimal
static int kdecode_number(const char *word, unsigned long long *value)
{
    char *after = NULL;
    *value = strtoull(word, &after, 10);
    return (after != word) && (*after == '\0');
}

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

// `field` of the line `line`, its fields apart by tabs, copied into `word`, which holds `room`: 1, or 0 where the line
// has no such field
static int kdecode_field(const char *line, unsigned int field, char *word, size_t room)
{
    const char *at = line;
    for (unsigned int passed = 0u; passed < field; passed += 1u)
    {
        at = strchr(at, '\t');
        if (at == NULL)
        {
            return 0;
        }
        at += 1;
    }
    const size_t length = strcspn(at, "\t\r\n");
    // a field cut to its room is the field's start, which no oracle field this reads reaches
    const int written = snprintf(word, room, "%.*s", (int)length, at);
    return written >= 0;
}

// the oracle `bytes` hold, `size` of them, read into `oracle`: its codes, and where `codes_only` is 0 its row's kind
// and its columns too
static void kdecode_oracle_parsed(const unsigned char *bytes, unsigned long long size, KdecodeOracle *oracle,
                                  int codes_only)
{
    static char s_line[KDECODE_LINE];
    unsigned long long at = 0ull;
    while (qry_line_next(bytes, size, &at, s_line, sizeof(s_line)))
    {
        char where[KDECODE_WORD];
        char kind[KDECODE_WORD];
        char code[KDECODE_WORD];
        char form[KDECODE_WORD];
        if (!kdecode_field(s_line, 0u, where, sizeof(where)) || !kdecode_field(s_line, 2u, kind, sizeof(kind)) ||
            !kdecode_field(s_line, 3u, code, sizeof(code)) || !kdecode_field(s_line, 4u, form, sizeof(form)))
        {
            continue;
        }
        if (!codes_only && (strcmp(where, "row") == 0) && (oracle->kind[0] == '\0'))
        {
            snprintf(oracle->kind, sizeof(oracle->kind), "%s", kind);
        }
        if (!codes_only && (strncmp(where, "column ", 7u) == 0))
        {
            const unsigned long place = strtoul(where + 7, NULL, 10);
            if (place < QRY_ROW_COLUMNS)
            {
                // a column's place is below QRY_ROW_COLUMNS, which an unsigned int holds
                KdecodeColumn *const column = &oracle->column[place];
                snprintf(column->name, sizeof(column->name), "%s", kind);
                snprintf(column->form, sizeof(column->form), "%s", form);
                const char *const gloss = strrchr(s_line, '\t');
                const char *const named = (gloss != NULL) ? strstr(gloss, ".oracle.tsv") : NULL;
                const char *start = named;
                while ((start != NULL) && (start > gloss) && (isalnum((unsigned char)start[-1]) || (start[-1] == '_')))
                {
                    start -= 1;
                }
                if ((named != NULL) && (start < named))
                {
                    // the oracle's name, its stem and .oracle.tsv, is far shorter than an int counts
                    snprintf(column->codes_from, sizeof(column->codes_from), "%.*s",
                             (int)((named - start) + (ptrdiff_t)strlen(".oracle.tsv")), start);
                }
                oracle->columns =
                    ((unsigned int)place >= oracle->columns) ? ((unsigned int)place + 1u) : oracle->columns;
            }
        }
        if (strncmp(code, "0x", 2u) == 0)
        {
            if (oracle->codes == oracle->code_room)
            {
                const unsigned int room = (oracle->code_room == 0u) ? 32u : (oracle->code_room * 2u);
                KdecodeCode *const larger = (KdecodeCode *)realloc(oracle->code, room * sizeof(KdecodeCode));
                if (larger == NULL)
                {
                    continue;
                }
                oracle->code = larger;
                oracle->code_room = room;
            }
            // a family numbered past its name, `record 1`, is the family its name gives
            KdecodeCode *const one = &oracle->code[oracle->codes];
            snprintf(one->family, sizeof(one->family), "%.*s", (int)strcspn(where, " "), where);
            snprintf(one->name, sizeof(one->name), "%s", kind);
            one->value = strtoull(code, NULL, 16);
            oracle->codes += 1u;
        }
    }
}

static void kdecode_oracle_free(KdecodeOracle *oracle)
{
    free(oracle->code);
    memset(oracle, 0, sizeof(*oracle));
}

// the name of the code `value` of the family `family` in `oracle`, or NULL where it gives none
static const char *kdecode_code_named(const KdecodeOracle *oracle, const char *family, unsigned long long value)
{
    for (unsigned int at = 0u; at < oracle->codes; at += 1u)
    {
        if ((oracle->code[at].value == value) && (strcmp(oracle->code[at].family, family) == 0))
        {
            return oracle->code[at].name;
        }
    }
    return NULL;
}

// 1 where `oracle` gives a code of the family `family`
static int kdecode_family_given(const KdecodeOracle *oracle, const char *family)
{
    for (unsigned int at = 0u; at < oracle->codes; at += 1u)
    {
        if (strcmp(oracle->code[at].family, family) == 0)
        {
            return 1;
        }
    }
    return 0;
}

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

// room in `*items` for one more of `width` bytes, `*count` held and `*room` given: 1, or 0 where the system gives none
static int kdecode_grown(void **items, size_t width, size_t count, size_t *room)
{
    if (count < *room)
    {
        return 1;
    }
    const size_t larger = (*room == 0u) ? 256u : (*room * 2u);
    void *const moved = realloc(*items, larger * width);
    if (moved == NULL)
    {
        return 0;
    }
    *items = moved;
    *room = larger;
    return 1;
}

// `size` bytes at `bytes` added to the run's bytes, a word's boundary first: where they stand, or 0 with the run marked
// failed where the system gives no memory. The bytes start past a first word of zeros, so that no bytes stand at 0
static size_t kdecode_kept(KdecodeRun *run, const void *bytes, size_t size)
{
    const size_t at = (run->size == 0u) ? 8u : ((run->size + 7u) & ~(size_t)7u);
    if ((at + size) > run->room)
    {
        size_t larger = (run->room == 0u) ? (1u << 20u) : run->room;
        while (larger < (at + size))
        {
            larger *= 2u;
        }
        unsigned char *const moved = (unsigned char *)realloc(run->bytes, larger);
        if (moved == NULL)
        {
            run->failed = 1;
            return 0u;
        }
        run->bytes = moved;
        run->room = larger;
    }
    memset(run->bytes + run->size, 0, at - run->size);
    memcpy(run->bytes + at, bytes, size);
    run->size = at + size;
    return at;
}

// the word at `place` of the `size` bytes at `bytes`, the low byte first, its bytes past `size` taken as zeros
static unsigned long long kdecode_word(const unsigned char *bytes, unsigned long long size, unsigned long long place)
{
    unsigned long long word = 0ull;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        const unsigned long long byte = (place * 8ull) + at;
        word |= (byte < size) ? ((unsigned long long)bytes[byte] << (8u * at)) : 0ull;
    }
    return word;
}

// a text `length` bytes at `bytes` of the file `file` kept under `number`
static void kdecode_text_kept(KdecodeRun *run, unsigned long long file, unsigned long long number,
                              const unsigned char *bytes, unsigned long long length)
{
    if (!kdecode_grown((void **)&run->text, sizeof(KdecodeText), run->texts, &run->text_room))
    {
        run->failed = 1;
        return;
    }
    // a text is a blob's bytes, which the mapped run holds and a size_t counts
    const size_t at = kdecode_kept(run, bytes, (size_t)length);
    run->text[run->texts] = (KdecodeText){file, number, at, length};
    run->texts += (at != 0u) ? 1u : 0u;
}

// an oracle the run holds kept, `size` bytes at `bytes` under the name at `name_at`, in place of an earlier blob of
// the name: the blobs come in the order of their sequences, and the latest is kept
static void kdecode_held_kept(KdecodeRun *run, size_t name_at, const unsigned char *bytes, unsigned long long size)
{
    size_t place = 0u;
    while ((place < run->helds) &&
           (strcmp((const char *)(run->bytes + run->held[place].name_at), (const char *)(run->bytes + name_at)) != 0))
    {
        place += 1u;
    }
    if ((place == run->helds) && !kdecode_grown((void **)&run->held, sizeof(KdecodeHeld), run->helds, &run->held_room))
    {
        run->failed = 1;
        return;
    }
    // an oracle is a blob's bytes, which the mapped run holds and a size_t counts
    const size_t at = kdecode_kept(run, bytes, (size_t)size);
    run->held[place] = (KdecodeHeld){name_at, at, size};
    run->helds += ((place == run->helds) && (at != 0u)) ? 1u : 0u;
}

// one blob of the run, handed by qry_file_each in the order of the sequences: its sequence and name kept; an oracle
// and a text of the run's table of texts kept; and where it is of the table asked, a k-file's head and text kept and a
// row of the range kept
static void kdecode_blob(void *context, const char *name, unsigned long long sequence, const unsigned char *bytes,
                         unsigned long long size)
{
    KdecodeRun *const run = (KdecodeRun *)context;
    if (run->failed)
    {
        return;
    }
    if (!kdecode_grown((void **)&run->blob, sizeof(KdecodeBlob), run->blobs, &run->blob_room))
    {
        run->failed = 1;
        return;
    }
    const size_t name_at = kdecode_kept(run, name, strlen(name) + 1u);
    run->blob[run->blobs] = (KdecodeBlob){sequence, name_at};
    run->blobs += 1u;
    const size_t length = strlen(name);
    const size_t suffix = strlen(".oracle.tsv");
    if ((length > suffix) && (strcmp(name + length - suffix, ".oracle.tsv") == 0))
    {
        kdecode_held_kept(run, name_at, bytes, size);
    }
    const unsigned long long words = (size + 7ull) / 8ull;
    if ((strcmp(name, QRY_TEXTS_TABLE) == 0) && (words >= 4ull) && (kdecode_word(bytes, size, 3ull) <= (size - 32ull)))
    {
        kdecode_text_kept(run, 0ull, kdecode_word(bytes, size, 2ull), bytes + 32u, kdecode_word(bytes, size, 3ull));
    }
    if (strcmp(name, run->table) != 0)
    {
        return;
    }
    const unsigned long long change = (words >= 4ull) ? kdecode_word(bytes, size, 3ull) : ~0ull;
    if ((change == KREP_ROW_HEAD) && (words >= 5ull) &&
        kdecode_grown((void **)&run->file, sizeof(KdecodeFile), run->files, &run->file_room))
    {
        KdecodeFile *const file = &run->file[run->files];
        file->key = sequence;
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            // the kind's four bytes stand the low first in its word
            file->kind[at] = (char)((kdecode_word(bytes, size, 4ull) >> (8u * at)) & 0xffull);
        }
        file->kind[4] = '\0';
        run->files += 1u;
    }
    if ((change == KREP_ROW_TEXT) && (words >= 6ull) && (kdecode_word(bytes, size, 5ull) <= (size - 48ull)))
    {
        kdecode_text_kept(run, kdecode_word(bytes, size, 0ull), kdecode_word(bytes, size, 4ull), bytes + 48u,
                          kdecode_word(bytes, size, 5ull));
    }
    const int ranged =
        (sequence >= run->from) && (sequence <= run->to) && ((run->most == 0ull) || (run->rows < run->most));
    if (!ranged || !kdecode_grown((void **)&run->row, sizeof(KdecodeRow), run->rows, &run->row_room))
    {
        run->failed = run->failed || ranged;
        return;
    }
    // a row is a blob's bytes, which the mapped run holds and a size_t counts
    const size_t at = kdecode_kept(run, bytes, (size_t)size);
    run->row[run->rows] = (KdecodeRow){sequence, at, (size_t)words};
    run->rows += (at != 0u) ? 1u : 0u;
}

// the oracle at `path` from the top of the tree read into `oracle` as kdecode_oracle_parsed reads it: the one the run
// holds, the latest blob of its name, or where the run holds none, the tree's. 1, or 0 where neither could be read
static int kdecode_oracle_read(const KdecodeRun *run, const char *path, KdecodeOracle *oracle, int codes_only)
{
    for (size_t at = 0u; at < run->helds; at += 1u)
    {
        if (strcmp((const char *)(run->bytes + run->held[at].name_at), path) == 0)
        {
            kdecode_oracle_parsed(run->bytes + run->held[at].bytes_at, run->held[at].size, oracle, codes_only);
            return 1;
        }
    }
    char top[1024];
    char full[1400];
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_top(top, sizeof(top)) || (snprintf(full, sizeof(full), "%s/%s", top, path) <= 0) ||
        !qry_file_read(qry_run(), full, &bytes, &size))
    {
        return 0;
    }
    kdecode_oracle_parsed(bytes, size, oracle, codes_only);
    free(bytes);
    return 1;
}

// the word at `place` of the kept row `row`
static unsigned long long kdecode_row_word(const KdecodeRun *run, const KdecodeRow *row, size_t place)
{
    unsigned long long word = 0ull;
    memcpy(&word, run->bytes + row->words_at + (place * 8u), 8u);
    return word;
}

// the name of the blob whose sequence is `key`, or NULL where the run holds none: the blobs stand in the order of
// their sequences, and a search halves them
static const char *kdecode_key_named(const KdecodeRun *run, unsigned long long key)
{
    size_t low = 0u;
    size_t high = run->blobs;
    while (low < high)
    {
        const size_t middle = low + ((high - low) / 2u);
        if (run->blob[middle].sequence < key)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return ((low < run->blobs) && (run->blob[low].sequence == key))
               ? (const char *)(run->bytes + run->blob[low].name_at)
               : NULL;
}

// the text of the file `file` numbered `number`, its length in `length`, or NULL where the run holds none
static const char *kdecode_text(const KdecodeRun *run, unsigned long long file, unsigned long long number,
                                unsigned long long *length)
{
    for (size_t at = 0u; at < run->texts; at += 1u)
    {
        if ((run->text[at].file == file) && (run->text[at].number == number))
        {
            *length = run->text[at].length;
            return (const char *)(run->bytes + run->text[at].bytes_at);
        }
    }
    return NULL;
}

// ` [<number> <text>]` of the text `number` of the file `file`, `-` where the number is 0, and the number alone where
// the run holds no such text
static void kdecode_text_printed(const KdecodeRun *run, unsigned long long file, unsigned long long number)
{
    unsigned long long length = 0ull;
    const char *const text = (number != 0ull) ? kdecode_text(run, file, number, &length) : NULL;
    if (number == 0ull)
    {
        printf(" -");
    }
    else if (text == NULL)
    {
        printf(" %llx", number);
    }
    else
    {
        // a text is printed to its length, which an int counts
        printf(" [%llx %.*s]", number, (int)length, text);
    }
}

// ` who <named>` of the who words `words` hold, nothing where both are 0, a row of no who
static void kdecode_who_printed(const unsigned long long *words)
{
    char named[KDECODE_NAMED];
    if (((words[0] != 0ull) || (words[1] != 0ull)) && qry_who_named(qry_who_of_words(words), named, sizeof(named)))
    {
        printf(" who %s", named);
    }
}

// the column `column` of value `value` printed as `oracle` gives it
static void kdecode_column_printed(const KdecodeRun *run, const KdecodeOracle *oracle, const KdecodeColumn *column,
                                   unsigned long long value)
{
    const char *const form = column->form;
    const char *const either = strchr(form, '|');
    char family[KDECODE_WORD] = "";
    if ((form[0] == '<') && (strchr(form, '>') != NULL))
    {
        // the family is the form's word, between its brackets, far shorter than an int counts
        snprintf(family, sizeof(family), "%.*s", (int)(strchr(form, '>') - form - 1), form + 1);
    }
    const char *const named = kdecode_code_named(oracle, family, value);
    if (strcmp(family, "key") == 0)
    {
        const char *const blob = kdecode_key_named(run, value);
        if (blob != NULL)
        {
            printf(" %llu:%s", value, blob);
        }
        else
        {
            printf(" %llu", value);
        }
    }
    else if (strcmp(family, "text") == 0)
    {
        kdecode_text_printed(run, 0ull, value);
    }
    else if ((either != NULL) && (value <= 1ull))
    {
        // the first word stands for 1 and the second for 0, each far shorter than an int counts
        printf(" %.*s", (value == 1ull) ? (int)(either - form) : (int)strlen(either + 1),
               (value == 1ull) ? form : (either + 1));
    }
    else if (named != NULL)
    {
        printf(" %s", named);
    }
    else if (kdecode_family_given(oracle, family) || (strcmp(family, "hash") == 0) || (either != NULL))
    {
        printf(" %llx", value);
    }
    else
    {
        printf(" %llu", value);
    }
}

// the row `row` of a table with an oracle, its who's two words then its columns, decoded: its key, the row's kind, each
// column, any word past the oracle's columns in hex, and its who named
static void kdecode_columns_printed(const KdecodeRun *run, const KdecodeOracle *oracle, const KdecodeRow *row)
{
    unsigned long long who[2] = {0ull, 0ull};
    for (size_t at = 0u; (at < 2u) && (at < row->words); at += 1u)
    {
        who[at] = kdecode_row_word(run, row, at);
    }
    printf("%llu %s", row->key, oracle->kind);
    for (size_t at = 2u; at < row->words; at += 1u)
    {
        const size_t place = at - 2u;
        const unsigned long long value = kdecode_row_word(run, row, at);
        if ((place < oracle->columns) && (oracle->column[place].form[0] != '\0'))
        {
            kdecode_column_printed(run, oracle, &oracle->column[place], value);
        }
        else
        {
            printf(" %llx", value);
        }
    }
    kdecode_who_printed(who);
    printf("\n");
}

// the oracle of the kind of the file `key` of the table, read into `oracle` once a file: the kind's own, its folder and
// its stem the kind's letters in lower case
static const KdecodeOracle *kdecode_format_oracle(const KdecodeRun *run, unsigned long long key, KdecodeOracle *oracle,
                                                  unsigned long long *read_for)
{
    if (*read_for == key)
    {
        return oracle;
    }
    kdecode_oracle_free(oracle);
    *read_for = key;
    for (size_t at = 0u; at < run->files; at += 1u)
    {
        if (run->file[at].key != key)
        {
            continue;
        }
        char stem[5];
        size_t length = 0u;
        while ((length < 4u) && (run->file[at].kind[length] != '\0'))
        {
            stem[length] = (char)tolower((unsigned char)run->file[at].kind[length]);
            length += 1u;
        }
        stem[length] = '\0';
        char path[256];
        snprintf(path, sizeof(path), "%s/%s/%s.oracle.tsv", QRY_KFILE_ORACLES, stem, stem);
        kdecode_oracle_read(run, path, oracle, 1);
    }
    return oracle;
}

// the row `row` of a k-file's table decoded: its key, its change as `rows` names it, the file's key and the numbers the
// change gives, a put record's code named by its format's oracle, the texts of the file a record names, and its who
static void kdecode_change_printed(const KdecodeRun *run, const KdecodeOracle *rows, const KdecodeRow *row,
                                   KdecodeOracle *format, unsigned long long *format_for)
{
    if (row->words < 4u)
    {
        printf("%llu", row->key);
        for (size_t at = 0u; at < row->words; at += 1u)
        {
            printf(" %llx", kdecode_row_word(run, row, at));
        }
        printf("\n");
        return;
    }
    const unsigned long long change = kdecode_row_word(run, row, 3u);
    const unsigned long long file = (change == KREP_ROW_HEAD) ? row->key : kdecode_row_word(run, row, 0u);
    const unsigned long long who[2] = {kdecode_row_word(run, row, 1u), kdecode_row_word(run, row, 2u)};
    const char *const named = kdecode_code_named(rows, "row", change);
    if (named != NULL)
    {
        printf("%llu %s", row->key, named);
    }
    else
    {
        printf("%llu %llx", row->key, change);
    }
    if ((change == KREP_ROW_HEAD) && (row->words >= 7u))
    {
        const unsigned long long kind = kdecode_row_word(run, row, 4u);
        const unsigned long long length = kdecode_row_word(run, row, 6u);
        const size_t room = (row->words - 7u) * 8u;
        char letters[5] = "";
        for (unsigned int at = 0u; (at < 4u) && (((kind >> (8u * at)) & 0xffull) != 0ull); at += 1u)
        {
            // the kind's four bytes stand the low first in its word, its zeros past its letters
            letters[at] = (char)((kind >> (8u * at)) & 0xffull);
        }
        // the name's length is at most its row's bytes, which an int counts
        printf(" %s %llu %.*s", letters, kdecode_row_word(run, row, 5u), (int)((length <= room) ? length : room),
               (const char *)(run->bytes + row->words_at + 56u));
    }
    else if ((change == KREP_ROW_TEXT) && (row->words >= 5u))
    {
        printf(" %llu", file);
        kdecode_text_printed(run, file, kdecode_row_word(run, row, 4u));
    }
    else
    {
        printf(" %llu", file);
        // a put gives its place, its code and its count; a set its place and its count; a drop its place
        const size_t opening = (change == KREP_ROW_PUT) ? 7u : ((change == KREP_ROW_SET) ? 6u : 5u);
        for (size_t at = 4u; (at < opening) && (at < row->words); at += 1u)
        {
            const unsigned long long value = kdecode_row_word(run, row, at);
            const KdecodeOracle *const kind =
                ((change == KREP_ROW_PUT) && (at == 5u)) ? kdecode_format_oracle(run, file, format, format_for) : NULL;
            const char *const code = (kind != NULL) ? kdecode_code_named(kind, "record", value) : NULL;
            if (code != NULL)
            {
                printf(" %s", code);
            }
            else
            {
                printf(" %llx", value);
            }
        }
        for (size_t at = opening; at < row->words; at += 1u)
        {
            printf(" %llx", kdecode_row_word(run, row, at));
        }
        for (size_t at = opening; at < row->words; at += 1u)
        {
            unsigned long long length = 0ull;
            const unsigned long long number = kdecode_row_word(run, row, at);
            const char *const text = (number != 0ull) ? kdecode_text(run, file, number, &length) : NULL;
            if (text != NULL)
            {
                // a text a record names is printed to its length, which an int counts
                printf(" [%llx %.*s]", number, (int)length, text);
            }
        }
    }
    kdecode_who_printed(who);
    printf("\n");
}

// the table `table` of the run at `path` from `from` through `to`, `most` rows of it or every one where `most` is 0,
// raw or decoded: 1, or 0 where the run could not be read
static int kdecode_table(const char *path, const char *table, int raw, unsigned long long from, unsigned long long to,
                         unsigned long long most)
{
    KdecodeRun run;
    memset(&run, 0, sizeof(run));
    run.table = table;
    run.from = from;
    run.to = to;
    run.most = most;
    char error[1400];
    const int read = qry_file_each(path, kdecode_blob, &run, error, sizeof(error));
    if (!read || run.failed)
    {
        fprintf(stderr, "kdecode: %s\n", run.failed ? "no memory to read the run" : error);
    }
    else if (error[0] != '\0')
    {
        fprintf(stderr, "kdecode: %s\n", error);
    }
    KdecodeOracle oracle;
    KdecodeOracle format;
    memset(&oracle, 0, sizeof(oracle));
    memset(&format, 0, sizeof(format));
    unsigned long long format_for = 0ull;
    char oracle_path[256];
    snprintf(oracle_path, sizeof(oracle_path), "%s/%s.oracle.tsv", QRY_ORACLES, table);
    const int texts = (strcmp(table, QRY_TEXTS_TABLE) == 0);
    const int columned = !raw && !texts && kdecode_oracle_read(&run, oracle_path, &oracle, 0) && (oracle.columns != 0u);
    for (unsigned int at = 0u; columned && (at < oracle.columns); at += 1u)
    {
        char from_path[256];
        const char *const named = oracle.column[at].codes_from;
        if ((named[0] != '\0') && (snprintf(from_path, sizeof(from_path), "%s/%s", QRY_ORACLES, named) > 0))
        {
            kdecode_oracle_read(&run, from_path, &oracle, 1);
        }
    }
    // a table with no oracle is a k-file's where the run holds a head of it, and is given raw where it holds none
    const int changed = !raw && !texts && !columned && (run.files != 0u);
    if (changed)
    {
        kdecode_oracle_free(&oracle);
        kdecode_oracle_read(&run, QRY_KFILE_ROWS, &oracle, 1);
    }
    const int bare = raw || (!texts && !columned && !changed);
    for (size_t at = 0u; read && !run.failed && (at < run.rows); at += 1u)
    {
        const KdecodeRow *const row = &run.row[at];
        if (bare || (texts && (row->words < 4u)))
        {
            printf("%llx", row->key);
            for (size_t place = 0u; place < row->words; place += 1u)
            {
                printf(" %llx", kdecode_row_word(&run, row, place));
            }
            printf("\n");
        }
        else if (texts)
        {
            printf("%llu text", row->key);
            kdecode_text_printed(&run, 0ull, kdecode_row_word(&run, row, 2u));
            printf("\n");
        }
        else if (columned)
        {
            kdecode_columns_printed(&run, &oracle, row);
        }
        else
        {
            kdecode_change_printed(&run, &oracle, row, &format, &format_for);
        }
    }
    kdecode_oracle_free(&oracle);
    kdecode_oracle_free(&format);
    free(run.blob);
    free(run.text);
    free(run.file);
    free(run.row);
    free(run.held);
    free(run.bytes);
    return read && !run.failed;
}

int main(int count, char **words)
{
    int raw = 0;
    unsigned long long from = 0ull;
    unsigned long long to = ~0ull;
    unsigned long long slice = 0ull;
    const char *table = NULL;
    int given = 1;
    while ((given < count) && (strncmp(words[given], "--", 2u) == 0))
    {
        if (strcmp(words[given], "--raw") == 0)
        {
            raw = 1;
            given += 1;
        }
        else if ((strcmp(words[given], "--from") == 0) && ((given + 1) < count) &&
                 kdecode_number(words[given + 1], &from))
        {
            given += 2;
        }
        else if ((strcmp(words[given], "--to") == 0) && ((given + 1) < count) && kdecode_number(words[given + 1], &to))
        {
            given += 2;
        }
        else if ((strcmp(words[given], "--count") == 0) && ((given + 1) < count) &&
                 kdecode_number(words[given + 1], &slice))
        {
            given += 2;
        }
        else if ((strcmp(words[given], "--table") == 0) && ((given + 1) < count))
        {
            table = words[given + 1];
            given += 2;
        }
        else
        {
            break;
        }
    }
    if (given >= count)
    {
        fprintf(stderr, "kdecode [--raw] [--from <record>] [--count <records>] <file>...\n"
                        "kdecode [--raw] [--from <key>] [--to <key>] [--count <rows>] --table <table> <run.qry>...\n");
        return 2;
    }
#if defined(_WIN32)
    // the text written as its bytes, each line's end the one byte the format gives it
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    // a k-file's records are counted by an unsigned int, and a record past what it counts is past every record
    const unsigned int record_from = (from > 0xffffffffull) ? 0xffffffffu : (unsigned int)from;
    const unsigned int record_slice = (slice > 0xffffffffull) ? 0u : (unsigned int)slice;
    int read = 1;
    for (; given < count; given += 1)
    {
        unsigned char *bytes = NULL;
        unsigned long long size = 0ull;
        if (table != NULL)
        {
            read = kdecode_table(words[given], table, raw, from, to, slice) && read;
            continue;
        }
        if (!qry_file_read(qry_run(), words[given], &bytes, &size) || (size < KREP_HEAD_BYTES) ||
            (memcmp(bytes, KREP_MAGIC, 8u) != 0))
        {
            const int ran = (size >= 8u) &&
                            ((memcmp(bytes, QRY_TABLE_MAGIC, 8u) == 0) || (memcmp(bytes, QRY_FRAME_MAGIC, 8u) == 0));
            fprintf(stderr, ran ? "kdecode: %s is a run: name its table with --table\n" : "kdecode: %s is no k-file\n",
                    words[given]);
            free(bytes);
            read = 0;
            continue;
        }
        char kind[4];
        memcpy(kind, bytes + 8u, 4u);
        free(bytes);
        char error[1400];
        if (!raw && (memcmp(kind, KQR_KIND, 4u) == 0))
        {
            if (!kqr_read(&s_record, words[given], "-", error, sizeof(error)))
            {
                fprintf(stderr, "kdecode: %s\n", error);
                read = 0;
                continue;
            }
            QryText text = {0};
            kqr_text_sliced(&s_record, &text, 1, record_from, record_slice);
            fwrite(text.bytes, 1u, (size_t)text.size, stdout);
            qry_text_freed(&text);
            kqr_free(&s_record);
            continue;
        }
        if (krep_read(&s_file, words[given], kind, error, sizeof(error)) != 1)
        {
            fprintf(stderr, "kdecode: %s\n", error);
            read = 0;
            continue;
        }
        kdecode_records(&s_file, raw, record_from, record_slice);
        krep_free(&s_file);
    }
    return read ? 0 : 1;
}
