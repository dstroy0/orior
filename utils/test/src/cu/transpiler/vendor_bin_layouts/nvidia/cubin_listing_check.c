// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_listing_check.c: the cut of a cubin's listing into each kernel's held on a listing shaped as the vendor prints
// a cubin of two kernels
//
//   lines     a kernel's listing is its code section's instruction lines in order, with no other line
//   labels    a label the code names is put as the offset of the instruction it stands before, forward and back
//   end       a label after the last instruction stands at the end of the code
//   unset     a mark naming a label the section does not set is kept as printed
//   apart     a section no kernel is named for, and a kernel no section holds, are left out
//   info      a section's `.sectioninfo` and `.sectionflags` lines open no section of their own
//   note      a note the cubin carries for an instruction, `(*"BRANCH_TARGETS ..."*)`, is put as the one space a raw
//             read prints in its place
//
//     cubin_listing_check
#include "../../../../../../../src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_listing.h"

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

static const char s_listing[] = "\t.headerflags\t@\"EF_CUDA_64BIT_ADDRESS EF_CUDA_SM86\"\n"
                                "\t.section\t.nv.info.k_one,\"\",@\"SHT_CUDA_INFO\"\n"
                                "        /*0000*/ \t.byte\t0x04, 0x11\n"
                                "//--------------------- .text.k_one --------------------------\n"
                                "\t.section\t.text.k_one,\"ax\",@progbits\n"
                                "\t.sectionflags\t@\"\"\n"
                                "\t.sectioninfo\t@\"SHI_REGISTERS=24\"\n"
                                "\t.align\t128\n"
                                "        .global         k_one\n"
                                "k_one:\n"
                                ".text.k_one:\n"
                                "        /*0000*/                   IMAD.MOV.U32 R1, RZ, RZ, c[0x0][0x28] ;\n"
                                "        /*0010*/                   BSSY B6, `(.L_x_0) ;\n"
                                "        /*0020*/               @P0 BRA `(.L_x_1) ;\n"
                                ".L_x_0:\n"
                                "        /*0030*/                   EXIT ;\n"
                                ".L_x_1:\n"
                                "        /*0040*/                   BRA `(.L_x_1);\n"
                                "        /*0050*/                   CALL.REL.NOINC `(elsewhere) ;\n"
                                "        /*0060*/                   BRX R2 -0x70                                "
                                "(*\"BRANCH_TARGETS .L_x_0,.L_x_1\"*);\n"
                                ".L_x_9:\n"
                                "//--------------------- .text.k_two --------------------------\n"
                                "\t.section\t.text.k_two,\"ax\",@progbits\n"
                                "k_two:\n"
                                ".text.k_two:\n"
                                "        /*0000*/                   BRA `(.L_x_2);\n"
                                "        /*0010*/                   NOP;\n"
                                ".L_x_2:\n"
                                "        /*0020*/                   EXIT ;\n"
                                "//--------------------- .text.k_unnamed --------------------------\n"
                                "\t.section\t.text.k_unnamed,\"ax\",@progbits\n"
                                "        /*0000*/                   EXIT ;\n";

static const char s_one[] = "        /*0000*/                   IMAD.MOV.U32 R1, RZ, RZ, c[0x0][0x28] ;\n"
                            "        /*0010*/                   BSSY B6, 0x30 ;\n"
                            "        /*0020*/               @P0 BRA 0x40 ;\n"
                            "        /*0030*/                   EXIT ;\n"
                            "        /*0040*/                   BRA 0x40;\n"
                            "        /*0050*/                   CALL.REL.NOINC `(elsewhere) ;\n"
                            "        /*0060*/                   BRX R2 -0x70 ;\n";

static const char s_two[] = "        /*0000*/                   BRA 0x20;\n"
                            "        /*0010*/                   NOP;\n"
                            "        /*0020*/                   EXIT ;\n";

// 1 where kernel `at` of `cut` reads exactly `expected`
static int cut_reads(const CubinListingCut *cut, unsigned int at, const char *expected)
{
    return cut->found[at] && (cut->size[at] == strlen(expected)) &&
           (memcmp(cut->text + cut->start[at], expected, cut->size[at]) == 0);
}

int main(void)
{
    const char *const names[3] = {"k_two", "k_absent", "k_one"};
    CubinListingCut cut;
    check_that(cubin_listing_cut(s_listing, sizeof(s_listing) - 1u, names, 3u, &cut), "the listing is cut");
    check_that(cut_reads(&cut, 2u, s_one),
               "lines, labels, unset: k_one reads its instruction lines with each label it sets put as its offset, "
               "forward and back, and the label it does not set kept as printed");
    check_that(cut_reads(&cut, 0u, s_two), "labels: k_two's forward label is put as the offset after the NOP");
    check_that(!cut.found[1] && (cut.size[1] == 0u), "apart: a kernel no section holds is not found");
    check_that(cut.text_size == (strlen(s_one) + strlen(s_two)),
               "apart: no line of the section no kernel is named for is kept");
    cubin_listing_cut_freed(&cut);
    // a label after the last instruction, the listing ending with no newline
    static const char s_ending[] = "\t.section\t.text.k_end,\"ax\",@progbits\n"
                                   "        /*0000*/                   BRA `(.L_x_5);\n"
                                   ".L_x_5:";
    const char *const ending[1] = {"k_end"};
    check_that(cubin_listing_cut(s_ending, sizeof(s_ending) - 1u, ending, 1u, &cut) &&
                   cut_reads(&cut, 0u, "        /*0000*/                   BRA 0x10;\n"),
               "end: a label after the last instruction stands at the end of the code, and a listing with no last "
               "newline ends its section");
    cubin_listing_cut_freed(&cut);
    printf("cubin_listing_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
