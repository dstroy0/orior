// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_toolchains.c: `orior file toolchains`: with no words, every toolchain as orior finds it, a group at a
// time, with its version where it says one, and whether orior itself is on the PATH. `install` opens a tool's install
// page, or runs the line its makers give to install it by, in this terminal; `add-path` puts the folder its program
// was found in, or orior's own, on the reader's PATH; `use` has orior run a tool from a folder and `forget` drops that
// folder; `add` and `add-group` add a toolchain or a group of the reader's, and `remove` and `remove-group` take one
// out again
#include "../runs/runner.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "command_line_internal.h"

static CliJsonNode s_node[1u << 16u];
static char s_tree_text[1u << 22u];
static CfgJsonToken s_token[1u << 16u];
static CliJson s_json;
static char s_found[1u << 22u];
static char s_line[1u << 16u];

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
    CliLine line = {s_line, sizeof(s_line), 0ull};
    const char *const named = (id != NULL) ? id : "";
    cli_line_text(&line, "  ");
    cli_line_text(&line, named);
    for (unsigned long long letters =
             cli_text_letters(named, cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}));
         letters < wide; letters += 1ull)
    {
        cli_line_character(&line, ' ');
    }
    cli_line_text(&line, "  ");
    cli_line_text(&line, shown);
    for (unsigned long long letters = cellul.len(&(CatenaFinitaCfg){.src = shown, .cap = CLI_CELLS_POOL});
         letters < 11ull; letters += 1ull)
    {
        cli_line_character(&line, ' ');
    }
    cli_line_text(&line, "  ");
    if ((program != NULL) && (version != NULL))
    {
        cli_line_text(&line, version);
        cli_line_text(&line, "  ");
        cli_line_text(&line, program);
    }
    else if (program != NULL)
    {
        cli_line_text(&line, program);
    }
    else if (install != NULL)
    {
        cli_line_text(&line, "install: ");
        cli_line_text(&line, install);
    }
    cli_out_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
}

// a group written, and each toolchain found in it
static void toolchains_group(void *context, const char *group)
{
    const CliToolchainsListed *const listed = context;
    CliLine line = {s_line, sizeof(s_line), 0ull};
    cli_line_character(&line, '\n');
    cli_line_text(&line, group);
    cli_out_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
    int any = 0;
    for (unsigned int tool = s_json.node[listed->found].first; tool != CLI_JSON_NONE; tool = s_json.node[tool].next)
    {
        const char *const held = toolchains_text(tool, "group");
        if ((held != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = held, .other = group, .cap = CLI_CELLS_POOL})))
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

// every toolchain as orior finds it, a group at a time, and whether orior is on the PATH
static int toolchains_listed(void)
{
    CliLine found = {s_found, sizeof(s_found), 0ull};
    cli_toolchains_check(1, &found);
    cli_json_begun(&s_json, s_node, sizeof(s_node) / sizeof(s_node[0]), s_tree_text, sizeof(s_tree_text));
    char why[512];
    CliLine unread = {why, sizeof(why), 0ull};
    CliToolchainsListed listed = {cli_json_read(&s_json, s_found, cli_line_fits(&found) ? found.at : 0ull, s_token,
                                                sizeof(s_token) / sizeof(s_token[0]), &unread),
                                  0ull};
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
    char folder[CLI_PATH_LONGEST];
    int on_path = 0;
    char said_text[1024];
    CliLine said = {said_text, sizeof(said_text), 0ull};
    if (!cli_toolchains_own(folder, &on_path, &said))
    {
        cli_err_line(said_text, cli_line_fits(&said) ? said.at : 0ull);
        return 0;
    }
    CliLine line = {s_line, sizeof(s_line), 0ull};
    cli_line_text(&line, on_path ? "\norior is on PATH: "
                                 : "\norior is not on PATH: orior file toolchains add-path orior adds ");
    cli_line_text(&line, folder);
    cli_out_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
    return 0;
}

// what a toolchain command did written, 0; or why it did not, NO_CODE
static int toolchains_done(int done, const CliLine *out, const CliLine *said)
{
    if (done)
    {
        cli_out_line(out->out, cli_line_fits(out) ? out->at : 0ull);
        return 0;
    }
    cli_err_line(said->out, cli_line_fits(said) ? said->at : 0ull);
    return CLI_NO_CODE;
}

// tool `id` installed: by the line its makers give, run in bash in this terminal, or its install page opened
static int toolchains_install(const char *id, CliLine *out, CliLine *said)
{
    static char s_setup[1u << 14u];
    CliLine setup = {s_setup, sizeof(s_setup), 0ull};
    if (!cli_toolchains_setup_line(id, &setup, said) || (cli_line_closed(&setup) == 0ull))
    {
        said->at = 0ull;
        static char s_url[CLI_PATH_LONGEST * 4u];
        CliLine url = {s_url, sizeof(s_url), 0ull};
        const int opened = cli_toolchains_open_install(id, &url, said);
        if (opened)
        {
            cli_line_text(out, "opened ");
            cli_line_counted(out, s_url, cli_line_fits(&url) ? url.at : 0ull);
        }
        return toolchains_done(opened, out, said);
    }
    char bash[CLI_PATH_LONGEST];
    if (!cli_runner_bash(bash, said))
    {
        return toolchains_done(0, out, said);
    }
    CliLine line = {s_line, sizeof(s_line), 0ull};
    cli_line_text(&line, "$ ");
    cli_line_text(&line, s_setup);
    cli_err_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
    static char s_path[CLI_PATH_TEXT];
    cli_toolchains_run_path(s_path, sizeof(s_path));
    const CliSetting setting = {"PATH", s_path};
    const char *const words[4] = {bash, "-c", s_setup, NULL};
    long long code = 0ll;
    if (!cli_spawn_shared(words, &setting, 1u, &code, said))
    {
        return toolchains_done(0, out, said);
    }
    return (code >= 0ll) ? (int)code : CLI_NO_CODE;
}

int cli_toolchains_words(const char *const *words, unsigned int count)
{
    static char s_out[CLI_PATH_LONGEST * 4u];
    static char s_said[CLI_PATH_LONGEST * 4u];
    CliLine out = {s_out, sizeof(s_out), 0ull};
    CliLine said = {s_said, sizeof(s_said), 0ull};
    const char *const first = (count > 0u) ? words[0] : NULL;
    const char *const second = (count > 1u) ? words[1] : NULL;
    if ((first != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "add", .cap = CLI_CELLS_POOL})))
    {
        return toolchains_done(cli_toolchain_added(words + 1, count - 1u, &out, &said), &out, &said);
    }
    if ((first == NULL) ||
        ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "check", .cap = CLI_CELLS_POOL})) && (second == NULL)))
    {
        return toolchains_listed();
    }
    char folder[CLI_PATH_LONGEST] = "";
    if ((count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "add-group", .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(&out, "added the group ");
        cli_line_text(&out, second);
        return toolchains_done(cli_toolchains_add_group(second, &said), &out, &said);
    }
    if ((count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "remove", .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(&out, "took out ");
        cli_line_text(&out, second);
        return toolchains_done(cli_toolchains_remove(second, &said), &out, &said);
    }
    if ((count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "remove-group", .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(&out, "took out the group ");
        cli_line_text(&out, second);
        return toolchains_done(cli_toolchains_remove_group(second, &said), &out, &said);
    }
    if ((count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "install", .cap = CLI_CELLS_POOL})))
    {
        return toolchains_install(second, &out, &said);
    }
    if ((count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "add-path", .cap = CLI_CELLS_POOL})))
    {
        const int added = cli_toolchains_add_to_path(second, folder, &said);
        cli_line_text(&out, folder);
        cli_line_text(&out, " is on your PATH for every terminal started from now");
        return toolchains_done(added, &out, &said);
    }
    if ((count == 3u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "use", .cap = CLI_CELLS_POOL})))
    {
        char program[CLI_PATH_LONGEST] = "";
        const int chosen = cli_toolchains_choose(second, words[2], program, &said);
        cli_line_text(&out, "orior runs ");
        cli_line_text(&out, program);
        return toolchains_done(chosen, &out, &said);
    }
    if ((count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "forget", .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(&out, "orior looks for ");
        cli_line_text(&out, second);
        cli_line_text(&out, " on the PATH again");
        return toolchains_done(cli_toolchains_forget(second, &said), &out, &said);
    }
    cli_err("toolchains takes check, install <tool>, add-path <tool|orior>, use <tool> <folder>, forget <tool>, add "
            "<name> --group <group> --program <name,...>, add-group <group>, remove <tool> or remove-group <group>");
    return CLI_WRONG;
}
