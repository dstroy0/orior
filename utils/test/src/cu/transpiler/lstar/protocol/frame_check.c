// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_check.c: the frame's asks held to what reads them (The frame asked, Open 6.3)
//
//   walk       the halving walk reads the greatest number a counted relation holds at, asking fewer times than the
//              span, and 0 where nothing past 0 holds
//   head       krs_write's two head lines come from the .ksc's rows, each left out where its row is missing and named
//   opening    the kernel mode's place comes from the .ksc's row, 0 where none stands
//   fixed      a register's reading at the slot is classified from its values over the cases, each kind once, and
//              krs_write's fixed lines come from the .ksc's rows, each left out where its row is missing and named
//
//     frame_check <folder>
//
// It runs in a query run (qry_run.sh), under the top of the tree QRY_TOP names, the folder within it.
#include "../../../../../../../src/cu/transpiler/lstar/parser/krs_write/head.h"
#include "../../../../../../../src/cu/transpiler/lstar/protocol/table/identity/fixed_read.h"
#include "../../../../../../../src/cu/transpiler/lstar/protocol/table/identity/frame_walk.h"
#include "../../../../../../../src/cu/transpiler/vendor_bin_layouts/nvidia/khw_machine_write/opening.h"
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

// a relation that holds at every number up to its bound, counting how often it is asked
typedef struct
{
    unsigned long long bound;
    unsigned int asked;
} Counted;

static int counted_holds(unsigned long long number, void *context)
{
    Counted *const counted = (Counted *)context;
    counted->asked += 1u;
    return number <= counted->bound;
}

int main(int count, char **words)
{
    if (count < 2)
    {
        printf("frame_check <folder>\n");
        return 2;
    }
    Counted five = {5ull, 0u};
    check_that(frame_greatest(counted_holds, &five, 0ull, 32ull) == 5ull,
               "the walk reads the greatest number a relation holds at");
    check_that(five.asked <= 6u, "and asks no more times than the span's halvings");
    Counted none = {0ull, 0u};
    check_that(frame_greatest(counted_holds, &none, 0ull, 32ull) == 0ull, "and 0 where nothing past 0 holds");
    Counted all = {31ull, 0u};
    check_that(frame_greatest(counted_holds, &all, 0ull, 32ull) == 31ull,
               "and the number before the falsy end where every one before it holds");

    // a .ksc written through the run's writer with both rows, with the container's row alone and with none
    char path[1024];
    snprintf(path, sizeof(path), "%s/frame_check_both.ksc", words[1]);
    const char *const both = "ksc part\nrun answers 00000001 container returned\nrun answers 00000001 opening least\n";
    check_that(qry_filed(qry_run(), path, both, strlen(both)) != 0ull, "a .ksc with both rows is written");
    QryText head = {NULL, 0ull, 0ull, 0};
    char missing[128];
    check_that(krs_head_written(&head, path, missing, sizeof(missing)) == 2u, "both head lines come from both rows");
    check_that((head.bytes != NULL) && (strcmp(head.bytes, "toolchain cubin_write\nheader container\n") == 0),
               "as toolchain cubin_write then header container");
    check_that(missing[0] == '\0', "and nothing is missing");
    free(head.bytes);
    snprintf(path, sizeof(path), "%s/frame_check_one.ksc", words[1]);
    const char *const one = "ksc part\nrun answers 00000001 container returned\n";
    check_that(qry_filed(qry_run(), path, one, strlen(one)) != 0ull, "a .ksc with the container's row alone is written");
    QryText one_head = {NULL, 0ull, 0ull, 0};
    check_that(krs_head_written(&one_head, path, missing, sizeof(missing)) == 1u, "one line comes from one row");
    check_that((one_head.bytes != NULL) && (strcmp(one_head.bytes, "toolchain cubin_write\n") == 0),
               "the toolchain's");
    check_that(strcmp(missing, "header") == 0, "and the header is named missing");
    free(one_head.bytes);
    snprintf(path, sizeof(path), "%s/frame_check_none.ksc", words[1]);
    const char *const none_text = "ksc part\n";
    check_that(qry_filed(qry_run(), path, none_text, strlen(none_text)) != 0ull, "a .ksc with no row is written");
    QryText none_head = {NULL, 0ull, 0ull, 0};
    check_that(krs_head_written(&none_head, path, missing, sizeof(missing)) == 0u, "no line comes from no row");
    check_that(strcmp(missing, "toolchain, header") == 0, "and both are named missing");
    free(none_head.bytes);

    // the place the least opening begins at, read from the same three files
    snprintf(path, sizeof(path), "%s/frame_check_both.ksc", words[1]);
    check_that(khw_opening_read(path) == 1u, "the place the least opening begins at is the row's word");
    snprintf(path, sizeof(path), "%s/frame_check_one.ksc", words[1]);
    check_that(khw_opening_read(path) == 0u, "and 0 where the .ksc holds no such row");
    snprintf(path, sizeof(path), "%s/frame_check_absent.ksc", words[1]);
    check_that(khw_opening_read(path) == 0u, "and 0 where there is no .ksc");

    // a register's reading at the slot, classified from its values over eight cases with the case's words made up
    unsigned long long held[8];
    unsigned int first[8];
    unsigned int second[8];
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        first[at] = 0x100u + (at * 3u);
        second[at] = 0x200u + (at * 5u);
        held[at] = at;
    }
    check_that(fixed_kind(held, first, second, 8u) == FIXED_LANE_NUMBER,
               "values that are their case's number are the lane's number");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        held[at] = 0x7ffff000ull + (16ull * at);
    }
    check_that(fixed_kind(held, first, second, 8u) == FIXED_RECORD, "values stepping 16 a case are the record");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        held[at] = 0xfffffff0ull + (16ull * at);
    }
    check_that(fixed_kind(held, first, second, 8u) == FIXED_RECORD,
               "and still the record where the low word wraps past the 32-bit end");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        held[at] = 0x7ffff000ull + (32ull * at);
    }
    check_that(fixed_kind(held, first, second, 8u) == FIXED_INDEX, "values stepping 32 a case are the index");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        held[at] = first[at];
    }
    check_that(fixed_kind(held, first, second, 8u) == FIXED_CASE_WORD_0, "values that are the case's first word");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        held[at] = second[at];
    }
    check_that(fixed_kind(held, first, second, 8u) == FIXED_CASE_WORD_1, "and its second");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        held[at] = 7ull;
    }
    check_that(fixed_kind(held, first, second, 8u) == FIXED_NOTHING, "one constant is nothing fixed");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        held[at] = 0ull;
    }
    check_that(fixed_kind(held, first, second, 8u) == FIXED_NOTHING, "and so is 0");
    check_that(fixed_kind(held, first, second, 1u) == FIXED_NOTHING, "and so is one case alone");
    check_that((strcmp(fixed_kind_question(FIXED_RECORD), "fixed record") == 0) &&
                   (strcmp(fixed_kind_question(FIXED_CASE_WORD_1), "case word 1") == 0) &&
                   (fixed_kind_question(FIXED_NOTHING)[0] == '\0'),
               "a kind's row question names it, and nothing's is empty");
    printf("frame_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
