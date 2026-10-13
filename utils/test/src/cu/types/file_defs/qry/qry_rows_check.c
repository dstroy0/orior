// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_rows_check.c: a row of a table of the run and a text of its table of texts held to qry_buffer.h
//
//   rows       a row handed is a blob of its table's name, the who of the code that handed it, then its columns as
//              handed; a row of more columns than a row holds is not handed
//   texts      a text's number is the 64-bit FNV-1a hash of its bytes, its row handed once however often this process
//              numbers it: of no who, its number, its length and its bytes filled out to a whole word with zeros; an
//              empty text has no number
//
//     qry_rows_check
//
// It runs in a query run (qry_run.sh).
#include "../../../../../../../src/cu/types/file_defs/qry/qry_buffer.h"

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

// the 64-bit FNV-1a hash of the text `text`
static unsigned long long check_hash(const char *text)
{
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (const char *at = text; *at != '\0'; at += 1)
    {
        hash = (hash ^ (unsigned char)*at) * 0x100000001b3ull;
    }
    return hash;
}

int main(void)
{
    QryBuffer *const run = qry_run();
    if (run == NULL)
    {
        printf("qry_rows_check: no run's writer runs\n");
        return 2;
    }

    // rows
    const unsigned long long columns[3] = {7ull, 0x8000000000000001ull, 0ull};
    const QryWho who = QRY_WHO_HERE;
    check_that(qry_row_handed_by(run, "check_rows", columns, 3u, QRY_ORDINARY, who) == 1,
               "rows: a row of three columns is handed");
    unsigned long long many[QRY_ROW_COLUMNS + 1u];
    memset(many, 0, sizeof(many));
    check_that(qry_row_handed(run, "check_rows", many, QRY_ROW_COLUMNS + 1u) == 0,
               "rows: a row of more columns than a row holds is not handed");
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    unsigned long long words[5];
    qry_who_words(who, words);
    const unsigned long long expected[5] = {words[0], words[1], columns[0], columns[1], columns[2]};
    check_that(qry_joined(run, "check_rows", &bytes, &size) && (size == sizeof(expected)) &&
                   (memcmp(bytes, expected, sizeof(expected)) == 0),
               "rows: the table holds the one row, its who's two words, then its columns as handed");
    free(bytes);

    // texts
    const char *const said = "a text the check numbers twice";
    const unsigned long long number = qry_row_text(run, said, strlen(said));
    check_that(number == check_hash(said), "texts: a text's number is the FNV-1a hash of its bytes");
    check_that(qry_row_text(run, said, strlen(said)) == number, "texts: numbered again, it has the same number");
    check_that(qry_row_text(run, said, 0ull) == 0ull, "texts: an empty text has no number");
    bytes = NULL;
    size = 0ull;
    // the table's rows walked, each its opening and its bytes filled out to a whole word, and this text's counted
    unsigned int rows = 0u;
    int whole = 1;
    int filled = 1;
    const int joined = qry_joined(run, QRY_TEXTS_TABLE, &bytes, &size);
    for (unsigned long long at = 0ull; joined && whole && (at < size);)
    {
        unsigned long long opening[4];
        whole = ((size - at) >= sizeof(opening));
        if (!whole)
        {
            break;
        }
        memcpy(opening, bytes + at, sizeof(opening));
        const unsigned long long padded = ((opening[3] + 7ull) / 8ull) * 8ull;
        whole = ((size - at - sizeof(opening)) >= padded);
        if (whole && (opening[2] == number))
        {
            rows += 1u;
            whole = (opening[0] == 0ull) && (opening[1] == 0ull) && (opening[3] == strlen(said)) &&
                    (memcmp(bytes + at + sizeof(opening), said, strlen(said)) == 0);
            for (unsigned long long place = opening[3]; whole && (place < padded); place += 1ull)
            {
                filled = filled && (bytes[at + sizeof(opening) + place] == 0u);
            }
        }
        at += sizeof(opening) + padded;
    }
    check_that(joined && whole && (rows == 1u),
               "texts: the table holds its row once, of no who, its number, its length and its bytes");
    check_that(filled, "texts: and the bytes filled out to a whole word with zeros");
    free(bytes);

    printf("qry_rows_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
