// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_tool.c: the calls of the toolchains and the tools that read a file: every toolchain as orior_cli finds it,
// with the reader's groups and orior's own folder; a toolchain or a group added and taken out, its version, the line
// that installs it, a folder put on the PATH, chosen and forgotten; a file validated, the line that runs it, saved as a
// template; the tree's environments and one chosen, the servers notified their settings changed; a text formatted and
// the width its formatter wraps; LaTeX made into pages; a change reviewed by its structure; the prints a change added;
// the tree's settings; and what a file declares of itself and who uses it
#include "../code/prints.h"
#include "../code/purpose.h"
#include "../code/review.h"
#include "../command_line/callback_cfg.h"
#include "../comms/link.h"
#include "../editor/latex.h"
#include "../machine/settings.h"
#include "../machine/trust.h"
#include "../plugins/templates.h"
#include "../plugins/validate.h"
#include "../runs/run_file.h"
#include "../toolchains/envs.h"
#include "../toolchains/format.h"
#include "../toolchains/toolchains.h"
#include "serve_internal.h"

// the most packages one environment is requested
#define CLI_SERVE_REQUIRES 4096u

// a group of the reader's written after those written
static void tool_group_write(const CliEachGroupCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->out[out->at - 1u] == '[') ? "" : ",");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)args->group,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->group, .cap = CLI_CELLS_POOL}));
}

// whether the tree's view saw a file of the name or the ending `kind`, its kinds `context`
static int tool_kind_visit(const CliEachKindCfg *args)
{
    return EMBED_CALL(cli_files_path_kind.contains, CliFilesPathKindCfg, .files = args->context, .path = args->kind);
}

static int toolchains_check(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    (void)err_msg;
    mmgr_line_put(out, "{\"tools\":");
    // the kinds of the open tree's files, as its view saw them, where the call requests which tools the tree uses; none
    // where it does not or no tree is open
    char root[CLI_PATH_MAX_LEN];
    char unopened[CLI_NAME_MAX_LEN];
    VerbaLinea ignored_err = {unopened, sizeof(unopened), 0ull};
    const CliFiles *const kinds = (EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx,
                                              .request = arguments, .key = "uses", .otherwise_i32 = 0) &&
                                   EMBED_CALL(cli_serve_err_msg_request_root_internal.tree,
                                              CliServeErrMsgRequestRootCtx, .root = root, .err_msg = &ignored_err))
                                      ? EMBED_CALL(cli_files_root_paths.kinds_view, CliFilesRootPathsCfg, .root = root)
                                      : NULL;
    EMBED_CALL(cli_toolchains_versions.check, CliToolchainsVersionsCfg, .versions = 0,
               .seen = (kinds != NULL) ? tool_kind_visit : NULL, .context_read = kinds, .out = out);
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = kinds);
    mmgr_line_put(out, ",\"groups\":[");
    EMBED_CALL(cli_toolchains_each.groups, CliToolchainsEachCfg, .each = tool_group_write, .context = out);
    mmgr_line_put(out, "],\"own\":");
    char folder[CLI_PATH_MAX_LEN];
    int on_path = 0;
    char quiet[1024];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    if (EMBED_CALL(cli_toolchains_err_msg_folder_on_path.own, CliToolchainsErrMsgFolderOnPathCfg, .folder = folder,
                   .on_path = &on_path, .err_msg = &ignored))
    {
        mmgr_line_put(out, "{\"folder\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)folder,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, on_path ? ",\"on_path\":true}" : ",\"on_path\":false}");
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_ch(out, '}');
    return 1;
}

static int toolchain_add(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char id[CLI_NAME_MAX_LEN];
    if (!EMBED_CALL(
            cli_toolchains_err_msg.add, CliToolchainsErrMsgCfg, .given = arguments->json,
            .tool = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "tool"),
            .id_out = id, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)id,
               .length = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
    return 1;
}

typedef struct
{
    int done;
    VerbaLinea *out;
    const char *text;
    unsigned long long size;
} ServeCallToolCtx;
// a call of a name that answers with nothing: 1 with `null` written, or 0 with why given
EMBED_INLINE int tool_finish(const ServeCallToolCtx *args)
{
    mmgr_line_put(args->out, args->done ? "null" : "");
    return args->done;
}

static int toolchain_add_group(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    return (name != NULL) &&
           EMBED_CALL(tool_finish, ServeCallToolCtx,
                      .done = EMBED_CALL(cli_toolchains_err_msg_name.add_group, CliToolchainsErrMsgNameCfg,
                                         .name = name, .err_msg = err_msg),
                      .out = out);
}

static int toolchain_remove(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    return (id != NULL) && EMBED_CALL(tool_finish, ServeCallToolCtx,
                                      .done = EMBED_CALL(cli_toolchains_id_err_msg.remove, CliToolchainsIdErrMsgCfg,
                                                         .id = id, .err_msg = err_msg),
                                      .out = out);
}

static int toolchain_remove_group(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    return (name != NULL) &&
           EMBED_CALL(tool_finish, ServeCallToolCtx,
                      .done = EMBED_CALL(cli_toolchains_err_msg_name.remove_group, CliToolchainsErrMsgNameCfg,
                                         .name = name, .err_msg = err_msg),
                      .out = out);
}

static int toolchain_forget(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    return (id != NULL) && EMBED_CALL(tool_finish, ServeCallToolCtx,
                                      .done = EMBED_CALL(cli_toolchains_id_err_msg.forget, CliToolchainsIdErrMsgCfg,
                                                         .id = id, .err_msg = err_msg),
                                      .out = out);
}

// a text a call wrote written as a JSON string, where `done`
EMBED_INLINE int tool_text_free(const ServeCallToolCtx *args)
{
    const char *text = args->text;
    VerbaLinea *out = args->out;
    if (args->done)
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)text, .length = args->size);
    }
    return args->done;
}

static int toolchain_version(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char text[4096];
    VerbaLinea line = {text, sizeof(text), 0ull};
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    const int done = (id != NULL) && EMBED_CALL(cli_toolchains_id_err_msg.version, CliToolchainsIdErrMsgCfg, .id = id,
                                                .out = &line, .err_msg = err_msg);
    return EMBED_CALL(tool_text_free, ServeCallToolCtx, .done = done, .text = text,
                      .size = mmgr_line_ok(&line) ? line.at : 0ull, .out = out);
}

static int toolchain_setup(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char text[4096];
    VerbaLinea line = {text, sizeof(text), 0ull};
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    const int done = (id != NULL) && EMBED_CALL(cli_toolchains_id_err_msg.setup_line, CliToolchainsIdErrMsgCfg,
                                                .id = id, .out = &line, .err_msg = err_msg);
    return EMBED_CALL(tool_text_free, ServeCallToolCtx, .done = done, .text = text,
                      .size = mmgr_line_ok(&line) ? line.at : 0ull, .out = out);
}

static int toolchain_add_path(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    const char *const what = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "what", .err_msg = err_msg);
    const int done =
        (what != NULL) && EMBED_CALL(cli_toolchains_err_msg_folder.add_to_path, CliToolchainsErrMsgFolderCfg,
                                     .what = what, .folder = folder, .err_msg = err_msg);
    return EMBED_CALL(tool_text_free, ServeCallToolCtx, .done = done, .text = folder,
                      .size = done ? cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) : 0ull,
                      .out = out);
}

static int toolchain_use(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char program[CLI_PATH_MAX_LEN];
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    const char *const folder = (id != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "folder", .err_msg = err_msg)
                                            : NULL;
    if ((folder == NULL) || !EMBED_CALL(cli_toolchains_id.choose, CliToolchainsIdCfg, .id = id, .folder_read = folder,
                                        .program = program, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)program,
               .length = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}));
    return 1;
}

static int validate_file(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    char id[CLI_NAME_MAX_LEN];
    const char *const language = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                            .key = "language", .err_msg = err_msg);
    if (language == NULL)
    {
        return 0;
    }
    if (!EMBED_CALL(cli_validate.tool_for, CliValidateCfg, .language = language, .id = id))
    {
        mmgr_line_put(err_msg, "no tool plugin validates ");
        mmgr_line_put(err_msg, language);
        return 0;
    }
    return EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx, .request = arguments,
                      .root = root, .full = full, .err_msg = err_msg) &&
           EMBED_CALL(cli_validate_path.validate, CliValidatePathCfg, .id_read = id, .path = full, .report = args->out,
                      .err_msg = err_msg);
}

static int run_file_line(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const language = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                            .key = "language", .err_msg = err_msg);
    return (language != NULL) &&
           EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx, .request = arguments,
                      .root = root, .full = full, .err_msg = err_msg) &&
           EMBED_CALL(cli_run_file.line, CliRunFileCfg, .root = root, .path = full, .language = language, .out = out,
                      .err_msg = err_msg);
}

// the shell line that verifies the disassembly of the module at the argument `path`, for the terminal to run: this
// program, with the tree as its root, running the console command disasm-verify on the module
static int disasm_verify_line(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    char program[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx, .request = arguments,
                    .root = root, .full = full, .err_msg = err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_path_room_internal.program_own, CliPathRoomCtx, .path_out = program,
                    .room_size = sizeof(program)))
    {
        mmgr_line_put(err_msg, "orior's program could not be found");
        return 0;
    }
    char piece[1024];
    CliJsonQuoting quoting;
    VerbaLinea line;
    EMBED_CALL(cli_json_tree_out.json_quoting_begin, CliJsonTreeOutCfg, .quoting = &quoting, .out = out, .line = &line,
               .piece = piece, .piece_size = sizeof(piece));
    EMBED_CALL(cli_link_out_path.path_word, CliLinkOutPathCfg, .path = program, .out = &line);
    mmgr_line_put(&line, " --root ");
    EMBED_CALL(cli_link_out_path.path_word, CliLinkOutPathCfg, .path = root, .out = &line);
    mmgr_line_put(&line, " run disasm-verify ");
    EMBED_CALL(cli_link_out_path.path_word, CliLinkOutPathCfg, .path = full, .out = &line);
    EMBED_CALL(cli_json_tree_out.json_quoting_end, CliJsonTreeOutCfg, .quoting = &quoting, .line = &line);
    return 1;
}

static int template_save(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char made[CLI_PATH_MAX_LEN];
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    if ((name == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg) ||
        !EMBED_CALL(cli_templates_name.save, CliTemplatesNameCfg, .root = root, .name = name, .made = made,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)made,
               .length = cellul.len(&(CatenaFinitaCfg){.src = made, .cap = CLI_CELLS_POOL}));
    return 1;
}

static int envs_find(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_envs_table.find, CliEnvsCfg, .root = root, .out = out);
    return 1;
}

static int env_use(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    static const char *s_requires[CLI_SERVE_REQUIRES];
    static CliTreeEnvironment s_environment;
    char root[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    const char *const place = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                         .key = "place", .err_msg = err_msg);
    if ((place == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.strings, CliServeErrMsgCtx, .request = arguments, .key = "requires",
                    .items = s_requires, .room = CLI_SERVE_REQUIRES, .count = &count, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    const char *const kind = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                        .key = "kind", .otherwise = NULL);
    if (kind == NULL)
    {
        EMBED_CALL(cli_toolchains_environment_table.set_environment, CliToolchainsEnvironmentCfg, .environment = NULL);
        mmgr_line_put(out, "{\"python\":null,\"missing\":[]}");
    }
    else
    {
        memor.set(&(MemoriaCfg){.dst = &s_environment, .val = 0, .bytes = sizeof(s_environment)});
        if (!EMBED_CALL(cli_envs_root.resolve, CliEnvsRootCfg, .root = root, .kind = kind, .place = place,
                        .environment = &s_environment, .err_msg = err_msg))
        {
            return 0;
        }
        mmgr_line_put(out, "{\"python\":");
        if (s_environment.has_python)
        {
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                       .text_bytes = (const unsigned char *)s_environment.python,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = s_environment.python, .cap = CLI_CELLS_POOL}));
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_put(out, ",\"missing\":");
        EMBED_CALL(cli_envs_out.missing, CliEnvsOutCfg, .environment_read = &s_environment, .requires = s_requires,
                   .count = count, .out = out);
        mmgr_line_ch(out, '}');
        EMBED_CALL(cli_toolchains_environment_table.set_environment, CliToolchainsEnvironmentCfg,
                   .environment = &s_environment);
    }
    cli_servers_settings_change();
    return 1;
}

static int format_text(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    char program[CLI_NAME_MAX_LEN];
    unsigned long long length = 0ull;
    const char *const language = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                            .key = "language", .err_msg = err_msg);
    const char *const text = (language != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                             .request = arguments, .key = "text", .err_msg = err_msg)
                                                : NULL;
    if ((text == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx,
                                      .request = arguments, .root = root, .full = full, .err_msg = err_msg))
    {
        return 0;
    }
    if (EMBED_CALL(cli_format_language.program_get, CliFormatLanguageCfg, .language = language, .program = program) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = program, .other = "prettier", .cap = CLI_CELLS_POOL})) &&
        EMBED_CALL(cli_trust_tree_words.prettier_block, CliTrustTreeWordsCfg, .tree = root))
    {
        mmgr_line_put(err_msg, "Prettier would run the tree's own configuration, and the tree is not trusted: File, "
                               "Trust This Tree.");
        return 0;
    }
    (void)EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "text"),
        .length_pointer = &length);
    return EMBED_CALL(cli_format_path.format, CliFormatPathCfg, .path = full, .language = language, .text = text,
                      .length = length, .out = out, .err_msg = err_msg);
}

static int format_width(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned int width = 0u;
    const char *const language = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                            .key = "language", .err_msg = err_msg);
    if ((language == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx,
                                          .request = arguments, .root = root, .full = full, .err_msg = err_msg))
    {
        return 0;
    }
    if (EMBED_CALL(cli_format_path_language.width, CliFormatPathLanguageCfg, .path = full, .language = language,
                   .width = &width))
    {
        mmgr_line_decimal(args->out, width, 1u);
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    return 1;
}

static int format_margin(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    char by[CLI_NAME_MAX_LEN];
    unsigned int width = 0u;
    const char *const language = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                            .key = "language", .err_msg = err_msg);
    if ((language == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx,
                                          .request = arguments, .root = root, .full = full, .err_msg = err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_format_path_language.margin, CliFormatPathLanguageCfg, .path = full, .language = language,
                    .width = &width, .by = by))
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    mmgr_line_put(out, "{\"width\":");
    mmgr_line_decimal(out, width, 1u);
    mmgr_line_put(out, ",\"by\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)by,
               .length = cellul.len(&(CatenaFinitaCfg){.src = by, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(out, '}');
    return 1;
}

static int latex_make(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ull;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    const char *const text = (path != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "text", .err_msg = err_msg)
                                            : NULL;
    if ((text == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                      .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    (void)EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "text"),
        .length_pointer = &length);
    return EMBED_CALL(cli_latex.make, CliLatexCfg, .root = root, .path = path, .text = text, .length = length,
                      .out = out, .err_msg = err_msg);
}

static int review_structure(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    return EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_review_table.review, CliReviewCfg, .root = root,
                      .commit = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                           .request = arguments, .key = "commit", .otherwise = NULL),
                      .out = out, .err_msg = err_msg);
}

static int prints_add(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    return EMBED_CALL(cli_prints.add, CliPrintsCfg, .root = root, .out = out, .err_msg = err_msg);
}

static int settings_tree(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    CliChain *const chain = g_cli_cells->temporary_buf_alloc(sizeof(*chain), &g_cli_settings_cells);
    if (chain == NULL)
    {
        mmgr_line_put(err_msg, "the cell a tree's settings are read into could not be taken");
        return 0;
    }
    EMBED_CALL(cli_settings_path_table.tree, CliSettingsPathCfg, .root = root, .chain = chain);
    EMBED_CALL(cli_settings_table.chain_write, CliSettingsCfg, .chain_read = chain, .out = out);
    EMBED_CALL(cli_settings_path_table.chain_free, CliSettingsPathCfg, .chain = chain);
    g_cli_cells->persistent_buf_release(chain, &g_cli_settings_cells);
    return 1;
}

static int file_purpose(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    return (path != NULL) &&
           EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_purpose_table.write, CliPurposeCfg, .root = root, .path = path, .out = out,
                      .err_msg = err_msg);
}

static int file_users(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    return (path != NULL) &&
           EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_purpose_table.users, CliPurposeCfg, .root = root, .path = path, .out = out,
                      .err_msg = err_msg);
}

const CliServeCall g_cli_serve_tool_calls[] = {
    {"toolchains_check", toolchains_check},
    {"toolchain_add", toolchain_add},
    {"toolchain_add_group", toolchain_add_group},
    {"toolchain_remove", toolchain_remove},
    {"toolchain_remove_group", toolchain_remove_group},
    {"toolchain_version", toolchain_version},
    {"toolchain_setup", toolchain_setup},
    {"toolchain_add_path", toolchain_add_path},
    {"toolchain_use", toolchain_use},
    {"toolchain_forget", toolchain_forget},
    {"validate_file", validate_file},
    {"run_file_line", run_file_line},
    {"disasm_verify_line", disasm_verify_line},
    {"template_save", template_save},
    {"envs_found", envs_find},
    {"env_use", env_use},
    {"format_text", format_text},
    {"format_width", format_width},
    {"format_margin", format_margin},
    {"latex_make", latex_make},
    {"review_structure", review_structure},
    {"prints_added", prints_add},
    {"settings_tree", settings_tree},
    {"file_purpose", file_purpose},
    {"file_users", file_users},
};
const unsigned int g_cli_serve_tool_count = sizeof(g_cli_serve_tool_calls) / sizeof(g_cli_serve_tool_calls[0]);
