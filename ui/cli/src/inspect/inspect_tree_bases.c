// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_bases.c: the class a Python file's base names. A bare name is a class of the file, or a name a `from`
// import takes, followed to the module it is taken from; a dotted one is a class of a module the file imports whole,
// by its name or its alias, or of a module a `from` import takes from a package. Six imports in, the base is outside
// the tree. A class, an import and a name are read out of a file's facts cell through the guard, by the file's token
#include "../text/text.h"
#include "inspect_tree_internal.h"

// the most imports a base is followed through
#define CLI_TREE_BASE_DEPTH 6u

// the most bytes a name or a module spec of a file is read into on the stack
#define CLI_TREE_BASE_BYTES 1024u

typedef struct
{
    const CliFactsCell *facts;
    CliInspectText text;
    const char *bytes;
    unsigned long long size;
    const CliInspectName *name_inspect_name_pointer;
    unsigned int file;
    const char *spec;
    unsigned long long spec_size;
    const char *reserved;
    unsigned long long allocated_size;
    const char *name;
    unsigned int depth;
    unsigned int *found_file;
    unsigned int *found_class;
    const char *module;
    unsigned long long module_size;
    void *record;
    unsigned long long offset;
    unsigned long long record_bytes;
    char *out;
    unsigned long long out_bytes;
} InspectTreeBasesCtx;
// the record at `offset` of the facts cell `facts` read into `record`, `record_bytes` of it
EMBED_INLINE void bases_record(const InspectTreeBasesCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->facts->cell,
                                                            .offset = args->offset,
                                                            .dst = args->record,
                                                            .bytes = (embed_word)args->record_bytes});
}

// the text `text` of the facts cell `facts` read into `out`, `out_bytes` long, a 0 after it: 1, or 0 where it does
// not fit
EMBED_INLINE int bases_text(const InspectTreeBasesCtx *args)
{
    if (args->text.size >= args->out_bytes)
    {
        return 0;
    }
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->facts->cell,
                                                            .offset = CLI_FACTS_TEXT_AT(args->facts, args->text.at),
                                                            .dst = args->out,
                                                            .bytes = args->text.size});
    args->out[args->text.size] = '\0';
    return 1;
}

// whether a text of the facts cell `facts` is `size` bytes of `bytes`
EMBED_INLINE int bases_same(const InspectTreeBasesCtx *args)
{
    const char *bytes = args->bytes;
    return (args->text.size == args->size) && (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){
                                                   .cell = args->facts->cell,
                                                   .offset = CLI_FACTS_TEXT_AT(args->facts, args->text.at),
                                                   .src = bytes,
                                                   .bytes = (embed_word)args->size}) == 0);
}

// what a name a `from` import takes is called in the file: its alias, or the name itself
EMBED_INLINE CliInspectText bases_call(const InspectTreeBasesCtx *args)
{
    return args->name_inspect_name_pointer->has_alias ? args->name_inspect_name_pointer->alias
                                                      : args->name_inspect_name_pointer->name;
}

// the class `name` names in the module `spec`, `spec_size` bytes of it, found from the file at `file` with `reserved`
// after it where `allocated_size` is not 0, `depth` imports in
EMBED_INLINE int bases_in_module(const InspectTreeBasesCtx *args)
{
    unsigned int file = args->file;
    const char *spec = args->spec;
    unsigned long long size = args->size;
    unsigned int depth = args->depth;
    unsigned int *found_file = args->found_file;
    unsigned int *found_class = args->found_class;
    char joined[CLI_TREE_PATH_BYTES];
    VerbaLinea line = {joined, sizeof(joined), 0ULL};
    mmgr_line_put_n(&line, spec, args->spec_size);
    if (args->allocated_size > 0u)
    {
        mmgr_line_put(&line, ((args->spec_size > 0u) && (spec[args->spec_size - 1u] == '.')) ? "" : ".");
        mmgr_line_put_n(&line, args->reserved, args->allocated_size);
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
    CliTreeFile tree_file;
    CLI_TREE_FILE_READ(args->file, &tree_file);
    const CliFactsCell *const facts = &tree_file.facts;
    for (unsigned int at = 0u; at < facts->classes; at += 1u)
    {
        CliInspectClass one;
        EMBED_CALL(bases_record, InspectTreeBasesCtx, .facts = facts, .offset = CLI_FACTS_CLASS_AT(facts, at),
                   .record = &one, .record_bytes = sizeof(one));
        if (EMBED_CALL(bases_same, InspectTreeBasesCtx, .facts = facts, .text = one.name, .bytes = args->name,
                       .size = args->size))
        {
            *found_file = args->file;
            *found_class = at;
            return 1;
        }
    }
    for (unsigned int import = 0u; import < facts->imports; import += 1u)
    {
        CliInspectImport one;
        EMBED_CALL(bases_record, InspectTreeBasesCtx, .facts = facts, .offset = CLI_FACTS_IMPORT_AT(facts, import),
                   .record = &one, .record_bytes = sizeof(one));
        for (unsigned int at = 0u; at < one.names; at += 1u)
        {
            CliInspectName reserved;
            EMBED_CALL(bases_record, InspectTreeBasesCtx, .facts = facts,
                       .offset = CLI_FACTS_NAME_AT(facts, one.name_first + at), .record = &reserved,
                       .record_bytes = sizeof(reserved));
            if (EMBED_CALL(bases_same, InspectTreeBasesCtx, .facts = facts,
                           .text = EMBED_CALL(bases_call, InspectTreeBasesCtx, .name_inspect_name_pointer = &reserved),
                           .bytes = args->name, .size = args->size))
            {
                char spec[CLI_TREE_BASE_BYTES];
                char taken[CLI_TREE_BASE_BYTES];
                if (!EMBED_CALL(bases_text, InspectTreeBasesCtx, .facts = facts, .text = one.spec, .out = spec,
                                .out_bytes = sizeof(spec)) ||
                    !EMBED_CALL(bases_text, InspectTreeBasesCtx, .facts = facts, .text = reserved.name, .out = taken,
                                .out_bytes = sizeof(taken)))
                {
                    return 0;
                }
                return EMBED_CALL(bases_in_module, InspectTreeBasesCtx, .file = args->file, .spec = spec,
                                  .spec_size = one.spec.size, .reserved = NULL, .allocated_size = 0ULL, .name = taken,
                                  .size = reserved.name.size, .depth = args->depth, .found_file = found_file,
                                  .found_class = found_class);
            }
        }
    }
    return 0;
}

// the class `name` names in the module the file at `file` reads as `module`
EMBED_INLINE int bases_dot(const InspectTreeBasesCtx *args)
{
    CliTreeFile tree_file;
    CLI_TREE_FILE_READ(args->file, &tree_file);
    const CliFactsCell *const facts = &tree_file.facts;
    for (unsigned int import = 0u; import < facts->imports; import += 1u)
    {
        CliInspectImport one;
        EMBED_CALL(bases_record, InspectTreeBasesCtx, .facts = facts, .offset = CLI_FACTS_IMPORT_AT(facts, import),
                   .record = &one, .record_bytes = sizeof(one));
        char spec[CLI_TREE_BASE_BYTES];
        if (!EMBED_CALL(bases_text, InspectTreeBasesCtx, .facts = facts, .text = one.spec, .out = spec,
                        .out_bytes = sizeof(spec)))
        {
            continue;
        }
        const int whole = one.has_alias ? EMBED_CALL(bases_same, InspectTreeBasesCtx, .facts = facts, .text = one.alias,
                                                     .bytes = args->module, .size = args->module_size)
                                        : EMBED_CALL(bases_same, InspectTreeBasesCtx, .facts = facts, .text = one.spec,
                                                     .bytes = args->module, .size = args->module_size);
        if ((one.names == 0u) && whole)
        {
            return EMBED_CALL(bases_in_module, InspectTreeBasesCtx, .file = args->file, .spec = spec,
                              .spec_size = one.spec.size, .reserved = NULL, .allocated_size = 0ULL, .name = args->name,
                              .size = args->size, .depth = args->depth, .found_file = args->found_file,
                              .found_class = args->found_class);
        }
        for (unsigned int at = 0u; at < one.names; at += 1u)
        {
            CliInspectName reserved;
            EMBED_CALL(bases_record, InspectTreeBasesCtx, .facts = facts,
                       .offset = CLI_FACTS_NAME_AT(facts, one.name_first + at), .record = &reserved,
                       .record_bytes = sizeof(reserved));
            if (EMBED_CALL(bases_same, InspectTreeBasesCtx, .facts = facts,
                           .text = EMBED_CALL(bases_call, InspectTreeBasesCtx, .name_inspect_name_pointer = &reserved),
                           .bytes = args->module, .size = args->module_size))
            {
                char taken[CLI_TREE_BASE_BYTES];
                if (!EMBED_CALL(bases_text, InspectTreeBasesCtx, .facts = facts, .text = reserved.name, .out = taken,
                                .out_bytes = sizeof(taken)))
                {
                    return 0;
                }
                return EMBED_CALL(bases_in_module, InspectTreeBasesCtx, .file = args->file, .spec = spec,
                                  .spec_size = one.spec.size, .reserved = taken, .allocated_size = reserved.name.size,
                                  .name = args->name, .size = args->size, .depth = args->depth,
                                  .found_file = args->found_file, .found_class = args->found_class);
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
