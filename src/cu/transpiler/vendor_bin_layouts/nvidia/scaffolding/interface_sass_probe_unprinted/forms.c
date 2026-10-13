// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// forms.c: the forms: a question asked, an operand, a form's text, lines and addresses, a memory form's text and lines, the tally and the value, a form run and every form asked
#include "unprinted_internal.h"

// how a form's unprinted run answered, value by value
typedef struct
{
    unsigned int alike;
    unsigned int asked;
    char otherwise[512];
} UnprintedTally;

// question `number` asked at every value of its field, each value's answer printed and written into `record`: how many
// values answered as printed, or -1 where the question was not put
int unprinted_ask(const UnprintedQuestion *question, unsigned int number, FILE *record)
{
    // the field's bits as the record names them, none where the question turns no bit and runs its instruction as
    // the assembler wrote it
    char span[32];
    if (question->bits == 0u)
    {
        snprintf(span, sizeof(span), "none");
    }
    else
    {
        snprintf(span, sizeof(span), "%u-%u", question->first, (question->first + question->bits) - 1u);
    }
    unsigned long long low = 0ull;
    unsigned long long high = 0ull;
    const unsigned int count = unprinted_text(s_kernel_text, question->lines, s_asking, sizeof(s_asking))
                                   ? sass_assemble_lines(&s_machine, s_asking, SASS_CONTROL_SAFE, s_code,
                                                         sizeof(s_code))
                                   : 0u;
    const int alone = (count != 0u) && sass_assemble(&s_machine, question->turned, 0ull, 0ull, SASS_CONTROL_SAFE,
                                                     &low, &high);
    unsigned int place = 0u;
    if (!alone || (unprinted_find(s_code, count, low, high, &place, 1u) != 1u))
    {
        printf("  %s: not put, the question did not assemble or its instruction was not found once\n",
               question->turned);
        fprintf(record, "| %u | `%s` | %s | not put | | |\n", number, question->turned, span);
        return -1;
    }
    // the instruction read back through the machine file, which gives its text where the reader holds its field
    char read[SASS_MACHINE_TEXT];
    if (!sass_encoding_read(&s_machine, low, high, 0ull, read, sizeof(read)) || (strcmp(read, question->turned) != 0))
    {
        printf("  %s reads back as %s\n", question->turned, read);
    }
    const unsigned long long code_size = 16ull * count;
    char held[64];
    unprinted_binary(unprinted_bits_read(low, high, question->first, question->bits), question->bits, held);
    char fixed[64] = "";
    for (unsigned int at = 0u; at < UNPRINTED_FIXED; at += 1u)
    {
        const UnprintedFixed *const one = &question->fixed[at];
        if (one->bits != 0u)
        {
            const size_t written = strlen(fixed);
            snprintf(&fixed[written], sizeof(fixed) - written, ", bit %u held at %llu", one->first, one->value);
        }
    }
    printf("  %s, bits %s%s, the form holds %s, as printed %08x\n", question->turned, span, fixed, held,
           question->answer);
    int printed = 0;
    const unsigned long long values = 1ull << question->bits;
    for (unsigned long long value = 0ull; value < values; value += 1ull)
    {
        memcpy(s_turned, s_code, (size_t)code_size);
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
            snprintf(reading, sizeof(reading), "%08x%s", answered[0], same ? ", as printed" : "");
        }
        else
        {
            snprintf(reading, sizeof(reading), "did not run: %s", ended);
        }
        printf("    %s %s\n", digits, reading);
        fprintf(record, "| %u | `%s` | %s%s | %s | %s | %s |\n", number, question->turned, span, fixed, held, digits,
                reading);
    }
    printf("    %d of %llu values answer as printed\n", printed, values);
    return printed;
}

// One operand of a form filled for the forms question, from its form's own operand `base` of `kind` at `place`, into
// `operand`. The first operand is the result: R8, P0 or UR6. A register read is R0 and R6 in turn, which hold 0xb and
// 0x7, a predicate read P1, which is true, and a uniform register read UR4, which holds the memory descriptor. RZ, PT,
// URZ, a number and a constant past bank 0 are kept, and a constant in bank 0 reads c[0x0][0x0], which every run
// holds alike. A register keeps what follows its name, a half or a width. 1, or 0 where the kind is one this question
// does not fill
static int unprinted_operand(const char *base, unsigned int kind, unsigned int mark, unsigned int place,
                             unsigned int *reads, char *operand, size_t room)
{
    static const char *const s_marks[] = {"", "-", "~", "!"};
    const char *const shown = (mark < (sizeof(s_marks) / sizeof(s_marks[0]))) ? s_marks[mark] : "";
    const char *const dot = strchr(base, '.');
    const char *const after = (dot != NULL) ? dot : "";
    switch (kind)
    {
    case SASS_OPERAND_REGISTER:
        if ((place != 0u) && (strncmp(base, "RZ", 2u) == 0))
        {
            return snprintf(operand, room, "%s%s", shown, base) < (int)room;
        }
        *reads += (place != 0u) ? 1u : 0u;
        return snprintf(operand, room, "%s%s%s", shown, (place == 0u) ? "R8" : (((*reads % 2u) == 1u) ? "R0" : "R6"),
                        after) < (int)room;
    case SASS_OPERAND_PREDICATE:
        return snprintf(operand, room, "%s%s", shown,
                        (place == 0u) ? "P0" : ((strcmp(base, "PT") == 0) ? "PT" : "P1")) < (int)room;
    case SASS_OPERAND_UNIFORM:
        return snprintf(operand, room, "%s%s", shown,
                        (place == 0u) ? "UR6" : ((strcmp(base, "URZ") == 0) ? "URZ" : "UR4")) < (int)room;
    case SASS_OPERAND_IMMEDIATE:
        return (place != 0u) && (snprintf(operand, room, "%s%s", shown, base) < (int)room);
    case SASS_OPERAND_CONSTANT:
        return (place != 0u) && (snprintf(operand, room, "%s%s", shown,
                                          (strncmp(base, "c[0x0]", 6u) == 0) ? "c[0x0][0x0]" : base) < (int)room);
    default:
        return 0;
    }
}

// `form` written with its operands filled (unprinted_operand) into `text`: 1, or 0 with the reason in `why`, where its
// result is no register, predicate or uniform register, or an operand is a kind the question does not fill
static int unprinted_form_text(const SassForm *form, char *text, size_t room, const char **why)
{
    const int result = (form->operands != 0u) &&
                       ((form->kind[0] == SASS_OPERAND_REGISTER) || (form->kind[0] == SASS_OPERAND_PREDICATE) ||
                        (form->kind[0] == SASS_OPERAND_UNIFORM));
    if (!result)
    {
        *why = "its first operand is no register, predicate or uniform register to read an answer from";
        return 0;
    }
    SassInstructionParts base;
    sass_instruction_read(form->text, &base);
    size_t at = (size_t)snprintf(text, room, "%s", form->operation);
    unsigned int reads = 0u;
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        char operand[SASS_MACHINE_TOKEN + 8u];
        if (!unprinted_operand(base.operand[place], form->kind[place], form->mark[place], place, &reads, operand,
                               sizeof(operand)))
        {
            *why = "an operand is an address, a label or a kind the question does not fill";
            return 0;
        }
        at += (size_t)snprintf(text + at, (at < room) ? (room - at) : 0u, "%s%s", (place == 0u) ? " " : ", ",
                               operand);
    }
    return at < room;
}

// The lines of the forms question: UNPRINTED_COPIES copies of `instruction`, each after P1 and P2 are set again and
// each moving its result into R8 where the result is a predicate or a uniform register. The first copy's R8 is moved
// into R7, which the kernel stores as the first word, and each copy past it stores its R8 as a word of its own
static int unprinted_form_lines(const char *instruction, unsigned int result, char *lines, size_t room)
{
    size_t at = (size_t)snprintf(lines, room, "IMAD.MOV.U32 R6, RZ, RZ, R7\n");
    for (unsigned int copy = 0u; (copy < UNPRINTED_COPIES) && (at < room); copy += 1u)
    {
        const char *const moved = (result == SASS_OPERAND_PREDICATE) ? "SEL R8, R0, RZ, P0\n"
                                  : (result == SASS_OPERAND_UNIFORM) ? "MOV R8, UR6\n"
                                                                     : "";
        char kept[64];
        if (copy == 0u)
        {
            snprintf(kept, sizeof(kept), "IMAD.MOV.U32 R7, RZ, RZ, R8");
        }
        else
        {
            snprintf(kept, sizeof(kept), "STG.E term[UR4][R4.64+0x%x], R8", 4u * copy);
        }
        at += (size_t)snprintf(lines + at, room - at, "%s%s\n%s%s%s", UNPRINTED_PREDICATES, instruction, moved, kept,
                               (copy + 1u < UNPRINTED_COPIES) ? "\n" : "");
    }
    return at < room;
}

// 1 where `form` takes an address
static int unprinted_form_addresses(const SassForm *form)
{
    int addresses = 0;
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        addresses = addresses || (form->kind[place] == SASS_OPERAND_ADDRESS);
    }
    return addresses;
}

// `form`, which takes an address, written for the memory question into `text`: a predicate P0, or PT where it holds
// PT; a register before the address R10, the one an atomic writes the word it found into; the address the answer's
// third word, R4 with 0x8 added, 64 bits wide where the form's own is; and a register after it R0 and R6 in turn,
// which hold 0xb and 0x7. 1, or 0 with the reason in `why` where an operand is a kind the question does not fill
static int unprinted_memory_text(const SassForm *form, char *text, size_t room, const char **why)
{
    SassInstructionParts base;
    sass_instruction_read(form->text, &base);
    size_t at = (size_t)snprintf(text, room, "%s", form->operation);
    int after = 0;
    unsigned int reads = 0u;
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        char operand[SASS_MACHINE_TOKEN + 8u];
        switch (form->kind[place])
        {
        case SASS_OPERAND_PREDICATE:
            snprintf(operand, sizeof(operand), "%s", (strcmp(base.operand[place], "PT") == 0) ? "PT" : "P0");
            break;
        case SASS_OPERAND_ADDRESS:
            snprintf(operand, sizeof(operand), "[R4%s+0x8]", (strstr(base.operand[place], ".64") != NULL) ? ".64" : "");
            after = 1;
            break;
        case SASS_OPERAND_REGISTER:
            reads += after ? 1u : 0u;
            snprintf(operand, sizeof(operand), "%s", !after ? "R10" : (((reads % 2u) == 1u) ? "R0" : "R6"));
            break;
        default:
            *why = "an operand beside its address is a kind the memory question does not fill";
            return 0;
        }
        at += (size_t)snprintf(text + at, (at < room) ? (room - at) : 0u, "%s%s", (place == 0u) ? " " : ", ",
                               operand);
    }
    return at < room;
}

// The lines of the memory question: `instruction` once, the word it found stored as the answer's second word, and the
// answer's third word, which the instruction wrote, loaded into R7, which the kernel stores as the first
static int unprinted_memory_lines(const char *instruction, char *lines, size_t room)
{
    return snprintf(lines, room,
                    "IMAD.MOV.U32 R6, RZ, RZ, R7\n" UNPRINTED_PREDICATES "%s\nSTG.E term[UR4][R4.64+0x4], R10\n"
                    "LDG.E.CONSTANT R7, term[UR4][R4.64+0x8]",
                    instruction) < (int)room;
}

// `value` written as `bits` binary digits and what it answered appended to the tally's list of values answering
// otherwise than the form's own bits
static void unprinted_tally_otherwise(UnprintedTally *tally, unsigned long long value, unsigned int bits,
                                      const char *reading)
{
    char digits[64];
    unprinted_binary(value, (bits < 63u) ? bits : 63u, digits);
    const size_t at = strlen(tally->otherwise);
    snprintf(&tally->otherwise[at], sizeof(tally->otherwise) - at, "%s%s %s", (at == 0u) ? "" : "; ", digits, reading);
}

// The values asked of one run: every one where the run is UNPRINTED_FIELD_MOST bits or fewer, else the form's own
// value with each bit turned in turn. Value `index` of them, or the count where `index` is past them
static unsigned long long unprinted_value(unsigned int bits, unsigned long long own, unsigned long long index,
                                          unsigned long long *count)
{
    *count = (bits <= UNPRINTED_FIELD_MOST) ? (1ull << bits) : (unsigned long long)bits;
    return (bits <= UNPRINTED_FIELD_MOST) ? index : (own ^ (1ull << index));
}

// One unprinted run of a form asked at its values, `copies` at a run, against `own`, what the form's own bits
// answered. With several copies each answers one word, held against own[0]; with one, all four words are its and are
// held against own's four. A run that did not run asks each of its values again alone, the other copies at the form's
// own bits
static void unprinted_form_run(const unsigned int *places, unsigned int copies, unsigned long long code_size,
                               const SassRun *run, unsigned long long held, const unsigned int *own,
                               UnprintedTally *tally)
{
    const unsigned int bits = (run->last - run->first) + 1u;
    unsigned long long count = 0ull;
    unprinted_value(bits, held, 0ull, &count);
    for (unsigned long long start = 0ull; start < count; start += copies)
    {
        memcpy(s_turned, s_code, (size_t)code_size);
        for (unsigned int copy = 0u; (copy < copies) && ((start + copy) < count); copy += 1u)
        {
            unprinted_place_write(s_turned, places[copy], NULL, 0u, run->first, bits,
                                  unprinted_value(bits, held, start + copy, &count));
        }
        unsigned int answered[UNPRINTED_COPIES] = {0u, 0u, 0u, 0u};
        char ended[96] = "";
        const int ran = unprinted_run(s_turned, code_size, answered, ended, sizeof(ended));
        for (unsigned int copy = 0u; (copy < copies) && ((start + copy) < count); copy += 1u)
        {
            const unsigned long long value = unprinted_value(bits, held, start + copy, &count);
            unsigned int word = answered[copy];
            int alone = ran;
            if (!ran && (copies > 1u))
            {
                memcpy(s_turned, s_code, (size_t)code_size);
                unprinted_place_write(s_turned, places[0], NULL, 0u, run->first, bits, value);
                alone = unprinted_run(s_turned, code_size, answered, ended, sizeof(ended));
                word = answered[0];
            }
            tally->asked += 1u;
            char reading[128];
            if (!alone)
            {
                snprintf(reading, sizeof(reading), "did not run: %s", ended);
            }
            else if (copies > 1u)
            {
                snprintf(reading, sizeof(reading), "%08x", word);
            }
            else
            {
                snprintf(reading, sizeof(reading), "%08x %08x %08x %08x", answered[0], answered[1], answered[2],
                         answered[3]);
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

// Every form the machine file holds with an operand it does not print, each written with its operands filled
// (unprinted_form_text), UNPRINTED_COPIES copies to a cubin, and each unprinted run asked at its values against what
// the form's own bits answer. A row of `record` for each run asked and each form not asked
void unprinted_forms(FILE *record)
{
    fprintf(record, "# Every form's unprinted operands, asked of the part\n\n"
                    "Written by `interface_sass_probe_unprinted` (`interface_sass_unprinted.sh --forms`) whole on "
                    "every run. Each form holding a run of an operand it does not print is written with its operands "
                    "filled: its result R8, P0 or UR6, registers it reads R0 and R6 (0xb and 0x7), predicates it "
                    "reads P1 (true), uniform registers UR4, a constant in bank 0 c[0x0][0x0], four copies to a "
                    "run, each answering one word. A form that takes an address is written once a run with its address "
                    "the answer's third word, R4 with 0x8 added, a register before the address R10 and one after it R0 "
                    "and R6, and answers four words: the third word loaded back, the word it found, the third word, "
                    "and 0. Every cubin declares %u registers a thread. Each run is asked at every value where it is "
                    "%u bits or fewer, else at the form's own value with each bit turned, against what the form's own "
                    "bits answer. A run every value of which answers alike is one the part does not read on that "
                    "question.\n\n"
                    "| form | asked as | bits | the form holds | its own bits answer | values alike | values "
                    "otherwise |\n|---|---|---|---|---|---|---|\n",
            UNPRINTED_REGISTERS, UNPRINTED_FIELD_MOST);
    unsigned int forms = 0u;
    unsigned int asked = 0u;
    unsigned int unread = 0u;
    for (unsigned int number = 0u; number < s_machine.forms; number += 1u)
    {
        const SassForm *const form = &s_machine.form[number];
        int holds = 0;
        for (unsigned int at = 0u; at < form->runs; at += 1u)
        {
            holds = holds || (form->run[at].operand >= form->operands);
        }
        if (!holds)
        {
            continue;
        }
        forms += 1u;
        char instruction[SASS_MACHINE_TEXT];
        static char s_lines[4096];
        const char *why = "";
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        unsigned int places[UNPRINTED_COPIES] = {0u, 0u, 0u, 0u};
        // a form that takes an address is asked once a run through the word it writes, and every other through the
        // copies of its result
        const int memory = unprinted_form_addresses(form);
        const unsigned int copies = memory ? 1u : UNPRINTED_COPIES;
        const int written = memory ? unprinted_memory_text(form, instruction, sizeof(instruction), &why)
                                   : unprinted_form_text(form, instruction, sizeof(instruction), &why);
        const int lined = written && (memory ? unprinted_memory_lines(instruction, s_lines, sizeof(s_lines))
                                             : unprinted_form_lines(instruction, form->kind[0], s_lines,
                                                                    sizeof(s_lines)));
        const unsigned int count = (lined && unprinted_text(s_kernel_text, s_lines, s_asking, sizeof(s_asking)))
                                       ? sass_assemble_lines(&s_machine, s_asking, SASS_CONTROL_SAFE, s_code,
                                                             sizeof(s_code))
                                       : 0u;
        const int alone =
            (count != 0u) && sass_assemble(&s_machine, instruction, 0ull, 0ull, SASS_CONTROL_SAFE, &low, &high);
        const int found = alone && (unprinted_find(s_code, count, low, high, places, UNPRINTED_COPIES) == copies);
        why = (!written) ? why : (!found ? "the question did not assemble, or its copies were not found" : why);
        unsigned int own[UNPRINTED_COPIES] = {0u, 0u, 0u, 0u};
        char ended[96] = "";
        const int ran = found && unprinted_run(s_code, 16ull * count, own, ended, sizeof(ended));
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
            printf("  %s: not asked, %s%s%s\n", form->text, why, (found && !ran) ? ": " : "",
                   (found && !ran) ? ended : "");
            fprintf(record, "| `%s` | %s | | | not asked: %s | | |\n", form->text,
                    written ? instruction : "", why);
            continue;
        }
        asked += 1u;
        for (unsigned int at = 0u; at < form->runs; at += 1u)
        {
            const SassRun *const run = &form->run[at];
            if (run->operand < form->operands)
            {
                continue;
            }
            const unsigned int bits = (run->last - run->first) + 1u;
            const unsigned long long held = unprinted_bits_read(low, high, run->first, bits);
            UnprintedTally tally;
            memset(&tally, 0, sizeof(tally));
            unprinted_form_run(places, copies, 16ull * count, run, held, own, &tally);
            unread += (tally.alike == tally.asked) ? 1u : 0u;
            char digits[64];
            unprinted_binary(held, (bits < 63u) ? bits : 63u, digits);
            printf("  %s, bits %u-%u, %u of %u values alike%s%s\n", instruction, run->first, run->last, tally.alike,
                   tally.asked, (tally.otherwise[0] != '\0') ? ": " : "", tally.otherwise);
            fprintf(record, "| `%s` | `%s` | %u-%u | %s | %08x | %u of %u | %s |\n", form->text, instruction,
                    run->first, run->last, digits, own[0], tally.alike, tally.asked, tally.otherwise);
        }
    }
    fprintf(record, "\n%u forms hold an unprinted run, %u asked, %u runs answering alike at every value asked.\n",
            forms, asked, unread);
    printf("interface sass unprinted: %u forms hold an unprinted run, %u asked, %u runs answering alike\n", forms,
           asked, unread);
}
