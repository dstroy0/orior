// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// forms.c: the forms: the listing, the operations taken and counted, a form printed, a bit read, and the fields
#include "probe_main_internal.h"

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
    snprintf(cubin, sizeof(cubin), "%s/%s.cubin", probe->folder, name);
    snprintf(output, sizeof(output), "%s/%s.sass", probe->folder, name);
    char *const command[] = {"nvdisasm", "-c", "-hex", cubin, NULL};
    const int status = sass_run(command, output);
    if (status != 0)
    {
        printf("  %s: nvdisasm exited %d\n%s", name, status, (status > 0) ? sass_output() : "");
        return 0;
    }
    if (!sass_listing_read(sass_output(), listing))
    {
        printf("  %s: more than %u instructions\n", name, SASS_LISTING_LIMIT);
        return 0;
    }
    // the rest of the cubin as cuobjdump reads it: its sections, symbols, segments and the kernel's attributes
    snprintf(output, sizeof(output), "%s/%s.elf", probe->folder, name);
    char *const elf[] = {"cuobjdump", "-elf", cubin, NULL};
    const int elf_status = sass_run(elf, output);
    if (elf_status != 0)
    {
        printf("  %s: cuobjdump exited %d\n%s", name, elf_status, (elf_status > 0) ? sass_output() : "");
        return 0;
    }
    return 1;
}

// each instruction of the listing whose operation is not yet kept, kept
void sass_operations_take(SassProbe *probe, const SassListing *listing)
{
    for (unsigned int number = 0u; number < listing->count; number += 1u)
    {
        const SassInstruction *const instruction = &listing->instructions[number];
        // the key is the low 12 bits of the word, which fit an unsigned int
        const unsigned int key = (unsigned int)(instruction->low & SASS_OPERATION_MASK);
        int known = 0;
        for (unsigned int kept = 0u; kept < probe->operations; kept += 1u)
        {
            known = known || (probe->operation[kept].key == key);
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
        found += (strcmp(parts.operation, operation) == 0) ? 1u : 0u;
    }
    return found;
}

// the operations the form's listing holds more of than the kernel's, each once with how many more, and those it holds
// fewer of
void sass_form_print(const char *name, const SassListing *form, const SassListing *kernel_listing)
{
    printf("form %s:", name);
    for (int more = 1; more >= 0; more -= 1)
    {
        const SassListing *const counted = more ? form : kernel_listing;
        const SassListing *const against = more ? kernel_listing : form;
        printf("%s", more ? "" : " |");
        for (unsigned int number = 0u; number < counted->count; number += 1u)
        {
            SassParts parts;
            sass_parts_read(counted->instructions[number].text, &parts);
            int earlier = 0;
            for (unsigned int before = 0u; before < number; before += 1u)
            {
                SassParts other;
                sass_parts_read(counted->instructions[before].text, &other);
                earlier = earlier || (strcmp(other.operation, parts.operation) == 0);
            }
            const unsigned int here = sass_operation_count(counted, parts.operation);
            const unsigned int there = sass_operation_count(against, parts.operation);
            // NOP fills the code to its alignment and is none of a form's
            if (!earlier && (here > there) && (strcmp(parts.operation, "NOP") != 0))
            {
                printf(" %s%s x%u", more ? "" : "-", parts.operation, here - there);
            }
        }
    }
    printf("\n");
}

// what turning one bit over did to the base's text: its category and an example of the change
static void sass_bit_read(const SassParts *base, const char *base_text, const char *text, char *category, char *example)
{
    SassParts parts;
    sass_parts_read(text, &parts);
    snprintf(example, SASS_EXAMPLE, "%s", text);
    if ((strcmp(text, "illegal") == 0) || (strcmp(text, "unprinted") == 0))
    {
        snprintf(category, SASS_CATEGORY, "%s", text);
        return;
    }
    if (strcmp(text, base_text) == 0)
    {
        snprintf(category, SASS_CATEGORY, "unchanged");
        return;
    }
    if (strcmp(parts.operation, base->operation) != 0)
    {
        const size_t stem = strcspn(base->operation, ".");
        const int same_stem =
            (strcspn(parts.operation, ".") == stem) && (strncmp(parts.operation, base->operation, stem) == 0);
        snprintf(category, SASS_CATEGORY, "%s", same_stem ? "modifier" : "operation");
        snprintf(example, SASS_EXAMPLE, "%s -> %s", base->operation, parts.operation);
        return;
    }
    if (strcmp(parts.predicate, base->predicate) != 0)
    {
        snprintf(category, SASS_CATEGORY, "predicate");
        snprintf(example, SASS_EXAMPLE, "'%s' -> '%s'", base->predicate, parts.predicate);
        return;
    }
    unsigned int changed = 0u;
    unsigned int which = 0u;
    for (unsigned int operand = 0u; (operand < parts.operands) && (operand < base->operands); operand += 1u)
    {
        if (strcmp(parts.operand[operand], base->operand[operand]) != 0)
        {
            changed += 1u;
            which = operand;
        }
    }
    if ((parts.operands == base->operands) && (changed == 1u))
    {
        snprintf(category, SASS_CATEGORY, "operand %u", which);
        snprintf(example, SASS_EXAMPLE, "%s -> %s", base->operand[which], parts.operand[which]);
        return;
    }
    snprintf(category, SASS_CATEGORY, "operands");
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
        low[1u + bit] = low[0] ^ ((bit < 64u) ? (1ull << bit) : 0ull);
        high[1u + bit] = high[0] ^ ((bit >= 64u) ? (1ull << (bit - 64u)) : 0ull);
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/opcode_%03x", probe->folder, operation->key);
    printf("operation %03x: %s (0x%016llx 0x%016llx)\n", operation->key, operation->first.text, low[0], high[0]);
    if (!sass_decode(probe->architecture, path, low, high, SASS_ENCODINGS, s_sass_texts))
    {
        printf("  not decoded\n");
        return 0;
    }
    SassParts base;
    sass_parts_read(s_sass_texts[0], &base);
    snprintf(path, sizeof(path), "%s/opcode_%03x.bits", probe->folder, operation->key);
    FILE *const bits = fopen(path, "w");
    if (bits != NULL)
    {
        fprintf(bits, "base: %s\n", s_sass_texts[0]);
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
                fprintf(bits, "bit %u: %s: %s\n", bit, category, s_sass_texts[1u + bit]);
            }
        }
        // a modifier bit is a run of its own, since each names a modifier of its own
        const int joined = (bit < SASS_BITS) && (bit != 0u) && (strcmp(category, run_category) == 0) &&
                           (strcmp(category, "modifier") != 0) && (bit != 64u);
        if (!joined && (bit != 0u))
        {
            printf("  bits %3u-%3u %s: %s\n", run_start, bit - 1u, run_category, run_example);
        }
        if (!joined)
        {
            run_start = bit;
            memcpy(run_category, category, sizeof(run_category));
            memcpy(run_example, example, sizeof(run_example));
        }
    }
    if (bits != NULL)
    {
        fclose(bits);
    }
    return 1;
}
