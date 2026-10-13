// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// machine.c: the machine file: a form of the machine, the exit encoding, the operations that transfer control or wait, the barrier and the schedule, the machine taken from text, the runs, kinds and answers written and read, and the file written and read
#include "sass_machine_internal.h"

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
    unsigned long long found = 0ull;
    for (unsigned int number = 0u; number < machine->forms; number += 1u)
    {
        found = (strcmp(machine->form[number].operation, "EXIT") == 0) ? machine->form[number].low : found;
    }
    return found;
}

int sass_operation_control_or_wait(const char *operation)
{
    static const char *const s_unsafe[] = {"BRA",  "BRX",   "JMP",    "JMX",       "CALL", "RET",    "EXIT",   "BSSY",
                                           "BSYNC", "BREAK", "BMOV",   "WARPSYNC",  "YIELD", "BAR",   "DEPBAR", "NANOSLEEP",
                                           "BPT",  "RTT",   "KILL",   "RPCMOV",    "RETIRE", "PMTRIG"};
    for (unsigned int at = 0u; at < (sizeof(s_unsafe) / sizeof(s_unsafe[0])); at += 1u)
    {
        if (strncmp(operation, s_unsafe[at], strlen(s_unsafe[at])) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int sass_barrier_set(unsigned long long high, unsigned int first)
{
    return (unsigned int)((high >> (first - 64u)) & 7ull) != SASS_BARRIER_NONE;
}

unsigned int sass_operation_schedule(const SassMachine *machine, const char *operation, unsigned int *soonest)
{
    // the names before the first dot NVIDIA's compiler gives a write barrier at every place, or a read barrier
    static const char *const s_late[] = {"LDG", "LDS", "S2R", "S2UR", "F2I", "I2F", "MUFU", "ATOMG"};
    static const char *const s_store[] = {"STG", "STS", "STL", "ST", "RED"};
    const size_t base = strcspn(operation, ".");
    *soonest = SASS_STALL_LONGEST;
    for (unsigned int at = 0u; at < (sizeof(s_late) / sizeof(s_late[0])); at += 1u)
    {
        if ((strlen(s_late[at]) == base) && (strncmp(operation, s_late[at], base) == 0))
        {
            return SASS_SCHEDULE_LATE;
        }
    }
    for (unsigned int at = 0u; at < (sizeof(s_store) / sizeof(s_store[0])); at += 1u)
    {
        if ((strlen(s_store[at]) == base) && (strncmp(operation, s_store[at], base) == 0))
        {
            return SASS_SCHEDULE_STORE;
        }
    }
    for (unsigned int at = 0u; at < machine->soonests; at += 1u)
    {
        if (strcmp(operation, machine->soonest[at].operation) == 0)
        {
            *soonest = machine->soonest[at].stall;
        }
    }
    return SASS_SCHEDULE_FIXED;
}

int sass_machine_take(SassMachine *machine, const char *text, unsigned long long low, unsigned long long high,
                      SassForm **kept)
{
    SassInstructionParts parts;
    sass_instruction_read(text, &parts);
    const SassForm *const found = sass_machine_form(machine, &parts);
    if (found != NULL)
    {
        *kept = &machine->form[found - machine->form];
        return 1;
    }
    if (machine->forms == SASS_MACHINE_FORMS)
    {
        machine->refused += 1u;
        *kept = NULL;
        return 0;
    }
    SassForm *const form = &machine->form[machine->forms];
    memset(form, 0, sizeof(*form));
    memcpy(form->operation, parts.operation, sizeof(form->operation));
    form->operands = parts.operands;
    for (unsigned int place = 0u; place < parts.operands; place += 1u)
    {
        form->kind[place] = parts.kind[place];
        form->mark[place] = parts.mark[place];
    }
    form->low = low;
    form->high = high;
    snprintf(form->text, sizeof(form->text), "%s", text);
    machine->forms += 1u;
    *kept = form;
    return 1;
}

int sass_text_placed(const SassForm *form, char *written, size_t room)
{
    SassInstructionParts parts;
    sass_instruction_read(form->text, &parts);
    int printed = snprintf(written, room, "%s%s%s", parts.guard, (parts.guard[0] != '\0') ? " " : "", parts.operation);
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
        printed = (place == SASS_RUN_NO_PLACE)
                      ? snprintf(written + at, room - at, "%s%s%s", between, mark, parts.operand[operand])
                      : snprintf(written + at, room - at, "%s%s{%u}", between, mark, place);
        at += (printed > 0) ? (size_t)printed : room;
    }
    return (at < room) ? 1 : 0;
}

// the runs of `form` written into `written` as one column, each with its place, "none" where the probe found it none
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
            snprintf(place, sizeof(place), "%s", SASS_RUN_NO_PLACE_TEXT);
        }
        else
        {
            snprintf(place, sizeof(place), "%u", run->place);
        }
        const int printed = snprintf(written + at, room - at, "%s%u:%u-%u:%s", (number == 0u) ? "" : ";", run->operand,
                                     run->first, run->last, place);
        // snprintf gives the letters it would have written, which the room below bounds
        at += ((printed > 0) && ((size_t)printed < (room - at))) ? (size_t)printed : 0u;
    }
    if (form->runs == 0u)
    {
        snprintf(written, room, "none");
    }
}

// one form's runs column read into `form`, each run's place after its last bit where the column holds one and
// SASS_RUN_NO_PLACE where it does not: 1, or 0 where a run does not read
static int sass_runs_read(SassForm *form, const char *column)
{
    form->runs = 0u;
    if (strcmp(column, "none") == 0)
    {
        return 1;
    }
    const char *at = column;
    while (*at != '\0')
    {
        unsigned int operand = 0u;
        unsigned int first = 0u;
        unsigned int last = 0u;
        int read = 0;
        if ((sscanf(at, "%u:%u-%u%n", &operand, &first, &last, &read) != 3) || (form->runs == SASS_MACHINE_RUNS))
        {
            return 0;
        }
        unsigned int place = SASS_RUN_NO_PLACE;
        if ((at[read] == ':') && (strncmp(&at[read + 1], SASS_RUN_NO_PLACE_TEXT, strlen(SASS_RUN_NO_PLACE_TEXT)) != 0))
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

// the kinds and marks of `form` written into `written` as one column, "none" where it takes no operand
static void sass_kinds_write(const SassForm *form, char *written, size_t room)
{
    size_t at = 0u;
    written[0] = '\0';
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        const int printed = snprintf(written + at, room - at, "%s%s%s", (place == 0u) ? "" : ",",
                                     s_kind_names[form->kind[place]], s_mark_names[form->mark[place]]);
        // snprintf gives the letters it would have written, which the room below bounds
        at += ((printed > 0) && ((size_t)printed < (room - at))) ? (size_t)printed : 0u;
    }
    if (form->operands == 0u)
    {
        snprintf(written, room, "none");
    }
}

int sass_machine_write(const SassMachine *machine, const char *path)
{
    QryText file = {0};
    qry_text_printed(&file, "forms 2" SASS_LINE_END);
    qry_text_printed(&file,
                     "# The part's instructions as the cell's probes read them back: one line a form, an operation "
                     "and" SASS_LINE_END
                     "# the kind and mark of each printed operand, with the encoding the form was first seen with "
                     "and" SASS_LINE_END "# the instruction it was seen as. The format is the comment at the head of "
                     "sass_machine.h." SASS_LINE_END);
    qry_text_printed(&file, "part %s" SASS_LINE_END, machine->part);
    for (unsigned int number = 0u; number < machine->forms; number += 1u)
    {
        const SassForm *const form = &machine->form[number];
        char kinds[SASS_MACHINE_TOKEN * SASS_MACHINE_OPERANDS];
        char runs[SASS_MACHINE_RUNS * 16u];
        sass_kinds_write(form, kinds, sizeof(kinds));
        sass_runs_write(form, runs, sizeof(runs));
        const int asked = (form->relation[0] != '\0');
        qry_text_printed(&file, "form %s %s 0x%016llx 0x%016llx %s %s %s %s" SASS_LINE_END, form->operation, kinds,
                         form->low, form->high, runs, asked ? form->relation : SASS_RUN_NO_PLACE_TEXT,
                         asked ? ((form->signed_read != 0) ? "1" : "0") : SASS_RUN_NO_PLACE_TEXT, form->text);
    }
    // handed to the run's writer whole, which writes it in the file's place (qry_filed)
    if (qry_text_filed(qry_run(), path, &file) == 0ull)
    {
        printf("  sass_machine: %s could not be written: no run's writer runs, or it stands outside the tree\n", path);
        return 0;
    }
    return 1;
}

// the place of `word` among the names, or `count` where it is none of them
static unsigned int sass_name_place(const char *const *names, unsigned int count, const char *word, size_t length)
{
    unsigned int found = count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        found = ((strlen(names[at]) == length) && (strncmp(names[at], word, length) == 0)) ? at : found;
    }
    return found;
}

// one form's kinds column read into `form`: 1, or 0 where a kind or a mark is none the writer names
static int sass_kinds_read(SassForm *form, const char *column)
{
    form->operands = 0u;
    if (strcmp(column, "none") == 0)
    {
        return 1;
    }
    const char *at = column;
    while (*at != '\0')
    {
        const size_t length = strcspn(at, ",");
        // the mark is the last letter where it is one, and the kind the rest
        const size_t mark_length = ((length != 0u) && (sass_name_place(s_mark_names, (unsigned int)SASS_MARK_COUNT,
                                                                      at + length - 1u, 1u) != SASS_MARK_COUNT))
                                       ? 1u
                                       : 0u;
        const unsigned int kind =
            sass_name_place(s_kind_names, (unsigned int)SASS_KIND_COUNT, at, length - mark_length);
        const unsigned int mark =
            (mark_length == 0u)
                ? (unsigned int)SASS_MARK_NONE
                : sass_name_place(s_mark_names, (unsigned int)SASS_MARK_COUNT, at + length - 1u, 1u);
        if ((kind == SASS_KIND_COUNT) || (form->operands == SASS_MACHINE_OPERANDS))
        {
            return 0;
        }
        form->kind[form->operands] = kind;
        form->mark[form->operands] = mark;
        form->operands += 1u;
        at += length + ((at[length] == ',') ? 1u : 0u);
    }
    return 1;
}

// what the part answered on the run channel, read from the .ksc of the machine file's stem at `path` into `machine`:
// the soonest reads, each operation's the largest of its lines, the last register a question's code can name, and how
// many registers past the highest its code names a container declares. A machine with no .ksc beside it holds no
// soonest read, every register, and no count past
static void sass_answers_read(SassMachine *machine, const char *path)
{
    machine->register_last = SASS_MACHINE_UNANSWERED;
    machine->register_past = SASS_MACHINE_UNANSWERED;
    char ksc[1024];
    const size_t length = strlen(path);
    const int typed = (length > 4u) && (strcmp(path + length - 4u, ".khw") == 0);
    snprintf(ksc, sizeof(ksc), "%.*s.ksc", (int)(typed ? (length - 4u) : length), path);
    // the .ksc as the run holds it (ksc.h)
    static KscFile s_file;
    static KscRow s_row;
    ksc_read(&s_file, ksc);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        char writer[SASS_MACHINE_TOKEN];
        char reader[SASS_MACHINE_TOKEN];
        if (!ksc_row_read(s_file.line[line], "run", &s_row) || (strcmp(s_row.answer, "answers") != 0))
        {
            continue;
        }
        // a stall and a register are each at most eight hex digits, which an unsigned int holds
        const unsigned int answered = (unsigned int)s_row.value;
        if (strncmp(s_row.question, "register last", 13u) == 0)
        {
            machine->register_last = answered;
            continue;
        }
        if (strncmp(s_row.question, "register past", 13u) == 0)
        {
            machine->register_past = answered;
            continue;
        }
        if (sscanf(s_row.question, "stall %63s %63s", writer, reader) != 2)
        {
            continue;
        }
        const unsigned int stall = answered;
        unsigned int at = 0u;
        while ((at < machine->soonests) && (strcmp(machine->soonest[at].operation, writer) != 0))
        {
            at += 1u;
        }
        if (at == SASS_MACHINE_SOONEST)
        {
            continue;
        }
        if (at == machine->soonests)
        {
            snprintf(machine->soonest[at].operation, sizeof(machine->soonest[at].operation), "%s", writer);
            machine->soonest[at].stall = 0u;
            machine->soonests += 1u;
        }
        machine->soonest[at].stall = (stall > machine->soonest[at].stall) ? stall : machine->soonest[at].stall;
    }
    ksc_free(&s_file);
}

int sass_machine_read(SassMachine *machine, const char *path)
{
    // the machine file as the run holds it (qry_file_read)
    unsigned char *file_bytes = NULL;
    unsigned long long file_size = 0ull;
    unsigned long long file_at = 0ull;
    if (!qry_file_read(qry_run(), path, &file_bytes, &file_size))
    {
        printf("  sass_machine: %s could not be read\n", path);
        return 0;
    }
    memset(machine, 0, sizeof(*machine));
    char line[SASS_MACHINE_TEXT + (SASS_MACHINE_TOKEN * (SASS_MACHINE_OPERANDS + 4u))];
    unsigned int version = 0u;
    int broken = 0;
    while (qry_line_next(file_bytes, file_size, &file_at, line, sizeof(line)))
    {
        line[strcspn(line, "\r\n")] = '\0';
        if ((line[0] == '#') || (line[0] == '\0'))
        {
            continue;
        }
        if (strncmp(line, "forms ", 6u) == 0)
        {
            version = (unsigned int)strtoul(line + 6, NULL, 10);
            continue;
        }
        if (strncmp(line, "part ", 5u) == 0)
        {
            // a part's name is short, and a longer one is cut to what the field holds
            snprintf(machine->part, sizeof(machine->part), "%.*s", (int)(sizeof(machine->part) - 1u), line + 5);
            continue;
        }
        if (strncmp(line, "form ", 5u) != 0)
        {
            broken = 1;
            continue;
        }
        char operation[SASS_MACHINE_TOKEN];
        char kinds[SASS_MACHINE_TOKEN * SASS_MACHINE_OPERANDS];
        char runs[SASS_MACHINE_RUNS * 16u];
        char relation[SASS_MACHINE_TOKEN] = "";
        char signed_read[SASS_MACHINE_TOKEN] = "";
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        int at = 0;
        // the text past the runs, and at forms 2 past the relation and the signedness, is the instruction the form was
        // seen as, and holds spaces
        const int read = (version == 2u)
                             ? (sscanf(line + 5, "%63s %511s %llx %llx %511s %63s %63s %n", operation, kinds, &low,
                                       &high, runs, relation, signed_read, &at) == 7)
                             : (sscanf(line + 5, "%63s %511s %llx %llx %511s %n", operation, kinds, &low, &high, runs,
                                       &at) == 5);
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
        memset(form, 0, sizeof(*form));
        snprintf(form->operation, sizeof(form->operation), "%s", operation);
        snprintf(form->text, sizeof(form->text), "%s", line + 5 + at);
        form->low = low;
        form->high = high;
        if (strcmp(relation, SASS_RUN_NO_PLACE_TEXT) != 0)
        {
            snprintf(form->relation, sizeof(form->relation), "%s", relation);
            form->signed_read = (strcmp(signed_read, "1") == 0) ? 1 : 0;
        }
        broken = sass_kinds_read(form, kinds) ? broken : 1;
        broken = sass_runs_read(form, runs) ? broken : 1;
        machine->forms += 1u;
    }
    free(file_bytes);
    if (((version != 1u) && (version != 2u)) || broken || (machine->forms == 0u))
    {
        printf("  sass_machine: %s is not a machine file this reads (forms %u, %u read)\n", path, version,
               machine->forms);
        return 0;
    }
    sass_answers_read(machine, path);
    return 1;
}
