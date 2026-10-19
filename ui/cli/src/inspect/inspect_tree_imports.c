// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_imports.c: the files an import runs. A Python module is found from the importing file's folder, from
// the folder above its outermost package, and from the tree's top folder, in that order, as its own file or its
// package's `__init__.py`; a relative one from its package; and a Python import runs each package on the way to its
// module, the module, and each module a `from` import takes from a package. A JavaScript import is a relative path,
// with `.js`, `.mjs` or `/index.js` after it where the path names no file
#include "inspect_tree_internal.h"

// the most files one import runs that are kept
#define CLI_TREE_TARGETS 256u

// the file a module of `size` bytes of dotted names names from the folder `root`, `root_size` bytes of it: its own file
// or its package's `__init__.py`, CLI_TREE_NONE where neither is in the index
static unsigned int imports_module_in(const char *root, unsigned long long root_size, const char *dotted,
                                      unsigned long long size)
{
    char base[CLI_PATH_LONGEST * 2u];
    CliLine line = {base, sizeof(base), 0ull};
    cli_line_counted(&line, root, root_size);
    cli_line_text(&line, ((root_size > 0u) && (size > 0u)) ? "/" : "");
    for (unsigned long long at = 0ull; at < size; at += 1u)
    {
        cli_line_character(&line, (dotted[at] == '.') ? '/' : dotted[at]);
    }
    const unsigned long long base_size = line.at;
    cli_line_text(&line, ".py");
    if (!cli_line_fits(&line))
    {
        return CLI_TREE_NONE;
    }
    const unsigned int own = cli_tree_find(base, line.at);
    if ((own != CLI_TREE_NONE) || (base_size == 0u))
    {
        return own;
    }
    return cli_tree_find_joined(base, base_size, "__init__.py");
}

unsigned int cli_tree_python_module(unsigned int file, const char *spec, unsigned long long size)
{
    const char *const path = CLI_TREE_FILE(file).path;
    const unsigned long long path_size = CLI_TREE_FILE(file).path_size;
    unsigned long long dots = 0ull;
    while ((dots < size) && (spec[dots] == '.'))
    {
        dots += 1u;
    }
    const char *const dotted = spec + dots;
    const unsigned long long dotted_size = size - dots;
    if (dots > 0u)
    {
        unsigned long long base = cli_tree_parent(path, path_size);
        for (unsigned long long up = 1ull; up < dots; up += 1u)
        {
            base = cli_tree_parent(path, base);
        }
        return imports_module_in(path, base, dotted, dotted_size);
    }
    // the file's folder, the folder above its outermost package, and the tree's top folder
    const unsigned long long folder = cli_tree_parent(path, path_size);
    unsigned long long top = folder;
    while ((top > 0u) && (cli_tree_find_joined(path, top, "__init__.py") != CLI_TREE_NONE))
    {
        top = cli_tree_parent(path, top);
    }
    const unsigned long long roots[3] = {folder, top, 0ull};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        if ((at > 0u) && (roots[at] == roots[at - 1u]))
        {
            continue;
        }
        const unsigned int found = imports_module_in(path, roots[at], dotted, dotted_size);
        if (found != CLI_TREE_NONE)
        {
            return found;
        }
    }
    return CLI_TREE_NONE;
}

// a target put on `targets` where it is found, `count` of them
static void imports_target(unsigned int found, unsigned int *targets, unsigned int *count)
{
    if ((found != CLI_TREE_NONE) && (*count < CLI_TREE_TARGETS))
    {
        targets[*count] = found;
        *count += 1u;
    }
}

// the files a Python import runs into `targets`: their count
static unsigned int imports_python(unsigned int file, const CliInspectImport *import, unsigned int *targets)
{
    const CliFacts *const facts = &CLI_TREE_FILE(file).facts;
    const char *const spec = facts->text + import->spec.at;
    const unsigned long long size = import->spec.size;
    unsigned int count = 0u;
    unsigned long long dots = 0ull;
    while ((dots < size) && (spec[dots] == '.'))
    {
        dots += 1u;
    }
    char joined[CLI_PATH_LONGEST * 2u];
    CliLine line = {joined, sizeof(joined), 0ull};
    cli_line_counted(&line, spec, dots);
    int parts = 0;
    for (unsigned long long at = dots; at <= size; at += 1u)
    {
        if ((at < size) && (spec[at] != '.'))
        {
            continue;
        }
        unsigned long long start = at;
        while ((start > dots) && (spec[start - 1u] != '.'))
        {
            start -= 1u;
        }
        if (at == start)
        {
            continue;
        }
        cli_line_text(&line, parts ? "." : "");
        cli_line_counted(&line, spec + start, at - start);
        parts = 1;
        if (cli_line_fits(&line))
        {
            imports_target(cli_tree_python_module(file, joined, line.at), targets, &count);
        }
    }
    if (!parts && (dots > 0u))
    {
        imports_target(cli_tree_python_module(file, spec, size), targets, &count);
    }
    for (unsigned int at = 0u; at < import->names; at += 1u)
    {
        const CliInspectName *const name = &facts->name[import->name_first + at];
        line.at = 0ull;
        cli_line_counted(&line, spec, size);
        cli_line_text(&line, ((size > 0u) && (spec[size - 1u] == '.')) ? "" : ".");
        cli_line_counted(&line, facts->text + name->name.at, name->name.size);
        if (cli_line_fits(&line))
        {
            imports_target(cli_tree_python_module(file, joined, line.at), targets, &count);
        }
    }
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((targets[at] != file) && ((kept == 0u) || (targets[kept - 1u] != targets[at])))
        {
            targets[kept] = targets[at];
            kept += 1u;
        }
    }
    return kept;
}

// the file a JavaScript import names by a relative path: its place, CLI_TREE_NONE where none is
static unsigned int imports_javascript(unsigned int file, const CliInspectImport *import)
{
    const CliTreeFile *const held = &CLI_TREE_FILE(file);
    const char *const spec = held->facts.text + import->spec.at;
    const unsigned long long size = import->spec.size;
    char base[CLI_PATH_LONGEST * 2u];
    CliLine line = {base, sizeof(base), 0ull};
    cli_line_counted(&line, held->path, cli_tree_parent(held->path, held->path_size));
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
            line.at = cli_tree_parent(base, line.at);
        }
        else if ((part > 0u) && !((part == 1u) && (spec[at] == '.')))
        {
            cli_line_text(&line, (line.at > 0u) ? "/" : "");
            cli_line_counted(&line, spec + at, part);
        }
        at = end + 1u;
    }
    if (!cli_line_fits(&line))
    {
        return CLI_TREE_NONE;
    }
    const unsigned long long base_size = line.at;
    static const char *const s_ends[4] = {"", ".js", ".mjs", "/index.js"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        line.at = base_size;
        cli_line_text(&line, s_ends[at]);
        const unsigned int found = cli_line_fits(&line) ? cli_tree_find(base, line.at) : CLI_TREE_NONE;
        if (found != CLI_TREE_NONE)
        {
            return found;
        }
    }
    return CLI_TREE_NONE;
}

void cli_tree_targets(unsigned int file, unsigned int import, void (*each)(void *context, unsigned int target),
                      void *context)
{
    const CliTreeFile *const held = &CLI_TREE_FILE(file);
    const CliInspectImport *const one = &held->facts.import[import];
    if (held->python)
    {
        unsigned int targets[CLI_TREE_TARGETS];
        const unsigned int count = imports_python(file, one, targets);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            each(context, targets[at]);
        }
        return;
    }
    const unsigned int found = imports_javascript(file, one);
    if (found != CLI_TREE_NONE)
    {
        each(context, found);
    }
}
