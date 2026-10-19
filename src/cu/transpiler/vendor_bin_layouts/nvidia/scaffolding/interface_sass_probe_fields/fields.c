// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fields.c: the fields: each field read off the part, what writes first, a mark, a swap and an address swap, a redirect, and the fields named
#include "fields_internal.h"
#include "../../../../../includes/parsers/line_read.h"

// the answers interface_sass_run wrote for the write pass's cubins, read against the form the first instruction holds
// and labeled a bit a row to `record`. A bit the part refused, one it read inside a run the machine file records for
// an operand, one it read outside every recorded run, and one it left the answer unchanged are told apart
int fields_read(const char *machine_path, const char *instructions, const char *answers, const char *record)
{
    if (!sass_machine_read(&s_machine, machine_path))
    {
        fprintf(stderr, "the machine file %s did not read\n", machine_path);
        return 2;
    }
    char instruction[512];
    if (!fields_first(instructions, instruction, sizeof(instruction)))
    {
        fprintf(stderr, "the instructions %s hold no form\n", instructions);
        return 2;
    }
    SassInstructionParts parts;
    sass_instruction_read(instruction, &parts);
    const SassForm *const form = sass_machine_form(&s_machine, &parts);
    if (form == NULL)
    {
        fprintf(stderr, "the machine file holds no form for %s\n", instruction);
        return 2;
    }
    FILE *const in = fopen(answers, "rb");
    FILE *const out = fopen(record, "wb");
    if ((in == NULL) || (out == NULL))
    {
        fprintf(stderr, "the answers %s or the record %s was not reached\n", answers, record);
        return 2;
    }
    fprintf(out, "# A form's fields, found on the part\n\n");
    fprintf(out, "Written by `interface_sass_probe_fields read`. The form is `%s`. Each row is one of its operation "
                 "bits turned over and run on the part, against the answer the form gives untouched. A bit the part "
                 "refuses, one it reads inside a run the machine file records for an operand, one it reads outside "
                 "every recorded run, and one that leaves the answer unchanged are told apart. A bit held off the "
                 "part, whose turned operation key holds a control transfer, a wait or no form in the machine file, "
                 "is marked skipped and run on nothing.\n\n",
            instruction);
    fprintf(out, "| bit | operand | part |\n|---|---|---|\n");
    char line[256];
    char hidden_bits[512] = "";
    unsigned long long baseline = 0ull;
    int have_baseline = 0;
    unsigned int refused = 0u;
    unsigned int inside = 0u;
    unsigned int hidden = 0u;
    unsigned int unread = 0u;
    while (fgets(line, sizeof(line), in) != NULL)
    {
        unsigned int number = 0u;
        char kind[64];
        char word[64];
        const char *read = line;
        word[0] = '\0';
        if (!line_unsigned(&read, &number) || !line_word(&read, kind, sizeof(kind)))
        {
            continue;
        }
        // the word after the kind, where the line holds one
        line_word(&read, word, sizeof(word));
        if (number == 0u)
        {
            const char *base = word;
            have_baseline = (strcmp(kind, "answered") == 0) && line_number(&base, 1, &baseline);
            continue;
        }
        const unsigned int bit = number - 1u;
        const unsigned int operand = fields_operand_at(form, bit);
        const int in_run = (operand < form->operands);
        char place[32];
        if (in_run)
        {
            snprintf(place, sizeof(place), "%u", operand);
        }
        else
        {
            snprintf(place, sizeof(place), "-");
        }
        const char *part = NULL;
        if ((strcmp(kind, "refused") == 0) || (strcmp(kind, "hung") == 0) || (strcmp(kind, "skipped") == 0))
        {
            // a bit the part refused, one it ran too long on, and one held off the part as unsafe to run are each no
            // operand field; they share the first column and the row says which
            refused += 1u;
            part = (kind[0] == 'h') ? "hung" : ((kind[0] == 's') ? "skipped" : "refused");
        }
        else
        {
            unsigned long long answered = 0ull;
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
                const size_t at = strlen(hidden_bits);
                snprintf(&hidden_bits[at], sizeof(hidden_bits) - at, "%s%u", (at == 0u) ? "" : " ", bit);
            }
        }
        fprintf(out, "| %u | %s | %s |\n", bit, place, part);
    }
    fprintf(out, "\n%u refused, %u read inside a recorded run, %u read outside every recorded run, %u unread.\n",
            refused, inside, hidden, unread);
    fclose(in);
    fclose(out);
    // a tab-delimited line the gathering script turns into one row: the form, its counts, and the bits it reads that
    // the machine file records for no operand
    printf("FORMFIELDS\t%s\t%u\t%u\t%u\t%u\t%s\n", instruction, refused, inside, hidden, unread, hidden_bits);
    return 0;
}

// 1 where the operation writes its first register operand, which the redirect may move to the observed register. A
// return, a register branch or a call reads its first register instead and leaves it where it stands; a store, a branch
// to a label and a predicate setter carry no register first operand and never reach here
static int fields_writes_first(const char *operation)
{
    static const char *const reads[] = {"RET", "BRX", "JMX", "CALL", "BRA", "JMP", "EXIT", "BAR", "NOP"};
    for (unsigned int at = 0u; at < (sizeof(reads) / sizeof(reads[0])); at += 1u)
    {
        if (strncmp(operation, reads[at], strlen(reads[at])) == 0)
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
// and `put` itself is the register read or written. Any other suffix is kept
static void fields_swap(const char *operand, const char *put, char *out, size_t room)
{
    size_t length = 0u;
    if (operand[0] == 'R')
    {
        length = (operand[1] == 'Z') ? 2u : (1u + strspn(operand + 1, "0123456789"));
    }
    const char *const rest = operand + length;
    const size_t kept = strlen(rest) - (sass_high_half(operand) ? 3u : 0u);
    snprintf(out, room, "%s%.*s", put, (int)kept, rest);
}

// `operand`, an address, written to `out` with the register its address is read from replaced by `put`: the register
// after the last `[`. A descriptor's uniform register before it is kept, and an address whose last `[` holds no
// register is copied as it stands. 1, or 0 where `out` will not hold it
static int fields_address_swap(const char *operand, const char *put, char *out, size_t room)
{
    const char *const open = strrchr(operand, '[');
    if ((open == NULL) || (open[1] != 'R') || ((open[2] != 'Z') && ((open[2] < '0') || (open[2] > '9'))))
    {
        return snprintf(out, room, "%s", operand) < (int)room;
    }
    char swapped[SASS_MACHINE_TOKEN];
    fields_swap(open + 1, put, swapped, sizeof(swapped));
    return snprintf(out, room, "%.*s%s", (int)(open + 1 - operand), operand, swapped) < (int)room;
}

// `instruction` written to `redirected`, rebuilt from its parts, with its first operand moved where the kernel reads it
// and each source register operand moved to the next of the head's set registers, where the form's result and its
// source fields reach the answer. A global load's address is moved onto the case's, where the load reads the case's
// words and not memory no thread holds. A first operand that is a register moves to R8, the register the kernel stores
// as the answer. A first operand that is a predicate moves to P0, and `observe` takes the line that turns P0 into a
// value in R8; it is left empty otherwise. A form whose first operand is neither, or is one the operation reads, is
// copied as it stands, since nothing of it would be observed through R8. 1, or 0 where `redirected` or `observe` will
// not hold it
int fields_redirect(const char *instruction, char *redirected, size_t room, char *observe, size_t observe_room)
{
    static const char *const sources[FIELDS_SOURCES] = {"R10", "R12", "R14", "R16", "R18", "R20"};
    SassInstructionParts parts;
    sass_instruction_read(instruction, &parts);
    observe[0] = '\0';
    const int writes_register = (parts.operands != 0u) && (parts.kind[0] == SASS_OPERAND_REGISTER);
    const int writes_predicate = (parts.operands != 0u) && (parts.kind[0] == SASS_OPERAND_PREDICATE);
    if ((!writes_register && !writes_predicate) || !fields_writes_first(parts.operation))
    {
        return snprintf(redirected, room, "%s", instruction) < (int)room;
    }
    if (writes_predicate && (snprintf(observe, observe_room, "%s", FIELDS_LINES_PREDICATE) >= (int)observe_room))
    {
        return 0;
    }
    size_t at = 0u;
    if (parts.guard[0] != '\0')
    {
        at += (size_t)snprintf(&redirected[at], (at < room) ? (room - at) : 0u, "%s ", parts.guard);
    }
    at += (size_t)snprintf(&redirected[at], (at < room) ? (room - at) : 0u, "%s", parts.operation);
    unsigned int source = 0u;
    for (unsigned int operand = 0u; operand < parts.operands; operand += 1u)
    {
        char text[SASS_MACHINE_TOKEN];
        const char *put = NULL;
        if (parts.kind[operand] == SASS_OPERAND_REGISTER)
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
            snprintf(text, sizeof(text), "P0");
        }
        else if (put != NULL)
        {
            fields_swap(parts.operand[operand], put, text, sizeof(text));
        }
        else if ((parts.kind[operand] == SASS_OPERAND_ADDRESS) && (strncmp(parts.operation, "LDG", 3u) == 0))
        {
            if (!fields_address_swap(parts.operand[operand], FIELDS_LOAD_ADDRESS, text, sizeof(text)))
            {
                return 0;
            }
        }
        else
        {
            snprintf(text, sizeof(text), "%s", parts.operand[operand]);
        }
        at += (size_t)snprintf(&redirected[at], (at < room) ? (room - at) : 0u, "%s%s%s", (operand == 0u) ? " " : ", ",
                               fields_mark(parts.mark[operand]), text);
    }
    return at < room;
}

// the first instruction of `instructions`, moved where the kernel reads it and assembled alone, with each operation bit
// turned and read back through the machine file, nothing run. The untouched encoding is printed first, under `base`,
// with what it reads back as. Each bit is printed with what its turned encoding reads
// back as: `no form` where no form holds it, `same text` where it reads back as the instruction itself, which no field
// of the machine file holds, and the text it reads back as otherwise. Lines begin `NAMES`, for a script to gather. 0,
// or 2 where nothing was read
int fields_names(const char *machine_path, const char *instructions)
{
    if (!sass_machine_read(&s_machine, machine_path))
    {
        fprintf(stderr, "the machine file %s did not read\n", machine_path);
        return 2;
    }
    char instruction[512];
    char redirected[512];
    char observe[64];
    unsigned long long low = 0ull;
    unsigned long long high = 0ull;
    if (!fields_first(instructions, instruction, sizeof(instruction)) ||
        !fields_redirect(instruction, redirected, sizeof(redirected), observe, sizeof(observe)) ||
        !sass_assemble(&s_machine, redirected, 0ull, 0ull, SASS_CONTROL_SAFE, &low, &high))
    {
        fprintf(stderr, "the instructions %s hold no form that assembles\n", instructions);
        return 2;
    }
    char text[256];
    char base_text[256];
    SassInstructionParts base;
    if (!sass_encoding_read(&s_machine, low, high, 0ull, base_text, sizeof(base_text)))
    {
        fprintf(stderr, "%s does not read back\n", redirected);
        return 2;
    }
    sass_instruction_read(base_text, &base);
    printf("NAMES\t%s\tbase\t%s\n", instruction, base_text);
    for (unsigned int bit = 0u; bit < FIELDS_OPERATION_BITS; bit += 1u)
    {
        const unsigned long long turned_low = (bit < 64u) ? (low ^ (1ull << bit)) : low;
        const unsigned long long turned_high = (bit < 64u) ? high : (high ^ (1ull << (bit - 64u)));
        if (!sass_encoding_read(&s_machine, turned_low, turned_high, 0ull, text, sizeof(text)))
        {
            printf("NAMES\t%s\t%u\tno form\n", instruction, bit);
            continue;
        }
        if (strcmp(text, base_text) == 0)
        {
            printf("NAMES\t%s\t%u\tsame text\n", instruction, bit);
            continue;
        }
        printf("NAMES\t%s\t%u\t%s\n", instruction, bit, text);
    }
    return 0;
}
