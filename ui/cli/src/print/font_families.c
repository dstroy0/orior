// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// font_families.c: the font files of a family found in the system's font folders, and the monospace fonts impero draws
// with
#include "font_families.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"

typedef struct
{
    const char *one;
    const char *other;
    char (*folders)[CLI_PATH_MAX_LEN];
    unsigned int count;
    const char *name;
    char *found;
    unsigned int *count_pointer;
    const char *base;
    const char *under;
} FamiliesCtx;

// Each file found under a folder, to a depth, whose name is `name` in ASCII's either case
typedef struct
{
    const char *name;
    char *found;
    const char *folder;
    unsigned int depth;
} CliFontWalk;

EMBED_INLINE int cli_same_name(const FamiliesCtx *args)
{
    const char *other = args->other;
    return cellul.eq(&(CatenaFinitaCfg){.src = args->one, .other = other, .cap = CLI_POOL_BYTES, .ci = 1});
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
    else if (!args->is_folder && EMBED_CALL(cli_same_name, FamiliesCtx, .one = args->name, .other = walk->name))
    {
        memor.cpy(&(MemoriaCfg){.dst = walk->found,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}) + 1u});
    }
}

// The font file `name` found in the folders: in one of them, or, off Windows, under one to a depth of four, as the
// fonts of Linux distributions stand in folders of their own under the fonts folder
EMBED_INLINE int cli_find(const FamiliesCtx *args)
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
EMBED_INLINE void cli_folder(const FamiliesCtx *args)
{
    unsigned int *count = args->count_pointer;
    VerbaLinea line = {args->folders[*count], CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->base);
    mmgr_line_put(&line, args->under);
    *count += (mmgr_line_finish(&line) != 0ULL) ? 1u : 0u;
}

// the folders the system saves its fonts in, into `folders`, which is sized for four: their count
EMBED_INLINE unsigned int cli_font_folders(const FamiliesCtx *args)
{
    unsigned int count = 0u;
    char named[CLI_PATH_MAX_LEN];
#if defined(_WIN32)
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "WINDIR", .value_out = named,
                   .capacity = sizeof(named)))
    {
        EMBED_CALL(cli_folder, FamiliesCtx, .folders = args->folders, .count_pointer = &count, .base = named,
                   .under = "/Fonts");
    }
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "LOCALAPPDATA",
                   .value_out = named, .capacity = sizeof(named)))
    {
        EMBED_CALL(cli_folder, FamiliesCtx, .folders = args->folders, .count_pointer = &count, .base = named,
                   .under = "/Microsoft/Windows/Fonts");
    }
#elif defined(__APPLE__)
    EMBED_CALL(cli_folder, FamiliesCtx, .folders = args->folders, .count_pointer = &count,
               .base = "/System/Library/Fonts", .under = "");
    EMBED_CALL(cli_folder, FamiliesCtx, .folders = args->folders, .count_pointer = &count, .base = "/Library/Fonts",
               .under = "");
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME", .value_out = named,
                   .capacity = sizeof(named)))
    {
        EMBED_CALL(cli_folder, FamiliesCtx, .folders = args->folders, .count_pointer = &count, .base = named,
                   .under = "/Library/Fonts");
    }
#else
    EMBED_CALL(cli_folder, FamiliesCtx, .folders = args->folders, .count_pointer = &count, .base = "/usr/share/fonts",
               .under = "");
    EMBED_CALL(cli_folder, FamiliesCtx, .folders = args->folders, .count_pointer = &count,
               .base = "/usr/local/share/fonts", .under = "");
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME", .value_out = named,
                   .capacity = sizeof(named)))
    {
        EMBED_CALL(cli_folder, FamiliesCtx, .folders = args->folders, .count_pointer = &count, .base = named,
                   .under = "/.local/share/fonts");
    }
#endif
    return count;
}

void cli_font_find(const CliFontFamiliesCfg *args)
{
    unsigned int count = args->count;
    char folders[4][CLI_PATH_MAX_LEN];
    const unsigned int folder_count = EMBED_CALL(cli_font_folders, FamiliesCtx, .folders = folders);
    for (unsigned int face = 0u; face < 4u; face += 1u)
    {
        args->paths[face][0] = '\0';
    }
    for (unsigned int family = 0u; family < count; family += 1u)
    {
        if (EMBED_CALL(cli_find, FamiliesCtx, .folders = folders, .count = folder_count,
                       .name = args->families[family][0], .found = args->paths[0]))
        {
            for (unsigned int face = 1u; face < 4u; face += 1u)
            {
                EMBED_CALL(cli_find, FamiliesCtx, .folders = folders, .count = folder_count,
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
