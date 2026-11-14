// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_toolchains.c: `imp file toolchains`: with no words, every toolchain as impero finds it, a group
// at a time, with its version where it reports one, and whether impero itself is on the PATH. `install` opens a
// tool's install page, or runs the line its makers give to install it by, in this terminal; `add-path` puts the folder
// its program was found in, or orior's own, on the reader's PATH; `use` has imp run a tool from a folder and
// `forget` drops that folder; `add` and `add-group` add a toolchain or a group of the reader's, and `remove` and
// `remove-group` take one out again
#include "../runs/runner.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "callback_cfg.h"
#include "command_line_internal.h"
#include "pool.h"

// the toolchains found, read as a tree sized to them while they are listed
static CliJson s_json;

// the most bytes of a line of the listing or a tool's setup line, and of the toolchains found as JSON
#define CLI_TOOLCHAINS_LINE (CLI_PATH_MAX_LEN * 4u)
#define CLI_TOOLCHAINS_FOUND (1u << 20u)

typedef struct
{
    const VerbaLinea *line;
    unsigned int tool;
    const char *name;
    unsigned long long pixel_width;
    int done;
    const VerbaLinea *out;
    const VerbaLinea *err_msg;
    const char *id;
    VerbaLinea *out_out;
    VerbaLinea *err_msg_out;
} CommandLineToolchainsCtx;
// a line put out; an empty line where it did not fit
EMBED_INLINE void toolchains_line_out(const CommandLineToolchainsCtx *args)
{
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = args->line->out,
               .size = mmgr_line_ok(args->line) ? args->line->at : 0ULL);
}

// What the listing is writing: the toolchains found, and the widest id among them
typedef struct
{
    unsigned int found;
    unsigned long long pixel_width;
} CliToolchainsListing;

// a string member of a toolchain found, NULL where it has none
EMBED_INLINE const char *toolchains_text(const CommandLineToolchainsCtx *args)
{
    const char *name = args->name;
    return EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json,
                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json,
                                         .object = args->tool, .name = name),
                      .length_pointer = NULL);
}

// a toolchain written: its id, its state as the reader reads it, and its version and program, else its install page
EMBED_INLINE void toolchains_print(const CommandLineToolchainsCtx *args)
{
    static const char *const s_states[4][2] = {
        {"env", "named"}, {"chosen", "chosen"}, {"path", "on PATH"}, {"found", "not on PATH"}};
    const char *const state =
        EMBED_CALL(toolchains_text, CommandLineToolchainsCtx, .tool = args->tool, .name = "state");
    const char *displayed = "missing";
    for (unsigned int at = 0u; (at < 4u) && (state != NULL); at += 1u)
    {
        displayed = (cellul.eq(&(CatenaFinitaCfg){.src = state, .other = s_states[at][0], .cap = CLI_POOL_BYTES}))
                        ? s_states[at][1]
                        : displayed;
    }
    const char *const id = EMBED_CALL(toolchains_text, CommandLineToolchainsCtx, .tool = args->tool, .name = "id");
    const char *const program =
        EMBED_CALL(toolchains_text, CommandLineToolchainsCtx, .tool = args->tool, .name = "program");
    const char *const version =
        EMBED_CALL(toolchains_text, CommandLineToolchainsCtx, .tool = args->tool, .name = "version");
    const char *const install =
        EMBED_CALL(toolchains_text, CommandLineToolchainsCtx, .tool = args->tool, .name = "install");
    char text[CLI_TOOLCHAINS_LINE];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    const char *const named = (id != NULL) ? id : "";
    mmgr_line_put(&line, "  ");
    mmgr_line_put(&line, named);
    for (unsigned long long letters =
             EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = named,
                        .size = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_POOL_BYTES}));
         letters < args->pixel_width; letters += 1ULL)
    {
        mmgr_line_ch(&line, ' ');
    }
    mmgr_line_put(&line, "  ");
    mmgr_line_put(&line, displayed);
    for (unsigned long long letters = cellul.len(&(CatenaFinitaCfg){.src = displayed, .cap = CLI_POOL_BYTES});
         letters < 11ULL; letters += 1ULL)
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
    EMBED_CALL(toolchains_line_out, CommandLineToolchainsCtx, .line = &line);
}

// a group written, and each toolchain found in it
static void toolchains_group(const CliEachGroupCfg *args)
{
    const CliToolchainsListing *const list_entries = args->context;
    char text[CLI_TOOLCHAINS_LINE];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_ch(&line, '\n');
    mmgr_line_put(&line, args->group);
    EMBED_CALL(toolchains_line_out, CommandLineToolchainsCtx, .line = &line);
    int any = 0;
    for (unsigned int tool = s_json.node[list_entries->found].first; tool != CLI_JSON_NONE;
         tool = s_json.node[tool].next)
    {
        const char *const tool_group =
            EMBED_CALL(toolchains_text, CommandLineToolchainsCtx, .tool = tool, .name = "group");
        if ((tool_group != NULL) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = tool_group, .other = args->group, .cap = CLI_POOL_BYTES})))
        {
            EMBED_CALL(toolchains_print, CommandLineToolchainsCtx, .tool = tool,
                       .pixel_width = list_entries->pixel_width);
            any = 1;
        }
    }
    if (!any)
    {
        EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = "  no toolchain yet");
    }
}

// every toolchain as impero finds it, a group at a time, and whether impero is on the PATH
static int toolchains_list(void)
{
    CliGrownLine found;
    cli_grown_begin(&found, CLI_TOOLCHAINS_FOUND, g_cli_toolchains_words_owner);
    EMBED_CALL(cli_toolchains_versions.check, CliToolchainsVersionsCfg, .versions = 1, .seen = NULL,
               .context_read = NULL, .out = &found.line);
    const unsigned long long found_size = (mmgr_line_finish(&found.line) != 0ULL) ? cli_grown_size(&found) : 0ULL;
    const char *const found_text = found.cell;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &s_json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[512];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    CliToolchainsListing list_entries = {
        (found_size != 0ULL) ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                          .json = &s_json, .text = found_text, .size = found_size, .nodes_more = 0u,
                                          .text_more = 0ULL, .err_msg = &unread, .owner = g_cli_toolchains_words_owner)
                             : CLI_JSON_NONE,
        0ULL};
    cli_grown_end(&found);
    if (EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &s_json,
                   .node = list_entries.found, .type = CFG_JSON_ARRAY))
    {
        for (unsigned int tool = s_json.node[list_entries.found].first; tool != CLI_JSON_NONE;
             tool = s_json.node[tool].next)
        {
            const char *const id = EMBED_CALL(toolchains_text, CommandLineToolchainsCtx, .tool = tool, .name = "id");
            const unsigned long long size =
                (id != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_POOL_BYTES}) : 0ULL;
            list_entries.pixel_width = (size > list_entries.pixel_width) ? size : list_entries.pixel_width;
        }
        EMBED_CALL(cli_toolchains_each.groups, CliToolchainsEachCfg, .each = toolchains_group,
                   .context = &list_entries);
    }
    g_cli_guard->persistent_buf_release(s_json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char folder[CLI_PATH_MAX_LEN];
    int on_path = 0;
    char err_buf[1024];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ULL};
    if (!EMBED_CALL(cli_toolchains_err_msg_folder_on_path.own, CliToolchainsErrMsgFolderOnPathCfg, .folder = folder,
                    .on_path = &on_path, .err_msg = &err_msg))
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = err_buf,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        return 0;
    }
    char text[CLI_TOOLCHAINS_LINE];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_put(&line, on_path ? "\nimp is on PATH: "
                                 : "\nimpero is not on PATH: imp file toolchains add-path impero adds ");
    mmgr_line_put(&line, folder);
    EMBED_CALL(toolchains_line_out, CommandLineToolchainsCtx, .line = &line);
    return 0;
}

// what a toolchain command did written, 0; or why it did not, NO_CODE
EMBED_INLINE int toolchains_finish(const CommandLineToolchainsCtx *args)
{
    if (args->done)
    {
        EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = args->out->out,
                   .size = mmgr_line_ok(args->out) ? args->out->at : 0ULL);
        return 0;
    }
    EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = args->err_msg->out,
               .size = mmgr_line_ok(args->err_msg) ? args->err_msg->at : 0ULL);
    return CLI_NO_CODE;
}

// tool `id` installed: by the line its makers give, run in bash in this terminal, or its install page opened
EMBED_INLINE int toolchains_install(const CommandLineToolchainsCtx *args)
{
    const char *id = args->id;
    VerbaLinea *out = args->out_out;
    VerbaLinea *err_msg = args->err_msg_out;
    char setup_line[CLI_TOOLCHAINS_LINE];
    VerbaLinea setup = {setup_line, sizeof(setup_line), 0ULL};
    const int has_setup = EMBED_CALL(cli_toolchains_id_err_msg.setup_line, CliToolchainsIdErrMsgCfg, .id = id,
                                     .out = &setup, .err_msg = err_msg);
    if (!has_setup || (mmgr_line_finish(&setup) == 0u))
    {
        err_msg->at = 0ULL;
        char url_text[CLI_PATH_MAX_LEN * 4u];
        VerbaLinea url = {url_text, sizeof(url_text), 0ULL};
        const int opened = EMBED_CALL(cli_toolchains_id_err_msg.open_install, CliToolchainsIdErrMsgCfg, .id = id,
                                      .out = &url, .err_msg = err_msg);
        if (opened)
        {
            mmgr_line_put(out, "opened ");
            mmgr_line_put_n(out, url_text, mmgr_line_ok(&url) ? url.at : 0ULL);
        }
        return EMBED_CALL(toolchains_finish, CommandLineToolchainsCtx, .done = opened, .out = out, .err_msg = err_msg);
    }
    char bash[CLI_PATH_MAX_LEN];
    // the PATH a setup runs with, as long as the system lets an environment's value be
    char *const path = EMBED_CALL(cli_runner_name.bash, CliRunnerNameCfg, .path = bash, .err_msg = err_msg)
                           ? g_cli_guard->temporary_buf_alloc(CLI_PATH_TEXT, g_cli_toolchains_words_owner)
                           : NULL;
    if (path == NULL)
    {
        return EMBED_CALL(toolchains_finish, CommandLineToolchainsCtx, .done = 0, .out = out, .err_msg = err_msg);
    }
    char displayed_text[CLI_TOOLCHAINS_LINE + 2u];
    VerbaLinea displayed = {displayed_text, sizeof(displayed_text), 0ULL};
    mmgr_line_put(&displayed, "$ ");
    mmgr_line_put(&displayed, setup_line);
    EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = displayed_text,
               .size = mmgr_line_ok(&displayed) ? displayed.at : 0ULL);
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = path, .size = CLI_PATH_TEXT);
    const CliSetting setting = {"PATH", path};
    const char *const words[4] = {bash, "-c", setup_line, NULL};
    long long code = 0LL;
    const int spawned =
        EMBED_CALL(cli_words_err_msg_settings_internal.spawn_share, CliWordsErrMsgSettingsCtx, .words = words,
                   .settings = &setting, .count = 1u, .code = &code, .err_msg = err_msg);
    g_cli_guard->persistent_buf_release(path, g_cli_toolchains_words_owner);
    if (!spawned)
    {
        return EMBED_CALL(toolchains_finish, CommandLineToolchainsCtx, .done = 0, .out = out, .err_msg = err_msg);
    }
    return (code >= 0LL) ? (int)code : CLI_NO_CODE;
}

int cli_toolchains_words(const CliCommandLineWordsCountCtx *args)
{
    char out_text[CLI_PATH_MAX_LEN * 4u];
    char err_buf[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea out = {out_text, sizeof(out_text), 0ULL};
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ULL};
    const char *const first = (args->count > 0u) ? args->words[0] : NULL;
    const char *const second = (args->count > 1u) ? args->words[1] : NULL;
    if ((first != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "add", .cap = CLI_POOL_BYTES})))
    {
        return EMBED_CALL(toolchains_finish, CommandLineToolchainsCtx,
                          .done = EMBED_CALL(cli_command_line_out_err_msg_internal.toolchain_add,
                                             CliCommandLineOutErrMsgCtx, .words = args->words + 1,
                                             .count = args->count - 1u, .out = &out, .err_msg = &err_msg),
                          .out = &out, .err_msg = &err_msg);
    }
    if ((first == NULL) ||
        ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "check", .cap = CLI_POOL_BYTES})) && (second == NULL)))
    {
        return toolchains_list();
    }
    char folder[CLI_PATH_MAX_LEN] = "";
    if ((args->count == 2u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "add-group", .cap = CLI_POOL_BYTES})))
    {
        mmgr_line_put(&out, "added the group ");
        mmgr_line_put(&out, second);
        return EMBED_CALL(toolchains_finish, CommandLineToolchainsCtx,
                          .done = EMBED_CALL(cli_toolchains_err_msg_name.add_group, CliToolchainsErrMsgNameCfg,
                                             .name = second, .err_msg = &err_msg),
                          .out = &out, .err_msg = &err_msg);
    }
    if ((args->count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "remove", .cap = CLI_POOL_BYTES})))
    {
        mmgr_line_put(&out, "took out ");
        mmgr_line_put(&out, second);
        return EMBED_CALL(toolchains_finish, CommandLineToolchainsCtx,
                          .done = EMBED_CALL(cli_toolchains_id_err_msg.remove, CliToolchainsIdErrMsgCfg, .id = second,
                                             .err_msg = &err_msg),
                          .out = &out, .err_msg = &err_msg);
    }
    if ((args->count == 2u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "remove-group", .cap = CLI_POOL_BYTES})))
    {
        mmgr_line_put(&out, "took out the group ");
        mmgr_line_put(&out, second);
        return EMBED_CALL(toolchains_finish, CommandLineToolchainsCtx,
                          .done = EMBED_CALL(cli_toolchains_err_msg_name.remove_group, CliToolchainsErrMsgNameCfg,
                                             .name = second, .err_msg = &err_msg),
                          .out = &out, .err_msg = &err_msg);
    }
    if ((args->count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "install", .cap = CLI_POOL_BYTES})))
    {
        return EMBED_CALL(toolchains_install, CommandLineToolchainsCtx, .id = second, .out_out = &out,
                          .err_msg_out = &err_msg);
    }
    if ((args->count == 2u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "add-path", .cap = CLI_POOL_BYTES})))
    {
        const int added = EMBED_CALL(cli_toolchains_err_msg_folder.add_to_path, CliToolchainsErrMsgFolderCfg,
                                     .what = second, .folder = folder, .err_msg = &err_msg);
        mmgr_line_put(&out, folder);
        mmgr_line_put(&out, " is on your PATH for every terminal started from now");
        return EMBED_CALL(toolchains_finish, CommandLineToolchainsCtx, .done = added, .out = &out, .err_msg = &err_msg);
    }
    if ((args->count == 3u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "use", .cap = CLI_POOL_BYTES})))
    {
        char program[CLI_PATH_MAX_LEN] = "";
        const int chosen = EMBED_CALL(cli_toolchains_id.choose, CliToolchainsIdCfg, .id = second,
                                      .folder_read = args->words[2], .program = program, .err_msg = &err_msg);
        mmgr_line_put(&out, "impero runs ");
        mmgr_line_put(&out, program);
        return EMBED_CALL(toolchains_finish, CommandLineToolchainsCtx, .done = chosen, .out = &out,
                          .err_msg = &err_msg);
    }
    if ((args->count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "forget", .cap = CLI_POOL_BYTES})))
    {
        mmgr_line_put(&out, "impero looks for ");
        mmgr_line_put(&out, second);
        mmgr_line_put(&out, " on the PATH again");
        return EMBED_CALL(toolchains_finish, CommandLineToolchainsCtx,
                          .done = EMBED_CALL(cli_toolchains_id_err_msg.forget, CliToolchainsIdErrMsgCfg, .id = second,
                                             .err_msg = &err_msg),
                          .out = &out, .err_msg = &err_msg);
    }
    EMBED_CALL(
        cli_command_line_internal.err, CliCommandLineCtx,
        .text =
            "toolchains takes check, install <tool>, add-path <tool|orior>, use <tool> <folder>, forget <tool>, add "
            "<name> --group <group> --program <name,...>, add-group <group>, remove <tool> or remove-group <group>");
    return CLI_WRONG;
}
