// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_chosen.c: the folders the reader gave orior, by tool, stored in toolchains.json in orior's own folder; and
// what each tool's search found, cached in toolchains_found.json there, so a tool is searched for again only where what
// the search reads has changed
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "toolchains_internal.h"

// A tool's search cached: its id, how it was found, as CliFound's `state` records, and its program, empty where it is
// missing
typedef struct
{
    char id[CLI_NAME_MAX_LEN];
    const char *state;
    char program[CLI_PATH_MAX_LEN];
} CliToolFound;

// The searches cached: what the search read as they were made, the PATH's folders, the manifest as built and the
// reader's own two files, as one hash; the tools, `count` of them in a cell of `most`; whether the file was read; the
// checks of every tool locking the cache, which is written once as the last lets it go; and whether it changed since
// it was written
typedef struct
{
    unsigned long long against;
    CliToolFound *tool;
    unsigned int count;
    unsigned int most;
    int read;
    unsigned int refs;
    int changed;
} CliToolsFound;

static CliToolsFound s_found;

// the states a search is cached with: those found by what the search reads, not by a variable or the tree
static const char *const s_found_states[4] = {"chosen", "path", "found", "missing"};

// the places the cache begins with
#define CLI_FOUND_MOST 1024u

typedef struct
{
    CliJson *json;
    char *path;
    unsigned long long hash;
    const void *bytes;
    unsigned long long size;
    const char (*path_callback)[CLI_PATH_MAX_LEN];
    unsigned int count;
    const char *id;
    const char *state;
    const char *program;
    const char *text;
    unsigned long long against;
} ToolchainsChosenCtx;
// The folders the reader gave, by tool, read into a tree sized to them with room for one folder more, which
// cli_json_free gives back: the object of them
EMBED_INLINE unsigned int cli_chosen_read(const ToolchainsChosenCtx *args)
{
    CliJson *json = args->json;
    char *path = args->path;
    const int named =
        EMBED_CALL(cli_toolchains_internal.file, CliToolchainsCtx, .name = "toolchains.json", .path = path);
    return EMBED_CALL(cli_toolchains_json_internal.file_read, CliToolchainsJsonCtx, .json = json,
                      .path_read = named ? path : "", .nodes_more = 1u,
                      .text_more = CLI_PATH_MAX_LEN + CLI_NAME_MAX_LEN);
}

// the tree given back, and the lock with it
EMBED_INLINE void cli_chosen_release(const ToolchainsChosenCtx *args)
{
    CliJson *json = args->json;
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = json);
}

int cli_toolchains_on_choose(const CliToolchainsIdFolderCfg *args)
{
    CliJson json;
    char path[CLI_PATH_MAX_LEN];
    const unsigned int chosen = EMBED_CALL(cli_chosen_read, ToolchainsChosenCtx, .json = &json, .path = path);
    const char *const given =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = chosen, .name = args->id),
                   .length_pointer = NULL);
    if (given != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->folder,
                                .src = given,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = given, .cap = CLI_CELLS_POOL}) + 1u});
    }
    EMBED_CALL(cli_chosen_release, ToolchainsChosenCtx, .json = &json);
    return given != NULL;
}

void cli_toolchains_chosen_each(const CliToolchainsContextCfg *args)
{
    static char s_ids[256][CLI_NAME_MAX_LEN];
    static char s_folders[256][CLI_PATH_MAX_LEN];
    CliJson json;
    char path[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    const unsigned int chosen = EMBED_CALL(cli_chosen_read, ToolchainsChosenCtx, .json = &json, .path = path);
    // a read the cells refused has no tree, and tells nothing
    if (chosen == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_chosen_release, ToolchainsChosenCtx, .json = &json);
        return;
    }
    EMBED_CALL(cli_json_tree_object_name.json_sort, CliJsonTreeObjectNameCfg, .json = &json, .object = chosen);
    for (unsigned int item = json.node[chosen].first; (item != CLI_JSON_NONE) && (count < 256u);
         item = json.node[item].next)
    {
        const char *const folder = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                              .json_read = &json, .node = item, .length_pointer = NULL);
        if (folder != NULL)
        {
            (void)cellul.copy(&(CatenaFinitaCfg){
                .dst = s_ids[count], .src = json.text + json.node[item].key, .cap = CLI_NAME_MAX_LEN});
            memor.cpy(
                &(MemoriaCfg){.dst = s_folders[count],
                              .src = folder,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
            count += 1u;
        }
    }
    EMBED_CALL(cli_chosen_release, ToolchainsChosenCtx, .json = &json);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(args->each_callback, CliEachIdFolderCfg, .context = args->context, .id = s_ids[at],
                   .folder = s_folders[at]);
    }
}

int cli_toolchains_choose(const CliToolchainsIdCfg *args)
{
    CliJson manifest_json;
    char dir[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_toolchains_text_internal.trim, CliToolchainsTextCtx, .text = args->folder_read, .out = dir,
               .room = sizeof(dir));
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                             .json_out = &manifest_json, .nodes_more = 0u, .text_more = 0ull);
    const unsigned int tool =
        EMBED_CALL(cli_toolchains_json_manifest.tool, CliToolchainsJsonManifestCfg, .json_out = &manifest_json,
                   .manifest = manifest, .id = args->id, .err_msg = args->err_msg);
    if (tool == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &manifest_json);
        return 0;
    }
    if (!EMBED_CALL(cli_toolchains_json_dir.tool_program_in, CliToolchainsJsonDirCfg, .json = &manifest_json,
                    .tool = tool, .dir = dir, .path = args->program))
    {
        mmgr_line_put(args->err_msg, dir);
        mmgr_line_put(args->err_msg, " contains none of ");
        const unsigned int programs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = &manifest_json, .object = tool, .name = "programs");
        for (unsigned int item = manifest_json.node[programs].first; item != CLI_JSON_NONE;
             item = manifest_json.node[item].next)
        {
            mmgr_line_put(args->err_msg, (item != manifest_json.node[programs].first) ? ", " : "");
            const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                .json_read = &manifest_json, .node = item, .length_pointer = NULL);
            mmgr_line_put(args->err_msg, (name != NULL) ? name : "");
        }
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &manifest_json);
        return 0;
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &manifest_json);
    CliJson json;
    char path[CLI_PATH_MAX_LEN];
    const unsigned int chosen = EMBED_CALL(cli_chosen_read, ToolchainsChosenCtx, .json = &json, .path = path);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = chosen,
               .name = args->id,
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = &json, .text = dir,
                                   .length = cellul.len(&(CatenaFinitaCfg){.src = dir, .cap = CLI_CELLS_POOL})));
    const int saved = (path[0] != '\0') &&
                      EMBED_CALL(cli_toolchains_path_internal.file_save, CliToolchainsPathCtx, .json_read = &json,
                                 .node = chosen, .path_read = path, .sorted = 1, .err_msg = args->err_msg);
    EMBED_CALL(cli_chosen_release, ToolchainsChosenCtx, .json = &json);
    if ((path[0] == '\0') && !saved)
    {
        mmgr_line_put(args->err_msg, "orior_cli has no folder of its own to store toolchains in");
    }
    return saved;
}

int cli_toolchains_forget(const CliToolchainsIdErrMsgCfg *args)
{
    CliJson json;
    char path[CLI_PATH_MAX_LEN];
    const unsigned int chosen = EMBED_CALL(cli_chosen_read, ToolchainsChosenCtx, .json = &json, .path = path);
    int saved = 1;
    if (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = chosen,
                   .name = args->id) != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = &json, .object = chosen,
                   .name = args->id);
        saved = EMBED_CALL(cli_toolchains_path_internal.file_save, CliToolchainsPathCtx, .json_read = &json,
                           .node = chosen, .path_read = path, .sorted = 1, .err_msg = args->err_msg);
    }
    EMBED_CALL(cli_chosen_release, ToolchainsChosenCtx, .json = &json);
    return saved;
}

int cli_toolchains_chosen_program(const CliToolchainsIdPathCfg *args)
{
    CliJson manifest_json;
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_toolchains_id_folder.choose, CliToolchainsIdFolderCfg, .id = args->id, .folder = folder))
    {
        return 0;
    }
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ull};
    const unsigned int tool =
        EMBED_CALL(cli_toolchains_json_manifest.tool, CliToolchainsJsonManifestCfg, .json_out = &manifest_json,
                   .manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                          .json_out = &manifest_json, .nodes_more = 0u, .text_more = 0ull),
                   .id = args->id, .err_msg = &err_msg);
    const int found =
        (tool != CLI_JSON_NONE) && EMBED_CALL(cli_toolchains_json_dir.tool_program_in, CliToolchainsJsonDirCfg,
                                              .json = &manifest_json, .tool = tool, .dir = folder, .path = args->path);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &manifest_json);
    return found;
}

// the hash `hash` carried on over `size` bytes at `bytes`, FNV-1a's
EMBED_INLINE unsigned long long found_hash(const ToolchainsChosenCtx *args)
{
    unsigned long long hash = args->hash;
    const unsigned char *const at = args->bytes;
    for (unsigned long long one = 0ull; one < args->size; one += 1ull)
    {
        hash = (hash ^ at[one]) * 1099511628211ull;
    }
    return hash;
}

// what the search reads, as one hash: the manifest as built, the PATH's `count` folders, and the size and the time of
// the last change of the reader's own toolchains and of the folders the reader gave
EMBED_INLINE unsigned long long found_against(const ToolchainsChosenCtx *args)
{
    const char (*path)[CLI_PATH_MAX_LEN] = args->path_callback;
    static unsigned long long s_manifest;
    // the manifest is built into the program, and is hashed once
    s_manifest = (s_manifest != 0ull) ? s_manifest
                                      : EMBED_CALL(found_hash, ToolchainsChosenCtx, .hash = 14695981039346656037ull,
                                                   .bytes = g_cli_toolchains_json, .size = g_cli_toolchains_json_size);
    unsigned long long hash = s_manifest;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        hash = EMBED_CALL(found_hash, ToolchainsChosenCtx, .hash = hash, .bytes = path[at],
                          .size = cellul.len(&(CatenaFinitaCfg){.src = path[at], .cap = CLI_CELLS_POOL}) + 1u);
    }
    static const char *const s_files[2] = {"user_toolchains.json", "toolchains.json"};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        char file[CLI_PATH_MAX_LEN];
        unsigned long long stamp[2] = {0ull, 0ull};
        if (EMBED_CALL(cli_toolchains_internal.file, CliToolchainsCtx, .name = s_files[at], .path = file))
        {
            (void)EMBED_CALL(cli_path_size_internal.file_stamp, CliPathSizeCtx, .path = file, .size_pointer = &stamp[0],
                             .changed = &stamp[1]);
        }
        hash = EMBED_CALL(found_hash, ToolchainsChosenCtx, .hash = hash, .bytes = stamp, .size = sizeof(stamp));
    }
    return hash;
}

// the place of the tool cached of `id`, or the cache's count where none is. Under the cache's lock
EMBED_INLINE unsigned int found_place(const ToolchainsChosenCtx *args)
{
    unsigned int place = 0u;
    while ((place < s_found.count) &&
           !cellul.eq(&(CatenaFinitaCfg){.src = s_found.tool[place].id, .other = args->id, .cap = CLI_NAME_MAX_LEN}))
    {
        place += 1u;
    }
    return place;
}

// tool `id` cached as found `state`, with `program`, in its place or after the others, the cache taken at its most as
// the first is put: 1, or 0 where the id is too long or the cellblock cannot meet the cache. Under the cache's lock
EMBED_INLINE int found_put(const ToolchainsChosenCtx *args)
{
    const char *state = args->state;
    const unsigned long long id_length = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_NAME_MAX_LEN});
    const unsigned long long program_length =
        cellul.len(&(CatenaFinitaCfg){.src = args->program, .cap = CLI_PATH_MAX_LEN});
    const unsigned int place = found_place(args);
    if ((id_length >= CLI_NAME_MAX_LEN) || (program_length >= CLI_PATH_MAX_LEN))
    {
        return 0;
    }
    if (s_found.most == 0u)
    {
        s_found.tool = g_cli_cells->persistent_buf_alloc((unsigned long long)CLI_FOUND_MOST * sizeof(*s_found.tool),
                                                         &g_cli_toolchains_found_cells);
        s_found.most = (s_found.tool != NULL) ? CLI_FOUND_MOST : 0u;
    }
    if (place >= s_found.most)
    {
        return 0;
    }
    CliToolFound *const tool = &s_found.tool[place];
    memor.cpy(&(MemoriaCfg){.dst = tool->id, .src = args->id, .bytes = (size_t)id_length + 1u});
    memor.cpy(&(MemoriaCfg){.dst = tool->program, .src = args->program, .bytes = (size_t)program_length + 1u});
    tool->state = state;
    s_found.count += (place == s_found.count) ? 1u : 0u;
    s_found.changed = 1;
    return 1;
}

// the state a search is cached with that reads as `text`, or NULL where none does
EMBED_INLINE const char *found_state(const ToolchainsChosenCtx *args)
{
    for (unsigned int at = 0u; (args->text != NULL) && (at < 4u); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->text, .other = s_found_states[at], .cap = CLI_NAME_MAX_LEN}))
        {
            return s_found_states[at];
        }
    }
    return NULL;
}

// the cache read from toolchains_found.json once, and emptied where what the search reads is not what it was as the
// cache was made. Under the cache's lock
static void found_current(const ToolchainsChosenCtx *args)
{
    unsigned long long against = args->against;
    if (!s_found.read)
    {
        s_found.read = 1;
        CliJson json;
        char path[CLI_PATH_MAX_LEN];
        const int named =
            EMBED_CALL(cli_toolchains_internal.file, CliToolchainsCtx, .name = "toolchains_found.json", .path = path);
        const unsigned int root_node =
            EMBED_CALL(cli_toolchains_json_internal.file_read, CliToolchainsJsonCtx, .json = &json,
                       .path_read = named ? path : "", .nodes_more = 0u, .text_more = 0ull);
        const char *const hex =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = root_node, .name = "against"),
                       .length_pointer = NULL);
        for (const char *at = hex; (at != NULL) && (*at != '\0'); at += 1)
        {
            // a hex digit read as its value, which an unsigned long long represents
            const unsigned long long digit =
                (*at >= 'a') ? (unsigned long long)(*at - 'a' + 10) : (unsigned long long)(*at - '0');
            s_found.against = (s_found.against << 4u) | (digit & 0xFull);
        }
        const unsigned int tools = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = root_node, .name = "tools");
        for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                            .node = tools, .kind = CFG_JSON_OBJECT)
                                     ? json.node[tools].first
                                     : CLI_JSON_NONE;
             item != CLI_JSON_NONE; item = json.node[item].next)
        {
            const char *const state = EMBED_CALL(
                found_state, ToolchainsChosenCtx,
                .text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                      .json_read = &json, .object = item, .name = "state"),
                                   .length_pointer = NULL));
            const char *const program =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = item, .name = "program"),
                           .length_pointer = NULL);
            if (state != NULL)
            {
                (void)EMBED_CALL(found_put, ToolchainsChosenCtx, .id = json.text + json.node[item].key, .state = state,
                                 .program = (program != NULL) ? program : "");
            }
        }
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        s_found.changed = 0;
    }
    if (s_found.against != against)
    {
        s_found.against = against;
        s_found.count = 0u;
        s_found.changed = 1;
    }
}

// the cache written to toolchains_found.json where it changed since it was, its folder made. Under the cache's lock
static void found_write(void)
{
    char path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    if (!s_found.changed ||
        !EMBED_CALL(cli_toolchains_internal.file, CliToolchainsCtx, .name = "toolchains_found.json", .path = path))
    {
        return;
    }
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    }
    char hex[17];
    for (unsigned int at = 0u; at < 16u; at += 1u)
    {
        hex[at] = "0123456789abcdef"[(s_found.against >> ((15u - at) * 4u)) & 0xFull];
    }
    hex[16] = '\0';
    CliFileWriting file;
    char piece[4096];
    if (!EMBED_CALL(cli_path_file_internal.file_begin, CliPathFileCtx, .file = &file, .path = path))
    {
        return;
    }
    VerbaLinea out = {piece, sizeof(piece), 0ull, cli_file_put, &file, 0ull};
    mmgr_line_put(&out, "{\n  \"against\": \"");
    mmgr_line_put(&out, hex);
    mmgr_line_put(&out, "\",\n  \"tools\": {");
    for (unsigned int at = 0u; at < s_found.count; at += 1u)
    {
        const CliToolFound *const tool = &s_found.tool[at];
        mmgr_line_put(&out, (at != 0u) ? ",\n    " : "\n    ");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
                   .text_bytes = (const unsigned char *)tool->id,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = tool->id, .cap = CLI_NAME_MAX_LEN}));
        mmgr_line_put(&out, ": {\"state\": \"");
        mmgr_line_put(&out, tool->state);
        mmgr_line_put(&out, "\", \"program\": ");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
                   .text_bytes = (const unsigned char *)tool->program,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = tool->program, .cap = CLI_PATH_MAX_LEN}));
        mmgr_line_ch(&out, '}');
    }
    mmgr_line_put(&out, "\n  }\n}\n");
    const int written = mmgr_line_flush(&out);
    s_found.changed = !EMBED_CALL(cli_file_internal.file_end, CliFileCtx, .file = &file, .whole = written);
}

int cli_toolchains_found_cache(const CliToolchainsIdCtx *args)
{
    const char *id = args->id;
    const char (*path)[CLI_PATH_MAX_LEN] = args->path_callback;
    unsigned int count = args->count;
    const unsigned long long against =
        EMBED_CALL(found_against, ToolchainsChosenCtx, .path_callback = path, .count = count);
    EMBED_CALL(found_current, ToolchainsChosenCtx, .against = against);
    const unsigned int place = EMBED_CALL(found_place, ToolchainsChosenCtx, .id = id);
    const CliToolFound *const tool = (place < s_found.count) ? &s_found.tool[place] : NULL;
    // a program cached that is gone from its folder is searched for again
    const int cached =
        (tool != NULL) && ((tool->program[0] == '\0') ||
                           EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = tool->program));
    if (cached)
    {
        args->found->state = tool->state;
        args->found->has_program = tool->program[0] != '\0';
        memor.cpy(&(MemoriaCfg){.dst = args->found->program, .src = tool->program, .bytes = CLI_PATH_MAX_LEN});
        memor.cpy(&(MemoriaCfg){.dst = args->found->folder, .src = tool->program, .bytes = CLI_PATH_MAX_LEN});
        char *cut = args->found->folder;
        const unsigned long long cut_past = EMBED_CALL(
            cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->found->folder,
            .at = cellul.len(&(CatenaFinitaCfg){.src = args->found->folder, .cap = CLI_CELLS_POOL}), .set = "/\\");
        cut = (cut_past != 0ull) ? (args->found->folder + (cut_past - 1ull)) : cut;
        *cut = '\0';
    }
    return cached;
}

void cli_toolchains_found_put(const CliToolchainsIdPathCtx *args)
{
    const char *id = args->id;
    const char (*path)[CLI_PATH_MAX_LEN] = args->path_callback;
    unsigned int count = args->count;
    const char *const state = EMBED_CALL(found_state, ToolchainsChosenCtx, .text = args->found_read->state);
    if (state == NULL)
    {
        return;
    }
    const unsigned long long against =
        EMBED_CALL(found_against, ToolchainsChosenCtx, .path_callback = path, .count = count);
    EMBED_CALL(found_current, ToolchainsChosenCtx, .against = against);
    (void)EMBED_CALL(found_put, ToolchainsChosenCtx, .id = id, .state = state,
                     .program = args->found_read->has_program ? args->found_read->program : "");
    if (s_found.refs == 0u)
    {
        found_write();
    }
}

void cli_toolchains_found_ref(void)
{
    s_found.refs += 1u;
}

void cli_toolchains_found_let(void)
{
    s_found.refs -= 1u;
    if (s_found.refs == 0u)
    {
        found_write();
    }
}
