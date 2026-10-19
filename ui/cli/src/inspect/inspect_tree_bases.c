// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_bases.c: the class a Python file's base names. A bare name is a class of the file, or a name a `from`
// import takes, followed to the module it is taken from; a dotted one is a class of a module the file imports whole,
// by its name or its alias, or of a module a `from` import takes from a package. Six imports in, the base is outside
// the tree
#include "inspect_tree_internal.h"

// the most imports a base is followed through
#define CLI_TREE_BASE_DEPTH 6u

// whether a text of `facts` is `size` bytes of `bytes`
static int bases_same(const CliFacts *facts, CliInspectText text, const char *bytes, unsigned long long size)
{
    return (text.size == size) &&
           (memor.cmp(&(MemoriaCfg){.src = facts->text + text.at, .other = bytes, .bytes = (size_t)size}) == 0);
}

// what a name a `from` import takes is called in the file: its alias, or the name itself
static CliInspectText bases_called(const CliInspectName *name)
{
    return name->has_alias ? name->alias : name->name;
}

// the class `name` names in the module `spec`, `spec_size` bytes of it, found from the file at `file` with `taken`
// after it where `taken_size` is not 0, `depth` imports in
static int bases_in_module(unsigned int file, const char *spec, unsigned long long spec_size, const char *taken,
                           unsigned long long taken_size, const char *name, unsigned long long size, unsigned int depth,
                           unsigned int *found_file, unsigned int *found_class)
{
    char joined[CLI_PATH_LONGEST * 2u];
    CliLine line = {joined, sizeof(joined), 0ull};
    cli_line_counted(&line, spec, spec_size);
    if (taken_size > 0u)
    {
        cli_line_text(&line, ((spec_size > 0u) && (spec[spec_size - 1u] == '.')) ? "" : ".");
        cli_line_counted(&line, taken, taken_size);
    }
    if (!cli_line_fits(&line))
    {
        return 0;
    }
    const unsigned int from = cli_tree_python_module(file, joined, line.at);
    if (from == CLI_TREE_NONE)
    {
        return 0;
    }
    return cli_tree_base_of(from, name, size, depth + 1u, found_file, found_class);
}

// the class a bare `name` names in the file at `file`
static int bases_bare(unsigned int file, const char *name, unsigned long long size, unsigned int depth,
                      unsigned int *found_file, unsigned int *found_class)
{
    const CliFacts *const facts = &CLI_TREE_FILE(file).facts;
    for (unsigned int at = 0u; at < facts->classes; at += 1u)
    {
        if (bases_same(facts, facts->class_of[at].name, name, size))
        {
            *found_file = file;
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
            if (bases_same(facts, bases_called(taken), name, size))
            {
                return bases_in_module(file, facts->text + one->spec.at, one->spec.size, NULL, 0ull,
                                       facts->text + taken->name.at, taken->name.size, depth, found_file, found_class);
            }
        }
    }
    return 0;
}

// the class `name` names in the module the file at `file` reads as `module`
static int bases_dotted(unsigned int file, const char *module, unsigned long long module_size, const char *name,
                        unsigned long long size, unsigned int depth, unsigned int *found_file,
                        unsigned int *found_class)
{
    const CliFacts *const facts = &CLI_TREE_FILE(file).facts;
    for (unsigned int import = 0u; import < facts->imports; import += 1u)
    {
        const CliInspectImport *const one = &facts->import[import];
        const char *const spec = facts->text + one->spec.at;
        const int whole = one->has_alias ? bases_same(facts, one->alias, module, module_size)
                                         : bases_same(facts, one->spec, module, module_size);
        if ((one->names == 0u) && whole)
        {
            return bases_in_module(file, spec, one->spec.size, NULL, 0ull, name, size, depth, found_file, found_class);
        }
        for (unsigned int at = 0u; at < one->names; at += 1u)
        {
            const CliInspectName *const taken = &facts->name[one->name_first + at];
            if (bases_same(facts, bases_called(taken), module, module_size))
            {
                return bases_in_module(file, spec, one->spec.size, facts->text + taken->name.at, taken->name.size, name,
                                       size, depth, found_file, found_class);
            }
        }
    }
    return 0;
}

int cli_tree_base_of(unsigned int file, const char *base, unsigned long long size, unsigned int depth,
                     unsigned int *found_file, unsigned int *found_class)
{
    if (depth > CLI_TREE_BASE_DEPTH)
    {
        return 0;
    }
    unsigned long long dot = size;
    while ((dot > 0u) && (base[dot - 1u] != '.'))
    {
        dot -= 1u;
    }
    if (dot == 0u)
    {
        return bases_bare(file, base, size, depth, found_file, found_class);
    }
    return bases_dotted(file, base, dot - 1u, base + dot, size - dot, depth, found_file, found_class);
}
