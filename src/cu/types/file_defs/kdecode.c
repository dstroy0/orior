// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kdecode.c: a k-file of records (krep_records.h) written for a person to read, the way a stack trace is read
// through its symbols: each record in the text its format gives it, each who named through the who table
// (qry_buffer.h, QryWho), and a kind no format here reads given as its records' codes and numbers and its texts.
//
//     kdecode [--raw] [--from <record>] [--count <records>] <file>...
//
// A reader that shows a file grows it a slice at a time, from the record it stands at, a slice after it or before it
// as it scrolls and on both sides as it stands still. Where it moves fast it asks --raw, each record a line of its code
// and its numbers in hex as the file holds them, a text by its number and a who by its code: the fewest letters to
// draw, so that the most records fit what it shows. Where it stands still it asks without, each record decoded. The
// records are counted from 0, as the file holds them, the texts apart, and a raw line and a decoded line stand for
// the same record. Each file is read as the run holds it where a run's writer runs, and from the disk where none
// does. Exit 0 where every file was read, 1 where one was not
#include "kqr/kqr.h"
#include "krep_head.h"
#include "krep_records.h"
#include "qry/qry_buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

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
            char named[2400];
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
static int kdecode_number(const char *word, unsigned int *value)
{
    char *after = NULL;
    const unsigned long long read = strtoull(word, &after, 10);
    // a count of records is at most what an unsigned int holds
    *value = (unsigned int)read;
    return (after != word) && (*after == '\0') && (read <= 0xffffffffull);
}

int main(int count, char **words)
{
    int raw = 0;
    unsigned int from = 0u;
    unsigned int slice = 0u;
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
        else if ((strcmp(words[given], "--count") == 0) && ((given + 1) < count) &&
                 kdecode_number(words[given + 1], &slice))
        {
            given += 2;
        }
        else
        {
            break;
        }
    }
    if (given >= count)
    {
        fprintf(stderr, "kdecode [--raw] [--from <record>] [--count <records>] <file>...\n");
        return 2;
    }
#if defined(_WIN32)
    // the text written as its bytes, each line's end the one byte the format gives it
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    int read = 1;
    for (; given < count; given += 1)
    {
        unsigned char *bytes = NULL;
        unsigned long long size = 0ull;
        if (!qry_file_read(qry_run(), words[given], &bytes, &size) || (size < KREP_HEAD_BYTES) ||
            (memcmp(bytes, KREP_MAGIC, 8u) != 0))
        {
            fprintf(stderr, "kdecode: %s is no k-file\n", words[given]);
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
            kqr_text_sliced(&s_record, &text, 1, from, slice);
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
        kdecode_records(&s_file, raw, from, slice);
        krep_free(&s_file);
    }
    return read ? 0 : 1;
}
