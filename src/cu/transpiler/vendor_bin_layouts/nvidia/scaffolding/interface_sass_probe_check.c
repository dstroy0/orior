// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_probe_check.c: one kernel's text found in what a toolchain printed, and every form of the machine
// assembled again and read back, which shows whether a form carries what the part read from it (sass_machine.h)
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "interface_sass_probe.h"
#include "memoria_operor/memoria_operor.h"

#include "../sass_assemble.h"
#include "../sass_machine.h"

unsigned int sass_text_read(const char *output, const char *kernel, char *text, unsigned int room)
{
    char wanted[256];
    {
        VerbaLinea buffer_line = mmgr_line_over(wanted, sizeof(wanted));
        mmgr_line_put(&buffer_line, ".text.");
        mmgr_line_put(&buffer_line, kernel);
        mmgr_line_put(&buffer_line, ",");
        (void)mmgr_line_finish(&buffer_line);
    }
    unsigned int at = 0u;
    int inside = 0;
    const char *walk = output;
    while ((walk != NULL) && (*walk != '\0'))
    {
        const size_t length = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = walk,
                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "\n");
        char line[SASS_TEXT * 2u];
        const size_t taken = (length < (sizeof(line) - 1u)) ? length : (sizeof(line) - 1u);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = line, .src = walk, .bytes = taken);
        line[taken] = '\0';
        // the disassembler's output ends its lines the way the host does, and a carriage return is none of the text
        line[EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "\r")] = '\0';
        walk += length + ((walk[length] == '\n') ? 1u : 0u);
        // a listing holds one section for each function, and the call writes back only that of the kernel. The line
        // `.sectioninfo` names no section, and the name whose front it shares is not a line that opens one
        const char *const section =
            EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = ".section", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
        if ((section != NULL) && ((section[8] == ' ') || (section[8] == '\t')))
        {
            inside = (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                 .other = wanted, .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL)
                         ? 1
                         : 0;
            continue;
        }
        if (inside == 0)
        {
            continue;
        }
        const char *const address =
            EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "/*",
                       .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
        const char *chosen = NULL;
        size_t copied = 0u;
        if ((address != NULL) &&
            (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = address, .cap = PLATFORM_TEXT_TO_TERMINATOR) > 8u) &&
            (address[6] == '*') && (address[7] == '/'))
        {
            // an instruction: its text lies past the address, /*0000*/, and before the encoding printed after it
            chosen = address + 8;
            const char *const encoding =
                EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = chosen, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                           .other = "/*", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
            copied = (encoding != NULL)
                         ? (size_t)(encoding - chosen)
                         : EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = chosen, .cap = PLATFORM_TEXT_TO_TERMINATOR);
        }
        else if ((line[0] == '.') &&
                 (line[EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR) - 1u] ==
                  ':'))
        {
            chosen = line;
            copied = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR);
        }
        if (chosen == NULL)
        {
            continue;
        }
        while ((copied != 0u) && ((chosen[copied - 1u] == ' ') || (chosen[copied - 1u] == ';')))
        {
            copied -= 1u;
        }
        while ((copied != 0u) && (*chosen == ' '))
        {
            chosen += 1;
            copied -= 1u;
        }
        if ((copied != 0u) && ((at + copied + 1u) < room))
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &text[at], .src = chosen, .bytes = copied);
            at += (unsigned int)copied;
            text[at] = '\n';
            at += 1u;
        }
    }
    text[at] = '\0';
    return at;
}

// the max instructions one check decodes at a time
#define SASS_CHECK_BLOCK 400u

static unsigned long long s_check_low[SASS_CHECK_BLOCK];
static unsigned long long s_check_high[SASS_CHECK_BLOCK];
static char s_check_texts[SASS_CHECK_BLOCK][SASS_TEXT];
static char s_check_queried[SASS_CHECK_BLOCK][SASS_TEXT];

// `text` with .reuse cut out of it, into `without`: reuse lies in the scheduler's bits, which the text does not carry
static void sass_reuse_cut(const char *text, char *without, size_t room)
{
    size_t at = 0u;
    for (const char *walk = text; (*walk != '\0') && (at < (room - 1u)); walk += 1)
    {
        if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = walk, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = ".reuse", .ci = EMBED_FALSE))
        {
            walk += 5;
            continue;
        }
        without[at] = *walk;
        at += 1u;
    }
    without[at] = '\0';
}

unsigned int sass_machine_check(const SassMachine *machine, const SassListing *listing, const char *architecture,
                                const char *folder, SassCheck *tally, unsigned int report)
{
    unsigned int filled = 0u;
    unsigned int differed = 0u;
    for (unsigned int number = 0u; number <= listing->count; number += 1u)
    {
        const SassInstruction *const instruction = &listing->instructions[number];
        const int listed = (number < listing->count) && ((instruction->low != 0ULL) || (instruction->high != 0ULL));
        if (listed)
        {
            tally->checked += 1u;
            unsigned long long low = 0ULL;
            unsigned long long high = 0ULL;
            // a branch counts its target from itself, and a listing's own branch names where it is
            if (!sass_assemble(machine, instruction->text, instruction->address, instruction->address,
                               SASS_CONTROL_BASE, &low, &high))
            {
                tally->refused += 1u;
                differed += 1u;
                continue;
            }
            // the bits of the scheduler are none of the text, and the form carries whatever they were when the probe
            // saw it
            const unsigned long long control = 0xfffffe0000000000ULL;
            const int same_bits = (low == instruction->low) && ((high | control) == (instruction->high | control));
            tally->same_bits += same_bits ? 1u : 0u;
            SassInstructionParts parts;
            sass_instruction_read(instruction->text, &parts);
            int by_bytes = 0;
            for (unsigned int place = 0u; place < parts.operands; place += 1u)
            {
                by_bytes = by_bytes || (parts.type[place] == SASS_OPERAND_LABEL) ||
                           (parts.type[place] == SASS_OPERAND_UNKNOWN);
            }
            // a branch counts its target from where it is, and a relocated operand is not in the instruction at all.
            // The loader puts it there, and the listing prints what the ELF says it will be. Neither reads back from
            // the bytes alone. For those, the bytes are all that the call can fill to the listing
            if (by_bytes)
            {
                tally->by_bytes += 1u;
                differed += same_bits ? 0u : 1u;
                if (!same_bits && (differed <= report))
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, "  check: ");
                    mmgr_line_put(output_line, instruction->text);
                    mmgr_line_put(output_line, "\n    assembled 0x");
                    mmgr_line_hex(output_line, low, 16u);
                    mmgr_line_put(output_line, " 0x");
                    mmgr_line_hex(output_line, high, 16u);
                    mmgr_line_put(output_line, "\n    listed    0x");
                    mmgr_line_hex(output_line, instruction->low, 16u);
                    mmgr_line_put(output_line, " 0x");
                    mmgr_line_hex(output_line, instruction->high, 16u);
                    mmgr_line_ch(output_line, '\n');
                    platform_line_out_ended();
                }
                continue;
            }
            s_check_low[filled] = low;
            s_check_high[filled] = high;
            {
                VerbaLinea buffer_line = mmgr_line_over(s_check_queried[filled], SASS_TEXT);
                mmgr_line_put(&buffer_line, instruction->text);
                (void)mmgr_line_finish(&buffer_line);
            }
            filled += 1u;
        }
        // the block decoded whenever it is full, and at the end of the listing
        if ((filled == SASS_CHECK_BLOCK) || ((number == listing->count) && (filled != 0u)))
        {
            char path[1024];
            {
                VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
                mmgr_line_put(&buffer_line, folder);
                mmgr_line_put(&buffer_line, "/written");
                (void)mmgr_line_finish(&buffer_line);
            }
            if (!sass_decode(architecture, path, s_check_low, s_check_high, filled, s_check_texts))
            {
                tally->refused += filled;
                return differed + filled;
            }
            for (unsigned int at = 0u; at < filled; at += 1u)
            {
                char query_text[SASS_TEXT];
                char read[SASS_TEXT];
                sass_reuse_cut(s_check_queried[at], query_text, sizeof(query_text));
                sass_reuse_cut(s_check_texts[at], read, sizeof(read));
                const int same = (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = query_text,
                                             .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = read, .ci = EMBED_FALSE));
                tally->same_text += same ? 1u : 0u;
                differed += same ? 0u : 1u;
                if (!same && (differed <= report))
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, "  check: queried ");
                    mmgr_line_put(output_line, query_text);
                    mmgr_line_put(output_line, "\n         read  ");
                    mmgr_line_put(output_line, read);
                    mmgr_line_ch(output_line, '\n');
                    platform_line_out_ended();
                }
            }
            filled = 0u;
        }
    }
    return differed;
}
