// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs.c: the packages the files of a folder require, read from its `setup.cfg`, Pipfile and `requirements.txt`, by
// name, each once
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "envs_internal.h"

CliCellsTally g_cli_envs_cells = CLI_CELLS_TALLY("cli_envs");

void *cli_envs_moved(void *cell, unsigned long long used, unsigned long long room, const char *what)
{
    void *const moved = cli_cell_taken_by(&g_cli_envs_cells, room, what);
    if (moved == NULL)
    {
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = moved, .src = cell, .bytes = (size_t)used});
    cli_cell_given_by(&g_cli_envs_cells, cell);
    return moved;
}

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

// room made in `required` for one name more of `length` bytes, the text's room doubled to hold it: 1, or 0 where a
// larger cell is refused
static int cli_required_room(CliRequired *required, unsigned long long length)
{
    if (required->count == required->room)
    {
        const unsigned int doubled = (required->room == 0u) ? CLI_ENVS_NAMES_FIRST : (required->room * 2u);
        const unsigned int room = (doubled < CLI_ENVS_NAMES) ? doubled : CLI_ENVS_NAMES;
        CliRequiredName *const moved =
            cli_envs_moved(required->name, (unsigned long long)required->count * sizeof(CliRequiredName),
                           (unsigned long long)room * sizeof(CliRequiredName), "the names a folder requires");
        if (moved == NULL)
        {
            return 0;
        }
        required->name = moved;
        required->room = room;
    }
    unsigned long long text_room = required->text_room;
    while ((required->used + length) > text_room)
    {
        text_room = (text_room == 0ull) ? CLI_ENVS_TEXT_FIRST : (text_room * 2ull);
    }
    text_room = (text_room < CLI_ENVS_TEXT) ? text_room : CLI_ENVS_TEXT;
    if (text_room != required->text_room)
    {
        char *const moved = cli_envs_moved(required->text, required->used, text_room, "the names' text");
        if (moved == NULL)
        {
            return 0;
        }
        required->text = moved;
        // a text's room is CLI_ENVS_TEXT at the most, which an unsigned int holds
        required->text_room = (unsigned int)text_room;
    }
    return 1;
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
    if (!cli_required_room(required, length))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = required->text + required->used, .src = name.at, .bytes = length});
    required->name[required->count].start = required->used;
    // a name is below CLI_ENVS_TEXT bytes, which an unsigned int holds
    required->name[required->count].length = (unsigned int)length;
    required->used += (unsigned int)length;
    required->count += 1u;
}

// the file `name` in `folder` mapped, where it is UTF-8 whole: its bytes, or NULL
static const unsigned char *cli_text_mapped(const char *folder, const char *name, unsigned long long *size)
{
    char path[CLI_PATH_LONGEST];
    cli_full(folder, name, path);
    return cli_file_held_utf8(path, size);
}

// the next line of `size` bytes from `*at`, `*at` moved past it: 1, or 0 where the text has ended
static int cli_next_line(const unsigned char *bytes, unsigned long long size, unsigned long long *at, CliSpan *line)
{
    if (*at >= size)
    {
        return 0;
    }
    const unsigned char *const newline = memor.chr(&(MemoriaCfg){.src = bytes + *at, .val = '\n', .bytes = size - *at});
    // a newline found lies in the bytes, at or past `*at`
    const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : size;
    *line = (CliSpan){(const char *)bytes + *at, end - *at};
    if ((newline != NULL) && (line->length > 0ull) && (line->at[line->length - 1ull] == '\r'))
    {
        line->length -= 1ull;
    }
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
            while ((name.length > 0ull) && (name.at[0] == '"'))
            {
                name = (CliSpan){name.at + 1, name.length - 1ull};
            }
            while ((name.length > 0ull) && (name.at[name.length - 1ull] == '"'))
            {
                name.length -= 1ull;
            }
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
        cli_file_let_go(bytes, size);
    }
}

void cli_required_written(const CliRequired *required, CliLine *out)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < required->count; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, required->text + required->name[at].start, required->name[at].length);
    }
    cli_line_character(out, ']');
}

void cli_required_given(CliRequired *required)
{
    cli_cell_given_by(&g_cli_envs_cells, required->name);
    cli_cell_given_by(&g_cli_envs_cells, required->text);
    *required = (CliRequired){NULL, 0u, 0u, NULL, 0u, 0u};
}

void cli_envs_requires_in(const char *folder, CliLine *out)
{
    CliRequired required = {NULL, 0u, 0u, NULL, 0u, 0u};
    cli_required_in(folder, &required);
    cli_required_written(&required, out);
    cli_required_given(&required);
}
