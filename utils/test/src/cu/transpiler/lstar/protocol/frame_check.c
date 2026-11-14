// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_check.c: the frame's queries checked against what reads them (Open 6.3)
//
//   walk       the halving walk reads the greatest number a counted relation is true at, querying fewer times than the
//              span, and 0 where nothing past 0 is true
//   head       orior_krs_write's two head lines come from the .ksc's rows, each left out where its row is missing and
//              named
//   opening    the kernel mode's place comes from the .ksc's row, 0 where none is, and its probe place from the
//              probe place's row, none where none is
//   fixed      a register's reading at the probe place is classified from its values over the cases, each type once,
//              and orior_krs_write's fixed lines come from the .ksc's rows, the ruleset's own line retained where no
//              row is, each left out where it has neither and named
//   frame      orior_krs_write's frame lines come from the table of frame forms and the named ruleset's parameters, the
//              writers of one placed text being one, the ruleset's own line retained where the run passes none and
//              stored in the .krf where a written one replaces it
//
//     frame_check <folder>
//
// It runs in a query run (qry_run.sh), under the top of the tree QRY_TOP names, the folder within it.
#include "../../../../../../../includes/file_defs/khw/khw_machine_write/opening.h"
#include "../../../../../../../includes/file_defs/krf/krf.h"
#include "../../../../../../../includes/file_defs/krs/krs_write/fixed.h"
#include "../../../../../../../includes/file_defs/krs/krs_write/frame.h"
#include "../../../../../../../includes/file_defs/krs/krs_write/head.h"
#include "../../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../../../../src/cu/transpiler/lstar/protocol/table/identity/fixed_read.h"
#include "../../../../../../../src/cu/transpiler/lstar/protocol/table/identity/frame_walk.h"
#include "verba_scribo/verba_linea.h"
#include <stdio.h>
#include <string.h>

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

// a relation that is true at every number up to its bound, counting how often it is queried
typedef struct
{
    unsigned long long bound;
    unsigned int queries;
} Counted;

static int counted_matches(unsigned long long number, void *context)
{
    Counted *const counted = (Counted *)context;
    counted->queries += 1u;
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
    check_that(frame_greatest(counted_matches, &five, 0ull, 32ull) == 5ull,
               "the walk reads the greatest number a relation is true at");
    check_that(five.queries <= 6u, "and queries no more times than the span's halvings");
    Counted none = {0ull, 0u};
    check_that(frame_greatest(counted_matches, &none, 0ull, 32ull) == 0ull, "and 0 where nothing past 0 is true");
    Counted all = {31ull, 0u};
    check_that(frame_greatest(counted_matches, &all, 0ull, 32ull) == 31ull,
               "and the number before the falsy end where every one before it is true");

    // a .ksc written through the run's writer with both rows, with the container's row alone and with none
    char path[1024];
    snprintf(path, sizeof(path), "%s/frame_check_both.ksc", words[1]);
    const char *const both = "ksc part\nrun answers 00000001 container returned\nrun answers 00000001 opening least\n";
    check_that(qry_member_filed(path, both, strlen(both)) != 0ull, "a .ksc with both rows is written");
    static char s_head_bytes[QRY_WRITER_RING_BYTES / 2ull];
    VerbaLinea head = {s_head_bytes, sizeof(s_head_bytes), 0ull};
    char missing[128];
    check_that(krs_head_written(head.out, head.capacity, &head.at, path, missing, sizeof(missing)) == 2u,
               "both head lines come from both rows");
    check_that((mmgr_line_finish(&head) != 0u) && (strcmp(head.out, "toolchain cubin_write\nheader container\n") == 0),
               "as toolchain cubin_write then header container");
    check_that(missing[0] == '\0', "and nothing is missing");
    snprintf(path, sizeof(path), "%s/frame_check_one.ksc", words[1]);
    const char *const one = "ksc part\nrun answers 00000001 container returned\n";
    check_that(qry_member_filed(path, one, strlen(one)) != 0ull, "a .ksc with the container's row alone is written");
    static char s_one_head_bytes[QRY_WRITER_RING_BYTES / 2ull];
    VerbaLinea one_head = {s_one_head_bytes, sizeof(s_one_head_bytes), 0ull};
    check_that(krs_head_written(one_head.out, one_head.capacity, &one_head.at, path, missing, sizeof(missing)) == 1u,
               "one line comes from one row");
    check_that((mmgr_line_finish(&one_head) != 0u) && (strcmp(one_head.out, "toolchain cubin_write\n") == 0),
               "the toolchain's");
    check_that(strcmp(missing, "header") == 0, "and the header is named missing");
    snprintf(path, sizeof(path), "%s/frame_check_none.ksc", words[1]);
    const char *const none_text = "ksc part\n";
    check_that(qry_member_filed(path, none_text, strlen(none_text)) != 0ull, "a .ksc with no row is written");
    static char s_none_head_bytes[QRY_WRITER_RING_BYTES / 2ull];
    VerbaLinea none_head = {s_none_head_bytes, sizeof(s_none_head_bytes), 0ull};
    check_that(krs_head_written(none_head.out, none_head.capacity, &none_head.at, path, missing, sizeof(missing)) == 0u,
               "no line comes from no row");
    check_that(strcmp(missing, "toolchain, header") == 0, "and both are named missing");

    // the place the least opening begins at, read from the same three files
    snprintf(path, sizeof(path), "%s/frame_check_both.ksc", words[1]);
    check_that(khw_opening_read(path) == 1u, "the place the least opening begins at is the row's word");
    snprintf(path, sizeof(path), "%s/frame_check_one.ksc", words[1]);
    check_that(khw_opening_read(path) == 0u, "and 0 where the .ksc contains no such row");
    snprintf(path, sizeof(path), "%s/frame_check_absent.ksc", words[1]);
    check_that(khw_opening_read(path) == 0u, "and 0 where there is no .ksc");

    // the probe place, read from the .ksc's row where one is
    snprintf(path, sizeof(path), "%s/frame_check_probe.ksc", words[1]);
    const char *const place_row = "ksc part\nrun answers 00000001 opening least\nrun answers 00000010 probe place\n";
    check_that(qry_member_filed(path, place_row, strlen(place_row)) != 0ull,
               "a .ksc with the probe place's row is written");
    check_that(khw_probe_place_read(path) == 16u, "the probe place is the row's word");
    snprintf(path, sizeof(path), "%s/frame_check_both.ksc", words[1]);
    check_that(khw_probe_place_read(path) == KHW_PROBE_PLACE_NONE, "and none where the .ksc contains no such row");
    snprintf(path, sizeof(path), "%s/frame_check_absent.ksc", words[1]);
    check_that(khw_probe_place_read(path) == KHW_PROBE_PLACE_NONE, "and none where there is no .ksc");

    // a register's reading at the probe place, classified from its values over eight cases with the case's words made
    // up
    unsigned long long values[8];
    unsigned int first[8];
    unsigned int second[8];
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        first[at] = 0x100u + (at * 3u);
        second[at] = 0x200u + (at * 5u);
        values[at] = at;
    }
    check_that(fixed_type(values, first, second, 8u) == FIXED_LANE_NUMBER,
               "values that are their case's number are the lane's number");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        values[at] = 0x7ffff000ull + (16ull * at);
    }
    check_that(fixed_type(values, first, second, 8u) == FIXED_RECORD, "values stepping 16 a case are the record");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        values[at] = 0xfffffff0ull + (16ull * at);
    }
    check_that(fixed_type(values, first, second, 8u) == FIXED_RECORD,
               "and still the record where the low word wraps past the 32-bit end");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        values[at] = 0x7ffff000ull + (32ull * at);
    }
    check_that(fixed_type(values, first, second, 8u) == FIXED_INDEX, "values stepping 32 a case are the index");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        values[at] = first[at];
    }
    check_that(fixed_type(values, first, second, 8u) == FIXED_CASE_WORD_0, "values that are the case's first word");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        values[at] = second[at];
    }
    check_that(fixed_type(values, first, second, 8u) == FIXED_CASE_WORD_1, "and its second");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        values[at] = 7ull;
    }
    check_that(fixed_type(values, first, second, 8u) == FIXED_NOTHING, "one constant is nothing fixed");
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        values[at] = 0ull;
    }
    check_that(fixed_type(values, first, second, 8u) == FIXED_NOTHING, "and so is 0");
    check_that(fixed_type(values, first, second, 1u) == FIXED_NOTHING, "and so is one case alone");
    check_that((strcmp(fixed_type_question(FIXED_RECORD), "fixed record") == 0) &&
                   (strcmp(fixed_type_question(FIXED_CASE_WORD_1), "case word 1") == 0) &&
                   (fixed_type_question(FIXED_NOTHING)[0] == '\0'),
               "a type's row question names it, and nothing's is empty");

    // orior_krs_write's fixed lines from a .ksc with the four rows and the field, with the zero row alone and no field,
    // and with none
    snprintf(path, sizeof(path), "%s/frame_check_fixed.ksc", words[1]);
    const char *const four = "ksc part\nfield rd 16 8\nrun answers 000000ff fixed zero\nrun answers 00000000 fixed "
                             "lane_number\nrun answers 00000004 fixed record\nrun answers 00000002 fixed index\n";
    check_that(qry_member_filed(path, four, strlen(four)) != 0ull, "a .ksc with the four fixed rows is written");
    static char s_fixed_head_bytes[QRY_WRITER_RING_BYTES / 2ull];
    VerbaLinea fixed_head = {s_fixed_head_bytes, sizeof(s_fixed_head_bytes), 0ull};
    check_that(krs_fixed_written(fixed_head.out, fixed_head.capacity, &fixed_head.at, path, NULL, missing,
                                 sizeof(missing)) == 4u,
               "four fixed lines come from four rows");
    check_that((mmgr_line_finish(&fixed_head) != 0u) &&
                   (strcmp(fixed_head.out,
                           "fixed zero = RZ\nfixed lane_number = R0\nfixed record = R4\nfixed index = R2\n") == 0),
               "zero as RZ, the field's top, then lane_number, record and index as R<n>");
    check_that(missing[0] == '\0', "and nothing is missing");
    snprintf(path, sizeof(path), "%s/frame_check_fixed_one.ksc", words[1]);
    const char *const one_fixed = "ksc part\nrun answers 00000009 fixed zero\n";
    check_that(qry_member_filed(path, one_fixed, strlen(one_fixed)) != 0ull,
               "a .ksc with the zero row alone and no field row is written");
    static char s_one_fixed_head_bytes[QRY_WRITER_RING_BYTES / 2ull];
    VerbaLinea one_fixed_head = {s_one_fixed_head_bytes, sizeof(s_one_fixed_head_bytes), 0ull};
    check_that(krs_fixed_written(one_fixed_head.out, one_fixed_head.capacity, &one_fixed_head.at, path, NULL, missing,
                                 sizeof(missing)) == 1u,
               "one fixed line comes from one row");
    check_that((mmgr_line_finish(&one_fixed_head) != 0u) && (strcmp(one_fixed_head.out, "fixed zero = R9\n") == 0),
               "as R9 where no field row gives the top");
    check_that(strcmp(missing, "lane_number, record, index") == 0, "and the other three are named missing");
    snprintf(path, sizeof(path), "%s/frame_check_none.ksc", words[1]);
    static char s_none_fixed_head_bytes[QRY_WRITER_RING_BYTES / 2ull];
    VerbaLinea none_fixed_head = {s_none_fixed_head_bytes, sizeof(s_none_fixed_head_bytes), 0ull};
    check_that(krs_fixed_written(none_fixed_head.out, none_fixed_head.capacity, &none_fixed_head.at, path, NULL,
                                 missing, sizeof(missing)) == 0u,
               "no fixed line comes from no row");
    check_that(strcmp(missing, "zero, lane_number, record, index") == 0, "and all four are named missing");

    // the lines the ruleset gave before are retained where no row replaces them, the queried four in their order and
    // the rest as given
    char ruleset_path[1024];
    snprintf(ruleset_path, sizeof(ruleset_path), "%s/frame_check_fixed.krs", words[1]);
    const char *const given = "krs 1\nruleset sass\nfixed zero = RZ\nfixed lane_number = R0\nfixed record = R1\nfixed "
                              "body = R3\nfixed carry = R12\n";
    check_that(qry_member_filed(ruleset_path, given, strlen(given)) != 0ull,
               "a ruleset with five fixed lines given before is written");
    snprintf(path, sizeof(path), "%s/frame_check_fixed_record.ksc", words[1]);
    const char *const record_row = "ksc part\nfield rd 16 8\nrun answers 00000004 fixed record\n";
    check_that(qry_member_filed(path, record_row, strlen(record_row)) != 0ull,
               "and a .ksc with the record's row alone");
    char removals[1024];
    snprintf(removals, sizeof(removals), "%s/frame_check_fixed_record.krf", words[1]);
    const char *const fresh = "krf frame_check_fixed_record\n";
    check_that(qry_member_filed(removals, fresh, strlen(fresh)) != 0ull, "and the .krf beside it, its head alone");
    static char s_head_line_bytes[QRY_WRITER_RING_BYTES / 2ull];
    VerbaLinea head_line = {s_head_line_bytes, sizeof(s_head_line_bytes), 0ull};
    check_that(krs_fixed_written(head_line.out, head_line.capacity, &head_line.at, path, ruleset_path, missing,
                                 sizeof(missing)) == 5u,
               "five fixed lines come from one row and four given lines");
    check_that((mmgr_line_finish(&head_line) != 0u) &&
                   (strcmp(head_line.out, "fixed zero = RZ\nfixed lane_number = R0\nfixed record = R4\nfixed body = "
                                          "R3\nfixed carry = R12\n") == 0),
               "the queried four in their order, the row's line where a row is and the given where none, then the "
               "rest as given");
    check_that(strcmp(missing, "index") == 0, "and the one with neither is named missing");
    static KrfFile s_removed;
    static KrfRow s_removal;
    check_that(krf_read(&s_removed, removals) && (s_removed.lines == 3u) &&
                   krf_row_read(s_removed.line[2], &s_removal) && (strcmp(s_removal.type, "krs") == 0) &&
                   (strcmp(s_removal.reason, "replaced") == 0) && (strcmp(s_removal.row, "fixed record = R1") == 0),
               "the given line the row replaced is stored in the .krf beside the .ksc, under a who line");
    krf_free(&s_removed);

    // the frame forms: the table's rows, eight frame relations', ten the ladder's predicate relations' and eight their
    // mirrors', and a form's line as the schema's form through a ruleset naming the eight
    check_that(krs_frame_read(NULL) == 27u,
               "the table of frame forms beside its source contains twenty-seven rows, the "
               "frame's nine, the tests' ten and their mirrors' eight");
    char named[1024];
    snprintf(named, sizeof(named), "%s/frame_check_named.krs", words[1]);
    const char *const naming =
        "krs 1\nruleset cu\nform return =     return;\\n\nform word_set to value =     {to} = (unsigned "
        "int){value};\\n\nform global_load_constant_word to address offset =     {to} = *(const unsigned int "
        "*)({address} + {offset}ull);\\n\nform record_store_word offset from =     *(unsigned int *)(record_address + "
        "{offset}ull) = {from};\\n\nform thread_idx_x to =     {to} = threadIdx.x;\\n\nform block_idx_x to =     {to} "
        "= "
        "blockIdx.x;\\n\nform exit_if where =     if ({where})\\n    {\\n        return;\\n    }\\n\nform block_dim_x "
        "to =     "
        "{to} = blockDim.x;\\n\nerr parameter_load_word to offset\n";
    check_that(qry_member_filed(named, naming, strlen(naming)) != 0ull,
               "a ruleset naming the schema's nine forms is written");
    const KrsFrameForm lane = {"frame.lane", "S2R {0}"};
    char line[1024];
    check_that(krs_frame_named(&lane, named, line, sizeof(line)) &&
                   (strcmp(line, "form thread_idx_x to = S2R {to}\\n\n") == 0),
               "a lane form is named thread_idx_x, its destination as to");
    const KrsFrameForm load = {"frame.load", "LD {2}, [{0}.64], {1}"};
    check_that(krs_frame_named(&load, named, line, sizeof(line)) &&
                   (strcmp(line, "form global_load_constant_word to address offset = LD {to}, [{address}.64], "
                                 "{offset}\\n\n") == 0),
               "a load form is named global_load_constant_word, its places as to, address and offset");
    const KrsFrameForm number = {"frame.number", "MOV {1}, {0}"};
    check_that(krs_frame_named(&number, named, line, sizeof(line)) &&
                   (strcmp(line, "form word_set to value = MOV {to}, {value}\\n\n") == 0),
               "a number form is named word_set, its destination as to and its number as value");
    const KrsFrameForm dim = {"frame.dim", "MOV {0}, c[0x0][0x0]"};
    check_that(krs_frame_named(&dim, named, line, sizeof(line)) &&
                   (strcmp(line, "form block_dim_x to = MOV {to}, c[0x0][0x0]\\n\n") == 0),
               "a dim form is named block_dim_x, its destination as to and the constant bank's operand as it is");
    const KrsFrameForm guarded = {"frame.exit", "@{0} EXIT"};
    check_that(krs_frame_named(&guarded, named, line, sizeof(line)) &&
                   (strcmp(line, "form exit_if where = @{where} EXIT\\n\n") == 0),
               "a guarded exit is named exit_if, its guard as where");
    const KrsFrameForm plain = {"frame.exit", "EXIT"};
    check_that(krs_frame_named(&plain, named, line, sizeof(line)) && (strcmp(line, "form return = EXIT\\n\n") == 0),
               "and an exit with no guard is named return");
    const KrsFrameForm parameter = {"frame.parameter", "MOV {1}, c[0x0][0x160+{0}]"};
    check_that(krs_frame_named(&parameter, named, line, sizeof(line)) &&
                   (strcmp(line, "form parameter_load_word to offset = MOV {to}, c[0x0][0x160+{offset}]\\n\n") == 0),
               "a parameter form is named parameter_load_word, its destination as to and its place among the "
               "parameters as offset");
    const KrsFrameForm stray = {"frame.load", "LD {2}, [{0}.64], {3}"};
    check_that(!krs_frame_named(&stray, named, line, sizeof(line)),
               "a text containing a place no parameter takes is not named");
    const KrsFrameForm unknown = {"frame.bank", "BANK {0}"};
    check_that(!krs_frame_named(&unknown, named, line, sizeof(line)), "and nor is a relation the table maps none of");

    // the frame lines written: two loads of one text are one line, a lane's line replaces the given one and the
    // given goes to the .krf, and a row the run passes none for retains its given line, in the table's order
    snprintf(ruleset_path, sizeof(ruleset_path), "%s/frame_check_frame.krs", words[1]);
    const char *const frame_given =
        "krs 1\nform thread_idx_x to = S2R {to}, SR_TID.X\\n\nform block_idx_x to = S2R {to}, SR_CTAID.X\\n\n";
    check_that(qry_member_filed(ruleset_path, frame_given, strlen(frame_given)) != 0ull,
               "a part's ruleset with two frame lines given before is written");
    snprintf(path, sizeof(path), "%s/frame_check_frame.ksc", words[1]);
    snprintf(removals, sizeof(removals), "%s/frame_check_frame.krf", words[1]);
    const char *const frame_fresh = "krf frame_check_frame\n";
    check_that(qry_member_filed(removals, frame_fresh, strlen(frame_fresh)) != 0ull,
               "and the .krf beside its .ksc, its head alone");
    const KrsFrameForm passed[4] = {load, {"frame.load", "LD {2}, [{0}.64], {1}"}, lane, guarded};
    static char s_frame_forms_bytes[QRY_WRITER_RING_BYTES / 2ull];
    VerbaLinea frame_forms = {s_frame_forms_bytes, sizeof(s_frame_forms_bytes), 0ull};
    check_that(krs_frame_written(frame_forms.out, frame_forms.capacity, &frame_forms.at, passed, 4u, named,
                                 ruleset_path, path) == 5u,
               "five lines come from two loads of one text, a lane, a guarded exit and the block's given line");
    check_that((mmgr_line_finish(&frame_forms) != 0u) &&
                   (strcmp(frame_forms.out, "form thread_idx_x to = S2R {to}\\n\nform block_idx_x to = S2R {to}, "
                                            "SR_CTAID.X\\n\nform global_load_constant_word to address offset = LD "
                                            "{to}, [{address}.64], {offset}\\n\nform return = EXIT\\n\nform exit_if "
                                            "where = @{where} EXIT\\n\n") == 0),
               "in the table's order: the lane's line, the block's given line, one load line, and return then exit_if "
               "from the one guarded exit");
    static KrfFile s_frame_removed;
    check_that(krf_read(&s_frame_removed, removals) && (s_frame_removed.lines == 3u) &&
                   krf_row_read(s_frame_removed.line[2], &s_removal) && (strcmp(s_removal.type, "krs") == 0) &&
                   (strcmp(s_removal.reason, "replaced") == 0) &&
                   (strcmp(s_removal.row, "form thread_idx_x to = S2R {to}, SR_TID.X\\n") == 0),
               "the given lane line the written one replaced is stored in the .krf, under a who line");
    krf_free(&s_frame_removed);

    // the mirror rows: an at-max form writes its own test and the greater-or-equal test with its words changed over,
    // and a test two rows write is one line, from its own relation's form where one is passed
    snprintf(named, sizeof(named), "%s/frame_check_tests.krs", words[1]);
    const char *const tests = "krs 1\nruleset cu\nform test_word_le where left right = {where} = ({left} <= "
                              "{right});\\n\nform test_word_ge where left right = {where} = ({left} >= {right});\\n\n";
    check_that(qry_member_filed(named, tests, strlen(tests)) != 0ull,
               "a ruleset naming the two tests of at max and at least is written");
    const KrsFrameForm at_max = {"ladder.at_max", "ISETP.LE.U32.AND {2}, PT, {0}, {1}, PT"};
    static char s_mirrored_bytes[QRY_WRITER_RING_BYTES / 2ull];
    VerbaLinea mirrored = {s_mirrored_bytes, sizeof(s_mirrored_bytes), 0ull};
    check_that(krs_frame_written(mirrored.out, mirrored.capacity, &mirrored.at, &at_max, 1u, named, NULL, path) == 2u,
               "two lines come from one at-max form");
    check_that((mmgr_line_finish(&mirrored) != 0u) &&
                   (strcmp(mirrored.out, "form test_word_le where left right = ISETP.LE.U32.AND {where}, PT, {left}, "
                                         "{right}, PT\\n\nform test_word_ge where left right = ISETP.LE.U32.AND "
                                         "{where}, PT, {right}, {left}, PT\\n\n") == 0),
               "its own test, and the greater-or-equal test with left and right changed over");
    const KrsFrameForm both_tests[2] = {at_max, {"ladder.at_least", "ISETP.GE.U32.AND {2}, PT, {0}, {1}, PT"}};
    static char s_own_bytes[QRY_WRITER_RING_BYTES / 2ull];
    VerbaLinea own = {s_own_bytes, sizeof(s_own_bytes), 0ull};
    check_that(krs_frame_written(own.out, own.capacity, &own.at, both_tests, 2u, named, NULL, path) == 2u,
               "an at-max and an at-least form write two lines, each test once");
    check_that((mmgr_line_finish(&own) != 0u) &&
                   (strcmp(own.out, "form test_word_le where left right = ISETP.LE.U32.AND {where}, PT, {left}, "
                                    "{right}, PT\\n\nform test_word_ge where left right = ISETP.GE.U32.AND {where}, "
                                    "PT, {left}, {right}, PT\\n\n") == 0),
               "each from its own relation's form");
    printf("frame_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
