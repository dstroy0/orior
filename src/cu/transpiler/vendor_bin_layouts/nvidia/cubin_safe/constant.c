// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// constant.c: rule 13, a read of the constant bank held before the part where it reaches a bank the container has no
// section for or past the bank's end, read through the vendor's own instruction set
#include "cubin_safe_internal.h"

#include "../tables/read.h"

// the vendor's instruction set for the part, read once; empty where the part's tables are not pulled
static TablesSet s_tables;

static int s_tables_ready;

int cubin_safe_tables_read(const char *part)
{
    char path[512];
    snprintf(path, sizeof(path), "build/vendor/nvidia/tables/%s_instructions.txt", part);
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    unsigned char *text = NULL;
    size_t length = 0u;
    size_t room = 0u;
    for (;;)
    {
        if (length == room)
        {
            room = (room == 0u) ? (1u << 20u) : (room * 2u);
            unsigned char *const grown = (unsigned char *)realloc(text, room);
            if (grown == NULL)
            {
                free(text);
                fclose(file);
                return 0;
            }
            text = grown;
        }
        const size_t got = fread(text + length, 1u, room - length, file);
        if (got == 0u)
        {
            break;
        }
        length += got;
    }
    fclose(file);
    tables_set_freed(&s_tables);
    const int read = tables_set_read(&s_tables, text, length);
    free(text);
    s_tables_ready = read && (s_tables.count != 0u);
    return s_tables_ready;
}

int cubin_safe_tables_ready(void)
{
    return s_tables_ready;
}

const TablesSet *cubin_safe_tables(void)
{
    return &s_tables;
}

int cubin_constant_out_of_bank(const struct TablesSetTag *tables, unsigned long long low, unsigned long long high,
                               unsigned long long bank_size)
{
    const TablesClass *const class_ = tables_class_of(tables, low, high);
    if ((class_ == NULL) || (class_->bank[0] == '\0'))
    {
        return 0;
    }
    const TablesField *const bank = tables_field_by_operand(class_, class_->bank);
    const TablesField *const offset = tables_field_by_operand(class_, class_->offset);
    if ((bank == NULL) || (offset == NULL))
    {
        return 0;
    }
    // bank 0 is the parameters' bank, the one section of a bank a container of ours carries; the offset's field
    // counts words of four bytes, and the word read must end inside the bank where the bank's size is known
    if (tables_field_read(bank, low, high) != 0ull)
    {
        return 1;
    }
    const unsigned long long bytes = tables_field_read(offset, low, high) * 4ull;
    return (bank_size != 0ull) && (bytes + 4ull > bank_size);
}
