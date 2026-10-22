// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link_prepare.c: orior's server put on another machine where it is not there: the machine asked what it is, the
// server for it found or built here, sent under a name of its size and the time it was written, and the line that
// starts it on the tree given
#include "../runs/runner.h"
#include "../text/json_tree.h"
#include "link.h"

#include "../text/text.h"

// the most lines of a failed build said
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

int cli_link_target(const char *system, char *target, CliLine *said)
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
        cli_line_text(said, "orior_cli builds no server for ");
        cli_line_counted(said, arch, arch_size);
        return 0;
    }
    if (!prepare_is(kind, kind_size, "Linux"))
    {
        cli_line_text(said, "orior_cli builds no server for ");
        cli_line_counted(said, kind, kind_size);
        cli_line_text(said, " from this machine");
        return 0;
    }
    CliLine line = {target, CLI_NAME_LONGEST, 0ull};
    cli_line_text(&line, named);
    cli_line_text(&line, "-unknown-linux-musl");
    cli_line_closed(&line);
    return 1;
}

// the last lines of what a build wrote to its errors said
static void prepare_build_said(const CliRan *ran, CliLine *said)
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
    cli_lossy(said, (const unsigned char *)ran->err + start, end - start);
}

// orior's server built here for `target` from the tree's own sources, its path into `path`: 1, or 0 with why said
static int prepare_built(const char *target, CliLinkStated stated, void *context, char *path, CliLine *said)
{
    char tree[CLI_PATH_LONGEST];
    char script[CLI_PATH_LONGEST];
    CliLine script_line = {script, sizeof(script), 0ull};
    const int found = cli_tree_found(tree);
    if (found)
    {
        cli_line_text(&script_line, tree);
        cli_line_text(&script_line, "/ui/cli/build.sh");
    }
    if (!found || (cli_line_closed(&script_line) == 0ull) || !query_is_file(script))
    {
        cli_line_text(said, "no orior_cli server for ");
        cli_line_text(said, target);
        cli_line_text(said, " is beside this program, under servers/");
        cli_line_text(said, target);
        cli_line_text(said, "/orior_cli");
        return 0;
    }
    char told[CLI_NAME_LONGEST + 64u];
    CliLine told_line = {told, sizeof(told), 0ull};
    cli_line_text(&told_line, "Building orior's server for ");
    cli_line_text(&told_line, target);
    cli_line_closed(&told_line);
    stated(context, "preparing", told);
    char bash[CLI_PATH_LONGEST];
    if (!cli_runner_bash(bash, said))
    {
        return 0;
    }
    CliLine built = {path, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&built, tree);
    cli_line_text(&built, "/build/cli/");
    cli_line_text(&built, target);
    cli_line_text(&built, "/orior_cli");
    cli_line_closed(&built);
    const char *const words[4] = {bash, script, target, NULL};
    CliRan *const ran = cli_ran_taken();
    int done = cli_ran(words, tree, NULL, 0u, ran, said);
    if (done && ((ran->code != 0) || !query_is_file(path)))
    {
        prepare_build_said(ran, said);
        done = 0;
    }
    cli_ran_given(ran);
    return done;
}

// orior's server for a machine `uname -sm` says is `system`, its path into `path`: ORIOR_SERVER where it is set; on
// Windows this program, which serves as well; elsewhere one built for the machine's target, beside this program under
// servers/, or built from the tree's own sources for it. 1, or 0 with why said
static int prepare_server(const char *system, CliLinkStated stated, void *context, char *path, CliLine *said)
{
    if (query_environment_read("ORIOR_SERVER", path, CLI_PATH_LONGEST))
    {
        return 1;
    }
    char program[CLI_PATH_LONGEST];
    const int known = qry_member_program(program, sizeof(program));
    if (cli_link_is_windows(system))
    {
        if (!known)
        {
            cli_line_text(said, "this program's path is not known");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = path, .src = program, .bytes = sizeof(program)});
        return 1;
    }
    char target[CLI_NAME_LONGEST];
    if (!cli_link_target(system, target, said))
    {
        return 0;
    }
    char folder[CLI_PATH_LONGEST];
    CliLine beside = {path, CLI_PATH_LONGEST, 0ull};
    if (known && cli_path_parent(program, folder))
    {
        cli_line_text(&beside, folder);
        cli_line_text(&beside, "/servers/");
        cli_line_text(&beside, target);
        cli_line_text(&beside, "/orior_cli");
        if ((cli_line_closed(&beside) != 0ull) && query_is_file(path))
        {
            return 1;
        }
    }
    return prepare_built(target, stated, context, path, said);
}

// the line `uname -sm` and the server's home on the machine, asked of it, into `system` and `home`, which each hold
// CLI_PATH_LONGEST: 1, or 0 with why said
static int prepare_probe(const CliLinkAddress *address, char *system, char *home, CliLine *said)
{
    char probe[2u * CLI_PATH_LONGEST];
    CliLine probe_line = {probe, sizeof(probe), 0ull};
    if (!cli_link_ask(address, "uname -sm; printf '%s\\n' \"${ORIOR_SERVER_HOME:-$HOME/.orior/server}\"", NULL, 0ull,
                      &probe_line, said))
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
        const unsigned long long size = (stop - start < CLI_PATH_LONGEST) ? (stop - start) : (CLI_PATH_LONGEST - 1u);
        memor.cpy(&(MemoriaCfg){.dst = fields[field], .src = probe + start, .bytes = (size_t)size});
        fields[field][size] = '\0';
        at = (end < probe_line.at) ? (end + 1u) : end;
    }
    return 1;
}

int cli_link_prepare(const CliLinkAddress *address, CliLinkStated stated, void *context, CliLine *start, CliLine *said)
{
    char system[CLI_PATH_LONGEST];
    char home[CLI_PATH_LONGEST];
    char local[CLI_PATH_LONGEST];
    unsigned long long size = 0ull;
    unsigned long long changed = 0ull;
    if (!prepare_probe(address, system, home, said) || !prepare_server(system, stated, context, local, said))
    {
        return 0;
    }
    if (!cli_file_stamp(local, &size, &changed))
    {
        cli_line_text(said, local);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    char folder[CLI_PATH_LONGEST + 64u];
    CliLine folder_line = {folder, sizeof(folder), 0ull};
    cli_line_text(&folder_line, home);
    cli_line_text(&folder_line, "/" CLI_VERSION "-");
    cli_line_decimal(&folder_line, size, 1u);
    cli_line_character(&folder_line, '-');
    cli_line_decimal(&folder_line, changed / 1000u, 1u);
    cli_line_closed(&folder_line);
    char program[CLI_PATH_LONGEST + 128u];
    CliLine program_line = {program, sizeof(program), 0ull};
    cli_line_text(&program_line, folder);
    cli_line_text(&program_line, cli_link_is_windows(system) ? "/orior_cli.exe" : "/orior_cli");
    cli_line_closed(&program_line);
    char line[(4u * CLI_PATH_LONGEST) + 128u];
    CliLine test = {line, sizeof(line), 0ull};
    cli_line_text(&test, "test -x ");
    cli_link_quote(program, cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}), &test);
    cli_line_closed(&test);
    char why[CLI_SAID_LONGEST];
    CliLine none = {why, sizeof(why), 0ull};
    CliLine nothing = {NULL, 0ull, 0ull};
    if (!cli_link_ask(address, line, NULL, 0ull, &nothing, &none))
    {
        char told[CLI_PATH_LONGEST + 64u];
        CliLine told_line = {told, sizeof(told), 0ull};
        cli_line_text(&told_line, "Sending orior's server to ");
        cli_link_machine(address, &told_line);
        cli_line_closed(&told_line);
        stated(context, "preparing", told);
        unsigned long long bytes_size = 0ull;
        const unsigned char *const bytes = cli_file_held(local, &bytes_size);
        if (bytes == NULL)
        {
            cli_line_text(said, local);
            cli_line_text(said, ": it could not be read");
            return 0;
        }
        char part[CLI_PATH_LONGEST + 136u];
        CliLine part_line = {part, sizeof(part), 0ull};
        cli_line_text(&part_line, program);
        cli_line_text(&part_line, ".part");
        cli_line_closed(&part_line);
        CliLine send = {line, sizeof(line), 0ull};
        cli_line_text(&send, "mkdir -p ");
        cli_link_quote(folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), &send);
        cli_line_text(&send, " && cat > ");
        cli_link_quote(part, cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL}), &send);
        cli_line_text(&send, " && chmod +x ");
        cli_link_quote(part, cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL}), &send);
        cli_line_text(&send, " && mv -f ");
        cli_link_quote(part, cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL}), &send);
        cli_line_character(&send, ' ');
        cli_link_quote(program, cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}), &send);
        cli_line_closed(&send);
        const int sent = cli_link_ask(address, line, (const char *)bytes, bytes_size, &nothing, said);
        cli_file_let_go(bytes, bytes_size);
        if (!sent)
        {
            return 0;
        }
    }
    cli_line_text(start, "exec ");
    cli_link_quote(program, cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}), start);
    cli_line_text(start, " serve --root ");
    cli_link_quote(address->folder, cellul.len(&(CatenaFinitaCfg){.src = address->folder, .cap = CLI_CELLS_POOL}),
                   start);
    return 1;
}
