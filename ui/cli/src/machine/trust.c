// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// trust.c: the reader's answers stored by folder, in trust.json in orior's home
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "trust_internal.h"

// the nodes and the bytes of their texts the answers stored take past those read, the list, its two lists, the folder
// answered and the names of the lists among them; and the bytes the answers are written in
#define CLI_TRUST_NODES_MORE 4u
#define CLI_TRUST_TEXT_MORE 64u

// the file of answers read and written under its lock

typedef struct
{
    char *path;
    const char *folder;
    char *key;
    CliJson *json;
    const char *path_read;
    unsigned int *trusted;
    unsigned int *refused;
    const CliJson *json_read;
    unsigned int list;
    const char *tree;
    const CliJson *from;
    unsigned int into;
} TrustCtx;
// the file of answers into `path`, sized for CLI_PATH_MAX_LEN: 1, or 0 where orior_cli has no home folder
EMBED_INLINE int cli_trust_file(const TrustCtx *args)
{
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        return 0;
    }
    VerbaLinea line = {args->path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, home);
    mmgr_line_put(&line, "/trust.json");
    return mmgr_line_finish(&line) != 0ull;
}

// A folder as the answers are stored by into `key`, sized for CLI_PATH_MAX_LEN: its path with `/` between its parts,
// and in lower case where the system does not distinguish the cases
EMBED_INLINE void cli_key(const TrustCtx *args)
{
    VerbaLinea line = {args->key, CLI_PATH_MAX_LEN, 0ull};
#if defined(_WIN32)
    static unsigned int s_letters[CLI_PATH_MAX_LEN];
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->folder,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_CELLS_POOL}),
                   .letters = s_letters, .room_u64 = CLI_PATH_MAX_LEN);
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        EMBED_CALL(cli_line_letter_internal.utf8_put, CliLineLetterCtx, .line = &line,
                   .letter = (s_letters[at] == '\\')
                                 ? '/'
                                 : EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = s_letters[at]));
    }
#else
    (void)EMBED_CALL(cli_text_from.put_swap, CliTextFromCfg, .out = &line, .text = args->folder, .from = '\\',
                     .to = '/');
#endif
    mmgr_line_finish(&line);
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL});
    length = EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = args->key, .at = length,
                        .set = "/");
    args->key[length] = '\0';
}

int cli_trust_under(const CliTrustFolderInnerCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_CELLS_POOL});
    return EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->inner, .other_pointer = args->folder,
                      .size = length) &&
           ((args->inner[length] == '\0') || (args->inner[length] == '/'));
}

// the file of answers at `path` read into a tree in a cell of its own, given back with cli_json_free, the lock
// acquired: its `trusted` and `refused` lists, each none where the file has no such list
EMBED_INLINE void cli_state_read(const TrustCtx *args)
{
    CliJson *json = args->json;
    const char *path = args->path_read;
    unsigned int *trusted = args->trusted;
    unsigned int *refused = args->refused;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_err_msg.json_file_take, CliJsonTreeErrMsgCfg, .json = json, .path = path,
                   .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_cli_machine_cells);
    const int object = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                  .node = root, .kind = CFG_JSON_OBJECT);
    *trusted = object ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                   .object = root, .name = "trusted")
                      : CLI_JSON_NONE;
    *refused = object ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                   .object = root, .name = "refused")
                      : CLI_JSON_NONE;
    *trusted = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = *trusted,
                          .kind = CFG_JSON_ARRAY)
                   ? *trusted
                   : CLI_JSON_NONE;
    *refused = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = *refused,
                          .kind = CFG_JSON_ARRAY)
                   ? *refused
                   : CLI_JSON_NONE;
}

// the length of the longest folder of a list that is the tree or contains it, plus one, or 0 where none is
EMBED_INLINE unsigned long long cli_nearest(const TrustCtx *args)
{
    const CliJson *json = args->json_read;
    unsigned long long nearest = 0ull;
    for (unsigned int item = (args->list != CLI_JSON_NONE) ? json->node[args->list].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const folder = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                              .json_read = json, .node = item, .length_pointer = NULL);
        if ((folder != NULL) &&
            EMBED_CALL(cli_trust_folder_inner.under, CliTrustFolderInnerCfg, .inner = args->tree, .folder = folder) &&
            ((cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1ull) > nearest))
        {
            nearest = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1ull;
        }
    }
    return nearest;
}

int cli_trust_answer_in(const CliTrustTreeCfg *args)
{
    int *trusted = args->trusted;
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_key, TrustCtx, .folder = args->tree, .key = key);
    CliJson json = {0};
    unsigned int yes = CLI_JSON_NONE;
    unsigned int no = CLI_JSON_NONE;
    EMBED_CALL(cli_state_read, TrustCtx, .json = &json, .path_read = args->path, .trusted = &yes, .refused = &no);
    const unsigned long long by_trusted =
        EMBED_CALL(cli_nearest, TrustCtx, .json_read = &json, .list = yes, .tree = key);
    const unsigned long long by_refused =
        EMBED_CALL(cli_nearest, TrustCtx, .json_read = &json, .list = no, .tree = key);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    *trusted = by_trusted > by_refused;
    return (by_trusted != 0ull) || (by_refused != 0ull);
}

int cli_trust_answer(const CliTrustTreeCfg *args)
{
    int *trusted = args->trusted;
    char path[CLI_PATH_MAX_LEN];
    *trusted = 0;
    return EMBED_CALL(cli_trust_file, TrustCtx, .path = path) &&
           EMBED_CALL(cli_trust_tree.answer_in, CliTrustTreeCfg, .path = path, .tree = args->tree, .trusted = trusted);
}

// the folders of a list of `from` not under `folder` put after the items of `into` in `json`
EMBED_INLINE void cli_folders_without(const TrustCtx *args)
{
    const char *folder = args->folder;
    CliJson *json = args->json;
    for (unsigned int item = (args->list != CLI_JSON_NONE) ? args->from->node[args->list].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = args->from->node[item].next)
    {
        unsigned long long length = 0ull;
        const char *const one = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                           .json_read = args->from, .node = item, .length_pointer = &length);
        if ((one != NULL) &&
            !EMBED_CALL(cli_trust_folder_inner.under, CliTrustFolderInnerCfg, .inner = one, .folder = folder))
        {
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = args->into,
                       .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init,
                                           CliJsonTreeTextLengthKindCfg, .json = json, .text = one, .length = length));
        }
    }
}

int cli_trust_set_in(const CliTrustFolderCfg *args)
{
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_key, TrustCtx, .folder = args->folder, .key = key);
    const unsigned long long key_length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
    CliJson read = {0};
    unsigned int yes = CLI_JSON_NONE;
    unsigned int no = CLI_JSON_NONE;
    EMBED_CALL(cli_state_read, TrustCtx, .json = &read, .path_read = args->path, .trusted = &yes, .refused = &no);
    // the answers stored, made in a tree of the room of those read and of the folder answered
    CliJson json = {0};
    int done = EMBED_CALL(cli_json_tree_nodes_most.json_reserve, CliJsonTreeNodesMostCfg, .json = &json,
                          .nodes_most = read.nodes + CLI_TRUST_NODES_MORE,
                          .text_most = read.used + key_length + CLI_TRUST_TEXT_MORE, .what = "the answers stored",
                          .owner = &g_cli_machine_cells);
    const unsigned int trust_obj =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &json, .kind = CFG_JSON_OBJECT);
    const unsigned int yes_list =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &json, .kind = CFG_JSON_ARRAY);
    const unsigned int no_list =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &json, .kind = CFG_JSON_ARRAY);
    EMBED_CALL(cli_folders_without, TrustCtx, .from = &read, .list = yes, .folder = key, .json = &json,
               .into = yes_list);
    EMBED_CALL(cli_folders_without, TrustCtx, .from = &read, .list = no, .folder = key, .json = &json, .into = no_list);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &read);
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = &json,
               .array = args->trusted_value ? yes_list : no_list,
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = &json, .text = key, .length = key_length));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = trust_obj,
               .name = "trusted", .value = yes_list);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = trust_obj,
               .name = "refused", .value = no_list);
    char parent[CLI_PATH_MAX_LEN];
    done = done && !json.full;
    if (!done)
    {
        mmgr_line_put(args->err_msg, "the answers stored do not fit");
    }
    if (done &&
        EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->path, .out = parent) &&
        !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = parent))
    {
        mmgr_line_put(args->err_msg, parent);
        mmgr_line_put(args->err_msg, ": it could not be made");
        done = 0;
    }
    const unsigned long long out_room =
        EMBED_CALL(cli_json_tree_node_out.json_pretty_room, CliJsonTreeNodeOutCfg, .json_read = &json, .indent = 2u);
    char *const out_cell = g_cli_cells->temporary_buf_alloc(out_room, &g_cli_machine_cells);
    VerbaLinea out = mmgr_line_over(out_cell, out_room);
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = trust_obj,
               .out = &out, .indent = 2u, .sorted = 0);
    if (done && (!mmgr_line_ok(&out) || !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                                    .path = args->path, .bytes = out.out, .size = out.at)))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": it could not be written");
        done = 0;
    }
    g_cli_cells->persistent_buf_release(out.out, &g_cli_machine_cells);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    if (done)
    {
        cli_trust_forget();
    }
    return done;
}

int cli_trust_set(const CliTrustFolderCfg *args)
{
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_trust_file, TrustCtx, .path = path))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no home folder");
        return 0;
    }
    return EMBED_CALL(cli_trust_folder.set_in, CliTrustFolderCfg, .path = path, .folder = args->folder,
                      .trusted_value = args->trusted_value, .err_msg = args->err_msg);
}
