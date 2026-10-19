// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// who_check.c: a who held to what the who table gives (who.h)
//
//   here       the who of a line of this file numbers this file, gives the line, and numbers this program's source
//   code       a who's code reads back as the who it was made from, and a code of other letters is none
//   named      a who named through the table gives this file's path and line, and the program's source
//   filed      a file written through the run's writer is the run's, read back as the run holds it
//
//     who_check <folder>
//
// It runs in a query run (qry_run.sh), the folder within the top of the tree.
#include "../../../../../../../src/cu/includes/file_defs/qry/writer/qry_member.h"

#include "../../../../../../../src/cu/includes/parsers/who.h"
#include "../../../../../../../src/cu/transpiler/lstar/protocol/table_text.h"
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
    const Who here = WHO_HERE;
    check_that(here.source != 0u, "the who of a line of this file numbers this file");
    check_that(here.line == line, "and gives its line");
    check_that(here.program == here.source, "and numbers this program's source, this file");
    check_that(here.time != 0ull, "and gives the time");

    char code[WHO_CODE_LETTERS + 1u];
    who_code(here, code, sizeof(code));
    Who again;
    check_that(who_read(code, &again) && (again.source == here.source) && (again.line == here.line) &&
                   (again.program == here.program) && (again.time == here.time),
               "a who's code reads back as the who it was made from");
    check_that(!who_read("not a code", &again), "a code of other letters is none");

    char named[2400];
    char expected[128];
    snprintf(expected, sizeof(expected), "utils/test/src/cu/types/file_defs/who/who_check.c:%u ", line);
    check_that(who_named(here, named, sizeof(named)) && (strncmp(named, expected, strlen(expected)) == 0),
               "a who named gives this file's path and line");
    check_that(strstr(named, " utils/test/src/cu/types/file_defs/who/who_check.c 20") != NULL,
               "and the program's source, and the year");
    printf("  %s: %s\n", code, named);

    char path[1024];
    snprintf(path, sizeof(path), "%s/who_check.txt", words[1]);
    check_that(qry_member_filed(path, "who\n", 4u) != 0ull, "a file is written through the run's writer");
    qry_member_settled();
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    check_that(qry_member_file_blob(path, &bytes, &size) && (size == 4u) && (memcmp(bytes, "who\n", 4u) == 0),
               "and is the run's, read back as the run holds it");

    printf("who_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
