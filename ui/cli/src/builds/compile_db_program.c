// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_program.c: the program a CMake or Meson build orior_cli configured makes from a source, read from CMake's
// file API reply and from Meson's intro-targets.json, each file mapped and walked by key in one pass
#include "../text/text.h"
#include "compile_db_internal.h"

// The search: the folders found, the path wanted, and the best program yet with how it ranks
typedef struct
{
    CliFoundFolders folders;
    char required[CLI_PATH_MAX_LEN];
    int found;
    int debugging;
    unsigned long long time;
    CliProgram best;
} CliProgramSearch;

static CliProgramSearch s_search;

typedef struct
{
    const char *path;
    char *out;
    const char *text;
    const char *one;
    const char *other;
    int debugging;
    unsigned long long time;
    const CliProgram *program;
    unsigned long long size;
    const CliReply *reply;
    unsigned long long node;
    const char *folder;
    CliReply *file;
    const CliReply *target;
    unsigned long long item;
    const char *source;
    unsigned long long root;
    char *program_capacity;
    const CliReply *chains_file;
    unsigned long long chains;
    const char *language;
    char *compiler;
    const char *build;
    const char *config;
    const char *reply_folder;
    const char *compiler_read;
    const char *info;
} CompileDbProgramCtx;
// a path as the search compares paths: with forward slashes, and on Windows in lower case
EMBED_INLINE void cli_same(const CompileDbProgramCtx *args)
{
    memor.cpy(&(MemoriaCfg){.dst = args->out,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->out, .cap = CLI_POOL_BYTES});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = args->out, .size = length, .from = '\\',
               .to = '/');
#if defined(_WIN32)
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = args->out, .size = length);
#endif
}

// whether a path stands alone, not under a folder
EMBED_INLINE int cli_absolute(const CompileDbProgramCtx *args)
{
    return (args->path[0] == '/') || (args->path[0] == '\\') || ((args->path[0] != '\0') && (args->path[1] == ':'));
}

// whether a text is one of two, ASCII's case aside
EMBED_INLINE int cli_one_get(const CompileDbProgramCtx *args)
{
    const char *text = args->text;
    const char *other = args->other;
    char lower[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){
        .dst = lower, .src = text, .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES}) + 1u});
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = lower,
               .size = cellul.len(&(CatenaFinitaCfg){.src = lower, .cap = CLI_POOL_BYTES}));
    return (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = args->one, .cap = CLI_POOL_BYTES})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = other, .cap = CLI_POOL_BYTES}));
}

// a program found retained where it ranks as high as the best yet: with debugging information first, then the later
EMBED_INLINE void cli_program_append(const CompileDbProgramCtx *args)
{
    int debugging = args->debugging;
    unsigned long long time = args->time;
    const int higher = !s_search.found || (debugging > s_search.debugging) ||
                       ((debugging == s_search.debugging) && (time >= s_search.time));
    if (higher)
    {
        s_search.found = 1;
        s_search.debugging = debugging;
        s_search.time = time;
        s_search.best = *args->program;
    }
}

// `text` copied whole into `out`, sized for `size`
EMBED_INLINE void cli_copy(const CompileDbProgramCtx *args)
{
    VerbaLinea line = {args->out, args->size, 0ULL};
    mmgr_line_put(&line, args->text);
    mmgr_line_finish(&line);
}

// the reply file a member `jsonFile` of the object at `node` of `reply` names, in the reply folder `folder`, opened
// and mapped into `file`: 1, or 0 where the member names none or the file cannot be read
EMBED_INLINE int cli_reply_file_read(const CompileDbProgramCtx *args)
{
    const CliReply *reply = args->reply;
    char name[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = reply,
                    .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = reply, .object = args->node,
                                     .key = "jsonFile"),
                    .out = name, .capacity = sizeof(name)))
    {
        return 0;
    }
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder, .relative = name, .joined = path);
    return EMBED_CALL(cli_reply_path.read, CliReplyPathCfg, .path = path, .reply_out = args->file);
}

// whether the source at `item` of a target's reply is the path wanted, joined to `source` where it is not absolute
EMBED_INLINE int cli_source_want(const CompileDbProgramCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->target,
                    .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                     .object = args->item, .key = "path"),
                    .out = path, .capacity = sizeof(path)))
    {
        return 0;
    }
    char full[CLI_PATH_MAX_LEN];
    char compared[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_absolute, CompileDbProgramCtx, .path = path))
    {
        EMBED_CALL(cli_copy, CompileDbProgramCtx, .out = full, .size = sizeof(full), .text = path);
    }
    else
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->source, .relative = path, .joined = full);
    }
    EMBED_CALL(cli_same, CompileDbProgramCtx, .path = full, .out = compared);
    return cellul.eq(&(CatenaFinitaCfg){.src = compared, .other = s_search.required, .cap = CLI_POOL_BYTES});
}

// the first artifact of the target that is a program, not its symbols or import library, into `program`: 1, or 0
EMBED_INLINE int cli_target_program(const CompileDbProgramCtx *args)
{
    const unsigned long long artifacts = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                                    .object = args->root, .key = "artifacts");
    for (unsigned long long at =
             EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = args->target, .array = artifacts);
         at != CLI_REPLY_NONE; at = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->target, .item = at))
    {
        if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->target,
                        .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                         .object = at, .key = "path"),
                        .out = args->program_capacity, .capacity = CLI_PATH_MAX_LEN))
        {
            continue;
        }
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = args->program_capacity, .cap = CLI_POOL_BYTES});
        const int skipped =
            (length >= 4ULL) &&
            ((cellul.eq(&(CatenaFinitaCfg){
                 .src = args->program_capacity + length - 4ULL, .other = ".pdb", .cap = CLI_POOL_BYTES})) ||
             (cellul.eq(&(CatenaFinitaCfg){
                 .src = args->program_capacity + length - 4ULL, .other = ".lib", .cap = CLI_POOL_BYTES})));
        if (!skipped)
        {
            return 1;
        }
    }
    return 0;
}

// the id of the compiler the toolchains at `chains` give `language`, into `compiler`, "" where none is
EMBED_INLINE void cli_chain_compiler(const CompileDbProgramCtx *args)
{
    args->compiler[0] = '\0';
    for (unsigned long long at =
             EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = args->chains_file, .array = args->chains);
         at != CLI_REPLY_NONE; at = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->chains_file, .item = at))
    {
        char chain_language[CLI_NAME_MAX_LEN];
        if (EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->chains_file,
                       .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->chains_file,
                                        .object = at, .key = "language"),
                       .out = chain_language, .capacity = sizeof(chain_language)) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = chain_language, .other = args->language, .cap = CLI_POOL_BYTES})))
        {
            (void)EMBED_CALL(
                cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->chains_file,
                .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->chains_file,
                                 .object = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg,
                                                      .reply = args->chains_file, .object = at, .key = "compiler"),
                                 .key = "id"),
                .out = args->compiler, .capacity = CLI_NAME_MAX_LEN);
        }
    }
}

// one executable target of a CMake build put among those found where it makes `required` from a source
EMBED_INLINE void cli_cmake_target(const CompileDbProgramCtx *args)
{
    const unsigned long long root = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = args->target);
    if (!EMBED_CALL(cli_reply_at.string_is, CliReplyAtCfg, .reply = args->target,
                    .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target, .object = root,
                                     .key = "type"),
                    .word_text = "EXECUTABLE"))
    {
        return;
    }
    unsigned long long matched = CLI_REPLY_NONE;
    for (unsigned long long one =
             EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = args->target,
                        .array = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                            .object = root, .key = "sources"));
         (matched == CLI_REPLY_NONE) && (one != CLI_REPLY_NONE);
         one = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->target, .item = one))
    {
        matched = EMBED_CALL(cli_source_want, CompileDbProgramCtx, .target = args->target, .item = one,
                             .source = args->source)
                      ? one
                      : CLI_REPLY_NONE;
    }
    CliProgram found;
    if ((matched == CLI_REPLY_NONE) || !EMBED_CALL(cli_target_program, CompileDbProgramCtx, .target = args->target,
                                                   .root = root, .program_capacity = found.program))
    {
        return;
    }
    // the source's language by its compile group, C where it has none
    const unsigned long long place =
        EMBED_CALL(cli_reply_at.whole, CliReplyAtCfg, .reply = args->target,
                   .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                    .object = matched, .key = "compileGroupIndex"));
    const unsigned long long group =
        (place != ~0ULL)
            ? EMBED_CALL(cli_reply_array.item, CliReplyArrayCfg, .reply = args->target,
                         .array = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                             .object = root, .key = "compileGroups"),
                         .index = place)
            : CLI_REPLY_NONE;
    char language[CLI_NAME_MAX_LEN];
    if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->target,
                    .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target, .object = group,
                                     .key = "language"),
                    .out = language, .capacity = sizeof(language)))
    {
        EMBED_CALL(cli_copy, CompileDbProgramCtx, .out = language, .size = sizeof(language), .text = "C");
    }
    EMBED_CALL(cli_copy, CompileDbProgramCtx, .out = found.tool, .size = sizeof(found.tool), .text = "cmake");
    EMBED_CALL(cli_copy, CompileDbProgramCtx, .out = found.build, .size = sizeof(found.build), .text = args->build);
    if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->target,
                    .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target, .object = root,
                                     .key = "name"),
                    .out = found.target, .capacity = sizeof(found.target)))
    {
        found.target[0] = '\0';
    }
    EMBED_CALL(cli_copy, CompileDbProgramCtx, .out = found.config, .size = sizeof(found.config), .text = args->config);
    if (!EMBED_CALL(cli_absolute, CompileDbProgramCtx, .path = found.program))
    {
        char relative[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_copy, CompileDbProgramCtx, .out = relative, .size = sizeof(relative), .text = found.program);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->build, .relative = relative,
                   .joined = found.program);
    }
    EMBED_CALL(cli_chain_compiler, CompileDbProgramCtx, .chains_file = args->chains_file, .chains = args->chains,
               .language = language, .compiler = found.compiler);
    EMBED_CALL(cli_program_append, CompileDbProgramCtx,
               .debugging = EMBED_CALL(cli_one_get, CompileDbProgramCtx, .text = args->config, .one = "debug",
                                       .other = "relwithdebinfo"),
               .time = args->time, .program = &found);
}

// the executable targets of one CMake build that make `required` from a source, each put among those found
EMBED_INLINE void cli_cmake_programs(const CompileDbProgramCtx *args)
{
    char index[CLI_PATH_MAX_LEN];
    CliStamped stamped;
    CliReply reply = {NULL, 0ULL};
    if (!EMBED_CALL(cli_compile_db_reply_internal.compile_newest_index, CliCompileDbReplyCtx,
                    .reply = args->reply_folder, .index = index) ||
        !EMBED_CALL(cli_compile_db_path_stamped_internal.compile_stamp, CliCompileDbPathStampedCtx, .path = index,
                    .stamped = &stamped) ||
        !EMBED_CALL(cli_reply_path.read, CliReplyPathCfg, .path = index, .reply_out = &reply))
    {
        return;
    }
    const unsigned long long answers =
        EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply,
                   .object = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = &reply), .key = "reply");
    CliReply model = {NULL, 0ULL};
    CliReply chains_file = {NULL, 0ULL};
    const int modelled = EMBED_CALL(cli_reply_file_read, CompileDbProgramCtx, .reply = &reply,
                                    .node = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply,
                                                       .object = answers, .key = "codemodel-v2"),
                                    .folder = args->reply_folder, .file = &model);
    (void)EMBED_CALL(cli_reply_file_read, CompileDbProgramCtx, .reply = &reply,
                     .node = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply, .object = answers,
                                        .key = "toolchains-v1"),
                     .folder = args->reply_folder, .file = &chains_file);
    EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &reply);
    const unsigned long long chains =
        EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &chains_file,
                   .object = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = &chains_file), .key = "toolchains");
    const unsigned long long paths =
        EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &model,
                   .object = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = &model), .key = "paths");
    char source[CLI_PATH_MAX_LEN];
    char build[CLI_PATH_MAX_LEN];
    if (modelled &&
        EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = &model,
                   .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &model, .object = paths,
                                    .key = "source"),
                   .out = source, .capacity = sizeof(source)) &&
        EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = &model,
                   .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &model, .object = paths,
                                    .key = "build"),
                   .out = build, .capacity = sizeof(build)))
    {
        // the configuration the build's folder is named for, or the first
        const char *named = build;
        const unsigned long long named_end =
            EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = build,
                       .at = cellul.len(&(CatenaFinitaCfg){.src = build, .cap = CLI_POOL_BYTES}), .set = "/\\");
        named = (named_end != 0ULL) ? (build + named_end) : named;
        const unsigned long long configurations =
            EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &model,
                       .object = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = &model), .key = "configurations");
        unsigned long long configuration =
            EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = &model, .array = configurations);
        for (unsigned long long at = configuration; at != CLI_REPLY_NONE;
             at = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = &model, .item = at))
        {
            char name[CLI_NAME_MAX_LEN];
            if (EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = &model,
                           .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &model, .object = at,
                                            .key = "name"),
                           .out = name, .capacity = sizeof(name)) &&
                EMBED_CALL(cli_one_get, CompileDbProgramCtx, .text = name, .one = named, .other = named))
            {
                configuration = at;
                break;
            }
        }
        char config[CLI_NAME_MAX_LEN];
        if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = &model,
                        .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &model,
                                         .object = configuration, .key = "name"),
                        .out = config, .capacity = sizeof(config)))
        {
            config[0] = '\0';
        }
        const unsigned long long targets = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &model,
                                                      .object = configuration, .key = "targets");
        for (unsigned long long at =
                 EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = &model, .array = targets);
             at != CLI_REPLY_NONE; at = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = &model, .item = at))
        {
            CliReply target = {NULL, 0ULL};
            if (EMBED_CALL(cli_reply_file_read, CompileDbProgramCtx, .reply = &model, .node = at,
                           .folder = args->reply_folder, .file = &target))
            {
                EMBED_CALL(cli_cmake_target, CompileDbProgramCtx, .target = &target, .chains_file = &chains_file,
                           .chains = chains, .source = source, .build = build, .config = config, .time = stamped.time);
                EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &target);
            }
        }
    }
    EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &chains_file);
    EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &model);
}

// the compiler's kind by its stem in lower case: MSVC for cl, Clang where the stem contains clang, else GNU
EMBED_INLINE const char *cli_meson_compiler_kind(const CompileDbProgramCtx *args)
{
    char stem[CLI_NAME_MAX_LEN] = "";
    const char *name = args->compiler_read;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = name,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}), .set = "/\\");
    name = (name_end != 0ULL) ? (name + name_end) : name;
    EMBED_CALL(cli_copy, CompileDbProgramCtx, .out = stem, .size = sizeof(stem), .text = name);
    char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = stem,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_POOL_BYTES}), .byte = '.');
    if ((dot != NULL) && (dot != stem))
    {
        dot[0] = '\0';
    }
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = stem,
               .size = cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_POOL_BYTES}));
    if (cellul.eq(&(CatenaFinitaCfg){.src = stem, .other = "cl", .cap = CLI_POOL_BYTES}))
    {
        return "MSVC";
    }
    return (cellul.find(&(CatenaFinitaCfg){
                .src = stem, .cap = CLI_POOL_BYTES, .other = "clang", .other_cap = CLI_POOL_BYTES}) != NULL)
               ? "Clang"
               : "GNU";
}

// whether the array `node` of `reply`, a target's group of sources, names the source the search looks for
EMBED_INLINE int cli_sources_required(const CompileDbProgramCtx *args)
{
    for (unsigned long long place =
             EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = args->reply, .array = args->node);
         place != CLI_REPLY_NONE; place = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->reply, .item = place))
    {
        char path[CLI_PATH_MAX_LEN];
        char compared[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->reply, .at = place, .out = path,
                        .capacity = sizeof(path)))
        {
            continue;
        }
        EMBED_CALL(cli_same, CompileDbProgramCtx, .path = path, .out = compared);
        if (cellul.eq(&(CatenaFinitaCfg){.src = compared, .other = s_search.required, .cap = CLI_POOL_BYTES}))
        {
            return 1;
        }
    }
    return 0;
}

// the executables of one Meson build that make `required` from a source, each put among those found
EMBED_INLINE void cli_meson_programs(const CompileDbProgramCtx *args)
{
    char targets_path[CLI_PATH_MAX_LEN];
    char build[CLI_PATH_MAX_LEN];
    CliStamped stamped;
    CliReply reply = {NULL, 0ULL};
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->info, .relative = "intro-targets.json",
               .joined = targets_path);
    if (!EMBED_CALL(cli_compile_db_path_stamped_internal.compile_stamp, CliCompileDbPathStampedCtx,
                    .path = targets_path, .stamped = &stamped) ||
        !EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->info, .out = build) ||
        !EMBED_CALL(cli_reply_path.read, CliReplyPathCfg, .path = targets_path, .reply_out = &reply))
    {
        return;
    }
    const char *config = build;
    const unsigned long long config_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = build,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = build, .cap = CLI_POOL_BYTES}), .set = "/\\");
    config = (config_end != 0ULL) ? (build + config_end) : config;
    for (unsigned long long target = EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = &reply,
                                                .array = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = &reply));
         target != CLI_REPLY_NONE; target = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = &reply, .item = target))
    {
        if (!EMBED_CALL(cli_reply_at.string_is, CliReplyAtCfg, .reply = &reply,
                        .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply, .object = target,
                                         .key = "type"),
                        .word_text = "executable"))
        {
            continue;
        }
        const unsigned long long groups = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply,
                                                     .object = target, .key = "target_sources");
        unsigned long long group = CLI_REPLY_NONE;
        for (unsigned long long one =
                 EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = &reply, .array = groups);
             (group == CLI_REPLY_NONE) && (one != CLI_REPLY_NONE);
             one = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = &reply, .item = one))
        {
            const unsigned long long sources = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply,
                                                          .object = one, .key = "sources");
            group =
                EMBED_CALL(cli_sources_required, CompileDbProgramCtx, .reply = &reply, .node = sources) ? one : group;
        }
        CliProgram found;
        if ((group == CLI_REPLY_NONE) ||
            !EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = &reply,
                        .at = EMBED_CALL(cli_reply_array.item, CliReplyArrayCfg, .reply = &reply,
                                         .array = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg,
                                                             .reply = &reply, .object = target, .key = "filename"),
                                         .index = 0ULL),
                        .out = found.program, .capacity = sizeof(found.program)))
        {
            continue;
        }
        char compiler[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = &reply,
                        .at = EMBED_CALL(cli_reply_array.item, CliReplyArrayCfg, .reply = &reply,
                                         .array = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg,
                                                             .reply = &reply, .object = group, .key = "compiler"),
                                         .index = 0ULL),
                        .out = compiler, .capacity = sizeof(compiler)))
        {
            compiler[0] = '\0';
        }
        EMBED_CALL(cli_copy, CompileDbProgramCtx, .out = found.tool, .size = sizeof(found.tool), .text = "meson");
        EMBED_CALL(cli_copy, CompileDbProgramCtx, .out = found.build, .size = sizeof(found.build), .text = build);
        if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = &reply,
                        .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply, .object = target,
                                         .key = "name"),
                        .out = found.target, .capacity = sizeof(found.target)))
        {
            found.target[0] = '\0';
        }
        EMBED_CALL(cli_copy, CompileDbProgramCtx, .out = found.config, .size = sizeof(found.config), .text = config);
        EMBED_CALL(cli_copy, CompileDbProgramCtx, .out = found.compiler, .size = sizeof(found.compiler),
                   .text = EMBED_CALL(cli_meson_compiler_kind, CompileDbProgramCtx, .compiler_read = compiler));
        EMBED_CALL(cli_program_append, CompileDbProgramCtx,
                   .debugging = EMBED_CALL(cli_one_get, CompileDbProgramCtx, .text = config, .one = "debug",
                                           .other = "debugoptimized"),
                   .time = stamped.time, .program = &found);
    }
    EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &reply);
}

int cli_compile_db_program_get(const CliCompileDbRootFileCfg *args)
{
    CliProgram *program = args->program;
    static const char *const s_reply[4] = {".cmake", "api", "v1", "reply"};
    static const char *const s_info[1] = {"meson-info"};
    EMBED_CALL(cli_same, CompileDbProgramCtx, .path = args->file, .out = s_search.required);
    s_search.found = 0;
    EMBED_CALL(cli_compile_db_found_top_internal.compile_found_alloc, CliCompileDbFoundTopCtx,
               .found = &s_search.folders);
    char top[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = "build/cmake", .joined = top);
    EMBED_CALL(cli_compile_db_found_internal.compile_folders_by_name, CliCompileDbFoundCtx, .top = top, .tail = s_reply,
               .count = 4u, .found = &s_search.folders);
    for (unsigned int at = 0u; at < s_search.folders.count; at += 1u)
    {
        EMBED_CALL(cli_cmake_programs, CompileDbProgramCtx, .reply_folder = s_search.folders.path[at]);
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = "build/meson", .joined = top);
    EMBED_CALL(cli_compile_db_found_internal.compile_folders_by_name, CliCompileDbFoundCtx, .top = top, .tail = s_info,
               .count = 1u, .found = &s_search.folders);
    for (unsigned int at = 0u; at < s_search.folders.count; at += 1u)
    {
        EMBED_CALL(cli_meson_programs, CompileDbProgramCtx, .info = s_search.folders.path[at]);
    }
    const int found = s_search.found;
    if (found)
    {
        *program = s_search.best;
    }
    g_cli_guard->persistent_buf_release(s_search.folders.path, g_cli_compile_owner);
    *(&s_search.folders) = (CliFoundFolders){NULL, 0u, 0u};
    return found;
}
