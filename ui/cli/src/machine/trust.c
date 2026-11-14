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
    VerbaLinea *out;
    int add;
} TrustCtx;
// the file of answers into `path`, sized for CLI_PATH_MAX_LEN: 1, or 0 where impero has no home folder
EMBED_INLINE int cli_trust_file(const TrustCtx *args)
{
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        return 0;
    }
    VerbaLinea line = {args->path, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, home);
    mmgr_line_put(&line, "/trust.json");
    return mmgr_line_finish(&line) != 0ULL;
}

// A folder as the answers are stored by into `key`, sized for CLI_PATH_MAX_LEN: its path with `/` between its parts,
// and in lower case where the system does not distinguish the cases
EMBED_INLINE void cli_key(const TrustCtx *args)
{
    VerbaLinea line = {args->key, CLI_PATH_MAX_LEN, 0ULL};
#if defined(_WIN32)
    static unsigned int s_letters[CLI_PATH_MAX_LEN];
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->folder,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_POOL_BYTES}),
                   .letters = s_letters, .capacity_u64 = CLI_PATH_MAX_LEN);
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line,
                   .letter = (s_letters[at] == '\\')
                                 ? '/'
                                 : EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = s_letters[at]));
    }
#else
    (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = &line, .text = args->folder, .from = '\\',
                     .to = '/');
#endif
    mmgr_line_finish(&line);
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES});
    length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->key, .at = length,
                        .set = "/");
    args->key[length] = '\0';
}

int cli_trust_under(const CliTrustFolderInnerCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_POOL_BYTES});
    return EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->inner, .other_pointer = args->folder,
                      .size = length) &&
           ((args->inner[length] == '\0') || (args->inner[length] == '/'));
}

// the file of answers at `path` read into a tree in a cell of its own, released through persistent_buf_release, the
// lock acquired: its `trusted` and `refused` lists, each none where the file has no such list
EMBED_INLINE void cli_state_read(const TrustCtx *args)
{
    CliJson *json = args->json;
    const char *path = args->path_read;
    unsigned int *trusted = args->trusted;
    unsigned int *refused = args->refused;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_err_msg.json_file_alloc, CliJsonTreeErrMsgCfg, .json = json, .path = path,
                   .nodes_more = 0u, .text_more = 0ULL, .err_msg = &err_msg, .owner = g_cli_machine_owner);
    const int object = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                  .node = root, .type = CFG_JSON_OBJECT);
    *trusted = object ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                   .object = root, .name = "trusted")
                      : CLI_JSON_NONE;
    *refused = object ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                   .object = root, .name = "refused")
                      : CLI_JSON_NONE;
    *trusted = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = *trusted,
                          .type = CFG_JSON_ARRAY)
                   ? *trusted
                   : CLI_JSON_NONE;
    *refused = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = *refused,
                          .type = CFG_JSON_ARRAY)
                   ? *refused
                   : CLI_JSON_NONE;
}

// the length of the longest folder of a list that is the tree or contains it, plus one, or 0 where none is
EMBED_INLINE unsigned long long cli_nearest(const TrustCtx *args)
{
    const CliJson *json = args->json_read;
    unsigned long long nearest = 0ULL;
    for (unsigned int item = (args->list != CLI_JSON_NONE) ? json->node[args->list].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const folder = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                              .json_read = json, .node = item, .length_pointer = NULL);
        if ((folder != NULL) &&
            EMBED_CALL(cli_trust_folder_inner.under, CliTrustFolderInnerCfg, .inner = args->tree, .folder = folder) &&
            ((cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES}) + 1ULL) > nearest))
        {
            nearest = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES}) + 1ULL;
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
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    *trusted = by_trusted > by_refused;
    return (by_trusted != 0ULL) || (by_refused != 0ULL);
}

int cli_trust_answer(const CliTrustTreeCfg *args)
{
    int *trusted = args->trusted;
    char path[CLI_PATH_MAX_LEN];
    *trusted = 0;
    return EMBED_CALL(cli_trust_file, TrustCtx, .path = path) &&
           EMBED_CALL(cli_trust_tree.answer_in, CliTrustTreeCfg, .path = path, .tree = args->tree, .trusted = trusted);
}

// the folders of `list` written into `out` as JSON strings with commas between, the folder `tree` left out, and `tree`
// itself written last where `add` is 1
EMBED_INLINE void cli_list_write(const TrustCtx *args)
{
    const CliJson *json = args->json_read;
    int first = 1;
    for (unsigned int item = (args->list != CLI_JSON_NONE) ? json->node[args->list].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        unsigned long long length = 0ULL;
        const char *const folder = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                              .json_read = json, .node = item, .length_pointer = &length);
        if ((folder == NULL) ||
            cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = args->tree, .cap = CLI_PATH_MAX_LEN}))
        {
            continue;
        }
        mmgr_line_put(args->out, first ? "" : ", ");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = folder,
                   .length = length);
        first = 0;
    }
    if (args->add)
    {
        mmgr_line_put(args->out, first ? "" : ", ");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = args->tree,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->tree, .cap = CLI_PATH_MAX_LEN}));
    }
}

int cli_trust_answer_set(const CliTrustFolderCfg *args)
{
    char path[CLI_PATH_MAX_LEN];
    if (args->path != NULL)
    {
        VerbaLinea line = {path, sizeof(path), 0ULL};
        mmgr_line_put(&line, args->path);
        (void)mmgr_line_finish(&line);
    }
    else if (!EMBED_CALL(cli_trust_file, TrustCtx, .path = path))
    {
        mmgr_line_put(args->err_msg, "impero finds no folder of its own");
        return 0;
    }
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_key, TrustCtx, .folder = args->folder, .key = key);
    CliJson json = {0};
    unsigned int yes = CLI_JSON_NONE;
    unsigned int no = CLI_JSON_NONE;
    EMBED_CALL(cli_state_read, TrustCtx, .json = &json, .path_read = path, .trusted = &yes, .refused = &no);
    // the answers written anew: each list's folders as they stand, less this folder, and this folder in the list its
    // answer names; each folder's bytes escape to six at the most
    const unsigned long long capacity = (json.used * 6ULL) + (CLI_PATH_MAX_LEN * 6ULL) + 64ULL;
    char *const cell = g_cli_guard->temporary_buf_alloc(capacity, g_cli_machine_owner);
    if (cell == NULL)
    {
        g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
        mmgr_line_put(args->err_msg, "the cellblock could not meet the answers");
        return 0;
    }
    VerbaLinea out = mmgr_line_over(cell, capacity);
    mmgr_line_put(&out, "{\n  \"trusted\": [");
    EMBED_CALL(cli_list_write, TrustCtx, .json_read = &json, .list = yes, .tree = key, .out = &out,
               .add = args->trusted_value != 0);
    mmgr_line_put(&out, "],\n  \"refused\": [");
    EMBED_CALL(cli_list_write, TrustCtx, .json_read = &json, .list = no, .tree = key, .out = &out,
               .add = args->trusted_value == 0);
    mmgr_line_put(&out, "]\n}\n");
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    const int written =
        mmgr_line_ok(&out) && EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path,
                                         .bytes = out.out, .size = out.at);
    if (!written)
    {
        mmgr_line_put(args->err_msg, "the answers could not be written to ");
        mmgr_line_put(args->err_msg, path);
    }
    g_cli_guard->persistent_buf_release(cell, g_cli_machine_owner);
    return written;
}
