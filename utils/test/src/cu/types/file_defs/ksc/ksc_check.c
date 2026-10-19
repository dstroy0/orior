// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksc_check.c: the .ksc's reader and writer (ksc.h, kfile_lines.h) held to what its users read of it, on a
// classification this check writes and on a part's own
//
//   rows       a row of a channel is read with its answer, its word as a number and its question; a who line is no row
//   replaced   the rows of a question are let go, the rows put go after the channel's last row, and the channel's
//              counts are taken again over the rows it then holds; traced, the rows let go that no row put gives
//              again are handed back
//   who        what is put goes after a who line of the code that put it, the who before it written again after it
//              where lines follow, and a who line left over no line is let go
//   a part's   every cost row of the part's .ksc named on the command line put again in its place, in the order the
//   file
//              holds them, leaves the file as it was under one who line of the code that put them
//
//     ksc_check <folder> [<a part's .ksc>]
//
// It runs in a query run (qry_run.sh): every file it writes is handed to the run's writer.
#include "../../../../../../../src/cu/includes/file_defs/ksc/ksc.h"

#include "../../../../../../../src/cu/includes/parsers/who.h"
#include "verba_scribo/verba_scribo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int held, const char *what)
{
    s_checks += 1u;
    if (held == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

// the file joined as a check reads it and as it stood before a write, and each of those with its who lines left out
static char s_text_bytes[QRY_WRITER_RING_BYTES / 4ull];
static char s_before_bytes[QRY_WRITER_RING_BYTES / 4ull];
static char s_text_rows_bytes[QRY_WRITER_RING_BYTES / 4ull];
static char s_before_rows_bytes[QRY_WRITER_RING_BYTES / 4ull];

// the `written` bytes at `bytes` ended, of `room`: `bytes`, or NULL where they did not fit
static const char *ended(char *bytes, size_t room, size_t written)
{
    if (!verba_finis.ok(&(VerbaFinisCfg){.cap = room, .at = written}))
    {
        return NULL;
    }
    verba_finis.finish(&(VerbaFinisCfg){.out = bytes, .cap = room, .at = written});
    return bytes;
}

// `file`'s lines joined, each ended by a newline, into the `room` bytes at `bytes`: `bytes`, or NULL where they did not
// fit
static const char *joined(const KscFile *file, char *bytes, size_t room)
{
    size_t written = 0u;
    for (unsigned int at = 0u; at < file->lines; at += 1u)
    {
        written = verba_textus.put(&(VerbaTextusCfg){.out = bytes, .cap = room, .at = written, .text = file->line[at]});
        written = verba_littera.ch(&(VerbaLitteraCfg){.out = bytes, .cap = room, .at = written, .ch = '\n'});
    }
    return ended(bytes, room, written);
}

// `text` with its who lines left out, into the `room` bytes at `bytes`: `bytes`, or NULL where it did not fit
static const char *without_who(const char *text, char *bytes, size_t room)
{
    size_t written = 0u;
    for (const char *line = text; (line != NULL) && (*line != '\0');)
    {
        const char *const end = strchr(line, '\n');
        const size_t length = (end != NULL) ? (size_t)(end - line) : strlen(line);
        if (strncmp(line, "who ", 4u) != 0)
        {
            written = verba_textus.put_n(
                &(VerbaTextusCfg){.out = bytes, .cap = room, .at = written, .text = line, .text_len = length});
            written = verba_littera.ch(&(VerbaLitteraCfg){.out = bytes, .cap = room, .at = written, .ch = '\n'});
        }
        line = (end != NULL) ? (end + 1) : (line + length);
    }
    return ended(bytes, room, written);
}

// 1 where every cost row of `file` stands under a who line whose code is `code`
static int cost_rows_under(const KscFile *file, const char *code)
{
    static KscRow s_read;
    char whose[WHO_CODE_LETTERS + 8u];
    whose[0] = '\0';
    for (unsigned int at = 0u; at < file->lines; at += 1u)
    {
        if (strncmp(file->line[at], "who ", 4u) == 0)
        {
            snprintf(whose, sizeof(whose), "%s", file->line[at] + 4u);
            continue;
        }
        if (ksc_row_read(file->line[at], "run", &s_read) && (strncmp(s_read.question, "cost ", 5u) == 0) &&
            (strcmp(whose, code) != 0))
        {
            return 0;
        }
    }
    return 1;
}

static const char *const s_written = "ksc part\n"
                                     "count run      answers  3\n"
                                     "count run      nothing  0\n"
                                     "run answers 00000001 ask 1 2 3 4 5 6\n"
                                     "run answers 00000005 stall IADD3 IMAD\n"
                                     "run answers 00000007 stall LDG STG\n"
                                     "field stall 105 4\n";

static KscFile s_file;

int main(int count, char **words)
{
    if (count < 2)
    {
        printf("ksc_check <folder> [<a part's .ksc>]\n");
        return 2;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/ksc_check.ksc", words[1]);
    if (qry_member_filed(path, s_written, strlen(s_written)) == 0ull)
    {
        printf("  %s could not be written: no run's writer runs, or it stands outside the tree\n", path);
        return 2;
    }
    check_that(ksc_read(&s_file, path) && (s_file.lines == 7u), "a .ksc is read, a line each");
    static KscRow s_row;
    check_that(ksc_row_read(s_file.line[4], "run", &s_row) && (strcmp(s_row.answer, "answers") == 0) &&
                   (s_row.value == 5ull) && (strcmp(s_row.question, "stall IADD3 IMAD") == 0),
               "a row is read with its answer, its word as a number and its question");
    check_that(!ksc_row_read(s_file.line[1], "run", &s_row) && !ksc_row_read(s_file.line[6], "run", &s_row),
               "a count and a field are no rows");

    const char *const stalls[2] = {"run answers 00000004 stall IADD3 IMAD", "run nothing 00000000 stall FFMA FADD"};
    const Who put = WHO_HERE;
    check_that(ksc_rows_replaced_by(&s_file, "run", "stall", stalls, 2u, put), "a question's rows are replaced");
    char code[WHO_CODE_LETTERS + 1u];
    who_code(put, code, sizeof(code));
    char expected[1024];
    snprintf(expected, sizeof(expected),
             "ksc part\n"
             "count run      answers  2\n"
             "count run      nothing  1\n"
             "run answers 00000001 ask 1 2 3 4 5 6\n"
             "who %s\n"
             "run answers 00000004 stall IADD3 IMAD\n"
             "run nothing 00000000 stall FFMA FADD\n"
             "who -\n"
             "field stall 105 4\n",
             code);
    const char *text = joined(&s_file, s_text_bytes, sizeof(s_text_bytes));
    check_that((text != NULL) && (strcmp(text, expected) == 0),
               "the rows put after the channel's last row kept, under a who line of the code that put them, the who "
               "before them, none, written again after them, and the channel's counts taken again");
    check_that(ksc_row_read(s_file.line[4], "run", &s_row) == 0, "a who line is no row");
    Who there;
    check_that(kfile_lines_who_at(&s_file, 5u, &there) && (there.source == put.source) && (there.line == put.line),
               "the who in force over the rows put is the code that put them");
    check_that(!kfile_lines_who_at(&s_file, 8u, &there), "and past them no who is in force, as before");

    const char *const again[1] = {"run answers 00000009 stall LDG STG"};
    const Who later = WHO_HERE;
    check_that(ksc_rows_replaced_by(&s_file, "run", "stall", again, 1u, later), "the rows are replaced again");
    text = joined(&s_file, s_text_bytes, sizeof(s_text_bytes));
    char later_code[WHO_CODE_LETTERS + 1u];
    who_code(later, later_code, sizeof(later_code));
    snprintf(expected, sizeof(expected),
             "ksc part\n"
             "count run      answers  2\n"
             "count run      nothing  0\n"
             "run answers 00000001 ask 1 2 3 4 5 6\n"
             "who %s\n"
             "run answers 00000009 stall LDG STG\n"
             "who -\n"
             "field stall 105 4\n",
             later_code);
    check_that((text != NULL) && (strcmp(text, expected) == 0),
               "and the who line left over no line by the rows let go is let go");
    check_that(ksc_write(&s_file, path) == 1, "the .ksc is written through the run's writer");
    check_that(ksc_read(&s_file, path) && (s_file.lines == 8u), "and read back, line for line");
    ksc_free(&s_file);

    // the rows let go that no row put gives again, handed back for a .krf to keep
    static KscFile s_traced;
    ksc_line_add(&s_traced, "ksc part");
    ksc_line_add(&s_traced, "run answers 00000005 stall IADD3 IMAD");
    ksc_line_add(&s_traced, "run answers 00000007 stall LDG STG");
    const char *const traced_rows[2] = {"run answers 00000005 stall IADD3 IMAD", "run answers 00000009 stall FFMA"};
    KfileLines removed = {0};
    check_that(ksc_rows_replaced_traced_by(&s_traced, "run", "stall", traced_rows, 2u, WHO_HERE, &removed),
               "a question's rows are replaced with the rows let go traced");
    check_that((removed.lines == 1u) && (strcmp(removed.line[0], "run answers 00000007 stall LDG STG") == 0),
               "the row let go that no row put gives again is handed back, as the file held it");
    kfile_lines_free(&removed);
    check_that(ksc_rows_replaced_traced_by(&s_traced, "run", "stall", traced_rows, 2u, WHO_HERE, &removed) &&
                   (removed.lines == 0u),
               "and none where every row let go is put again");
    kfile_lines_free(&removed);
    ksc_free(&s_traced);

    if (count >= 3)
    {
        check_that(ksc_read(&s_file, words[2]) && (s_file.lines != 0u), "a part's .ksc reads");
        const char *const before = joined(&s_file, s_before_bytes, sizeof(s_before_bytes));
        const unsigned int lines = s_file.lines;
        const Who costs = WHO_HERE;
        unsigned int replaced = 0u;
        static char s_rows[4096][128];
        unsigned int rows = 0u;
        for (unsigned int at = 0u; (at < lines) && (rows < 4096u); at += 1u)
        {
            if (ksc_row_read(s_file.line[at], "run", &s_row) && (strncmp(s_row.question, "cost ", 5u) == 0))
            {
                snprintf(s_rows[rows], sizeof(s_rows[rows]), "%s", s_file.line[at]);
                rows += 1u;
            }
        }
        for (unsigned int at = 0u; at < rows; at += 1u)
        {
            char asked[64];
            ksc_row_read(s_rows[at], "run", &s_row);
            // the words a cost row's question begins with, the form's: `cost`, its low word and its high word, sixteen
            // hex digits each, a space between
            snprintf(asked, sizeof(asked), "%.*s", 38, s_row.question);
            const char *const one[1] = {s_rows[at]};
            replaced += ksc_rows_replaced_by(&s_file, "run", asked, one, 1u, costs) ? 1u : 0u;
        }
        who_code(costs, code, sizeof(code));
        const char *const after = joined(&s_file, s_text_bytes, sizeof(s_text_bytes));
        // the rows as they were, the who lines aside, and every cost row under the who that put it again
        const char *const before_rows = without_who(before, s_before_rows_bytes, sizeof(s_before_rows_bytes));
        const char *const after_rows = without_who(after, s_text_rows_bytes, sizeof(s_text_rows_bytes));
        const int same = (before_rows != NULL) && (after_rows != NULL) && (strcmp(before_rows, after_rows) == 0);
        const int under = (after != NULL) && cost_rows_under(&s_file, code);
        printf("  %s: %u lines, %u cost rows put again in their places, %s\n", words[2], lines, replaced,
               (same && under) ? "as it was under one who line" : "DIFFERING");
        check_that((rows != 0u) && (replaced == rows) && same && under,
                   "a part's cost rows put again in the order it holds them leave it as it was under one who line");
        ksc_free(&s_file);
    }

    printf("ksc_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
