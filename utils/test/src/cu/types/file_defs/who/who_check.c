// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// who_check.c: a who held to what the who table gives (qry_buffer.h, QryWho)
//
//   here       the who of a line of this file numbers this file, gives the line, and numbers this program's source
//   code       a who's code reads back as the who it was made from, and a code of other letters is none
//   named      a who named through the table gives this file's path and line, and the program's source
//   filed      a file written through the run's writer is followed in the run by its row of the table of files: the
//              who that wrote it, its sequence and its path's number in the table of texts
//
//     who_check <folder>
//
// It runs in a query run (qry_run.sh), under the top of the tree QRY_TOP names, the folder within it.
#include "../../../../../../../src/cu/types/file_defs/qry/qry_buffer.h"

#include <stdio.h>
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

int main(int count, char **words)
{
    if (count < 2)
    {
        printf("who_check <folder>\n");
        return 2;
    }
    const unsigned int line = (unsigned int)__LINE__ + 1u;
    const QryWho here = QRY_WHO_HERE;
    check_that(here.source != 0u, "the who of a line of this file numbers this file");
    check_that(here.line == line, "and gives its line");
    check_that(here.program == here.source, "and numbers this program's source, this file");
    check_that(here.nanoseconds != 0ull, "and gives the time");

    char code[QRY_WHO_CODE_LETTERS + 1u];
    qry_who_code(here, code, sizeof(code));
    QryWho again;
    check_that(qry_who_read(code, &again) && (again.source == here.source) && (again.line == here.line) &&
                   (again.program == here.program) && (again.nanoseconds == here.nanoseconds),
               "a who's code reads back as the who it was made from");
    check_that(!qry_who_read("not a code", &again), "a code of other letters is none");

    char named[2400];
    char expected[128];
    snprintf(expected, sizeof(expected), "utils/test/src/cu/types/file_defs/who/who_check.c:%u ", line);
    check_that(qry_who_named(here, named, sizeof(named)) && (strncmp(named, expected, strlen(expected)) == 0),
               "a who named gives this file's path and line");
    check_that(strstr(named, " utils/test/src/cu/types/file_defs/who/who_check.c 20") != NULL,
               "and the program's source, and the year");
    printf("  %s: %s\n", code, named);

    QryBuffer *const run = qry_run();
    char path[1024];
    snprintf(path, sizeof(path), "%s/who_check.txt", words[1]);
    const unsigned int filed_line = (unsigned int)__LINE__ + 1u;
    const unsigned long long sequence = qry_filed(run, path, "who\n", 4u);
    check_that(sequence != 0ull, "a file is written through the run's writer");
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    check_that(qry_latest(run, QRY_FILES_TABLE, &bytes, &size, NULL) && (size == (4u * sizeof(unsigned long long))),
               "and followed in the run by its row of the table of files");
    // the row: the who's two words, the write's sequence and its path's number in the table of texts
    unsigned long long filed[4] = {0ull, 0ull, 0ull, 0ull};
    if (size == sizeof(filed))
    {
        memcpy(filed, bytes, sizeof(filed));
    }
    const QryWho wrote = qry_who_of_words(filed);
    check_that((filed[2] == sequence) && (wrote.source == here.source) && (wrote.line == filed_line) &&
                   (wrote.program == here.program),
               "its sequence and the who that wrote it, this file at the line of the write");
    char named_path[1024];
    check_that(qry_file_named(path, named_path, sizeof(named_path)) &&
                   (filed[3] == qry_row_text(run, named_path, strlen(named_path))),
               "and its path, by its number in the table of texts");

    printf("who_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
