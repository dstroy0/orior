// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_asks.c: the container asked: every ask of the container put to the part in turn
#include "probe_ask_internal.h"

// a question of the interface's own, asked in the part's code: the instruction to put in place of form_0's own, and the
// word the device should answer for the case sass_cubin_answer gives it
typedef struct
{
    const char *instruction;
    unsigned int answer;
} SassAsk;

// each question of the interface's own written into a cubin of its own, run, and its answer same to what the question
// says the part should say: how many answered so. form_0's kernel is the kernel, which loads the case's first two
// words into R0 and R7, and stores R7 as the first word of the answer
unsigned int sass_cubin_asks(SassProbe *probe, const SassMachine *machine, unsigned int *asked)
{
    static const SassAsk s_asks[] = {
        // the case is 0x0000000b and 0x00000007, and the part is asked what each instruction makes of them
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
        // The three comparisons no listing ever held, which the widening round found one bit from ones that were
        // (sass_machine_widen): the compiler read zero and below off the negations of NE and GE: nothing had run
        // these until here. Each is asked once where it should fire and once where it should not, and the answer is
        // the case's first word where the predicate held and zero where it did not
        {"ISETP.EQ.U32.AND P0, PT, R7, R7, PT\nSEL R7, R0, RZ, P0", 0x0000000bu},
        {"ISETP.EQ.U32.AND P0, PT, R7, R0, PT\nSEL R7, R0, RZ, P0", 0x00000000u},
        {"ISETP.LT.AND P0, PT, R7, R0, PT\nSEL R7, R0, RZ, P0", 0x0000000bu},
        {"ISETP.LT.AND P0, PT, R0, R7, PT\nSEL R7, R0, RZ, P0", 0x00000000u},
        // the .EX of it, which takes the low words' answer as its last operand: the pair R0 and R7 against itself,
        // then against one whose high word differs
        {"ISETP.EQ.U32.AND P6, PT, R0, R0, PT\nISETP.EQ.U32.AND.EX P0, PT, R7, R7, PT, P6\nSEL R7, R0, RZ, P0",
         0x0000000bu},
        {"ISETP.EQ.U32.AND P6, PT, R0, R0, PT\nISETP.EQ.U32.AND.EX P0, PT, R7, R0, PT, P6\nSEL R7, R0, RZ, P0",
         0x00000000u},
        // .hi on a number is the high half of it as .hi on a register is the pair's second register. No
        // listing prints this: nvdisasm writes a pair's second register out: every instruction ever assembled
        // carried .hi on a register alone, and a ruleset writing a 64-bit form against a literal is the first thing
        // to ask for the other half. 0x7_0000000b answers 7 where the half is taken and 11 where the whole number
        // is written and the field truncates it; 0x1_00000000 answers 1 against 0
        {"IMAD.MOV.U32 R7, RZ, RZ, 30064771083.hi", 0x00000007u},
        {"IMAD.MOV.U32 R7, RZ, RZ, 4294967296.hi", 0x00000001u},
        // A 64-bit load, which launch_load_wide wants: a ruleset cannot write [R2.64+{offset}] and [R2.64+{offset}+4],
        // since adding 4 to a parameter is arithmetic and a .krs does none: the pair must come in one
        // instruction. R2 still holds the case's address here, whose two words are 0xb and 0x7
        {"LDG.E.64.CONSTANT R8, [R2.64]\nIMAD.MOV.U32 R7, RZ, RZ, R8", 0x0000000bu},
        {"LDG.E.64.CONSTANT R8, [R2.64]\nIMAD.MOV.U32 R7, RZ, RZ, R9", 0x00000007u},
        // The other widths, which are names until the part is asked. .128 should write four registers from R8:
        // R9 still reads the case's second word; .U8 should write one byte zero extended, which 0xffffffff stored
        // and read back as 0xff tells apart from a word. The store is to the answer's third slot, which nothing
        // reads, and the safe control the assembler writes waits on it before the load
        {"LDG.E.128.CONSTANT R8, [R2.64]\nIMAD.MOV.U32 R7, RZ, RZ, R9", 0x00000007u},
        {"IMAD.MOV.U32 R8, RZ, RZ, 4294967295\nSTG.E [R4.64+0x8], R8\nLDG.E.U8.CONSTANT R9, [R4.64+0x8]\n"
         "IMAD.MOV.U32 R7, RZ, RZ, R9",
         0x000000ffu},
        // word_mul_low and word_mul_high, which the compiler fused into one IMAD.WIDE.U32 writing an aligned
        // pair
        // (form_13: MOV R7, RZ then IMAD.WIDE.U32 R6, R9, R0, R6). The core names the two halves apart: a pair
        // cannot be promised, and each half is asked here on its own: the low is the product's low word plus the
        // addend with its carry kept, and the high is the product's high word plus that carry. 0xffffffff squared
        // is 0xfffffffe00000001, and with 0xffffffff added the low is 0 carrying 1 and the high is 0xffffffff
        {"IMAD.MOV.U32 R2, RZ, RZ, 4294967295\nIMAD.MOV.U32 R3, RZ, RZ, 4294967295\n"
         "IMAD.MOV.U32 R6, RZ, RZ, 4294967295\nIMAD R8, R2, R3, RZ\nIADD3 R8, P6, R8, R6, RZ\n"
         "IMAD.MOV.U32 R7, RZ, RZ, R8",
         0x00000000u},
        {"IMAD.MOV.U32 R2, RZ, RZ, 4294967295\nIMAD.MOV.U32 R3, RZ, RZ, 4294967295\n"
         "IMAD.MOV.U32 R6, RZ, RZ, 4294967295\nIMAD R8, R2, R3, RZ\nIADD3 R8, P6, R8, R6, RZ\n"
         "IMAD.HI.U32 R9, R2, R3, RZ\nIMAD.X R9, RZ, RZ, R9, P6\nIMAD.MOV.U32 R7, RZ, RZ, R9",
         0xffffffffu},
        // predicate_bitxor and predicate_bitand as sass.krs writes them, their scratch in R2, R3 and R6, which the kernel
        // leaves free: R4 and R5 hold the address the answer is stored to and R7 holds the answer. P1 is true, since
        // the case's first word is not zero, and P2 is given the word that makes it true or the zero that does not
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
        // global_add_atomic_word, which the PTX question could only assemble and run: what the reduction leaves is read
        // here,
        // where the instructions are ours. The answer's third slot is set to zero, added to twice - once by one out
        // of a register, since the reduction reads no number out of the instruction, and once by the case's first
        // word - and read back. 0 + 1 + 0xb is 0xc, and a reduction that added nothing would answer 0
        {"STG.E [R4.64+0x8], RZ\nMOV R6, 0x1\nRED.E.ADD.STRONG.GPU [R4.64+0x8], R6\n"
         "RED.E.ADD.STRONG.GPU [R4.64+0x8], R0\nLDG.E.CONSTANT R7, [R4.64+0x8]",
         0x0000000cu},
        // Which numbers the part keeps for itself. R255 reads zero whatever is written into it, and that alone is
        // what RZ is: the part answers it, and the file does not.
        //
        // The high numbers are a different question and are not asked here. A kernel is given the registers its
        // count declares, and a question spliced into form_0 inherits form_0's count, which is far below
        // the numbers sm_86.kdm pins and cannot ask about one above it. The last register a kernel declaring 255 can
        // name is asked on the run channel (klq_identity register) and held in sm_86.ksc
        {"MOV R255, 0x5a3c69a5\nIMAD.MOV.U32 R7, RZ, RZ, R255", 0x00000000u},
        // The precepts of the alphabet (precepts.h) the part has never been asked for. Everything above this point
        // was asked because a ruleset wanted it; these are asked because the alphabet has them, and a part that has
        // no word for one is a part the web has to define it for.
        //
        // NOT, NAND and NOR cost nothing to ask: LOP3's immediate is the truth table of its three inputs, indexed by
        // a<<2|b<<1|c, and the part already answered for AND at 0xc0, OR at 0xfc and XOR at 0x3c. Their complements
        // are 0x3f, 0x03 and, for NOT of the first input alone, 0x0f. The case is 0xb and 0x7
        {"LOP3.LUT R7, R0, RZ, RZ, 0x0f, !PT", 0xfffffff4u},
        {"LOP3.LUT R7, R0, R7, RZ, 0x3f, !PT", 0xfffffffcu},
        {"LOP3.LUT R7, R0, R7, RZ, 0x03, !PT", 0xfffffff0u},
        // ASR, which differs from SHR only where the sign is set. The case's first word is negated first: -11 is
        // 0xfffffff5 and carrying its sign right 7 places leaves every bit set
        {"IADD3 R6, -R0, RZ, RZ\nSHF.R.S32.HI R7, RZ, R7, R6", 0xffffffffu},
        // ROR and ROL, on the two definitions that carry a funnel. The .U32 definitions were asked first and the part
        // said no to both: SHF.R.U32.HI R7, R0, R7, R0 answered 0 and SHF.L.U32 R7, R6, R7, R6 answered 0xfffffa80,
        // each the plain shift with the bits that left dropped. Giving a 32-bit shift the same register twice does
        // not rotate it, because there is no second half of the funnel for it to read. The .U64 definitions have one,
        // and neither reached a listing: the compiler never wrote either, and both came here from turning a listed
        // form's bits one at a time. 0xb carried right 7 places brings its low 7 bits back at the top, and
        // 0xfffffff5 carried left 7 brings its top 7 back at the bottom
        {"SHF.R.U64 R7, R0, R7, R0", 0x16000000u},
        {"IADD3 R6, -R0, RZ, RZ\nSHF.L.U64.HI R7, R6, R7, R6", 0xfffffaffu},
    };
    static char s_was[65536];
    static char s_asking[65536];
    if (sass_cubin_text(probe->folder, "form_0", s_was, sizeof(s_was)) == 0u)
    {
        return 0u;
    }
    unsigned int right = 0u;
    for (unsigned int number = 0u; number < (sizeof(s_asks) / sizeof(s_asks[0])); number += 1u)
    {
        *asked += 1u;
        char answered[256];
        char path[1024];
        unsigned int word = 0u;
        if (!sass_ask_text(s_was, s_asks[number].instruction, s_asking, sizeof(s_asking)) ||
            !sass_cubin_from_text(machine, probe->folder, "form_0", s_asking, "asked"))
        {
            // the question could not be written as the part's own code, which is a refusal before the part sees it
            sass_class_take(SASS_CHANNEL_RUN, SASS_CLASS_ILLEGAL, s_asks[number].instruction, 0u);
            printf("  ask %s: not written\n", s_asks[number].instruction);
            continue;
        }
        snprintf(path, sizeof(path), "%s/asked.cubin", probe->folder);
        if (!sass_cubin_answer(probe, path, answered, sizeof(answered)))
        {
            // the part was handed the question and would not run it: illegal on its own, however it assembled
            sass_class_take(SASS_CHANNEL_RUN, SASS_CLASS_ILLEGAL, s_asks[number].instruction, 0u);
            continue;
        }
        // the answer's first word, which the run prints after "answered "
        word = (unsigned int)strtoul(answered + 9, NULL, 16);
        const int same = (word == s_asks[number].answer);
        right += same ? 1u : 0u;
        // it ran and answered. Where the answer is what the question says, the part holds the question; where it
        // differs, the part took the question and what came back is not what it was read as, which is nothing this
        // can build on
        sass_class_take(SASS_CHANNEL_RUN, same ? SASS_CLASS_ANSWERS : SASS_CLASS_NOTHING, s_asks[number].instruction,
                        word);
        char named[SASS_TEXT];
        sass_ask_named(s_asks[number].instruction, named, sizeof(named));
        printf("  ask %-52s the part answers %08x, the question says %08x%s\n", named, word, s_asks[number].answer,
               same ? "" : " <- differs");
    }
    return right;
}
