// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krf_check.c: the .krf's reader and writer (krf.h, kfile_lines.h) held to what its users read of it, on a file of
// removals this check writes
//
//   query      the run's name, query<n>-<datetime>, is read from the run's own .qry
//   beside     the .krf beside a k-file is the same path with its extension .krf, whichever way the path is written
//   removed    a removal starts the file with its head where there is none, puts a row of the k-file's type, the
//              run's name, the reason and the row's own text under a who line, and a later removal goes after it
//   rows       a removed row is read with its four parts; the head and a who line are no rows
//
//     krf_check <folder>
//
// It runs in a query run (qry_run.sh): every file it writes is handed to the run's writer.
#include "../../../../../../../src/cu/includes/file_defs/krf/krf.h"

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

// the file joined as a check reads it
static char s_text_bytes[QRY_WRITER_RING_BYTES / 2ull];

// `file`'s lines joined, each ended by a newline, into s_text_bytes: s_text_bytes, or NULL where they did not fit
static const char *joined(const KrfFile *file)
{
    size_t written = 0u;
    for (unsigned int at = 0u; at < file->lines; at += 1u)
    {
        written = verba_textus.put(
            &(VerbaTextusCfg){.out = s_text_bytes, .cap = sizeof(s_text_bytes), .at = written, .text = file->line[at]});
        written = verba_littera.ch(
            &(VerbaLitteraCfg){.out = s_text_bytes, .cap = sizeof(s_text_bytes), .at = written, .ch = '\n'});
    }
    if (!verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_text_bytes), .at = written}))
    {
        return NULL;
    }
    verba_finis.finish(&(VerbaFinisCfg){.out = s_text_bytes, .cap = sizeof(s_text_bytes), .at = written});
    return s_text_bytes;
}

static KrfFile s_file;

int main(int count, char **words)
{
    if (count < 2)
    {
        printf("krf_check <folder>\n");
        return 2;
    }
    char query[KRF_QUERY];
    check_that(krf_query_named(query, sizeof(query)) && (strncmp(query, "query", 5u) == 0) &&
                   (strstr(query, ".qry") == NULL),
               "the run's name is read from its .qry, query<n>-<datetime>, without the extension");

    char beside[1024];
    krf_path_beside("a/b/sm_86.ksc", beside, sizeof(beside));
    check_that(strcmp(beside, "a/b/sm_86.krf") == 0, "the .krf beside a k-file is its path with the extension .krf");
    krf_path_beside("a.b\\c\\sass.krs", beside, sizeof(beside));
    check_that(strcmp(beside, "a.b\\c\\sass.krf") == 0, "whichever way the path is written, the last extension alone");
    krf_path_beside("sm_86", beside, sizeof(beside));
    check_that(strcmp(beside, "sm_86.krf") == 0, "and added where the path has none");

    // a file of this run's own, named by the run, fresh where the folder holds the files of earlier runs
    char path[1024];
    snprintf(path, sizeof(path), "%s/krf_check_%s.krf", words[1], query);
    const char *const first[1] = {"run answers 00000010 slot place"};
    const Who put = WHO_HERE;
    check_that(krf_removed_by(path, "ksc", "replaced", first, 1u, put), "a removal is kept in a .krf of the part");
    check_that(krf_read(&s_file, path) && (s_file.lines == 3u),
               "the file starts with its head, a who line and the row");
    char code[WHO_CODE_LETTERS + 1u];
    who_code(put, code, sizeof(code));
    char expected[2048];
    snprintf(expected, sizeof(expected),
             "krf krf_check_%s\n"
             "who %s\n"
             "removed ksc %s replaced run answers 00000010 slot place\n",
             query, code, query);
    const char *text = joined(&s_file);
    check_that((text != NULL) && (strcmp(text, expected) == 0),
               "the head names the part from the path, and the row its type, the run, the reason and the text");
    static KrfRow s_row;
    check_that(krf_row_read(s_file.line[2], &s_row) && (strcmp(s_row.type, "ksc") == 0) &&
                   (strcmp(s_row.query, query) == 0) && (strcmp(s_row.reason, "replaced") == 0) &&
                   (strcmp(s_row.row, "run answers 00000010 slot place") == 0),
               "a removed row is read with its four parts");
    check_that(!krf_row_read(s_file.line[0], &s_row) && !krf_row_read(s_file.line[1], &s_row),
               "the head and a who line are no rows");
    krf_free(&s_file);

    const char *const later_rows[2] = {"fixed record = R1",
                                       "form word_add to left right = IADD {to}, {left}, {right}\\n"};
    const Who later = WHO_HERE;
    check_that(krf_removed_by(path, "krs", "refused", later_rows, 2u, later), "a later removal is kept after it");
    check_that(krf_read(&s_file, path) && (s_file.lines == 6u), "under a who line of its own");
    char later_code[WHO_CODE_LETTERS + 1u];
    who_code(later, later_code, sizeof(later_code));
    snprintf(expected, sizeof(expected),
             "krf krf_check_%s\n"
             "who %s\n"
             "removed ksc %s replaced run answers 00000010 slot place\n"
             "who %s\n"
             "removed krs %s refused fixed record = R1\n"
             "removed krs %s refused form word_add to left right = IADD {to}, {left}, {right}\\n\n",
             query, code, query, later_code, query, query);
    text = joined(&s_file);
    check_that((text != NULL) && (strcmp(text, expected) == 0), "the earlier rows standing as they were");
    check_that(krf_row_read(s_file.line[5], &s_row) && (strcmp(s_row.type, "krs") == 0) &&
                   (strcmp(s_row.reason, "refused") == 0) && (strncmp(s_row.row, "form word_add", 13u) == 0),
               "and a form's line is read back whole");
    krf_free(&s_file);
    printf("krf_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
