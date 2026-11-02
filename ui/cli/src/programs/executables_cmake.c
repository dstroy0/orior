// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_cmake.c: the file a CMake build made, as the reply of CMake's file API names it, and the file a Meson
// build made, as `meson introspect` names it, each reply walked by key off its bytes
#include "../command_line/callback_cfg.h"
#include "../text/reply.h"
#include "../text/text.h"
#include "executables_internal.h"

// The reply's index files: the last of them by name
typedef struct
{
    char last[CLI_NAME_MAX_LEN];
    int has;
} CliIndex;

static void cli_index_each(const CliEachNameIsFolderCfg *args)
{
    CliIndex *const index = (CliIndex *)args->context;
    if (!args->is_folder &&
        (cellul.starts(&(CatenaFinitaCfg){.src = args->name, .other = "index-", .cap = CLI_CELLS_POOL})) &&
        (!index->has ||
         (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->name, .other_pointer = index->last) > 0)))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = index->last,
                          .src = args->name,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1u});
        index->has = 1;
    }
}

typedef struct
{
    const char *folder;
    const char *name;
    char *out;
    const char *path;
    CliReply *reply;
    VerbaLinea *err_msg;
    const char *one;
    const char *other;
    const CliReply *reply_read;
    unsigned long long array;
    const char *match;
    const char *required;
    const char *key;
    const CliReply *target;
    const CliReply *model;
    const char *config;
} ExecutablesCmakeCtx;
// `name` joined to `folder` into `out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void cli_in(const ExecutablesCmakeCtx *args)
{
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    mmgr_line_finish(&line);
}

// the reply file at `path` read into `reply`: 1, or 0 with why given
EMBED_INLINE int cli_reply_file(const ExecutablesCmakeCtx *args)
{
    const char *path = args->path;
    CliReply *reply = args->reply;
    if (EMBED_CALL(cli_reply_path.read, CliReplyPathCfg, .path = path, .reply_out = reply))
    {
        return 1;
    }
    mmgr_line_put(args->err_msg, path);
    mmgr_line_put(args->err_msg, ": it could not be read");
    return 0;
}

// whether two texts are the same, ASCII's case aside
EMBED_INLINE int cli_same_case_aside(const ExecutablesCmakeCtx *args)
{
    const char *other = args->other;
    return cellul.eq(&(CatenaFinitaCfg){.src = args->one, .other = other, .cap = CLI_CELLS_POOL, .ci = 1});
}

// the member `key` of the first item of the array at `array` whose member `match` is `required`, decoded into `out`,
// which is sized for CLI_PATH_MAX_LEN: 1, or 0 where none is
EMBED_INLINE int cli_found_in(const ExecutablesCmakeCtx *args)
{
    const CliReply *reply = args->reply_read;
    unsigned long long array = args->array;
    const char *key = args->key;
    char *out = args->out;
    for (unsigned long long item = EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = reply, .array = array);
         item != CLI_REPLY_NONE; item = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = reply, .item = item))
    {
        if (EMBED_CALL(cli_reply_at.string_is, CliReplyAtCfg, .reply = reply,
                       .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = reply, .object = item,
                                        .key = args->match),
                       .word = args->required))
        {
            return EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = reply,
                              .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = reply,
                                               .object = item, .key = key),
                              .out = out, .capacity = CLI_PATH_MAX_LEN);
        }
    }
    return 0;
}

// the first path of a target's artifacts that is no debug database or import library into `out`: 1, or 0
EMBED_INLINE int cli_artifact(const ExecutablesCmakeCtx *args)
{
    char *out = args->out;
    static const char *const s_passed[4] = {".pdb", ".lib", ".exp", ".ilk"};
    const unsigned long long artifacts =
        EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                   .object = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = args->target), .key = "artifacts");
    for (unsigned long long item =
             EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = args->target, .array = artifacts);
         item != CLI_REPLY_NONE; item = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->target, .item = item))
    {
        if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->target,
                        .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                         .object = item, .key = "path"),
                        .out = out, .capacity = CLI_PATH_MAX_LEN))
        {
            continue;
        }
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = out, .cap = CLI_CELLS_POOL});
        int passed = 0;
        for (unsigned int at = 0u; !passed && (at < 4u); at += 1u)
        {
            passed = (length >= 4ull) && EMBED_CALL(cli_same_case_aside, ExecutablesCmakeCtx,
                                                    .one = out + length - 4ull, .other = s_passed[at]);
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
EMBED_INLINE unsigned long long cli_configuration(const ExecutablesCmakeCtx *args)
{
    const unsigned long long configs =
        EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->model,
                   .object = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = args->model), .key = "configurations");
    const unsigned long long first =
        EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = args->model, .array = configs);
    for (unsigned long long item = first; item != CLI_REPLY_NONE;
         item = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->model, .item = item))
    {
        char named[CLI_NAME_MAX_LEN];
        if (EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->model,
                       .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->model,
                                        .object = item, .key = "name"),
                       .out = named, .capacity = sizeof(named)) &&
            EMBED_CALL(cli_same_case_aside, ExecutablesCmakeCtx, .one = named, .other = args->config))
        {
            return item;
        }
    }
    return first;
}

int cli_cmake_artifact(const CliExecutablesNameOutCtx *args)
{
    char reply_folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_in, ExecutablesCmakeCtx, .folder = args->build, .name = ".cmake/api/v1/reply", .out = reply_folder);
    CliIndex index = {"", 0};
    if (!EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = reply_folder,
                    .each = cli_index_each, .context_bytess = &index))
    {
        mmgr_line_put(args->err_msg, reply_folder);
        mmgr_line_put(args->err_msg, ": it could not be read");
        return 0;
    }
    if (!index.has)
    {
        mmgr_line_put(args->err_msg, "CMake wrote no reply in ");
        mmgr_line_put(args->err_msg, reply_folder);
        return 0;
    }
    char path[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_in, ExecutablesCmakeCtx, .folder = reply_folder, .name = index.last, .out = path);
    CliReply reply = {NULL, 0ull};
    if (!EMBED_CALL(cli_reply_file, ExecutablesCmakeCtx, .path = path, .reply = &reply, .err_msg = args->err_msg))
    {
        return 0;
    }
    const int modelled = EMBED_CALL(
        cli_found_in, ExecutablesCmakeCtx, .reply_read = &reply,
        .array = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply,
                            .object = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = &reply), .key = "objects"),
        .match = "kind", .required = "codemodel", .key = "jsonFile", .out = file);
    EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &reply);
    if (!modelled)
    {
        mmgr_line_put(args->err_msg, "CMake's reply contains no code model");
        return 0;
    }
    EMBED_CALL(cli_in, ExecutablesCmakeCtx, .folder = reply_folder, .name = file, .out = path);
    CliReply model = {NULL, 0ull};
    if (!EMBED_CALL(cli_reply_file, ExecutablesCmakeCtx, .path = path, .reply = &model, .err_msg = args->err_msg))
    {
        return 0;
    }
    const unsigned long long chosen =
        EMBED_CALL(cli_configuration, ExecutablesCmakeCtx, .model = &model, .config = args->config);
    int targeted = 0;
    if (chosen == CLI_REPLY_NONE)
    {
        mmgr_line_put(args->err_msg, "CMake's code model contains no configuration");
    }
    else
    {
        targeted = EMBED_CALL(cli_found_in, ExecutablesCmakeCtx, .reply_read = &model,
                              .array = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &model,
                                                  .object = chosen, .key = "targets"),
                              .match = "name", .required = args->name, .key = "jsonFile", .out = file);
        if (!targeted)
        {
            mmgr_line_put(args->err_msg, "CMake's code model contains no target ");
            mmgr_line_put(args->err_msg, args->name);
        }
    }
    EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &model);
    if (!targeted)
    {
        return 0;
    }
    EMBED_CALL(cli_in, ExecutablesCmakeCtx, .folder = reply_folder, .name = file, .out = path);
    CliReply target = {NULL, 0ull};
    if (!EMBED_CALL(cli_reply_file, ExecutablesCmakeCtx, .path = path, .reply = &target, .err_msg = args->err_msg))
    {
        return 0;
    }
    char artifact[CLI_PATH_MAX_LEN];
    const int found = EMBED_CALL(cli_artifact, ExecutablesCmakeCtx, .target = &target, .out = artifact);
    EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &target);
    if (!found)
    {
        mmgr_line_put(args->err_msg, "CMake names no file for ");
        mmgr_line_put(args->err_msg, args->name);
        return 0;
    }
    const int absolute =
        (artifact[0] == '/') || (artifact[0] == '\\') || ((artifact[0] != '\0') && (artifact[1] == ':'));
    if (absolute)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->out,
                                .src = artifact,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = artifact, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        EMBED_CALL(cli_in, ExecutablesCmakeCtx, .folder = args->build, .name = artifact, .out = args->out);
    }
    return 1;
}

int cli_meson_artifact(const CliExecutablesNameOutCtx *args)
{
    const char *const arguments[3] = {"introspect", "--targets", args->build};
    CliRunResult *const run_result = cli_run_result_alloc();
    if (!EMBED_CALL(cli_executables_name_internal.output, CliExecutablesNameCtx, .tool = "meson", .name = "meson",
                    .request = arguments, .count = 3u, .folder = NULL, .run_result = run_result,
                    .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    // the program's output read where it stands
    const CliReply reply = {run_result->out, run_result->out_size};
    const unsigned long long targets = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = &reply);
    int found = 0;
    for (unsigned long long item =
             EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = &reply, .array = targets);
         !found && (item != CLI_REPLY_NONE);
         item = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = &reply, .item = item))
    {
        if (!EMBED_CALL(cli_reply_at.string_is, CliReplyAtCfg, .reply = &reply,
                        .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply, .object = item,
                                         .key = "name"),
                        .word = args->name) ||
            !EMBED_CALL(cli_reply_at.string_is, CliReplyAtCfg, .reply = &reply,
                        .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply, .object = item,
                                         .key = "type"),
                        .word = "executable"))
        {
            continue;
        }
        // the first target named so ends the search, whether or not it names a file
        found = EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = &reply,
                           .at = EMBED_CALL(cli_reply_array.item, CliReplyArrayCfg, .reply = &reply,
                                            .array = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg,
                                                                .reply = &reply, .object = item, .key = "filename"),
                                            .index = 0ull),
                           .out = args->out, .capacity = CLI_PATH_MAX_LEN)
                    ? 1
                    : -1;
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    if (found != 1)
    {
        mmgr_line_put(args->err_msg, "Meson names no executable ");
        mmgr_line_put(args->err_msg, args->name);
        return 0;
    }
    return 1;
}
