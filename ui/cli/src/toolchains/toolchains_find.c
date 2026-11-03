// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_find.c: each tool found, as the search last found it where nothing it reads has changed, every tool
// written as the window reads it, a tool's version queried, and its install line and page
#include "../command_line/callback_cfg.h"
#include "../machine/report.h"
#include "../text/text.h"
#include "toolchains_internal.h"

// how long a program may take to report its version before it is stopped, in milliseconds
#define CLI_PATIENCE 6000ULL

// the PATH's folders for one call, and the folders a tool's places name, under the lock
char (*g_cli_toolchains_path)[CLI_PATH_MAX_LEN];
static char s_positions[64][CLI_PATH_MAX_LEN];

unsigned int cli_toolchains_find_path(void)
{
    unsigned int count = 0u;
    g_cli_cells->persistent_buf_release(g_cli_toolchains_path, &g_cli_toolchains_cells);
    g_cli_toolchains_path = cli_toolchains_path_allocated(&count, &g_cli_toolchains_cells);
    return count;
}

void cli_toolchains_find_release(const CliToolchainsJsonCtx *args)
{
    if (args->json != NULL)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = args->json);
    }
    g_cli_cells->persistent_buf_release(g_cli_toolchains_path, &g_cli_toolchains_cells);
    g_cli_toolchains_path = NULL;
}

typedef struct
{
    const CliJson *json;
    unsigned int tool;
    const char *dir;
    CliFound *found;
    const char *state;
    const char *program;
    VerbaLinea *out;
    const char *text;
    const CliFound *found_read;
    VerbaLinea *err_msg;
    const char *key;
    int added;
    int (*seen)(const CliEachKindCfg *args);
    const void *context_read;
    unsigned long long length;
    unsigned long long *length_out;
    const CliSpawned *spawned_read;
} ToolchainsFindCtx;
// whether the folder is passed over for the tool: a part of it is a name `notIn` lists, case aside
EMBED_INLINE int cli_passed_over(const ToolchainsFindCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int not_in = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->tool, .name = "notIn");
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = not_in, .kind = CFG_JSON_ARRAY)
                                 ? json->node[not_in].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = item, .length_pointer = NULL);
        const unsigned long long length =
            (name != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) : 0ULL;
        const char *part = args->dir;
        while ((name != NULL) && (part[0] != '\0'))
        {
            const char *end = part;
            end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ULL,
                              .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}), .set = "/\\");
            if (((unsigned long long)(end - part) == length) &&
                cellul.starts(&(CatenaFinitaCfg){.src = part, .other = name, .cap = length, .ci = 1}))
            {
                return 1;
            }
            part = (end[0] != '\0') ? (end + 1) : end;
        }
    }
    return 0;
}

// whether the folder contains what the tool's `contains` names, where it names one
EMBED_INLINE int cli_dir_has_tool(const ToolchainsFindCtx *args)
{
    const CliJson *json = args->json;
    const char *const inner =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->tool, .name = "contains"),
                   .length_pointer = NULL);
    if (inner == NULL)
    {
        return 1;
    }
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, args->dir);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, inner);
    return (mmgr_line_finish(&line) != 0ULL) &&
           EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = path);
}

// the program found set with its state, and its folder
EMBED_INLINE void cli_set(const ToolchainsFindCtx *args)
{
    const char *state = args->state;
    args->found->state = state;
    args->found->has_program = 1;
    memor.cpy(&(MemoriaCfg){.dst = args->found->program,
                            .src = args->program,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->program, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = args->found->folder,
                            .src = args->program,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->program, .cap = CLI_CELLS_POOL}) + 1u});
    char *cut = args->found->folder + cellul.len(&(CatenaFinitaCfg){.src = args->found->folder, .cap = CLI_CELLS_POOL});
    cut = args->found->folder + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                           .text = args->found->folder,
                                           .at = (unsigned long long)(cut - args->found->folder), .set = "/\\");
    cut -= (cut > args->found->folder) ? 1 : 0;
    cut[0] = '\0';
}

void cli_toolchains_find(const CliToolchainsJsonCfg *args)
{
    const CliJson *json = args->json;
    unsigned int tool = args->tool;
    const char (*path)[CLI_PATH_MAX_LEN] = args->path_callback;
    unsigned int count = args->count;
    CliFound *found = args->found;
    found->state = "missing";
    found->has_program = 0;
    found->program[0] = '\0';
    found->folder[0] = '\0';
    char program[CLI_PATH_MAX_LEN];
    const char *const env =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = tool, .name = "env"),
                   .length_pointer = NULL);
    if ((env != NULL) && EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = env,
                                    .value_out = program, .capacity = sizeof(program)))
    {
        EMBED_CALL(cli_set, ToolchainsFindCtx, .found = found, .state = "env", .program = program);
        return;
    }
    const char *const id =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = tool, .name = "id"),
                   .length_pointer = NULL);
    CliTreeEnvironment environment;
    if ((id != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = id, .other = "python", .cap = CLI_CELLS_POOL})) &&
        EMBED_CALL(cli_toolchains_table.environment, CliToolchainsCfg, .environment_out = &environment) &&
        environment.has_python)
    {
        EMBED_CALL(cli_set, ToolchainsFindCtx, .found = found, .state = "tree", .program = environment.python);
        return;
    }
    if ((id == NULL) || EMBED_CALL(cli_toolchains_id_internal.found_cache, CliToolchainsIdCtx, .id = id,
                                   .path_callback = path, .count = count, .found = found))
    {
        return;
    }
    char chosen[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_toolchains_id_folder.choose, CliToolchainsIdFolderCfg, .id = id, .folder = chosen) &&
        EMBED_CALL(cli_toolchains_json_dir.tool_program_in, CliToolchainsJsonDirCfg, .json = json, .tool = tool,
                   .dir = chosen, .path = program))
    {
        EMBED_CALL(cli_set, ToolchainsFindCtx, .found = found, .state = "chosen", .program = program);
        EMBED_CALL(cli_toolchains_id_path_internal.found_append, CliToolchainsIdPathCtx, .id = id,
                   .path_callback = path, .count = count, .found_read = found);
        return;
    }
    const unsigned int programs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = tool, .name = "programs");
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = programs, .kind = CFG_JSON_ARRAY)
                                 ? json->node[programs].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = item, .length_pointer = NULL);
        for (unsigned int at = 0u; (name != NULL) && (at < count); at += 1u)
        {
            if (!EMBED_CALL(cli_passed_over, ToolchainsFindCtx, .json = json, .tool = tool, .dir = path[at]) &&
                EMBED_CALL(cli_dir_has_tool, ToolchainsFindCtx, .json = json, .tool = tool, .dir = path[at]) &&
                EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = path[at], .names = &name,
                           .count = 1u, .path = program))
            {
                EMBED_CALL(cli_set, ToolchainsFindCtx, .found = found, .state = "path", .program = program);
                EMBED_CALL(cli_toolchains_id_path_internal.found_append, CliToolchainsIdPathCtx, .id = id,
                           .path_callback = path, .count = count, .found_read = found);
                return;
            }
        }
    }
    const unsigned int positions =
        EMBED_CALL(cli_toolchains_json_count.folders_for, CliToolchainsJsonCountCfg, .json = json,
                   .by_system = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = tool, .name = "places"),
                   .folders = s_positions, .count = 0u, .capacity = 64u);
    for (unsigned int at = 0u; at < positions; at += 1u)
    {
        if (!EMBED_CALL(cli_passed_over, ToolchainsFindCtx, .json = json, .tool = tool, .dir = s_positions[at]) &&
            EMBED_CALL(cli_dir_has_tool, ToolchainsFindCtx, .json = json, .tool = tool, .dir = s_positions[at]) &&
            EMBED_CALL(cli_toolchains_json_dir.tool_program_in, CliToolchainsJsonDirCfg, .json = json, .tool = tool,
                       .dir = s_positions[at], .path = program))
        {
            EMBED_CALL(cli_set, ToolchainsFindCtx, .found = found, .state = "found", .program = program);
            EMBED_CALL(cli_toolchains_id_path_internal.found_append, CliToolchainsIdPathCtx, .id = id,
                       .path_callback = path, .count = count, .found_read = found);
            return;
        }
    }
    EMBED_CALL(cli_toolchains_id_path_internal.found_append, CliToolchainsIdPathCtx, .id = id, .path_callback = path,
               .count = count, .found_read = found);
}

// a text written as a JSON string, or null where it is none
EMBED_INLINE void cli_quoted_or_null(const ToolchainsFindCtx *args)
{
    VerbaLinea *out = args->out;
    const char *text = args->text;
    if (text == NULL)
    {
        mmgr_line_put(out, "null");
        return;
    }
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = text,
               .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// what a spawned program wrote, its output then its errors, each ended with a line's end and the whole with a 0, in a
// buffer of 64 KiB; what does not fit is not read
EMBED_INLINE const char *cli_version_output_read(const ToolchainsFindCtx *args)
{
    static char s_output_buf[1u << 16u];
    unsigned long long read = 0ULL;
    for (unsigned int stream = 0u; stream < 2u; stream += 1u)
    {
        const unsigned long long pipe = (stream == 0u) ? args->spawned_read->out : args->spawned_read->err;
        // two bytes stay free of what is read, for the line's end and the 0 after it
        unsigned long long got = 1ULL;
        while ((got != 0ULL) && ((read + 2ULL) < sizeof(s_output_buf)))
        {
            const unsigned long long capacity = sizeof(s_output_buf) - read - 2ULL;
            got = EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                             .pipe_handle = pipe, .piece = s_output_buf + read, .capacity = capacity);
            read += (got < capacity) ? got : capacity;
        }
        s_output_buf[read] = '\n';
        read += (read + 1ULL < sizeof(s_output_buf)) ? 1ULL : 0ULL;
    }
    s_output_buf[read] = '\0';
    return s_output_buf;
}

// whether the `length` bytes at `text` hold a dotted number: a digit, a point and a digit
EMBED_INLINE int cli_version_dotted(const ToolchainsFindCtx *args)
{
    for (unsigned long long at = 0ULL; (at + 3ULL) <= args->length; at += 1ULL)
    {
        const char *const three = args->text + at;
        if ((three[0] >= '0') && (three[0] <= '9') && (three[1] == '.') && (three[2] >= '0') && (three[2] <= '9'))
        {
            return 1;
        }
    }
    return 0;
}

// the line of `text` that tells a version: the first holding a dotted number, else the first not blank, its white
// either side trimmed and its length put in `length_out`; NULL where every line is blank
EMBED_INLINE const char *cli_version_line_pick(const ToolchainsFindCtx *args)
{
    const char *first = NULL;
    unsigned long long first_length = 0ULL;
    const char *at = args->text;
    while (at[0] != '\0')
    {
        const char *const end =
            at + EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL,
                            .size = cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}), .set = "\n");
        const char *const from =
            at + EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL,
                            .size = (unsigned long long)(end - at), .set = CLI_TEXT_WHITE);
        const unsigned long long length =
            EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                       .at = (unsigned long long)(end - from), .set = CLI_TEXT_WHITE);
        if (EMBED_CALL(cli_version_dotted, ToolchainsFindCtx, .text = from, .length = length))
        {
            *args->length_out = length;
            return from;
        }
        if ((length > 0ULL) && (first == NULL))
        {
            first = from;
            first_length = length;
        }
        at = (end[0] != '\0') ? (end + 1) : end;
    }
    *args->length_out = first_length;
    return first;
}

// the bytes of the first 120 letters of the `length` bytes at `text`, a letter whole
EMBED_INLINE unsigned long long cli_version_cut(const ToolchainsFindCtx *args)
{
    unsigned long long cut = 0ULL;
    unsigned int letters = 0u;
    while ((cut < args->length) && (letters < 120u))
    {
        cut += 1ULL;
        while ((cut < args->length) && ((((unsigned char)args->text[cut]) & 0xc0u) == 0x80u))
        {
            cut += 1ULL;
        }
        letters += 1u;
    }
    return cut;
}

// What tool `tool` reports its version is into `out`: the first line it writes with a dotted number, else its first
// line, at most 120 letters. 1, or 0 with why given
static int cli_version_get(const ToolchainsFindCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea *err_msg = args->err_msg;
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->tool, .name = "name"),
                   .length_pointer = NULL);
    const unsigned int version = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->tool, .name = "version");
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = version,
                    .kind = CFG_JSON_ARRAY))
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is not queried for its version: running it opens a window or takes long");
        return 0;
    }
    if (!args->found_read->has_program)
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is not found");
        return 0;
    }
    const char *words[32];
    unsigned int count = 0u;
    words[count] = args->found_read->program;
    count += 1u;
    for (unsigned int item = json->node[version].first; (item != CLI_JSON_NONE) && (count < 31u);
         item = json->node[item].next)
    {
        const char *const word_text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                 .json_read = json, .node = item, .length_pointer = NULL);
        if (word_text != NULL)
        {
            words[count] = word_text;
            count += 1u;
        }
    }
    words[count] = NULL;
    CliSpawned spawned;
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn_as, CliWordsErrMsgCtx, .priority = CLI_PRIORITY_GUEST,
                    .spawned = &spawned, .words = words, .folder = NULL, .settings = NULL, .count = 0u, .detached = 1,
                    .err_msg = err_msg))
    {
        return 0;
    }
    // the output read first, then the errors, each until it ends or the patience runs out
    const unsigned long long began = cli_clock_milliseconds();
    long long code = 0LL;
    while (!EMBED_CALL(cli_spawned_internal.spawn_poll, CliSpawnedCtx, .spawned = &spawned, .code = &code))
    {
        if ((cli_clock_milliseconds() - began) >= CLI_PATIENCE)
        {
            EMBED_CALL(cli_id_internal.spawn_tree_end, CliIdCtx, .id_u64 = spawned.id);
            EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &spawned);
            EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &spawned);
            mmgr_line_put(err_msg, name);
            mmgr_line_put(err_msg, " did not report its version in 6 s");
            return 0;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 40u);
    }
    const char *const output = EMBED_CALL(cli_version_output_read, ToolchainsFindCtx, .spawned_read = &spawned);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &spawned);
    unsigned long long length = 0ULL;
    const char *const line =
        EMBED_CALL(cli_version_line_pick, ToolchainsFindCtx, .text = output, .length_out = &length);
    if (line == NULL)
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " printed nothing");
        return 0;
    }
    mmgr_line_put_n(args->out, line, EMBED_CALL(cli_version_cut, ToolchainsFindCtx, .text = line, .length = length));
    return 1;
}

int cli_toolchains_version(const CliToolchainsIdErrMsgCfg *args)
{
    CliJson json;
    const unsigned int tool =
        EMBED_CALL(cli_toolchains_json_manifest.tool, CliToolchainsJsonManifestCfg, .json_out = &json,
                   .manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                          .json_out = &json, .nodes_more = 0u, .text_more = 0ULL),
                   .id = args->id, .err_msg = args->err_msg);
    int done = 0;
    if (tool != CLI_JSON_NONE)
    {
        CliFound found;
        const unsigned int count = cli_toolchains_find_path();
        EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = &json, .tool = tool,
                   .path_callback = (const char (*)[CLI_PATH_MAX_LEN])g_cli_toolchains_path, .count = count,
                   .found = &found);
        done = EMBED_CALL(cli_version_get, ToolchainsFindCtx, .json = &json, .tool = tool, .found_read = &found,
                          .out = args->out, .err_msg = args->err_msg);
    }
    EMBED_CALL(cli_toolchains_json_internal.find_release, CliToolchainsJsonCtx, .json = &json);
    return done;
}

// a member of a tool written: its text, or its list of texts, or null
EMBED_INLINE void cli_member_write(const ToolchainsFindCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea *out = args->out;
    const unsigned int value = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->tool, .name = args->key);
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = value,
                   .kind = CFG_JSON_ARRAY))
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = value,
                   .out = out, .indent = 0u, .sorted = 0);
        return;
    }
    EMBED_CALL(cli_quoted_or_null, ToolchainsFindCtx, .out = out,
               .text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                                  .node = value, .length_pointer = NULL));
}

// a tool's id, name, group and system, each quoted, empty where it gives none, then its programs and its formats, an
// empty list where it gives none, written into `out`
EMBED_INLINE void cli_tool_names_write(const ToolchainsFindCtx *args)
{
    const CliJson *const json = args->json;
    static const char *const s_keys[4] = {"id", "name", "group", "for"};
    for (unsigned int key = 0u; key < 4u; key += 1u)
    {
        mmgr_line_put(args->out, (key != 0u) ? ",\"" : "\"");
        mmgr_line_put(args->out, s_keys[key]);
        mmgr_line_put(args->out, "\":");
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->tool, .name = s_keys[key]),
                       .length_pointer = NULL);
        EMBED_CALL(cli_quoted_or_null, ToolchainsFindCtx, .out = args->out, .text = (text != NULL) ? text : "");
    }
    mmgr_line_put(args->out, ",\"programs\":");
    EMBED_CALL(cli_member_write, ToolchainsFindCtx, .json = json, .tool = args->tool, .key = "programs",
               .out = args->out);
    mmgr_line_put(args->out, ",\"formats\":");
    const unsigned int formats = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->tool, .name = "formats");
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = formats,
                   .kind = CFG_JSON_ARRAY))
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = formats,
                   .out = args->out, .indent = 0u, .sorted = 0);
    }
    else
    {
        mmgr_line_put(args->out, "[]");
    }
}

// whether a tool is used: it is needed, or `added` to the tree, or `seen` answers for one of the kinds it `uses`
EMBED_INLINE int cli_tool_used(const ToolchainsFindCtx *args)
{
    const CliJson *const json = args->json;
    if (args->added || EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                  .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                     .json_read = json, .object = args->tool, .name = "needed"),
                                  .kind = CFG_JSON_TRUE))
    {
        return 1;
    }
    const unsigned int uses = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->tool, .name = "uses");
    if ((args->seen == NULL) || !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                            .node = uses, .kind = CFG_JSON_ARRAY))
    {
        return 0;
    }
    for (unsigned int use = json->node[uses].first; use != CLI_JSON_NONE; use = json->node[use].next)
    {
        const char *const kind = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = use, .length_pointer = NULL);
        if ((kind != NULL) && EMBED_CALL(args->seen, CliEachKindCfg, .context = args->context_read,
                                         .kind = (kind[0] == '*') ? (kind + 1) : kind))
        {
            return 1;
        }
    }
    return 0;
}

// whether a tool's manifest lists its version command
EMBED_INLINE int cli_tool_versioned(const ToolchainsFindCtx *args)
{
    return EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = args->json, .object = args->tool, .name = "version"),
                      .kind = CFG_JSON_ARRAY);
}

// a tool's setup, versioned, needed, added and used marks written into `out`; `text` is the tool's id, NULL where it
// gives none, and `added` is set from it
EMBED_INLINE void cli_tool_flags_write(const ToolchainsFindCtx *args)
{
    const CliJson *const json = args->json;
    mmgr_line_put(args->out, (EMBED_CALL(cli_toolchains_json_map.for_system, CliToolchainsJsonMapCfg, .json = json,
                                         .map = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                           .json_read = json, .object = args->tool, .name = "setup")) !=
                              CLI_JSON_NONE)
                                 ? ",\"setup\":true"
                                 : ",\"setup\":false");
    mmgr_line_put(args->out, EMBED_CALL(cli_tool_versioned, ToolchainsFindCtx, .json = json, .tool = args->tool)
                                 ? ",\"versioned\":true"
                                 : ",\"versioned\":false");
    mmgr_line_put(args->out, EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                           .json_read = json, .object = args->tool, .name = "needed"),
                                        .kind = CFG_JSON_TRUE)
                                 ? ",\"needed\":true"
                                 : ",\"needed\":false");
    const int added =
        (args->text != NULL) && EMBED_CALL(cli_toolchains_id_folder.add, CliToolchainsIdFolderCfg, .id = args->text);
    mmgr_line_put(args->out, added ? ",\"added\":true" : ",\"added\":false");
    mmgr_line_put(args->out, EMBED_CALL(cli_tool_used, ToolchainsFindCtx, .json = json, .tool = args->tool,
                                        .added = added, .seen = args->seen, .context_read = args->context_read)
                                 ? ",\"used\":true"
                                 : ",\"used\":false");
}

// the version a found tool's version command prints, written into `out` as its "version" key; nothing where the
// manifest lists no version command, the program is not found or the command fails
EMBED_INLINE void cli_tool_version_write(const ToolchainsFindCtx *args)
{
    if (!args->found_read->has_program ||
        !EMBED_CALL(cli_tool_versioned, ToolchainsFindCtx, .json = args->json, .tool = args->tool))
    {
        return;
    }
    char line[512];
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea version_line = {line, sizeof(line), 0ULL};
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ULL};
    if (EMBED_CALL(cli_version_get, ToolchainsFindCtx, .json = args->json, .tool = args->tool,
                   .found_read = args->found_read, .out = &version_line, .err_msg = &err_msg))
    {
        mmgr_line_finish(&version_line);
        mmgr_line_put(args->out, ",\"version\":");
        EMBED_CALL(cli_quoted_or_null, ToolchainsFindCtx, .out = args->out, .text = line);
    }
}

void cli_toolchains_check(const CliToolchainsVersionsCfg *args)
{
    VerbaLinea *out = args->out;
    CliJson json;
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                             .json_out = &json, .nodes_more = 0u, .text_more = 0ULL);
    const unsigned int count = cli_toolchains_find_path();
    cli_toolchains_found_ref();
    mmgr_line_ch(out, '[');
    int first = 1;
    for (unsigned int tool = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                        .node = manifest, .kind = CFG_JSON_ARRAY)
                                 ? json.node[manifest].first
                                 : CLI_JSON_NONE;
         tool != CLI_JSON_NONE; tool = json.node[tool].next)
    {
        CliFound found;
        EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = &json, .tool = tool,
                   .path_callback = (const char (*)[CLI_PATH_MAX_LEN])g_cli_toolchains_path, .count = count,
                   .found = &found);
        const char *const id =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = tool, .name = "id"),
                       .length_pointer = NULL);
        char chosen[CLI_PATH_MAX_LEN];
        const int has_chosen = (id != NULL) && EMBED_CALL(cli_toolchains_id_folder.choose, CliToolchainsIdFolderCfg,
                                                          .id = id, .folder = chosen);
        const unsigned int install =
            EMBED_CALL(cli_toolchains_json_map.for_system, CliToolchainsJsonMapCfg, .json = &json,
                       .map = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                         .object = tool, .name = "install"));
        mmgr_line_put(out, first ? "{" : ",{");
        first = 0;
        EMBED_CALL(cli_tool_names_write, ToolchainsFindCtx, .json = &json, .tool = tool, .out = out);
        mmgr_line_put(out, ",\"state\":");
        EMBED_CALL(cli_quoted_or_null, ToolchainsFindCtx, .out = out, .text = found.state);
        mmgr_line_put(out, ",\"program\":");
        EMBED_CALL(cli_quoted_or_null, ToolchainsFindCtx, .out = out, .text = found.has_program ? found.program : NULL);
        mmgr_line_put(out, ",\"folder\":");
        EMBED_CALL(cli_quoted_or_null, ToolchainsFindCtx, .out = out, .text = found.has_program ? found.folder : NULL);
        mmgr_line_put(out, ",\"chosen\":");
        EMBED_CALL(cli_quoted_or_null, ToolchainsFindCtx, .out = out, .text = has_chosen ? chosen : NULL);
        mmgr_line_put(out, ",\"install\":");
        EMBED_CALL(cli_quoted_or_null, ToolchainsFindCtx, .out = out,
                   .text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                      .json_read = &json, .node = install, .length_pointer = NULL));
        EMBED_CALL(cli_tool_flags_write, ToolchainsFindCtx, .json = &json, .tool = tool, .out = out, .text = id,
                   .seen = args->seen, .context_read = args->context_read);
        if (args->versions)
        {
            EMBED_CALL(cli_tool_version_write, ToolchainsFindCtx, .json = &json, .tool = tool, .found_read = &found,
                       .out = out);
        }
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
    cli_toolchains_found_let();
    EMBED_CALL(cli_toolchains_json_internal.find_release, CliToolchainsJsonCtx, .json = &json);
}

int cli_toolchains_setup_line(const CliToolchainsIdErrMsgCfg *args)
{
    CliJson json;
    const unsigned int tool =
        EMBED_CALL(cli_toolchains_json_manifest.tool, CliToolchainsJsonManifestCfg, .json_out = &json,
                   .manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                          .json_out = &json, .nodes_more = 0u, .text_more = 0ULL),
                   .id = args->id, .err_msg = args->err_msg);
    const char *const line =
        (tool != CLI_JSON_NONE)
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                         .node = EMBED_CALL(cli_toolchains_json_map.for_system, CliToolchainsJsonMapCfg, .json = &json,
                                            .map = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                              .json_read = &json, .object = tool, .name = "setup")),
                         .length_pointer = NULL)
            : NULL;
    if ((tool != CLI_JSON_NONE) && (line == NULL))
    {
        const char *const name =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = tool, .name = "name"),
                       .length_pointer = NULL);
        mmgr_line_put(args->err_msg, "orior_cli has no way to install ");
        mmgr_line_put(args->err_msg, (name != NULL) ? name : args->id);
        mmgr_line_put(args->err_msg, " on ");
        mmgr_line_put(args->err_msg, cli_toolchains_system());
        mmgr_line_put(args->err_msg, ": its install page has the steps");
    }
    if (line != NULL)
    {
        mmgr_line_put(args->out, line);
    }
    EMBED_CALL(cli_toolchains_json_internal.find_release, CliToolchainsJsonCtx, .json = &json);
    return line != NULL;
}

int cli_toolchains_open_install(const CliToolchainsIdErrMsgCfg *args)
{
    CliJson json;
    char url[CLI_PATH_MAX_LEN];
    const unsigned int tool =
        EMBED_CALL(cli_toolchains_json_manifest.tool, CliToolchainsJsonManifestCfg, .json_out = &json,
                   .manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                          .json_out = &json, .nodes_more = 0u, .text_more = 0ULL),
                   .id = args->id, .err_msg = args->err_msg);
    const char *const page =
        (tool != CLI_JSON_NONE)
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                         .node = EMBED_CALL(cli_toolchains_json_map.for_system, CliToolchainsJsonMapCfg, .json = &json,
                                            .map = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                              .json_read = &json, .object = tool, .name = "install")),
                         .length_pointer = NULL)
            : NULL;
    if ((tool != CLI_JSON_NONE) && (page == NULL))
    {
        const char *const name =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = tool, .name = "name"),
                       .length_pointer = NULL);
        mmgr_line_put(args->err_msg, (name != NULL) ? name : args->id);
        mmgr_line_put(args->err_msg, " has no install page for ");
        mmgr_line_put(args->err_msg, cli_toolchains_system());
    }
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = url, .text = page, .otherwise = "");
    EMBED_CALL(cli_toolchains_json_internal.find_release, CliToolchainsJsonCtx, .json = &json);
    if (page == NULL)
    {
        return 0;
    }
    if (!cellul.starts(&(CatenaFinitaCfg){.src = url, .other = "https://", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(args->err_msg, url);
        mmgr_line_put(args->err_msg, " is not an https page");
        return 0;
    }
    if (!EMBED_CALL(cli_report_err_msg.open_url, CliReportErrMsgCfg, .url = url, .err_msg = args->err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, url);
    return 1;
}
