// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_calls.c: the call hierarchy the tree's index answers where no language server does. A call of a name
// reaches the functions of that name in the caller's file, or, where its file declares none, those of the other files
// of its language, by their paths and their places. A function's callees are each function its calls reach, with the
// spans of those calls; its callers each function of the tree, by the paths in order, that calls its name where that
// call reaches it. A walk's lists are cells sized to the tree's functions and the calls counted, given back as it ends
#include "../command_line/cells.h"
#include "inspect_tree_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// What one call reaches: each function's file and place, and the order and the kept lists they are sorted by, each
// as long as the tree has functions
static unsigned int *s_reached_file;
static unsigned int *s_reached_at;
static unsigned int *s_order;
static unsigned int *s_kept_file;
static unsigned int *s_kept_at;

// A function's callees, each file and place, in the order first reached, and its calls: each call's callee and span
static unsigned int *s_callee_file;
static unsigned int *s_callee_at;
static unsigned int *s_callee_sites;
static unsigned int *s_callee_first;
static unsigned int *s_site_callee;
static CliInspectSpan *s_site;
static CliInspectSpan *s_grouped;

// the functions of the tree's files, and the most calls one function makes
static void calls_counted(unsigned long long *functions, unsigned long long *calls)
{
    *functions = 0ull;
    *calls = 0ull;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        const CliTreeFile *const held = &CLI_TREE_FILE(file);
        for (unsigned int at = 0u; held->used && (at < held->facts.functions); at += 1u)
        {
            *functions += 1ull;
            *calls = (held->facts.function[at].calls > *calls) ? held->facts.function[at].calls : *calls;
        }
    }
}

// the lists of what one call reaches carved from a cell for `functions`: the cell, NULL where it cannot be taken
static unsigned int *calls_reached_taken(unsigned long long functions)
{
    unsigned int *const cell =
        cli_cell_taken((5ull * functions + 5ull) * sizeof(unsigned int), "the functions a call reaches");
    if (cell != NULL)
    {
        s_reached_file = cell;
        s_reached_at = s_reached_file + functions + 1ull;
        s_order = s_reached_at + functions + 1ull;
        s_kept_file = s_order + functions + 1ull;
        s_kept_at = s_kept_file + functions + 1ull;
    }
    return cell;
}

// whether a function of `facts` is named `size` bytes of `name`
static int calls_named(const CliFacts *facts, const CliInspectFunction *function, const char *name,
                       unsigned long long size)
{
    return (function->name.size == size) &&
           (memor.cmp(&(MemoriaCfg){.src = facts->text + function->name.at, .other = name, .bytes = (size_t)size}) ==
            0);
}

// whether the file at `file` declares a function named `size` bytes of `name`
static int calls_declares(unsigned int file, const char *name, unsigned long long size)
{
    const CliFacts *const facts = &CLI_TREE_FILE(file).facts;
    for (unsigned int at = 0u; at < facts->functions; at += 1u)
    {
        if (calls_named(facts, &facts->function[at], name, size))
        {
            return 1;
        }
    }
    return 0;
}

// two reached functions in the order of their paths, then their places
static int calls_reached_order(const void *one, const void *other)
{
    const unsigned int left = *(const unsigned int *)one;
    const unsigned int right = *(const unsigned int *)other;
    const int paths =
        cli_text_order(CLI_TREE_FILE(s_reached_file[left]).path, CLI_TREE_FILE(s_reached_file[right]).path);
    if (paths != 0)
    {
        return paths;
    }
    return (s_reached_at[left] < s_reached_at[right]) ? -1 : (s_reached_at[left] > s_reached_at[right]) ? 1 : 0;
}

// two files in the order of their paths
static int calls_path_order(const void *one, const void *other)
{
    return cli_text_order(CLI_TREE_FILE(*(const unsigned int *)one).path,
                          CLI_TREE_FILE(*(const unsigned int *)other).path);
}

// the functions a call of `size` bytes of `name` in the file at `file` reaches, into `s_reached_*`: their count
static unsigned int calls_reached(unsigned int file, const char *name, unsigned long long size)
{
    unsigned int count = 0u;
    const CliFacts *const own = &CLI_TREE_FILE(file).facts;
    for (unsigned int at = 0u; at < own->functions; at += 1u)
    {
        if (calls_named(own, &own->function[at], name, size))
        {
            s_reached_file[count] = file;
            s_reached_at[count] = at;
            count += 1u;
        }
    }
    if (count > 0u)
    {
        return count;
    }
    for (unsigned int other = 0u; other < g_cli_tree_files; other += 1u)
    {
        const CliTreeFile *const held = &CLI_TREE_FILE(other);
        if (!held->used || (other == file) || (held->python != CLI_TREE_FILE(file).python))
        {
            continue;
        }
        for (unsigned int at = 0u; at < held->facts.functions; at += 1u)
        {
            if (calls_named(&held->facts, &held->facts.function[at], name, size))
            {
                s_reached_file[count] = other;
                s_reached_at[count] = at;
                s_order[count] = count;
                count += 1u;
            }
        }
    }
    qsort(s_order, count, sizeof(s_order[0]), calls_reached_order);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_kept_file[at] = s_reached_file[s_order[at]];
        s_kept_at[at] = s_reached_at[s_order[at]];
    }
    memor.cpy(&(MemoriaCfg){.dst = s_reached_file, .src = s_kept_file, .bytes = count * sizeof(s_reached_file[0])});
    memor.cpy(&(MemoriaCfg){.dst = s_reached_at, .src = s_kept_at, .bytes = count * sizeof(s_reached_at[0])});
    return count;
}

// the place among the callees of the function at `at` of the file at `file`, put after the others where it is not
// among them: its place
static unsigned int calls_callee(unsigned int file, unsigned int at, unsigned int *callees)
{
    for (unsigned int one = 0u; one < *callees; one += 1u)
    {
        if ((s_callee_file[one] == file) && (s_callee_at[one] == at))
        {
            return one;
        }
    }
    s_callee_file[*callees] = file;
    s_callee_at[*callees] = at;
    s_callee_sites[*callees] = 0u;
    *callees += 1u;
    return *callees - 1u;
}

// the lists of a function's callees and its calls carved from a cell for `sites` calls: the cell, NULL where it
// cannot be taken
static unsigned char *calls_callees_taken(unsigned long long sites)
{
    const unsigned long long numbers = (5ull * sites + 5ull) * sizeof(unsigned int);
    unsigned char *const cell = cli_cell_taken(numbers + ((2ull * sites + 2ull) * sizeof(CliInspectSpan)),
                                               "a function's callees and their calls");
    if (cell != NULL)
    {
        s_callee_file = (unsigned int *)(void *)cell;
        s_callee_at = s_callee_file + sites + 1ull;
        s_callee_sites = s_callee_at + sites + 1ull;
        s_callee_first = s_callee_sites + sites + 1ull;
        s_site_callee = s_callee_first + sites + 1ull;
        s_site = (CliInspectSpan *)(void *)(cell + numbers);
        s_grouped = s_site + sites + 1ull;
    }
    return cell;
}

// the callees of the function at `at` of the file at `file`, `sites` calls counted, handed to `each`
static void calls_callees_handed(unsigned int file, unsigned int at, unsigned long long sites_most,
                                 CliInspectReached each, void *context)
{
    unsigned char *const cell = calls_callees_taken(sites_most);
    if (cell == NULL)
    {
        return;
    }
    const CliFacts *const facts = &CLI_TREE_FILE(file).facts;
    const CliInspectFunction *const function = &facts->function[at];
    unsigned int callees = 0u;
    unsigned int sites = 0u;
    for (unsigned int call = 0u; call < function->calls; call += 1u)
    {
        const CliInspectName *const name = &facts->name[function->call_first + call];
        const unsigned int count = calls_reached(file, facts->text + name->name.at, name->name.size);
        for (unsigned int one = 0u; one < count; one += 1u)
        {
            const unsigned int callee = calls_callee(s_reached_file[one], s_reached_at[one], &callees);
            s_site_callee[sites] = callee;
            s_site[sites].from = name->from;
            s_site[sites].to = name->to;
            s_callee_sites[callee] += 1u;
            sites += 1u;
        }
    }
    unsigned int first = 0u;
    for (unsigned int callee = 0u; callee < callees; callee += 1u)
    {
        s_callee_first[callee] = first;
        first += s_callee_sites[callee];
        s_callee_sites[callee] = 0u;
    }
    for (unsigned int site = 0u; site < sites; site += 1u)
    {
        const unsigned int callee = s_site_callee[site];
        s_grouped[s_callee_first[callee] + s_callee_sites[callee]] = s_site[site];
        s_callee_sites[callee] += 1u;
    }
    for (unsigned int callee = 0u; callee < callees; callee += 1u)
    {
        const CliTreeFile *const held = &CLI_TREE_FILE(s_callee_file[callee]);
        each(context, held->path, &held->facts, s_callee_at[callee], s_grouped + s_callee_first[callee],
             s_callee_sites[callee]);
    }
    cli_cell_given(cell);
}

void cli_inspect_tree_callees(const char *path, unsigned int at, CliInspectReached each, void *context)
{
    cli_lock_take(&g_cli_tree_lock);
    const unsigned int file = cli_tree_find(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    const CliFacts *const facts = (file != CLI_TREE_NONE) ? &CLI_TREE_FILE(file).facts : NULL;
    unsigned long long functions = 0ull;
    unsigned long long calls_most = 0ull;
    calls_counted(&functions, &calls_most);
    unsigned int *const reached = ((facts != NULL) && (at < facts->functions)) ? calls_reached_taken(functions) : NULL;
    if (reached != NULL)
    {
        // the calls counted by what each reaches, then the callees' cell taken at that count
        const CliInspectFunction *const function = &facts->function[at];
        unsigned long long sites = 0ull;
        for (unsigned int call = 0u; call < function->calls; call += 1u)
        {
            const CliInspectName *const name = &facts->name[function->call_first + call];
            sites += calls_reached(file, facts->text + name->name.at, name->name.size);
        }
        calls_callees_handed(file, at, sites, each, context);
    }
    cli_cell_given(reached);
    cli_lock_give(&g_cli_tree_lock);
}

// whether a call of `size` bytes of `name` in the file at `other` reaches the file at `file`: in its own file where
// that declares the name, in the other files of its language where it does not
static int calls_reaches(unsigned int other, unsigned int file, const char *name, unsigned long long size)
{
    if (calls_declares(other, name, size))
    {
        return other == file;
    }
    return (other != file) && (CLI_TREE_FILE(other).python == CLI_TREE_FILE(file).python);
}

void cli_inspect_tree_callers(const char *path, unsigned int at, CliInspectReached each, void *context)
{
    cli_lock_take(&g_cli_tree_lock);
    const unsigned int file = cli_tree_find(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    if ((file == CLI_TREE_NONE) || (at >= CLI_TREE_FILE(file).facts.functions))
    {
        cli_lock_give(&g_cli_tree_lock);
        return;
    }
    // the files in the order of their paths, and the calls of one caller, a cell sized to the files and the most calls
    // one function makes
    unsigned long long functions = 0ull;
    unsigned long long calls_most = 0ull;
    calls_counted(&functions, &calls_most);
    const unsigned long long numbers = (g_cli_tree_files + 1ull) * sizeof(unsigned int);
    unsigned char *const cell =
        cli_cell_taken(numbers + ((calls_most + 1ull) * sizeof(CliInspectSpan)), "a function's callers");
    if (cell == NULL)
    {
        cli_lock_give(&g_cli_tree_lock);
        return;
    }
    unsigned int *const sorted_files = (unsigned int *)(void *)cell;
    CliInspectSpan *const grouped = (CliInspectSpan *)(void *)(cell + ((numbers + 7ull) & ~7ull));
    const CliFacts *const facts = &CLI_TREE_FILE(file).facts;
    const CliInspectFunction *const function = &facts->function[at];
    const char *const name = facts->text + function->name.at;
    const unsigned long long size = function->name.size;
    unsigned int count = 0u;
    for (unsigned int other = 0u; other < g_cli_tree_files; other += 1u)
    {
        if (CLI_TREE_FILE(other).used)
        {
            sorted_files[count] = other;
            count += 1u;
        }
    }
    qsort(sorted_files, count, sizeof(sorted_files[0]), calls_path_order);
    for (unsigned int sorted = 0u; sorted < count; sorted += 1u)
    {
        const unsigned int other = sorted_files[sorted];
        const CliFacts *const read = &CLI_TREE_FILE(other).facts;
        const int reaches = calls_reaches(other, file, name, size);
        for (unsigned int index = 0u; reaches && (index < read->functions); index += 1u)
        {
            const CliInspectFunction *const caller = &read->function[index];
            unsigned int sites = 0u;
            for (unsigned int call = 0u; call < caller->calls; call += 1u)
            {
                const CliInspectName *const one = &read->name[caller->call_first + call];
                if ((one->name.size == size) &&
                    (memor.cmp(&(MemoriaCfg){.src = read->text + one->name.at, .other = name, .bytes = (size_t)size}) ==
                     0))
                {
                    grouped[sites].from = one->from;
                    grouped[sites].to = one->to;
                    sites += 1u;
                }
            }
            if (sites > 0u)
            {
                each(context, CLI_TREE_FILE(other).path, read, index, grouped, sites);
            }
        }
    }
    cli_cell_given(cell);
    cli_lock_give(&g_cli_tree_lock);
}
