// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// icon_file.c: the program's icon file, orior.ico, written into the folder it runs in: the eye the window draws as
// its icon, at every size the system shows an icon, each image a 32-bit bitmap with its alpha and an empty mask
#include "../../../../../ui/cli/src/command_line/cli_internal.h"
#include "../../../../../ui/cli/src/view/window.h"

#include <stdio.h>

// the sizes the file holds, the system's own from the list's smallest to the largest it shows
static const unsigned int s_sizes[] = {16u, 24u, 32u, 48u, 64u, 128u, 256u};
#define ICON_SIZES (sizeof(s_sizes) / sizeof(s_sizes[0]))
// the file's head and one image's entry, and a bitmap's header, in bytes as the format lays them
#define ICON_HEAD 6u
#define ICON_ENTRY 16u
#define ICON_BITMAP_HEADER 40u

static void woken(void *context)
{
    (void)context;
}

// `value`'s low `bytes` written little-endian
static void put(FILE *file, unsigned int value, unsigned int bytes)
{
    for (unsigned int byte = 0u; byte < bytes; byte += 1u)
    {
        fputc((int)((value >> (8u * byte)) & 0xffu), file);
    }
}

// the bytes of one image: the bitmap header, the pixels, and the mask of one bit a pixel with rows padded to a word
static unsigned int image_bytes(unsigned int size)
{
    const unsigned int mask_row = ((size + 31u) / 32u) * 4u;
    return ICON_BITMAP_HEADER + size * size * 4u + mask_row * size;
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    static GuiWindow window;
    if (!EMBED_CALL(gui_window_woken.begin, GuiWindowWokenCfg, .window = &window, .err_msg = &why))
    {
        mmgr_line_finish(&why);
        printf("the window did not begin: %s\n", why_text);
        return 1;
    }
    FILE *const file = fopen("orior.ico", "wb");
    if (file == NULL)
    {
        printf("orior.ico could not be written\n");
        return 1;
    }
    put(file, 0u, 2u);
    put(file, 1u, 2u);
    put(file, (unsigned int)ICON_SIZES, 2u);
    unsigned int offset = ICON_HEAD + ICON_ENTRY * (unsigned int)ICON_SIZES;
    for (unsigned int at = 0u; at < ICON_SIZES; at += 1u)
    {
        const unsigned int size = s_sizes[at];
        // a side of 256 is written as 0, the byte's wrap the format reads as 256
        put(file, size & 0xffu, 1u);
        put(file, size & 0xffu, 1u);
        put(file, 0u, 2u);
        put(file, 1u, 2u);
        put(file, 32u, 2u);
        put(file, image_bytes(size), 4u);
        put(file, offset, 4u);
        offset += image_bytes(size);
    }
    for (unsigned int at = 0u; at < ICON_SIZES; at += 1u)
    {
        const unsigned int size = s_sizes[at];
        GuiRaster icon;
        memor.set(&(MemoriaCfg){.dst = &icon, .val = 0, .bytes = sizeof(icon)});
        if (!EMBED_CALL(cli_window_icon.icon, GuiWindowIconCfg, .window = &window, .icon = &icon, .size = size))
        {
            printf("the icon at %u was not drawn\n", size);
            fclose(file);
            return 1;
        }
        put(file, ICON_BITMAP_HEADER, 4u);
        put(file, size, 4u);
        // the header's height counts the pixels and the mask both
        put(file, size * 2u, 4u);
        put(file, 1u, 2u);
        put(file, 32u, 2u);
        put(file, 0u, 4u);
        put(file, size * size * 4u, 4u);
        put(file, 0u, 4u);
        put(file, 0u, 4u);
        put(file, 0u, 4u);
        put(file, 0u, 4u);
        // the rows bottom up, a pixel's blue, green, red and alpha bytes in that order, which a color's word holds
        for (unsigned int row = size; row > 0u; row -= 1u)
        {
            for (unsigned int column = 0u; column < size; column += 1u)
            {
                put(file, icon.pixels[(row - 1u) * size + column], 4u);
            }
        }
        const unsigned int mask_row = ((size + 31u) / 32u) * 4u;
        for (unsigned int byte = 0u; byte < mask_row * size; byte += 1u)
        {
            put(file, 0u, 1u);
        }
        EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &icon);
    }
    fclose(file);
    EMBED_CALL(gui_window_table.end, GuiWindowCfg, .window = &window);
    printf("orior.ico written with %u sizes, %u to %u\n", (unsigned int)ICON_SIZES, s_sizes[0], s_sizes[ICON_SIZES - 1u]);
    return 0;
}
