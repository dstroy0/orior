// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tables_read.c: the vendor's instruction set read for the command line: each encoding given named by its class,
// its constant operand's bank, byte offset and bytes read, and rule 13's verdict against a bank size; or the spans
// of the constant address fields every class of the set uses, counted
//
//     tables_read <instructions.txt> class [--bank <size>] <low> <high>...
//     tables_read <instructions.txt> spans
//
// `class` prints one line an encoding, `<low> <high> <class> [bank <n> offset 0x<bytes> reads <bytes> <verdict>]`,
// the verdict `held` where rule 13 holds the read against the bank of `--bank` bytes, `passes` where it does not,
// and `unknown` where no size is given, the bank alone then held against (cubin_safe/constant.c). `spans` prints each
// distinct pair of a bank field's span and an offset field's span with the count of classes laying their constant
// operand so, `bank <first>..<last> offset <first>..<last> <count>`. Exit 0, 2 where the words are not as above, and 1
// where the instruction set could not be read
#include "../cubin_safe.h"
#include "read.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the file at `path` whole, or NULL
static unsigned char *whole(const char *path, size_t *size)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return NULL;
    }
    unsigned char *bytes = NULL;
    size_t length = 0u;
    size_t room = 0u;
    for (;;)
    {
        if (length == room)
        {
            room = (room == 0u) ? (1u << 20u) : (room * 2u);
            unsigned char *const grown = (unsigned char *)realloc(bytes, room);
            if (grown == NULL)
            {
                free(bytes);
                fclose(file);
                return NULL;
            }
            bytes = grown;
        }
        const size_t got = fread(bytes + length, 1u, room - length, file);
        if (got == 0u)
        {
            break;
        }
        length += got;
    }
    fclose(file);
    *size = length;
    return bytes;
}

static int spans_printed(const TablesSet *set)
{
    unsigned int bank_first[256];
    unsigned int bank_last[256];
    unsigned int offset_first[256];
    unsigned int offset_last[256];
    unsigned int counted[256];
    unsigned int spans = 0u;
    for (unsigned int at = 0u; at < set->count; at += 1u)
    {
        const TablesClass *const class_ = &set->class_[at];
        const TablesField *const bank =
            (class_->bank[0] != '\0') ? tables_field_by_operand(class_, class_->bank) : NULL;
        const TablesField *const offset =
            (class_->offset[0] != '\0') ? tables_field_by_operand(class_, class_->offset) : NULL;
        if ((bank == NULL) || (offset == NULL))
        {
            continue;
        }
        // the lowest bit of every run and the highest, the fields' spans
        unsigned int span[4] = {bank->run[0].first, bank->run[0].last, offset->run[0].first, offset->run[0].last};
        for (unsigned int run = 1u; run < bank->runs; run += 1u)
        {
            span[0] = (bank->run[run].first < span[0]) ? bank->run[run].first : span[0];
            span[1] = (bank->run[run].last > span[1]) ? bank->run[run].last : span[1];
        }
        for (unsigned int run = 1u; run < offset->runs; run += 1u)
        {
            span[2] = (offset->run[run].first < span[2]) ? offset->run[run].first : span[2];
            span[3] = (offset->run[run].last > span[3]) ? offset->run[run].last : span[3];
        }
        unsigned int found = spans;
        for (unsigned int each = 0u; (each < spans) && (found == spans); each += 1u)
        {
            found = ((bank_first[each] == span[0]) && (bank_last[each] == span[1]) && (offset_first[each] == span[2]) &&
                     (offset_last[each] == span[3]))
                        ? each
                        : found;
        }
        if ((found == spans) && (spans < 256u))
        {
            bank_first[spans] = span[0];
            bank_last[spans] = span[1];
            offset_first[spans] = span[2];
            offset_last[spans] = span[3];
            counted[spans] = 0u;
            spans += 1u;
        }
        if (found < 256u)
        {
            counted[found] += 1u;
        }
    }
    for (unsigned int each = 0u; each < spans; each += 1u)
    {
        printf("bank %u..%u offset %u..%u %u\n", bank_first[each], bank_last[each], offset_first[each],
               offset_last[each], counted[each]);
    }
    return 0;
}

int main(int count, char **words)
{
    if ((count < 3) || ((strcmp(words[2], "class") != 0) && (strcmp(words[2], "spans") != 0)))
    {
        fprintf(stderr, "tables_read <instructions.txt> class [--bank <size>] <low> <high>...\n"
                        "tables_read <instructions.txt> spans\n");
        return 2;
    }
    size_t size = 0u;
    unsigned char *const text = whole(words[1], &size);
    TablesSet set = {NULL, 0u, 0u};
    if ((text == NULL) || !tables_set_read(&set, text, size) || (set.count == 0u))
    {
        fprintf(stderr, "tables_read: %s holds no instruction set\n", words[1]);
        free(text);
        return 1;
    }
    free(text);
    if (strcmp(words[2], "spans") == 0)
    {
        const int code = spans_printed(&set);
        tables_set_freed(&set);
        return code;
    }
    int given = 3;
    unsigned long long bank_size = 0ull;
    if (((given + 1) < count) && (strcmp(words[given], "--bank") == 0))
    {
        bank_size = strtoull(words[given + 1], NULL, 0);
        given += 2;
    }
    for (; (given + 1) < count; given += 2)
    {
        const unsigned long long low = strtoull(words[given], NULL, 16);
        const unsigned long long high = strtoull(words[given + 1], NULL, 16);
        char line[256];
        const int classed = tables_class_described(&set, low, high, line, sizeof(line));
        const TablesClass *const class_ = classed ? tables_class_of(&set, low, high) : NULL;
        const int constant = (class_ != NULL) && (class_->bank[0] != '\0');
        const char *verdict = "";
        if (constant)
        {
            verdict = (bank_size == 0ull)                                      ? " unknown"
                      : cubin_constant_out_of_bank(&set, low, high, bank_size) ? " held"
                                                                               : " passes";
        }
        printf("%s%s\n", line, verdict);
    }
    tables_set_freed(&set);
    return 0;
}
