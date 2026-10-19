// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// format.c: a file's language by the plugins, the toolchain that formats a language, and a text formatted by it
#include "../plugins/plugins.h"
#include "../text/text.h"
#include "format_internal.h"

CliFormatRoom g_cli_format;
CliLock g_cli_format_lock = CLI_LOCK_INIT;

void cli_format_released(void)
{
    cli_json_given(&g_cli_format.json);
    cli_cell_given(g_cli_format.settings);
    g_cli_format.settings = NULL;
    g_cli_format.settings_size = 0ull;
    cli_lock_give(&g_cli_format_lock);
}

// A choice of a file's language: the extension in lower case, and the plugin chosen and where it came from
typedef struct
{
    char extension[CLI_NAME_LONGEST];
    char id[CLI_NAME_LONGEST];
    int chosen;
} CliLanguageChoice;

static CliLanguageChoice s_choice;

// whether two texts are the same, ASCII's case aside
static int cli_same_case_aside(const char *one, unsigned long long one_length, const char *other)
{
    if (one_length != cellul.len(&(CatenaFinitaCfg){.src = other, .cap = CLI_CELLS_POOL}))
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < one_length; at += 1ull)
    {
        const char left = ((one[at] >= 'A') && (one[at] <= 'Z')) ? (char)(one[at] + 32) : one[at];
        const char right = ((other[at] >= 'A') && (other[at] <= 'Z')) ? (char)(other[at] + 32) : other[at];
        if (left != right)
        {
            return 0;
        }
    }
    return 1;
}

static void cli_plugin_each(void *context, const CliPlugin *plugin)
{
    (void)context;
    CliJson json;
    char why[CLI_NAME_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_taken(&json, plugin->text, plugin->size, 0u, 0ull, &unread);
    const unsigned int extensions = cli_json_member(&json, root, "extensions");
    int names = 0;
    for (unsigned int at = 0u; !names && (cli_json_item(&json, extensions, at) != CLI_JSON_NONE); at += 1u)
    {
        unsigned long long length = 0ull;
        const char *const one = cli_json_string(&json, cli_json_item(&json, extensions, at), &length);
        names = (one != NULL) && cli_same_case_aside(one, length, s_choice.extension);
    }
    if (names && (!s_choice.chosen ||
                  (cellul.eq(&(CatenaFinitaCfg){.src = plugin->source, .other = "user", .cap = CLI_CELLS_POOL}))))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = s_choice.id,
                          .src = plugin->id,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = plugin->id, .cap = CLI_CELLS_POOL}) + 1u});
        s_choice.chosen = 1;
    }
    cli_json_given(&json);
}

int cli_format_language_of(const char *path, char *id)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    const char *name = path;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    if ((dot == NULL) || (dot == name))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    unsigned int kept = 0u;
    for (const char *at = dot + 1; (at[0] != '\0') && (kept < (CLI_NAME_LONGEST - 1u)); at += 1)
    {
        s_choice.extension[kept] = ((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0];
        kept += 1u;
    }
    s_choice.extension[kept] = '\0';
    s_choice.chosen = 0;
    cli_plugins_each(cli_plugin_each, NULL);
    const int chosen = s_choice.chosen;
    if (chosen)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = id,
                          .src = s_choice.id,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_choice.id, .cap = CLI_CELLS_POOL}) + 1u});
    }
    cli_lock_give(&s_lock);
    return chosen;
}

void cli_format_languages(CliLine *out)
{
    cli_lock_take(&g_cli_format_lock);
    const unsigned int manifest = cli_toolchains_manifest(&g_cli_format.json, 0u, 0ull);
    cli_line_character(out, '[');
    unsigned int written = 0u;
    for (unsigned int index = 0u; cli_json_item(&g_cli_format.json, manifest, index) != CLI_JSON_NONE; index += 1u)
    {
        const unsigned int tool = cli_json_item(&g_cli_format.json, manifest, index);
        if (cli_json_member(&g_cli_format.json, tool, "format") == CLI_JSON_NONE)
        {
            continue;
        }
        const unsigned int formats = cli_json_member(&g_cli_format.json, tool, "formats");
        for (unsigned int at = 0u; cli_json_item(&g_cli_format.json, formats, at) != CLI_JSON_NONE; at += 1u)
        {
            unsigned long long length = 0ull;
            const char *const one =
                cli_json_string(&g_cli_format.json, cli_json_item(&g_cli_format.json, formats, at), &length);
            if (one != NULL)
            {
                cli_line_text(out, (written != 0u) ? "," : "");
                cli_json_quoted(out, one, length);
                written += 1u;
            }
        }
    }
    cli_line_character(out, ']');
    cli_format_released();
}

unsigned int cli_format_tool(const char *language, CliLine *said)
{
    CliJson *const json = &g_cli_format.json;
    cli_json_given(json);
    const unsigned int manifest = cli_toolchains_manifest(json, 0u, 0ull);
    for (unsigned int index = 0u; cli_json_item(json, manifest, index) != CLI_JSON_NONE; index += 1u)
    {
        const unsigned int tool = cli_json_item(json, manifest, index);
        const unsigned int format = cli_json_member(json, tool, "format");
        const unsigned int formats = cli_json_member(json, tool, "formats");
        for (unsigned int at = 0u; (format != CLI_JSON_NONE) && (cli_json_item(json, formats, at) != CLI_JSON_NONE);
             at += 1u)
        {
            const char *const one = cli_json_string(json, cli_json_item(json, formats, at), NULL);
            if ((one != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = one, .other = language, .cap = CLI_CELLS_POOL})))
            {
                return tool;
            }
        }
    }
    cli_line_text(said, "orior has no formatter for ");
    cli_line_text(said, language);
    cli_line_text(said, ": it formats with ");
    unsigned int written = 0u;
    for (unsigned int index = 0u; cli_json_item(json, manifest, index) != CLI_JSON_NONE; index += 1u)
    {
        const unsigned int tool = cli_json_item(json, manifest, index);
        const char *const name = cli_json_string(json, cli_json_member(json, tool, "name"), NULL);
        if ((cli_json_member(json, tool, "format") != CLI_JSON_NONE) && (name != NULL))
        {
            cli_line_text(said, (written != 0u) ? ", " : "");
            cli_line_text(said, name);
            written += 1u;
        }
    }
    return CLI_JSON_NONE;
}

int cli_format_program_of(const char *language, char *program)
{
    cli_lock_take(&g_cli_format_lock);
    char why[CLI_NAME_LONGEST * 4u];
    CliLine unknown = {why, sizeof(why), 0ull};
    const unsigned int tool = cli_format_tool(language, &unknown);
    const char *const named =
        (tool != CLI_JSON_NONE)
            ? cli_json_string(
                  &g_cli_format.json,
                  cli_json_member(&g_cli_format.json, cli_json_member(&g_cli_format.json, tool, "format"), "program"),
                  NULL)
            : NULL;
    if (named != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = program,
                                .src = named,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) + 1u});
    }
    cli_format_released();
    return named != NULL;
}

void cli_format_edition_of(const char *path, char *edition)
{
    static const char *const s_names[1] = {"Cargo.toml"};
    memor.cpy(&(MemoriaCfg){.dst = edition,
                            .src = "2021",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "2021", .cap = CLI_CELLS_POOL}) + 1u});
    if (!cli_format_nearest(path, s_names, 1u))
    {
        return;
    }
    for (const char *line = g_cli_format.settings; line[0] != '\0';)
    {
        const char *end = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '\n'});
        end = (end != NULL) ? end : (line + cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}));
        const char *at = line;
        while ((at < end) && ((at[0] == ' ') || (at[0] == '\t')))
        {
            at += 1;
        }
        if (((end - at) >= 7) && (memor.cmp(&(MemoriaCfg){.src = at, .other = "edition", .bytes = 7u}) == 0))
        {
            at += 7;
            while ((at < end) && ((at[0] == ' ') || (at[0] == '\t')))
            {
                at += 1;
            }
            if ((at < end) && (at[0] == '='))
            {
                at += 1;
                const char *stop = end;
                while ((at < stop) && ((at[0] == ' ') || ((at[0] >= '\t') && (at[0] <= '\r')) || (at[0] == '"')))
                {
                    at += 1;
                }
                while ((stop > at) &&
                       ((stop[-1] == ' ') || ((stop[-1] >= '\t') && (stop[-1] <= '\r')) || (stop[-1] == '"')))
                {
                    stop -= 1;
                }
                // the edition lies between its quotes, its end at or past its start
                const unsigned long long length = (unsigned long long)(stop - at);
                if (length < CLI_NAME_LONGEST)
                {
                    memor.cpy(&(MemoriaCfg){.dst = edition, .src = at, .bytes = length});
                    edition[length] = '\0';
                }
                return;
            }
        }
        line = (end[0] == '\n') ? (end + 1) : end;
    }
}

// a formatter's word with `{file}` and `{edition}` written in, put after the words: its start, or NULL where they are
// full
static const char *cli_word_filled(const char *word, const char *file, const char *edition, unsigned long long *used)
{
    char *const start = g_cli_format.words + *used;
    CliLine line = {start, CLI_WORDS_TEXT - *used, 0ull};
    for (const char *at = word; at[0] != '\0';)
    {
        if (cellul.starts(&(CatenaFinitaCfg){.src = at, .other = "{file}", .cap = CLI_CELLS_POOL}))
        {
            cli_line_text(&line, file);
            at += 6;
        }
        else if (cellul.starts(&(CatenaFinitaCfg){.src = at, .other = "{edition}", .cap = CLI_CELLS_POOL}))
        {
            cli_line_text(&line, edition);
            at += 9;
        }
        else
        {
            cli_line_character(&line, at[0]);
            at += 1;
        }
    }
    cli_line_closed(&line);
    if (!cli_line_fits(&line))
    {
        return NULL;
    }
    *used += line.at + 1ull;
    return start;
}

int cli_format(const char *path, const char *language, const char *text, unsigned long long length, CliLine *out,
               CliLine *said)
{
    cli_lock_take(&g_cli_format_lock);
    CliJson *const json = &g_cli_format.json;
    const unsigned int tool = cli_format_tool(language, said);
    if (tool == CLI_JSON_NONE)
    {
        cli_format_released();
        return 0;
    }
    const unsigned int format = cli_json_member(json, tool, "format");
    const char *const named = cli_json_string(json, cli_json_member(json, format, "program"), NULL);
    const char *const tool_name = cli_json_string(json, cli_json_member(json, tool, "name"), NULL);
    const char *const program_name = (named != NULL) ? named : "";
    unsigned int count = 0u;
    char (*const folders)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&count);
    cli_toolchains_find(json, tool, (const char (*)[CLI_PATH_LONGEST])folders, count, &g_cli_format.found);
    cli_cell_given(folders);
    char program[CLI_PATH_LONGEST];
    const char *const names[1] = {program_name};
    if ((g_cli_format.found.folder[0] == '\0') ||
        !cli_toolchains_program_in(g_cli_format.found.folder, names, 1u, program))
    {
        cli_line_text(said, (tool_name != NULL) ? tool_name : program_name);
        cli_line_text(said, " is not found: File, Toolchains opens its install page or takes its folder");
        cli_format_released();
        return 0;
    }
    const unsigned int args = cli_json_member(json, format, "args");
    char edition[CLI_NAME_LONGEST] = "";
    for (unsigned int at = 0u; cli_json_item(json, args, at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const word = cli_json_string(json, cli_json_item(json, args, at), NULL);
        if ((word != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = word, .cap = CLI_CELLS_POOL, .other = "{edition}", .other_cap = CLI_CELLS_POOL}) != NULL))
        {
            cli_format_edition_of(path, edition);
            break;
        }
    }
    const char *words[CLI_WORDS_MOST + 2u] = {program};
    unsigned int word_count = 1u;
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; (word_count <= CLI_WORDS_MOST) && (cli_json_item(json, args, at) != CLI_JSON_NONE);
         at += 1u)
    {
        const char *const word = cli_json_string(json, cli_json_item(json, args, at), NULL);
        const char *const filled = (word != NULL) ? cli_word_filled(word, path, edition, &used) : NULL;
        if (filled != NULL)
        {
            words[word_count] = filled;
            word_count += 1u;
        }
    }
    words[word_count] = NULL;
    cli_toolchains_run_path(g_cli_format.path, sizeof(g_cli_format.path));
    const CliSetting settings[1] = {{"PATH", g_cli_format.path}};
    char folder[CLI_PATH_LONGEST];
    const int in_folder = cli_path_parent(path, folder) && query_is_folder(folder);
    CliRan *const ran = cli_ran_taken();
    char why[CLI_NAME_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    const int started = cli_ran_within(words, in_folder ? folder : NULL, settings, 1u, text, length,
                                       CLI_FORMAT_PATIENCE, ran, &unstarted);
    cli_format_released();
    const int formatted = started && !ran->stopped && (ran->code == 0ll);
    if (!started)
    {
        cli_line_text(said, program);
        cli_line_text(said, ": ");
        cli_line_counted(said, why, unstarted.at);
    }
    else if (ran->stopped)
    {
        cli_line_text(said, program_name);
        cli_line_text(said, " took longer than 30 s and was stopped");
    }
    else if (!formatted)
    {
        cli_format_failed(program_name, path, ran, said);
    }
    else if (!cli_format_utf8(ran->out, ran->out_size))
    {
        cli_line_text(said, program_name);
        cli_line_text(said, " wrote text that is not UTF-8");
        cli_ran_given(ran);
        return 0;
    }
    else
    {
        cli_json_quoted(out, ran->out, ran->out_size);
    }
    cli_ran_given(ran);
    return formatted;
}
