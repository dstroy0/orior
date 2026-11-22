// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fields.c: the fields: each field read off the part, what writes first, a mark, a swap and an address swap, a
// redirect, and the fields named
#include "fields_internal.h"
#include "memoria_operor/memoria_linea.h"
#include "../../../../../../platform/platform.h"
#include "../../../../../engine/engine_note.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "../../../../../../../includes/parsers/line_read.h"

// the answers interface_sass_run wrote for the write pass's cubins, read against the form the first instruction
// contains and labeled a bit a row to `record`. A bit the part refused, one it read inside a run the machine file
// records for an operand, one it read outside every recorded run, and one it left the answer unchanged are
// distinguished
int fields_read(const char *machine_path, const char *instructions, const char *answers, const char *record)
{
    if (!sass_machine_read(&s_machine, machine_path))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the machine file ");
            mmgr_line_put(&engine_note.line, machine_path);
            mmgr_line_put(&engine_note.line, " did not read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    char instruction[512];
    if (!fields_first(instructions, instruction, sizeof(instruction)))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the instructions ");
            mmgr_line_put(&engine_note.line, instructions);
            mmgr_line_put(&engine_note.line, " contain no form");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    SassInstructionParts parts;
    sass_instruction_read(instruction, &parts);
    const SassForm *const form = sass_machine_form(&s_machine, &parts);
    if (form == NULL)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the machine file contains no form for ");
            mmgr_line_put(&engine_note.line, instruction);
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    PlatformFile *const in = platform_file_open(answers, "rb");
    PlatformFile *const out = platform_file_open(record, "wb");
    if ((in == NULL) || (out == NULL))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the answers ");
            mmgr_line_put(&engine_note.line, answers);
            mmgr_line_put(&engine_note.line, " or the record ");
            mmgr_line_put(&engine_note.line, record);
            mmgr_line_put(&engine_note.line, " was not reached");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    {
        mmgr_line_put(platform_file_line(out), "# A form's fields, found on the part\n\n");
    }
    {
        mmgr_line_put(platform_file_line(out), "Written by `interface_sass_probe_fields read`. The form is `");
        mmgr_line_put(platform_file_line(out), instruction);
        mmgr_line_put(
            platform_file_line(out),
            "`. Each row is one of its operation bits turned over and run on the part, against the answer the form "
            "gives untouched. A bit the part refuses, one it reads inside a run the machine file records for an "
            "operand, one it reads outside every recorded run, and one that leaves the answer unchanged are "
            "distinguished. A bit blocked from thepart, whose turned operation key contains a control transfer, a wait "
            "or no form in the machine file, is marked skipped and run on nothing.\n\n");
    }
    {
        mmgr_line_put(platform_file_line(out), "| bit | operand | part |\n|---|---|---|\n");
    }
    char line[256];
    char hidden_bits[512] = "";
    unsigned long long baseline = 0ULL;
    int have_baseline = 0;
    unsigned int refused = 0u;
    unsigned int inside = 0u;
    unsigned int hidden = 0u;
    unsigned int unread = 0u;
    while (platform_file_text_read(line, sizeof(line), in) != NULL)
    {
        unsigned int number = 0u;
        char type[64];
        char word[64];
        const char *read = line;
        word[0] = '\0';
        if (!line_unsigned(&read, &number) || !line_word(&read, type, sizeof(type)))
        {
            continue;
        }
        // the word after the type, where the line contains one
        line_word(&read, word, sizeof(word));
        if (number == 0u)
        {
            const char *base = word;
            have_baseline =
                (mmgr_text_equal(type, "answered", PLATFORM_TEXT_TO_TERMINATOR)) && line_number(&base, 1, &baseline);
            continue;
        }
        const unsigned int bit = number - 1u;
        const unsigned int operand = fields_operand_at(form, bit);
        const int in_run = (operand < form->operands);
        char place[32];
        if (in_run)
        {
            VerbaLinea buffer_line = mmgr_line_over(place, sizeof(place));
            mmgr_line_decimal(&buffer_line, operand, 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        else
        {
            VerbaLinea buffer_line = mmgr_line_over(place, sizeof(place));
            mmgr_line_put(&buffer_line, "-");
            (void)mmgr_line_finish(&buffer_line);
        }
        const char *part = NULL;
        if ((mmgr_text_equal(type, "refused", PLATFORM_TEXT_TO_TERMINATOR)) ||
            (mmgr_text_equal(type, "hung", PLATFORM_TEXT_TO_TERMINATOR)) ||
            (mmgr_text_equal(type, "skipped", PLATFORM_TEXT_TO_TERMINATOR)))
        {
            // a bit the part refused, one it ran too long on, and one blocked from the part as unsafe to run are each
            // no operand field; they share the first column and the row specifies which
            refused += 1u;
            part = (type[0] == 'h') ? "hung" : ((type[0] == 's') ? "skipped" : "refused");
        }
        else
        {
            unsigned long long answered = 0ULL;
            const char *given = word;
            const int answered_read = have_baseline && line_number(&given, 1, &answered) && (answered != baseline);
            if (!answered_read)
            {
                unread += 1u;
                part = "unread";
            }
            else if (in_run)
            {
                inside += 1u;
                part = "read";
            }
            else
            {
                hidden += 1u;
                part = "read, no recorded run";
                const size_t at = mmgr_text_length(hidden_bits, PLATFORM_TEXT_TO_TERMINATOR);
                {
                    VerbaLinea buffer_line = mmgr_line_over(&hidden_bits[at], sizeof(hidden_bits) - at);
                    mmgr_line_put(&buffer_line, (at == 0u) ? "" : " ");
                    mmgr_line_decimal(&buffer_line, bit, 1u);
                    (void)mmgr_line_finish(&buffer_line);
                }
            }
        }
        {
            mmgr_line_put(platform_file_line(out), "| ");
            mmgr_line_decimal(platform_file_line(out), bit, 1u);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_put(platform_file_line(out), place);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_put(platform_file_line(out), part);
            mmgr_line_put(platform_file_line(out), " |\n");
        }
    }
    {
        mmgr_line_put(platform_file_line(out), "\n");
        mmgr_line_decimal(platform_file_line(out), refused, 1u);
        mmgr_line_put(platform_file_line(out), " refused, ");
        mmgr_line_decimal(platform_file_line(out), inside, 1u);
        mmgr_line_put(platform_file_line(out), " read inside a recorded run, ");
        mmgr_line_decimal(platform_file_line(out), hidden, 1u);
        mmgr_line_put(platform_file_line(out), " read outside every recorded run, ");
        mmgr_line_decimal(platform_file_line(out), unread, 1u);
        mmgr_line_put(platform_file_line(out), " unread.\n");
    }
    platform_file_close(in);
    platform_file_close(out);
    // a tab-delimited line the gathering script turns into one row: the form, its counts, and the bits it reads that
    // the machine file records for no operand
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "FORMFIELDS\t");
        mmgr_line_put(output_line, instruction);
        mmgr_line_put(output_line, "\t");
        mmgr_line_decimal(output_line, refused, 1u);
        mmgr_line_put(output_line, "\t");
        mmgr_line_decimal(output_line, inside, 1u);
        mmgr_line_put(output_line, "\t");
        mmgr_line_decimal(output_line, hidden, 1u);
        mmgr_line_put(output_line, "\t");
        mmgr_line_decimal(output_line, unread, 1u);
        mmgr_line_put(output_line, "\t");
        mmgr_line_put(output_line, hidden_bits);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}

// 1 where the operation writes its first register operand, which the redirect may move to the observed register. A
// return, a register branch or a call reads its first register instead and leaves it where it is; a store, a branch
// to a label and a predicate setter carry no register first operand and never reach here
static int fields_writes_first(const char *operation)
{
    static const char *const reads[] = {"RET", "BRX", "JMX", "CALL", "BRA", "JMP", "EXIT", "BAR", "NOP"};
    for (unsigned int at = 0u; at < (sizeof(reads) / sizeof(reads[0])); at += 1u)
    {
        if (mmgr_text_order(operation, reads[at], (size_t)(mmgr_text_length(reads[at], PLATFORM_TEXT_TO_TERMINATOR))) ==
            0)
        {
            return 0;
        }
    }
    return 1;
}

// the sign an operand carries back as its text, since the parts reader cuts it off: the negate, the bitwise not and the
// predicate not, or nothing
static const char *fields_mark(unsigned int mark)
{
    if (mark == SASS_MARK_NEGATE)
    {
        return "-";
    }
    if (mark == SASS_MARK_INVERT)
    {
        return "~";
    }
    if (mark == SASS_MARK_NOT)
    {
        return "!";
    }
    return "";
}

// `operand`, which begins with a register token (R then digits, or RZ), written to `out` with that token replaced by
// `put`. A .hi names the second register of a pair and leaves the operand a register of the same form: it is dropped,
// and `put` itself is the register read or written. Any other suffix is retained
static void fields_swap(const char *operand, const char *put, char *out, size_t room)
{
    size_t length = 0u;
    if (operand[0] == 'R')
    {
        length =
            (operand[1] == 'Z') ? 2u : (1u + mmgr_text_span(operand + 1, "0123456789", PLATFORM_TEXT_TO_TERMINATOR));
    }
    const char *const rest = operand + length;
    const size_t rest_length =
        mmgr_text_length(rest, PLATFORM_TEXT_TO_TERMINATOR) - (sass_high_half(operand) ? 3u : 0u);
    {
        VerbaLinea buffer_line = mmgr_line_over(out, room);
        mmgr_line_put(&buffer_line, put);
        mmgr_line_put_max(&buffer_line, rest, (size_t)((int)rest_length));
        (void)mmgr_line_finish(&buffer_line);
    }
}

// `operand`, an address, written to `out` with the register its address is read from replaced by `put`: the register
// after the last `[`. A descriptor's uniform register before it is retained, and an address whose last `[` contains no
// register is copied as it is. 1, or 0 where `out` will not contain it
static int fields_address_swap(const char *operand, const char *put, char *out, size_t room)
{
    const char *const open = mmgr_text_char_last(operand, '[', PLATFORM_TEXT_TO_TERMINATOR);
    if ((open == NULL) || (open[1] != 'R') || ((open[2] != 'Z') && ((open[2] < '0') || (open[2] > '9'))))
    {
        VerbaLinea formed_line_1 = mmgr_line_over(out, room);
        mmgr_line_put(&formed_line_1, operand);
        (void)mmgr_line_finish(&formed_line_1);
        return (int)formed_line_1.at < (int)room;
    }
    char swapped[SASS_MACHINE_TOKEN];
    fields_swap(open + 1, put, swapped, sizeof(swapped));
    VerbaLinea formed_line_2 = mmgr_line_over(out, room);
    mmgr_line_put_max(&formed_line_2, operand, (size_t)((int)(open + 1 - operand)));
    mmgr_line_put(&formed_line_2, swapped);
    (void)mmgr_line_finish(&formed_line_2);
    return (int)formed_line_2.at < (int)room;
}

// `instruction` written to `redirected`, rebuilt from its parts, with its first operand moved where the kernel reads it
// and each source register operand moved to the next of the head's set registers, where the form's result and its
// source fields reach the answer. A global load's address is moved onto the case's, where the load reads the case's
// words and not memory no thread uses. A first operand that is a register moves to R8, the register the kernel stores
// as the answer. A first operand that is a predicate moves to P0, and `observe` takes the line that turns P0 into a
// value in R8; it is left empty otherwise. A form whose first operand is neither, or is one the operation reads, is
// copied as it is, since nothing of it would be observed through R8. 1, or 0 where `redirected` or `observe` will
// not contain it
int fields_redirect(const char *instruction, char *redirected, size_t room, char *observe, size_t observe_room)
{
    static const char *const sources[FIELDS_SOURCES] = {"R10", "R12", "R14", "R16", "R18", "R20"};
    SassInstructionParts parts;
    sass_instruction_read(instruction, &parts);
    observe[0] = '\0';
    const int writes_register = (parts.operands != 0u) && (parts.type[0] == SASS_OPERAND_REGISTER);
    const int writes_predicate = (parts.operands != 0u) && (parts.type[0] == SASS_OPERAND_PREDICATE);
    if ((!writes_register && !writes_predicate) || !fields_writes_first(parts.operation))
    {
        VerbaLinea formed_line_3 = mmgr_line_over(redirected, room);
        mmgr_line_put(&formed_line_3, instruction);
        (void)mmgr_line_finish(&formed_line_3);
        return (int)formed_line_3.at < (int)room;
    }
    VerbaLinea formed_line_29;
    if (writes_predicate && ((formed_line_29 = mmgr_line_over(observe, observe_room),
                              mmgr_line_put(&formed_line_29, FIELDS_LINES_PREDICATE),
                              (void)mmgr_line_finish(&formed_line_29), (int)formed_line_29.at) >= (int)observe_room))
    {
        return 0;
    }
    size_t at = 0u;
    if (parts.guard[0] != '\0')
    {
        VerbaLinea formed_line_4 = mmgr_line_over(&redirected[at], (at < room) ? (room - at) : 0u);
        mmgr_line_put(&formed_line_4, parts.guard);
        mmgr_line_put(&formed_line_4, " ");
        (void)mmgr_line_finish(&formed_line_4);
        at += (size_t)(int)formed_line_4.at;
    }
    VerbaLinea formed_line_5 = mmgr_line_over(&redirected[at], (at < room) ? (room - at) : 0u);
    mmgr_line_put(&formed_line_5, parts.operation);
    (void)mmgr_line_finish(&formed_line_5);
    at += (size_t)(int)formed_line_5.at;
    unsigned int source = 0u;
    for (unsigned int operand = 0u; operand < parts.operands; operand += 1u)
    {
        char text[SASS_MACHINE_TOKEN];
        const char *put = NULL;
        if (parts.type[operand] == SASS_OPERAND_REGISTER)
        {
            if (operand == 0u)
            {
                put = "R8";
            }
            else if (source < FIELDS_SOURCES)
            {
                put = sources[source];
                source += 1u;
            }
        }
        if ((operand == 0u) && writes_predicate)
        {
            VerbaLinea buffer_line = mmgr_line_over(text, sizeof(text));
            mmgr_line_put(&buffer_line, "P0");
            (void)mmgr_line_finish(&buffer_line);
        }
        else if (put != NULL)
        {
            fields_swap(parts.operand[operand], put, text, sizeof(text));
        }
        else if ((parts.type[operand] == SASS_OPERAND_ADDRESS) &&
                 (mmgr_text_starts(parts.operation, "LDG", PLATFORM_TEXT_TO_TERMINATOR)))
        {
            if (!fields_address_swap(parts.operand[operand], FIELDS_LOAD_ADDRESS, text, sizeof(text)))
            {
                return 0;
            }
        }
        else
        {
            VerbaLinea buffer_line = mmgr_line_over(text, sizeof(text));
            mmgr_line_put(&buffer_line, parts.operand[operand]);
            (void)mmgr_line_finish(&buffer_line);
        }
        VerbaLinea formed_line_6 = mmgr_line_over(&redirected[at], (at < room) ? (room - at) : 0u);
        mmgr_line_put(&formed_line_6, (operand == 0u) ? " " : ", ");
        mmgr_line_put(&formed_line_6, fields_mark(parts.mark[operand]));
        mmgr_line_put(&formed_line_6, text);
        (void)mmgr_line_finish(&formed_line_6);
        at += (size_t)(int)formed_line_6.at;
    }
    return at < room;
}

// the first instruction of `instructions`, moved where the kernel reads it and assembled alone, with each operation bit
// turned and read back through the machine file, nothing run. The untouched encoding is printed first, under `base`,
// with what it reads back as. Each bit is printed with what its turned encoding reads
// back as: `no form` where no form matches it, `same text` where it reads back as the instruction itself, which no
// field of the machine file matches, and the text it reads back as otherwise. Lines begin `NAMES`, for a script to
// gather. 0, or 2 where nothing was read
int fields_names(const char *machine_path, const char *instructions)
{
    if (!sass_machine_read(&s_machine, machine_path))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the machine file ");
            mmgr_line_put(&engine_note.line, machine_path);
            mmgr_line_put(&engine_note.line, " did not read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    char instruction[512];
    char redirected[512];
    char observe[64];
    unsigned long long low = 0ULL;
    unsigned long long high = 0ULL;
    if (!fields_first(instructions, instruction, sizeof(instruction)) ||
        !fields_redirect(instruction, redirected, sizeof(redirected), observe, sizeof(observe)) ||
        !sass_assemble(&s_machine, redirected, 0ULL, 0ULL, SASS_CONTROL_SAFE, &low, &high))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the instructions ");
            mmgr_line_put(&engine_note.line, instructions);
            mmgr_line_put(&engine_note.line, " contain no form that assembles");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    char text[256];
    char base_text[256];
    SassInstructionParts base;
    if (!sass_encoding_read(&s_machine, low, high, 0ULL, base_text, sizeof(base_text)))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, redirected);
            mmgr_line_put(&engine_note.line, " does not read back");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    sass_instruction_read(base_text, &base);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "NAMES\t");
        mmgr_line_put(output_line, instruction);
        mmgr_line_put(output_line, "\tbase\t");
        mmgr_line_put(output_line, base_text);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (unsigned int bit = 0u; bit < FIELDS_OPERATION_BITS; bit += 1u)
    {
        const unsigned long long turned_low = (bit < 64u) ? (low ^ (1ULL << bit)) : low;
        const unsigned long long turned_high = (bit < 64u) ? high : (high ^ (1ULL << (bit - 64u)));
        if (!sass_encoding_read(&s_machine, turned_low, turned_high, 0ULL, text, sizeof(text)))
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "NAMES\t");
                mmgr_line_put(output_line, instruction);
                mmgr_line_put(output_line, "\t");
                mmgr_line_decimal(output_line, bit, 1u);
                mmgr_line_put(output_line, "\tno form");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            continue;
        }
        if (mmgr_text_equal(text, base_text, PLATFORM_TEXT_TO_TERMINATOR))
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "NAMES\t");
                mmgr_line_put(output_line, instruction);
                mmgr_line_put(output_line, "\t");
                mmgr_line_decimal(output_line, bit, 1u);
                mmgr_line_put(output_line, "\tsame text");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            continue;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "NAMES\t");
            mmgr_line_put(output_line, instruction);
            mmgr_line_put(output_line, "\t");
            mmgr_line_decimal(output_line, bit, 1u);
            mmgr_line_put(output_line, "\t");
            mmgr_line_put(output_line, text);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    return 0;
}
