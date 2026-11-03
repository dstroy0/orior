// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs.c: the packages the files of a folder require, read from its `setup.cfg`, Pipfile and `requirements.txt`, by
// name, each once
#include "../command_line/cells.h"
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "envs_internal.h"

// A span of a text: where it starts and its bytes
typedef struct
{
    const char *at;
    unsigned long long length;
} CliSpan;

typedef struct
{
    CliSpan span;
    const char *text;
    CliRequired *required;
    CliSpan name;
    const char *folder;
    const char *name_text;
    unsigned long long *size_pointer;
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long *at;
    CliSpan *line;
} EnvsCtx;
// the span with the whitespace at either end left off
EMBED_INLINE CliSpan cli_trim(const EnvsCtx *args)
{
    unsigned long long start = args->span.length;
    unsigned long long end = 0ULL;
    unsigned long long at = 0ULL;
    while (at < args->span.length)
    {
        unsigned int letter = 0u;
        const unsigned long long step = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                   .text_bytes = (const unsigned char *)args->span.at,
                                                   .length = args->span.length, .at = at, .letter = &letter);
        if (!EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx, .letter = letter))
        {
            start = (start == args->span.length) ? at : start;
            end = at + step;
        }
        at += step;
    }
    return (start < end) ? (CliSpan){args->span.at + start, end - start} : (CliSpan){args->span.at, 0ULL};
}

// whether the span starts with `text`, and whether it is `text`
EMBED_INLINE int cli_starts(const EnvsCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    return (args->span.length >= length) &&
           (memor.cmp(&(MemoriaCfg){.src = args->span.at, .other = args->text, .bytes = length}) == 0);
}

EMBED_INLINE int cli_is(const EnvsCtx *args)
{
    return (args->span.length == cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL})) &&
           cli_starts(args);
}

void cli_required_alloc(const CliEnvsCtx *args)
{
    CliRequired *required = args->required;
    const unsigned long long name_bytes = (unsigned long long)CLI_ENVS_NAMES * sizeof(CliRequiredName);
    char *const cell = g_cli_cells->temporary_buf_alloc(name_bytes + CLI_ENVS_TEXT, &g_cli_envs_cells);
    *required = (CliRequired){NULL, 0u, 0u, NULL, 0u, 0u};
    if (cell != NULL)
    {
        *required =
            (CliRequired){(CliRequiredName *)(void *)cell, 0u, CLI_ENVS_NAMES, cell + name_bytes, 0u, CLI_ENVS_TEXT};
    }
}

// a name put among those required: the letters, numbers, `-`, `_` and `.` it starts with, where they are some and no
// name the same but for ASCII's case is there
static void cli_add(const EnvsCtx *args)
{
    unsigned long long length = 0ULL;
    while (length < args->name.length)
    {
        unsigned int letter = 0u;
        const unsigned long long step = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                   .text_bytes = (const unsigned char *)args->name.at,
                                                   .length = args->name.length, .at = length, .letter = &letter);
        if (!EMBED_CALL(cli_files_letter_internal.hit_wordy, CliFilesLetterCtx, .letter = letter) && (letter != '-') &&
            (letter != '.'))
        {
            break;
        }
        length += step;
    }
    if ((length == 0ULL) || (args->required->count >= CLI_ENVS_NAMES) ||
        ((args->required->used + length) > CLI_ENVS_TEXT))
    {
        return;
    }
    for (unsigned int at = 0u; at < args->required->count; at += 1u)
    {
        if (args->required->name[at].length != length)
        {
            continue;
        }
        unsigned long long same = 0ULL;
        while (same < length)
        {
            const char one = args->required->text[args->required->name[at].start + same];
            const char other = args->name.at[same];
            const char one_lower = ((one >= 'A') && (one <= 'Z')) ? (char)(one + 32) : one;
            const char other_lower = ((other >= 'A') && (other <= 'Z')) ? (char)(other + 32) : other;
            if (one_lower != other_lower)
            {
                break;
            }
            same += 1ULL;
        }
        if (same == length)
        {
            return;
        }
    }
    if ((args->required->count >= args->required->capacity) ||
        ((args->required->used + length) > args->required->text_capacity))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->required->text + args->required->used, .src = args->name.at, .bytes = length});
    args->required->name[args->required->count].start = args->required->used;
    // a name is below CLI_ENVS_TEXT bytes, which an unsigned int represents
    args->required->name[args->required->count].length = (unsigned int)length;
    args->required->used += (unsigned int)length;
    args->required->count += 1u;
}

// the file `name` in `folder` mapped, where it is UTF-8 whole: its bytes, or NULL
EMBED_INLINE const unsigned char *cli_text_map(const EnvsCtx *args)
{
    unsigned long long *size = args->size_pointer;
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder, .relative = args->name_text, .joined = path);
    return EMBED_CALL(cli_path_internal.file_alloc_utf8, CliPathCtx, .path = path, .size_pointer = size,
                      .owner = &g_cli_envs_cells);
}

// the next line of `size` bytes from `*at`, `*at` moved past it: 1, or 0 where the text has ended
EMBED_INLINE int cli_next_line(const EnvsCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long *at = args->at;
    CliSpan *line = args->line;
    if (*at >= size)
    {
        return 0;
    }
    const CliTextLine bounds =
        EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = (const char *)args->bytes, .at = *at, .size = size);
    const unsigned long long end = bounds.stop;
    *line = (CliSpan){(const char *)args->bytes + *at, end - *at};
    line->length -= bounds.stop - bounds.cut;
    *at = end + 1ULL;
    return 1;
}

// the names `setup.cfg` requires under `[options]`, `install_requires`, those on its line and those indented below it
EMBED_INLINE void cli_setup_read(const EnvsCtx *args)
{
    CliSpan line;
    unsigned long long at = 0ULL;
    int options = 0;
    int allocating = 0;
    while (EMBED_CALL(cli_next_line, EnvsCtx, .bytes = args->bytes, .size = args->size, .at = &at, .line = &line))
    {
        const CliSpan trimmed = EMBED_CALL(cli_trim, EnvsCtx, .span = line);
        if (EMBED_CALL(cli_starts, EnvsCtx, .span = trimmed, .text = "["))
        {
            options = EMBED_CALL(cli_is, EnvsCtx, .span = trimmed, .text = "[options]");
            allocating = 0;
            continue;
        }
        if (!options || EMBED_CALL(cli_starts, EnvsCtx, .span = trimmed, .text = "#") || (trimmed.length == 0ULL))
        {
            continue;
        }
        if (EMBED_CALL(cli_starts, EnvsCtx, .span = trimmed, .text = "install_requires"))
        {
            allocating = 1;
            CliSpan rest = {trimmed.at + 16, trimmed.length - 16ULL};
            while ((rest.length > 0ULL) && EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx,
                                                      .letter = (unsigned char)rest.at[0]))
            {
                rest = (CliSpan){rest.at + 1, rest.length - 1ULL};
            }
            if (!EMBED_CALL(cli_starts, EnvsCtx, .span = rest, .text = "="))
            {
                continue;
            }
            unsigned long long from = 1ULL;
            for (unsigned long long place = 1ULL; place <= rest.length; place += 1ULL)
            {
                if ((place == rest.length) || (rest.at[place] == ',') || (rest.at[place] == ';'))
                {
                    EMBED_CALL(cli_add, EnvsCtx, .required = args->required,
                               .name = (CliSpan){rest.at + from, place - from});
                    from = place + 1ULL;
                }
            }
        }
        else if (allocating && (line.length > 0ULL) && ((line.at[0] == ' ') || (line.at[0] == '\t')))
        {
            EMBED_CALL(cli_add, EnvsCtx, .required = args->required, .name = trimmed);
        }
        else
        {
            allocating = 0;
        }
    }
}

// the names a Pipfile requires under `[packages]`, and those of `requirements.txt`, each a line not a comment or an
// option
EMBED_INLINE void cli_pipfile_read(const EnvsCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    CliSpan line;
    unsigned long long at = 0ULL;
    int packages = 0;
    while (EMBED_CALL(cli_next_line, EnvsCtx, .bytes = bytes, .size = size, .at = &at, .line = &line))
    {
        const CliSpan trimmed = EMBED_CALL(cli_trim, EnvsCtx, .span = line);
        if (EMBED_CALL(cli_starts, EnvsCtx, .span = trimmed, .text = "["))
        {
            packages = EMBED_CALL(cli_is, EnvsCtx, .span = trimmed, .text = "[packages]");
            continue;
        }
        const char *const equals =
            packages ? memor.chr(&(MemoriaCfg){.src = trimmed.at, .val = '=', .bytes = trimmed.length}) : NULL;
        if (equals != NULL)
        {
            // the name before the equals sign, trimmed and with its quotes left off
            CliSpan name =
                EMBED_CALL(cli_trim, EnvsCtx, .span = (CliSpan){trimmed.at, (unsigned long long)(equals - trimmed.at)});
            const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                       .text = name.at, .at = 0ULL, .size = name.length, .set = "\"");
            name = (CliSpan){name.at + lead, name.length - lead};
            name.length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = name.at,
                                     .at = name.length, .set = "\"");
            EMBED_CALL(cli_add, EnvsCtx, .required = args->required, .name = name);
        }
    }
}

EMBED_INLINE void cli_requirements_read(const EnvsCtx *args)
{
    CliSpan line;
    unsigned long long at = 0ULL;
    while (EMBED_CALL(cli_next_line, EnvsCtx, .bytes = args->bytes, .size = args->size, .at = &at, .line = &line))
    {
        const CliSpan trimmed = EMBED_CALL(cli_trim, EnvsCtx, .span = line);
        if ((trimmed.length > 0ULL) && !EMBED_CALL(cli_starts, EnvsCtx, .span = trimmed, .text = "#") &&
            !EMBED_CALL(cli_starts, EnvsCtx, .span = trimmed, .text = "-"))
        {
            EMBED_CALL(cli_add, EnvsCtx, .required = args->required, .name = trimmed);
        }
    }
}

void cli_required_in(const CliEnvsCtx *args)
{
    args->required->count = 0u;
    args->required->used = 0u;
    static const char *const s_files[3] = {"setup.cfg", "Pipfile", "requirements.txt"};
    for (unsigned int file = 0u; file < 3u; file += 1u)
    {
        unsigned long long size = 0ULL;
        const unsigned char *const bytes = EMBED_CALL(cli_text_map, EnvsCtx, .folder = args->folder,
                                                      .name_text = s_files[file], .size_pointer = &size);
        if (bytes == NULL)
        {
            continue;
        }
        if (file == 0u)
        {
            EMBED_CALL(cli_setup_read, EnvsCtx, .required = args->required, .bytes = bytes, .size = size);
        }
        else if (file == 1u)
        {
            EMBED_CALL(cli_pipfile_read, EnvsCtx, .required = args->required, .bytes = bytes, .size = size);
        }
        else
        {
            EMBED_CALL(cli_requirements_read, EnvsCtx, .required = args->required, .bytes = bytes, .size = size);
        }
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_envs_cells);
    }
}

void cli_required_free(const CliEnvsCtx *args)
{
    CliRequired *required = args->required;
    g_cli_cells->persistent_buf_release(required->name, &g_cli_envs_cells);
    *required = (CliRequired){NULL, 0u, 0u, NULL, 0u, 0u};
}
