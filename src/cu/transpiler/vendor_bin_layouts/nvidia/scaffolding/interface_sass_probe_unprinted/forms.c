// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// forms.c: the forms: a question probed, an operand, a form's text, lines and addresses, a memory form's text and
// lines, the tally and the value, a form run and every form probed
#include "unprinted_internal.h"
#include "../../../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

// how a form's unprinted run answered, value by value
typedef struct
{
    unsigned int alike;
    unsigned int queried;
    char otherwise[512];
} UnprintedTally;

// question `number` probed at every value of its field, each value's answer printed and written into `record`: how many
// values answered as printed, or -1 where the question was not put
int unprinted_query(const UnprintedQuestion *question, unsigned int number, PlatformFile *record)
{
    // the field's bits as the record names them, none where the question turns no bit and runs its instruction as
    // the assembler wrote it
    char span[32];
    if (question->bits == 0u)
    {
        VerbaLinea buffer_line = mmgr_line_over(span, sizeof(span));
        mmgr_line_put(&buffer_line, "none");
        (void)mmgr_line_finish(&buffer_line);
    }
    else
    {
        VerbaLinea buffer_line = mmgr_line_over(span, sizeof(span));
        mmgr_line_decimal(&buffer_line, question->first, 1u);
        mmgr_line_put(&buffer_line, "-");
        mmgr_line_decimal(&buffer_line, (question->first + question->bits) - 1u, 1u);
        (void)mmgr_line_finish(&buffer_line);
    }
    unsigned long long low = 0ULL;
    unsigned long long high = 0ULL;
    const unsigned int count =
        unprinted_text(s_kernel_text, question->lines, s_query_text, sizeof(s_query_text))
            ? sass_assemble_lines(&s_machine, s_query_text, SASS_CONTROL_SAFE, s_code, sizeof(s_code))
            : 0u;
    const int alone =
        (count != 0u) && sass_assemble(&s_machine, question->turned, 0ULL, 0ULL, SASS_CONTROL_SAFE, &low, &high);
    unsigned int place = 0u;
    if (!alone || (unprinted_find(s_code, count, low, high, &place, 1u) != 1u))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, question->turned);
            mmgr_line_put(output_line,
                          ": not put, the question did not assemble or its instruction was not found once");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            mmgr_line_put(platform_file_line(record), "| ");
            mmgr_line_decimal(platform_file_line(record), number, 1u);
            mmgr_line_put(platform_file_line(record), " | `");
            mmgr_line_put(platform_file_line(record), question->turned);
            mmgr_line_put(platform_file_line(record), "` | ");
            mmgr_line_put(platform_file_line(record), span);
            mmgr_line_put(platform_file_line(record), " | not put | | |\n");
        }
        return -1;
    }
    // the instruction read back through the machine file, which gives its text where the reader defines its field
    char read[SASS_MACHINE_TEXT];
    if (!sass_encoding_read(&s_machine, low, high, 0ULL, read, sizeof(read)) ||
        (!mmgr_text_equal(read, question->turned, PLATFORM_TEXT_TO_TERMINATOR)))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, question->turned);
        mmgr_line_put(output_line, " reads back as ");
        mmgr_line_put(output_line, read);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const unsigned long long code_size = 16ULL * count;
    char value_text[64];
    unprinted_binary(unprinted_bits_read(low, high, question->first, question->bits), question->bits, value_text);
    char fixed[64] = "";
    for (unsigned int at = 0u; at < UNPRINTED_FIXED; at += 1u)
    {
        const UnprintedFixed *const one = &question->fixed[at];
        if (one->bits != 0u)
        {
            const size_t written = mmgr_text_length(fixed, PLATFORM_TEXT_TO_TERMINATOR);
            {
                VerbaLinea buffer_line = mmgr_line_over(&fixed[written], sizeof(fixed) - written);
                mmgr_line_put(&buffer_line, ", bit ");
                mmgr_line_decimal(&buffer_line, one->first, 1u);
                mmgr_line_put(&buffer_line, " fixed at ");
                mmgr_line_decimal(&buffer_line, one->value, 1u);
                (void)mmgr_line_finish(&buffer_line);
            }
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, question->turned);
        mmgr_line_put(output_line, ", bits ");
        mmgr_line_put(output_line, span);
        mmgr_line_put(output_line, fixed);
        mmgr_line_put(output_line, ", the form contains ");
        mmgr_line_put(output_line, value_text);
        mmgr_line_put(output_line, ", as printed ");
        mmgr_line_hex(output_line, question->answer, 8u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    int printed = 0;
    const unsigned long long values = 1ULL << question->bits;
    for (unsigned long long value = 0ULL; value < values; value += 1ULL)
    {
        mmgr_bytes_copy(s_turned, s_code, (size_t)code_size);
        unprinted_place_write(s_turned, place, question->fixed, UNPRINTED_FIXED, question->first, question->bits,
                              value);
        unsigned int answered[UNPRINTED_COPIES] = {0u, 0u, 0u, 0u};
        char ended[96] = "";
        const int ran = unprinted_run(s_turned, code_size, answered, ended, sizeof(ended));
        char digits[64];
        unprinted_binary(value, question->bits, digits);
        const int same = ran && (answered[0] == question->answer);
        printed += same;
        char reading[128];
        if (ran)
        {
            VerbaLinea buffer_line = mmgr_line_over(reading, sizeof(reading));
            mmgr_line_hex(&buffer_line, answered[0], 8u);
            mmgr_line_put(&buffer_line, same ? ", as printed" : "");
            (void)mmgr_line_finish(&buffer_line);
        }
        else
        {
            VerbaLinea buffer_line = mmgr_line_over(reading, sizeof(reading));
            mmgr_line_put(&buffer_line, "did not run: ");
            mmgr_line_put(&buffer_line, ended);
            (void)mmgr_line_finish(&buffer_line);
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    ");
            mmgr_line_put(output_line, digits);
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, reading);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            mmgr_line_put(platform_file_line(record), "| ");
            mmgr_line_decimal(platform_file_line(record), number, 1u);
            mmgr_line_put(platform_file_line(record), " | `");
            mmgr_line_put(platform_file_line(record), question->turned);
            mmgr_line_put(platform_file_line(record), "` | ");
            mmgr_line_put(platform_file_line(record), span);
            mmgr_line_put(platform_file_line(record), fixed);
            mmgr_line_put(platform_file_line(record), " | ");
            mmgr_line_put(platform_file_line(record), value_text);
            mmgr_line_put(platform_file_line(record), " | ");
            mmgr_line_put(platform_file_line(record), digits);
            mmgr_line_put(platform_file_line(record), " | ");
            mmgr_line_put(platform_file_line(record), reading);
            mmgr_line_put(platform_file_line(record), " |\n");
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    ");
        mmgr_line_decimal_signed(output_line, printed);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, values, 1u);
        mmgr_line_put(output_line, " values answer as printed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return printed;
}

// One operand of a form filled for the forms question, from its form's own operand `base` of `type` at `place`, into
// `operand`. The first operand is the result: R8, P0 or UR6. A register read is R0 and R6 in turn, which contain 0xb
// and 0x7, a predicate read P1, which is true, and a uniform register read UR4, which contains the memory descriptor.
// RZ, PT, URZ, a number and a constant past bank 0 are retained, and a constant in bank 0 reads c[0x0][0x0], which
// every run contains alike. A register retains what follows its name, a half or a width. 1, or 0 where the type is one
// this question does not fill
static int unprinted_operand(const char *base, unsigned int type, unsigned int mark, unsigned int place,
                             unsigned int *reads, char *operand, size_t room)
{
    static const char *const s_marks[] = {"", "-", "~", "!"};
    const char *const displayed = (mark < (sizeof(s_marks) / sizeof(s_marks[0]))) ? s_marks[mark] : "";
    const char *const dot = mmgr_text_char(base, '.', PLATFORM_TEXT_TO_TERMINATOR);
    const char *const after = (dot != NULL) ? dot : "";
    switch (type)
    {
    case SASS_OPERAND_REGISTER:
        if ((place != 0u) && (mmgr_text_starts(base, "RZ", PLATFORM_TEXT_TO_TERMINATOR)))
        {
            VerbaLinea formed_line_1 = mmgr_line_over(operand, room);
            mmgr_line_put(&formed_line_1, displayed);
            mmgr_line_put(&formed_line_1, base);
            (void)mmgr_line_finish(&formed_line_1);
            return (int)formed_line_1.at < (int)room;
        }
        *reads += (place != 0u) ? 1u : 0u;
        VerbaLinea formed_line_2 = mmgr_line_over(operand, room);
        mmgr_line_put(&formed_line_2, displayed);
        mmgr_line_put(&formed_line_2, (place == 0u) ? "R8" : (((*reads % 2u) == 1u) ? "R0" : "R6"));
        mmgr_line_put(&formed_line_2, after);
        (void)mmgr_line_finish(&formed_line_2);
        return (int)formed_line_2.at < (int)room;
    case SASS_OPERAND_PREDICATE: {
        VerbaLinea formed_line_43 = mmgr_line_over(operand, room);
        mmgr_line_put(&formed_line_43, displayed);
        mmgr_line_put(&formed_line_43,
                      (place == 0u) ? "P0"
                                    : ((mmgr_text_equal(base, "PT", PLATFORM_TEXT_TO_TERMINATOR)) ? "PT" : "P1"));
        (void)mmgr_line_finish(&formed_line_43);
        return (int)formed_line_43.at < (int)room;
    }
    case SASS_OPERAND_UNIFORM: {
        VerbaLinea formed_line_44 = mmgr_line_over(operand, room);
        mmgr_line_put(&formed_line_44, displayed);
        mmgr_line_put(&formed_line_44,
                      (place == 0u) ? "UR6"
                                    : ((mmgr_text_equal(base, "URZ", PLATFORM_TEXT_TO_TERMINATOR)) ? "URZ" : "UR4"));
        (void)mmgr_line_finish(&formed_line_44);
        return (int)formed_line_44.at < (int)room;
    }
    case SASS_OPERAND_IMMEDIATE: {
        VerbaLinea formed_line_63;
        return (place != 0u) && ((formed_line_63 = mmgr_line_over(operand, room),
                                  mmgr_line_put(&formed_line_63, displayed), mmgr_line_put(&formed_line_63, base),
                                  (void)mmgr_line_finish(&formed_line_63), (int)formed_line_63.at) < (int)room);
    }
    case SASS_OPERAND_CONSTANT: {
        VerbaLinea formed_line_64;
        return (place != 0u) &&
               ((formed_line_64 = mmgr_line_over(operand, room), mmgr_line_put(&formed_line_64, displayed),
                 mmgr_line_put(&formed_line_64,
                               (mmgr_text_starts(base, "c[0x0]", PLATFORM_TEXT_TO_TERMINATOR)) ? "c[0x0][0x0]" : base),
                 (void)mmgr_line_finish(&formed_line_64), (int)formed_line_64.at) < (int)room);
    }
    default:
        return 0;
    }
}

// `form` written with its operands filled (unprinted_operand) into `text`: 1, or 0 with the reason in `why`, where its
// result is no register, predicate or uniform register, or an operand is a type the question does not fill
static int unprinted_form_text(const SassForm *form, char *text, size_t room, const char **why)
{
    const int result = (form->operands != 0u) &&
                       ((form->type[0] == SASS_OPERAND_REGISTER) || (form->type[0] == SASS_OPERAND_PREDICATE) ||
                        (form->type[0] == SASS_OPERAND_UNIFORM));
    if (!result)
    {
        *why = "its first operand is no register, predicate or uniform register to read an answer from";
        return 0;
    }
    SassInstructionParts base;
    sass_instruction_read(form->text, &base);
    VerbaLinea formed_line_3 = mmgr_line_over(text, room);
    mmgr_line_put(&formed_line_3, form->operation);
    (void)mmgr_line_finish(&formed_line_3);
    size_t at = (size_t)(int)formed_line_3.at;
    unsigned int reads = 0u;
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        char operand[SASS_MACHINE_TOKEN + 8u];
        if (!unprinted_operand(base.operand[place], form->type[place], form->mark[place], place, &reads, operand,
                               sizeof(operand)))
        {
            *why = "an operand is an address, a label or a type the question does not fill";
            return 0;
        }
        VerbaLinea formed_line_4 = mmgr_line_over(text + at, (at < room) ? (room - at) : 0u);
        mmgr_line_put(&formed_line_4, (place == 0u) ? " " : ", ");
        mmgr_line_put(&formed_line_4, operand);
        (void)mmgr_line_finish(&formed_line_4);
        at += (size_t)(int)formed_line_4.at;
    }
    return at < room;
}

// The lines of the forms question: UNPRINTED_COPIES copies of `instruction`, each after P1 and P2 are set again and
// each moving its result into R8 where the result is a predicate or a uniform register. The first copy's R8 is moved
// into R7, which the kernel stores as the first word, and each copy past it stores its R8 as a word of its own
static int unprinted_form_lines(const char *instruction, unsigned int result, char *lines, size_t room)
{
    VerbaLinea formed_line_5 = mmgr_line_over(lines, room);
    mmgr_line_put(&formed_line_5, "IMAD.MOV.U32 R6, RZ, RZ, R7\n");
    (void)mmgr_line_finish(&formed_line_5);
    size_t at = (size_t)(int)formed_line_5.at;
    for (unsigned int copy = 0u; (copy < UNPRINTED_COPIES) && (at < room); copy += 1u)
    {
        const char *const moved = (result == SASS_OPERAND_PREDICATE) ? "SEL R8, R0, RZ, P0\n"
                                  : (result == SASS_OPERAND_UNIFORM) ? "MOV R8, UR6\n"
                                                                     : "";
        char field_text[64];
        if (copy == 0u)
        {
            VerbaLinea buffer_line = mmgr_line_over(field_text, sizeof(field_text));
            mmgr_line_put(&buffer_line, "IMAD.MOV.U32 R7, RZ, RZ, R8");
            (void)mmgr_line_finish(&buffer_line);
        }
        else
        {
            VerbaLinea buffer_line = mmgr_line_over(field_text, sizeof(field_text));
            mmgr_line_put(&buffer_line, "STG.E term[UR4][R4.64+0x");
            mmgr_line_hex(&buffer_line, 4u * copy, 1u);
            mmgr_line_put(&buffer_line, "], R8");
            (void)mmgr_line_finish(&buffer_line);
        }
        VerbaLinea formed_line_6 = mmgr_line_over(lines + at, room - at);
        mmgr_line_put(&formed_line_6, UNPRINTED_PREDICATES);
        mmgr_line_put(&formed_line_6, instruction);
        mmgr_line_put(&formed_line_6, "\n");
        mmgr_line_put(&formed_line_6, moved);
        mmgr_line_put(&formed_line_6, field_text);
        mmgr_line_put(&formed_line_6, (copy + 1u < UNPRINTED_COPIES) ? "\n" : "");
        (void)mmgr_line_finish(&formed_line_6);
        at += (size_t)(int)formed_line_6.at;
    }
    return at < room;
}

// 1 where `form` takes an address
static int unprinted_form_addresses(const SassForm *form)
{
    int addresses = 0;
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        addresses = addresses || (form->type[place] == SASS_OPERAND_ADDRESS);
    }
    return addresses;
}

// `form`, which takes an address, written for the memory question into `text`: a predicate P0, or PT where it contains
// PT; a register before the address R10, the one an atomic writes the word it found into; the address the answer's
// third word, R4 with 0x8 added, 64 bits wide where the form's own is; and a register after it R0 and R6 in turn,
// which contain 0xb and 0x7. 1, or 0 with the reason in `why` where an operand is a type the question does not fill
static int unprinted_memory_text(const SassForm *form, char *text, size_t room, const char **why)
{
    SassInstructionParts base;
    sass_instruction_read(form->text, &base);
    VerbaLinea formed_line_7 = mmgr_line_over(text, room);
    mmgr_line_put(&formed_line_7, form->operation);
    (void)mmgr_line_finish(&formed_line_7);
    size_t at = (size_t)(int)formed_line_7.at;
    int after = 0;
    unsigned int reads = 0u;
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        char operand[SASS_MACHINE_TOKEN + 8u];
        switch (form->type[place])
        {
        case SASS_OPERAND_PREDICATE: {
            VerbaLinea formed_line_45 = mmgr_line_over(operand, sizeof(operand));
            mmgr_line_put(&formed_line_45,
                          (mmgr_text_equal(base.operand[place], "PT", PLATFORM_TEXT_TO_TERMINATOR)) ? "PT" : "P0");
            (void)mmgr_line_finish(&formed_line_45);
            (void)formed_line_45.at;
        }
        break;
        case SASS_OPERAND_ADDRESS: {
            VerbaLinea formed_line_46 = mmgr_line_over(operand, sizeof(operand));
            mmgr_line_put(&formed_line_46, "[R4");
            mmgr_line_put(&formed_line_46,
                          (mmgr_text_find(base.operand[place], ".64", PLATFORM_TEXT_TO_TERMINATOR) != NULL) ? ".64"
                                                                                                            : "");
            mmgr_line_put(&formed_line_46, "+0x8]");
            (void)mmgr_line_finish(&formed_line_46);
            (void)formed_line_46.at;
        }
            after = 1;
            break;
        case SASS_OPERAND_REGISTER:
            reads += after ? 1u : 0u;
            {
                VerbaLinea buffer_line = mmgr_line_over(operand, sizeof(operand));
                mmgr_line_put(&buffer_line, !after ? "R10" : (((reads % 2u) == 1u) ? "R0" : "R6"));
                (void)mmgr_line_finish(&buffer_line);
            }
            break;
        default:
            *why = "an operand beside its address is a type the memory question does not fill";
            return 0;
        }
        VerbaLinea formed_line_8 = mmgr_line_over(text + at, (at < room) ? (room - at) : 0u);
        mmgr_line_put(&formed_line_8, (place == 0u) ? " " : ", ");
        mmgr_line_put(&formed_line_8, operand);
        (void)mmgr_line_finish(&formed_line_8);
        at += (size_t)(int)formed_line_8.at;
    }
    return at < room;
}

// The lines of the memory question: `instruction` once, the word it found stored as the answer's second word, and the
// answer's third word, which the instruction wrote, loaded into R7, which the kernel stores as the first
static int unprinted_memory_lines(const char *instruction, char *lines, size_t room)
{
    VerbaLinea formed_line = mmgr_line_over(lines, room);
    mmgr_line_put(&formed_line, "IMAD.MOV.U32 R6, RZ, RZ, R7\n" UNPRINTED_PREDICATES);
    mmgr_line_put(&formed_line, instruction);
    mmgr_line_put(&formed_line, "\nSTG.E term[UR4][R4.64+0x4], R10\nLDG.E.CONSTANT R7, term[UR4][R4.64+0x8]");
    (void)mmgr_line_finish(&formed_line);
    return (int)formed_line.at < (int)room;
}

// `value` written as `bits` binary digits and what it answered appended to the tally's list of values answering
// otherwise than the form's own bits
static void unprinted_tally_otherwise(UnprintedTally *tally, unsigned long long value, unsigned int bits,
                                      const char *reading)
{
    char digits[64];
    unprinted_binary(value, (bits < 63u) ? bits : 63u, digits);
    const size_t at = mmgr_text_length(tally->otherwise, PLATFORM_TEXT_TO_TERMINATOR);
    {
        VerbaLinea buffer_line = mmgr_line_over(&tally->otherwise[at], sizeof(tally->otherwise) - at);
        mmgr_line_put(&buffer_line, (at == 0u) ? "" : "; ");
        mmgr_line_put(&buffer_line, digits);
        mmgr_line_put(&buffer_line, " ");
        mmgr_line_put(&buffer_line, reading);
        (void)mmgr_line_finish(&buffer_line);
    }
}

// The values probed in one run: every one where the run is UNPRINTED_FIELD_MAX bits or fewer, else the form's own
// value with each bit turned in turn. Value `index` of them, or the count where `index` is past them
static unsigned long long unprinted_value(unsigned int bits, unsigned long long own, unsigned long long index,
                                          unsigned long long *count)
{
    *count = (bits <= UNPRINTED_FIELD_MAX) ? (1ULL << bits) : (unsigned long long)bits;
    return (bits <= UNPRINTED_FIELD_MAX) ? index : (own ^ (1ULL << index));
}

// One unprinted run of a form probed at its values, `copies` at a run, against `own`, what the form's own bits
// answered. With several copies each answers one word, compared against own[0]; with one, all four words are its and
// are compared against own's four. A run that did not run probes each of its values again alone, the other copies at
// the form's own bits
static void unprinted_form_run(const unsigned int *places, unsigned int copies, unsigned long long code_size,
                               const SassRun *run, unsigned long long field_value, const unsigned int *own,
                               UnprintedTally *tally)
{
    const unsigned int bits = (run->last - run->first) + 1u;
    unsigned long long count = 0ULL;
    unprinted_value(bits, field_value, 0ULL, &count);
    for (unsigned long long start = 0ULL; start < count; start += copies)
    {
        mmgr_bytes_copy(s_turned, s_code, (size_t)code_size);
        for (unsigned int copy = 0u; (copy < copies) && ((start + copy) < count); copy += 1u)
        {
            unprinted_place_write(s_turned, places[copy], NULL, 0u, run->first, bits,
                                  unprinted_value(bits, field_value, start + copy, &count));
        }
        unsigned int answered[UNPRINTED_COPIES] = {0u, 0u, 0u, 0u};
        char ended[96] = "";
        const int ran = unprinted_run(s_turned, code_size, answered, ended, sizeof(ended));
        for (unsigned int copy = 0u; (copy < copies) && ((start + copy) < count); copy += 1u)
        {
            const unsigned long long value = unprinted_value(bits, field_value, start + copy, &count);
            unsigned int word = answered[copy];
            int alone = ran;
            if (!ran && (copies > 1u))
            {
                mmgr_bytes_copy(s_turned, s_code, (size_t)code_size);
                unprinted_place_write(s_turned, places[0], NULL, 0u, run->first, bits, value);
                alone = unprinted_run(s_turned, code_size, answered, ended, sizeof(ended));
                word = answered[0];
            }
            tally->queried += 1u;
            char reading[128];
            if (!alone)
            {
                VerbaLinea buffer_line = mmgr_line_over(reading, sizeof(reading));
                mmgr_line_put(&buffer_line, "did not run: ");
                mmgr_line_put(&buffer_line, ended);
                (void)mmgr_line_finish(&buffer_line);
            }
            else if (copies > 1u)
            {
                VerbaLinea buffer_line = mmgr_line_over(reading, sizeof(reading));
                mmgr_line_hex(&buffer_line, word, 8u);
                (void)mmgr_line_finish(&buffer_line);
            }
            else
            {
                VerbaLinea buffer_line = mmgr_line_over(reading, sizeof(reading));
                mmgr_line_hex(&buffer_line, answered[0], 8u);
                mmgr_line_put(&buffer_line, " ");
                mmgr_line_hex(&buffer_line, answered[1], 8u);
                mmgr_line_put(&buffer_line, " ");
                mmgr_line_hex(&buffer_line, answered[2], 8u);
                mmgr_line_put(&buffer_line, " ");
                mmgr_line_hex(&buffer_line, answered[3], 8u);
                (void)mmgr_line_finish(&buffer_line);
            }
            const int alike = (copies > 1u) ? (word == own[0])
                                            : ((answered[0] == own[0]) && (answered[1] == own[1]) &&
                                               (answered[2] == own[2]) && (answered[3] == own[3]));
            if (alone && alike)
            {
                tally->alike += 1u;
                continue;
            }
            unprinted_tally_otherwise(tally, value, bits, reading);
        }
    }
}

// Every form the machine file contains with an operand it does not print, each written with its operands filled
// (unprinted_form_text), UNPRINTED_COPIES copies to a cubin, and each unprinted run probed at its values against what
// the form's own bits answer. A row of `record` for each run probed and each form not probed
void unprinted_forms(PlatformFile *record)
{
    {
        mmgr_line_put(
            platform_file_line(record),
            "# Every form's unprinted operands, probed on the part\n\nWritten by `interface_sass_probe_unprinted` "
            "(`interface_sass_unprinted.sh --forms`) whole on every run. Each form containing a run of an operand it "
            "does not print is written with its operands filled: its result R8, P0 or UR6, registers it reads R0 and "
            "R6 (0xb and 0x7), predicates it reads P1 (true), uniform registers UR4, a constant in bank 0 c[0x0][0x0], "
            "four copies to a run, each answering one word. A form that takes an address is written once a run with "
            "its address the answer's third word, R4 with 0x8 added, a register before the address R10 and one after "
            "it R0 and R6, and answers four words: the third word loaded back, the word it found, the third word, and "
            "0. Every cubin declares ");
        mmgr_line_decimal(platform_file_line(record), UNPRINTED_REGISTERS, 1u);
        mmgr_line_put(platform_file_line(record),
                      " registers a thread. Each run is probed at every value where it is ");
        mmgr_line_decimal(platform_file_line(record), UNPRINTED_FIELD_MAX, 1u);
        mmgr_line_put(platform_file_line(record),
                      " bits or fewer, else at the form's own value with each bit turned, against what the form's own "
                      "bits answer. A run every value of which answers alike is one the part does not read on that "
                      "question.\n\n| form | probed as | bits | the form contains | its own bits answer | values alike "
                      "| values otherwise |\n|---|---|---|---|---|---|---|\n");
    }
    unsigned int forms = 0u;
    unsigned int queried = 0u;
    unsigned int unread = 0u;
    for (unsigned int number = 0u; number < s_machine.forms; number += 1u)
    {
        const SassForm *const form = &s_machine.form[number];
        int matches = 0;
        for (unsigned int at = 0u; at < form->runs; at += 1u)
        {
            matches = matches || (form->run[at].operand >= form->operands);
        }
        if (!matches)
        {
            continue;
        }
        forms += 1u;
        char instruction[SASS_MACHINE_TEXT];
        static char s_lines[4096];
        const char *why = "";
        unsigned long long low = 0ULL;
        unsigned long long high = 0ULL;
        unsigned int places[UNPRINTED_COPIES] = {0u, 0u, 0u, 0u};
        // a form that takes an address is probed once a run through the word it writes, and every other through the
        // copies of its result
        const int memory = unprinted_form_addresses(form);
        const unsigned int copies = memory ? 1u : UNPRINTED_COPIES;
        const int written = memory ? unprinted_memory_text(form, instruction, sizeof(instruction), &why)
                                   : unprinted_form_text(form, instruction, sizeof(instruction), &why);
        const int lined =
            written && (memory ? unprinted_memory_lines(instruction, s_lines, sizeof(s_lines))
                               : unprinted_form_lines(instruction, form->type[0], s_lines, sizeof(s_lines)));
        const unsigned int count =
            (lined && unprinted_text(s_kernel_text, s_lines, s_query_text, sizeof(s_query_text)))
                ? sass_assemble_lines(&s_machine, s_query_text, SASS_CONTROL_SAFE, s_code, sizeof(s_code))
                : 0u;
        const int alone =
            (count != 0u) && sass_assemble(&s_machine, instruction, 0ULL, 0ULL, SASS_CONTROL_SAFE, &low, &high);
        const int found = alone && (unprinted_find(s_code, count, low, high, places, UNPRINTED_COPIES) == copies);
        why = (!written) ? why : (!found ? "the question did not assemble, or its copies were not found" : why);
        unsigned int own[UNPRINTED_COPIES] = {0u, 0u, 0u, 0u};
        char ended[96] = "";
        const int ran = found && unprinted_run(s_code, 16ULL * count, own, ended, sizeof(ended));
        const int steady = ran && (memory || ((own[1] == own[0]) && (own[2] == own[0]) && (own[3] == own[0])));
        if (found && !ran)
        {
            why = "its own bits did not run";
        }
        if (ran && !steady)
        {
            why = "its copies answered unlike one another at its own bits";
        }
        if (!steady)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, form->text);
                mmgr_line_put(output_line, ": not probed, ");
                mmgr_line_put(output_line, why);
                mmgr_line_put(output_line, (found && !ran) ? ": " : "");
                mmgr_line_put(output_line, (found && !ran) ? ended : "");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            {
                mmgr_line_put(platform_file_line(record), "| `");
                mmgr_line_put(platform_file_line(record), form->text);
                mmgr_line_put(platform_file_line(record), "` | ");
                mmgr_line_put(platform_file_line(record), written ? instruction : "");
                mmgr_line_put(platform_file_line(record), " | | | not probed: ");
                mmgr_line_put(platform_file_line(record), why);
                mmgr_line_put(platform_file_line(record), " | | |\n");
            }
            continue;
        }
        queried += 1u;
        for (unsigned int at = 0u; at < form->runs; at += 1u)
        {
            const SassRun *const run = &form->run[at];
            if (run->operand < form->operands)
            {
                continue;
            }
            const unsigned int bits = (run->last - run->first) + 1u;
            const unsigned long long field_value = unprinted_bits_read(low, high, run->first, bits);
            UnprintedTally tally;
            mmgr_bytes_set(&tally, 0, sizeof(tally));
            unprinted_form_run(places, copies, 16ULL * count, run, field_value, own, &tally);
            unread += (tally.alike == tally.queried) ? 1u : 0u;
            char digits[64];
            unprinted_binary(field_value, (bits < 63u) ? bits : 63u, digits);
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, instruction);
                mmgr_line_put(output_line, ", bits ");
                mmgr_line_decimal(output_line, run->first, 1u);
                mmgr_line_put(output_line, "-");
                mmgr_line_decimal(output_line, run->last, 1u);
                mmgr_line_put(output_line, ", ");
                mmgr_line_decimal(output_line, tally.alike, 1u);
                mmgr_line_put(output_line, " of ");
                mmgr_line_decimal(output_line, tally.queried, 1u);
                mmgr_line_put(output_line, " values alike");
                mmgr_line_put(output_line, (tally.otherwise[0] != '\0') ? ": " : "");
                mmgr_line_put(output_line, tally.otherwise);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            {
                mmgr_line_put(platform_file_line(record), "| `");
                mmgr_line_put(platform_file_line(record), form->text);
                mmgr_line_put(platform_file_line(record), "` | `");
                mmgr_line_put(platform_file_line(record), instruction);
                mmgr_line_put(platform_file_line(record), "` | ");
                mmgr_line_decimal(platform_file_line(record), run->first, 1u);
                mmgr_line_put(platform_file_line(record), "-");
                mmgr_line_decimal(platform_file_line(record), run->last, 1u);
                mmgr_line_put(platform_file_line(record), " | ");
                mmgr_line_put(platform_file_line(record), digits);
                mmgr_line_put(platform_file_line(record), " | ");
                mmgr_line_hex(platform_file_line(record), own[0], 8u);
                mmgr_line_put(platform_file_line(record), " | ");
                mmgr_line_decimal(platform_file_line(record), tally.alike, 1u);
                mmgr_line_put(platform_file_line(record), " of ");
                mmgr_line_decimal(platform_file_line(record), tally.queried, 1u);
                mmgr_line_put(platform_file_line(record), " | ");
                mmgr_line_put(platform_file_line(record), tally.otherwise);
                mmgr_line_put(platform_file_line(record), " |\n");
            }
        }
    }
    {
        mmgr_line_put(platform_file_line(record), "\n");
        mmgr_line_decimal(platform_file_line(record), forms, 1u);
        mmgr_line_put(platform_file_line(record), " forms contain an unprinted run, ");
        mmgr_line_decimal(platform_file_line(record), queried, 1u);
        mmgr_line_put(platform_file_line(record), " probed, ");
        mmgr_line_decimal(platform_file_line(record), unread, 1u);
        mmgr_line_put(platform_file_line(record), " runs answering alike at every value probed.\n");
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass unprinted: ");
        mmgr_line_decimal(output_line, forms, 1u);
        mmgr_line_put(output_line, " forms contain an unprinted run, ");
        mmgr_line_decimal(output_line, queried, 1u);
        mmgr_line_put(output_line, " probed, ");
        mmgr_line_decimal(output_line, unread, 1u);
        mmgr_line_put(output_line, " runs answering alike");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}
