// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_bases.c: the class a Python file's base names. A bare name is a class of the file, or a name a `from`
// import takes, followed to the module it is taken from; a dotted one is a class of a module the file imports whole,
// by its name or its alias, or of a module a `from` import takes from a package. Six imports in, the base is outside
// the tree
#include "../text/text.h"
#include "inspect_tree_internal.h"

// the most imports a base is followed through
#define CLI_TREE_BASE_DEPTH 6u

typedef struct
{
    const CliFacts *facts;
    CliInspectText text;
    const char *bytes;
    unsigned long long size;
    const CliInspectName *name_inspect_name_pointer;
    unsigned int file;
    const char *spec;
    unsigned long long spec_size;
    const char *taken;
    unsigned long long taken_size;
    const char *name;
    unsigned int depth;
    unsigned int *found_file;
    unsigned int *found_class;
    const char *module;
    unsigned long long module_size;
} InspectTreeBasesCtx;
// whether a text of `facts` is `size` bytes of `bytes`
EMBED_INLINE int bases_same(const InspectTreeBasesCtx *args)
{
    const char *bytes = args->bytes;
    return (args->text.size == args->size) &&
           (memor.cmp(&(MemoriaCfg){
                .src = args->facts->text + args->text.at, .other = bytes, .bytes = (size_t)args->size}) == 0);
}

// what a name a `from` import takes is called in the file: its alias, or the name itself
EMBED_INLINE CliInspectText bases_call(const InspectTreeBasesCtx *args)
{
    return args->name_inspect_name_pointer->has_alias ? args->name_inspect_name_pointer->alias
                                                      : args->name_inspect_name_pointer->name;
}

// the class `name` names in the module `spec`, `spec_size` bytes of it, found from the file at `file` with `taken`
// after it where `taken_size` is not 0, `depth` imports in
EMBED_INLINE int bases_in_module(const InspectTreeBasesCtx *args)
{
    unsigned int file = args->file;
    const char *spec = args->spec;
    unsigned long long size = args->size;
    unsigned int depth = args->depth;
    unsigned int *found_file = args->found_file;
    unsigned int *found_class = args->found_class;
    char joined[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {joined, sizeof(joined), 0ull};
    mmgr_line_put_n(&line, spec, args->spec_size);
    if (args->taken_size > 0u)
    {
        mmgr_line_put(&line, ((args->spec_size > 0u) && (spec[args->spec_size - 1u] == '.')) ? "" : ".");
        mmgr_line_put_n(&line, args->taken, args->taken_size);
    }
    if (!mmgr_line_ok(&line))
    {
        return 0;
    }
    const unsigned int from = EMBED_CALL(cli_inspect_tree_size_file_internal.tree_python_module,
                                         CliInspectTreeSizeFileCtx, .file = file, .spec = joined, .size = line.at);
    if (from == CLI_TREE_NONE)
    {
        return 0;
    }
    return EMBED_CALL(cli_inspect_tree_size_internal.tree_base_get, CliInspectTreeSizeCtx, .file = from,
                      .base = args->name, .size = size, .depth = depth + 1u, .found_file = found_file,
                      .found_class = found_class);
}

// the class a bare `name` names in the file at `file`
EMBED_INLINE int bases_bare(const InspectTreeBasesCtx *args)
{
    unsigned int *found_file = args->found_file;
    unsigned int *found_class = args->found_class;
    const CliFacts *const facts = &CLI_TREE_FILE(args->file).facts;
    for (unsigned int at = 0u; at < facts->classes; at += 1u)
    {
        if (EMBED_CALL(bases_same, InspectTreeBasesCtx, .facts = facts, .text = facts->class_of[at].name,
                       .bytes = args->name, .size = args->size))
        {
            *found_file = args->file;
            *found_class = at;
            return 1;
        }
    }
    for (unsigned int import = 0u; import < facts->imports; import += 1u)
    {
        const CliInspectImport *const one = &facts->import[import];
        for (unsigned int at = 0u; at < one->names; at += 1u)
        {
            const CliInspectName *const taken = &facts->name[one->name_first + at];
            if (EMBED_CALL(bases_same, InspectTreeBasesCtx, .facts = facts,
                           .text = EMBED_CALL(bases_call, InspectTreeBasesCtx, .name_inspect_name_pointer = taken),
                           .bytes = args->name, .size = args->size))
            {
                return EMBED_CALL(bases_in_module, InspectTreeBasesCtx, .file = args->file,
                                  .spec = facts->text + one->spec.at, .spec_size = one->spec.size, .taken = NULL,
                                  .taken_size = 0ull, .name = facts->text + taken->name.at, .size = taken->name.size,
                                  .depth = args->depth, .found_file = found_file, .found_class = found_class);
            }
        }
    }
    return 0;
}

// the class `name` names in the module the file at `file` reads as `module`
EMBED_INLINE int bases_dot(const InspectTreeBasesCtx *args)
{
    const CliFacts *const facts = &CLI_TREE_FILE(args->file).facts;
    for (unsigned int import = 0u; import < facts->imports; import += 1u)
    {
        const CliInspectImport *const one = &facts->import[import];
        const char *const spec = facts->text + one->spec.at;
        const int whole = one->has_alias
                              ? EMBED_CALL(bases_same, InspectTreeBasesCtx, .facts = facts, .text = one->alias,
                                           .bytes = args->module, .size = args->module_size)
                              : EMBED_CALL(bases_same, InspectTreeBasesCtx, .facts = facts, .text = one->spec,
                                           .bytes = args->module, .size = args->module_size);
        if ((one->names == 0u) && whole)
        {
            return EMBED_CALL(bases_in_module, InspectTreeBasesCtx, .file = args->file, .spec = spec,
                              .spec_size = one->spec.size, .taken = NULL, .taken_size = 0ull, .name = args->name,
                              .size = args->size, .depth = args->depth, .found_file = args->found_file,
                              .found_class = args->found_class);
        }
        for (unsigned int at = 0u; at < one->names; at += 1u)
        {
            const CliInspectName *const taken = &facts->name[one->name_first + at];
            if (EMBED_CALL(bases_same, InspectTreeBasesCtx, .facts = facts,
                           .text = EMBED_CALL(bases_call, InspectTreeBasesCtx, .name_inspect_name_pointer = taken),
                           .bytes = args->module, .size = args->module_size))
            {
                return EMBED_CALL(bases_in_module, InspectTreeBasesCtx, .file = args->file, .spec = spec,
                                  .spec_size = one->spec.size, .taken = facts->text + taken->name.at,
                                  .taken_size = taken->name.size, .name = args->name, .size = args->size,
                                  .depth = args->depth, .found_file = args->found_file,
                                  .found_class = args->found_class);
            }
        }
    }
    return 0;
}

int cli_tree_base_get(const CliInspectTreeSizeCtx *args)
{
    if (args->depth > CLI_TREE_BASE_DEPTH)
    {
        return 0;
    }
    unsigned long long dot = args->size;
    dot = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->base, .at = dot,
                     .set = ".");
    if (dot == 0u)
    {
        return EMBED_CALL(bases_bare, InspectTreeBasesCtx, .file = args->file, .name = args->base, .size = args->size,
                          .depth = args->depth, .found_file = args->found_file, .found_class = args->found_class);
    }
    return EMBED_CALL(bases_dot, InspectTreeBasesCtx, .file = args->file, .module = args->base, .module_size = dot - 1u,
                      .name = args->base + dot, .size = args->size - dot, .depth = args->depth,
                      .found_file = args->found_file, .found_class = args->found_class);
}
