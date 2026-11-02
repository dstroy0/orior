// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link_prepare.c: orior's server put on another machine where it is not there: the machine queried what it is, the
// server for it found or built here, sent under a name of its size and the time it was written, and the line that
// starts it on the tree given
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../runs/runner.h"
#include "../text/json_tree.h"
#include "link.h"

#include "../text/text.h"

// the most lines of a failed build reported
#define CLI_LINK_BUILD_LINES 6u

int cli_link_is_windows(const CliLinkErrMsgSystemCfg *args)
{
    static const char *const starts[4] = {"MINGW", "MSYS", "CYGWIN", "Windows"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->system, .other_pointer = starts[at],
                       .size = cellul.len(&(CatenaFinitaCfg){.src = starts[at], .cap = CLI_CELLS_POOL})))
        {
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    const char *system;
    unsigned long long *at;
    const char **word;
    unsigned long long *size;
    const char *word_value;
    unsigned long long size_value;
    const char *name;
    const CliRan *ran;
    VerbaLinea *err_msg;
    const char *target;
    CliLinkStated stated;
    void *context;
    char *path;
    const CliLinkAddress *address;
    char *system_out;
    char *home;
} LinkPrepareCtx;
// the word of `system` at `*at` past the spaces before it, its start and length; `*at` past it
EMBED_INLINE void prepare_word(const LinkPrepareCtx *args)
{
    unsigned long long *at = args->at;
    const char **word = args->word;
    unsigned long long *size = args->size;
    *at +=
        EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = args->system + *at, .at = 0ull,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->system + *at, .cap = CLI_CELLS_POOL}),
                   .set = CLI_TEXT_WHITE);
    *word = args->system + *at;
    *at += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->system + *at, .at = 0ull,
                      .size = cellul.len(&(CatenaFinitaCfg){.src = args->system + *at, .cap = CLI_CELLS_POOL}),
                      .set = CLI_TEXT_WHITE);
    *size = (unsigned long long)(args->system + *at - *word);
}

// whether `size` bytes of `word` are `name`
EMBED_INLINE int prepare_is(const LinkPrepareCtx *args)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) == args->size_value) &&
           (memor.cmp(&(MemoriaCfg){.src = args->word_value, .other = args->name, .bytes = (size_t)args->size_value}) ==
            0);
}

int cli_link_target(const CliLinkErrMsgSystemCfg *args)
{
    unsigned long long at = 0ull;
    const char *kind = NULL;
    const char *arch = NULL;
    unsigned long long kind_size = 0ull;
    unsigned long long arch_size = 0ull;
    EMBED_CALL(prepare_word, LinkPrepareCtx, .system = args->system, .at = &at, .word = &kind, .size = &kind_size);
    EMBED_CALL(prepare_word, LinkPrepareCtx, .system = args->system, .at = &at, .word = &arch, .size = &arch_size);
    const char *const named =
        (EMBED_CALL(prepare_is, LinkPrepareCtx, .word_value = arch, .size_value = arch_size, .name = "x86_64") ||
         EMBED_CALL(prepare_is, LinkPrepareCtx, .word_value = arch, .size_value = arch_size, .name = "amd64"))
            ? "x86_64"
        : (EMBED_CALL(prepare_is, LinkPrepareCtx, .word_value = arch, .size_value = arch_size, .name = "aarch64") ||
           EMBED_CALL(prepare_is, LinkPrepareCtx, .word_value = arch, .size_value = arch_size, .name = "arm64"))
            ? "aarch64"
            : NULL;
    if (named == NULL)
    {
        mmgr_line_put(args->err_msg, "orior_cli builds no server for ");
        mmgr_line_put_n(args->err_msg, arch, arch_size);
        return 0;
    }
    if (!EMBED_CALL(prepare_is, LinkPrepareCtx, .word_value = kind, .size_value = kind_size, .name = "Linux"))
    {
        mmgr_line_put(args->err_msg, "orior_cli builds no server for ");
        mmgr_line_put_n(args->err_msg, kind, kind_size);
        mmgr_line_put(args->err_msg, " from this machine");
        return 0;
    }
    VerbaLinea line = {args->target, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&line, named);
    mmgr_line_put(&line, "-unknown-linux-musl");
    mmgr_line_finish(&line);
    return 1;
}

// the last lines of what a build wrote to its errors reported
EMBED_INLINE void prepare_build_err_write(const LinkPrepareCtx *args)
{
    unsigned long long start = args->ran->err_size;
    unsigned int lines = 0u;
    start = EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = args->ran->err, .at = start,
                       .set = "\n");
    const unsigned long long end = start;
    while (start > 0u)
    {
        if ((args->ran->err[start - 1u] == '\n') && (lines + 1u == CLI_LINK_BUILD_LINES))
        {
            break;
        }
        lines += (args->ran->err[start - 1u] == '\n') ? 1u : 0u;
        start -= 1u;
    }
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
               .text_bytes = (const unsigned char *)args->ran->err + start, .length = end - start);
}

// orior's server built here for `target` from the tree's own sources, its path into `path`: 1, or 0 with why given
EMBED_INLINE int prepare_build(const LinkPrepareCtx *args)
{
    char *path = args->path;
    VerbaLinea *err_msg = args->err_msg;
    char tree[CLI_PATH_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    VerbaLinea script_line = {script, sizeof(script), 0ull};
    const int found = EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = tree);
    if (found)
    {
        mmgr_line_put(&script_line, tree);
        mmgr_line_put(&script_line, "/ui/cli/build.sh");
    }
    if (!found || (mmgr_line_finish(&script_line) == 0ull) ||
        !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = script))
    {
        mmgr_line_put(err_msg, "no orior_cli server for ");
        mmgr_line_put(err_msg, args->target);
        mmgr_line_put(err_msg, " is beside this program, under servers/");
        mmgr_line_put(err_msg, args->target);
        mmgr_line_put(err_msg, "/orior_cli");
        return 0;
    }
    char msg_buf[CLI_NAME_MAX_LEN + 64u];
    VerbaLinea msg = {msg_buf, sizeof(msg_buf), 0ull};
    mmgr_line_put(&msg, "Building orior's server for ");
    mmgr_line_put(&msg, args->target);
    mmgr_line_finish(&msg);
    EMBED_CALL(args->stated, CliLinkStatedCfg, .context = args->context, .state = "preparing", .detail = msg_buf);
    char bash[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_runner_name.bash, CliRunnerNameCfg, .path = bash, .err_msg = err_msg))
    {
        return 0;
    }
    VerbaLinea built = {path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&built, tree);
    mmgr_line_put(&built, "/build/cli/");
    mmgr_line_put(&built, args->target);
    mmgr_line_put(&built, "/orior_cli");
    mmgr_line_finish(&built);
    const char *const words[4] = {bash, script, args->target, NULL};
    CliRan *const ran = cli_ran_take();
    int done = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = tree, .settings = NULL,
                          .count = 0u, .ran = ran, .err_msg = err_msg);
    if (done &&
        ((ran->code != 0) || !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path)))
    {
        EMBED_CALL(prepare_build_err_write, LinkPrepareCtx, .ran = ran, .err_msg = err_msg);
        done = 0;
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return done;
}

// orior's server for a machine `uname -sm` reports is `system`, its path into `path`: ORIOR_SERVER where it is set; on
// Windows this program, which serves as well; elsewhere one built for the machine's target, beside this program under
// servers/, or built from the tree's own sources for it. 1, or 0 with why given
EMBED_INLINE int prepare_server(const LinkPrepareCtx *args)
{
    const char *system = args->system;
    char *path = args->path;
    VerbaLinea *err_msg = args->err_msg;
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_SERVER",
                   .value_out = path, .room = CLI_PATH_MAX_LEN))
    {
        return 1;
    }
    char program[CLI_PATH_MAX_LEN];
    const int known = qry_member_program(program, sizeof(program));
    if (EMBED_CALL(cli_link_err_msg_system.is_windows, CliLinkErrMsgSystemCfg, .system = system))
    {
        if (!known)
        {
            mmgr_line_put(err_msg, "this program's path is not known");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = path, .src = program, .bytes = sizeof(program)});
        return 1;
    }
    char target[CLI_NAME_MAX_LEN];
    if (!EMBED_CALL(cli_link_err_msg_system.target, CliLinkErrMsgSystemCfg, .system = system, .target = target,
                    .err_msg = err_msg))
    {
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea beside = {path, CLI_PATH_MAX_LEN, 0ull};
    if (known && EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = program, .out = folder))
    {
        mmgr_line_put(&beside, folder);
        mmgr_line_put(&beside, "/servers/");
        mmgr_line_put(&beside, target);
        mmgr_line_put(&beside, "/orior_cli");
        if ((mmgr_line_finish(&beside) != 0ull) &&
            EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
        {
            return 1;
        }
    }
    return EMBED_CALL(prepare_build, LinkPrepareCtx, .target = target, .stated = args->stated, .context = args->context,
                      .path = path, .err_msg = err_msg);
}

// the line `uname -sm` and the server's home on the machine, queried from it, into `system` and `home`, which each are
// sized for CLI_PATH_MAX_LEN: 1, or 0 with why given
EMBED_INLINE int prepare_probe(const LinkPrepareCtx *args)
{
    const CliLinkAddress *address = args->address;
    VerbaLinea *err_msg = args->err_msg;
    char probe[2u * CLI_PATH_MAX_LEN];
    VerbaLinea probe_line = {probe, sizeof(probe), 0ull};
    if (!EMBED_CALL(cli_link_size.remote_run, CliLinkSizeCfg, .address = address,
                    .line = "uname -sm; printf '%s\\n' \"${ORIOR_SERVER_HOME:-$HOME/.orior/server}\"", .input = NULL,
                    .size = 0ull, .out = &probe_line, .err_msg = err_msg))
    {
        return 0;
    }
    char *const fields[2] = {args->system_out, args->home};
    unsigned long long at = 0ull;
    for (unsigned int field = 0u; field < 2u; field += 1u)
    {
        unsigned long long end = at;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = probe, .at = end,
                         .size = probe_line.at, .set = "\n");
        unsigned long long start = at;
        unsigned long long stop = end;
        while ((start < stop) && (cellul.chr(&(CatenaFinitaCfg){
                                      .src = " \t\r\v\f", .cap = CLI_CELLS_POOL, .byte = probe[start]}) != NULL))
        {
            start += 1u;
        }
        while ((stop > start) && (cellul.chr(&(CatenaFinitaCfg){
                                      .src = " \t\r\v\f", .cap = CLI_CELLS_POOL, .byte = probe[stop - 1u]}) != NULL))
        {
            stop -= 1u;
        }
        const unsigned long long size = (stop - start < CLI_PATH_MAX_LEN) ? (stop - start) : (CLI_PATH_MAX_LEN - 1u);
        memor.cpy(&(MemoriaCfg){.dst = fields[field], .src = probe + start, .bytes = (size_t)size});
        fields[field][size] = '\0';
        at = (end < probe_line.at) ? (end + 1u) : end;
    }
    return 1;
}

int cli_link_prepare(const CliLinkErrMsgCfg *args)
{
    char system[CLI_PATH_MAX_LEN];
    char home[CLI_PATH_MAX_LEN];
    char local[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ull;
    unsigned long long changed = 0ull;
    if (!EMBED_CALL(prepare_probe, LinkPrepareCtx, .address = args->address, .system_out = system, .home = home,
                    .err_msg = args->err_msg) ||
        !EMBED_CALL(prepare_server, LinkPrepareCtx, .system = system, .stated = args->stated, .context = args->context,
                    .path = local, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_path_size_internal.file_stamp, CliPathSizeCtx, .path = local, .size_pointer = &size,
                    .changed = &changed))
    {
        mmgr_line_put(args->err_msg, local);
        mmgr_line_put(args->err_msg, ": it could not be read");
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea folder_line = {folder, sizeof(folder), 0ull};
    mmgr_line_put(&folder_line, home);
    mmgr_line_put(&folder_line, "/" CLI_VERSION "-");
    mmgr_line_decimal(&folder_line, size, 1u);
    mmgr_line_ch(&folder_line, '-');
    mmgr_line_decimal(&folder_line, changed / 1000u, 1u);
    mmgr_line_finish(&folder_line);
    char program[CLI_PATH_MAX_LEN + 128u];
    VerbaLinea program_line = {program, sizeof(program), 0ull};
    mmgr_line_put(&program_line, folder);
    mmgr_line_put(&program_line,
                  EMBED_CALL(cli_link_err_msg_system.is_windows, CliLinkErrMsgSystemCfg, .system = system)
                      ? "/orior_cli.exe"
                      : "/orior_cli");
    mmgr_line_finish(&program_line);
    char line[(4u * CLI_PATH_MAX_LEN) + 128u];
    VerbaLinea test = {line, sizeof(line), 0ull};
    mmgr_line_put(&test, "test -x ");
    EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = program,
               .size = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}), .out = &test);
    mmgr_line_finish(&test);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea none = {why, sizeof(why), 0ull};
    VerbaLinea nothing = {NULL, 0ull, 0ull};
    if (!EMBED_CALL(cli_link_size.remote_run, CliLinkSizeCfg, .address = args->address, .line = line, .input = NULL,
                    .size = 0ull, .out = &nothing, .err_msg = &none))
    {
        char msg_buf[CLI_PATH_MAX_LEN + 64u];
        VerbaLinea msg = {msg_buf, sizeof(msg_buf), 0ull};
        mmgr_line_put(&msg, "Sending orior's server to ");
        EMBED_CALL(cli_link_out.machine, CliLinkOutCfg, .address = args->address, .out = &msg);
        mmgr_line_finish(&msg);
        EMBED_CALL(args->stated, CliLinkStatedCfg, .context = args->context, .state = "preparing", .detail = msg_buf);
        unsigned long long bytes_size = 0ull;
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = local,
                                                      .size_pointer = &bytes_size, .owner = &g_cli_comms_cells);
        if (bytes == NULL)
        {
            mmgr_line_put(args->err_msg, local);
            mmgr_line_put(args->err_msg, ": it could not be read");
            return 0;
        }
        char part[CLI_PATH_MAX_LEN + 136u];
        VerbaLinea part_line = {part, sizeof(part), 0ull};
        mmgr_line_put(&part_line, program);
        mmgr_line_put(&part_line, ".part");
        mmgr_line_finish(&part_line);
        VerbaLinea send = {line, sizeof(line), 0ull};
        mmgr_line_put(&send, "mkdir -p ");
        EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = folder,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), .out = &send);
        mmgr_line_put(&send, " && cat > ");
        EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = part,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL}), .out = &send);
        mmgr_line_put(&send, " && chmod +x ");
        EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = part,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL}), .out = &send);
        mmgr_line_put(&send, " && mv -f ");
        EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = part,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL}), .out = &send);
        mmgr_line_ch(&send, ' ');
        EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = program,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}), .out = &send);
        mmgr_line_finish(&send);
        const int sent =
            EMBED_CALL(cli_link_size.remote_run, CliLinkSizeCfg, .address = args->address, .line = line,
                       .input = (const char *)bytes, .size = bytes_size, .out = &nothing, .err_msg = args->err_msg);
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_comms_cells);
        if (!sent)
        {
            return 0;
        }
    }
    mmgr_line_put(args->start, "exec ");
    EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = program,
               .size = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}), .out = args->start);
    mmgr_line_put(args->start, " serve --root ");
    EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = args->address->folder,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->address->folder, .cap = CLI_CELLS_POOL}),
               .out = args->start);
    return 1;
}
