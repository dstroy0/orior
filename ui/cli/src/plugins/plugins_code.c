// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_code.c: a script plugin's code: the file its plugin.json names as `main`, or each of the files it names there
// in order, joined by a line's end, plugin.js where it names none, every one in the plugin's own folder. The code of a
// script plugin that comes with orior is read the same way as the program is built, and built into it
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "plugins.h"

// the most files a plugin's `main` names
#define CLI_PLUGINS_MAINS 32u
// The files a plugin's `main` names
typedef struct
{
    char main[CLI_PLUGINS_MAINS][CLI_PATH_MAX_LEN];
    unsigned int mains;
} CliCodeMains;

typedef struct
{
    CliCodeMains *mains;
    const char *name;
    const char *text;
    unsigned long long size;
    const char *folder;
    VerbaLinea *out;
    VerbaLinea *err_msg;
} PluginsCodeCtx;
// `name` recorded as one of the files the plugin's code is read from, where capacity is left
EMBED_INLINE void code_main_append(const PluginsCodeCtx *args)
{
    CliCodeMains *mains = args->mains;
    if (mains->mains < CLI_PLUGINS_MAINS)
    {
        VerbaLinea main_line = {mains->main[mains->mains], CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&main_line, args->name);
        mains->mains += (mmgr_line_finish(&main_line) != 0ULL) ? 1u : 0u;
    }
}

// the files the plugin.json `text`, `size` bytes, names as `main` into `mains`: 1, or 0 where it does not read
EMBED_INLINE int code_mains_read(const PluginsCodeCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    CliCodeMains *mains = args->mains;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = &json, .text = text, .size = size, .nodes_more = 0u, .text_more = 0ULL,
                                         .err_msg = &err_msg, .owner = &g_cli_plugins_cells);
    mains->mains = 0u;
    const unsigned int main = (root != CLI_JSON_NONE)
                                  ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = &json, .object = root, .name = "main")
                                  : CLI_JSON_NONE;
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                        .node = main, .kind = CFG_JSON_ARRAY)
                                 ? json.node[main].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json.node[item].next)
    {
        const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = &json, .node = item, .length_pointer = NULL);
        // a name that is no string is recorded empty and refused as no file of the folder's
        EMBED_CALL(code_main_append, PluginsCodeCtx, .mains = mains, .name = (name != NULL) ? name : "");
    }
    const char *const one = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                       .json_read = &json, .node = main, .length_pointer = NULL);
    if (mains->mains == 0u)
    {
        EMBED_CALL(code_main_append, PluginsCodeCtx, .mains = mains, .name = (one != NULL) ? one : "plugin.js");
    }
    g_cli_cells->persistent_buf_release(json.node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return root != CLI_JSON_NONE;
}

// whether `name` is a file of the plugin's own folder: each part of it none empty, `.` or `..`, and the name not whole
EMBED_INLINE int code_own(const PluginsCodeCtx *args)
{
    int own = (args->name[0] != '/') && (args->name[0] != '\\') &&
              (cellul.chr(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL, .byte = ':'}) == NULL);
    const char *part = args->name;
    while (own && (part[0] != '\0'))
    {
        const char *end = part;
        end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ULL,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}), .set = "/\\");
        const unsigned long long length = (unsigned long long)(end - part);
        own = (length != 0ULL) && !((length == 1u) && (part[0] == '.')) &&
              !((length == 2u) && (part[0] == '.') && (part[1] == '.'));
        part = (end[0] != '\0') ? (end + 1) : end;
    }
    return own && (args->name[0] != '\0');
}

// the file `name` of the plugin's folder `folder` read onto the end of `out`: 1, or 0 with why given
EMBED_INLINE int code_file_read(const PluginsCodeCtx *args)
{
    char file[CLI_PATH_MAX_LEN];
    VerbaLinea line = {file, sizeof(file), 0ULL};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    unsigned long long file_size = 0ULL;
    const unsigned long long capacity = mmgr_line_ok(args->out) ? (args->out->capacity - args->out->at) : 0ULL;
    const int found =
        (mmgr_line_finish(&line) != 0ULL) &&
        EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = file, .size_pointer = &file_size) &&
        (capacity > file_size);
    const unsigned long long size =
        found ? qry_member_file_read(file, (unsigned char *)(args->out->out + args->out->at), capacity)
              : QRY_MEMBER_UNREAD;
    if (size == QRY_MEMBER_UNREAD)
    {
        mmgr_line_put(args->err_msg, file);
        mmgr_line_put(args->err_msg, ": it could not be read");
        return 0;
    }
    args->out->at += size;
    return 1;
}

int cli_plugins_code_in(const CliPluginsIdCfg *args)
{
    static CliCodeMains s_mains;
    int done = EMBED_CALL(code_mains_read, PluginsCodeCtx, .text = args->text, .size = args->size, .mains = &s_mains);
    if (!done)
    {
        mmgr_line_put(args->err_msg, args->id);
        mmgr_line_put(args->err_msg, ": its plugin.json does not read");
    }
    for (unsigned int at = 0u; done && (at < s_mains.mains); at += 1u)
    {
        done = EMBED_CALL(code_own, PluginsCodeCtx, .name = s_mains.main[at]);
        if (!done)
        {
            mmgr_line_put(args->err_msg, args->id);
            mmgr_line_put(args->err_msg, " names ");
            mmgr_line_put(args->err_msg, s_mains.main[at]);
            mmgr_line_put(args->err_msg, ", which is not a file of its own folder");
        }
    }
    for (unsigned int at = 0u; done && (at < s_mains.mains); at += 1u)
    {
        mmgr_line_put(args->out, (at > 0u) ? "\n" : "");
        done = EMBED_CALL(code_file_read, PluginsCodeCtx, .folder = args->folder_read, .name = s_mains.main[at],
                          .out = args->out, .err_msg = args->err_msg);
    }
    return done;
}
