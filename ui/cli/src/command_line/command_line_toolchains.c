// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_toolchains.c: `orior_cli file toolchains`: with no words, every toolchain as orior_cli finds it, a group
// at a time, with its version where it reports one, and whether orior_cli itself is on the PATH. `install` opens a
// tool's install page, or runs the line its makers give to install it by, in this terminal; `add-path` puts the folder
// its program was found in, or orior's own, on the reader's PATH; `use` has orior_cli run a tool from a folder and
// `forget` drops that folder; `add` and `add-group` add a toolchain or a group of the reader's, and `remove` and
// `remove-group` take one out again
#include "../runs/runner.h"
#include "../text/pieces.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "cells.h"
#include "command_line_internal.h"

static CellOwner s_cells = MMGR_CELL_OWNER("cli_toolchains_words");

// the toolchains found, read as a tree sized to them while they are listed
static CliJson s_json;

// the bytes of a piece a line is written in before the piece is stored in a cell of its own
#define CLI_TOOLCHAINS_PIECE 256u

// a line written in pieces put out, joined into a cell of its size and the cell given back; an empty line where it is
// not whole
static void toolchains_line_out(CliPieces *pieces, VerbaLinea *line)
{
    unsigned long long size = 0ull;
    char *const joined = cli_pieces_joined(pieces, line, "a line of the toolchains", &size);
    cli_out_line((joined != NULL) ? joined : "", (joined != NULL) ? size : 0ull);
    cli_cell_release(joined, NULL);
}

// What the listing is writing: the toolchains found, and the widest id among them
typedef struct
{
    unsigned int found;
    unsigned long long wide;
} CliToolchainsListed;

// a string member of a toolchain found, NULL where it has none
static const char *toolchains_text(unsigned int tool, const char *name)
{
    return cli_json_string(&s_json, cli_json_member(&s_json, tool, name), NULL);
}

// a toolchain written: its id, its state as the reader reads it, and its version and program, else its install page
static void toolchains_printed(unsigned int tool, unsigned long long wide)
{
    static const char *const s_states[4][2] = {
        {"env", "named"}, {"chosen", "chosen"}, {"path", "on PATH"}, {"found", "not on PATH"}};
    const char *const state = toolchains_text(tool, "state");
    const char *shown = "missing";
    for (unsigned int at = 0u; (at < 4u) && (state != NULL); at += 1u)
    {
        shown = (cellul.eq(&(CatenaFinitaCfg){.src = state, .other = s_states[at][0], .cap = CLI_CELLS_POOL}))
                    ? s_states[at][1]
                    : shown;
    }
    const char *const id = toolchains_text(tool, "id");
    const char *const program = toolchains_text(tool, "program");
    const char *const version = toolchains_text(tool, "version");
    const char *const install = toolchains_text(tool, "install");
    CliPieces pieces;
    VerbaLinea line;
    char piece[CLI_TOOLCHAINS_PIECE];
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
    const char *const named = (id != NULL) ? id : "";
    mmgr_line_put(&line, "  ");
    mmgr_line_put(&line, named);
    for (unsigned long long letters =
             cli_text_letters(named, cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}));
         letters < wide; letters += 1ull)
    {
        mmgr_line_ch(&line, ' ');
    }
    mmgr_line_put(&line, "  ");
    mmgr_line_put(&line, shown);
    for (unsigned long long letters = cellul.len(&(CatenaFinitaCfg){.src = shown, .cap = CLI_CELLS_POOL});
         letters < 11ull; letters += 1ull)
    {
        mmgr_line_ch(&line, ' ');
    }
    mmgr_line_put(&line, "  ");
    if ((program != NULL) && (version != NULL))
    {
        mmgr_line_put(&line, version);
        mmgr_line_put(&line, "  ");
        mmgr_line_put(&line, program);
    }
    else if (program != NULL)
    {
        mmgr_line_put(&line, program);
    }
    else if (install != NULL)
    {
        mmgr_line_put(&line, "install: ");
        mmgr_line_put(&line, install);
    }
    toolchains_line_out(&pieces, &line);
}

// a group written, and each toolchain found in it
static void toolchains_group(void *context, const char *group)
{
    const CliToolchainsListed *const listed = context;
    CliPieces pieces;
    VerbaLinea line;
    char piece[CLI_TOOLCHAINS_PIECE];
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
    mmgr_line_ch(&line, '\n');
    mmgr_line_put(&line, group);
    toolchains_line_out(&pieces, &line);
    int any = 0;
    for (unsigned int tool = s_json.node[listed->found].first; tool != CLI_JSON_NONE; tool = s_json.node[tool].next)
    {
        const char *const tool_group = toolchains_text(tool, "group");
        if ((tool_group != NULL) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = tool_group, .other = group, .cap = CLI_CELLS_POOL})))
        {
            toolchains_printed(tool, listed->wide);
            any = 1;
        }
    }
    if (!any)
    {
        cli_out("  no toolchain yet");
    }
}

// every toolchain as orior_cli finds it, a group at a time, and whether orior_cli is on the PATH
static int toolchains_listed(void)
{
    CliPieces pieces;
    VerbaLinea found;
    char piece[CLI_TOOLCHAINS_PIECE];
    cli_pieces_begun(&pieces, &found, piece, sizeof(piece));
    cli_toolchains_check(1, NULL, NULL, &found);
    unsigned long long size = 0ull;
    char *const joined = cli_pieces_joined(&pieces, &found, "the toolchains found", &size);
    cli_json_begun(&s_json, NULL, 0u, NULL, 0ull);
    char why[512];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    CliToolchainsListed listed = {
        (joined != NULL) ? cli_json_taken(&s_json, joined, size, 0u, 0ull, &unread) : CLI_JSON_NONE, 0ull};
    cli_cell_release(joined, NULL);
    if (cli_json_is(&s_json, listed.found, CFG_JSON_ARRAY))
    {
        for (unsigned int tool = s_json.node[listed.found].first; tool != CLI_JSON_NONE; tool = s_json.node[tool].next)
        {
            const char *const id = toolchains_text(tool, "id");
            const unsigned long long size =
                (id != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) : 0ull;
            listed.wide = (size > listed.wide) ? size : listed.wide;
        }
        cli_toolchains_groups(toolchains_group, &listed);
    }
    cli_json_given(&s_json);
    char folder[CLI_PATH_MAX_LEN];
    int on_path = 0;
    char err_buf[1024];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ull};
    if (!cli_toolchains_own(folder, &on_path, &err_msg))
    {
        cli_err_line(err_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        return 0;
    }
    VerbaLinea line;
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
    mmgr_line_put(&line, on_path ? "\norior_cli is on PATH: "
                                 : "\norior_cli is not on PATH: orior_cli file toolchains add-path orior_cli adds ");
    mmgr_line_put(&line, folder);
    toolchains_line_out(&pieces, &line);
    return 0;
}

// what a toolchain command did written, 0; or why it did not, NO_CODE
static int toolchains_done(int done, const VerbaLinea *out, const VerbaLinea *err_msg)
{
    if (done)
    {
        cli_out_line(out->out, mmgr_line_ok(out) ? out->at : 0ull);
        return 0;
    }
    cli_err_line(err_msg->out, mmgr_line_ok(err_msg) ? err_msg->at : 0ull);
    return CLI_NO_CODE;
}

// tool `id` installed: by the line its makers give, run in bash in this terminal, or its install page opened
static int toolchains_install(const char *id, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliPieces pieces;
    VerbaLinea setup;
    char piece[CLI_TOOLCHAINS_PIECE];
    cli_pieces_begun(&pieces, &setup, piece, sizeof(piece));
    const int has_setup = cli_toolchains_setup_line(id, &setup, err_msg);
    unsigned long long setup_size = 0ull;
    char *const setup_line = cli_pieces_joined(&pieces, &setup, "a tool's setup line", &setup_size);
    if (!has_setup || (setup_line == NULL) || (setup_size == 0ull))
    {
        cli_cell_release(setup_line, NULL);
        err_msg->at = 0ull;
        char url_text[CLI_PATH_MAX_LEN * 4u];
        VerbaLinea url = {url_text, sizeof(url_text), 0ull};
        const int opened = cli_toolchains_open_install(id, &url, err_msg);
        if (opened)
        {
            mmgr_line_put(out, "opened ");
            mmgr_line_put_n(out, url_text, mmgr_line_ok(&url) ? url.at : 0ull);
        }
        return toolchains_done(opened, out, err_msg);
    }
    char bash[CLI_PATH_MAX_LEN];
    // the PATH a setup runs with, as long as the system lets an environment's value be
    char *const path =
        cli_runner_bash(bash, err_msg) ? g_cli_cells->persistent_buf_alloc(CLI_PATH_TEXT, &s_cells) : NULL;
    if (path == NULL)
    {
        cli_cell_release(setup_line, NULL);
        return toolchains_done(0, out, err_msg);
    }
    VerbaLinea line;
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
    mmgr_line_put(&line, "$ ");
    mmgr_line_put(&line, setup_line);
    unsigned long long shown_size = 0ull;
    char *const shown = cli_pieces_joined(&pieces, &line, "a setup line shown", &shown_size);
    cli_err_line((shown != NULL) ? shown : "", (shown != NULL) ? shown_size : 0ull);
    cli_cell_release(shown, NULL);
    cli_toolchains_run_path(path, CLI_PATH_TEXT);
    const CliSetting setting = {"PATH", path};
    const char *const words[4] = {bash, "-c", setup_line, NULL};
    long long code = 0ll;
    const int spawned = cli_spawn_shared(words, &setting, 1u, &code, err_msg);
    cli_cell_release(path, &s_cells);
    cli_cell_release(setup_line, NULL);
    if (!spawned)
    {
        return toolchains_done(0, out, err_msg);
    }
    return (code >= 0ll) ? (int)code : CLI_NO_CODE;
}

int cli_toolchains_words(const char *const *words, unsigned int count)
{
    char out_text[CLI_PATH_MAX_LEN * 4u];
    char err_buf[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea out = {out_text, sizeof(out_text), 0ull};
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ull};
    const char *const first = (count > 0u) ? words[0] : NULL;
    const char *const second = (count > 1u) ? words[1] : NULL;
    if ((first != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "add", .cap = CLI_CELLS_POOL})))
    {
        return toolchains_done(cli_toolchain_added(words + 1, count - 1u, &out, &err_msg), &out, &err_msg);
    }
    if ((first == NULL) ||
        ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "check", .cap = CLI_CELLS_POOL})) && (second == NULL)))
    {
        return toolchains_listed();
    }
    char folder[CLI_PATH_MAX_LEN] = "";
    if ((count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "add-group", .cap = CLI_CELLS_POOL})))
    {
        mmgr_line_put(&out, "added the group ");
        mmgr_line_put(&out, second);
        return toolchains_done(cli_toolchains_add_group(second, &err_msg), &out, &err_msg);
    }
    if ((count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "remove", .cap = CLI_CELLS_POOL})))
    {
        mmgr_line_put(&out, "took out ");
        mmgr_line_put(&out, second);
        return toolchains_done(cli_toolchains_remove(second, &err_msg), &out, &err_msg);
    }
    if ((count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "remove-group", .cap = CLI_CELLS_POOL})))
    {
        mmgr_line_put(&out, "took out the group ");
        mmgr_line_put(&out, second);
        return toolchains_done(cli_toolchains_remove_group(second, &err_msg), &out, &err_msg);
    }
    if ((count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "install", .cap = CLI_CELLS_POOL})))
    {
        return toolchains_install(second, &out, &err_msg);
    }
    if ((count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "add-path", .cap = CLI_CELLS_POOL})))
    {
        const int added = cli_toolchains_add_to_path(second, folder, &err_msg);
        mmgr_line_put(&out, folder);
        mmgr_line_put(&out, " is on your PATH for every terminal started from now");
        return toolchains_done(added, &out, &err_msg);
    }
    if ((count == 3u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "use", .cap = CLI_CELLS_POOL})))
    {
        char program[CLI_PATH_MAX_LEN] = "";
        const int chosen = cli_toolchains_choose(second, words[2], program, &err_msg);
        mmgr_line_put(&out, "orior_cli runs ");
        mmgr_line_put(&out, program);
        return toolchains_done(chosen, &out, &err_msg);
    }
    if ((count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "forget", .cap = CLI_CELLS_POOL})))
    {
        mmgr_line_put(&out, "orior_cli looks for ");
        mmgr_line_put(&out, second);
        mmgr_line_put(&out, " on the PATH again");
        return toolchains_done(cli_toolchains_forget(second, &err_msg), &out, &err_msg);
    }
    cli_err("toolchains takes check, install <tool>, add-path <tool|orior>, use <tool> <folder>, forget <tool>, add "
            "<name> --group <group> --program <name,...>, add-group <group>, remove <tool> or remove-group <group>");
    return CLI_WRONG;
}
