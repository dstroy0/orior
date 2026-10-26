// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link_prepare.c: orior's server put on another machine where it is not there: the machine queried what it is, the
// server for it found or built here, sent under a name of its size and the time it was written, and the line that
// starts it on the tree given
#include "../runs/runner.h"
#include "../text/json_tree.h"
#include "link.h"

#include "../text/text.h"

// the most lines of a failed build reported
#define CLI_LINK_BUILD_LINES 6u

int cli_link_is_windows(const char *system)
{
    static const char *const starts[4] = {"MINGW", "MSYS", "CYGWIN", "Windows"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (cli_text_same(system, starts[at], cellul.len(&(CatenaFinitaCfg){.src = starts[at], .cap = CLI_CELLS_POOL})))
        {
            return 1;
        }
    }
    return 0;
}

// the word of `system` at `*at` past the spaces before it, its start and length; `*at` past it
static void prepare_word(const char *system, unsigned long long *at, const char **word, unsigned long long *size)
{
    while ((system[*at] != '\0') &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n\v\f", .cap = CLI_CELLS_POOL, .byte = system[*at]}) != NULL))
    {
        *at += 1u;
    }
    *word = system + *at;
    while ((system[*at] != '\0') &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n\v\f", .cap = CLI_CELLS_POOL, .byte = system[*at]}) == NULL))
    {
        *at += 1u;
    }
    *size = (unsigned long long)(system + *at - *word);
}

// whether `size` bytes of `word` are `name`
static int prepare_is(const char *word, unsigned long long size, const char *name)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) == size) &&
           (memor.cmp(&(MemoriaCfg){.src = word, .other = name, .bytes = (size_t)size}) == 0);
}

int cli_link_target(const char *system, char *target, VerbaLinea *err_msg)
{
    unsigned long long at = 0ull;
    const char *kind = NULL;
    const char *arch = NULL;
    unsigned long long kind_size = 0ull;
    unsigned long long arch_size = 0ull;
    prepare_word(system, &at, &kind, &kind_size);
    prepare_word(system, &at, &arch, &arch_size);
    const char *const named = (prepare_is(arch, arch_size, "x86_64") || prepare_is(arch, arch_size, "amd64")) ? "x86_64"
                              : (prepare_is(arch, arch_size, "aarch64") || prepare_is(arch, arch_size, "arm64"))
                                  ? "aarch64"
                                  : NULL;
    if (named == NULL)
    {
        mmgr_line_put(err_msg, "orior_cli builds no server for ");
        mmgr_line_put_n(err_msg, arch, arch_size);
        return 0;
    }
    if (!prepare_is(kind, kind_size, "Linux"))
    {
        mmgr_line_put(err_msg, "orior_cli builds no server for ");
        mmgr_line_put_n(err_msg, kind, kind_size);
        mmgr_line_put(err_msg, " from this machine");
        return 0;
    }
    VerbaLinea line = {target, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&line, named);
    mmgr_line_put(&line, "-unknown-linux-musl");
    mmgr_line_finish(&line);
    return 1;
}

// the last lines of what a build wrote to its errors reported
static void prepare_build_err_written(const CliRan *ran, VerbaLinea *err_msg)
{
    unsigned long long start = ran->err_size;
    unsigned int lines = 0u;
    while ((start > 0u) && (ran->err[start - 1u] == '\n'))
    {
        start -= 1u;
    }
    const unsigned long long end = start;
    while (start > 0u)
    {
        if ((ran->err[start - 1u] == '\n') && (lines + 1u == CLI_LINK_BUILD_LINES))
        {
            break;
        }
        lines += (ran->err[start - 1u] == '\n') ? 1u : 0u;
        start -= 1u;
    }
    cli_lossy(err_msg, (const unsigned char *)ran->err + start, end - start);
}

// orior's server built here for `target` from the tree's own sources, its path into `path`: 1, or 0 with why given
static int prepare_built(const char *target, CliLinkStated stated, void *context, char *path, VerbaLinea *err_msg)
{
    char tree[CLI_PATH_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    VerbaLinea script_line = {script, sizeof(script), 0ull};
    const int found = cli_tree_found(tree);
    if (found)
    {
        mmgr_line_put(&script_line, tree);
        mmgr_line_put(&script_line, "/ui/cli/build.sh");
    }
    if (!found || (mmgr_line_finish(&script_line) == 0ull) || !query_is_file(script))
    {
        mmgr_line_put(err_msg, "no orior_cli server for ");
        mmgr_line_put(err_msg, target);
        mmgr_line_put(err_msg, " is beside this program, under servers/");
        mmgr_line_put(err_msg, target);
        mmgr_line_put(err_msg, "/orior_cli");
        return 0;
    }
    char msg_buf[CLI_NAME_MAX_LEN + 64u];
    VerbaLinea msg = {msg_buf, sizeof(msg_buf), 0ull};
    mmgr_line_put(&msg, "Building orior's server for ");
    mmgr_line_put(&msg, target);
    mmgr_line_finish(&msg);
    stated(context, "preparing", msg_buf);
    char bash[CLI_PATH_MAX_LEN];
    if (!cli_runner_bash(bash, err_msg))
    {
        return 0;
    }
    VerbaLinea built = {path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&built, tree);
    mmgr_line_put(&built, "/build/cli/");
    mmgr_line_put(&built, target);
    mmgr_line_put(&built, "/orior_cli");
    mmgr_line_finish(&built);
    const char *const words[4] = {bash, script, target, NULL};
    CliRan *const ran = cli_ran_taken();
    int done = cli_ran(words, tree, NULL, 0u, ran, err_msg);
    if (done && ((ran->code != 0) || !query_is_file(path)))
    {
        prepare_build_err_written(ran, err_msg);
        done = 0;
    }
    cli_ran_given(ran);
    return done;
}

// orior's server for a machine `uname -sm` reports is `system`, its path into `path`: ORIOR_SERVER where it is set; on
// Windows this program, which serves as well; elsewhere one built for the machine's target, beside this program under
// servers/, or built from the tree's own sources for it. 1, or 0 with why given
static int prepare_server(const char *system, CliLinkStated stated, void *context, char *path, VerbaLinea *err_msg)
{
    if (query_environment_read("ORIOR_SERVER", path, CLI_PATH_MAX_LEN))
    {
        return 1;
    }
    char program[CLI_PATH_MAX_LEN];
    const int known = qry_member_program(program, sizeof(program));
    if (cli_link_is_windows(system))
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
    if (!cli_link_target(system, target, err_msg))
    {
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea beside = {path, CLI_PATH_MAX_LEN, 0ull};
    if (known && cli_path_parent(program, folder))
    {
        mmgr_line_put(&beside, folder);
        mmgr_line_put(&beside, "/servers/");
        mmgr_line_put(&beside, target);
        mmgr_line_put(&beside, "/orior_cli");
        if ((mmgr_line_finish(&beside) != 0ull) && query_is_file(path))
        {
            return 1;
        }
    }
    return prepare_built(target, stated, context, path, err_msg);
}

// the line `uname -sm` and the server's home on the machine, queried from it, into `system` and `home`, which each are
// sized for CLI_PATH_MAX_LEN: 1, or 0 with why given
static int prepare_probe(const CliLinkAddress *address, char *system, char *home, VerbaLinea *err_msg)
{
    char probe[2u * CLI_PATH_MAX_LEN];
    VerbaLinea probe_line = {probe, sizeof(probe), 0ull};
    if (!cli_link_remote_run(address, "uname -sm; printf '%s\\n' \"${ORIOR_SERVER_HOME:-$HOME/.orior/server}\"", NULL,
                             0ull, &probe_line, err_msg))
    {
        return 0;
    }
    char *const fields[2] = {system, home};
    unsigned long long at = 0ull;
    for (unsigned int field = 0u; field < 2u; field += 1u)
    {
        unsigned long long end = at;
        while ((end < probe_line.at) && (probe[end] != '\n'))
        {
            end += 1u;
        }
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

int cli_link_prepare(const CliLinkAddress *address, CliLinkStated stated, void *context, VerbaLinea *start,
                     VerbaLinea *err_msg)
{
    char system[CLI_PATH_MAX_LEN];
    char home[CLI_PATH_MAX_LEN];
    char local[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ull;
    unsigned long long changed = 0ull;
    if (!prepare_probe(address, system, home, err_msg) || !prepare_server(system, stated, context, local, err_msg))
    {
        return 0;
    }
    if (!cli_file_stamp(local, &size, &changed))
    {
        mmgr_line_put(err_msg, local);
        mmgr_line_put(err_msg, ": it could not be read");
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
    mmgr_line_put(&program_line, cli_link_is_windows(system) ? "/orior_cli.exe" : "/orior_cli");
    mmgr_line_finish(&program_line);
    char line[(4u * CLI_PATH_MAX_LEN) + 128u];
    VerbaLinea test = {line, sizeof(line), 0ull};
    mmgr_line_put(&test, "test -x ");
    cli_link_quote(program, cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}), &test);
    mmgr_line_finish(&test);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea none = {why, sizeof(why), 0ull};
    VerbaLinea nothing = {NULL, 0ull, 0ull};
    if (!cli_link_remote_run(address, line, NULL, 0ull, &nothing, &none))
    {
        char msg_buf[CLI_PATH_MAX_LEN + 64u];
        VerbaLinea msg = {msg_buf, sizeof(msg_buf), 0ull};
        mmgr_line_put(&msg, "Sending orior's server to ");
        cli_link_machine(address, &msg);
        mmgr_line_finish(&msg);
        stated(context, "preparing", msg_buf);
        unsigned long long bytes_size = 0ull;
        const unsigned char *const bytes = cli_file_read(local, &bytes_size);
        if (bytes == NULL)
        {
            mmgr_line_put(err_msg, local);
            mmgr_line_put(err_msg, ": it could not be read");
            return 0;
        }
        char part[CLI_PATH_MAX_LEN + 136u];
        VerbaLinea part_line = {part, sizeof(part), 0ull};
        mmgr_line_put(&part_line, program);
        mmgr_line_put(&part_line, ".part");
        mmgr_line_finish(&part_line);
        VerbaLinea send = {line, sizeof(line), 0ull};
        mmgr_line_put(&send, "mkdir -p ");
        cli_link_quote(folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), &send);
        mmgr_line_put(&send, " && cat > ");
        cli_link_quote(part, cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL}), &send);
        mmgr_line_put(&send, " && chmod +x ");
        cli_link_quote(part, cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL}), &send);
        mmgr_line_put(&send, " && mv -f ");
        cli_link_quote(part, cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL}), &send);
        mmgr_line_ch(&send, ' ');
        cli_link_quote(program, cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}), &send);
        mmgr_line_finish(&send);
        const int sent = cli_link_remote_run(address, line, (const char *)bytes, bytes_size, &nothing, err_msg);
        cli_file_let_go(bytes, bytes_size);
        if (!sent)
        {
            return 0;
        }
    }
    mmgr_line_put(start, "exec ");
    cli_link_quote(program, cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}), start);
    mmgr_line_put(start, " serve --root ");
    cli_link_quote(address->folder, cellul.len(&(CatenaFinitaCfg){.src = address->folder, .cap = CLI_CELLS_POOL}),
                   start);
    return 1;
}
