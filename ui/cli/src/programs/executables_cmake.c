// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_cmake.c: the file a CMake build made, as the reply of CMake's file API names it, and the file a Meson
// build made, as `meson introspect` names it, each reply walked by key off its bytes
#include "../text/reply.h"
#include "../text/text.h"
#include "executables_internal.h"

// The reply's index files: the last of them by name
typedef struct
{
    char last[CLI_NAME_MAX_LEN];
    int has;
} CliIndex;

static void cli_index_each(void *context, const char *name, int is_folder)
{
    CliIndex *const index = (CliIndex *)context;
    if (!is_folder && (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "index-", .cap = CLI_CELLS_POOL})) &&
        (!index->has || (cli_text_order(name, index->last) > 0)))
    {
        memor.cpy(&(MemoriaCfg){.dst = index->last,
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
        index->has = 1;
    }
}

// `name` joined to `folder` into `out`, sized for CLI_PATH_MAX_LEN
static void cli_in(const char *folder, const char *name, char *out)
{
    VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    mmgr_line_finish(&line);
}

// the reply file at `path` read into `reply`: 1, or 0 with why given
static int cli_reply_file(const char *path, CliReply *reply, VerbaLinea *err_msg)
{
    if (cli_reply_read(path, reply))
    {
        return 1;
    }
    mmgr_line_put(err_msg, path);
    mmgr_line_put(err_msg, ": it could not be read");
    return 0;
}

// whether two texts are the same, ASCII's case aside
static int cli_same_case_aside(const char *one, const char *other)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = one, .other = other, .cap = CLI_CELLS_POOL, .ci = 1});
}

// the member `key` of the first item of the array at `array` whose member `match` is `wanted`, decoded into `out`,
// which is sized for CLI_PATH_MAX_LEN: 1, or 0 where none is
static int cli_found_in(const CliReply *reply, unsigned long long array, const char *match, const char *wanted,
                        const char *key, char *out)
{
    for (unsigned long long item = cli_reply_first(reply, array); item != CLI_REPLY_NONE;
         item = cli_reply_next(reply, item))
    {
        if (cli_reply_string_is(reply, cli_reply_member(reply, item, match), wanted))
        {
            return cli_reply_string(reply, cli_reply_member(reply, item, key), out, CLI_PATH_MAX_LEN);
        }
    }
    return 0;
}

// the first path of a target's artifacts that is no debug database or import library into `out`: 1, or 0
static int cli_artifact(const CliReply *target, char *out)
{
    static const char *const s_passed[4] = {".pdb", ".lib", ".exp", ".ilk"};
    const unsigned long long artifacts = cli_reply_member(target, cli_reply_root(target), "artifacts");
    for (unsigned long long item = cli_reply_first(target, artifacts); item != CLI_REPLY_NONE;
         item = cli_reply_next(target, item))
    {
        if (!cli_reply_string(target, cli_reply_member(target, item, "path"), out, CLI_PATH_MAX_LEN))
        {
            continue;
        }
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = out, .cap = CLI_CELLS_POOL});
        int passed = 0;
        for (unsigned int at = 0u; !passed && (at < 4u); at += 1u)
        {
            passed = (length >= 4ull) && cli_same_case_aside(out + length - 4ull, s_passed[at]);
        }
        if (!passed)
        {
            return 1;
        }
    }
    return 0;
}

// the configuration of the code model named `config`, its case aside, else the first: its object, CLI_REPLY_NONE
// where the model contains none
static unsigned long long cli_configuration(const CliReply *model, const char *config)
{
    const unsigned long long configs = cli_reply_member(model, cli_reply_root(model), "configurations");
    const unsigned long long first = cli_reply_first(model, configs);
    for (unsigned long long item = first; item != CLI_REPLY_NONE; item = cli_reply_next(model, item))
    {
        char named[CLI_NAME_MAX_LEN];
        if (cli_reply_string(model, cli_reply_member(model, item, "name"), named, sizeof(named)) &&
            cli_same_case_aside(named, config))
        {
            return item;
        }
    }
    return first;
}

int cli_cmake_artifact(const char *build, const char *name, const char *config, char *out, VerbaLinea *err_msg)
{
    char reply_folder[CLI_PATH_MAX_LEN];
    cli_in(build, ".cmake/api/v1/reply", reply_folder);
    CliIndex index = {"", 0};
    if (!query_folder_each(reply_folder, cli_index_each, &index))
    {
        mmgr_line_put(err_msg, reply_folder);
        mmgr_line_put(err_msg, ": it could not be read");
        return 0;
    }
    if (!index.has)
    {
        mmgr_line_put(err_msg, "CMake wrote no reply in ");
        mmgr_line_put(err_msg, reply_folder);
        return 0;
    }
    char path[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    cli_in(reply_folder, index.last, path);
    CliReply reply = {NULL, 0ull};
    if (!cli_reply_file(path, &reply, err_msg))
    {
        return 0;
    }
    const int modelled = cli_found_in(&reply, cli_reply_member(&reply, cli_reply_root(&reply), "objects"), "kind",
                                      "codemodel", "jsonFile", file);
    cli_reply_let_go(&reply);
    if (!modelled)
    {
        mmgr_line_put(err_msg, "CMake's reply contains no code model");
        return 0;
    }
    cli_in(reply_folder, file, path);
    CliReply model = {NULL, 0ull};
    if (!cli_reply_file(path, &model, err_msg))
    {
        return 0;
    }
    const unsigned long long chosen = cli_configuration(&model, config);
    int targeted = 0;
    if (chosen == CLI_REPLY_NONE)
    {
        mmgr_line_put(err_msg, "CMake's code model contains no configuration");
    }
    else
    {
        targeted = cli_found_in(&model, cli_reply_member(&model, chosen, "targets"), "name", name, "jsonFile", file);
        if (!targeted)
        {
            mmgr_line_put(err_msg, "CMake's code model contains no target ");
            mmgr_line_put(err_msg, name);
        }
    }
    cli_reply_let_go(&model);
    if (!targeted)
    {
        return 0;
    }
    cli_in(reply_folder, file, path);
    CliReply target = {NULL, 0ull};
    if (!cli_reply_file(path, &target, err_msg))
    {
        return 0;
    }
    char artifact[CLI_PATH_MAX_LEN];
    const int found = cli_artifact(&target, artifact);
    cli_reply_let_go(&target);
    if (!found)
    {
        mmgr_line_put(err_msg, "CMake names no file for ");
        mmgr_line_put(err_msg, name);
        return 0;
    }
    const int absolute =
        (artifact[0] == '/') || (artifact[0] == '\\') || ((artifact[0] != '\0') && (artifact[1] == ':'));
    if (absolute)
    {
        memor.cpy(&(MemoriaCfg){.dst = out,
                                .src = artifact,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = artifact, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        cli_in(build, artifact, out);
    }
    return 1;
}

int cli_meson_artifact(const char *build, const char *name, char *out, VerbaLinea *err_msg)
{
    const char *const args[3] = {"introspect", "--targets", build};
    CliRan *const ran = cli_ran_taken();
    if (!cli_executables_output("meson", "meson", args, 3u, NULL, ran, err_msg))
    {
        cli_ran_given(ran);
        return 0;
    }
    // the program's output read where it stands
    const CliReply reply = {ran->out, ran->out_size};
    const unsigned long long targets = cli_reply_root(&reply);
    int found = 0;
    for (unsigned long long item = cli_reply_first(&reply, targets); !found && (item != CLI_REPLY_NONE);
         item = cli_reply_next(&reply, item))
    {
        if (!cli_reply_string_is(&reply, cli_reply_member(&reply, item, "name"), name) ||
            !cli_reply_string_is(&reply, cli_reply_member(&reply, item, "type"), "executable"))
        {
            continue;
        }
        // the first target named so ends the search, whether or not it names a file
        found = cli_reply_string(&reply, cli_reply_item(&reply, cli_reply_member(&reply, item, "filename"), 0ull), out,
                                 CLI_PATH_MAX_LEN)
                    ? 1
                    : -1;
    }
    cli_ran_given(ran);
    if (found != 1)
    {
        mmgr_line_put(err_msg, "Meson names no executable ");
        mmgr_line_put(err_msg, name);
        return 0;
    }
    return 1;
}
