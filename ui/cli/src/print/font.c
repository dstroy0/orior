// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// font.c: a font's tables read, its character map, a glyph's advance and place, and the system's monospace fonts
#include "font.h"

unsigned int cli_font_u16(const CliFont *font, unsigned long long at)
{
    return ((at + 2ull) <= font->size) ? (((unsigned int)font->data[at] << 8u) | font->data[at + 1u]) : 0u;
}

int cli_font_i16(const CliFont *font, unsigned long long at)
{
    const unsigned int value = cli_font_u16(font, at);
    // the two bytes read as the signed number they write
    return (value >= 0x8000u) ? ((int)value - 0x10000) : (int)value;
}

static unsigned long long cli_u32(const CliFont *font, unsigned long long at)
{
    return ((at + 4ull) <= font->size)
               ? (((unsigned long long)cli_font_u16(font, at) << 16u) | cli_font_u16(font, at + 2u))
               : 0ull;
}

// the place of the table `tag`, 1 where it is found
static int cli_table(const CliFont *font, const char *tag, unsigned long long *at)
{
    for (unsigned int index = font->tables; index > 0u; index -= 1u)
    {
        if (memor.cmp(&(MemoriaCfg){.src = font->tag[index - 1u], .other = tag, .bytes = 4u}) == 0)
        {
            *at = font->table[index - 1u];
            return 1;
        }
    }
    return 0;
}

// a range of characters put after the map's
static void cli_range(CliFont *font, unsigned int start, unsigned int end, unsigned int glyph)
{
    if (font->ranges < CLI_FONT_RANGES)
    {
        font->range[font->ranges][0] = start;
        font->range[font->ranges][1] = end;
        font->range[font->ranges][2] = glyph;
        font->ranges += 1u;
    }
}

// The character map as ranges of characters and the glyph each range starts at: format 12 where the font has it,
// else format 4. 1, or 0 where it has neither
static int cli_cmap_read(CliFont *font)
{
    unsigned long long cmap = 0ull;
    if (!cli_table(font, "cmap", &cmap))
    {
        return 0;
    }
    const unsigned int count = cli_font_u16(font, cmap + 2u);
    unsigned int best_rank = 0u;
    unsigned long long best = 0ull;
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const unsigned long long at = cmap + 4u + (index * 8ull);
        const unsigned int platform = cli_font_u16(font, at);
        const unsigned int encoding = cli_font_u16(font, at + 2u);
        const unsigned long long offset = cmap + cli_u32(font, at + 4u);
        const unsigned int format = cli_font_u16(font, offset);
        const unsigned int rank =
            (((platform == 3u) && (encoding == 10u) && (format == 12u)) ||
             ((platform == 0u) && ((encoding == 4u) || (encoding == 6u)) && (format == 12u)))
                ? 3u
            : (((platform == 3u) && (encoding == 1u) && (format == 4u)) || ((platform == 0u) && (format == 4u))) ? 2u
                                                                                                                 : 0u;
        if (rank > best_rank)
        {
            best_rank = rank;
            best = offset;
        }
    }
    if (best_rank == 0u)
    {
        return 0;
    }
    font->ranges = 0u;
    if (cli_font_u16(font, best) == 12u)
    {
        const unsigned long long groups = cli_u32(font, best + 12u);
        for (unsigned long long group = 0ull; group < groups; group += 1ull)
        {
            const unsigned long long at = best + 16u + (group * 12ull);
            // each is a 32 bit field of the table
            cli_range(font, (unsigned int)cli_u32(font, at), (unsigned int)cli_u32(font, at + 4u),
                      (unsigned int)cli_u32(font, at + 8u));
        }
        return 1;
    }
    const unsigned int segments = cli_font_u16(font, best + 6u) / 2u;
    const unsigned long long ends = best + 14u;
    const unsigned long long starts = ends + (segments * 2ull) + 2ull;
    const unsigned long long deltas = starts + (segments * 2ull);
    const unsigned long long offsets = deltas + (segments * 2ull);
    for (unsigned int segment = 0u; segment < segments; segment += 1u)
    {
        const unsigned int end = cli_font_u16(font, ends + (segment * 2ull));
        const unsigned int start = cli_font_u16(font, starts + (segment * 2ull));
        const unsigned int delta = cli_font_u16(font, deltas + (segment * 2ull));
        const unsigned int range_offset = cli_font_u16(font, offsets + (segment * 2ull));
        if (start == 0xffffu)
        {
            continue;
        }
        for (unsigned int code = start; code <= end; code += 1u)
        {
            unsigned int glyph = (code + delta) & 0xffffu;
            if (range_offset != 0u)
            {
                const unsigned long long at = offsets + (segment * 2ull) + range_offset + ((code - start) * 2ull);
                const unsigned int found = cli_font_u16(font, at);
                glyph = (found == 0u) ? 0u : ((found + delta) & 0xffffu);
            }
            cli_range(font, code, code, glyph);
        }
    }
    return 1;
}

int cli_font_read(CliFont *font, const unsigned char *data, unsigned long long size, unsigned int (*ranges)[3])
{
    font->data = data;
    font->size = size;
    font->range = ranges;
    font->ranges = 0u;
    font->tables = 0u;
    const unsigned long long base =
        ((size >= 4u) && (memor.cmp(&(MemoriaCfg){.src = data, .other = "ttcf", .bytes = 4u}) == 0))
            ? cli_u32(font, 12u)
            : 0ull;
    const unsigned int count = cli_font_u16(font, base + 4u);
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const unsigned long long at = base + 12u + (index * 16ull);
        if (((at + 4ull) > size) || (font->tables >= CLI_FONT_TABLES))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = font->tag[font->tables], .src = data + at, .bytes = 4u});
        font->table[font->tables] = cli_u32(font, at + 8u);
        font->tables += 1u;
    }
    unsigned long long head = 0ull;
    unsigned long long hhea = 0ull;
    unsigned long long maxp = 0ull;
    unsigned long long place = 0ull;
    if (!cli_table(font, "head", &head) || !cli_table(font, "hhea", &hhea) || !cli_table(font, "maxp", &maxp) ||
        !cli_table(font, "glyf", &place) || !cli_table(font, "loca", &place))
    {
        return 0;
    }
    font->units = (float)cli_font_u16(font, head + 18u);
    font->long_loca = cli_font_i16(font, head + 50u) == 1;
    font->ascent = (float)cli_font_i16(font, hhea + 4u);
    font->descent = (float)cli_font_i16(font, hhea + 6u);
    font->metrics = cli_font_u16(font, hhea + 34u);
    font->glyphs = cli_font_u16(font, maxp + 4u);
    return cli_cmap_read(font);
}

unsigned int cli_font_glyph(const CliFont *font, unsigned int letter)
{
    // the first range whose last character is not before the letter, found by halves
    unsigned int low = 0u;
    unsigned int high = font->ranges;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (font->range[middle][1] < letter)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    if ((low < font->ranges) && (font->range[low][0] <= letter))
    {
        return (font->range[low][2] + (letter - font->range[low][0])) & 0xffffu;
    }
    return 0u;
}

float cli_font_advance(const CliFont *font, unsigned int glyph)
{
    unsigned long long hmtx = 0ull;
    if (!cli_table(font, "hmtx", &hmtx))
    {
        return font->units / 2.0f;
    }
    const unsigned int last = (font->metrics > 0u) ? (font->metrics - 1u) : 0u;
    const unsigned int index = (glyph < last) ? glyph : last;
    return (float)cli_font_u16(font, hmtx + (index * 4ull));
}

int cli_font_glyph_range(const CliFont *font, unsigned int glyph, unsigned long long *from, unsigned long long *to)
{
    unsigned long long loca = 0ull;
    unsigned long long glyf = 0ull;
    if ((glyph >= font->glyphs) || !cli_table(font, "loca", &loca) || !cli_table(font, "glyf", &glyf))
    {
        return 0;
    }
    const unsigned long long start =
        font->long_loca ? cli_u32(font, loca + (glyph * 4ull)) : (cli_font_u16(font, loca + (glyph * 2ull)) * 2ull);
    const unsigned long long end = font->long_loca ? cli_u32(font, loca + (glyph * 4ull) + 4u)
                                                   : (cli_font_u16(font, loca + (glyph * 2ull) + 2u) * 2ull);
    *from = glyf + start;
    *to = glyf + end;
    return end > start;
}

// Each file found under a folder, to a depth, whose name is `name` in ASCII's either case
typedef struct
{
    const char *name;
    char *found;
    const char *folder;
    unsigned int depth;
} CliFontWalk;

static int cli_same_name(const char *one, const char *other)
{
    for (; (one[0] != '\0') && (other[0] != '\0'); one += 1, other += 1)
    {
        const char left = ((one[0] >= 'A') && (one[0] <= 'Z')) ? (char)(one[0] + 32) : one[0];
        const char right = ((other[0] >= 'A') && (other[0] <= 'Z')) ? (char)(other[0] + 32) : other[0];
        if (left != right)
        {
            return 0;
        }
    }
    return (one[0] == '\0') && (other[0] == '\0');
}

static void cli_walk_each(void *context, const char *name, int is_folder)
{
    CliFontWalk *const walk = (CliFontWalk *)context;
    if (walk->found[0] != '\0')
    {
        return;
    }
    char path[CLI_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, walk->folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    if (cli_line_closed(&line) == 0ull)
    {
        return;
    }
    if (is_folder && (walk->depth > 1u))
    {
        CliFontWalk inner = {walk->name, walk->found, path, walk->depth - 1u};
        query_folder_each(path, cli_walk_each, &inner);
    }
    else if (!is_folder && cli_same_name(name, walk->name))
    {
        memor.cpy(&(MemoriaCfg){.dst = walk->found,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    }
}

// The font file `name` found in the folders: in one of them, or, off Windows, under one to a depth of four, as the
// fonts of Linux distributions stand in folders of their own under the fonts folder
static int cli_find(char (*folders)[CLI_PATH_LONGEST], unsigned int count, const char *name, char *found)
{
    found[0] = '\0';
    if (name[0] == '\0')
    {
        return 0;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        CliLine line = {found, CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, folders[at]);
        cli_line_character(&line, '/');
        cli_line_text(&line, name);
        if ((cli_line_closed(&line) != 0ull) && query_is_file(found))
        {
            return 1;
        }
        found[0] = '\0';
#if !defined(_WIN32)
        CliFontWalk walk = {name, found, folders[at], 4u};
        query_folder_each(folders[at], cli_walk_each, &walk);
        if (found[0] != '\0')
        {
            return 1;
        }
#endif
    }
    return 0;
}

// a folder put after the folders looked in, `under` after `base`
static void cli_folder(char (*folders)[CLI_PATH_LONGEST], unsigned int *count, const char *base, const char *under)
{
    CliLine line = {folders[*count], CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, base);
    cli_line_text(&line, under);
    *count += (cli_line_closed(&line) != 0ull) ? 1u : 0u;
}

void cli_font_monospace(char (*paths)[CLI_PATH_LONGEST])
{
    static const char *const s_families[8][4] = {
        {"IBMPlexMono-Regular.ttf", "IBMPlexMono-Bold.ttf", "IBMPlexMono-Italic.ttf", "IBMPlexMono-BoldItalic.ttf"},
        {"consola.ttf", "consolab.ttf", "consolai.ttf", "consolaz.ttf"},
        {"CascadiaMono.ttf", "", "", ""},
        {"DejaVuSansMono.ttf", "DejaVuSansMono-Bold.ttf", "DejaVuSansMono-Oblique.ttf",
         "DejaVuSansMono-BoldOblique.ttf"},
        {"LiberationMono-Regular.ttf", "LiberationMono-Bold.ttf", "LiberationMono-Italic.ttf",
         "LiberationMono-BoldItalic.ttf"},
        {"Menlo.ttc", "", "", ""},
        {"cour.ttf", "courbd.ttf", "couri.ttf", "courbi.ttf"},
        {"Courier New.ttf", "Courier New Bold.ttf", "Courier New Italic.ttf", "Courier New Bold Italic.ttf"}};
    char folders[4][CLI_PATH_LONGEST];
    unsigned int count = 0u;
    char named[CLI_PATH_LONGEST];
#if defined(_WIN32)
    if (query_environment_read("WINDIR", named, sizeof(named)))
    {
        cli_folder(folders, &count, named, "/Fonts");
    }
    if (query_environment_read("LOCALAPPDATA", named, sizeof(named)))
    {
        cli_folder(folders, &count, named, "/Microsoft/Windows/Fonts");
    }
#elif defined(__APPLE__)
    cli_folder(folders, &count, "/System/Library/Fonts", "");
    cli_folder(folders, &count, "/Library/Fonts", "");
    if (query_environment_read("HOME", named, sizeof(named)))
    {
        cli_folder(folders, &count, named, "/Library/Fonts");
    }
#else
    cli_folder(folders, &count, "/usr/share/fonts", "");
    cli_folder(folders, &count, "/usr/local/share/fonts", "");
    if (query_environment_read("HOME", named, sizeof(named)))
    {
        cli_folder(folders, &count, named, "/.local/share/fonts");
    }
#endif
    for (unsigned int face = 0u; face < 4u; face += 1u)
    {
        paths[face][0] = '\0';
    }
    for (unsigned int family = 0u; family < 8u; family += 1u)
    {
        if (cli_find(folders, count, s_families[family][0], paths[0]))
        {
            for (unsigned int face = 1u; face < 4u; face += 1u)
            {
                cli_find(folders, count, s_families[family][face], paths[face]);
            }
            return;
        }
    }
}
