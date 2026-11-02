// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// xsettings_test.c: the XSETTINGS reader's check: the animations setting found past a string and a color in either byte
// order, a list without it not answered, and a list cut short anywhere not read past its end
#include "../../../../../ui/gui/src/xsettings.h"
#include "../cli/cli_check.h"

// a list written out in one byte order: the head, a string, a color and an integer named `name`, `size` letters
static unsigned long long list_written(unsigned char *out, int high, const char *name, unsigned int size, unsigned int value)
{
    unsigned long long at = 0ull;
    const unsigned int words[3] = {0u, 1u, 3u};
    out[at] = high ? 1u : 0u;
    out[at + 1ull] = 0u;
    out[at + 2ull] = 0u;
    out[at + 3ull] = 0u;
    at += 4ull;
    for (unsigned int word = 1u; word < 3u; word += 1u)
    {
        for (unsigned int byte = 0u; byte < 4u; byte += 1u)
        {
            out[at] = (unsigned char)(words[word] >> (high ? (8u * (3u - byte)) : (8u * byte)));
            at += 1ull;
        }
    }
    // a string: its name "N", padded to four, its serial, its length and "ab", padded to four
    static const unsigned char s_low_string[20] = {1u, 0u, 1u, 0u, 'N', 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 0u, 0u, 0u, 'a', 'b', 0u, 0u};
    static const unsigned char s_high_string[20] = {1u, 0u, 0u, 1u, 'N', 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 'a', 'b', 0u, 0u};
    for (unsigned int byte = 0u; byte < 20u; byte += 1u)
    {
        out[at] = high ? s_high_string[byte] : s_low_string[byte];
        at += 1ull;
    }
    // a color: its name "C", padded to four, its serial and its eight bytes
    static const unsigned char s_low_color[20] = {2u, 0u, 1u, 0u, 'C', 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    static const unsigned char s_high_color[20] = {2u, 0u, 0u, 1u, 'C', 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    for (unsigned int byte = 0u; byte < 20u; byte += 1u)
    {
        out[at] = high ? s_high_color[byte] : s_low_color[byte];
        at += 1ull;
    }
    out[at] = 0u;
    out[at + 1ull] = 0u;
    out[at + 2ull] = high ? 0u : (unsigned char)size;
    out[at + 3ull] = high ? (unsigned char)size : 0u;
    at += 4ull;
    for (unsigned int letter = 0u; letter < ((size + 3u) & ~3u); letter += 1u)
    {
        out[at] = (letter < size) ? (unsigned char)name[letter] : 0u;
        at += 1ull;
    }
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        out[at] = 0u;
        at += 1ull;
    }
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        out[at] = (unsigned char)(value >> (high ? (8u * (3u - byte)) : (8u * byte)));
        at += 1ull;
    }
    return at;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    static unsigned char s_list[128];
    int enabled = -1;
    const unsigned long long off = list_written(s_list, 0, "Gtk/EnableAnimations", 20u, 0u);
    cli_check(gui_xsettings_animations(s_list, off, &enabled) && (enabled == 0), "animations turned off are read, past a string, low byte first");
    enabled = -1;
    const unsigned long long on = list_written(s_list, 0, "Gtk/EnableAnimations", 20u, 1u);
    cli_check(gui_xsettings_animations(s_list, on, &enabled) && (enabled == 1), "animations on are read");
    enabled = -1;
    const unsigned long long high = list_written(s_list, 1, "Gtk/EnableAnimations", 20u, 0u);
    cli_check(gui_xsettings_animations(s_list, high, &enabled) && (enabled == 0), "and high byte first");
    enabled = -1;
    const unsigned long long other = list_written(s_list, 0, "Gtk/EnableAnimationZ", 20u, 0u);
    cli_check(!gui_xsettings_animations(s_list, other, &enabled) && (enabled == -1), "a list without the setting does not answer");
    const unsigned long long whole = list_written(s_list, 0, "Gtk/EnableAnimations", 20u, 0u);
    int cut_answered = 0;
    for (unsigned long long size = 0ull; size < whole; size += 1ull)
    {
        cut_answered += gui_xsettings_animations(s_list, size, &enabled);
    }
    cli_check(cut_answered == 0, "a list cut short anywhere does not answer");
    cli_check(!gui_xsettings_animations(NULL, 0ull, &enabled), "and nor does no list");
    return cli_checked();
}
