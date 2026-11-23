// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// forms.c: the forms: the listing, the operations taken and counted, a form printed, a bit read, and the fields
#include "../../../../../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "probe_main_internal.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int forms_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

SassProbe s_sass_probe;
SassMachine s_sass_machine;
SassListing s_sass_kernel;
SassListing s_sass_resident;
SassListing s_sass_form;
static char s_sass_texts[SASS_ENCODINGS][SASS_TEXT];

// `name`.cubin in the folder listed with its encodings into `listing`, the listing written to `name`.sass, and the
// whole ELF read by cuobjdump -elf into `name`.elf: 1, or 0 with the reason printed
int sass_list(SassProbe *probe, const char *name, SassListing *listing)
{
    char cubin[1024];
    char output[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(cubin, sizeof(cubin));
        mmgr_line_put(&buffer_line, probe->folder);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, name);
        mmgr_line_put(&buffer_line, ".cubin");
        (void)mmgr_line_finish(&buffer_line);
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(output, sizeof(output));
        mmgr_line_put(&buffer_line, probe->folder);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, name);
        mmgr_line_put(&buffer_line, ".sass");
        (void)mmgr_line_finish(&buffer_line);
    }
    char *const command[] = {"nvdisasm", "-c", "-hex", cubin, NULL};
    const int status = sass_run(command, output);
    if (status != 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, name);
            mmgr_line_put(output_line, ": nvdisasm exited ");
            mmgr_line_decimal_signed(output_line, status);
            mmgr_line_put(output_line, "\n");
            mmgr_line_put(output_line, (status > 0) ? sass_output() : "");
            platform_line_out_ended();
        }
        return 0;
    }
    if (!sass_listing_read(sass_output(), listing))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, name);
            mmgr_line_put(output_line, ": more than ");
            mmgr_line_decimal(output_line, SASS_LISTING_LIMIT, 1u);
            mmgr_line_put(output_line, " instructions");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    // the rest of the cubin as cuobjdump reads it: its sections, symbols, segments and the kernel's attributes
    {
        VerbaLinea buffer_line = mmgr_line_over(output, sizeof(output));
        mmgr_line_put(&buffer_line, probe->folder);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, name);
        mmgr_line_put(&buffer_line, ".elf");
        (void)mmgr_line_finish(&buffer_line);
    }
    char *const elf[] = {"cuobjdump", "-elf", cubin, NULL};
    const int elf_status = sass_run(elf, output);
    if (elf_status != 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, name);
            mmgr_line_put(output_line, ": cuobjdump exited ");
            mmgr_line_decimal_signed(output_line, elf_status);
            mmgr_line_put(output_line, "\n");
            mmgr_line_put(output_line, (elf_status > 0) ? sass_output() : "");
            platform_line_out_ended();
        }
        return 0;
    }
    return 1;
}

// each instruction of the listing whose operation is not yet stored, stored
void sass_operations_take(SassProbe *probe, const SassListing *listing)
{
    for (unsigned int number = 0u; number < listing->count; number += 1u)
    {
        const SassInstruction *const instruction = &listing->instructions[number];
        // the key is the low 12 bits of the word, which fit an unsigned int
        const unsigned int key = (unsigned int)(instruction->low & SASS_OPERATION_MASK);
        int known = 0;
        for (unsigned int taken = 0u; taken < probe->operations; taken += 1u)
        {
            known = known || (probe->operation[taken].key == key);
        }
        if (!known && (probe->operations < SASS_OPERATIONS))
        {
            probe->operation[probe->operations].key = key;
            probe->operation[probe->operations].first = *instruction;
            probe->operations += 1u;
        }
    }
}

// how many instructions of the listing have the operation
static unsigned int sass_operation_count(const SassListing *listing, const char *operation)
{
    unsigned int found = 0u;
    for (unsigned int number = 0u; number < listing->count; number += 1u)
    {
        SassParts parts;
        sass_parts_read(listing->instructions[number].text, &parts);
        found += (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = operation, .ci = EMBED_FALSE))
                     ? 1u
                     : 0u;
    }
    return found;
}

// the operations the form's listing contains more of than the kernel's, each once with how many more, and those it
// contains fewer of
void sass_form_print(const char *name, const SassListing *form, const SassListing *kernel_listing)
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "form ");
        mmgr_line_put(output_line, name);
        mmgr_line_put(output_line, ":");
        platform_line_out_ended();
    }
    for (int more = 1; more >= 0; more -= 1)
    {
        const SassListing *const counted = more ? form : kernel_listing;
        const SassListing *const against = more ? kernel_listing : form;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, more ? "" : " |");
            platform_line_out_ended();
        }
        for (unsigned int number = 0u; number < counted->count; number += 1u)
        {
            SassParts parts;
            sass_parts_read(counted->instructions[number].text, &parts);
            int earlier = 0;
            for (unsigned int before = 0u; before < number; before += 1u)
            {
                SassParts other;
                sass_parts_read(counted->instructions[before].text, &other);
                earlier = earlier ||
                          (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = other.operation,
                                      .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = parts.operation, .ci = EMBED_FALSE));
            }
            const unsigned int here = sass_operation_count(counted, parts.operation);
            const unsigned int there = sass_operation_count(against, parts.operation);
            // NOP fills the code to its alignment and is none of a form's
            if (!earlier && (here > there) &&
                (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "NOP", .ci = EMBED_FALSE)))
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, " ");
                mmgr_line_put(output_line, more ? "" : "-");
                mmgr_line_put(output_line, parts.operation);
                mmgr_line_put(output_line, " x");
                mmgr_line_decimal(output_line, here - there, 1u);
                platform_line_out_ended();
            }
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

// what turning one bit over did to the base's text: its category and an example of the change
static void sass_bit_read(const SassParts *base, const char *base_text, const char *text, char *category, char *example)
{
    SassParts parts;
    sass_parts_read(text, &parts);
    {
        VerbaLinea buffer_line = mmgr_line_over(example, SASS_EXAMPLE);
        mmgr_line_put(&buffer_line, text);
        (void)mmgr_line_finish(&buffer_line);
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "illegal",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "unprinted",
                    .ci = EMBED_FALSE)))
    {
        {
            VerbaLinea buffer_line = mmgr_line_over(category, SASS_CATEGORY);
            mmgr_line_put(&buffer_line, text);
            (void)mmgr_line_finish(&buffer_line);
        }
        return;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = base_text,
                   .ci = EMBED_FALSE))
    {
        {
            VerbaLinea buffer_line = mmgr_line_over(category, SASS_CATEGORY);
            mmgr_line_put(&buffer_line, "unchanged");
            (void)mmgr_line_finish(&buffer_line);
        }
        return;
    }
    if (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                    .other = base->operation, .ci = EMBED_FALSE))
    {
        const size_t stem = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = base->operation,
                                       .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".");
        const int same_stem = (EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = parts.operation,
                                          .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".") == stem) &&
                              (forms_text_order(parts.operation, base->operation, (size_t)(stem)) == 0);
        {
            VerbaLinea buffer_line = mmgr_line_over(category, SASS_CATEGORY);
            mmgr_line_put(&buffer_line, same_stem ? "modifier" : "operation");
            (void)mmgr_line_finish(&buffer_line);
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(example, SASS_EXAMPLE);
            mmgr_line_put(&buffer_line, base->operation);
            mmgr_line_put(&buffer_line, " -> ");
            mmgr_line_put(&buffer_line, parts.operation);
            (void)mmgr_line_finish(&buffer_line);
        }
        return;
    }
    if (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.predicate, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                    .other = base->predicate, .ci = EMBED_FALSE))
    {
        {
            VerbaLinea buffer_line = mmgr_line_over(category, SASS_CATEGORY);
            mmgr_line_put(&buffer_line, "predicate");
            (void)mmgr_line_finish(&buffer_line);
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(example, SASS_EXAMPLE);
            mmgr_line_put(&buffer_line, "'");
            mmgr_line_put(&buffer_line, base->predicate);
            mmgr_line_put(&buffer_line, "' -> '");
            mmgr_line_put(&buffer_line, parts.predicate);
            mmgr_line_put(&buffer_line, "'");
            (void)mmgr_line_finish(&buffer_line);
        }
        return;
    }
    unsigned int changed = 0u;
    unsigned int which = 0u;
    for (unsigned int operand = 0u; (operand < parts.operands) && (operand < base->operands); operand += 1u)
    {
        if (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.operand[operand], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = base->operand[operand], .ci = EMBED_FALSE))
        {
            changed += 1u;
            which = operand;
        }
    }
    if ((parts.operands == base->operands) && (changed == 1u))
    {
        {
            VerbaLinea buffer_line = mmgr_line_over(category, SASS_CATEGORY);
            mmgr_line_put(&buffer_line, "operand ");
            mmgr_line_decimal(&buffer_line, which, 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(example, SASS_EXAMPLE);
            mmgr_line_put(&buffer_line, base->operand[which]);
            mmgr_line_put(&buffer_line, " -> ");
            mmgr_line_put(&buffer_line, parts.operand[which]);
            (void)mmgr_line_finish(&buffer_line);
        }
        return;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(category, SASS_CATEGORY);
        mmgr_line_put(&buffer_line, "operands");
        (void)mmgr_line_finish(&buffer_line);
    }
}

// the operation's 128 bits turned over one at a time and decoded: each run of bits of one category printed, and each
// bit's reading written to opcode_<key>.bits in the folder. 1, or 0 where the disassembler failed
int sass_fields(SassProbe *probe, const SassOperation *operation)
{
    unsigned long long low[SASS_ENCODINGS];
    unsigned long long high[SASS_ENCODINGS];
    low[0] = operation->first.low;
    high[0] = operation->first.high;
    for (unsigned int bit = 0u; bit < SASS_BITS; bit += 1u)
    {
        low[1u + bit] = low[0] ^ ((bit < 64u) ? (1ULL << bit) : 0ULL);
        high[1u + bit] = high[0] ^ ((bit >= 64u) ? (1ULL << (bit - 64u)) : 0ULL);
    }
    char path[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, probe->folder);
        mmgr_line_put(&buffer_line, "/opcode_");
        mmgr_line_hex(&buffer_line, operation->key, 3u);
        (void)mmgr_line_finish(&buffer_line);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "operation ");
        mmgr_line_hex(output_line, operation->key, 3u);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, operation->first.text);
        mmgr_line_put(output_line, " (0x");
        mmgr_line_hex(output_line, low[0], 16u);
        mmgr_line_put(output_line, " 0x");
        mmgr_line_hex(output_line, high[0], 16u);
        mmgr_line_put(output_line, ")");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (!sass_decode(probe->architecture, path, low, high, SASS_ENCODINGS, s_sass_texts))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  not decoded");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    SassParts base;
    sass_parts_read(s_sass_texts[0], &base);
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, probe->folder);
        mmgr_line_put(&buffer_line, "/opcode_");
        mmgr_line_hex(&buffer_line, operation->key, 3u);
        mmgr_line_put(&buffer_line, ".bits");
        (void)mmgr_line_finish(&buffer_line);
    }
    PlatformFile *const bits = platform_file_open(path, "w");
    if (bits != NULL)
    {
        mmgr_line_put(platform_file_line(bits), "base: ");
        mmgr_line_put(platform_file_line(bits), s_sass_texts[0]);
        mmgr_line_put(platform_file_line(bits), "\n");
    }
    char run_category[SASS_CATEGORY] = "";
    char run_example[SASS_EXAMPLE] = "";
    unsigned int run_start = 0u;
    for (unsigned int bit = 0u; bit <= SASS_BITS; bit += 1u)
    {
        char category[SASS_CATEGORY] = "";
        char example[SASS_EXAMPLE] = "";
        if (bit < SASS_BITS)
        {
            sass_bit_read(&base, s_sass_texts[0], s_sass_texts[1u + bit], category, example);
            if (bits != NULL)
            {
                mmgr_line_put(platform_file_line(bits), "bit ");
                mmgr_line_decimal(platform_file_line(bits), bit, 1u);
                mmgr_line_put(platform_file_line(bits), ": ");
                mmgr_line_put(platform_file_line(bits), category);
                mmgr_line_put(platform_file_line(bits), ": ");
                mmgr_line_put(platform_file_line(bits), s_sass_texts[1u + bit]);
                mmgr_line_put(platform_file_line(bits), "\n");
            }
        }
        // a modifier bit is a run of its own, since each names a modifier of its own
        const int joined = (bit < SASS_BITS) && (bit != 0u) &&
                           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = category, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                       .other = run_category, .ci = EMBED_FALSE)) &&
                           (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = category, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                        .other = "modifier", .ci = EMBED_FALSE)) &&
                           (bit != 64u);
        if (!joined && (bit != 0u))
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  bits ");
            mmgr_line_decimal_columns(output_line, run_start, 3u);
            mmgr_line_put(output_line, "-");
            mmgr_line_decimal_columns(output_line, bit - 1u, 3u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, run_category);
            mmgr_line_put(output_line, ": ");
            mmgr_line_put(output_line, run_example);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (!joined)
        {
            run_start = bit;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = run_category, .src = category, .bytes = sizeof(run_category));
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = run_example, .src = example, .bytes = sizeof(run_example));
        }
    }
    if (bits != NULL)
    {
        platform_file_close(bits);
    }
    return 1;
}
