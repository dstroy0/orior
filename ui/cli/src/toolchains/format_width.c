// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// format_width.c: the width a formatter keeps a file's lines to, read from the project's settings for it, and the
// nearest settings file read
#include "format_internal.h"

// A span of a text: where it starts and its bytes
typedef struct
{
    const char *at;
    unsigned long long length;
} CliSpan;

// whether a byte is ASCII's whitespace
static int cli_blank(char letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
}

// the span with the whitespace at either end left off
static CliSpan cli_trimmed(CliSpan span)
{
    while ((span.length > 0ull) && cli_blank(span.at[0]))
    {
        span = (CliSpan){span.at + 1, span.length - 1ull};
    }
    while ((span.length > 0ull) && cli_blank(span.at[span.length - 1ull]))
    {
        span.length -= 1ull;
    }
    return span;
}

int cli_format_utf8(const char *bytes, unsigned long long size)
{
    for (unsigned long long at = 0ull; at < size;)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter((const unsigned char *)bytes, size, at, &letter);
        if ((step == 1ull) && ((unsigned char)bytes[at] >= 0x80u))
        {
            return 0;
        }
        at += step;
    }
    return 1;
}

void cli_format_failed(const char *program, const char *path, const CliRan *ran, CliLine *said)
{
    const char *name = path;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    cli_line_text(said, program);
    cli_line_text(said, " could not format ");
    cli_line_text(said, (name[0] != '\0') ? name : path);
    cli_line_text(said, ": ");
    const CliSpan whole = cli_trimmed((CliSpan){ran->err, ran->err_size});
    if (whole.length == 0ull)
    {
        cli_line_text(said, "it said nothing");
        return;
    }
    unsigned long long at = 0ull;
    for (unsigned int line = 0u; (line < 6u) && (at < whole.length); line += 1u)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = whole.at + at, .val = '\n', .bytes = whole.length - at});
        // a newline found lies in the text, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - whole.at) : whole.length;
        unsigned long long length = end - at;
        length -= ((newline != NULL) && (length > 0ull) && (whole.at[end - 1ull] == '\r')) ? 1ull : 0ull;
        if (line != 0u)
        {
            cli_line_character(said, '\n');
        }
        cli_lossy(said, (const unsigned char *)whole.at + at, length);
        at = end + 1ull;
    }
}

int cli_format_nearest(const char *path, const char *const *names, unsigned int count)
{
    char folder[CLI_PATH_LONGEST];
    char up[CLI_PATH_LONGEST];
    if (!cli_path_parent(path, folder))
    {
        return 0;
    }
    for (;;)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            char file[CLI_PATH_LONGEST];
            cli_full(folder, names[at], file);
            unsigned long long size = 0ull;
            const unsigned char *const bytes = cli_file_held(file, &size);
            if (bytes == NULL)
            {
                continue;
            }
            // the settings kept in a cell sized to the file, ended by a 0, the ones kept before given back
            cli_cell_given(g_cli_format.settings);
            g_cli_format.settings = cli_format_utf8((const char *)bytes, size)
                                        ? cli_cell_taken(size + 1ull, "a formatter's settings")
                                        : NULL;
            const int text = g_cli_format.settings != NULL;
            if (text)
            {
                memor.cpy(&(MemoriaCfg){.dst = g_cli_format.settings, .src = bytes, .bytes = size});
                g_cli_format.settings[size] = '\0';
            }
            g_cli_format.settings_size = text ? size : 0ull;
            cli_file_let_go(bytes, size);
            if (text)
            {
                return 1;
            }
        }
        if (!cli_path_parent(folder, up) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = up, .other = folder, .cap = CLI_CELLS_POOL})))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){
            .dst = folder, .src = up, .bytes = cellul.len(&(CatenaFinitaCfg){.src = up, .cap = CLI_CELLS_POOL}) + 1u});
    }
}

// the whole number set for `key` in a part of a line, `key = 88`, `key: 88` or `"key": 88`: 1 with it into
// `number`, or 0
static int cli_number_after(CliSpan part, const char *key, unsigned int *number)
{
    CliSpan rest = cli_trimmed(part);
    while ((rest.length > 0ull) && ((rest.at[0] == '"') || (rest.at[0] == '\'')))
    {
        rest = (CliSpan){rest.at + 1, rest.length - 1ull};
    }
    const unsigned long long key_length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
    if ((rest.length < key_length) ||
        (memor.cmp(&(MemoriaCfg){.src = rest.at, .other = key, .bytes = key_length}) != 0))
    {
        return 0;
    }
    rest = (CliSpan){rest.at + key_length, rest.length - key_length};
    while ((rest.length > 0ull) && ((rest.at[0] == '"') || (rest.at[0] == '\'')))
    {
        rest = (CliSpan){rest.at + 1, rest.length - 1ull};
    }
    rest = cli_trimmed(rest);
    if ((rest.length == 0ull) || ((rest.at[0] != '=') && (rest.at[0] != ':')))
    {
        return 0;
    }
    rest = cli_trimmed((CliSpan){rest.at + 1, rest.length - 1ull});
    unsigned long long value = 0ull;
    unsigned long long at = 0ull;
    while ((at < rest.length) && (rest.at[at] >= '0') && (rest.at[at] <= '9'))
    {
        value = (value * 10ull) + (unsigned long long)(rest.at[at] - '0');
        if (value > 0xffffffffull)
        {
            return 0;
        }
        at += 1ull;
    }
    // a value checked to fit 32 bits
    *number = (unsigned int)value;
    return at > 0ull;
}

int cli_format_number_in(const char *text, const char *table, const char *key, unsigned int *number)
{
    int inside = table == NULL;
    for (const char *line = text; line[0] != '\0';)
    {
        const char *end = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '\n'});
        end = (end != NULL) ? end : (line + cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}));
        const char *const hash =
            memor.chr(&(MemoriaCfg){.src = line, .val = '#', .bytes = (unsigned long long)(end - line)});
        const CliSpan code = cli_trimmed((CliSpan){line, (unsigned long long)(((hash != NULL) ? hash : end) - line)});
        line = (end[0] == '\n') ? (end + 1) : end;
        if ((code.length > 0ull) && (code.at[0] == '[') && (code.at[code.length - 1ull] == ']'))
        {
            CliSpan name = code;
            while ((name.length > 0ull) && ((name.at[0] == '[') || (name.at[0] == ']')))
            {
                name = (CliSpan){name.at + 1, name.length - 1ull};
            }
            while ((name.length > 0ull) &&
                   ((name.at[name.length - 1ull] == '[') || (name.at[name.length - 1ull] == ']')))
            {
                name.length -= 1ull;
            }
            name = cli_trimmed(name);
            inside = (table != NULL) &&
                     (name.length == cellul.len(&(CatenaFinitaCfg){.src = table, .cap = CLI_CELLS_POOL})) &&
                     (memor.cmp(&(MemoriaCfg){.src = name.at, .other = table, .bytes = name.length}) == 0);
            continue;
        }
        unsigned long long from = 0ull;
        for (unsigned long long at = 0ull; inside && (at <= code.length); at += 1ull)
        {
            if ((at < code.length) && (code.at[at] != ',') && (code.at[at] != '{') && (code.at[at] != '}'))
            {
                continue;
            }
            if (cli_number_after((CliSpan){code.at + from, at - from}, key, number))
            {
                return 1;
            }
            from = at + 1ull;
        }
    }
    return 0;
}

// whether Prettier's settings set `proseWrap` to `always`
static int cli_wraps_prose(const char *text)
{
    const char *part = text;
    for (const char *at = text;; at += 1)
    {
        if ((at[0] == '\n') || (at[0] == ',') || (at[0] == '{') || (at[0] == '}') || (at[0] == '\0'))
        {
            char piece[CLI_NAME_LONGEST * 4u];
            // the part lies before `at`, its end at or past its start
            const unsigned long long length = (unsigned long long)(at - part);
            if (length < sizeof(piece))
            {
                memor.cpy(&(MemoriaCfg){.dst = piece, .src = part, .bytes = length});
                piece[length] = '\0';
                if ((cellul.find(&(CatenaFinitaCfg){
                         .src = piece, .cap = CLI_CELLS_POOL, .other = "proseWrap", .other_cap = CLI_CELLS_POOL}) !=
                     NULL) &&
                    (cellul.find(&(CatenaFinitaCfg){
                         .src = piece, .cap = CLI_CELLS_POOL, .other = "always", .other_cap = CLI_CELLS_POOL}) != NULL))
                {
                    return 1;
                }
            }
            if (at[0] == '\0')
            {
                return 0;
            }
            part = at + 1;
        }
    }
}

int cli_format_margin(const char *path, const char *language, unsigned int *width, char *by)
{
    static const char *const s_black[1] = {"pyproject.toml"};
    static const char *const s_rustfmt[2] = {"rustfmt.toml", ".rustfmt.toml"};
    static const char *const s_clang[2] = {".clang-format", "_clang-format"};
    static const char *const s_prettier[4] = {".prettierrc", ".prettierrc.json", ".prettierrc.yaml", ".prettierrc.yml"};
    cli_lock_take(&g_cli_format_lock);
    char why[CLI_NAME_LONGEST * 4u];
    CliLine unknown = {why, sizeof(why), 0ull};
    const unsigned int tool = cli_format_tool(language, &unknown);
    const CliJson *const json = &g_cli_format.json;
    const char *const program =
        (tool != CLI_JSON_NONE)
            ? cli_json_string(json, cli_json_member(json, cli_json_member(json, tool, "format"), "program"), NULL)
            : NULL;
    const char *const *names = NULL;
    unsigned int count = 0u;
    const char *table = NULL;
    const char *key = NULL;
    unsigned int own = 0u;
    if ((program != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = program, .other = "black", .cap = CLI_CELLS_POOL})))
    {
        names = s_black;
        count = 1u;
        table = "tool.black";
        key = "line-length";
        own = 88u;
    }
    else if ((program != NULL) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = program, .other = "rustfmt", .cap = CLI_CELLS_POOL})))
    {
        names = s_rustfmt;
        count = 2u;
        key = "max_width";
        own = 100u;
    }
    else if ((program != NULL) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = program, .other = "clang-format", .cap = CLI_CELLS_POOL})))
    {
        names = s_clang;
        count = 2u;
        key = "ColumnLimit";
        own = 80u;
    }
    else if ((program != NULL) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = program, .other = "prettier", .cap = CLI_CELLS_POOL})))
    {
        names = s_prettier;
        count = 4u;
        key = "printWidth";
        own = 80u;
    }
    if (names == NULL)
    {
        cli_format_released();
        return 0;
    }
    const int settings = cli_format_nearest(path, names, count);
    if ((cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "markdown", .cap = CLI_CELLS_POOL})) &&
        !(settings && cli_wraps_prose(g_cli_format.settings)))
    {
        cli_format_released();
        return 0;
    }
    unsigned int found = own;
    if (settings && !cli_format_number_in(g_cli_format.settings, table, key, &found))
    {
        found = own;
    }
    const char *const name = cli_json_string(json, cli_json_member(json, tool, "name"), NULL);
    memor.cpy(&(MemoriaCfg){
        .dst = by,
        .src = (name != NULL) ? name : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (name != NULL) ? name : "", .cap = CLI_CELLS_POOL}) + 1u});
    *width = found;
    cli_format_released();
    return found > 0u;
}

int cli_format_width(const char *path, const char *language, unsigned int *width)
{
    char by[CLI_NAME_LONGEST];
    return cli_format_margin(path, language, width, by);
}
