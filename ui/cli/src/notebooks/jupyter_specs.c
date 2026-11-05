// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_specs.c: every kernel the machine has, each name once, in Jupyter's order: the specs in the folders Jupyter
// stores its data in, then a kernel for each Python orior_cli finds that has no spec of its own
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../toolchains/toolchains.h"
#include "jupyter_internal.h"

#include "../text/text.h"
#include <stdlib.h>

typedef struct
{
    void *context;
    const CliKernelSpec *spec;
} CliKernelEachCfg;
// what is done with each spec listed: 1 to go on, 0 to stop
typedef int (*CliKernelEach)(const CliKernelEachCfg *args);
// the most kernels listed and entries of a kernels folder, the longest name of an entry, and the most nodes and
// bytes of a spec read
#define CLI_KERNEL_LISTING 256u
#define CLI_KERNEL_ENTRIES 1024u
#define CLI_KERNEL_ENTRY 256u
#define CLI_KERNEL_SPEC_NODES 4096u
#define CLI_KERNEL_SPEC_TEXT (1u << 18u)

// What a listing of the specs reads into: the folders and Pythons, the names and first words of the specs listed, a
// kernels folder's entries, a spec read and the spec made
typedef struct
{
    char folders[CLI_KERNEL_FOLDERS][CLI_PATH_MAX_LEN];
    char pythons[CLI_KERNEL_PYTHONS][CLI_PATH_MAX_LEN];
    char (*names)[CLI_KERNEL_ENTRY];
    char (*programs)[CLI_PATH_MAX_LEN];
    unsigned int list_entries;
    char entries[CLI_KERNEL_ENTRIES][CLI_KERNEL_ENTRY];
    unsigned int entry_count;
    CliJsonNode nodes[CLI_KERNEL_SPEC_NODES];
    CfgJsonToken tokens[CLI_KERNEL_SPEC_NODES];
    char text[CLI_KERNEL_SPEC_TEXT];
    CliJson json;
    CliKernelSpec spec;
} CliKernelListing;

static CliKernelListing s_listing;

const char *cli_kernel_text(const CliJupyterSpecCtx *args)
{
    return (args->at == CLI_KERNEL_NONE) ? "" : args->spec->text + args->at;
}

typedef struct
{
    CliKernelSpec *spec;
    const char *text;
    unsigned long long length;
    unsigned int object;
    const char *name;
    const char *fallback;
    unsigned int *at;
    unsigned int root;
    const char *folder;
    CliKernelEach each;
    void *context;
    const char *data;
    unsigned int pythons;
    int has_environment;
} JupyterSpecsCtx;
// `length` bytes put in the spec's texts: their place, CLI_KERNEL_NONE where they do not fit
EMBED_INLINE unsigned int jupyter_append(const JupyterSpecsCtx *args)
{
    if ((args->spec->used + args->length + 1ULL > CLI_KERNEL_TEXT) ||
        (memor.chr(&(MemoriaCfg){.src = args->text, .val = '\0', .bytes = args->length}) != NULL))
    {
        args->spec->full = 1;
        return CLI_KERNEL_NONE;
    }
    const unsigned int at = args->spec->used;
    memor.cpy(&(MemoriaCfg){.dst = args->spec->text + at, .src = args->text, .bytes = args->length});
    args->spec->text[at + args->length] = '\0';
    args->spec->used += (unsigned int)args->length + 1u;
    return at;
}

// an entry of a kernels folder recorded
static void jupyter_entry(const CliEachNameIsFolderCfg *args)
{
    if ((s_listing.entry_count < CLI_KERNEL_ENTRIES) &&
        (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) < CLI_KERNEL_ENTRY))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = s_listing.entries[s_listing.entry_count],
                          .src = args->name,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1u});
        s_listing.entry_count += 1u;
    }
}

static int jupyter_entry_order(const void *left, const void *right)
{
    return EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = (const char *)left,
                      .other_pointer = (const char *)right);
}

// a string member of a spec read into the spec: 1, or 0 where it is no string, or absent with no `fallback`
EMBED_INLINE int jupyter_member(const JupyterSpecsCtx *args)
{
    unsigned int object = args->object;
    const char *name = args->name;
    unsigned int *at = args->at;
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = &s_listing.json, .object = object, .name = name);
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = &s_listing.json, .node = member, .length_pointer = &length);
    if ((member == CLI_JSON_NONE) && (args->fallback != NULL))
    {
        *at = EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = args->spec, .text = args->fallback,
                         .length = cellul.len(&(CatenaFinitaCfg){.src = args->fallback, .cap = CLI_CELLS_POOL}));
        return *at != CLI_KERNEL_NONE;
    }
    if (text == NULL)
    {
        return 0;
    }
    *at = EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = args->spec, .text = text, .length = length);
    return *at != CLI_KERNEL_NONE;
}

// the spec a kernel.json specifies, its name `name` and its folder `folder`, read into the listing's spec: 1, or 0
// where it is not one, as serde reads a Spec
EMBED_INLINE int jupyter_spec_read(const JupyterSpecsCtx *args)
{
    const char *name = args->name;
    const char *folder = args->folder;
    CliKernelSpec *const spec = &s_listing.spec;
    memor.set(&(MemoriaCfg){.dst = spec, .val = 0, .bytes = sizeof(*spec)});
    spec->name = EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = spec, .text = name,
                            .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_listing.json,
                    .node = args->root, .kind = CFG_JSON_OBJECT) ||
        !EMBED_CALL(jupyter_member, JupyterSpecsCtx, .object = args->root, .name = "display_name", .fallback = NULL,
                    .at = &spec->display_name, .spec = spec) ||
        !EMBED_CALL(jupyter_member, JupyterSpecsCtx, .object = args->root, .name = "language", .fallback = NULL,
                    .at = &spec->language, .spec = spec) ||
        !EMBED_CALL(jupyter_member, JupyterSpecsCtx, .object = args->root, .name = "interrupt_mode", .fallback = "",
                    .at = &spec->interrupt_mode, .spec = spec) ||
        !EMBED_CALL(jupyter_member, JupyterSpecsCtx, .object = args->root, .name = "folder", .fallback = "",
                    .at = &spec->folder, .spec = spec))
    {
        return 0;
    }
    const unsigned int argv = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = &s_listing.json, .object = args->root, .name = "argv");
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_listing.json, .node = argv,
                    .kind = CFG_JSON_ARRAY))
    {
        return 0;
    }
    for (unsigned int index = 0u;
         EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_listing.json,
                    .array = argv, .index = index) != CLI_JSON_NONE;
         index += 1u)
    {
        unsigned long long length = 0ULL;
        const char *const word_text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_listing.json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = &s_listing.json, .array = argv, .index = index),
                       .length_pointer = &length);
        if ((word_text == NULL) || (spec->argv_count == CLI_KERNEL_WORDS))
        {
            return 0;
        }
        spec->argv[spec->argv_count] =
            EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = spec, .text = word_text, .length = length);
        spec->argv_count += 1u;
    }
    const unsigned int env = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                        .json_read = &s_listing.json, .object = args->root, .name = "env");
    if ((env != CLI_JSON_NONE) && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                              .json_read = &s_listing.json, .node = env, .kind = CFG_JSON_OBJECT))
    {
        return 0;
    }
    for (unsigned int member = (env != CLI_JSON_NONE) ? s_listing.json.node[env].first : CLI_JSON_NONE;
         member != CLI_JSON_NONE; member = s_listing.json.node[member].next)
    {
        unsigned long long length = 0ULL;
        const char *const value = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                             .json_read = &s_listing.json, .node = member, .length_pointer = &length);
        if ((value == NULL) || (spec->env_count == CLI_KERNEL_SETTINGS))
        {
            return 0;
        }
        const char *const key = s_listing.json.text + s_listing.json.node[member].key;
        spec->env_name[spec->env_count] =
            EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = spec, .text = key,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
        spec->env_value[spec->env_count] =
            EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = spec, .text = value, .length = length);
        spec->env_count += 1u;
    }
    const unsigned int install = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                            .json_read = &s_listing.json, .object = args->root, .name = "install");
    spec->install = CLI_KERNEL_NONE;
    if ((install != CLI_JSON_NONE) &&
        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_listing.json,
                    .node = install, .kind = CFG_JSON_NULL) &&
        !EMBED_CALL(jupyter_member, JupyterSpecsCtx, .object = args->root, .name = "install", .fallback = NULL,
                    .at = &spec->install, .spec = spec))
    {
        return 0;
    }
    spec->folder = EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = spec, .text = folder,
                              .length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
    return !spec->full;
}

// the spec listed: its name and first word recorded, and handed on: 1 to go on, 0 to stop
EMBED_INLINE int jupyter_list(const JupyterSpecsCtx *args)
{
    if (s_listing.list_entries < CLI_KERNEL_LISTING)
    {
        VerbaLinea name = {s_listing.names[s_listing.list_entries], CLI_KERNEL_ENTRY, 0ULL};
        mmgr_line_put(&name, EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx,
                                        .spec = &s_listing.spec, .at = s_listing.spec.name));
        mmgr_line_finish(&name);
        VerbaLinea program = {s_listing.programs[s_listing.list_entries], CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&program, (s_listing.spec.argv_count > 0u)
                                    ? EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx,
                                                 .spec = &s_listing.spec, .at = s_listing.spec.argv[0])
                                    : "");
        mmgr_line_finish(&program);
        s_listing.list_entries += 1u;
    }
    return EMBED_CALL(args->each, CliKernelEachCfg, .context = args->context, .spec = &s_listing.spec);
}

// the specs of one folder of Jupyter's data, in the order of their folders' names, each handed on: 1 to go on, 0 to
// stop
EMBED_INLINE int jupyter_folder_specs(const JupyterSpecsCtx *args)
{
    char kernels[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->data, .relative = "kernels", .joined = kernels);
    s_listing.entry_count = 0u;
    (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = kernels,
                     .each = jupyter_entry, .context_bytess = NULL);
    qsort(s_listing.entries, s_listing.entry_count, CLI_KERNEL_ENTRY, jupyter_entry_order);
    for (unsigned int entry = 0u; entry < s_listing.entry_count; entry += 1u)
    {
        const char *const name = s_listing.entries[entry];
        int seen = 0;
        for (unsigned int one = 0u; (one < s_listing.list_entries) && !seen; one += 1u)
        {
            seen = (cellul.eq(&(CatenaFinitaCfg){.src = s_listing.names[one], .other = name, .cap = CLI_CELLS_POOL}));
        }
        char folder[CLI_PATH_MAX_LEN];
        char file[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = kernels, .relative = name, .joined = folder);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "kernel.json", .joined = file);
        unsigned long long size = 0ULL;
        const unsigned char *const bytes = seen ? NULL
                                                : EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = file,
                                                             .size_pointer = &size, .owner = &g_cli_notebook_cells);
        if (bytes == NULL)
        {
            continue;
        }
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &s_listing.json,
                   .node_json_node_pointer = s_listing.nodes, .nodes_max = CLI_KERNEL_SPEC_NODES,
                   .text_out = s_listing.text, .text_max = CLI_KERNEL_SPEC_TEXT);
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea unread = {why, sizeof(why), 0ULL};
        // a spec is read as a text, which is UTF-8 whole
        const unsigned int root =
            EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = (const char *)bytes, .size = size)
                ? EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg,
                             .json = &s_listing.json, .text = (const char *)bytes, .size = size,
                             .tokens = s_listing.tokens, .tokens_max = CLI_KERNEL_SPEC_NODES, .err_msg = &unread)
                : CLI_JSON_NONE;
        const int read = (root != CLI_JSON_NONE) &&
                         EMBED_CALL(jupyter_spec_read, JupyterSpecsCtx, .root = root, .name = name, .folder = folder);
        g_cli_cells->persistent_buf_release((void *)(uintptr_t)bytes, (&g_cli_notebook_cells));
        if (read && !jupyter_list(args))
        {
            return 0;
        }
    }
    return 1;
}

// a kernel for each Python found that no spec listed runs, each handed on
EMBED_INLINE void jupyter_python_specs(const JupyterSpecsCtx *args)
{
    for (unsigned int index = 0u; index < args->pythons; index += 1u)
    {
        const char *const python = s_listing.pythons[index];
        CliKernelSpec *const spec = &s_listing.spec;
        memor.set(&(MemoriaCfg){.dst = spec, .val = 0, .bytes = sizeof(*spec)});
        char name[32];
        VerbaLinea name_line = {name, sizeof(name), 0ULL};
        if ((index == 0u) && args->has_environment)
        {
            mmgr_line_put(&name_line, "python3-tree");
        }
        else
        {
            mmgr_line_put(&name_line, "python3-");
            mmgr_line_decimal(&name_line, index + 1u, 1u);
        }
        mmgr_line_finish(&name_line);
        char displayed[CLI_PATH_MAX_LEN + 16u];
        VerbaLinea displayed_line = {displayed, sizeof(displayed), 0ULL};
        mmgr_line_put(&displayed_line, "Python (");
        mmgr_line_put(&displayed_line, python);
        mmgr_line_put(&displayed_line, ")");
        mmgr_line_finish(&displayed_line);
        static const char *const words[4] = {"-m", "ipykernel_launcher", "-f", "{connection_file}"};
        spec->name = EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = spec, .text = name,
                                .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        spec->display_name =
            EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = spec, .text = displayed,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = displayed, .cap = CLI_CELLS_POOL}));
        spec->language = EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = spec, .text = "python", .length = 6u);
        spec->argv[0] = EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = spec, .text = python,
                                   .length = cellul.len(&(CatenaFinitaCfg){.src = python, .cap = CLI_CELLS_POOL}));
        for (unsigned int word_text = 0u; word_text < 4u; word_text += 1u)
        {
            spec->argv[word_text + 1u] =
                EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = spec, .text = words[word_text],
                           .length = cellul.len(&(CatenaFinitaCfg){.src = words[word_text], .cap = CLI_CELLS_POOL}));
        }
        spec->argv_count = 5u;
        spec->interrupt_mode =
            EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = spec, .text = "signal", .length = 6u);
        spec->folder = EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = spec, .text = "", .length = 0ULL);
        spec->install = CLI_KERNEL_NONE;
        if (!EMBED_CALL(cli_jupyter_internal.kernel_has_ipykernel, CliJupyterCtx, .python = python))
        {
            char install[CLI_PATH_MAX_LEN + 48u];
            VerbaLinea line = {install, sizeof(install), 0ULL};
            mmgr_line_ch(&line, '"');
            mmgr_line_put(&line, python);
            mmgr_line_put(&line, "\" -m pip install ipykernel");
            mmgr_line_finish(&line);
            spec->install = EMBED_CALL(jupyter_append, JupyterSpecsCtx, .spec = spec, .text = install,
                                       .length = cellul.len(&(CatenaFinitaCfg){.src = install, .cap = CLI_CELLS_POOL}));
        }
        int runs = 0;
        for (unsigned int one = 0u; (one < s_listing.list_entries) && !runs; one += 1u)
        {
            runs =
                (cellul.eq(&(CatenaFinitaCfg){.src = s_listing.programs[one], .other = python, .cap = CLI_CELLS_POOL}));
        }
        if (!runs && !jupyter_list(args))
        {
            return;
        }
    }
}

// every spec the machine has, each handed to `each` in Jupyter's order, under the listing's lock
EMBED_INLINE void jupyter_specs_each(const JupyterSpecsCtx *args)
{
    s_listing.list_entries = 0u;
    s_listing.names = g_cli_cells->persistent_buf_alloc(
        (unsigned long long)CLI_KERNEL_LISTING * (CLI_KERNEL_ENTRY + CLI_PATH_MAX_LEN), &g_cli_notebook_cells);
    if (s_listing.names == NULL)
    {
        return;
    }
    s_listing.programs = (char (*)[CLI_PATH_MAX_LEN])(void *)(s_listing.names + CLI_KERNEL_LISTING);
    int has_environment = 0;
    const unsigned int pythons = EMBED_CALL(cli_jupyter_pythons_internal.kernel_pythons, CliJupyterPythonsCtx,
                                            .pythons = s_listing.pythons, .has_environment = &has_environment);
    const unsigned int folders =
        EMBED_CALL(cli_jupyter_pythons_python_count_internal.kernel_data_folders, CliJupyterPythonsPythonCountCtx,
                   .pythons_callback = (const char (*)[CLI_PATH_MAX_LEN])s_listing.pythons, .python_count = pythons,
                   .folders = s_listing.folders);
    int going = 1;
    unsigned int folder = 0u;
    while (going && (folder < folders))
    {
        going = EMBED_CALL(jupyter_folder_specs, JupyterSpecsCtx, .data = s_listing.folders[folder], .each = args->each,
                           .context = args->context);
        folder += 1u;
    }
    if (going)
    {
        EMBED_CALL(jupyter_python_specs, JupyterSpecsCtx, .pythons = pythons, .has_environment = has_environment,
                   .each = args->each, .context = args->context);
    }
    g_cli_cells->persistent_buf_release(s_listing.names, &g_cli_notebook_cells);
    s_listing.names = NULL;
    s_listing.programs = NULL;
}

// a spec written as serde writes one, a comma before it where it is not the first
static int jupyter_spec_write(const CliKernelEachCfg *args)
{
    const CliKernelSpec *spec = args->spec;
    VerbaLinea *const out = (VerbaLinea *)args->context;
    const char last = (out->at > 0ULL) ? out->out[out->at - 1ULL] : '[';
    mmgr_line_put(out, (last == '[') ? "{\"name\":" : ",{\"name\":");
    const char *const texts[3] = {
        EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec, .at = spec->name),
        EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec, .at = spec->display_name),
        EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec, .at = spec->language)};
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = texts[0],
               .length = cellul.len(&(CatenaFinitaCfg){.src = texts[0], .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"display_name\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = texts[1],
               .length = cellul.len(&(CatenaFinitaCfg){.src = texts[1], .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"language\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = texts[2],
               .length = cellul.len(&(CatenaFinitaCfg){.src = texts[2], .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"argv\":[");
    for (unsigned int word_text = 0u; word_text < spec->argv_count; word_text += 1u)
    {
        const char *const text = EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec,
                                            .at = spec->argv[word_text]);
        mmgr_line_put(out, (word_text > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = text,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
    }
    const char *const mode =
        EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec, .at = spec->interrupt_mode);
    mmgr_line_put(out, "],\"interrupt_mode\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = mode,
               .length = cellul.len(&(CatenaFinitaCfg){.src = mode, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"env\":{");
    unsigned int order[CLI_KERNEL_SETTINGS] = {0u};
    for (unsigned int at = 0u; at < spec->env_count; at += 1u)
    {
        // the names in their bytes' order, as a BTreeMap orders them
        unsigned int place = at;
        while ((place > 0u) &&
               (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                           .one = EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec,
                                             .at = spec->env_name[order[place - 1u]]),
                           .other_pointer = EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx,
                                                       .spec = spec, .at = spec->env_name[at])) > 0))
        {
            order[place] = order[place - 1u];
            place -= 1u;
        }
        order[place] = at;
    }
    for (unsigned int at = 0u; at < spec->env_count; at += 1u)
    {
        const char *const name = EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec,
                                            .at = spec->env_name[order[at]]);
        const char *const value = EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec,
                                             .at = spec->env_value[order[at]]);
        mmgr_line_put(out, (at > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(out, ':');
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = value,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}));
    }
    const char *const folder =
        EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec, .at = spec->folder);
    mmgr_line_put(out, "},\"folder\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = folder,
               .length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"install\":");
    if (spec->install == CLI_KERNEL_NONE)
    {
        mmgr_line_put(out, "null");
    }
    else
    {
        const char *const install =
            EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec, .at = spec->install);
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = install,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = install, .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_ch(out, '}');
    return 1;
}

void cli_jupyter_specs_write(const CliJupyterOutCfg *args)
{
    mmgr_line_ch(args->out, '[');
    EMBED_CALL(jupyter_specs_each, JupyterSpecsCtx, .each = jupyter_spec_write, .context = args->out);
    mmgr_line_ch(args->out, ']');
}

// The spec sought by name, and where it is put
typedef struct
{
    const char *name;
    CliKernelSpec *spec;
    int found;
} CliKernelNeedle;

static int jupyter_spec_find(const CliKernelEachCfg *args)
{
    const CliKernelSpec *spec = args->spec;
    CliKernelNeedle *const needle = (CliKernelNeedle *)args->context;
    if (!cellul.eq(&(CatenaFinitaCfg){
            .src = EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec, .at = spec->name),
            .other = needle->name,
            .cap = CLI_CELLS_POOL}))
    {
        return 1;
    }
    memor.cpy(&(MemoriaCfg){.dst = needle->spec, .src = spec, .bytes = sizeof(*spec)});
    needle->found = 1;
    return 0;
}

int cli_jupyter_spec_by_name(const CliJupyterNameCfg *args)
{
    CliKernelNeedle needle = {args->name, args->spec, 0};
    EMBED_CALL(jupyter_specs_each, JupyterSpecsCtx, .each = jupyter_spec_find, .context = &needle);
    return needle.found;
}
