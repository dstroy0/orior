// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_cmake.c: a CMake build's entries read from its file API reply: one for each source of each target in
// the configuration the build's folder is named for, as the build's own compiler is given it, C and C++ alone. Each
// reply file stays mapped and walked by key in one pass, the entries made straight into the gathered tree
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "compile_db_internal.h"

typedef struct
{
    const CliReply *reply;
    unsigned long long node;
    const char *folder;
    CliReply *file;
    const char *flag;
    unsigned long long size;
    const char *lead;
    const char *text;
    const CliReply *target;
    unsigned long long at;
    unsigned long long group;
    int msvc;
    const char *language;
    const char *source;
    const char *build;
    const char *path;
    const CliCompilerOf *compiler;
    unsigned int entries;
    const CliReply *model;
    unsigned long long configurations;
} CompileDbCmakeCtx;

// the reply file a member `jsonFile` of the object at `node` of `reply` names, in the reply folder `folder`, opened
// and mapped into `file`: 1, or 0 where the member names none or the file cannot be read
EMBED_INLINE int cli_reply_file_read(const CompileDbCmakeCtx *args)
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

// a flag put after the compile group's flags
EMBED_INLINE void cli_flag_append(const CompileDbCmakeCtx *args)
{
    VerbaLinea *const text = &g_cli_gather.flags_text;
    const unsigned long long mark = text->at;
    if ((g_cli_gather.flags >= CLI_GATHER_FLAGS) || (mark > 0xffffffffULL))
    {
        return;
    }
    mmgr_line_put_n(text, args->flag, args->size);
    mmgr_line_ch(text, '\0');
    // a flag whose capacity the cellblock cannot meet is left out, the flags before it retained
    if (!mmgr_line_ok(text))
    {
        text->at = mark;
        return;
    }
    // where the flag starts, below 4 GiB as the test above ensures, which an unsigned int stores
    g_cli_gather.flag[g_cli_gather.flags] = (unsigned int)mark;
    g_cli_gather.flags += 1u;
}

static void cli_fragment_word(const CliEachWordSizeCfg *args)
{
    unsigned long long size = args->size;
    EMBED_CALL(cli_flag_append, CompileDbCmakeCtx, .flag = args->word_text, .size = size);
}

// a flag made of `lead` and `text` put after the flags
EMBED_INLINE void cli_flag_init(const CompileDbCmakeCtx *args)
{
    char flag[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {flag, sizeof(flag), 0ULL};
    mmgr_line_put(&line, args->lead);
    mmgr_line_put(&line, args->text);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_flag_append, CompileDbCmakeCtx, .flag = flag,
               .size = cellul.len(&(CatenaFinitaCfg){.src = flag, .cap = CLI_POOL_BYTES}));
}

// a string made in the entries tree with every backslash written as a slash
EMBED_INLINE unsigned int cli_slashed_init(const CompileDbCmakeCtx *args)
{
    const char *text = args->text;
    char slashed[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){
        .dst = slashed, .src = text, .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES}) + 1u});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = slashed,
               .size = cellul.len(&(CatenaFinitaCfg){.src = slashed, .cap = CLI_POOL_BYTES}), .from = '\\', .to = '/');
    return EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                      .json = &g_cli_gather.json, .text = slashed,
                      .length = cellul.len(&(CatenaFinitaCfg){.src = slashed, .cap = CLI_POOL_BYTES}));
}

// a fragment of a compile command at `at` of the target's reply decoded into the fragment line, at the bytes it is
// sized for, and its words put after the flags
EMBED_INLINE void cli_fragment_flags(const CompileDbCmakeCtx *args)
{
    unsigned long long at = args->at;
    VerbaLinea *const text = &g_cli_gather.fragment_text;
    const unsigned long long span =
        EMBED_CALL(cli_reply_at.string_span, CliReplyAtCfg, .reply = args->target, .at = at);
    text->at = 0ULL;
    if ((text->capacity <= (span + 1ULL)) ||
        !EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->target, .at = at, .out = text->out,
                    .capacity = span + 1ULL))
    {
        return;
    }
    EMBED_CALL(cli_compile_db_text_internal.compile_words_each, CliCompileDbTextCtx, .text = text->out,
               .length = cellul.len(&(CatenaFinitaCfg){.src = text->out, .cap = CLI_POOL_BYTES}),
               .each = cli_fragment_word, .context = NULL);
}

// a compile group's flags: its fragments' words, its include folders, its defines and its sysroot
EMBED_INLINE void cli_group_flags(const CompileDbCmakeCtx *args)
{
    g_cli_gather.flags = 0u;
    g_cli_gather.flags_text.at = 0ULL;
    const unsigned long long fragments = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                                    .object = args->group, .key = "compileCommandFragments");
    for (unsigned long long at =
             EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = args->target, .array = fragments);
         at != CLI_REPLY_NONE; at = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->target, .item = at))
    {
        EMBED_CALL(cli_fragment_flags, CompileDbCmakeCtx, .target = args->target,
                   .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target, .object = at,
                                    .key = "fragment"));
    }
    const unsigned long long includes = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                                   .object = args->group, .key = "includes");
    for (unsigned long long at =
             EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = args->target, .array = includes);
         at != CLI_REPLY_NONE; at = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->target, .item = at))
    {
        char path[CLI_PATH_MAX_LEN];
        const int system = EMBED_CALL(cli_reply_at.reply_true, CliReplyAtCfg, .reply = args->target,
                                      .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg,
                                                       .reply = args->target, .object = at, .key = "isSystem"));
        if (EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->target,
                       .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target, .object = at,
                                        .key = "path"),
                       .out = path, .capacity = sizeof(path)))
        {
            const char *const include_lead = system ? "-isystem" : "-I";
            EMBED_CALL(cli_flag_init, CompileDbCmakeCtx, .lead = args->msvc ? "/I" : include_lead, .text = path);
        }
    }
    const unsigned long long defines = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                                  .object = args->group, .key = "defines");
    for (unsigned long long at =
             EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = args->target, .array = defines);
         at != CLI_REPLY_NONE; at = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->target, .item = at))
    {
        char define[CLI_PATH_MAX_LEN];
        if (EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->target,
                       .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target, .object = at,
                                        .key = "define"),
                       .out = define, .capacity = sizeof(define)))
        {
            EMBED_CALL(cli_flag_init, CompileDbCmakeCtx, .lead = args->msvc ? "/D" : "-D", .text = define);
        }
    }
    char sysroot[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->target,
                   .at =
                       EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                  .object = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg,
                                                       .reply = args->target, .object = args->group, .key = "sysroot"),
                                  .key = "path"),
                   .out = sysroot, .capacity = sizeof(sysroot)))
    {
        EMBED_CALL(cli_flag_init, CompileDbCmakeCtx, .lead = "--sysroot=", .text = sysroot);
    }
}

// the compiler a CMake build gives `language`: its index among those read, or the count where it gives none
EMBED_INLINE unsigned int cli_compiler_get(const CompileDbCmakeCtx *args)
{
    unsigned int at = 0u;
    while ((at < g_cli_gather.compilers) &&
           (!cellul.eq(&(CatenaFinitaCfg){
               .src = g_cli_gather.compiler[at].language, .other = args->language, .cap = CLI_POOL_BYTES})))
    {
        at += 1u;
    }
    return at;
}

// the compilers of the toolchains file at `node` of the index's reply read: each language's path and id
EMBED_INLINE void cli_toolchains_read(const CompileDbCmakeCtx *args)
{
    const CliReply *reply = args->reply;
    CliReply chains_file = {NULL, 0ULL};
    g_cli_gather.compilers = 0u;
    if (!EMBED_CALL(cli_reply_file_read, CompileDbCmakeCtx, .reply = reply, .node = args->node, .folder = args->folder,
                    .file = &chains_file))
    {
        return;
    }
    const unsigned long long chains =
        EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &chains_file,
                   .object = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = &chains_file), .key = "toolchains");
    for (unsigned long long at =
             EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = &chains_file, .array = chains);
         (g_cli_gather.compilers < CLI_GATHER_LANGUAGES) && (at != CLI_REPLY_NONE);
         at = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = &chains_file, .item = at))
    {
        CliCompilerOf one;
        const unsigned long long compiler = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg,
                                                       .reply = &chains_file, .object = at, .key = "compiler");
        if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = &chains_file,
                        .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &chains_file,
                                         .object = at, .key = "language"),
                        .out = one.language, .capacity = sizeof(one.language)) ||
            !EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = &chains_file,
                        .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &chains_file,
                                         .object = compiler, .key = "path"),
                        .out = one.path, .capacity = sizeof(one.path)))
        {
            continue;
        }
        if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = &chains_file,
                        .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &chains_file,
                                         .object = compiler, .key = "id"),
                        .out = one.id, .capacity = sizeof(one.id)))
        {
            one.id[0] = '\0';
        }
        const unsigned int place = EMBED_CALL(cli_compiler_get, CompileDbCmakeCtx, .language = one.language);
        g_cli_gather.compiler[place] = one;
        g_cli_gather.compilers += (place == g_cli_gather.compilers) ? 1u : 0u;
    }
    EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &chains_file);
}

// one entry made for the source `path` of a compile group whose flags are read, appended to `entries`
EMBED_INLINE void cli_entry_init(const CompileDbCmakeCtx *args)
{
    CliJson *const json = &g_cli_gather.json;
    char file[CLI_PATH_MAX_LEN];
    if ((args->path[0] == '/') || (args->path[0] == '\\') || ((args->path[0] != '\0') && (args->path[1] == ':')))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = file,
                          .src = args->path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    }
    else
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->source, .relative = args->path, .joined = file);
    }
    const unsigned int arguments =
        EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = json, .type = CFG_JSON_ARRAY);
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = arguments,
               .value = EMBED_CALL(cli_slashed_init, CompileDbCmakeCtx, .text = args->compiler->path));
    for (unsigned int flag = 0u; flag < g_cli_gather.flags; flag += 1u)
    {
        const char *const text = g_cli_gather.flags_text.out + g_cli_gather.flag[flag];
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = arguments,
                   .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                       .json = json, .text = text,
                                       .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES})));
    }
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = arguments,
               .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                   .json = json, .text = args->msvc ? "/c" : "-c", .length = 2ULL));
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = arguments,
               .value = EMBED_CALL(cli_slashed_init, CompileDbCmakeCtx, .text = file));
    const unsigned int entry =
        EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = json, .type = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = entry,
               .name = "directory",
               .value =
                   EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                              .json = json, .text = args->build,
                              .length = cellul.len(&(CatenaFinitaCfg){.src = args->build, .cap = CLI_POOL_BYTES})));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = entry,
               .name = "file", .value = EMBED_CALL(cli_slashed_init, CompileDbCmakeCtx, .text = file));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = entry,
               .name = "arguments", .value = arguments);
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = args->entries,
               .value = entry);
}

// the entries of a target's C and C++ compile groups, one for each source, appended to `entries`: the group's source
// indexes rise, so its sources are walked once beside them
EMBED_INLINE void cli_target_entries(const CompileDbCmakeCtx *args)
{
    const unsigned long long root = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = args->target);
    const unsigned long long sources =
        EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target, .object = root, .key = "sources");
    const unsigned long long groups = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                                 .object = root, .key = "compileGroups");
    for (unsigned long long group =
             EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = args->target, .array = groups);
         group != CLI_REPLY_NONE; group = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->target, .item = group))
    {
        char language[CLI_NAME_MAX_LEN];
        if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->target,
                        .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                         .object = group, .key = "language"),
                        .out = language, .capacity = sizeof(language)) ||
            ((!cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "C", .cap = CLI_POOL_BYTES})) &&
             (!cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "CXX", .cap = CLI_POOL_BYTES}))))
        {
            continue;
        }
        const unsigned int compiler = EMBED_CALL(cli_compiler_get, CompileDbCmakeCtx, .language = language);
        if (compiler == g_cli_gather.compilers)
        {
            continue;
        }
        const CliCompilerOf *const one = &g_cli_gather.compiler[compiler];
        const int msvc = (cellul.eq(&(CatenaFinitaCfg){.src = one->id, .other = "MSVC", .cap = CLI_POOL_BYTES})) ||
                         EMBED_CALL(cli_compile_db_table.is_cl, CliCompileDbCfg, .compiler = one->path);
        EMBED_CALL(cli_group_flags, CompileDbCmakeCtx, .target = args->target, .group = group, .msvc = msvc);
        unsigned long long index =
            EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = args->target,
                       .array = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                           .object = group, .key = "sourceIndexes"));
        unsigned long long place = 0ULL;
        for (unsigned long long item =
                 EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = args->target, .array = sources);
             (item != CLI_REPLY_NONE) && (index != CLI_REPLY_NONE);
             item = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->target, .item = item))
        {
            while ((index != CLI_REPLY_NONE) &&
                   (EMBED_CALL(cli_reply_at.whole, CliReplyAtCfg, .reply = args->target, .at = index) < place))
            {
                index = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->target, .item = index);
            }
            const int listed = (index != CLI_REPLY_NONE) && (EMBED_CALL(cli_reply_at.whole, CliReplyAtCfg,
                                                                        .reply = args->target, .at = index) == place);
            place += 1ULL;
            if (!listed)
            {
                continue;
            }
            char path[CLI_PATH_MAX_LEN];
            if (EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->target,
                           .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->target,
                                            .object = item, .key = "path"),
                           .out = path, .capacity = sizeof(path)))
            {
                EMBED_CALL(cli_entry_init, CompileDbCmakeCtx, .source = args->source, .build = args->build,
                           .path = path, .compiler = one, .msvc = msvc, .entries = args->entries);
            }
            index = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->target, .item = index);
        }
    }
}

// the configuration of the codemodel the build's folder is named for, as orior_cli names it, or the first
EMBED_INLINE unsigned long long cli_configuration_get(const CompileDbCmakeCtx *args)
{
    const char *named = args->build;
    const unsigned long long named_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->build,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->build, .cap = CLI_POOL_BYTES}), .set = "/\\");
    named = (named_end != 0ULL) ? (args->build + named_end) : named;
    const unsigned long long first =
        EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = args->model, .array = args->configurations);
    for (unsigned long long at = first; at != CLI_REPLY_NONE;
         at = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = args->model, .item = at))
    {
        char name[CLI_NAME_MAX_LEN];
        int same = EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = args->model,
                              .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = args->model,
                                               .object = at, .key = "name"),
                              .out = name, .capacity = sizeof(name)) &&
                   (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}) ==
                    cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_POOL_BYTES}));
        same = same && cellul.eq(&(CatenaFinitaCfg){.src = name, .other = named, .cap = CLI_POOL_BYTES, .ci = 1});
        if (same)
        {
            return at;
        }
    }
    return first;
}

unsigned int cli_compile_cmake_entries(const CliCompileDbIndexCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    CliReply reply = {NULL, 0ULL};
    if (!EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = args->index_read,
                    .out = folder) ||
        !EMBED_CALL(cli_reply_path.read, CliReplyPathCfg, .path = args->index_read, .reply_out = &reply))
    {
        return CLI_JSON_NONE;
    }
    const unsigned long long answers =
        EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply,
                   .object = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = &reply), .key = "reply");
    CliReply model = {NULL, 0ULL};
    const int modelled = EMBED_CALL(cli_reply_file_read, CompileDbCmakeCtx, .reply = &reply,
                                    .node = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply,
                                                       .object = answers, .key = "codemodel-v2"),
                                    .folder = folder, .file = &model);
    EMBED_CALL(cli_toolchains_read, CompileDbCmakeCtx, .reply = &reply,
               .node = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &reply, .object = answers,
                                  .key = "toolchains-v1"),
               .folder = folder);
    EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &reply);
    if (!modelled)
    {
        return CLI_JSON_NONE;
    }
    const unsigned long long paths =
        EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &model,
                   .object = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = &model), .key = "paths");
    const unsigned long long configurations =
        EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &model,
                   .object = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = &model), .key = "configurations");
    char build[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = &model,
                    .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &model, .object = paths,
                                     .key = "source"),
                    .out = args->source, .capacity = CLI_PATH_MAX_LEN) ||
        !EMBED_CALL(cli_reply_at_out.string, CliReplyAtOutCfg, .reply = &model,
                    .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &model, .object = paths,
                                     .key = "build"),
                    .out = build, .capacity = sizeof(build)) ||
        (EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = &model, .array = configurations) ==
         CLI_REPLY_NONE))
    {
        EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &model);
        return CLI_JSON_NONE;
    }
    const unsigned long long configuration = EMBED_CALL(cli_configuration_get, CompileDbCmakeCtx, .model = &model,
                                                        .configurations = configurations, .build = build);
    const unsigned int entries = EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg,
                                            .json = &g_cli_gather.json, .type = CFG_JSON_ARRAY);
    const unsigned long long targets = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &model,
                                                  .object = configuration, .key = "targets");
    for (unsigned long long at = EMBED_CALL(cli_reply_array.first, CliReplyArrayCfg, .reply = &model, .array = targets);
         at != CLI_REPLY_NONE; at = EMBED_CALL(cli_reply.next, CliReplyCfg, .reply = &model, .item = at))
    {
        CliReply target = {NULL, 0ULL};
        if (EMBED_CALL(cli_reply_file_read, CompileDbCmakeCtx, .reply = &model, .node = at, .folder = folder,
                       .file = &target))
        {
            EMBED_CALL(cli_target_entries, CompileDbCmakeCtx, .target = &target, .source = args->source, .build = build,
                       .entries = entries);
            EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &target);
        }
    }
    EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &model);
    return entries;
}
