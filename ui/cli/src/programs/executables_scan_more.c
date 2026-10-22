// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_scan_more.c: a Go file's package, a .NET project's program, the string a call's keyword is set to, a
// build.zig's executables, the program a PyInstaller spec freezes, and a package.json's bins
#include "../text/json_tree.h"
#include "../text/text.h"
#include "executables_internal.h"

int cli_go_package(const char *text, char *out)
{
    int in_comment = 0;
    const char *line = text;
    while (line[0] != '\0')
    {
        const char *stop = line;
        while ((stop[0] != '\0') && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *const next = (stop[0] != '\0') ? (stop + 1) : stop;
        const char *from = line;
        unsigned long long length = (unsigned long long)(stop - line);
        cli_scan_trim(&from, &length);
        line = next;
        // whether the line closes a comment it is in
        int closes = 0;
        for (unsigned long long at = 0ull; !closes && ((at + 1ull) < length); at += 1ull)
        {
            closes = (from[at] == '*') && (from[at + 1ull] == '/');
        }
        if (in_comment)
        {
            in_comment = !closes;
            continue;
        }
        if ((length == 0ull) || ((length >= 2ull) && (from[0] == '/') && (from[1] == '/')))
        {
            continue;
        }
        if ((length >= 2ull) && (from[0] == '/') && (from[1] == '*'))
        {
            in_comment = !closes;
            continue;
        }
        if ((length < 8ull) || (memor.cmp(&(MemoriaCfg){.src = from, .other = "package ", .bytes = 8u}) != 0))
        {
            return 0;
        }
        const char *word = from + 8;
        const char *const end = from + length;
        while ((word < end) && cli_scan_white(word[0]))
        {
            word += 1;
        }
        const char *word_end = word;
        while ((word_end < end) && !cli_scan_white(word_end[0]))
        {
            word_end += 1;
        }
        cli_scan_copied(word, (unsigned long long)(word_end - word), out, CLI_NAME_LONGEST);
        return 1;
    }
    return 0;
}

// the text of the first element `tag` holds in an MSBuild project, trimmed: its start, its length into `length`, or
// NULL where it has none
static const char *cli_msbuild_value(const char *text, const char *tag, unsigned long long *length)
{
    char open[CLI_NAME_LONGEST];
    char close[CLI_NAME_LONGEST];
    CliLine opened = {open, sizeof(open), 0ull};
    CliLine closed = {close, sizeof(close), 0ull};
    cli_line_character(&opened, '<');
    cli_line_text(&opened, tag);
    cli_line_character(&opened, '>');
    cli_line_text(&closed, "</");
    cli_line_text(&closed, tag);
    cli_line_character(&closed, '>');
    cli_line_closed(&opened);
    cli_line_closed(&closed);
    const char *const found =
        cellul.find(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL, .other = open, .other_cap = CLI_CELLS_POOL});
    if (found == NULL)
    {
        return NULL;
    }
    const char *start = found + cellul.len(&(CatenaFinitaCfg){.src = open, .cap = CLI_CELLS_POOL});
    const char *const end = cellul.find(
        &(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL, .other = close, .other_cap = CLI_CELLS_POOL});
    if (end == NULL)
    {
        return NULL;
    }
    *length = (unsigned long long)(end - start);
    cli_scan_trim(&start, length);
    return start;
}

// whether `length` bytes of `text` are `word`, ASCII's case aside
static int cli_same(const char *text, unsigned long long length, const char *word)
{
    if (length != cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL}))
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const char letter = ((text[at] >= 'A') && (text[at] <= 'Z')) ? (char)(text[at] + ('a' - 'A')) : text[at];
        if (letter != word[at])
        {
            return 0;
        }
    }
    return 1;
}

int cli_dotnet_program(const char *project, const char *text, char *out)
{
    unsigned long long length = 0ull;
    const char *const output = cli_msbuild_value(text, "OutputType", &length);
    if ((output == NULL) || (!cli_same(output, length, "exe") && !cli_same(output, length, "winexe")))
    {
        return 0;
    }
    unsigned long long name_length = 0ull;
    const char *const name = cli_msbuild_value(text, "AssemblyName", &name_length);
    int named = (name != NULL) && (name_length != 0ull);
    for (unsigned long long at = 0ull; named && ((at + 1ull) < name_length); at += 1ull)
    {
        named = !((name[at] == '$') && (name[at + 1ull] == '('));
    }
    if (named)
    {
        cli_scan_copied(name, name_length, out, CLI_NAME_LONGEST);
        return 1;
    }
    // the project file's name without its extension
    const char *stem = project;
    for (const char *at = project; at[0] != '\0'; at += 1)
    {
        stem = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : stem;
    }
    const char *dot = NULL;
    for (const char *at = stem + 1; (stem[0] != '\0') && (at[0] != '\0'); at += 1)
    {
        dot = (at[0] == '.') ? at : dot;
    }
    cli_scan_copied(stem,
                    (dot != NULL) ? (unsigned long long)(dot - stem)
                                  : cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_CELLS_POOL}),
                    out, CLI_NAME_LONGEST);
    return stem[0] != '\0';
}

int cli_keyword(const char *call, unsigned long long length, const char *key, char *out)
{
    const unsigned long long key_length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
    unsigned long long from = 0ull;
    while ((from + key_length) <= length)
    {
        unsigned long long at = from;
        while (((at + key_length) <= length) &&
               (memor.cmp(&(MemoriaCfg){.src = call + at, .other = key, .bytes = key_length}) != 0))
        {
            at += 1ull;
        }
        if ((at + key_length) > length)
        {
            return 0;
        }
        from = at + key_length;
        const char before = (at > 0ull) ? call[at - 1ull] : ' ';
        if (((before >= 'a') && (before <= 'z')) || ((before >= 'A') && (before <= 'Z')) ||
            ((before >= '0') && (before <= '9')) || (before == '_') || ((unsigned char)before >= 0x80u))
        {
            continue;
        }
        unsigned long long rest = from;
        while ((rest < length) && cli_scan_white(call[rest]))
        {
            rest += 1ull;
        }
        if ((rest >= length) || (call[rest] != '='))
        {
            continue;
        }
        rest += 1ull;
        while ((rest < length) && cli_scan_white(call[rest]))
        {
            rest += 1ull;
        }
        if ((rest >= length) || ((call[rest] != '"') && (call[rest] != '\'')))
        {
            return 0;
        }
        const char quote = call[rest];
        unsigned long long end = rest + 1ull;
        while ((end < length) && (call[end] != quote))
        {
            end += 1ull;
        }
        cli_scan_copied(call + rest + 1ull, end - rest - 1ull, out, CLI_NAME_LONGEST);
        return 1;
    }
    return 0;
}

void cli_zig_exes(const char *text, CliNames *names)
{
    names->count = 0u;
    // each line up to its `//`, the lines joined by a newline, no longer than the text
    const unsigned long long room = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1ull;
    char *const kept = cli_cell_taken_by(&g_cli_executables_cells, room, "a build.zig's code");
    if (kept == NULL)
    {
        return;
    }
    CliLine code = {kept, room, 0ull};
    const char *line = text;
    for (int first = 1; line[0] != '\0'; first = 0)
    {
        const char *stop = line;
        while ((stop[0] != '\0') && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *end = ((stop > line) && (stop[-1] == '\r')) ? (stop - 1) : stop;
        for (const char *at = line; (at + 1) < end; at += 1)
        {
            if ((at[0] == '/') && (at[1] == '/'))
            {
                end = at;
                break;
            }
        }
        cli_line_text(&code, first ? "" : "\n");
        cli_line_counted(&code, line, (unsigned long long)(end - line));
        line = (stop[0] != '\0') ? (stop + 1) : stop;
    }
    cli_line_closed(&code);
    CliCall *calls = NULL;
    const unsigned int count = cli_calls_taken(kept, "addExecutable", &calls);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *call = calls[at].text;
        unsigned long long length = calls[at].length;
        while ((length > 0ull) && cli_scan_white(call[0]))
        {
            call += 1;
            length -= 1ull;
        }
        char name[CLI_NAME_LONGEST];
        name[0] = '\0';
        if ((length > 0ull) && (call[0] == '"'))
        {
            unsigned long long end = 1ull;
            while ((end < length) && (call[end] != '"'))
            {
                end += 1ull;
            }
            cli_scan_copied(call + 1, end - 1ull, name, sizeof(name));
        }
        else if (!cli_keyword(call, length, ".name", name))
        {
            name[0] = '\0';
        }
        if (name[0] != '\0')
        {
            cli_names_put(names, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), NULL);
        }
    }
    cli_cell_given_by(&g_cli_executables_cells, calls);
    cli_cell_given_by(&g_cli_executables_cells, kept);
}

int cli_spec_made(const char *text, char *exe, char *path)
{
    char *const code = cli_uncommented_taken(text, '\'');
    if (code == NULL)
    {
        return 0;
    }
    int found = cli_calls(code, "Analysis", NULL, 0u) != 0u;
    int named = 0;
    CliCall *calls = NULL;
    const unsigned int exes = found ? cli_calls_taken(code, "EXE", &calls) : 0u;
    for (unsigned int at = 0u; !named && (at < exes); at += 1u)
    {
        named = cli_keyword(calls[at].text, calls[at].length, "name", exe);
    }
    cli_cell_given_by(&g_cli_executables_cells, calls);
    calls = NULL;
    found = found && named;
    char folder[CLI_NAME_LONGEST];
    int collected = 0;
    const unsigned int collects = found ? cli_calls_taken(code, "COLLECT", &calls) : 0u;
    for (unsigned int at = 0u; !collected && (at < collects); at += 1u)
    {
        collected = cli_keyword(calls[at].text, calls[at].length, "name", folder);
    }
    cli_cell_given_by(&g_cli_executables_cells, calls);
    cli_cell_given_by(&g_cli_executables_cells, code);
    if (!found)
    {
        return 0;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = exe, .cap = CLI_CELLS_POOL});
    if ((length >= 4ull) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = exe + length - 4ull, .other = ".exe", .cap = CLI_CELLS_POOL})))
    {
        exe[length - 4ull] = '\0';
    }
    char file[CLI_NAME_LONGEST];
    cli_program_file(exe, file);
    CliLine line = {path, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, "dist/");
    if (collected)
    {
        cli_line_text(&line, folder);
        cli_line_character(&line, '/');
    }
    cli_line_text(&line, file);
    cli_line_closed(&line);
    return 1;
}

void cli_npm_bins(const char *text, CliNames *names, int *module)
{
    names->count = 0u;
    *module = 0;
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    char why[CLI_NAME_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int package = cli_json_taken(
        &json, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), 0u, 0ull, &said);
    if ((package == CLI_JSON_NONE) || !cli_json_is(&json, package, CFG_JSON_OBJECT))
    {
        cli_json_given(&json);
        return;
    }
    const char *const kind = cli_json_string(&json, cli_json_member(&json, package, "type"), NULL);
    *module = (kind != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "module", .cap = CLI_CELLS_POOL}));
    const char *full = cli_json_string(&json, cli_json_member(&json, package, "name"), NULL);
    full = (full != NULL) ? full : "";
    const char *const name =
        (cli_text_last(full, cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}), '/') != NULL)
            ? (cli_text_last(full, cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}), '/') + 1)
            : full;
    const unsigned int bin = cli_json_member(&json, package, "bin");
    const char *const entry = cli_json_string(&json, bin, NULL);
    if ((entry != NULL) && (name[0] != '\0'))
    {
        cli_names_put(names, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), entry);
    }
    else if (cli_json_is(&json, bin, CFG_JSON_OBJECT))
    {
        for (unsigned int member = json.node[bin].first; member != CLI_JSON_NONE; member = json.node[member].next)
        {
            const char *const script = cli_json_string(&json, member, NULL);
            if (script != NULL)
            {
                const char *const key = json.text + json.node[member].key;
                cli_names_put(names, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}), script);
            }
        }
    }
    cli_json_given(&json);
}
