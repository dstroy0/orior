// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tables_funnel.c: the vendor's two tables written out of a capture of its disassembler: the instruction set and
// the latencies
//
//     tables_funnel <capture> <instructions.txt> <latencies.txt>
//
// Exit 0 where both tables were written, 1 where the capture could not be read or a table could not be written, 2
// where the capture holds no instruction archive that unpacks whole, 3 where it holds no copies of the latencies.
#include "funnel.h"

#include <stdio.h>
#include <stdlib.h>

static unsigned char *tables_file_read(const char *path, size_t *size)
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

static int tables_file_written(const char *path, const TablesText *text)
{
    FILE *const file = fopen(path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    const int whole = (text->length == 0u) || (fwrite(text->bytes, 1u, text->length, file) == text->length);
    return (fclose(file) == 0) && whole;
}

int main(int count, char **words)
{
    if (count != 4)
    {
        fprintf(stderr, "tables_funnel <capture> <instructions.txt> <latencies.txt>\n");
        return 1;
    }
    size_t size = 0u;
    unsigned char *const bytes = tables_file_read(words[1], &size);
    if (bytes == NULL)
    {
        fprintf(stderr, "tables_funnel: %s could not be read\n", words[1]);
        return 1;
    }
    TablesText instructions = {NULL, 0u, 0u};
    TablesText latencies = {NULL, 0u, 0u};
    tables_funnelled(bytes, size, &instructions, &latencies);
    free(bytes);
    int code = 0;
    if (instructions.length == 0u)
    {
        fprintf(stderr, "tables_funnel: %s holds no instruction archive that unpacks whole\n", words[1]);
        code = 2;
    }
    else if (latencies.length == 0u)
    {
        fprintf(stderr, "tables_funnel: %s holds no copies of the latencies\n", words[1]);
        code = 3;
    }
    else if (!tables_file_written(words[2], &instructions) || !tables_file_written(words[3], &latencies))
    {
        fprintf(stderr, "tables_funnel: %s or %s could not be written\n", words[2], words[3]);
        code = 1;
    }
    else
    {
        printf("tables_funnel: %zu bytes of instructions, %zu of latencies\n", instructions.length, latencies.length);
    }
    tables_text_freed(&instructions);
    tables_text_freed(&latencies);
    return code;
}
