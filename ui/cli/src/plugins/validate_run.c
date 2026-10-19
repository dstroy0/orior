// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_run.c: one check of the copy: the words that run the prover on it, the limits' settings just before the
// file, and the check run within the tool's patience, its errors after its output
#include "validate_internal.h"

// `text` with every `{file}` in it written as `file`, put after the words
static void validate_filled(CliWords *words, const char *text, const char *file)
{
    char filled[CLI_PATH_TEXT];
    CliLine line = {filled, sizeof(filled), 0ull};
    for (const char *at = text; *at != '\0';)
    {
        if (cellul.starts(&(CatenaFinitaCfg){.src = at, .other = "{file}", .cap = CLI_CELLS_POOL}))
        {
            cli_line_text(&line, file);
            at += 6;
            continue;
        }
        cli_line_character(&line, *at);
        at += 1;
    }
    cli_line_closed(&line);
    cli_words_put(words, filled);
}

int cli_validate_command_for(const char *program, const char *file, const char *project, CliWords *words,
                             unsigned int *from, unsigned int *settings, char *dir, CliLine *said)
{
    const CliJson *const json = &g_cli_validate->json;
    const CliTool *const tool = &g_cli_validate->tool;
    static CliWords s_named;
    s_named.count = 0u;
    s_named.used = 0u;
    s_named.full = 0;
    const int inside = (project != NULL) && (cli_json_item(json, tool->in_project, 0u) != CLI_JSON_NONE);
    const unsigned int listed = inside ? tool->in_project : tool->args;
    if (!inside)
    {
        cli_words_put(&s_named, tool->program);
    }
    for (unsigned int index = 0u; cli_json_item(json, listed, index) != CLI_JSON_NONE; index += 1u)
    {
        validate_filled(&s_named, cli_json_string(json, cli_json_item(json, listed, index), NULL), file);
    }
    if (inside)
    {
        memor.cpy(&(MemoriaCfg){.dst = dir, .src = project, .bytes = CLI_PATH_LONGEST});
    }
    else if (!cli_path_parent(file, dir))
    {
        dir[0] = '\0';
    }
    char runs[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    const char *const first = s_named.word[0];
    if (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = tool->program, .cap = CLI_CELLS_POOL}))
    {
        memor.cpy(&(MemoriaCfg){.dst = runs,
                                .src = program,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else if (!cli_path_parent(program, folder) || !cli_toolchains_program_in(folder, &first, 1u, runs))
    {
        cli_line_text(said, tool->name);
        cli_line_text(said, " has no ");
        cli_line_text(said, first);
        cli_line_text(said, " beside ");
        cli_line_text(said, program);
        return 0;
    }
    unsigned int at = 1u;
    while ((at < s_named.count) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = s_named.word[at], .other = file, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    words->count = 0u;
    words->used = 0u;
    words->full = 0;
    cli_words_put(words, runs);
    for (unsigned int word = 1u; word < at; word += 1u)
    {
        cli_words_put(words, s_named.word[word]);
    }
    unsigned int keys[CLI_VALIDATE_BARRIERS];
    const unsigned int grown = cli_validate_grown_keys(keys);
    *from = words->count;
    *settings = grown;
    for (unsigned int key = 0u; key < grown; key += 1u)
    {
        char value[24];
        CliLine value_line = {value, sizeof(value), 0ull};
        cli_line_decimal(&value_line, g_cli_validate->grown[keys[key]], 1u);
        cli_line_closed(&value_line);
        char setting[CLI_PATH_LONGEST];
        CliLine line = {setting, sizeof(setting), 0ull};
        for (const char *letter = tool->barriers[keys[key]].lift; *letter != '\0';)
        {
            const int marked =
                (cellul.starts(&(CatenaFinitaCfg){.src = letter, .other = "{value}", .cap = CLI_CELLS_POOL}));
            cli_line_text(&line, marked ? value : "");
            cli_line_counted(&line, letter, marked ? 0ull : 1ull);
            letter += marked ? 7 : 1;
        }
        cli_line_closed(&line);
        cli_words_put(words, setting);
    }
    for (unsigned int word = at; word < s_named.count; word += 1u)
    {
        cli_words_put(words, s_named.word[word]);
    }
    return 1;
}

int cli_validate_checked(const CliWords *words, const char *dir, double *seconds, CliLine *said)
{
    cli_toolchains_run_path(g_cli_validate->path, CLI_PATH_TEXT);
    const CliSetting settings[1] = {{"PATH", g_cli_validate->path}};
    CliRan *const ran = cli_ran_taken();
    const unsigned long long began = cli_clock_milliseconds();
    char why[CLI_SAID_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    const int started = cli_ran_within(words->word, (dir[0] != '\0') ? dir : NULL, settings, 1u, NULL, 0ull,
                                       g_cli_validate->tool.patience * 1000ull, ran, &unstarted);
    *seconds = (double)(cli_clock_milliseconds() - began) / 1000.0;
    if (!started)
    {
        cli_ran_given(ran);
        cli_line_text(said, "the prover did not start: ");
        cli_line_counted(said, why, unstarted.at);
        return 0;
    }
    // an output that is not UTF-8 reads as nothing, as Rust's read_to_string leaves it
    const unsigned long long out = cli_utf8_whole(ran->out, ran->out_size) ? ran->out_size : 0ull;
    const unsigned long long err = cli_utf8_whole(ran->err, ran->err_size) ? ran->err_size : 0ull;
    // the output kept in a cell of its size, the one kept before given back
    cli_cell_given(g_cli_validate->output);
    g_cli_validate->output_size = 0ull;
    g_cli_validate->output = cli_cell_taken(out + err + 2ull, "a check's output");
    if (g_cli_validate->output == NULL)
    {
        cli_ran_given(ran);
        cli_line_text(said, "a check's output could not be kept");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = g_cli_validate->output, .src = ran->out, .bytes = out});
    unsigned long long used = out;
    unsigned long long blank = 0ull;
    while ((blank < err) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = ran->err[blank]}) != NULL))
    {
        blank += 1ull;
    }
    if (blank < err)
    {
        g_cli_validate->output[used] = '\n';
        memor.cpy(&(MemoriaCfg){.dst = g_cli_validate->output + used + 1ull, .src = ran->err, .bytes = err});
        used += 1ull + err;
    }
    g_cli_validate->output_size = used;
    g_cli_validate->late = ran->stopped;
    cli_ran_given(ran);
    return 1;
}
