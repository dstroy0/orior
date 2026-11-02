// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_calls.c: the call hierarchy the tree answers where no language server does. A call of a name
// reaches the functions of that name in the caller's file, or, where its file declares none, those of the other files
// of its language, by their paths and their places. A function's callees are each function its calls reach, with the
// spans of those calls; its callers each function of the tree, by the paths in order, that calls its name where that
// call reaches it. A walk's lists are cells sized to the tree's functions and the calls counted, given back as it ends
#include "../command_line/cells.h"
#include "inspect_tree_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// What one call reaches: each function's file and place, and the order and the stored lists they are sorted by, each
// as long as the tree has functions
static unsigned int *s_reached_file;
static unsigned int *s_reached_at;
static unsigned int *s_order;
static unsigned int *s_sorted_file;
static unsigned int *s_sorted_at;

// A function's callees, each file and place, in the order first reached, and its calls: each call's callee and span
static unsigned int *s_callee_file;
static unsigned int *s_callee_at;
static unsigned int *s_callee_sites;
static unsigned int *s_callee_first;
static unsigned int *s_site_callee;
static CliInspectSpan *s_site;
static CliInspectSpan *s_grouped;

typedef struct
{
    unsigned long long *functions;
    unsigned long long *calls;
    unsigned long long functions_value;
    const CliFacts *facts;
    const CliInspectFunction *function;
    const char *name;
    unsigned long long size;
    unsigned int file;
    unsigned int at;
    unsigned int *callees;
    unsigned long long sites;
    unsigned long long sites_most;
    CliInspectReached each;
    void *context;
    unsigned int other;
} InspectTreeCallsCtx;
// the functions of the tree's files, and the most calls one function makes
EMBED_INLINE void calls_count(const InspectTreeCallsCtx *args)
{
    unsigned long long *functions = args->functions;
    unsigned long long *calls = args->calls;
    *functions = 0ull;
    *calls = 0ull;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        const CliTreeFile *const tree_file = &CLI_TREE_FILE(file);
        for (unsigned int at = 0u; tree_file->used && (at < tree_file->facts.functions); at += 1u)
        {
            *functions += 1ull;
            *calls = (tree_file->facts.function[at].calls > *calls) ? tree_file->facts.function[at].calls : *calls;
        }
    }
}

// the lists of what one call reaches carved from a cell for `functions`: the cell, NULL where it cannot be taken
EMBED_INLINE unsigned int *calls_reached_take(const InspectTreeCallsCtx *args)
{
    unsigned int *const cell = g_cli_cells->persistent_buf_alloc(
        (5ull * args->functions_value + 5ull) * sizeof(unsigned int), &g_cli_inspect_cells);
    if (cell != NULL)
    {
        s_reached_file = cell;
        s_reached_at = s_reached_file + args->functions_value + 1ull;
        s_order = s_reached_at + args->functions_value + 1ull;
        s_sorted_file = s_order + args->functions_value + 1ull;
        s_sorted_at = s_sorted_file + args->functions_value + 1ull;
    }
    return cell;
}

// whether a function of `facts` is named `size` bytes of `name`
EMBED_INLINE int calls_by_name(const InspectTreeCallsCtx *args)
{
    return (args->function->name.size == args->size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->facts->text + args->function->name.at,
                                    .other = args->name,
                                    .bytes = (size_t)args->size}) == 0);
}

// whether the file at `file` declares a function named `size` bytes of `name`
EMBED_INLINE int calls_declares(const InspectTreeCallsCtx *args)
{
    const CliFacts *const facts = &CLI_TREE_FILE(args->file).facts;
    for (unsigned int at = 0u; at < facts->functions; at += 1u)
    {
        if (EMBED_CALL(calls_by_name, InspectTreeCallsCtx, .facts = facts, .function = &facts->function[at],
                       .name = args->name, .size = args->size))
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
        EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = CLI_TREE_FILE(s_reached_file[left]).path,
                   .other_pointer = CLI_TREE_FILE(s_reached_file[right]).path);
    if (paths != 0)
    {
        return paths;
    }
    return (s_reached_at[left] < s_reached_at[right]) ? -1 : (s_reached_at[left] > s_reached_at[right]) ? 1 : 0;
}

// two files in the order of their paths
static int calls_path_order(const void *one, const void *other)
{
    return EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = CLI_TREE_FILE(*(const unsigned int *)one).path,
                      .other_pointer = CLI_TREE_FILE(*(const unsigned int *)other).path);
}

// the functions a call of `size` bytes of `name` in the file at `file` reaches, into `s_reached_*`: their count
static unsigned int calls_reach(const InspectTreeCallsCtx *args)
{
    unsigned int count = 0u;
    const CliFacts *const own = &CLI_TREE_FILE(args->file).facts;
    for (unsigned int at = 0u; at < own->functions; at += 1u)
    {
        if (EMBED_CALL(calls_by_name, InspectTreeCallsCtx, .facts = own, .function = &own->function[at],
                       .name = args->name, .size = args->size))
        {
            s_reached_file[count] = args->file;
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
        const CliTreeFile *const tree_file = &CLI_TREE_FILE(other);
        if (!tree_file->used || (other == args->file) || (tree_file->python != CLI_TREE_FILE(args->file).python))
        {
            continue;
        }
        for (unsigned int at = 0u; at < tree_file->facts.functions; at += 1u)
        {
            if (EMBED_CALL(calls_by_name, InspectTreeCallsCtx, .facts = &tree_file->facts,
                           .function = &tree_file->facts.function[at], .name = args->name, .size = args->size))
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
        s_sorted_file[at] = s_reached_file[s_order[at]];
        s_sorted_at[at] = s_reached_at[s_order[at]];
    }
    memor.cpy(&(MemoriaCfg){.dst = s_reached_file, .src = s_sorted_file, .bytes = count * sizeof(s_reached_file[0])});
    memor.cpy(&(MemoriaCfg){.dst = s_reached_at, .src = s_sorted_at, .bytes = count * sizeof(s_reached_at[0])});
    return count;
}

// the place among the callees of the function at `at` of the file at `file`, put after the others where it is not
// among them: its place
EMBED_INLINE unsigned int calls_callee(const InspectTreeCallsCtx *args)
{
    unsigned int *callees = args->callees;
    for (unsigned int one = 0u; one < *callees; one += 1u)
    {
        if ((s_callee_file[one] == args->file) && (s_callee_at[one] == args->at))
        {
            return one;
        }
    }
    s_callee_file[*callees] = args->file;
    s_callee_at[*callees] = args->at;
    s_callee_sites[*callees] = 0u;
    *callees += 1u;
    return *callees - 1u;
}

// the lists of a function's callees and its calls carved from a cell for `sites` calls: the cell, NULL where it
// cannot be taken
EMBED_INLINE unsigned char *calls_callees_take(const InspectTreeCallsCtx *args)
{
    const unsigned long long numbers = (5ull * args->sites + 5ull) * sizeof(unsigned int);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(
        numbers + ((2ull * args->sites + 2ull) * sizeof(CliInspectSpan)), &g_cli_inspect_cells);
    if (cell != NULL)
    {
        s_callee_file = (unsigned int *)(void *)cell;
        s_callee_at = s_callee_file + args->sites + 1ull;
        s_callee_sites = s_callee_at + args->sites + 1ull;
        s_callee_first = s_callee_sites + args->sites + 1ull;
        s_site_callee = s_callee_first + args->sites + 1ull;
        s_site = (CliInspectSpan *)(void *)(cell + numbers);
        s_grouped = s_site + args->sites + 1ull;
    }
    return cell;
}

// the callees of the function at `at` of the file at `file`, `sites` calls counted, handed to `each`
EMBED_INLINE void calls_callees_dispatch(const InspectTreeCallsCtx *args)
{
    unsigned char *const cell = EMBED_CALL(calls_callees_take, InspectTreeCallsCtx, .sites = args->sites_most);
    if (cell == NULL)
    {
        return;
    }
    const CliFacts *const facts = &CLI_TREE_FILE(args->file).facts;
    const CliInspectFunction *const function = &facts->function[args->at];
    unsigned int callees = 0u;
    unsigned int sites = 0u;
    for (unsigned int call = 0u; call < function->calls; call += 1u)
    {
        const CliInspectName *const name = &facts->name[function->call_first + call];
        const unsigned int count = EMBED_CALL(calls_reach, InspectTreeCallsCtx, .file = args->file,
                                              .name = facts->text + name->name.at, .size = name->name.size);
        for (unsigned int one = 0u; one < count; one += 1u)
        {
            const unsigned int callee = EMBED_CALL(calls_callee, InspectTreeCallsCtx, .file = s_reached_file[one],
                                                   .at = s_reached_at[one], .callees = &callees);
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
        const CliTreeFile *const tree_file = &CLI_TREE_FILE(s_callee_file[callee]);
        EMBED_CALL(args->each, CliInspectReachedCfg, .context = args->context, .path = tree_file->path,
                   .facts = &tree_file->facts, .function = s_callee_at[callee],
                   .sites = s_grouped + s_callee_first[callee], .count = s_callee_sites[callee]);
    }
    g_cli_cells->persistent_buf_release(cell, &g_cli_inspect_cells);
}

void cli_inspect_tree_callees(const CliInspectTreePathCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_tree_lock);
    const unsigned int file =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
    const CliFacts *const facts = (file != CLI_TREE_NONE) ? &CLI_TREE_FILE(file).facts : NULL;
    unsigned long long functions = 0ull;
    unsigned long long calls_most = 0ull;
    EMBED_CALL(calls_count, InspectTreeCallsCtx, .functions = &functions, .calls = &calls_most);
    unsigned int *const reached =
        ((facts != NULL) && (args->at < facts->functions))
            ? EMBED_CALL(calls_reached_take, InspectTreeCallsCtx, .functions_value = functions)
            : NULL;
    if (reached != NULL)
    {
        // the calls counted by what each reaches, then the callees' cell taken at that count
        const CliInspectFunction *const function = &facts->function[args->at];
        unsigned long long sites = 0ull;
        for (unsigned int call = 0u; call < function->calls; call += 1u)
        {
            const CliInspectName *const name = &facts->name[function->call_first + call];
            sites += EMBED_CALL(calls_reach, InspectTreeCallsCtx, .file = file, .name = facts->text + name->name.at,
                                .size = name->name.size);
        }
        EMBED_CALL(calls_callees_dispatch, InspectTreeCallsCtx, .file = file, .at = args->at, .sites_most = sites,
                   .each = args->each, .context = args->context);
    }
    g_cli_cells->persistent_buf_release(reached, &g_cli_inspect_cells);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_tree_lock);
}

// whether a call of `size` bytes of `name` in the file at `other` reaches the file at `file`: in its own file where
// that declares the name, in the other files of its language where it does not
EMBED_INLINE int calls_reaches(const InspectTreeCallsCtx *args)
{
    if (EMBED_CALL(calls_declares, InspectTreeCallsCtx, .file = args->other, .name = args->name, .size = args->size))
    {
        return args->other == args->file;
    }
    return (args->other != args->file) && (CLI_TREE_FILE(args->other).python == CLI_TREE_FILE(args->file).python);
}

void cli_inspect_tree_callers(const CliInspectTreePathCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_tree_lock);
    const unsigned int file =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
    if ((file == CLI_TREE_NONE) || (args->at >= CLI_TREE_FILE(file).facts.functions))
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_tree_lock);
        return;
    }
    // the files in the order of their paths, and the calls of one caller, a cell sized to the files and the most calls
    // one function makes
    unsigned long long functions = 0ull;
    unsigned long long calls_most = 0ull;
    EMBED_CALL(calls_count, InspectTreeCallsCtx, .functions = &functions, .calls = &calls_most);
    const unsigned long long numbers = (g_cli_tree_files + 1ull) * sizeof(unsigned int);
    unsigned char *const cell = g_cli_cells->temporary_buf_alloc(
        numbers + ((calls_most + 1ull) * sizeof(CliInspectSpan)), &g_cli_inspect_cells);
    if (cell == NULL)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_tree_lock);
        return;
    }
    unsigned int *const sorted_files = (unsigned int *)(void *)cell;
    CliInspectSpan *const grouped = (CliInspectSpan *)(void *)(cell + ((numbers + 7ull) & ~7ull));
    const CliFacts *const facts = &CLI_TREE_FILE(file).facts;
    const CliInspectFunction *const function = &facts->function[args->at];
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
        const int reaches =
            EMBED_CALL(calls_reaches, InspectTreeCallsCtx, .other = other, .file = file, .name = name, .size = size);
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
                EMBED_CALL(args->each, CliInspectReachedCfg, .context = args->context,
                           .path = CLI_TREE_FILE(other).path, .facts = read, .function = index, .sites = grouped,
                           .count = sites);
            }
        }
    }
    g_cli_cells->persistent_buf_release(cell, &g_cli_inspect_cells);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_tree_lock);
}
