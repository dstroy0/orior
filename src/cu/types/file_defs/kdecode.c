// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kdecode.c: a k-file of records (krep_records.h) written for a person to read, the way a stack trace is read
// through its symbols: each record in the text its format gives it, each who named through the who table
// (qry_buffer.h, QryWho), and a kind no format here reads given as its records' codes and numbers and its texts. A
// table of a run's .qry is read the same way, the select over its rows: the rows of the table, a range of their keys,
// raw or decoded through the table's oracle.
//
//     kdecode [--raw] [--from <record>] [--count <records>] <file>...
//     kdecode [--raw] [--from <key>] [--to <key>] [--count <rows>] --table <table> <run.qry>...
//     kdecode [--latest] --blob <name> <run.qry>...
//
// A blob of the run by its name is printed whole, every blob of that name in sequence or the latest alone with
// --latest, each after a line of its sequence, its name and its size, a NUL byte shown as a space: a console's blob,
// a trace, a kernel listing or a file the run holds, as the run holds it.
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
#include "kdecode/kdecode_internal.h"

int main(int count, char **words)
{
    int raw = 0;
    unsigned long long from = 0ull;
    unsigned long long to = ~0ull;
    unsigned long long slice = 0ull;
    const char *table = NULL;
    const char *blob = NULL;
    int latest = 0;
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
        else if ((strcmp(words[given], "--blob") == 0) && ((given + 1) < count))
        {
            blob = words[given + 1];
            given += 2;
        }
        else if (strcmp(words[given], "--latest") == 0)
        {
            latest = 1;
            given += 1;
        }
        else
        {
            break;
        }
    }
    if (given >= count)
    {
        fprintf(stderr, "kdecode [--raw] [--from <record>] [--count <records>] <file>...\n"
                        "kdecode [--raw] [--from <key>] [--to <key>] [--count <rows>] --table <table> <run.qry>...\n"
                        "kdecode [--latest] --blob <name> <run.qry>...\n");
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
        if (blob != NULL)
        {
            read = kdecode_blobs(words[given], blob, latest) && read;
            continue;
        }
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
