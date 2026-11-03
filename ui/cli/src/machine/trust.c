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
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_CELLS_POOL}),
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
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL});
    length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->key, .at = length,
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
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_err_msg.json_file_alloc, CliJsonTreeErrMsgCfg, .json = json, .path = path,
                   .nodes_more = 0u, .text_more = 0ULL, .err_msg = &err_msg, .owner = &g_cli_machine_cells);
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
    unsigned long long nearest = 0ULL;
    for (unsigned int item = (args->list != CLI_JSON_NONE) ? json->node[args->list].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const folder = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                              .json_read = json, .node = item, .length_pointer = NULL);
        if ((folder != NULL) &&
            EMBED_CALL(cli_trust_folder_inner.under, CliTrustFolderInnerCfg, .inner = args->tree, .folder = folder) &&
            ((cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1ULL) > nearest))
        {
            nearest = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1ULL;
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
