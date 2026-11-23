// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_probe_read.c: running the disassembler through the interface and reading what it printed
#include "../../../../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "interface_sass_probe.h"
#include "memoria_operor/memoria_operor.h"

// the output a process gives, a cubin's whole listing with its encodings
#define SASS_OUTPUT_CAPACITY (4u * 1024u * 1024u)
// a process's limit: one disassembly
#define SASS_LIMIT 120000000ULL

static char s_sass_output[SASS_OUTPUT_CAPACITY];

int sass_run(char *const *command, const char *output_path)
{
    const InterfaceProbe probe = {command, output_path, SASS_LIMIT};
    InterfaceAnswer answer;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &answer, .bytes = sizeof(answer), .val = 0);
    answer.output = s_sass_output;
    answer.output_capacity = sizeof(s_sass_output);
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    if (interface_probe_run(&probe, &answer, &error) != 0L)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, command[0]);
            mmgr_line_put(output_line, ": the interface failed, module ");
            mmgr_line_decimal_signed(output_line, (int)error.module);
            mmgr_line_put(output_line, " site ");
            mmgr_line_decimal(output_line, error.site, 1u);
            mmgr_line_put(output_line, " status ");
            mmgr_line_decimal_signed(output_line, error.status);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return -1;
    }
    if (answer.output_bytes >= (unsigned long long)sizeof(s_sass_output))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, command[0]);
            mmgr_line_put(output_line, ": ");
            mmgr_line_decimal(output_line, answer.output_bytes, 1u);
            mmgr_line_put(output_line, " bytes of output, more than the ");
            mmgr_line_decimal(output_line, SASS_OUTPUT_CAPACITY, 1u);
            mmgr_line_put(output_line, " read");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return -1;
    }
    if (answer.ending != INTERFACE_ENDING_EXITED)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, command[0]);
            mmgr_line_put(output_line, ": ");
            mmgr_line_put(output_line, interface_ending_name(answer.ending));
            mmgr_line_put(output_line, ", code ");
            mmgr_line_decimal(output_line, answer.code, 1u);
            mmgr_line_put(output_line, ", fault ");
            mmgr_line_put(output_line, interface_fault_name(answer.fault));
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return -1;
    }
    // an exit status the interface read as exited is a process's own, which fits an int
    return (int)answer.code;
}

const char *sass_output(void)
{
    return s_sass_output;
}

// the length of the address comment "/*0a50*/" at `text`, 0 where there is none
static size_t sass_address_length(const char *text)
{
    if ((text[0] != '/') || (text[1] != '*'))
    {
        return 0u;
    }
    size_t digits = 0u;
    while ((text[2u + digits] != '\0') &&
           (EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = "0123456789abcdef", .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .byte = text[2u + digits]) != NULL))
    {
        digits += 1u;
    }
    return ((digits >= 4u) && (text[2u + digits] == '*') && (text[3u + digits] == '/')) ? (4u + digits) : 0u;
}

// the encoding word in the comment "/* 0x0000000500057210 */" at or after `text` on its line; 0 and `found` 0 where
// there is none
static unsigned long long sass_word_read(const char *text, const char *line_end, int *found)
{
    const char *const start = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                         .other = "/* 0x", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
    *found = (start != NULL) && (start < line_end);
    return *found ? EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = start + 5, .end = NULL, .base = 16) : 0ULL;
}

int sass_listing_read(const char *output, SassListing *listing)
{
    listing->count = 0u;
    int encoded = 0;
    const char *line = output;
    while (*line != '\0')
    {
        const char *const newline =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\n');
        const char *const line_end =
            (newline != NULL)
                ? newline
                : (line + EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR));
        const char *address = line;
        size_t address_length = 0u;
        while ((address < line_end) && (address_length == 0u))
        {
            address_length = sass_address_length(address);
            address += (address_length == 0u) ? 1 : 0;
        }
        if (address_length != 0u)
        {
            if (listing->count == SASS_LISTING_LIMIT)
            {
                return 0;
            }
            SassInstruction *const instruction = &listing->instructions[listing->count];
            listing->count += 1u;
            instruction->address =
                EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = address + 2, .end = NULL, .base = 16);
            const char *text = address + address_length;
            while ((text < line_end) && (*text == ' '))
            {
                text += 1;
            }
            const char *end = text;
            while ((end < line_end) && (*end != ';'))
            {
                end += 1;
            }
            const char *const semicolon = end;
            while ((end > text) && (end[-1] == ' '))
            {
                end -= 1;
            }
            // the text runs from its start to the ';', which lies past it
            size_t length = (size_t)(end - text);
            length = (length < (SASS_TEXT - 1u)) ? length : (SASS_TEXT - 1u);
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = instruction->text, .src = text, .bytes = length);
            instruction->text[length] = '\0';
            instruction->low = sass_word_read(semicolon, line_end, &encoded);
            instruction->high = 0ULL;
        }
        else if (encoded && (listing->count != 0u))
        {
            int found = 0;
            const unsigned long long high = sass_word_read(line, line_end, &found);
            listing->instructions[listing->count - 1u].high = found ? high : 0ULL;
            encoded = 0;
        }
        line = (newline != NULL) ? (newline + 1) : line_end;
    }
    return 1;
}

// `length` characters of `text` into `into`, cut to its capacity, spaces trimmed from both ends
static void sass_token_copy(char *into, const char *text, size_t length)
{
    while ((length != 0u) && (*text == ' '))
    {
        text += 1;
        length -= 1u;
    }
    while ((length != 0u) && (text[length - 1u] == ' '))
    {
        length -= 1u;
    }
    length = (length < (SASS_OPERAND_TEXT - 1u)) ? length : (SASS_OPERAND_TEXT - 1u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = into, .src = text, .bytes = length);
    into[length] = '\0';
}

void sass_parts_read(const char *text, SassParts *parts)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = parts, .bytes = sizeof(*parts), .val = 0);
    while (*text == ' ')
    {
        text += 1;
    }
    if (*text == '@')
    {
        const size_t length =
            EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " ");
        sass_token_copy(parts->predicate, text, length);
        text += length;
        while (*text == ' ')
        {
            text += 1;
        }
    }
    const size_t operation_length =
        EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " ");
    sass_token_copy(parts->operation, text, operation_length);
    text += operation_length;
    // the operands, split at each comma outside brackets
    int depth = 0;
    const char *start = text;
    for (const char *walk = text;; walk += 1)
    {
        const char character = *walk;
        depth += ((character == '[') || (character == '{') || (character == '(')) ? 1 : 0;
        depth -= ((character == ']') || (character == '}') || (character == ')')) ? 1 : 0;
        if ((character == '\0') || ((character == ',') && (depth == 0)))
        {
            // an operand runs from its start to the comma or the end past it
            const size_t length = (size_t)(walk - start);
            if ((parts->operands < SASS_OPERANDS) &&
                (EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = start, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = " ") < length))
            {
                sass_token_copy(parts->operand[parts->operands], start, length);
                parts->operands += 1u;
            }
            start = walk + 1;
        }
        if (character == '\0')
        {
            break;
        }
    }
}

// the encodings at `chosen` written to `path`, the low word then the high, each byte by byte from the lowest: 1, or 0
// where the file could not be written
static int sass_binary_write(const char *path, const unsigned long long *low, const unsigned long long *high,
                             const unsigned int *chosen, unsigned int count)
{
    PlatformFile *const file = platform_file_open(path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    int written = 1;
    for (unsigned int number = 0u; number < count; number += 1u)
    {
        const unsigned long long words[2] = {low[chosen[number]], high[chosen[number]]};
        for (unsigned int word = 0u; word < 2u; word += 1u)
        {
            for (unsigned int byte = 0u; byte < 8u; byte += 1u)
            {
                // one byte of the word, the lowest first
                written =
                    written && (platform_file_char_put((int)((words[word] >> (8u * byte)) & 0xffULL), file) != EOF);
            }
        }
    }
    return (platform_file_close(file) == 0) && written;
}

static SassListing s_sass_decoded;

// 1 where `operation` counts its last operand as a distance from the instruction after it: BRA with any modifier,
// CALL.REL and BSSY. JMP and CALL.ABS name an address outright
static int sass_operation_relative(const char *operation)
{
    const int branch = (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                   .other = "BRA", .ci = EMBED_FALSE)) &&
                       ((operation[3] == '\0') || (operation[3] == '.'));
    const int call = (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                 .other = "CALL.REL", .ci = EMBED_FALSE)) &&
                     ((operation[8] == '\0') || (operation[8] == '.'));
    const int sync = (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                 .other = "BSSY", .ci = EMBED_FALSE)) &&
                     ((operation[4] == '\0') || (operation[4] == '.'));
    return branch || call || sync;
}

// A relative target in `text`, which the disassembler prints as the address it lands on, written as its distance from
// the instruction after the one at `address`, where the encoding counts it from. One encoding decoded at two addresses
// prints two targets and encodes one distance: read as an address, every bit turned would move the target, since each
// turned encoding lies at an address of its own
static void sass_target_relative(char *text, unsigned long long address)
{
    const char *operation = text;
    if (operation[0] == '@')
    {
        operation += EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                .other = " ");
        operation += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                .other = " ");
    }
    char name[SASS_OPERAND_TEXT];
    {
        VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
        mmgr_line_put_max(&buffer_line, operation,
                          (size_t)((int)EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = operation,
                                                   .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " ")));
        (void)mmgr_line_finish(&buffer_line);
    }
    char *const comma =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ',');
    char *target = (comma != NULL) ? (comma + 1)
                                   : (char *)(operation + EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name,
                                                                     .cap = PLATFORM_TEXT_TO_TERMINATOR));
    target += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = target, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " ");
    const int negative = (target[0] == '-');
    if (!sass_operation_relative(name) ||
        (!EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = target + (negative ? 1 : 0),
                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "0x", .ci = EMBED_FALSE)))
    {
        return;
    }
    const unsigned long long magnitude =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = target + (negative ? 1 : 0), .end = NULL, .base = 16);
    // the address and the distance are both two's complement words of 64 bits, and their difference is taken as one
    const long long distance = (long long)((negative ? (0ULL - magnitude) : magnitude) - (address + 16ULL));
    const size_t room = SASS_TEXT - (size_t)(target - text);
    {
        VerbaLinea buffer_line = mmgr_line_over(target, room);
        mmgr_line_put(&buffer_line, (distance < 0) ? "-" : "");
        mmgr_line_put(&buffer_line, "0x");
        mmgr_line_hex(&buffer_line, (unsigned long long)((distance < 0) ? -distance : distance), 1u);
        (void)mmgr_line_finish(&buffer_line);
    }
}

// the encodings at `chosen` decoded, their texts into `texts` at the same places: the disassembler's exit status, or
// -1 where it did not exit or printed a line at an address past them
static int sass_decode_pass(const char *architecture, const char *path, const unsigned long long *low,
                            const unsigned long long *high, const unsigned int *chosen, unsigned int count,
                            char (*texts)[SASS_TEXT])
{
    char binary[1024];
    char output[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(binary, sizeof(binary));
        mmgr_line_put(&buffer_line, path);
        mmgr_line_put(&buffer_line, ".bin");
        (void)mmgr_line_finish(&buffer_line);
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(output, sizeof(output));
        mmgr_line_put(&buffer_line, path);
        mmgr_line_put(&buffer_line, ".out");
        (void)mmgr_line_finish(&buffer_line);
    }
    if (!sass_binary_write(binary, low, high, chosen, count))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, binary);
            mmgr_line_put(output_line, " could not be written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return -1;
    }
    char *const command[] = {"nvdisasm", "--binary", (char *)architecture, binary, NULL};
    const int status = sass_run(command, output);
    if (status != 0)
    {
        return status;
    }
    if (!sass_listing_read(sass_output(), &s_sass_decoded))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, output);
            mmgr_line_put(output_line, ": more than ");
            mmgr_line_decimal(output_line, SASS_LISTING_LIMIT, 1u);
            mmgr_line_put(output_line, " instructions");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return -1;
    }
    for (unsigned int number = 0u; number < count; number += 1u)
    {
        VerbaLinea buffer_line = mmgr_line_over(texts[chosen[number]], SASS_TEXT);
        mmgr_line_put(&buffer_line, "unprinted");
        (void)mmgr_line_finish(&buffer_line);
    }
    // each line goes to the encoding at its address, 16 bytes to an encoding
    for (unsigned int number = 0u; number < s_sass_decoded.count; number += 1u)
    {
        const SassInstruction *const decoded = &s_sass_decoded.instructions[number];
        if (((decoded->address % 16ULL) != 0ULL) || ((decoded->address / 16ULL) >= (unsigned long long)count))
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, output);
                mmgr_line_put(output_line, ": a line at address 0x");
                mmgr_line_hex(output_line, decoded->address, 1u);
                mmgr_line_put(output_line, ", past the ");
                mmgr_line_decimal(output_line, count, 1u);
                mmgr_line_put(output_line, " encodings given");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            return -1;
        }
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = texts[chosen[decoded->address / 16ULL]], .src = decoded->text,
                   .bytes = SASS_TEXT);
        sass_target_relative(texts[chosen[decoded->address / 16ULL]], decoded->address);
    }
    return 0;
}

int sass_decode(const char *architecture, const char *path, const unsigned long long *low,
                const unsigned long long *high, unsigned int count, char (*texts)[SASS_TEXT])
{
    unsigned int chosen[SASS_LISTING_LIMIT];
    if (count > SASS_LISTING_LIMIT)
    {
        return 0;
    }
    for (unsigned int number = 0u; number < count; number += 1u)
    {
        chosen[number] = number;
        {
            VerbaLinea buffer_line = mmgr_line_over(texts[number], SASS_TEXT);
            mmgr_line_put(&buffer_line, "illegal");
            (void)mmgr_line_finish(&buffer_line);
        }
    }
    const int status = sass_decode_pass(architecture, path, low, high, chosen, count, texts);
    if (status == 0)
    {
        return 1;
    }
    if (status < 0)
    {
        return 0;
    }
    // the disassembler refused the batch and named each encoding it refused by its address, 16 bytes to an encoding
    int refused[SASS_LISTING_LIMIT];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = refused, .bytes = sizeof(refused), .val = 0);
    unsigned int named = 0u;
    for (const char *walk =
             EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = sass_output(), .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "at address 0x", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
         walk != NULL;
         walk = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = walk + 1, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                           .other = "at address 0x", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE))
    {
        const unsigned long long place =
            EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = walk + 13, .end = NULL, .base = 16) / 16ULL;
        if (place < (unsigned long long)count)
        {
            named += refused[place] ? 0u : 1u;
            refused[place] = 1;
        }
    }
    if (named == 0u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  nvdisasm exited ");
            mmgr_line_decimal_signed(output_line, status);
            mmgr_line_put(output_line, " naming no encoding it refused:\n");
            mmgr_line_put(output_line, sass_output());
            platform_line_out_ended();
        }
        return 0;
    }
    unsigned int legal = 0u;
    for (unsigned int number = 0u; number < count; number += 1u)
    {
        if (!refused[number])
        {
            chosen[legal] = number;
            legal += 1u;
        }
    }
    char second[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(second, sizeof(second));
        mmgr_line_put(&buffer_line, path);
        mmgr_line_put(&buffer_line, "_legal");
        (void)mmgr_line_finish(&buffer_line);
    }
    return (legal == 0u) || (sass_decode_pass(architecture, second, low, high, chosen, legal, texts) == 0);
}
