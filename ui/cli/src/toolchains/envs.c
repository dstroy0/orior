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

// the span with the whitespace at either end left off
static CliSpan cli_trimmed(CliSpan span)
{
    unsigned long long start = span.length;
    unsigned long long end = 0ull;
    unsigned long long at = 0ull;
    while (at < span.length)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter((const unsigned char *)span.at, span.length, at, &letter);
        if (!cli_fuzzy_white(letter))
        {
            start = (start == span.length) ? at : start;
            end = at + step;
        }
        at += step;
    }
    return (start < end) ? (CliSpan){span.at + start, end - start} : (CliSpan){span.at, 0ull};
}

// whether the span starts with `text`, and whether it is `text`
static int cli_starts(CliSpan span, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    return (span.length >= length) && (memor.cmp(&(MemoriaCfg){.src = span.at, .other = text, .bytes = length}) == 0);
}

static int cli_is(CliSpan span, const char *text)
{
    return (span.length == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})) &&
           cli_starts(span, text);
}

void cli_required_taken(CliRequired *required)
{
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
static void cli_added(CliRequired *required, CliSpan name)
{
    unsigned long long length = 0ull;
    while (length < name.length)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter((const unsigned char *)name.at, name.length, length, &letter);
        if (!cli_hit_wordy(letter) && (letter != '-') && (letter != '.'))
        {
            break;
        }
        length += step;
    }
    if ((length == 0ull) || (required->count >= CLI_ENVS_NAMES) || ((required->used + length) > CLI_ENVS_TEXT))
    {
        return;
    }
    for (unsigned int at = 0u; at < required->count; at += 1u)
    {
        if (required->name[at].length != length)
        {
            continue;
        }
        unsigned long long same = 0ull;
        while (same < length)
        {
            const char one = required->text[required->name[at].start + same];
            const char other = name.at[same];
            const char one_lower = ((one >= 'A') && (one <= 'Z')) ? (char)(one + 32) : one;
            const char other_lower = ((other >= 'A') && (other <= 'Z')) ? (char)(other + 32) : other;
            if (one_lower != other_lower)
            {
                break;
            }
            same += 1ull;
        }
        if (same == length)
        {
            return;
        }
    }
    if ((required->count >= required->room) || ((required->used + length) > required->text_room))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = required->text + required->used, .src = name.at, .bytes = length});
    required->name[required->count].start = required->used;
    // a name is below CLI_ENVS_TEXT bytes, which an unsigned int represents
    required->name[required->count].length = (unsigned int)length;
    required->used += (unsigned int)length;
    required->count += 1u;
}

// the file `name` in `folder` mapped, where it is UTF-8 whole: its bytes, or NULL
static const unsigned char *cli_text_mapped(const char *folder, const char *name, unsigned long long *size)
{
    char path[CLI_PATH_MAX_LEN];
    cli_full(folder, name, path);
    return cli_file_taken_utf8(path, size, &g_cli_envs_cells);
}

// the next line of `size` bytes from `*at`, `*at` moved past it: 1, or 0 where the text has ended
static int cli_next_line(const unsigned char *bytes, unsigned long long size, unsigned long long *at, CliSpan *line)
{
    if (*at >= size)
    {
        return 0;
    }
    const CliTextLine bounds = cli_text_line_at((const char *)bytes, *at, size);
    const unsigned long long end = bounds.stop;
    *line = (CliSpan){(const char *)bytes + *at, end - *at};
    line->length -= bounds.stop - bounds.cut;
    *at = end + 1ull;
    return 1;
}

// the names `setup.cfg` requires under `[options]`, `install_requires`, those on its line and those indented below it
static void cli_setup_read(CliRequired *required, const unsigned char *bytes, unsigned long long size)
{
    CliSpan line;
    unsigned long long at = 0ull;
    int options = 0;
    int taking = 0;
    while (cli_next_line(bytes, size, &at, &line))
    {
        const CliSpan trimmed = cli_trimmed(line);
        if (cli_starts(trimmed, "["))
        {
            options = cli_is(trimmed, "[options]");
            taking = 0;
            continue;
        }
        if (!options || cli_starts(trimmed, "#") || (trimmed.length == 0ull))
        {
            continue;
        }
        if (cli_starts(trimmed, "install_requires"))
        {
            taking = 1;
            CliSpan rest = {trimmed.at + 16, trimmed.length - 16ull};
            while ((rest.length > 0ull) && cli_fuzzy_white((unsigned char)rest.at[0]))
            {
                rest = (CliSpan){rest.at + 1, rest.length - 1ull};
            }
            if (!cli_starts(rest, "="))
            {
                continue;
            }
            unsigned long long from = 1ull;
            for (unsigned long long place = 1ull; place <= rest.length; place += 1ull)
            {
                if ((place == rest.length) || (rest.at[place] == ',') || (rest.at[place] == ';'))
                {
                    cli_added(required, (CliSpan){rest.at + from, place - from});
                    from = place + 1ull;
                }
            }
        }
        else if (taking && (line.length > 0ull) && ((line.at[0] == ' ') || (line.at[0] == '\t')))
        {
            cli_added(required, trimmed);
        }
        else
        {
            taking = 0;
        }
    }
}

// the names a Pipfile requires under `[packages]`, and those of `requirements.txt`, each a line not a comment or an
// option
static void cli_pipfile_read(CliRequired *required, const unsigned char *bytes, unsigned long long size)
{
    CliSpan line;
    unsigned long long at = 0ull;
    int packages = 0;
    while (cli_next_line(bytes, size, &at, &line))
    {
        const CliSpan trimmed = cli_trimmed(line);
        if (cli_starts(trimmed, "["))
        {
            packages = cli_is(trimmed, "[packages]");
            continue;
        }
        const char *const equals =
            packages ? memor.chr(&(MemoriaCfg){.src = trimmed.at, .val = '=', .bytes = trimmed.length}) : NULL;
        if (equals != NULL)
        {
            // the name before the equals sign, trimmed and with its quotes left off
            CliSpan name = cli_trimmed((CliSpan){trimmed.at, (unsigned long long)(equals - trimmed.at)});
            const unsigned long long lead = cli_text_past_any(name.at, 0ull, name.length, "\"");
            name = (CliSpan){name.at + lead, name.length - lead};
            name.length = cli_text_back_past(name.at, name.length, "\"");
            cli_added(required, name);
        }
    }
}

static void cli_requirements_read(CliRequired *required, const unsigned char *bytes, unsigned long long size)
{
    CliSpan line;
    unsigned long long at = 0ull;
    while (cli_next_line(bytes, size, &at, &line))
    {
        const CliSpan trimmed = cli_trimmed(line);
        if ((trimmed.length > 0ull) && !cli_starts(trimmed, "#") && !cli_starts(trimmed, "-"))
        {
            cli_added(required, trimmed);
        }
    }
}

void cli_required_in(const char *folder, CliRequired *required)
{
    required->count = 0u;
    required->used = 0u;
    static const char *const s_files[3] = {"setup.cfg", "Pipfile", "requirements.txt"};
    for (unsigned int file = 0u; file < 3u; file += 1u)
    {
        unsigned long long size = 0ull;
        const unsigned char *const bytes = cli_text_mapped(folder, s_files[file], &size);
        if (bytes == NULL)
        {
            continue;
        }
        if (file == 0u)
        {
            cli_setup_read(required, bytes, size);
        }
        else if (file == 1u)
        {
            cli_pipfile_read(required, bytes, size);
        }
        else
        {
            cli_requirements_read(required, bytes, size);
        }
        cli_file_given(bytes, &g_cli_envs_cells);
    }
}

void cli_required_written(const CliRequired *required, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < required->count; at += 1u)
    {
        mmgr_line_put(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, required->text + required->name[at].start, required->name[at].length);
    }
    mmgr_line_ch(out, ']');
}

void cli_required_given(CliRequired *required)
{
    g_cli_cells->persistent_buf_release(required->name, &g_cli_envs_cells);
    *required = (CliRequired){NULL, 0u, 0u, NULL, 0u, 0u};
}

void cli_envs_requires_in(const char *folder, VerbaLinea *out)
{
    CliRequired required;
    cli_required_taken(&required);
    cli_required_in(folder, &required);
    cli_required_written(&required, out);
    cli_required_given(&required);
}
