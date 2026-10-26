// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_rows_check.c: a text of the run's table of texts checked against table_text.h
//
//   texts      a text's number is the 64-bit FNV-1a hash of its bytes, its row handed once however often this process
//              numbers it: of no who, its number, its length and its bytes filled out to a whole word with zeros; an
//              empty text has no number
//   bare       a row of check_rows, a table with no oracle and no head, is handed to the run, which kdecode_check.sh
//              reads raw and decoded
//
//     qry_rows_check
//
// It runs in a query run (qry_run.sh).
#include "../../../../../../../src/cu/includes/file_defs/qry/writer/qry_member.h"

#include "../../../../../../../src/cu/transpiler/lstar/protocol/table_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int passed, const char *what)
{
    s_checks += 1u;
    if (passed == 0)
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
    if (qry_member_run() == NULL)
    {
        printf("qry_rows_check: no run's writer runs\n");
        return 2;
    }

    // texts
    const char *const text = "a text the check numbers twice";
    const unsigned long long number = table_text(text, strlen(text));
    check_that(number == check_hash(text), "texts: a text's number is the FNV-1a hash of its bytes");
    check_that(table_text(text, strlen(text)) == number, "texts: numbered again, it has the same number");
    check_that(table_text(text, 0ull) == 0ull, "texts: an empty text has no number");
    // the table's rows walked, each its opening and its bytes filled out to a whole word, and this text's counted
    unsigned int rows = 0u;
    int whole = 1;
    int filled = 1;
    static unsigned char s_texts[QRY_WRITER_RING_BYTES / 2ull];
    const unsigned char *const bytes = s_texts;
    qry_member_settled();
    const unsigned long long size = qry_member_gathered(TABLE_TEXTS, s_texts, sizeof(s_texts));
    const int joined = (size != QRY_MEMBER_UNREAD);
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
            whole = (opening[0] == 0ull) && (opening[1] == 0ull) && (opening[3] == strlen(text)) &&
                    (memcmp(bytes + at + sizeof(opening), text, strlen(text)) == 0);
            for (unsigned long long place = opening[3]; whole && (place < padded); place += 1ull)
            {
                filled = filled && (bytes[at + sizeof(opening) + place] == 0u);
            }
        }
        at += sizeof(opening) + padded;
    }
    check_that(joined && whole && (rows == 1u),
               "texts: the table contains its row once, of no who, its number, its length and its bytes");
    check_that(filled, "texts: and the bytes filled out to a whole word with zeros");

    // bare
    const unsigned long long row[3] = {1ull, 2ull, 3ull};
    check_that(qry_member_append("check_rows", row, sizeof(row), QRY_ORDINARY) != 0ull,
               "bare: a row of a table with no oracle and no head is handed to the run");

    printf("qry_rows_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
