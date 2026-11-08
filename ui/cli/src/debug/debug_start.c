// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_start.c: what a session is started with, as its start specifies: its language, the adapter that debugs it, the
// program built where its language is built, and the request, launch or attach, with its arguments, its name and the
// session that started it
#include "debug_start.h"
#include "../tests/cargo_tests.h"
#include "../tests/test_runs.h"

typedef struct
{
    const CliJson *json;
    unsigned int object;
    const char *name;
    char *out;
    unsigned long long capacity;
    VerbaLinea *err_msg;
    unsigned long long parent;
    char *language;
    CliDebugSession *place;
    const char *root;
    unsigned int start;
    CliDebugPlan *plan;
} DebugStartCtx;

// a string member `name` of the object `object` of `json` into `out`, sized for `capacity`: 1, or 0 with the member
// named missing where it is not a string
EMBED_INLINE int start_field(const DebugStartCtx *args)
{
    const CliJson *json = args->json;
    unsigned int object = args->object;
    const char *name = args->name;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = object, .name = name),
                   .length_pointer = NULL);
    if (text == NULL)
    {
        mmgr_line_put(args->err_msg, "missing field `");
        mmgr_line_put(args->err_msg, name);
        mmgr_line_ch(args->err_msg, '`');
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->out,
                            .src = text,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES}) + 1u});
    return 1;
}

// the language of the session `parent` into `language`: 1, or 0 where it is not listed
EMBED_INLINE int start_parent_language(const DebugStartCtx *args)
{
    int found = 0;
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        const CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->reserved && session->list_entries && (session->info.id == args->parent))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = args->language,
                .src = session->info.language,
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = session->info.language, .cap = CLI_POOL_BYTES}) + 1u});
            found = 1;
        }
    }
    return found;
}

// the language, the adapter, and for a C file of a CMake or Meson build orior_cli configured the program that build
// makes from it, read into `plan`: 1, or 0 with why given
EMBED_INLINE int start_adapter(const DebugStartCtx *args)
{
    const char *root = args->root;
    const CliJson *json = args->json;
    VerbaLinea *err_msg = args->err_msg;
    CliJson *const to = &args->place->start;
    char path[CLI_PATH_MAX_LEN];
    unsigned long long parent = 0ULL;
    args->plan->has_created = 0;
    if ((cellul.eq(&(CatenaFinitaCfg){.src = args->plan->type, .other = "file", .cap = CLI_POOL_BYTES})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->plan->type, .other = "process", .cap = CLI_POOL_BYTES})))
    {
        if (!EMBED_CALL(start_field, DebugStartCtx, .json = json, .object = args->start, .name = "language",
                        .out = args->plan->language, .capacity = sizeof(args->plan->language), .err_msg = err_msg))
        {
            return 0;
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->plan->type, .other = "test", .cap = CLI_POOL_BYTES}))
    {
        if (!EMBED_CALL(start_field, DebugStartCtx, .json = json, .object = args->start, .name = "id",
                        .out = args->plan->id, .capacity = sizeof(args->plan->id), .err_msg = err_msg))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){
            .dst = args->plan->language,
            .src = EMBED_CALL(cli_cargo_tests_id.cargo_is_rust, CliCargoTestsIdCfg, .id = args->plan->id) ? "rust"
                                                                                                          : "python",
            .bytes = cellul.len(&(CatenaFinitaCfg){
                         .src = EMBED_CALL(cli_cargo_tests_id.cargo_is_rust, CliCargoTestsIdCfg, .id = args->plan->id)
                                    ? "rust"
                                    : "python",
                         .cap = CLI_POOL_BYTES}) +
                     1u});
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->plan->type, .other = "child", .cap = CLI_POOL_BYTES}))
    {
        if (!EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx,
                        .json = json,
                        .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->start, .name = "parent"),
                        .value_u64s = &parent))
        {
            mmgr_line_put(err_msg, "missing field `parent`");
            return 0;
        }
        if (!EMBED_CALL(start_parent_language, DebugStartCtx, .parent = parent, .language = args->plan->language))
        {
            mmgr_line_put(err_msg, "that session has ended");
            return 0;
        }
    }
    else if ((cellul.eq(&(CatenaFinitaCfg){.src = args->plan->type, .other = "address", .cap = CLI_POOL_BYTES})) ||
             (cellul.eq(&(CatenaFinitaCfg){.src = args->plan->type, .other = "record", .cap = CLI_POOL_BYTES})))
    {
        memor.cpy(&(MemoriaCfg){.dst = args->plan->language,
                                .src = "python",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "python", .cap = CLI_POOL_BYTES}) + 1u});
    }
    else
    {
        mmgr_line_put(err_msg, "unknown variant `");
        mmgr_line_put(err_msg, args->plan->type);
        mmgr_line_put(err_msg, "`, expected one of `file`, `test`, `process`, `address`, `child`, `record`");
        return 0;
    }
    args->plan->recording =
        (cellul.eq(&(CatenaFinitaCfg){.src = args->plan->type, .other = "record", .cap = CLI_POOL_BYTES}));
    if ((cellul.eq(&(CatenaFinitaCfg){.src = args->plan->type, .other = "file", .cap = CLI_POOL_BYTES})) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->plan->language, .other = "c", .cap = CLI_POOL_BYTES})))
    {
        char full[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(start_field, DebugStartCtx, .json = json, .object = args->start, .name = "path", .out = path,
                        .capacity = sizeof(path), .err_msg = err_msg))
        {
            return 0;
        }
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = full);
        args->plan->has_created = EMBED_CALL(cli_compile_db_root_file.program_get, CliCompileDbRootFileCfg,
                                             .root = root, .file = full, .program = &args->plan->made);
    }
    if (args->plan->recording)
    {
        args->plan->spec =
            EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_OBJECT);
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to, .object = args->plan->spec,
                   .name = "program",
                   .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                       .json = to, .text = "orior", .length = 5ULL));
        EMBED_CALL(
            cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to, .object = args->plan->spec,
            .name = "launch",
            .value = EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_OBJECT));
        memor.cpy(&(MemoriaCfg){
            .dst = args->plan->tool,
            .src = "orior's recorder",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "orior's recorder", .cap = CLI_POOL_BYTES}) + 1u});
        args->plan->tool_id[0] = '\0';
        args->plan->program[0] = '\0';
        return 1;
    }
    const char *ahead;
    if (!args->plan->has_created)
    {
        ahead = "";
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->plan->made.compiler, .other = "GNU", .cap = CLI_POOL_BYTES}))
    {
        ahead = "gcc";
    }
    else
    {
        ahead = "llvm";
    }
    return EMBED_CALL(cli_debug_err_msg_internal.adapter, CliDebugErrMsgCtx, .language = args->plan->language,
                      .ahead = ahead, .json_out = to, .spec = &args->plan->spec, .tool = args->plan->tool,
                      .tool_id = args->plan->tool_id, .program = args->plan->program, .err_msg = err_msg);
}

// the launch of a file: the program built where its language is built, the file itself where it runs from its source
EMBED_INLINE int start_file(const DebugStartCtx *args)
{
    const char *root = args->root;
    const CliJson *json = args->json;
    VerbaLinea *err_msg = args->err_msg;
    CliJson *const to = &args->place->start;
    char path[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char program[CLI_PATH_MAX_LEN];
    int built = 0;
    if (!EMBED_CALL(start_field, DebugStartCtx, .json = json, .object = args->start, .name = "path", .out = path,
                    .capacity = sizeof(path), .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = file);
    if (args->plan->has_created)
    {
        built = EMBED_CALL(cli_debug_err_msg_program_internal.build_program, CliDebugErrMsgProgramCtx,
                           .made = &args->plan->made, .program = program, .err_msg = err_msg);
        if (!built)
        {
            return 0;
        }
    }
    else if (!EMBED_CALL(cli_debug_err_msg_language_internal.build, CliDebugErrMsgLanguageCtx, .root = root,
                         .path = file, .language = args->plan->language, .ahead = args->plan->tool_id,
                         .program = program, .built = &built, .err_msg = err_msg))
    {
        return 0;
    }
    const char *const names[3] = {"program", "path", "root"};
    const char *const withs[3] = {built ? program : file, file, root};
    args->plan->request = "launch";
    args->plan->arguments =
        EMBED_CALL(cli_debug_json_internal.fill, CliDebugJsonCtx, .to = to, .json = to,
                   .from = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = to,
                                      .object = args->plan->spec, .name = "launch"),
                   .names = names, .withs = withs, .count = 3u, .numbered = 0);
    memor.cpy(&(MemoriaCfg){.dst = args->plan->name,
                            .src = path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}) + 1u});
    return 1;
}

// the launch of one test: a Rust test's program, built as cargo test builds it, run for that test alone, or pytest
// run on a Python test, in place of a program
EMBED_INLINE int start_test(const DebugStartCtx *args)
{
    const char *root = args->root;
    VerbaLinea *err_msg = args->err_msg;
    CliJson *const to = &args->place->start;
    const unsigned int launch = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = to,
                                           .object = args->plan->spec, .name = "launch");
    args->plan->request = "launch";
    memor.cpy(
        &(MemoriaCfg){.dst = args->plan->name,
                      .src = args->plan->id,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->plan->id, .cap = CLI_POOL_BYTES}) + 1u});
    if (EMBED_CALL(cli_cargo_tests_id.cargo_is_rust, CliCargoTestsIdCfg, .id = args->plan->id))
    {
        char program[CLI_PATH_MAX_LEN];
        char full[CLI_PATH_MAX_LEN];
        char folder[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_cargo_tests_id.cargo_binary, CliCargoTestsIdCfg, .root = root, .id = args->plan->id,
                        .program = program, .full = full, .package = folder, .err_msg = err_msg))
        {
            return 0;
        }
        const char *const names[3] = {"program", "path", "root"};
        const char *const withs[3] = {program, "", folder};
        args->plan->arguments = EMBED_CALL(cli_debug_json_internal.fill, CliDebugJsonCtx, .to = to, .json = to,
                                           .from = launch, .names = names, .withs = withs, .count = 3u, .numbered = 0);
        if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = to,
                        .node = args->plan->arguments, .type = CFG_JSON_OBJECT))
        {
            args->plan->arguments =
                EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_OBJECT);
        }
        const char *const words[4] = {full, "--exact", "--nocapture", "--test-threads=1"};
        const unsigned int args_local =
            EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_ARRAY);
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            EMBED_CALL(
                cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = to, .array = args_local,
                .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                    .json = to, .text = words[at],
                                    .length = cellul.len(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_POOL_BYTES})));
        }
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to,
                   .object = args->plan->arguments, .name = "args", .value = args_local);
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to,
                   .object = args->plan->arguments, .name = "cwd",
                   .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                       .json = to, .text = folder,
                                       .length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES})));
        return 1;
    }
    const char *const names[3] = {"program", "path", "root"};
    const char *const withs[3] = {"", "", root};
    args->plan->arguments = EMBED_CALL(cli_debug_json_internal.fill, CliDebugJsonCtx, .to = to, .json = to,
                                       .from = launch, .names = names, .withs = withs, .count = 3u, .numbered = 0);
    if (EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = to,
                   .node = args->plan->arguments, .type = CFG_JSON_OBJECT))
    {
        char python[CLI_PATH_MAX_LEN];
        const char *const words[4] = {args->plan->id, "-p", "no:cacheprovider", "-q"};
        const unsigned int args_local =
            EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_ARRAY);
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            EMBED_CALL(
                cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = to, .array = args_local,
                .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                    .json = to, .text = words[at],
                                    .length = cellul.len(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_POOL_BYTES})));
        }
        EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = to,
                   .object = args->plan->arguments, .name = "program");
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to,
                   .object = args->plan->arguments, .name = "module",
                   .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                       .json = to, .text = "pytest", .length = 6ULL));
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to,
                   .object = args->plan->arguments, .name = "args", .value = args_local);
        if (EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python))
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to,
                       .object = args->plan->arguments, .name = "python",
                       .value =
                           EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                      .json = to, .text = python,
                                      .length = cellul.len(&(CatenaFinitaCfg){.src = python, .cap = CLI_POOL_BYTES})));
        }
    }
    return 1;
}

// the attach to a process by its number, its native code where a session started it
EMBED_INLINE int start_process(const DebugStartCtx *args)
{
    const CliJson *json = args->json;
    CliJson *const to = &args->place->start;
    unsigned long long pid = 0ULL;
    if (!EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx, .json = json,
                    .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                       .object = args->start, .name = "pid"),
                    .value_u64s = &pid) ||
        (pid > 0xffffffffULL))
    {
        mmgr_line_put(args->err_msg, "missing field `pid`");
        return 0;
    }
    args->plan->has_parent =
        EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx, .json = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->start, .name = "parent"),
                   .value_u64s = &args->plan->parent);
    args->plan->has_pid = 1;
    args->plan->pid = (unsigned int)pid;
    char number[32];
    VerbaLinea pid_line = {number, sizeof(number), 0ULL};
    mmgr_line_decimal(&pid_line, pid, 1u);
    mmgr_line_finish(&pid_line);
    unsigned int template = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = to,
                                       .object = args->plan->spec, .name = "attach");
    if (template == CLI_JSON_NONE)
    {
        template = EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_OBJECT);
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to, .object = template,
                   .name = "pid",
                   .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                       .json = to, .text = "{pid}", .length = 5ULL));
    }
    const char *const names[2] = {"pid", "root"};
    const char *const withs[2] = {number, args->root};
    args->plan->request = "attach";
    args->plan->arguments = EMBED_CALL(cli_debug_json_internal.fill, CliDebugJsonCtx, .to = to, .json = to,
                                       .from = template, .names = names, .withs = withs, .count = 2u, .numbered = 1);
    VerbaLinea name = {args->plan->name, sizeof(args->plan->name), 0ULL};
    mmgr_line_put(&name, args->plan->has_parent ? "the native code of process " : "process ");
    mmgr_line_put(&name, number);
    mmgr_line_finish(&name);
    return 1;
}

// the attach to a debugger listening at an address, and to the child a session's program started
EMBED_INLINE int start_reach(const DebugStartCtx *args)
{
    const CliJson *json = args->json;
    CliJson *const to = &args->place->start;
    args->plan->request = "attach";
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->plan->type, .other = "child", .cap = CLI_POOL_BYTES}))
    {
        const unsigned int config = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = args->start, .name = "config");
        (void)EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx,
                         .json = json,
                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->start, .name = "parent"),
                         .value_u64s = &args->plan->parent);
        args->plan->has_parent = 1;
        args->plan->arguments =
            (config != CLI_JSON_NONE)
                ? EMBED_CALL(cli_json_tree_node.json_copy_from, CliJsonTreeNodeCfg, .to = to, .from = json,
                             .node = config)
                : EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_NULL);
        const char *const named =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = config, .name = "name"),
                       .length_pointer = NULL);
        unsigned long long sub = 0ULL;
        VerbaLinea name = {args->plan->name, sizeof(args->plan->name), 0ULL};
        if (named != NULL)
        {
            mmgr_line_put(&name, named);
        }
        else if (EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx,
                            .json = json,
                            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = config, .name = "subProcessId"),
                            .value_u64s = &sub))
        {
            mmgr_line_put(&name, "process ");
            mmgr_line_decimal(&name, sub, 1u);
        }
        else
        {
            mmgr_line_put(&name, "child");
        }
        mmgr_line_finish(&name);
        const unsigned int connect = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = json, .object = config, .name = "connect");
        unsigned long long port = 0ULL;
        args->plan->listening = EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned,
                                           CliDebugJsonNodeValueU64sCtx, .json = json,
                                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                              .json_read = json, .object = connect, .name = "port"),
                                           .value_u64s = &port);
        // a port retains its low 16 bits, as the protocol's ports are
        args->plan->port = (unsigned int)(port & 0xffffULL);
        memor.cpy(&(MemoriaCfg){
            .dst = args->plan->host,
            .src = EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx, .json = json,
                              .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = json, .object = connect, .name = "host"),
                              .fallback = "127.0.0.1"),
            .bytes = cellul.len(&(CatenaFinitaCfg){
                         .src = EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx,
                                           .json = json,
                                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                              .json_read = json, .object = connect, .name = "host"),
                                           .fallback = "127.0.0.1"),
                         .cap = CLI_POOL_BYTES}) +
                     1u});
        args->plan->ends_program = 1;
        return 1;
    }
    unsigned long long port = 0ULL;
    if (!EMBED_CALL(start_field, DebugStartCtx, .json = json, .object = args->start, .name = "host",
                    .out = args->plan->host, .capacity = sizeof(args->plan->host), .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx, .json = json,
                    .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                       .object = args->start, .name = "port"),
                    .value_u64s = &port) ||
        (port > 0xffffULL))
    {
        mmgr_line_put(args->err_msg, "missing field `port`");
        return 0;
    }
    args->plan->port = (unsigned int)port;
    args->plan->listening = 1;
    args->plan->ends_program = 0;
    args->plan->arguments =
        EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_OBJECT);
    const unsigned int connect =
        EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_OBJECT);
    EMBED_CALL(
        cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to, .object = connect, .name = "host",
        .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg, .json = to,
                            .text = args->plan->host,
                            .length = cellul.len(&(CatenaFinitaCfg){.src = args->plan->host, .cap = CLI_POOL_BYTES})));
    char number[32];
    VerbaLinea port_line = {number, sizeof(number), 0ULL};
    mmgr_line_decimal(&port_line, port, 1u);
    mmgr_line_finish(&port_line);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to, .object = connect,
               .name = "port",
               .value = EMBED_CALL(cli_json_tree_text_length_type.json_number_init, CliJsonTreeTextLengthTypeCfg,
                                   .json = to, .text = number));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to,
               .object = args->plan->arguments, .name = "connect", .value = connect);
    const char *const remote =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->start, .name = "remote"),
                   .length_pointer = NULL);
    if ((remote != NULL) && (remote[0] != '\0'))
    {
        const unsigned int mappings =
            EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_ARRAY);
        const unsigned int mapping =
            EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_OBJECT);
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to, .object = mapping,
                   .name = "localRoot",
                   .value =
                       EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                  .json = to, .text = args->root,
                                  .length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES})));
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to, .object = mapping,
                   .name = "remoteRoot",
                   .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                       .json = to, .text = remote,
                                       .length = cellul.len(&(CatenaFinitaCfg){.src = remote, .cap = CLI_POOL_BYTES})));
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = to, .array = mappings,
                   .value = mapping);
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to,
                   .object = args->plan->arguments, .name = "pathMappings", .value = mappings);
    }
    VerbaLinea name = {args->plan->name, sizeof(args->plan->name), 0ULL};
    mmgr_line_put(&name, args->plan->host);
    mmgr_line_ch(&name, ':');
    mmgr_line_put(&name, number);
    mmgr_line_finish(&name);
    return 1;
}

int cli_debug_plan(const CliDebugStartCfg *args)
{
    const CliJson *json = args->json;
    CliJson *const to = &args->place->start;
    // the spec and the arguments the session starts with, made in a tree of the most they take, its cell owned by the
    // place until it is freed
    g_cli_guard->persistent_buf_release(to->node, mmgr_owner_of_buf(cli_pool_series_cellularum(), to->node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = to, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    if (!EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = to,
                    .nodes_max = CLI_DEBUG_START_NODES, .text_max = CLI_DEBUG_START_TEXT,
                    .what = "the tree a debug session starts with", .owner = g_cli_debug_owner))
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the tree a debug session starts with");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = args->plan, .val = 0, .bytes = sizeof(*args->plan)});
    if (!EMBED_CALL(start_field, DebugStartCtx, .json = json, .object = args->start, .name = "kind",
                    .out = args->plan->type, .capacity = sizeof(args->plan->type), .err_msg = args->err_msg) ||
        !EMBED_CALL(start_adapter, DebugStartCtx, .place = args->place, .root = args->root, .json = json,
                    .start = args->start, .plan = args->plan, .err_msg = args->err_msg))
    {
        return 0;
    }
    int made = 0;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->plan->type, .other = "file", .cap = CLI_POOL_BYTES}))
    {
        made = EMBED_CALL(start_file, DebugStartCtx, .place = args->place, .root = args->root, .json = json,
                          .start = args->start, .plan = args->plan, .err_msg = args->err_msg);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->plan->type, .other = "test", .cap = CLI_POOL_BYTES}))
    {
        made = EMBED_CALL(start_test, DebugStartCtx, .place = args->place, .root = args->root, .plan = args->plan,
                          .err_msg = args->err_msg);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->plan->type, .other = "process", .cap = CLI_POOL_BYTES}))
    {
        made = EMBED_CALL(start_process, DebugStartCtx, .place = args->place, .root = args->root, .json = json,
                          .start = args->start, .plan = args->plan, .err_msg = args->err_msg);
    }
    else if (args->plan->recording)
    {
        char path[CLI_PATH_MAX_LEN];
        char full[CLI_PATH_MAX_LEN];
        char python[CLI_PATH_MAX_LEN];
        made = EMBED_CALL(start_field, DebugStartCtx, .json = json, .object = args->start, .name = "path", .out = path,
                          .capacity = sizeof(path), .err_msg = args->err_msg);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = path, .joined = full);
        python[0] = '\0';
        (void)EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python);
        args->plan->request = "launch";
        args->plan->arguments =
            EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_OBJECT);
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to,
                   .object = args->plan->arguments, .name = "program",
                   .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                       .json = to, .text = full,
                                       .length = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_POOL_BYTES})));
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to,
                   .object = args->plan->arguments, .name = "python",
                   .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                       .json = to, .text = python,
                                       .length = cellul.len(&(CatenaFinitaCfg){.src = python, .cap = CLI_POOL_BYTES})));
        memor.cpy(&(MemoriaCfg){.dst = args->plan->name,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}) + 1u});
    }
    else
    {
        made = EMBED_CALL(start_reach, DebugStartCtx, .place = args->place, .root = args->root, .json = json,
                          .start = args->start, .plan = args->plan, .err_msg = args->err_msg);
    }
    if (!made)
    {
        return 0;
    }
    const int launch =
        (cellul.eq(&(CatenaFinitaCfg){.src = args->plan->request, .other = "launch", .cap = CLI_POOL_BYTES}));
    if (launch &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->plan->language, .other = "python", .cap = CLI_POOL_BYTES})) &&
        !args->plan->recording &&
        EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = to,
                   .node = args->plan->arguments, .type = CFG_JSON_OBJECT))
    {
        // the processes a Python program starts are debugged with it
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to,
                   .object = args->plan->arguments, .name = "subProcess",
                   .value =
                       EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_TRUE));
    }
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->plan->type, .other = "child", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(cli_debug_json_language_internal.step_arguments, CliDebugJsonLanguageCtx,
                   .language = args->plan->language, .json = json,
                   .stepping = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->options, .name = "stepping"),
                   .to = to, .arguments = args->plan->arguments);
    }
    if (EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->options, .name = "entry")) &&
        launch)
    {
        if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = to,
                        .node = args->plan->arguments, .type = CFG_JSON_OBJECT))
        {
            args->plan->arguments =
                EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_OBJECT);
        }
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to,
                   .object = args->plan->arguments, .name = "stopOnEntry",
                   .value =
                       EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = to, .type = CFG_JSON_TRUE));
    }
    if (to->full)
    {
        mmgr_line_put(args->err_msg, "the session's start does not fit the buffer it is built in");
        return 0;
    }
    return 1;
}
