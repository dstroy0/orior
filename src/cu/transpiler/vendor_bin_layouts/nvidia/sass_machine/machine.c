// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// machine.c: the machine file. The module holds a form of the machine, the exit encoding, the operations that
// transfer control or wait, the barrier and the schedule. It takes the machine from text, writes and reads the runs,
// the types and the answers, and writes and reads the file.
#include "../../../../../../includes/parsers/line_read.h"
#include "../../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "sass_machine_internal.h"
#include "verba_scribo/verba_linea.h"
#include <stdio.h>
#include <string.h>

const SassForm *sass_machine_form(const SassMachine *machine, const SassInstructionParts *parts)
{
    const SassForm *found = NULL;
    for (unsigned int number = 0u; number < machine->forms; number += 1u)
    {
        found = sass_form_same(&machine->form[number], parts) ? &machine->form[number] : found;
    }
    return found;
}

unsigned long long sass_exit_encoding(const SassMachine *machine)
{
    unsigned long long found = 0ULL;
    for (unsigned int number = 0u; number < machine->forms; number += 1u)
    {
        found = (mmgr_text_equal(machine->form[number].operation, "EXIT", PLATFORM_TEXT_TO_TERMINATOR))
                    ? machine->form[number].low
                    : found;
    }
    return found;
}

int sass_operation_control_or_wait(const char *operation)
{
    static const char *const s_unsafe[] = {
        "BRA",      "BRX",   "JMP", "JMX",    "CALL",      "RET", "EXIT", "BSSY", "BSYNC",  "BREAK",  "BMOV",
        "WARPSYNC", "YIELD", "BAR", "DEPBAR", "NANOSLEEP", "BPT", "RTT",  "KILL", "RPCMOV", "RETIRE", "PMTRIG"};
    for (unsigned int at = 0u; at < (sizeof(s_unsafe) / sizeof(s_unsafe[0])); at += 1u)
    {
        if (mmgr_text_order(operation, s_unsafe[at],
                            (size_t)(mmgr_text_length(s_unsafe[at], PLATFORM_TEXT_TO_TERMINATOR))) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int sass_barrier_set(unsigned long long high, unsigned int first)
{
    return (unsigned int)((high >> (first - 64u)) & 7ULL) != SASS_BARRIER_NONE;
}

unsigned int sass_operation_schedule(const SassMachine *machine, const char *operation, unsigned int *soonest)
{
    // The names before the first dot that NVIDIA's compiler gives a write barrier at every place, or a read barrier.
    static const char *const s_late[] = {"LDG", "LDS", "S2R", "S2UR", "F2I", "I2F", "MUFU", "ATOMG"};
    static const char *const s_store[] = {"STG", "STS", "STL", "ST", "RED"};
    const size_t base = strcspn(operation, ".");
    *soonest = SASS_STALL_MAX_LEN;
    for (unsigned int at = 0u; at < (sizeof(s_late) / sizeof(s_late[0])); at += 1u)
    {
        if ((mmgr_text_length(s_late[at], PLATFORM_TEXT_TO_TERMINATOR) == base) &&
            (mmgr_text_order(operation, s_late[at], (size_t)(base)) == 0))
        {
            return SASS_SCHEDULE_LATE;
        }
    }
    for (unsigned int at = 0u; at < (sizeof(s_store) / sizeof(s_store[0])); at += 1u)
    {
        if ((mmgr_text_length(s_store[at], PLATFORM_TEXT_TO_TERMINATOR) == base) &&
            (mmgr_text_order(operation, s_store[at], (size_t)(base)) == 0))
        {
            return SASS_SCHEDULE_STORE;
        }
    }
    for (unsigned int at = 0u; at < machine->soonests; at += 1u)
    {
        if (mmgr_text_equal(operation, machine->soonest[at].operation, PLATFORM_TEXT_TO_TERMINATOR))
        {
            *soonest = machine->soonest[at].stall;
        }
    }
    return SASS_SCHEDULE_FIXED;
}

int sass_machine_take(SassMachine *machine, const char *text, unsigned long long low, unsigned long long high,
                      SassForm **taken)
{
    SassInstructionParts parts;
    sass_instruction_read(text, &parts);
    const SassForm *const found = sass_machine_form(machine, &parts);
    if (found != NULL)
    {
        *taken = &machine->form[found - machine->form];
        return 1;
    }
    if (machine->forms == SASS_MACHINE_FORMS)
    {
        machine->refused += 1u;
        *taken = NULL;
        return 0;
    }
    SassForm *const form = &machine->form[machine->forms];
    mmgr_bytes_set(form, 0, sizeof(*form));
    mmgr_bytes_copy(form->operation, parts.operation, sizeof(form->operation));
    form->operands = parts.operands;
    for (unsigned int place = 0u; place < parts.operands; place += 1u)
    {
        form->type[place] = parts.type[place];
        form->mark[place] = parts.mark[place];
    }
    form->low = low;
    form->high = high;
    {
        VerbaLinea buffer_line = mmgr_line_over(form->text, sizeof(form->text));
        mmgr_line_put(&buffer_line, text);
        (void)mmgr_line_finish(&buffer_line);
    }
    machine->forms += 1u;
    *taken = form;
    return 1;
}

int sass_machine_take_related(SassMachine *machine, const char *text, unsigned long long low, unsigned long long high,
                              const char *relation, SassForm **taken)
{
    SassInstructionParts parts;
    sass_instruction_read(text, &parts);
    for (unsigned int number = 0u; number < machine->forms; number += 1u)
    {
        SassForm *const form = &machine->form[number];
        if (sass_form_same(form, &parts) &&
            ((form->relation[0] == '\0') || (mmgr_text_equal(form->relation, relation, PLATFORM_TEXT_TO_TERMINATOR))))
        {
            *taken = form;
            return 1;
        }
    }
    if (machine->forms == SASS_MACHINE_FORMS)
    {
        machine->refused += 1u;
        *taken = NULL;
        return 0;
    }
    SassForm *const form = &machine->form[machine->forms];
    mmgr_bytes_set(form, 0, sizeof(*form));
    mmgr_bytes_copy(form->operation, parts.operation, sizeof(form->operation));
    form->operands = parts.operands;
    for (unsigned int place = 0u; place < parts.operands; place += 1u)
    {
        form->type[place] = parts.type[place];
        form->mark[place] = parts.mark[place];
    }
    form->low = low;
    form->high = high;
    {
        VerbaLinea buffer_line = mmgr_line_over(form->text, sizeof(form->text));
        mmgr_line_put(&buffer_line, text);
        (void)mmgr_line_finish(&buffer_line);
    }
    machine->forms += 1u;
    *taken = form;
    return 1;
}

// Return the bytes of the constant operand `text` that come before its immediate: the bank, the base and the `+`.
// Return the bytes up to the second bracket's inside if the operand has no `+`. Return 0 if the text is not a constant
// of the form `c[<bank>][<offset>]`.
static size_t sass_constant_head(const char *text)
{
    const char *const first_close = mmgr_text_char(text, ']', PLATFORM_TEXT_TO_TERMINATOR);
    const char *const second =
        (first_close != NULL) ? mmgr_text_char(first_close + 1, '[', PLATFORM_TEXT_TO_TERMINATOR) : NULL;
    if (second == NULL)
    {
        return 0u;
    }
    const char *const plus = strrchr(second, '+');
    // The head is a few bytes of a short operand.
    return (size_t)(((plus != NULL) ? plus : second) + 1 - text);
}

int sass_text_placed(const SassForm *form, char *written, size_t room)
{
    SassInstructionParts parts;
    sass_instruction_read(form->text, &parts);
    // A guard run with a place gets its place before the operation, in the guard's position.
    unsigned int guard_place = SASS_RUN_NO_PLACE;
    for (unsigned int number = 0u; number < form->runs; number += 1u)
    {
        guard_place =
            ((form->run[number].operand == SASS_MACHINE_OPERANDS) && (form->run[number].place != SASS_RUN_NO_PLACE))
                ? form->run[number].place
                : guard_place;
    }
    char guard[SASS_MACHINE_TOKEN];
    if (guard_place != SASS_RUN_NO_PLACE)
    {
        VerbaLinea buffer_line = mmgr_line_over(guard, sizeof(guard));
        mmgr_line_put(&buffer_line, "@{");
        mmgr_line_decimal(&buffer_line, guard_place, 1u);
        mmgr_line_put(&buffer_line, "}");
        (void)mmgr_line_finish(&buffer_line);
    }
    else
    {
        VerbaLinea buffer_line = mmgr_line_over(guard, sizeof(guard));
        mmgr_line_put(&buffer_line, parts.guard);
        (void)mmgr_line_finish(&buffer_line);
    }
    VerbaLinea formed_line_1 = mmgr_line_over(written, room);
    mmgr_line_put(&formed_line_1, guard);
    mmgr_line_put(&formed_line_1, (guard[0] != '\0') ? " " : "");
    mmgr_line_put(&formed_line_1, parts.operation);
    (void)mmgr_line_finish(&formed_line_1);
    int printed = (int)formed_line_1.at;
    size_t at = (printed > 0) ? (size_t)printed : room;
    for (unsigned int operand = 0u; (operand < parts.operands) && (at < room); operand += 1u)
    {
        unsigned int place = SASS_RUN_NO_PLACE;
        for (unsigned int number = 0u; number < form->runs; number += 1u)
        {
            place = ((form->run[number].operand == operand) && (form->run[number].place != SASS_RUN_NO_PLACE))
                        ? form->run[number].place
                        : place;
        }
        const char *const between = (operand == 0u) ? " " : ", ";
        const char *const mark = s_mark_names[parts.mark[operand]];
        const char *const text = parts.operand[operand];
        // A placed address keeps its brackets and what follows its register inside them, `[{<place>}.64]`. The
        // operand then reads as an address after the place takes its value.
        const int address = (parts.type[operand] == SASS_OPERAND_ADDRESS) && (text[0] == '[');
        const size_t inner = address ? (1u + strcspn(text + 1, ".+]")) : 0u;
        // A placed constant keeps its bank and its base and places its immediate, `c[0x0][0x160+{<place>}]`. The
        // operand then reads as a constant after the place takes its value.
        const size_t head = (parts.type[operand] == SASS_OPERAND_CONSTANT) ? sass_constant_head(text) : 0u;
        printed = (place == SASS_RUN_NO_PLACE) ? snprintf(written + at, room - at, "%s%s%s", between, mark, text)
                  : address ? snprintf(written + at, room - at, "%s%s[{%u}%s", between, mark, place, text + inner)
                  : head != 0u
                      ? snprintf(written + at, room - at, "%s%s%.*s{%u}]", between, mark, (int)head, text, place)
                      : snprintf(written + at, room - at, "%s%s{%u}", between, mark, place);
        at += (printed > 0) ? (size_t)printed : room;
    }
    return (at < room) ? 1 : 0;
}

// Write the runs of `form` into `written` as one column, each with its place, "none" if the probe found none.
static void sass_runs_write(const SassForm *form, char *written, size_t room)
{
    size_t at = 0u;
    written[0] = '\0';
    for (unsigned int number = 0u; number < form->runs; number += 1u)
    {
        const SassRun *const run = &form->run[number];
        char place[16];
        if (run->place == SASS_RUN_NO_PLACE)
        {
            VerbaLinea buffer_line = mmgr_line_over(place, sizeof(place));
            mmgr_line_put(&buffer_line, SASS_RUN_NO_PLACE_TEXT);
            (void)mmgr_line_finish(&buffer_line);
        }
        else
        {
            VerbaLinea buffer_line = mmgr_line_over(place, sizeof(place));
            mmgr_line_decimal(&buffer_line, run->place, 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        VerbaLinea formed_line_2 = mmgr_line_over(written + at, room - at);
        mmgr_line_put(&formed_line_2, (number == 0u) ? "" : ";");
        mmgr_line_decimal(&formed_line_2, run->operand, 1u);
        mmgr_line_put(&formed_line_2, ":");
        mmgr_line_decimal(&formed_line_2, run->first, 1u);
        mmgr_line_put(&formed_line_2, "-");
        mmgr_line_decimal(&formed_line_2, run->last, 1u);
        mmgr_line_put(&formed_line_2, ":");
        mmgr_line_put(&formed_line_2, place);
        (void)mmgr_line_finish(&formed_line_2);
        const int printed = (int)formed_line_2.at;
        // snprintf gives the characters of the whole text, and the room below bounds them.
        at += ((printed > 0) && ((size_t)printed < (room - at))) ? (size_t)printed : 0u;
    }
    if (form->runs == 0u)
    {
        VerbaLinea buffer_line = mmgr_line_over(written, room);
        mmgr_line_put(&buffer_line, "none");
        (void)mmgr_line_finish(&buffer_line);
    }
}

// Read one form's runs column into `form`. Each run's place comes after its last bit if the column contains one,
// and is SASS_RUN_NO_PLACE if it does not. Return 1, or 0 if a run does not read.
static int sass_runs_read(SassForm *form, const char *column)
{
    form->runs = 0u;
    if (mmgr_text_equal(column, "none", PLATFORM_TEXT_TO_TERMINATOR))
    {
        return 1;
    }
    const char *at = column;
    while (*at != '\0')
    {
        unsigned int operand = 0u;
        unsigned int first = 0u;
        unsigned int last = 0u;
        // A run is `<operand>:<first>-<last>`.
        const char *walk = at;
        int whole = line_unsigned(&walk, &operand) && (*walk == ':');
        walk += whole ? 1 : 0;
        whole = whole && line_unsigned(&walk, &first) && (*walk == '-');
        walk += whole ? 1 : 0;
        whole = whole && line_unsigned(&walk, &last);
        if (!whole || (form->runs == SASS_MACHINE_RUNS))
        {
            return 0;
        }
        // A run's characters are within its column, and an int counts them.
        const int read = (int)(walk - at);
        unsigned int place = SASS_RUN_NO_PLACE;
        if ((at[read] == ':') &&
            (mmgr_text_order(&at[read + 1], SASS_RUN_NO_PLACE_TEXT,
                             (size_t)(mmgr_text_length(SASS_RUN_NO_PLACE_TEXT, PLATFORM_TEXT_TO_TERMINATOR))) != 0))
        {
            place = (unsigned int)strtoul(&at[read + 1], NULL, 10);
        }
        form->run[form->runs].operand = operand;
        form->run[form->runs].first = first;
        form->run[form->runs].last = last;
        form->run[form->runs].place = place;
        form->runs += 1u;
        const size_t length = strcspn(at, ";");
        at += length + ((at[length] == ';') ? 1u : 0u);
    }
    return 1;
}

// Write the types and marks of `form` into `written` as one column, "none" if it takes no operand.
static void sass_types_write(const SassForm *form, char *written, size_t room)
{
    size_t at = 0u;
    written[0] = '\0';
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        VerbaLinea formed_line_3 = mmgr_line_over(written + at, room - at);
        mmgr_line_put(&formed_line_3, (place == 0u) ? "" : ",");
        mmgr_line_put(&formed_line_3, s_type_names[form->type[place]]);
        mmgr_line_put(&formed_line_3, s_mark_names[form->mark[place]]);
        (void)mmgr_line_finish(&formed_line_3);
        const int printed = (int)formed_line_3.at;
        // snprintf gives the characters of the whole text, and the room below bounds them.
        at += ((printed > 0) && ((size_t)printed < (room - at))) ? (size_t)printed : 0u;
    }
    if (form->operands == 0u)
    {
        VerbaLinea buffer_line = mmgr_line_over(written, room);
        mmgr_line_put(&buffer_line, "none");
        (void)mmgr_line_finish(&buffer_line);
    }
}

int sass_machine_write(const SassMachine *machine, const char *path)
{
    static char s_file_bytes[QRY_WRITER_RING_BYTES / 2ULL];
    VerbaLinea file = {s_file_bytes, sizeof(s_file_bytes), 0ULL};
    mmgr_line_put(&file, "forms 2" SASS_LINE_END);
    mmgr_line_put(&file,
                  "# The part's instructions as the cell's probes read them back: one line a form, an operation "
                  "and" SASS_LINE_END
                  "# the type and mark of each printed operand, with the encoding the form was first seen with "
                  "and" SASS_LINE_END "# the instruction it was seen as. The format is the comment at the head of "
                  "sass_machine.h." SASS_LINE_END);
    mmgr_line_put(&file, "part ");
    mmgr_line_put(&file, machine->part);
    mmgr_line_put(&file, SASS_LINE_END);
    for (unsigned int number = 0u; number < machine->forms; number += 1u)
    {
        const SassForm *const form = &machine->form[number];
        char types[SASS_MACHINE_TOKEN * SASS_MACHINE_OPERANDS];
        char runs[SASS_MACHINE_RUNS * 16u];
        sass_types_write(form, types, sizeof(types));
        sass_runs_write(form, runs, sizeof(runs));
        const int related = (form->relation[0] != '\0');
        mmgr_line_put(&file, "form ");
        mmgr_line_put(&file, form->operation);
        mmgr_line_put(&file, " ");
        mmgr_line_put(&file, types);
        mmgr_line_put(&file, " 0x");
        mmgr_line_hex(&file, form->low, 16u);
        mmgr_line_put(&file, " 0x");
        mmgr_line_hex(&file, form->high, 16u);
        mmgr_line_put(&file, " ");
        mmgr_line_put(&file, runs);
        mmgr_line_put(&file, " ");
        mmgr_line_put(&file, related ? form->relation : SASS_RUN_NO_PLACE_TEXT);
        mmgr_line_put(&file, " ");
        mmgr_line_put(&file, related ? ((form->signed_read != 0) ? "1" : "0") : SASS_RUN_NO_PLACE_TEXT);
        mmgr_line_put(&file, " ");
        mmgr_line_put(&file, form->text);
        mmgr_line_put(&file, SASS_LINE_END);
    }
    // The whole text goes to the run's writer, which writes it in the file's place (qry_member_filed).
    if (!mmgr_line_ok(&file) || !qry_member_filed(path, file.out, file.at))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  sass_machine: ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " could not be written: no run's writer runs, or it is outside the tree");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    return 1;
}

// Return the place of `word` among the names, or `count` if it is none of them.
static unsigned int sass_name_place(const char *const *names, unsigned int count, const char *word, size_t length)
{
    unsigned int found = count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        found = ((mmgr_text_length(names[at], PLATFORM_TEXT_TO_TERMINATOR) == length) &&
                 (mmgr_text_order(names[at], word, (size_t)(length)) == 0))
                    ? at
                    : found;
    }
    return found;
}

// Read one form's types column into `form`. Return 1, or 0 if a type or a mark is none the writer names.
static int sass_types_read(SassForm *form, const char *column)
{
    form->operands = 0u;
    if (mmgr_text_equal(column, "none", PLATFORM_TEXT_TO_TERMINATOR))
    {
        return 1;
    }
    const char *at = column;
    while (*at != '\0')
    {
        const size_t length = strcspn(at, ",");
        // The mark is the last character if it is one, and the type is the rest.
        const size_t mark_length = ((length != 0u) && (sass_name_place(s_mark_names, (unsigned int)SASS_MARK_COUNT,
                                                                       at + length - 1u, 1u) != SASS_MARK_COUNT))
                                       ? 1u
                                       : 0u;
        const unsigned int type =
            sass_name_place(s_type_names, (unsigned int)SASS_TYPE_COUNT, at, length - mark_length);
        const unsigned int mark =
            (mark_length == 0u) ? (unsigned int)SASS_MARK_NONE
                                : sass_name_place(s_mark_names, (unsigned int)SASS_MARK_COUNT, at + length - 1u, 1u);
        if ((type == SASS_TYPE_COUNT) || (form->operands == SASS_MACHINE_OPERANDS))
        {
            return 0;
        }
        form->type[form->operands] = type;
        form->mark[form->operands] = mark;
        form->operands += 1u;
        at += length + ((at[length] == ',') ? 1u : 0u);
    }
    return 1;
}

// Read what the part answered on the run channel into `machine`, from the .ksc of the machine file's stem at `path`.
// The answers are the soonest reads, each operation's the largest of its lines, and the last register a question's
// code can name. They also say how many registers past the highest named a container declares. A machine with no
// .ksc beside it contains no soonest read, every register, and no count past.
static void sass_answers_read(SassMachine *machine, const char *path)
{
    machine->register_last = SASS_MACHINE_UNANSWERED;
    machine->register_past = SASS_MACHINE_UNANSWERED;
    char ksc[1024];
    const size_t length = mmgr_text_length(path, PLATFORM_TEXT_TO_TERMINATOR);
    const int typed = (length > 4u) && (mmgr_text_equal(path + length - 4u, ".khw", PLATFORM_TEXT_TO_TERMINATOR));
    {
        VerbaLinea buffer_line = mmgr_line_over(ksc, sizeof(ksc));
        mmgr_line_put_max(&buffer_line, path, (size_t)((int)(typed ? (length - 4u) : length)));
        mmgr_line_put(&buffer_line, ".ksc");
        (void)mmgr_line_finish(&buffer_line);
    }
    // The .ksc as the run stores it (ksc.h).
    static KscFile s_file;
    static KscRow s_row;
    ksc_read(&s_file, ksc);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        char writer[SASS_MACHINE_TOKEN];
        char reader[SASS_MACHINE_TOKEN];
        if (!ksc_row_read(s_file.line[line], "run", &s_row) ||
            (!mmgr_text_equal(s_row.answer, "answers", PLATFORM_TEXT_TO_TERMINATOR)))
        {
            continue;
        }
        // A stall and a register are each at max eight hex digits, and an unsigned int represents each.
        const unsigned int answered = (unsigned int)s_row.value;
        if (mmgr_text_starts(s_row.question, "register last", PLATFORM_TEXT_TO_TERMINATOR))
        {
            machine->register_last = answered;
            continue;
        }
        if (mmgr_text_starts(s_row.question, "register past", PLATFORM_TEXT_TO_TERMINATOR))
        {
            machine->register_past = answered;
            continue;
        }
        const char *question = s_row.question;
        if (!line_characters(&question, "stall ") || !line_word(&question, writer, sizeof(writer)) ||
            !line_word(&question, reader, sizeof(reader)))
        {
            continue;
        }
        const unsigned int stall = answered;
        unsigned int at = 0u;
        while ((at < machine->soonests) &&
               (!mmgr_text_equal(machine->soonest[at].operation, writer, PLATFORM_TEXT_TO_TERMINATOR)))
        {
            at += 1u;
        }
        if (at == SASS_MACHINE_SOONEST)
        {
            continue;
        }
        if (at == machine->soonests)
        {
            {
                VerbaLinea buffer_line =
                    mmgr_line_over(machine->soonest[at].operation, sizeof(machine->soonest[at].operation));
                mmgr_line_put(&buffer_line, writer);
                (void)mmgr_line_finish(&buffer_line);
            }
            machine->soonest[at].stall = 0u;
            machine->soonests += 1u;
        }
        machine->soonest[at].stall = (stall > machine->soonest[at].stall) ? stall : machine->soonest[at].stall;
    }
    ksc_free(&s_file);
}

int sass_machine_read(SassMachine *machine, const char *path)
{
    // The machine file as the run stores it (qry_member_file).
    const unsigned char *file_bytes = NULL;
    unsigned long long file_size = 0ULL;
    unsigned long long file_at = 0ULL;
    if (!qry_member_file_blob(path, &file_bytes, &file_size))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  sass_machine: ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " could not be read");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    mmgr_bytes_set(machine, 0, sizeof(*machine));
    char line[SASS_MACHINE_TEXT + (SASS_MACHINE_TOKEN * (SASS_MACHINE_OPERANDS + 4u))];
    unsigned int version = 0u;
    int broken = 0;
    while (line_read(file_bytes, file_size, &file_at, line, sizeof(line)))
    {
        line[strcspn(line, "\r\n")] = '\0';
        if ((line[0] == '#') || (line[0] == '\0'))
        {
            continue;
        }
        if (mmgr_text_starts(line, "forms ", PLATFORM_TEXT_TO_TERMINATOR))
        {
            version = (unsigned int)strtoul(line + 6, NULL, 10);
            continue;
        }
        if (mmgr_text_starts(line, "part ", PLATFORM_TEXT_TO_TERMINATOR))
        {
            // A part's name is short, and the field cuts a longer one to what it fits.
            {
                VerbaLinea buffer_line = mmgr_line_over(machine->part, sizeof(machine->part));
                mmgr_line_put_max(&buffer_line, line + 5, (size_t)((int)(sizeof(machine->part) - 1u)));
                (void)mmgr_line_finish(&buffer_line);
            }
            continue;
        }
        if (!mmgr_text_starts(line, "form ", PLATFORM_TEXT_TO_TERMINATOR))
        {
            broken = 1;
            continue;
        }
        char operation[SASS_MACHINE_TOKEN];
        char types[SASS_MACHINE_TOKEN * SASS_MACHINE_OPERANDS];
        char runs[SASS_MACHINE_RUNS * 16u];
        char relation[SASS_MACHINE_TOKEN] = "";
        char signed_read[SASS_MACHINE_TOKEN] = "";
        unsigned long long low = 0ULL;
        unsigned long long high = 0ULL;
        // The text past the runs is the instruction the probe read the form as, and it contains spaces. At forms 2
        // it is also past the relation and the signedness.
        const char *seen = line + 5;
        int read = line_word(&seen, operation, sizeof(operation)) && line_word(&seen, types, sizeof(types)) &&
                   line_number(&seen, 1, &low) && line_number(&seen, 1, &high) && line_word(&seen, runs, sizeof(runs));
        read = read && ((version != 2u) || (line_word(&seen, relation, sizeof(relation)) &&
                                            line_word(&seen, signed_read, sizeof(signed_read))));
        while ((*seen == ' ') || (*seen == '\t'))
        {
            seen += 1;
        }
        if (!read)
        {
            broken = 1;
            continue;
        }
        if (machine->forms == SASS_MACHINE_FORMS)
        {
            machine->refused += 1u;
            continue;
        }
        SassForm *const form = &machine->form[machine->forms];
        mmgr_bytes_set(form, 0, sizeof(*form));
        {
            VerbaLinea buffer_line = mmgr_line_over(form->operation, sizeof(form->operation));
            mmgr_line_put(&buffer_line, operation);
            (void)mmgr_line_finish(&buffer_line);
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(form->text, sizeof(form->text));
            mmgr_line_put(&buffer_line, seen);
            (void)mmgr_line_finish(&buffer_line);
        }
        form->low = low;
        form->high = high;
        if (!mmgr_text_equal(relation, SASS_RUN_NO_PLACE_TEXT, PLATFORM_TEXT_TO_TERMINATOR))
        {
            {
                VerbaLinea buffer_line = mmgr_line_over(form->relation, sizeof(form->relation));
                mmgr_line_put(&buffer_line, relation);
                (void)mmgr_line_finish(&buffer_line);
            }
            form->signed_read = (mmgr_text_equal(signed_read, "1", PLATFORM_TEXT_TO_TERMINATOR)) ? 1 : 0;
        }
        broken = sass_types_read(form, types) ? broken : 1;
        broken = sass_runs_read(form, runs) ? broken : 1;
        machine->forms += 1u;
    }
    if (((version != 1u) && (version != 2u)) || broken || (machine->forms == 0u))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  sass_machine: ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " is not a machine file this reads (forms ");
            mmgr_line_decimal(output_line, version, 1u);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, machine->forms, 1u);
            mmgr_line_put(output_line, " read)");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    sass_answers_read(machine, path);
    return 1;
}
