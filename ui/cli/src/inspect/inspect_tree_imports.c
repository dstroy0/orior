// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_imports.c: the files an import runs. A Python module is found from the importing file's folder, from
// the folder above its outermost package, and from the tree's top folder, in that order, as its own file or its
// package's `__init__.py`; a relative one from its package; and a Python import runs each package on the way to its
// module, the module, and each module a `from` import takes from a package. A JavaScript import is a relative path,
// with `.js`, `.mjs` or `/index.js` after it where the path names no file
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "inspect_tree_internal.h"

// the most files one import runs that are recorded
#define CLI_TREE_TARGETS 256u

typedef struct
{
    const char *root;
    unsigned long long root_size;
    const char *dotted;
    unsigned long long size;
    unsigned int found;
    unsigned int *targets;
    unsigned int *count;
    unsigned int file;
    const CliInspectImport *import;
} InspectTreeImportsCtx;
// the file a module of `size` bytes of dotted names names from the folder `root`, `root_size` bytes of it: its own file
// or its package's `__init__.py`, CLI_TREE_NONE where neither is in the tree
EMBED_INLINE unsigned int imports_module_in(const InspectTreeImportsCtx *args)
{
    unsigned long long size = args->size;
    char base[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {base, sizeof(base), 0ull};
    mmgr_line_put_n(&line, args->root, args->root_size);
    mmgr_line_put(&line, ((args->root_size > 0u) && (size > 0u)) ? "/" : "");
    for (unsigned long long at = 0ull; at < size; at += 1u)
    {
        mmgr_line_ch(&line, (args->dotted[at] == '.') ? '/' : args->dotted[at]);
    }
    const unsigned long long base_size = line.at;
    mmgr_line_put(&line, ".py");
    if (!mmgr_line_ok(&line))
    {
        return CLI_TREE_NONE;
    }
    const unsigned int own =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = base, .size = line.at);
    if ((own != CLI_TREE_NONE) || (base_size == 0u))
    {
        return own;
    }
    return EMBED_CALL(cli_inspect_tree_folder_internal.tree_find_join, CliInspectTreeFolderCtx, .folder = base,
                      .folder_size = base_size, .name = "__init__.py");
}

unsigned int cli_tree_python_module(const CliInspectTreeSizeFileCtx *args)
{
    const char *const path = CLI_TREE_FILE(args->file).path;
    const unsigned long long path_size = CLI_TREE_FILE(args->file).path_size;
    unsigned long long dots = 0ull;
    dots = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->spec, .at = dots,
                      .size = args->size, .set = ".");
    const char *const dotted = args->spec + dots;
    const unsigned long long dotted_size = args->size - dots;
    if (dots > 0u)
    {
        unsigned long long base = EMBED_CALL(cli_inspect_tree_path_internal.tree_parent, CliInspectTreePathCtx,
                                             .path = path, .size = path_size);
        for (unsigned long long up = 1ull; up < dots; up += 1u)
        {
            base = EMBED_CALL(cli_inspect_tree_path_internal.tree_parent, CliInspectTreePathCtx, .path = path,
                              .size = base);
        }
        return EMBED_CALL(imports_module_in, InspectTreeImportsCtx, .root = path, .root_size = base, .dotted = dotted,
                          .size = dotted_size);
    }
    // the file's folder, the folder above its outermost package, and the tree's top folder
    const unsigned long long folder =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_parent, CliInspectTreePathCtx, .path = path, .size = path_size);
    unsigned long long top = folder;
    while ((top > 0u) && (EMBED_CALL(cli_inspect_tree_folder_internal.tree_find_join, CliInspectTreeFolderCtx,
                                     .folder = path, .folder_size = top, .name = "__init__.py") != CLI_TREE_NONE))
    {
        top = EMBED_CALL(cli_inspect_tree_path_internal.tree_parent, CliInspectTreePathCtx, .path = path, .size = top);
    }
    const unsigned long long roots[3] = {folder, top, 0ull};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        if ((at > 0u) && (roots[at] == roots[at - 1u]))
        {
            continue;
        }
        const unsigned int found = EMBED_CALL(imports_module_in, InspectTreeImportsCtx, .root = path,
                                              .root_size = roots[at], .dotted = dotted, .size = dotted_size);
        if (found != CLI_TREE_NONE)
        {
            return found;
        }
    }
    return CLI_TREE_NONE;
}

// a target put on `targets` where it is found, `count` of them
EMBED_INLINE void imports_target(const InspectTreeImportsCtx *args)
{
    unsigned int *count = args->count;
    if ((args->found != CLI_TREE_NONE) && (*count < CLI_TREE_TARGETS))
    {
        args->targets[*count] = args->found;
        *count += 1u;
    }
}

// the files a Python import runs into `targets`: their count
EMBED_INLINE unsigned int imports_python(const InspectTreeImportsCtx *args)
{
    unsigned int file = args->file;
    const CliFacts *const facts = &CLI_TREE_FILE(file).facts;
    const char *const spec = facts->text + args->import->spec.at;
    const unsigned long long size = args->import->spec.size;
    unsigned int count = 0u;
    unsigned long long dots = 0ull;
    dots = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = spec, .at = dots, .size = size,
                      .set = ".");
    char joined[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {joined, sizeof(joined), 0ull};
    mmgr_line_put_n(&line, spec, dots);
    int parts = 0;
    for (unsigned long long at = dots; at <= size; at += 1u)
    {
        if ((at < size) && (spec[at] != '.'))
        {
            continue;
        }
        unsigned long long start = at;
        start = (dots + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = spec + dots,
                                   .at = start - dots, .set = "."));
        if (at == start)
        {
            continue;
        }
        mmgr_line_put(&line, parts ? "." : "");
        mmgr_line_put_n(&line, spec + start, at - start);
        parts = 1;
        if (mmgr_line_ok(&line))
        {
            EMBED_CALL(imports_target, InspectTreeImportsCtx,
                       .found = EMBED_CALL(cli_inspect_tree_size_file_internal.tree_python_module,
                                           CliInspectTreeSizeFileCtx, .file = file, .spec = joined, .size = line.at),
                       .targets = args->targets, .count = &count);
        }
    }
    if (!parts && (dots > 0u))
    {
        EMBED_CALL(imports_target, InspectTreeImportsCtx,
                   .found = EMBED_CALL(cli_inspect_tree_size_file_internal.tree_python_module,
                                       CliInspectTreeSizeFileCtx, .file = file, .spec = spec, .size = size),
                   .targets = args->targets, .count = &count);
    }
    for (unsigned int at = 0u; at < args->import->names; at += 1u)
    {
        const CliInspectName *const name = &facts->name[args->import->name_first + at];
        line.at = 0ull;
        mmgr_line_put_n(&line, spec, size);
        mmgr_line_put(&line, ((size > 0u) && (spec[size - 1u] == '.')) ? "" : ".");
        mmgr_line_put_n(&line, facts->text + name->name.at, name->name.size);
        if (mmgr_line_ok(&line))
        {
            EMBED_CALL(imports_target, InspectTreeImportsCtx,
                       .found = EMBED_CALL(cli_inspect_tree_size_file_internal.tree_python_module,
                                           CliInspectTreeSizeFileCtx, .file = file, .spec = joined, .size = line.at),
                       .targets = args->targets, .count = &count);
        }
    }
    unsigned int unique_count = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((args->targets[at] != file) &&
            ((unique_count == 0u) || (args->targets[unique_count - 1u] != args->targets[at])))
        {
            args->targets[unique_count] = args->targets[at];
            unique_count += 1u;
        }
    }
    return unique_count;
}

// the file a JavaScript import names by a relative path: its place, CLI_TREE_NONE where none is
EMBED_INLINE unsigned int imports_javascript(const InspectTreeImportsCtx *args)
{
    const CliTreeFile *const tree_file = &CLI_TREE_FILE(args->file);
    const char *const spec = tree_file->facts.text + args->import->spec.at;
    const unsigned long long size = args->import->spec.size;
    char base[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {base, sizeof(base), 0ull};
    mmgr_line_put_n(&line, tree_file->path,
                    EMBED_CALL(cli_inspect_tree_path_internal.tree_parent, CliInspectTreePathCtx,
                               .path = tree_file->path, .size = tree_file->path_size));
    for (unsigned long long at = 0ull; at <= size;)
    {
        const char *const slash =
            (at < size) ? memor.chr(&(MemoriaCfg){.src = spec + at, .val = '/', .bytes = (size_t)(size - at)}) : NULL;
        const unsigned long long end = (slash != NULL) ? (unsigned long long)(slash - spec) : size;
        const unsigned long long part = end - at;
        if ((part == 2u) && (memor.cmp(&(MemoriaCfg){.src = spec + at, .other = "..", .bytes = 2u}) == 0))
        {
            if (line.at == 0u)
            {
                return CLI_TREE_NONE;
            }
            line.at = EMBED_CALL(cli_inspect_tree_path_internal.tree_parent, CliInspectTreePathCtx, .path = base,
                                 .size = line.at);
        }
        else if ((part > 0u) && !((part == 1u) && (spec[at] == '.')))
        {
            mmgr_line_put(&line, (line.at > 0u) ? "/" : "");
            mmgr_line_put_n(&line, spec + at, part);
        }
        at = end + 1u;
    }
    if (!mmgr_line_ok(&line))
    {
        return CLI_TREE_NONE;
    }
    const unsigned long long base_size = line.at;
    static const char *const s_ends[4] = {"", ".js", ".mjs", "/index.js"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        line.at = base_size;
        mmgr_line_put(&line, s_ends[at]);
        const unsigned int found = mmgr_line_ok(&line)
                                       ? EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx,
                                                    .path = base, .size = line.at)
                                       : CLI_TREE_NONE;
        if (found != CLI_TREE_NONE)
        {
            return found;
        }
    }
    return CLI_TREE_NONE;
}

void cli_tree_targets(const CliInspectTreeFileCtx *args)
{
    unsigned int file = args->file;
    unsigned int import = args->import;
    const CliTreeFile *const tree_file = &CLI_TREE_FILE(file);
    const CliInspectImport *const one = &tree_file->facts.import[import];
    if (tree_file->python)
    {
        unsigned int targets[CLI_TREE_TARGETS];
        const unsigned int count =
            EMBED_CALL(imports_python, InspectTreeImportsCtx, .file = file, .import = one, .targets = targets);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            EMBED_CALL(args->each_callback, CliEachTargetCfg, .context = args->context, .target = targets[at]);
        }
        return;
    }
    const unsigned int found = EMBED_CALL(imports_javascript, InspectTreeImportsCtx, .file = file, .import = one);
    if (found != CLI_TREE_NONE)
    {
        EMBED_CALL(args->each_callback, CliEachTargetCfg, .context = args->context, .target = found);
    }
}
