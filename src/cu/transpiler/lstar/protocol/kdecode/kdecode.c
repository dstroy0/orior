// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kdecode.c: a k-file of records (krep_records.h) written for a person to read, the way a stack trace is read
// through its symbols: each record in the text its format gives it, each who named through the who table
// (who.h, Who), and a type no format here reads given as its records' codes and numbers and its texts. A
// table of a run's .qry is read the same way, the select over its rows: the rows of the table, a range of their keys,
// raw or decoded through the table's oracle.
//
//     orior_kdecode [--raw] [--from <record>] [--count <records>] <file>...
//     orior_kdecode [--raw] [--from <key>] [--to <key>] [--count <rows>] --table <table> <run.qry>...
//     orior_kdecode [--latest] --blob <name> <run.qry>...
//
// orior_kdecode passes what it writes for a person to read to the run that it is a member of. It is the record
// `orior_kdecode` (KDECODE_DISPLAYED), with its failures among it. A member of no run writes it to its standard output,
// and its failures to its standard error.
//
// The program prints a blob of the run by its name whole: every blob of that name in sequence, or the latest alone
// with --latest. Each comes after a line of its sequence, its name and its size, with a NUL byte shown as a space. It
// is a blob of a console, a trace, a kernel listing or a file that the run stores, as the run stores it.
//
// A reader that shows a file grows it one slice at a time, from the record that it is at. It adds a slice after it or
// before it as it scrolls, and on both sides while it is stationary. Where it moves fast, it requests --raw.
//
// Then each record is a line of its code and its numbers in hex as the file stores them. A text goes by its number, and
// a who by its code. That is the fewest characters to draw, so that the most records fit what it shows. Where it is
// stationary, it requests no --raw, with each record decoded.
//
// The program counts the records from 0, as the file stores them, with the texts apart. A raw line and a decoded line
// are for the same record. The program reads each file as the run stores it where a writer of the run runs, and from
// the disk where none does.
//
// The rows of a table are its blobs in the run (qry_member.h), and the key of a row is the sequence of its blob. The
// options --from and --to give the least key and the max. The option --count gives the max rows, counted from the first
// row at or past --from. Raw, each row is a line of its key and every word of it in hex, as the run stores them.
//
// Decoded, a table whose oracle is beside qry_member.h (<table>.oracle.tsv) is a line of its key and the type of the
// row. Then each column as the oracle gives it, with the who named last. A key goes with the name of its blob, and a
// text by its number and its bytes. A code goes by its name in the oracle, or in the oracle that its gloss names. One
// of two words goes by the word, a hash in hex, and any other number in decimal.
//
// The table of texts is a line of its key and its text. Any other table belongs to a k-file (krep_tabled_by). Each row
// is its change as krep_records.oracle.tsv names it, the key of the file, and the numbers that the change gives. The
// oracle of its format names the code of a put record, and the texts of the file that a record names follow it.
//
// Exit 0 where the program read every file, and 1 where it did not read one
#include "../../../../../../includes/parsers/who.h"
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "kdecode/kdecode_internal.h"
#include "memoria_operor/memoria_operor.h"
#include "verba_scribo/verba_linea.h"

int program_main(int count, char **words)
{
    int raw = 0;
    unsigned long long from = 0ULL;
    unsigned long long to = ~0ULL;
    unsigned long long slice = 0ULL;
    const char *table = NULL;
    const char *blob = NULL;
    int latest = 0;
    int given = 1;
    while ((given < count) && (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = words[given],
                                          .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "--", .ci = EMBED_FALSE)))
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[given], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "--raw", .ci = EMBED_FALSE))
        {
            raw = 1;
            given += 1;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[given], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--from", .ci = EMBED_FALSE)) &&
                 ((given + 1) < count) && kdecode_number(words[given + 1], &from))
        {
            given += 2;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[given], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--to", .ci = EMBED_FALSE)) &&
                 ((given + 1) < count) && kdecode_number(words[given + 1], &to))
        {
            given += 2;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[given], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--count", .ci = EMBED_FALSE)) &&
                 ((given + 1) < count) && kdecode_number(words[given + 1], &slice))
        {
            given += 2;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[given], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--table", .ci = EMBED_FALSE)) &&
                 ((given + 1) < count))
        {
            table = words[given + 1];
            given += 2;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[given], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--blob", .ci = EMBED_FALSE)) &&
                 ((given + 1) < count))
        {
            blob = words[given + 1];
            given += 2;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[given], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--latest", .ci = EMBED_FALSE))
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
        mmgr_line_put(&s_displayed,
                      "orior_kdecode [--raw] [--from <record>] [--count <records>] <file>...\n"
                      "orior_kdecode [--raw] [--from <key>] [--to <key>] [--count <rows>] --table <table> "
                      "<run.qry>...\n"
                      "orior_kdecode [--latest] --blob <name> <run.qry>...\n");
        kdecode_failure_passed();
        qry_member_settled();
        return 2;
    }
    // an unsigned int counts the records of a k-file, and a record past what it counts is past every record
    const unsigned int record_from = (from > 0xffffffffULL) ? 0xffffffffu : (unsigned int)from;
    const unsigned int record_slice = (slice > 0xffffffffULL) ? 0u : (unsigned int)slice;
    int read = 1;
    for (; given < count; given += 1)
    {
        const unsigned char *bytes = NULL;
        unsigned long long size = 0ULL;
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
        if (!qry_member_file_blob(words[given], &bytes, &size) || (size < KREP_HEAD_BYTES) ||
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = bytes, .other = KREP_MAGIC, .bytes = 8u) != 0))
        {
            const int ran = kdecode_run_whole(bytes, size);
            kdecode_displayed_passed(1);
            mmgr_line_put(&s_displayed, "orior_kdecode: ");
            mmgr_line_put(&s_displayed, words[given]);
            mmgr_line_put(&s_displayed, ran ? " is a run: name its table with --table\n" : " is no k-file\n");
            kdecode_failure_passed();
            read = 0;
            continue;
        }
        char type[4];
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = type, .src = bytes + 8u, .bytes = 4u);
        char error[1400];
        if (!raw && (EMBED_CALL(memor.cmp, MemoriaCfg, .src = type, .other = KQR_TYPE, .bytes = 4u) == 0))
        {
            if (!kqr_read(&s_record, words[given], "-", error, sizeof(error)))
            {
                kdecode_displayed_passed(1);
                mmgr_line_put(&s_displayed, "orior_kdecode: ");
                mmgr_line_put(&s_displayed, error);
                mmgr_line_ch(&s_displayed, '\n');
                kdecode_failure_passed();
                read = 0;
                continue;
            }
            // the record's text taken into the text passed whole, the text before it passed first
            kdecode_displayed_passed(1);
            s_displayed.at = kqr_text_sliced(&s_record, s_displayed.out, s_displayed.capacity, s_displayed.at, 1,
                                             record_from, record_slice);
            kdecode_displayed_passed(1);
            kqr_free(&s_record);
            continue;
        }
        if (krep_read(&s_file, words[given], type, error, sizeof(error)) != 1)
        {
            kdecode_displayed_passed(1);
            mmgr_line_put(&s_displayed, "orior_kdecode: ");
            mmgr_line_put(&s_displayed, error);
            mmgr_line_ch(&s_displayed, '\n');
            kdecode_failure_passed();
            read = 0;
            continue;
        }
        kdecode_records(&s_file, raw, record_from, record_slice);
        krep_free(&s_file);
    }
    kdecode_displayed_passed(1);
    qry_member_settled();
    return read ? 0 : 1;
}
