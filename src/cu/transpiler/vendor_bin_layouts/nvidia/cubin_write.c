// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_write.c: a cubin written by the one emitter, from the layout of the ELF a CUDA toolchain writes
#include "cubin_write.h"

#include "../container_write.h"

#include <stdio.h>
#include <string.h>

// the layout of a cubin, in the folder of this file in the tree it was built from, read once a
// process; NULL where it could not be read, with the reason printed
static const ContainerLayout *cubin_layout(void)
{
    static ContainerLayout s_layout;
    static int s_read = 0;
    if (s_read == 0)
    {
        const char *const file = __FILE__;
        const char *const forward = strrchr(file, '/');
        const char *const back = strrchr(file, '\\');
        const char *const slash = (back == NULL) ? forward : ((forward == NULL) || (back > forward)) ? back : forward;
        // the folder is __FILE__ up to and with its last slash, and a file named with no folder is in this one
        const int folder = (slash != NULL) ? (int)((slash - file) + 1) : 0;
        char path[1024];
        snprintf(path, sizeof(path), "%.*self64_nvidia.tsv", folder, file);
        s_read = container_layout_read(&s_layout, path) ? 1 : -1;
    }
    return (s_read == 1) ? &s_layout : NULL;
}

int cubin_write(const CubinWrite *args, unsigned char *written, unsigned long long room, unsigned long long *size)
{
    const ContainerLayout *const layout = cubin_layout();
    if (layout == NULL)
    {
        return 0;
    }
    const ContainerWrite container = {layout,          args->pattern,   args->pattern_size, args->kernel,    args->code,
                                      args->code_size, args->registers, args->exits,        args->exit_count};
    return container_write(&container, written, room, size);
}

// the value of one hex digit, or 16 where `digit` is none
static unsigned int cubin_hex_digit(char digit)
{
    if ((digit >= '0') && (digit <= '9'))
    {
        return (unsigned int)(digit - '0');
    }
    if ((digit >= 'a') && (digit <= 'f'))
    {
        return (unsigned int)(digit - 'a') + 10u;
    }
    return 16u;
}

unsigned long long cubin_pattern_read(const char *path, unsigned char *pattern, unsigned long long room, char *kernel,
                                      unsigned int kernel_room)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        printf("  cubin_write: %s could not be read\n", path);
        return 0ull;
    }
    unsigned long long size = 0ull;
    int fits = 1;
    kernel[0] = '\0';
    char line[512];
    while (fits && (fgets(line, sizeof(line), file) != NULL))
    {
        char name[256];
        if (sscanf(line, "container kernel %255s", name) == 1)
        {
            snprintf(kernel, kernel_room, "%s", name);
            continue;
        }
        if (strncmp(line, "container pattern ", 18u) != 0)
        {
            continue;
        }
        for (unsigned int at = 18u; fits && (cubin_hex_digit(line[at]) < 16u); at += 2u)
        {
            const unsigned int high = cubin_hex_digit(line[at]);
            const unsigned int low = cubin_hex_digit(line[at + 1u]);
            fits = (low < 16u) && (size < room);
            if (fits)
            {
                pattern[size] = (unsigned char)((high << 4u) | low);
                size += 1ull;
            }
        }
    }
    fclose(file);
    if (!fits || (size == 0ull) || (kernel[0] == '\0'))
    {
        printf("  cubin_write: %s holds no container the system accepted that reads and fits\n", path);
        return 0ull;
    }
    return size;
}

unsigned int cubin_exits_find(const unsigned char *code, unsigned long long code_size, unsigned long long exit_low,
                              unsigned int *exits, unsigned int room)
{
    const ContainerLayout *const layout = cubin_layout();
    return (layout != NULL) ? container_endings_find(layout, code, code_size, exit_low, exits, room) : 0u;
}

unsigned int cubin_registers_read(const unsigned char *pattern, unsigned long long pattern_size, const char *kernel)
{
    const ContainerLayout *const layout = cubin_layout();
    return (layout != NULL) ? container_registers_read(layout, pattern, pattern_size, kernel) : 0u;
}

unsigned int cubin_code_sections(const unsigned char *cubin, unsigned long long size, unsigned long long *offsets,
                                 unsigned long long *sizes, unsigned int room)
{
    const ContainerLayout *const layout = cubin_layout();
    return (layout != NULL) ? container_code_sections(layout, cubin, size, offsets, sizes, room) : 0u;
}
