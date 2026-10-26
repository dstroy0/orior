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
static void tool_group_written(void *context, const char *group)
{
    VerbaLinea *const out = context;
    mmgr_line_put(out, (out->out[out->at - 1u] == '[') ? "" : ",");
    cli_json_quoted_lossy(out, (const unsigned char *)group,
                          cellul.len(&(CatenaFinitaCfg){.src = group, .cap = CLI_CELLS_POOL}));
}

// whether the tree's view saw a file of the name or the ending `kind`, its kinds `context`
static int tool_kind_seen(const void *context, const char *kind)
{
    return cli_files_contains(context, kind);
}

static int toolchains_check(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)err_msg;
    mmgr_line_put(out, "{\"tools\":");
    // the kinds of the open tree's files, as its view saw them, where the call requests which tools the tree uses; none
    // where it does not or no tree is open
    char root[CLI_PATH_MAX_LEN];
    char unopened[CLI_NAME_MAX_LEN];
    VerbaLinea ignored_err = {unopened, sizeof(unopened), 0ull};
    const CliFiles *const kinds =
        (cli_serve_bool_or(args, "uses", 0) && cli_serve_tree(root, &ignored_err)) ? cli_files_kinds_view(root) : NULL;
    cli_toolchains_check(0, (kinds != NULL) ? tool_kind_seen : NULL, kinds, out);
    cli_files_let(kinds);
    mmgr_line_put(out, ",\"groups\":[");
    cli_toolchains_groups(tool_group_written, out);
    mmgr_line_put(out, "],\"own\":");
    char folder[CLI_PATH_MAX_LEN];
    int on_path = 0;
    char quiet[1024];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    if (cli_toolchains_own(folder, &on_path, &ignored))
    {
        mmgr_line_put(out, "{\"folder\":");
        cli_json_quoted_lossy(out, (const unsigned char *)folder,
                              cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, on_path ? ",\"on_path\":true}" : ",\"on_path\":false}");
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_ch(out, '}');
    return 1;
}

static int toolchain_add(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char id[CLI_NAME_MAX_LEN];
    if (!cli_toolchains_add(args->json, cli_serve_arg(args, "tool"), id, err_msg))
    {
        return 0;
    }
    cli_json_quoted_lossy(out, (const unsigned char *)id,
                          cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
    return 1;
}

// a call of a name that answers with nothing: 1 with `null` written, or 0 with why given
static int tool_done(int done, VerbaLinea *out)
{
    mmgr_line_put(out, done ? "null" : "");
    return done;
}

static int toolchain_add_group(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const name = cli_serve_string(args, "name", err_msg);
    return (name != NULL) && tool_done(cli_toolchains_add_group(name, err_msg), out);
}

static int toolchain_remove(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const id = cli_serve_string(args, "id", err_msg);
    return (id != NULL) && tool_done(cli_toolchains_remove(id, err_msg), out);
}

static int toolchain_remove_group(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const name = cli_serve_string(args, "name", err_msg);
    return (name != NULL) && tool_done(cli_toolchains_remove_group(name, err_msg), out);
}

static int toolchain_forget(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const id = cli_serve_string(args, "id", err_msg);
    return (id != NULL) && tool_done(cli_toolchains_forget(id, err_msg), out);
}

// a text a call wrote written as a JSON string, where `done`
static int tool_text_given(int done, const char *text, unsigned long long size, VerbaLinea *out)
{
    if (done)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)text, size);
    }
    return done;
}

static int toolchain_version(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char text[4096];
    VerbaLinea line = {text, sizeof(text), 0ull};
    const char *const id = cli_serve_string(args, "id", err_msg);
    const int done = (id != NULL) && cli_toolchains_version(id, &line, err_msg);
    return tool_text_given(done, text, mmgr_line_ok(&line) ? line.at : 0ull, out);
}

static int toolchain_setup(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char text[4096];
    VerbaLinea line = {text, sizeof(text), 0ull};
    const char *const id = cli_serve_string(args, "id", err_msg);
    const int done = (id != NULL) && cli_toolchains_setup_line(id, &line, err_msg);
    return tool_text_given(done, text, mmgr_line_ok(&line) ? line.at : 0ull, out);
}

static int toolchain_add_path(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    const char *const what = cli_serve_string(args, "what", err_msg);
    const int done = (what != NULL) && cli_toolchains_add_to_path(what, folder, err_msg);
    return tool_text_given(done, folder,
                           done ? cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) : 0ull, out);
}

static int toolchain_use(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char program[CLI_PATH_MAX_LEN];
    const char *const id = cli_serve_string(args, "id", err_msg);
    const char *const folder = (id != NULL) ? cli_serve_string(args, "folder", err_msg) : NULL;
    if ((folder == NULL) || !cli_toolchains_choose(id, folder, program, err_msg))
    {
        return 0;
    }
    cli_json_quoted_lossy(out, (const unsigned char *)program,
                          cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}));
    return 1;
}

static int validate_file(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    char id[CLI_NAME_MAX_LEN];
    const char *const language = cli_serve_string(args, "language", err_msg);
    if (language == NULL)
    {
        return 0;
    }
    if (!cli_validate_tool_for(language, id))
    {
        mmgr_line_put(err_msg, "no tool plugin validates ");
        mmgr_line_put(err_msg, language);
        return 0;
    }
    return cli_serve_at(args, root, full, err_msg) && cli_validated(id, full, out, err_msg);
}

static int run_file_line(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const language = cli_serve_string(args, "language", err_msg);
    return (language != NULL) && cli_serve_at(args, root, full, err_msg) &&
           cli_run_file_line(root, full, language, out, err_msg);
}

// the shell line that verifies the disassembly of the module at the argument `path`, for the terminal to run: this
// program, with the tree as its root, running the console command disasm-verify on the module
static int disasm_verify_line(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    char program[CLI_PATH_MAX_LEN];
    if (!cli_serve_at(args, root, full, err_msg))
    {
        return 0;
    }
    if (!cli_program_own(program, sizeof(program)))
    {
        mmgr_line_put(err_msg, "orior's program could not be found");
        return 0;
    }
    char piece[1024];
    CliJsonQuoting quoting;
    VerbaLinea line;
    cli_json_quoting_begun(&quoting, out, &line, piece, sizeof(piece));
    cli_link_path_word(program, &line);
    mmgr_line_put(&line, " --root ");
    cli_link_path_word(root, &line);
    mmgr_line_put(&line, " run disasm-verify ");
    cli_link_path_word(full, &line);
    cli_json_quoting_ended(&quoting, &line);
    return 1;
}

static int template_save(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char made[CLI_PATH_MAX_LEN];
    const char *const name = cli_serve_string(args, "name", err_msg);
    if ((name == NULL) || !cli_serve_tree(root, err_msg) || !cli_templates_save(root, name, made, err_msg))
    {
        return 0;
    }
    cli_json_quoted_lossy(out, (const unsigned char *)made,
                          cellul.len(&(CatenaFinitaCfg){.src = made, .cap = CLI_CELLS_POOL}));
    return 1;
}

static int envs_found(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    char root[CLI_PATH_MAX_LEN];
    if (!cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    cli_envs_found(root, out);
    return 1;
}

static int env_use(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    static const char *s_requires[CLI_SERVE_REQUIRES];
    static CliTreeEnvironment s_environment;
    static CliLock s_lock = CLI_LOCK_INIT;
    char root[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    const char *const place = cli_serve_string(args, "place", err_msg);
    if ((place == NULL) || !cli_serve_strings(args, "requires", s_requires, CLI_SERVE_REQUIRES, &count, err_msg) ||
        !cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    const char *const kind = cli_serve_string_or(args, "kind", NULL);
    cli_lock_take(&s_lock);
    if (kind == NULL)
    {
        cli_toolchains_set_environment(NULL);
        mmgr_line_put(out, "{\"python\":null,\"missing\":[]}");
    }
    else
    {
        memor.set(&(MemoriaCfg){.dst = &s_environment, .val = 0, .bytes = sizeof(s_environment)});
        if (!cli_envs_resolve(root, kind, place, &s_environment, err_msg))
        {
            cli_lock_give(&s_lock);
            return 0;
        }
        mmgr_line_put(out, "{\"python\":");
        if (s_environment.has_python)
        {
            cli_json_quoted_lossy(out, (const unsigned char *)s_environment.python,
                                  cellul.len(&(CatenaFinitaCfg){.src = s_environment.python, .cap = CLI_CELLS_POOL}));
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_put(out, ",\"missing\":");
        cli_envs_missing(&s_environment, s_requires, count, out);
        mmgr_line_ch(out, '}');
        cli_toolchains_set_environment(&s_environment);
    }
    cli_lock_give(&s_lock);
    cli_servers_settings_changed();
    return 1;
}

static int format_text(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    char program[CLI_NAME_MAX_LEN];
    unsigned long long length = 0ull;
    const char *const language = cli_serve_string(args, "language", err_msg);
    const char *const text = (language != NULL) ? cli_serve_string(args, "text", err_msg) : NULL;
    if ((text == NULL) || !cli_serve_at(args, root, full, err_msg))
    {
        return 0;
    }
    if (cli_format_program_of(language, program) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = program, .other = "prettier", .cap = CLI_CELLS_POOL})) &&
        cli_trust_prettier_blocked(root))
    {
        mmgr_line_put(err_msg, "Prettier would run the tree's own configuration, and the tree is not trusted: File, "
                               "Trust This Tree.");
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &length);
    return cli_format(full, language, text, length, out, err_msg);
}

static int format_width(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned int width = 0u;
    const char *const language = cli_serve_string(args, "language", err_msg);
    if ((language == NULL) || !cli_serve_at(args, root, full, err_msg))
    {
        return 0;
    }
    if (cli_format_width(full, language, &width))
    {
        mmgr_line_decimal(out, width, 1u);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    return 1;
}

static int format_margin(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    char by[CLI_NAME_MAX_LEN];
    unsigned int width = 0u;
    const char *const language = cli_serve_string(args, "language", err_msg);
    if ((language == NULL) || !cli_serve_at(args, root, full, err_msg))
    {
        return 0;
    }
    if (!cli_format_margin(full, language, &width, by))
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    mmgr_line_put(out, "{\"width\":");
    mmgr_line_decimal(out, width, 1u);
    mmgr_line_put(out, ",\"by\":");
    cli_json_quoted_lossy(out, (const unsigned char *)by,
                          cellul.len(&(CatenaFinitaCfg){.src = by, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(out, '}');
    return 1;
}

static int latex_make(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ull;
    const char *const path = cli_serve_string(args, "path", err_msg);
    const char *const text = (path != NULL) ? cli_serve_string(args, "text", err_msg) : NULL;
    if ((text == NULL) || !cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &length);
    return cli_latex_make(root, path, text, length, out, err_msg);
}

static int review_structure(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    return cli_serve_tree(root, err_msg) && cli_review(root, cli_serve_string_or(args, "commit", NULL), out, err_msg);
}

static int prints_added(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    char root[CLI_PATH_MAX_LEN];
    if (!cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    return cli_prints_added(root, out, err_msg);
}

static int settings_tree(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    char root[CLI_PATH_MAX_LEN];
    if (!cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    CliChain *const chain = g_cli_cells->persistent_buf_alloc(sizeof(*chain), &g_cli_settings_cells);
    if (chain == NULL)
    {
        mmgr_line_put(err_msg, "the cell a tree's settings are read into could not be taken");
        return 0;
    }
    cli_settings_tree(root, chain);
    cli_settings_chain_written(chain, out);
    cli_settings_chain_given(chain);
    g_cli_cells->persistent_buf_release(chain, &g_cli_settings_cells);
    return 1;
}

static int file_purpose(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    const char *const path = cli_serve_string(args, "path", err_msg);
    return (path != NULL) && cli_serve_tree(root, err_msg) && cli_purpose_written(root, path, out, err_msg);
}

static int file_users(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    const char *const path = cli_serve_string(args, "path", err_msg);
    return (path != NULL) && cli_serve_tree(root, err_msg) && cli_purpose_users(root, path, out, err_msg);
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
    {"envs_found", envs_found},
    {"env_use", env_use},
    {"format_text", format_text},
    {"format_width", format_width},
    {"format_margin", format_margin},
    {"latex_make", latex_make},
    {"review_structure", review_structure},
    {"prints_added", prints_added},
    {"settings_tree", settings_tree},
    {"file_purpose", file_purpose},
    {"file_users", file_users},
};
const unsigned int g_cli_serve_tool_count = sizeof(g_cli_serve_tool_calls) / sizeof(g_cli_serve_tool_calls[0]);
