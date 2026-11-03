// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// font.c: a font's tables read, its character map, a glyph's advance and place, and the system's monospace fonts
#include "font.h"
#include "../command_line/callback_cfg.h"

unsigned int cli_font_u16(const CliFontAtCfg *args)
{
    return ((args->at + 2ULL) <= args->font->size)
               ? (((unsigned int)args->font->data[args->at] << 8u) | args->font->data[args->at + 1u])
               : 0u;
}

int cli_font_i16(const CliFontAtCfg *args)
{
    const unsigned int value = cli_font_at.u16(args);
    // the two bytes read as the signed number they write
    return (value >= 0x8000u) ? ((int)value - 0x10000) : (int)value;
}

typedef struct
{
    const CliFont *font;
    unsigned long long at;
    const char *tag;
    unsigned long long *at_pointer;
    CliFont *font_out;
    unsigned int start;
    unsigned int end;
    unsigned int glyph;
    const char *one;
    const char *other;
    char (*folders)[CLI_PATH_MAX_LEN];
    unsigned int count;
    const char *name;
    char *found;
    unsigned int *count_pointer;
    const char *base;
    const char *under;
} FontCtx;
EMBED_INLINE unsigned long long cli_u32(const FontCtx *args)
{
    const CliFont *font = args->font;
    unsigned long long at = args->at;
    return ((at + 4ULL) <= font->size)
               ? (((unsigned long long)EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = at) << 16u) |
                  EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = at + 2u))
               : 0ULL;
}

// the place of the table `tag`, 1 where it is found
EMBED_INLINE int cli_table(const FontCtx *args)
{
    unsigned long long *at = args->at_pointer;
    for (unsigned int index = args->font->tables; index > 0u; index -= 1u)
    {
        if (memor.cmp(&(MemoriaCfg){.src = args->font->tag[index - 1u], .other = args->tag, .bytes = 4u}) == 0)
        {
            *at = args->font->table[index - 1u];
            return 1;
        }
    }
    return 0;
}

// a range of characters put after the map's
EMBED_INLINE void cli_range(const FontCtx *args)
{
    if (args->font_out->ranges < CLI_FONT_RANGES)
    {
        args->font_out->range[args->font_out->ranges][0] = args->start;
        args->font_out->range[args->font_out->ranges][1] = args->end;
        args->font_out->range[args->font_out->ranges][2] = args->glyph;
        args->font_out->ranges += 1u;
    }
}

// The character map as ranges of characters and the glyph each range starts at: format 12 where the font has it,
// else format 4. 1, or 0 where it has neither
EMBED_INLINE int cli_cmap_read(const FontCtx *args)
{
    CliFont *font = args->font_out;
    unsigned long long cmap = 0ULL;
    if (!EMBED_CALL(cli_table, FontCtx, .font = font, .tag = "cmap", .at_pointer = &cmap))
    {
        return 0;
    }
    const unsigned int count = EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = cmap + 2u);
    unsigned int best_rank = 0u;
    unsigned long long best = 0ULL;
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const unsigned long long at = cmap + 4u + (index * 8ULL);
        const unsigned int platform = EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = at);
        const unsigned int encoding = EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = at + 2u);
        const unsigned long long offset = cmap + EMBED_CALL(cli_u32, FontCtx, .font = font, .at = at + 4u);
        const unsigned int format = EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = offset);
        unsigned int rank;
        if (((platform == 3u) && (encoding == 10u) && (format == 12u)) ||
            ((platform == 0u) && ((encoding == 4u) || (encoding == 6u)) && (format == 12u)))
        {
            rank = 3u;
        }
        else if (((platform == 3u) && (encoding == 1u) && (format == 4u)) || ((platform == 0u) && (format == 4u)))
        {
            rank = 2u;
        }
        else
        {
            rank = 0u;
        }
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
    if (EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = best) == 12u)
    {
        const unsigned long long groups = EMBED_CALL(cli_u32, FontCtx, .font = font, .at = best + 12u);
        for (unsigned long long group = 0ULL; group < groups; group += 1ULL)
        {
            const unsigned long long at = best + 16u + (group * 12ULL);
            // each is a 32 bit field of the table
            EMBED_CALL(cli_range, FontCtx, .font_out = font,
                       .start = (unsigned int)EMBED_CALL(cli_u32, FontCtx, .font = font, .at = at),
                       .end = (unsigned int)EMBED_CALL(cli_u32, FontCtx, .font = font, .at = at + 4u),
                       .glyph = (unsigned int)EMBED_CALL(cli_u32, FontCtx, .font = font, .at = at + 8u));
        }
        return 1;
    }
    const unsigned int segments = EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = best + 6u) / 2u;
    const unsigned long long ends = best + 14u;
    const unsigned long long starts = ends + (segments * 2ULL) + 2ULL;
    const unsigned long long deltas = starts + (segments * 2ULL);
    const unsigned long long offsets = deltas + (segments * 2ULL);
    for (unsigned int segment = 0u; segment < segments; segment += 1u)
    {
        const unsigned int end = EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = ends + (segment * 2ULL));
        const unsigned int start =
            EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = starts + (segment * 2ULL));
        const unsigned int delta =
            EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = deltas + (segment * 2ULL));
        const unsigned int range_offset =
            EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = offsets + (segment * 2ULL));
        if (start == 0xffffu)
        {
            continue;
        }
        for (unsigned int code = start; code <= end; code += 1u)
        {
            unsigned int glyph = (code + delta) & 0xffffu;
            if (range_offset != 0u)
            {
                const unsigned long long at = offsets + (segment * 2ULL) + range_offset + ((code - start) * 2ULL);
                const unsigned int found = EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = at);
                glyph = (found == 0u) ? 0u : ((found + delta) & 0xffffu);
            }
            EMBED_CALL(cli_range, FontCtx, .font_out = font, .start = code, .end = code, .glyph = glyph);
        }
    }
    return 1;
}

int cli_font_read(const CliFontDataCfg *args)
{
    CliFont *font = args->font_out;
    const unsigned char *data = args->data;
    unsigned long long size = args->size;
    unsigned int (*ranges)[3] = args->ranges;
    font->data = data;
    font->size = size;
    font->range = ranges;
    font->ranges = 0u;
    font->tables = 0u;
    const unsigned long long base =
        ((size >= 4u) && (memor.cmp(&(MemoriaCfg){.src = data, .other = "ttcf", .bytes = 4u}) == 0))
            ? EMBED_CALL(cli_u32, FontCtx, .font = font, .at = 12u)
            : 0ULL;
    const unsigned int count = EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = base + 4u);
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const unsigned long long at = base + 12u + (index * 16ULL);
        if (((at + 4ULL) > size) || (font->tables >= CLI_FONT_TABLES))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = font->tag[font->tables], .src = data + at, .bytes = 4u});
        font->table[font->tables] = EMBED_CALL(cli_u32, FontCtx, .font = font, .at = at + 8u);
        font->tables += 1u;
    }
    unsigned long long head = 0ULL;
    unsigned long long hhea = 0ULL;
    unsigned long long maxp = 0ULL;
    unsigned long long place = 0ULL;
    if (!EMBED_CALL(cli_table, FontCtx, .font = font, .tag = "head", .at_pointer = &head) ||
        !EMBED_CALL(cli_table, FontCtx, .font = font, .tag = "hhea", .at_pointer = &hhea) ||
        !EMBED_CALL(cli_table, FontCtx, .font = font, .tag = "maxp", .at_pointer = &maxp) ||
        !EMBED_CALL(cli_table, FontCtx, .font = font, .tag = "glyf", .at_pointer = &place) ||
        !EMBED_CALL(cli_table, FontCtx, .font = font, .tag = "loca", .at_pointer = &place))
    {
        return 0;
    }
    font->units = (float)EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = head + 18u);
    font->long_loca = EMBED_CALL(cli_font_at.i16, CliFontAtCfg, .font = font, .at = head + 50u) == 1;
    font->ascent = (float)EMBED_CALL(cli_font_at.i16, CliFontAtCfg, .font = font, .at = hhea + 4u);
    font->descent = (float)EMBED_CALL(cli_font_at.i16, CliFontAtCfg, .font = font, .at = hhea + 6u);
    font->metrics = EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = hhea + 34u);
    font->glyphs = EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = font, .at = maxp + 4u);
    return EMBED_CALL(cli_cmap_read, FontCtx, .font_out = font);
}

unsigned int cli_font_glyph(const CliFontCfg *args)
{
    // the first range whose last character is not before the letter, found by halves
    unsigned int low = 0u;
    unsigned int high = args->font->ranges;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (args->font->range[middle][1] < args->letter)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    if ((low < args->font->ranges) && (args->font->range[low][0] <= args->letter))
    {
        return (args->font->range[low][2] + (args->letter - args->font->range[low][0])) & 0xffffu;
    }
    return 0u;
}

float cli_font_advance(const CliFontGlyphCfg *args)
{
    unsigned long long hmtx = 0ULL;
    if (!EMBED_CALL(cli_table, FontCtx, .font = args->font, .tag = "hmtx", .at_pointer = &hmtx))
    {
        return args->font->units / 2.0f;
    }
    const unsigned int last = (args->font->metrics > 0u) ? (args->font->metrics - 1u) : 0u;
    const unsigned int index = (args->glyph < last) ? args->glyph : last;
    return (float)EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = args->font, .at = hmtx + (index * 4ULL));
}

int cli_font_glyph_range(const CliFontGlyphCfg *args)
{
    unsigned long long *from = args->from;
    unsigned long long *to = args->to;
    unsigned long long loca = 0ULL;
    unsigned long long glyf = 0ULL;
    if ((args->glyph >= args->font->glyphs) ||
        !EMBED_CALL(cli_table, FontCtx, .font = args->font, .tag = "loca", .at_pointer = &loca) ||
        !EMBED_CALL(cli_table, FontCtx, .font = args->font, .tag = "glyf", .at_pointer = &glyf))
    {
        return 0;
    }
    const unsigned long long start =
        args->font->long_loca
            ? EMBED_CALL(cli_u32, FontCtx, .font = args->font, .at = loca + (args->glyph * 4ULL))
            : (EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = args->font, .at = loca + (args->glyph * 2ULL)) * 2ULL);
    const unsigned long long end =
        args->font->long_loca
            ? EMBED_CALL(cli_u32, FontCtx, .font = args->font, .at = loca + (args->glyph * 4ULL) + 4u)
            : (EMBED_CALL(cli_font_at.u16, CliFontAtCfg, .font = args->font, .at = loca + (args->glyph * 2ULL) + 2u) *
               2ULL);
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

EMBED_INLINE int cli_same_name(const FontCtx *args)
{
    const char *other = args->other;
    return cellul.eq(&(CatenaFinitaCfg){.src = args->one, .other = other, .cap = CLI_CELLS_POOL, .ci = 1});
}

static void cli_walk_each(const CliEachNameIsFolderCfg *args)
{
    void *context = args->context;
    CliFontWalk *const walk = (CliFontWalk *)context;
    if (walk->found[0] != '\0')
    {
        return;
    }
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, walk->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return;
    }
    if (args->is_folder && (walk->depth > 1u))
    {
        CliFontWalk inner = {walk->name, walk->found, path, walk->depth - 1u};
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = path, .each = cli_walk_each,
                   .context_bytess = &inner);
    }
    else if (!args->is_folder && EMBED_CALL(cli_same_name, FontCtx, .one = args->name, .other = walk->name))
    {
        memor.cpy(&(MemoriaCfg){.dst = walk->found,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    }
}

// The font file `name` found in the folders: in one of them, or, off Windows, under one to a depth of four, as the
// fonts of Linux distributions stand in folders of their own under the fonts folder
EMBED_INLINE int cli_find(const FontCtx *args)
{
    args->found[0] = '\0';
    if (args->name[0] == '\0')
    {
        return 0;
    }
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        VerbaLinea line = {args->found, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, args->folders[at]);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, args->name);
        if ((mmgr_line_finish(&line) != 0ULL) &&
            EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->found))
        {
            return 1;
        }
        args->found[0] = '\0';
#if !defined(_WIN32)
        CliFontWalk walk = {args->name, args->found, args->folders[at], 4u};
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folders[at],
                   .each = cli_walk_each, .context_bytess = &walk);
        if (args->found[0] != '\0')
        {
            return 1;
        }
#endif
    }
    return 0;
}

// a folder put after the folders looked in, `under` after `base`
EMBED_INLINE void cli_folder(const FontCtx *args)
{
    unsigned int *count = args->count_pointer;
    VerbaLinea line = {args->folders[*count], CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->base);
    mmgr_line_put(&line, args->under);
    *count += (mmgr_line_finish(&line) != 0ULL) ? 1u : 0u;
}

// the folders the system saves its fonts in, into `folders`, which is sized for four: their count
EMBED_INLINE unsigned int cli_font_folders(const FontCtx *args)
{
    unsigned int count = 0u;
    char named[CLI_PATH_MAX_LEN];
#if defined(_WIN32)
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "WINDIR", .value_out = named,
                   .capacity = sizeof(named)))
    {
        EMBED_CALL(cli_folder, FontCtx, .folders = args->folders, .count_pointer = &count, .base = named,
                   .under = "/Fonts");
    }
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "LOCALAPPDATA",
                   .value_out = named, .capacity = sizeof(named)))
    {
        EMBED_CALL(cli_folder, FontCtx, .folders = args->folders, .count_pointer = &count, .base = named,
                   .under = "/Microsoft/Windows/Fonts");
    }
#elif defined(__APPLE__)
    EMBED_CALL(cli_folder, FontCtx, .folders = args->folders, .count_pointer = &count, .base = "/System/Library/Fonts",
               .under = "");
    EMBED_CALL(cli_folder, FontCtx, .folders = args->folders, .count_pointer = &count, .base = "/Library/Fonts",
               .under = "");
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME", .value_out = named,
                   .capacity = sizeof(named)))
    {
        EMBED_CALL(cli_folder, FontCtx, .folders = args->folders, .count_pointer = &count, .base = named,
                   .under = "/Library/Fonts");
    }
#else
    EMBED_CALL(cli_folder, FontCtx, .folders = args->folders, .count_pointer = &count, .base = "/usr/share/fonts",
               .under = "");
    EMBED_CALL(cli_folder, FontCtx, .folders = args->folders, .count_pointer = &count, .base = "/usr/local/share/fonts",
               .under = "");
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME", .value_out = named,
                   .capacity = sizeof(named)))
    {
        EMBED_CALL(cli_folder, FontCtx, .folders = args->folders, .count_pointer = &count, .base = named,
                   .under = "/.local/share/fonts");
    }
#endif
    return count;
}

void cli_font_find(const CliFontFamiliesCfg *args)
{
    unsigned int count = args->count;
    char folders[4][CLI_PATH_MAX_LEN];
    const unsigned int folder_count = EMBED_CALL(cli_font_folders, FontCtx, .folders = folders);
    for (unsigned int face = 0u; face < 4u; face += 1u)
    {
        args->paths[face][0] = '\0';
    }
    for (unsigned int family = 0u; family < count; family += 1u)
    {
        if (EMBED_CALL(cli_find, FontCtx, .folders = folders, .count = folder_count, .name = args->families[family][0],
                       .found = args->paths[0]))
        {
            for (unsigned int face = 1u; face < 4u; face += 1u)
            {
                EMBED_CALL(cli_find, FontCtx, .folders = folders, .count = folder_count,
                           .name = args->families[family][face], .found = args->paths[face]);
            }
            return;
        }
    }
}

void cli_font_monospace(const CliFontFamiliesCfg *args)
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
    EMBED_CALL(cli_font_families.find, CliFontFamiliesCfg, .families = s_families, .count = 8u, .paths = args->paths);
}
