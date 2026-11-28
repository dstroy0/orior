// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_queries.c: the container queried: every query of the container put to the part in turn
#include "../../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "probe_query_internal.h"

// an own question of the interface, queried in the code of the part. It has the instruction to put in place of the own
// instruction of form_0. It also has the word that the device must answer for the case from sass_cubin_answer
typedef struct
{
    const char *instruction;
    unsigned int answer;
} SassQuery;

// each own question of the interface, written into a cubin of its own and run. The call compares its answer with the
// answer that the question specifies, and returns how many agree. The kernel of form_0 is the kernel. It loads the
// first two words of the case into R0 and R7, and stores R7 as the first word of the answer
unsigned int sass_cubin_queries(SassProbe *probe, const SassMachine *machine, unsigned int *queried)
{
    static const SassQuery s_queries[] = {
        // the case is 0x0000000b and 0x00000007, and the program asks the part what each instruction makes of them
        {"IADD3 R7, R0, R7, RZ", 0x00000012u},
        {"LOP3.LUT R7, R0, R7, RZ, 0x3c, !PT", 0x0000000cu},
        {"LOP3.LUT R7, R0, R7, RZ, 0xc0, !PT", 0x00000003u},
        {"LOP3.LUT R7, R0, R7, RZ, 0xfc, !PT", 0x0000000fu},
        {"IMAD R7, R0, R7, RZ", 0x0000004du},
        {"SHF.L.U32 R7, R0, R7, RZ", 0x00000580u},
        {"SHF.R.U32.HI R7, RZ, R7, R0", 0x00000000u},
        {"IABS R7, R0", 0x0000000bu},
        {"IADD3 R7, -R0, RZ, RZ", 0xfffffff5u},
        {"SEL R7, R0, R7, PT", 0x0000000bu},
        // The three comparisons that no listing ever held. The widen round found them one bit from comparisons that a
        // listing held (sass_machine_widen). The compiler read zero and below from the negations of NE and GE, and
        // nothing ran these before this point. The program queries each once where it must fire and once where it must
        // not. The answer is the first word of the case where the predicate was true, and zero where it was not
        {"ISETP.EQ.U32.AND P0, PT, R7, R7, PT\nSEL R7, R0, RZ, P0", 0x0000000bu},
        {"ISETP.EQ.U32.AND P0, PT, R7, R0, PT\nSEL R7, R0, RZ, P0", 0x00000000u},
        {"ISETP.LT.AND P0, PT, R7, R0, PT\nSEL R7, R0, RZ, P0", 0x0000000bu},
        {"ISETP.LT.AND P0, PT, R0, R7, PT\nSEL R7, R0, RZ, P0", 0x00000000u},
        // the .EX of it, which takes the answer of the low words as its last operand. It compares the pair R0 and R7
        // with itself, then with a pair whose high word differs
        {"ISETP.EQ.U32.AND P6, PT, R0, R0, PT\nISETP.EQ.U32.AND.EX P0, PT, R7, R7, PT, P6\nSEL R7, R0, RZ, P0",
         0x0000000bu},
        {"ISETP.EQ.U32.AND P6, PT, R0, R0, PT\nISETP.EQ.U32.AND.EX P0, PT, R7, R0, PT, P6\nSEL R7, R0, RZ, P0",
         0x00000000u},
        // .hi on a number is the high half of it, as .hi on a register is the second register of the pair. No listing
        // prints this, because nvdisasm writes out the second register of a pair. Every instruction ever assembled
        // carried .hi on a register alone. A ruleset that writes a 64-bit form against a literal is the first thing to
        // query for the other half. 0x7_0000000b answers 7 where the call takes the half, and 11 where it writes the
        // whole number and the field truncates it. 0x1_00000000 answers 1 against 0
        {"IMAD.MOV.U32 R7, RZ, RZ, 30064771083.hi", 0x00000007u},
        {"IMAD.MOV.U32 R7, RZ, RZ, 4294967296.hi", 0x00000001u},
        // A 64-bit load, which launch_load_wide wants. A ruleset cannot write [R2.64+{offset}] and [R2.64+{offset}+4],
        // because an add of 4 to a parameter is arithmetic, and a .krs does none. Thus the pair must come in one
        // instruction. R2 still holds the address of the case here, and its two words are 0xb and 0x7
        {"LDG.E.64.CONSTANT R8, [R2.64]\nIMAD.MOV.U32 R7, RZ, RZ, R8", 0x0000000bu},
        {"LDG.E.64.CONSTANT R8, [R2.64]\nIMAD.MOV.U32 R7, RZ, RZ, R9", 0x00000007u},
        // The other widths, which are only names until the program queries the part. The width .128 must write four
        // registers from R8, and R9 still reads the second word of the case. The width .U8 must write one byte, zero
        // extended. A store of 0xffffffff that reads back as 0xff tells it from a word. The store goes to the third
        // word of the answer, which nothing reads. The safe control that the assembler writes waits on it before the
        // load
        {"LDG.E.128.CONSTANT R8, [R2.64]\nIMAD.MOV.U32 R7, RZ, RZ, R9", 0x00000007u},
        {"IMAD.MOV.U32 R8, RZ, RZ, 4294967295\nSTG.E [R4.64+0x8], R8\nLDG.E.U8.CONSTANT R9, [R4.64+0x8]\n"
         "IMAD.MOV.U32 R7, RZ, RZ, R9",
         0x000000ffu},
        // word_mul_low and word_mul_high, which the compiler fused into one IMAD.WIDE.U32 that writes an aligned pair.
        // That is form_13: MOV R7, RZ then IMAD.WIDE.U32 R6, R9, R0, R6. The core names the two halves apart, because
        // nothing can promise a pair. Thus the program queries each half here on its own. The low is the low product
        // word plus the addend, with its carry carried, and the high is the high word plus that carry. 0xffffffff
        // squared is 0xfffffffe00000001, and with 0xffffffff added the low is 0 with a carry of 1 and the high is
        // 0xffffffff
        {"IMAD.MOV.U32 R2, RZ, RZ, 4294967295\nIMAD.MOV.U32 R3, RZ, RZ, 4294967295\n"
         "IMAD.MOV.U32 R6, RZ, RZ, 4294967295\nIMAD R8, R2, R3, RZ\nIADD3 R8, P6, R8, R6, RZ\n"
         "IMAD.MOV.U32 R7, RZ, RZ, R8",
         0x00000000u},
        {"IMAD.MOV.U32 R2, RZ, RZ, 4294967295\nIMAD.MOV.U32 R3, RZ, RZ, 4294967295\n"
         "IMAD.MOV.U32 R6, RZ, RZ, 4294967295\nIMAD R8, R2, R3, RZ\nIADD3 R8, P6, R8, R6, RZ\n"
         "IMAD.HI.U32 R9, R2, R3, RZ\nIMAD.X R9, RZ, RZ, R9, P6\nIMAD.MOV.U32 R7, RZ, RZ, R9",
         0xffffffffu},
        // predicate_bitxor and predicate_bitand as sass.krs writes them, with their scratch in R2, R3 and R6, which the
        // kernel leaves free. R4 and R5 hold the address that the kernel stores the answer to, and R7 holds the answer.
        // P1 is true, because the first word of the case is not zero. P2 gets the word that makes it true, or the zero
        // that does not
        {"ISETP.NE.U32.AND P1, PT, R0, RZ, PT\nISETP.NE.U32.AND P2, PT, RZ, RZ, PT\nSEL R2, RZ, 0x1, P1\n"
         "SEL R3, RZ, 0x1, P2\nLOP3.LUT R2, R2, R3, RZ, 0x3c, !PT\nISETP.NE.U32.AND P0, PT, R2, RZ, PT\n"
         "SEL R7, R0, RZ, P0",
         0x0000000bu},
        {"ISETP.NE.U32.AND P1, PT, R0, RZ, PT\nISETP.NE.U32.AND P2, PT, R7, RZ, PT\nSEL R2, RZ, 0x1, P1\n"
         "SEL R3, RZ, 0x1, P2\nLOP3.LUT R2, R2, R3, RZ, 0x3c, !PT\nISETP.NE.U32.AND P0, PT, R2, RZ, PT\n"
         "SEL R7, R0, RZ, P0",
         0x00000000u},
        {"ISETP.NE.U32.AND P1, PT, R0, RZ, PT\nISETP.NE.U32.AND P2, PT, R7, RZ, PT\nMOV R6, 0x1\n"
         "SEL R2, R6, 0x0, P1\nSEL R3, R6, 0x0, P2\nLOP3.LUT R2, R2, R3, RZ, 0xc0, !PT\n"
         "ISETP.NE.U32.AND P0, PT, R2, RZ, PT\nSEL R7, R0, RZ, P0",
         0x0000000bu},
        {"ISETP.NE.U32.AND P1, PT, R0, RZ, PT\nISETP.NE.U32.AND P2, PT, RZ, RZ, PT\nMOV R6, 0x1\n"
         "SEL R2, R6, 0x0, P1\nSEL R3, R6, 0x0, P2\nLOP3.LUT R2, R2, R3, RZ, 0xc0, !PT\n"
         "ISETP.NE.U32.AND P0, PT, R2, RZ, PT\nSEL R7, R0, RZ, P0",
         0x00000000u},
        // global_add_atomic_word, which the PTX question could only assemble and run. The program reads here what the
        // reduction leaves, where the instructions are ours. It sets the third word of the answer to zero and adds to
        // it twice. The first add is a one from a register, because the reduction reads no number from the instruction.
        // The second add is the first word of the case, and then it reads the word back. 0 + 1 + 0xb is 0xc, and a
        // reduction that added nothing would answer 0
        {"STG.E [R4.64+0x8], RZ\nMOV R6, 0x1\nRED.E.ADD.STRONG.GPU [R4.64+0x8], R6\n"
         "RED.E.ADD.STRONG.GPU [R4.64+0x8], R0\nLDG.E.CONSTANT R7, [R4.64+0x8]",
         0x0000000cu},
        // Which numbers the part keeps for itself. R255 reads zero, whatever the code writes into it, and that alone is
        // what RZ is. The part answers it, and the file does not.
        //
        // The high numbers are a different question, and the program does not query them here. A kernel gets the
        // registers that its count declares. A question spliced into form_0 takes the count of form_0, which is far
        // below the numbers that sm_86.kdm pins. It cannot ask about one above that count. The run channel queries the
        // last register that a kernel with a count of 255 can name (orior_klq_identity register), and sm_86.ksc stores
        // it
        {"MOV R255, 0x5a3c69a5\nIMAD.MOV.U32 R7, RZ, RZ, R255", 0x00000000u},
        // The precepts of the alphabet (precepts.h) that the program never queried the part for. The program queried
        // everything above this point because a ruleset wanted it. It queries these because the alphabet has them. The
        // web must define a precept for a part that has no word for it.
        //
        // NOT, NAND and NOR cost nothing to query. The immediate of LOP3 is the truth table of its three inputs,
        // indexed by a<<2|b<<1|c. The part already answered for AND at 0xc0, OR at 0xfc and XOR at 0x3c. Their
        // complements are 0x3f, 0x03 and, for NOT of the first input alone, 0x0f. The case is 0xb and 0x7
        {"LOP3.LUT R7, R0, RZ, RZ, 0x0f, !PT", 0xfffffff4u},
        {"LOP3.LUT R7, R0, R7, RZ, 0x3f, !PT", 0xfffffffcu},
        {"LOP3.LUT R7, R0, R7, RZ, 0x03, !PT", 0xfffffff0u},
        // ASR, which differs from SHR only where the sign bit is 1. The call first negates the first word of the case:
        // -11 is 0xfffffff5, and a carry of its sign right 7 places leaves every bit set
        {"IADD3 R6, -R0, RZ, RZ\nSHF.R.S32.HI R7, RZ, R7, R6", 0xffffffffu},
        // ROR and ROL, on the two definitions that carry a funnel. The program queried the .U32 definitions first, and
        // the part refused both. SHF.R.U32.HI R7, R0, R7, R0 answered 0, and SHF.L.U32 R7, R6, R7, R6 answered
        // 0xfffffa80. Each was the plain shift, with the bits that left it dropped. A 32-bit shift with the same
        // register twice does not rotate, because there is no second half of the funnel for it to read.
        //
        // The .U64 definitions have one, and neither reached a listing. The compiler never wrote either, and both came
        // here from turns of the bits of a listed form, one at a time. 0xb carried right 7 places brings its low 7 bits
        // back at the top. 0xfffffff5 carried left 7 brings its top 7 back at the bottom
        {"SHF.R.U64 R7, R0, R7, R0", 0x16000000u},
        {"IADD3 R6, -R0, RZ, RZ\nSHF.L.U64.HI R7, R6, R7, R6", 0xfffffaffu},
    };
    static char s_was[65536];
    static char s_query_text[65536];
    if (sass_cubin_text(probe->folder, "form_0", s_was, sizeof(s_was)) == 0u)
    {
        return 0u;
    }
    unsigned int right = 0u;
    for (unsigned int number = 0u; number < (sizeof(s_queries) / sizeof(s_queries[0])); number += 1u)
    {
        *queried += 1u;
        char answered[256];
        char path[1024];
        unsigned int word = 0u;
        if (!sass_query_text(s_was, s_queries[number].instruction, s_query_text, sizeof(s_query_text)) ||
            !sass_cubin_from_text(machine, probe->folder, "form_0", s_query_text, "queried"))
        {
            // the call could not write the question as the own code of the part, which is a refusal before the part
            // sees it
            sass_class_take(SASS_CHANNEL_RUN, SASS_CLASS_ILLEGAL, s_queries[number].instruction, 0u);
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  query ");
                mmgr_line_put(output_line, s_queries[number].instruction);
                mmgr_line_put(output_line, ": not written");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            continue;
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
            mmgr_line_put(&buffer_line, probe->folder);
            mmgr_line_put(&buffer_line, "/queried.cubin");
            (void)mmgr_line_finish(&buffer_line);
        }
        if (!sass_cubin_answer(probe, path, answered, sizeof(answered)))
        {
            // the part got the question and did not run it: illegal on its own, however it assembled
            sass_class_take(SASS_CHANNEL_RUN, SASS_CLASS_ILLEGAL, s_queries[number].instruction, 0u);
            continue;
        }
        // the answer's first word, which the run prints after "answered "
        word =
            (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = answered + 9, .end = NULL, .base = 16);
        const int same = (word == s_queries[number].answer);
        right += same ? 1u : 0u;
        // it ran and answered. Where the answer is what the question specifies, the part satisfies the question. Where
        // it differs, the part took the question, but what came back does not match what the program took the
        // question to be. Nothing here can build on that
        sass_class_take(SASS_CHANNEL_RUN, same ? SASS_CLASS_ANSWERS : SASS_CLASS_NOTHING, s_queries[number].instruction,
                        word);
        char named[SASS_TEXT];
        sass_query_named(s_queries[number].instruction, named, sizeof(named));
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  query ");
            mmgr_line_put_columns(output_line, named, 52u);
            mmgr_line_put(output_line, " the part answers ");
            mmgr_line_hex(output_line, word, 8u);
            mmgr_line_put(output_line, ", the question specifies ");
            mmgr_line_hex(output_line, s_queries[number].answer, 8u);
            mmgr_line_put(output_line, same ? "" : " <- differs");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    return right;
}
