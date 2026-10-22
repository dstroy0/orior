// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_scan.c: what each kind of build script says of the programs it makes, read from its text: CMake's and
// Meson's executables and projects, the calls of a command in a script's code, and the comments taken out of it
// (executables_scan_more.c reads the rest)
#include "executables_internal.h"

CliCellsTally g_cli_executables_cells = CLI_CELLS_TALLY("cli_executables");

char *cli_uncommented_taken(const char *text, char quote)
{
    // the code with its comments taken out is no longer than the text
    const unsigned long long room = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1ull;
    char *const code = cli_cell_taken_by(&g_cli_executables_cells, room, "a script's code");
    if (code != NULL)
    {
        cli_uncommented(text, quote, code, room);
    }
    return code;
}

unsigned int cli_calls_taken(const char *code, const char *command, CliCall **calls)
{
    const unsigned int count = (code != NULL) ? cli_calls(code, command, NULL, 0u) : 0u;
    *calls = (count != 0u) ? cli_cell_taken_by(&g_cli_executables_cells, (unsigned long long)count * sizeof(CliCall),
                                               "the calls of a command")
                           : NULL;
    return (*calls != NULL) ? cli_calls(code, command, *calls, count) : 0u;
}

int cli_scan_white(char letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
}

// whether a letter is one of a word: a letter, a digit, `_`, or a byte of a letter past ASCII
static int cli_wordy(char letter)
{
    const unsigned char byte = (unsigned char)letter;
    return ((byte >= 'a') && (byte <= 'z')) || ((byte >= 'A') && (byte <= 'Z')) || ((byte >= '0') && (byte <= '9')) ||
           (byte == '_') || (byte >= 0x80u);
}

void cli_scan_trim(const char **text, unsigned long long *length)
{
    while ((*length > 0ull) && cli_scan_white((*text)[0]))
    {
        *text += 1;
        *length -= 1ull;
    }
    while ((*length > 0ull) && cli_scan_white((*text)[*length - 1ull]))
    {
        *length -= 1ull;
    }
}

void cli_scan_copied(const char *text, unsigned long long length, char *out, unsigned long long room)
{
    CliLine line = {out, room, 0ull};
    cli_line_counted(&line, text, length);
    cli_line_closed(&line);
}

void cli_names_put(CliNames *names, const char *name, unsigned long long length, const char *value)
{
    for (unsigned int at = 0u; at < names->count; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = names->name[at], .cap = CLI_CELLS_POOL}) == length) &&
            (memor.cmp(&(MemoriaCfg){.src = names->name[at], .other = name, .bytes = length}) == 0))
        {
            return;
        }
    }
    if (names->count < CLI_EXECUTABLES_NAMES)
    {
        cli_scan_copied(name, length, names->name[names->count], CLI_NAME_LONGEST);
        memor.cpy(&(MemoriaCfg){
            .dst = names->value[names->count],
            .src = (value != NULL) ? value : "",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = (value != NULL) ? value : "", .cap = CLI_CELLS_POOL}) + 1u});
        names->count += 1u;
    }
}

void cli_uncommented(const char *text, char quote, char *out, unsigned long long room)
{
    CliLine line = {out, room, 0ull};
    const char *at = text;
    while (at[0] != '\0')
    {
        int quoted = 0;
        const char *stop = at;
        while ((stop[0] != '\0') && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *const end = ((stop > at) && (stop[-1] == '\r')) ? (stop - 1) : stop;
        for (const char *letter = at; letter < end; letter += 1)
        {
            quoted = (letter[0] == quote) ? !quoted : quoted;
            if ((letter[0] == '#') && !quoted)
            {
                break;
            }
            cli_line_character(&line, letter[0]);
        }
        cli_line_character(&line, '\n');
        at = (stop[0] != '\0') ? (stop + 1) : stop;
    }
    cli_line_closed(&line);
}

// a letter in lower case, ASCII's
static char cli_lower(char letter)
{
    return ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter + ('a' - 'A')) : letter;
}

// whether the code at `code` starts with the first `length` letters of `command`, their case aside
static int cli_matched(const char *code, const char *command, unsigned long long length)
{
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        if ((code[at] == '\0') || (cli_lower(code[at]) != cli_lower(command[at])))
        {
            return 0;
        }
    }
    return 1;
}

// the length of the text inside the parentheses that open at `open`, to the one that closes them, or to the code's end
static unsigned long long cli_inside_length(const char *code, unsigned long long open)
{
    long long depth = 0ll;
    unsigned long long at = open;
    for (; code[at] != '\0'; at += 1ull)
    {
        depth += (code[at] == '(') ? 1ll : (code[at] == ')') ? -1ll : 0ll;
        if ((code[at] == ')') && (depth == 0ll))
        {
            return at - open - 1ull;
        }
    }
    return (at > open) ? (at - open - 1ull) : 0ull;
}

unsigned int cli_calls(const char *code, const char *command, CliCall *calls, unsigned int room)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = command, .cap = CLI_CELLS_POOL});
    unsigned int found = 0u;
    unsigned long long from = 0ull;
    while (code[from] != '\0')
    {
        // the command found from `from`, its case aside
        unsigned long long at = from;
        while ((code[at] != '\0') && !cli_matched(code + at, command, length))
        {
            at += 1ull;
        }
        if (code[at] == '\0')
        {
            break;
        }
        from = at + length;
        const int starts_word = (at == 0ull) || !cli_wordy(code[at - 1ull]);
        unsigned long long gap = 0ull;
        while (cli_scan_white(code[from + gap]))
        {
            gap += 1ull;
        }
        if (!starts_word || (code[from + gap] != '('))
        {
            continue;
        }
        const unsigned long long size = cli_inside_length(code, from + gap);
        if (calls == NULL)
        {
            found += 1u;
        }
        else if (found < room)
        {
            calls[found] = (CliCall){code + from + gap + 1ull, size};
            found += 1u;
        }
        from += gap + size;
    }
    return found;
}

// the `index`th word of a CMake call, its quotes taken off, into `out`, which holds CLI_NAME_LONGEST: 1, or 0 where it
// has no such word
static int cli_cmake_word(const CliCall *call, unsigned int index, char *out)
{
    const char *at = call->text;
    const char *const end = call->text + call->length;
    for (unsigned int word = 0u; at < end; word += 1u)
    {
        while ((at < end) && cli_scan_white(at[0]))
        {
            at += 1;
        }
        const char *stop = at;
        while ((stop < end) && !cli_scan_white(stop[0]))
        {
            stop += 1;
        }
        if (stop == at)
        {
            return 0;
        }
        if (word == index)
        {
            const char *from = at;
            while ((from < stop) && (from[0] == '"'))
            {
                from += 1;
            }
            const char *to = stop;
            while ((to > from) && (to[-1] == '"'))
            {
                to -= 1;
            }
            cli_scan_copied(from, (unsigned long long)(to - from), out, CLI_NAME_LONGEST);
            return 1;
        }
        at = stop;
    }
    return 0;
}

void cli_cmake_targets(const char *text, const char *project, CliNames *names)
{
    names->count = 0u;
    char *const code = cli_uncommented_taken(text, '"');
    CliCall *calls = NULL;
    const unsigned int count = cli_calls_taken(code, "add_executable", &calls);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char first[CLI_NAME_LONGEST];
        char second[CLI_NAME_LONGEST];
        if (cli_cmake_word(&calls[at], 1u, second) &&
            ((cellul.eq(&(CatenaFinitaCfg){.src = second, .other = "IMPORTED", .cap = CLI_CELLS_POOL})) ||
             (cellul.eq(&(CatenaFinitaCfg){.src = second, .other = "ALIAS", .cap = CLI_CELLS_POOL}))))
        {
            continue;
        }
        if (!cli_cmake_word(&calls[at], 0u, first))
        {
            continue;
        }
        char name[CLI_NAME_LONGEST];
        CliLine line = {name, sizeof(name), 0ull};
        for (const char *letter = first; letter[0] != '\0';)
        {
            if (cellul.starts(&(CatenaFinitaCfg){.src = letter, .other = "${PROJECT_NAME}", .cap = CLI_CELLS_POOL}))
            {
                cli_line_text(&line, project);
                letter += 15;
            }
            else
            {
                cli_line_character(&line, letter[0]);
                letter += 1;
            }
        }
        const unsigned long long length = cli_line_closed(&line);
        if ((length != 0ull) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = name, .cap = CLI_CELLS_POOL, .other = "${", .other_cap = CLI_CELLS_POOL}) == NULL))
        {
            cli_names_put(names, name, length, NULL);
        }
    }
    cli_cell_given_by(&g_cli_executables_cells, calls);
    cli_cell_given_by(&g_cli_executables_cells, code);
}

int cli_cmake_project(const char *text, char *out)
{
    char *const code = cli_uncommented_taken(text, '"');
    CliCall *calls = NULL;
    const unsigned int count = cli_calls_taken(code, "project", &calls);
    int found = 0;
    for (unsigned int at = 0u; !found && (at < count); at += 1u)
    {
        found = cli_cmake_word(&calls[at], 0u, out);
    }
    cli_cell_given_by(&g_cli_executables_cells, calls);
    cli_cell_given_by(&g_cli_executables_cells, code);
    return found;
}

// a Meson call's first argument, where it is a string written out, into `out`, which holds CLI_NAME_LONGEST: 1, or 0
static int cli_meson_first(const CliCall *call, char *out)
{
    const char *first = call->text;
    unsigned long long length = 0ull;
    while ((length < call->length) && (first[length] != ','))
    {
        length += 1ull;
    }
    cli_scan_trim(&first, &length);
    if ((length < 3ull) || (first[0] != '\'') || (first[length - 1ull] != '\''))
    {
        return 0;
    }
    cli_scan_copied(first + 1, length - 2ull, out, CLI_NAME_LONGEST);
    return 1;
}

void cli_meson_targets(const char *text, CliNames *names)
{
    names->count = 0u;
    char *const code = cli_uncommented_taken(text, '\'');
    CliCall *calls = NULL;
    const unsigned int count = cli_calls_taken(code, "executable", &calls);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char name[CLI_NAME_LONGEST];
        if (cli_meson_first(&calls[at], name))
        {
            cli_names_put(names, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), NULL);
        }
    }
    cli_cell_given_by(&g_cli_executables_cells, calls);
    cli_cell_given_by(&g_cli_executables_cells, code);
}

int cli_meson_project(const char *text, char *out)
{
    char *const code = cli_uncommented_taken(text, '\'');
    CliCall *calls = NULL;
    const unsigned int count = cli_calls_taken(code, "project", &calls);
    int found = 0;
    for (unsigned int at = 0u; !found && (at < count); at += 1u)
    {
        found = cli_meson_first(&calls[at], out);
    }
    cli_cell_given_by(&g_cli_executables_cells, calls);
    cli_cell_given_by(&g_cli_executables_cells, code);
    return found;
}
