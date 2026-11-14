// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_calls.c: the call hierarchy the tree answers where no language server does. A call of a name
// reaches the functions of that name in the caller's file, or, where its file declares none, those of the other files
// of its language, by their paths and their places. A function's callees are each function its calls reach, with the
// spans of those calls; its callers each function of the tree, by the paths in order, that calls its name where that
// call reaches it. A walk's lists are cells of the call's own, sized to the tree's functions and the calls counted;
// a function and a name are read out of a file's facts cell through the guard, by the file's token
#include "../command_line/pool.h"
#include "../text/sort.h"
#include "inspect_tree_internal.h"

#include "../text/text.h"

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
    const CliFactsCell *facts;
    const CliInspectFunction *function;
    const char *name;
    unsigned long long size;
    unsigned int file;
    unsigned int at;
    unsigned int *callees;
    unsigned long long sites;
    unsigned long long sites_max;
    CliInspectReached each;
    void *context;
    unsigned int other;
    CliInspectSpan *grouped;
    CliInspectText text;
    void *record;
    unsigned long long offset;
    unsigned long long bytes;
} InspectTreeCallsCtx;
// the record at `offset` of the facts cell `facts` read into `record`, `bytes` of it
EMBED_INLINE void calls_record(const InspectTreeCallsCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = args->facts->cell, .offset = args->offset, .dst = args->record, .bytes = (embed_word)args->bytes});
}

// whether the text `text` of the facts cell `facts` is `size` bytes of `name`
EMBED_INLINE int calls_text_is(const InspectTreeCallsCtx *args)
{
    return (args->text.size == args->size) && (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){
                                                   .cell = args->facts->cell,
                                                   .offset = CLI_FACTS_TEXT_AT(args->facts, args->text.at),
                                                   .src = args->name,
                                                   .bytes = (embed_word)args->size}) == 0);
}

// the functions of the tree's files, and the max calls one function makes
EMBED_INLINE void calls_count(const InspectTreeCallsCtx *args)
{
    unsigned long long *functions = args->functions;
    unsigned long long *calls = args->calls;
    *functions = 0ULL;
    *calls = 0ULL;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        CliTreeFile tree_file;
        CLI_TREE_FILE_READ(file, &tree_file);
        for (unsigned int at = 0u; tree_file.used && (at < tree_file.facts.functions); at += 1u)
        {
            CliInspectFunction one;
            EMBED_CALL(calls_record, InspectTreeCallsCtx, .facts = &tree_file.facts,
                       .offset = CLI_FACTS_FUNCTION_AT(&tree_file.facts, at), .record = &one, .bytes = sizeof(one));
            *functions += 1ULL;
            *calls = (one.calls > *calls) ? one.calls : *calls;
        }
    }
}

// the lists of what one call reaches carved from a cell for `functions`: the cell, NULL where it cannot be taken
EMBED_INLINE unsigned int *calls_reached_alloc(const InspectTreeCallsCtx *args)
{
    unsigned int *const cell = g_cli_guard->temporary_buf_alloc(
        (5ULL * args->functions_value + 5ULL) * sizeof(unsigned int), g_cli_inspect_owner);
    if (cell != NULL)
    {
        s_reached_file = cell;
        s_reached_at = s_reached_file + args->functions_value + 1ULL;
        s_order = s_reached_at + args->functions_value + 1ULL;
        s_sorted_file = s_order + args->functions_value + 1ULL;
        s_sorted_at = s_sorted_file + args->functions_value + 1ULL;
    }
    return cell;
}

// whether the function at `at` of the facts cell `facts` is named `size` bytes of `name`
EMBED_INLINE int calls_by_name(const InspectTreeCallsCtx *args)
{
    CliInspectFunction one;
    EMBED_CALL(calls_record, InspectTreeCallsCtx, .facts = args->facts,
               .offset = CLI_FACTS_FUNCTION_AT(args->facts, args->at), .record = &one, .bytes = sizeof(one));
    return EMBED_CALL(calls_text_is, InspectTreeCallsCtx, .facts = args->facts, .text = one.name, .name = args->name,
                      .size = args->size);
}

// whether the file at `file` declares a function named `size` bytes of `name`
EMBED_INLINE int calls_declares(const InspectTreeCallsCtx *args)
{
    CliTreeFile tree_file;
    CLI_TREE_FILE_READ(args->file, &tree_file);
    for (unsigned int at = 0u; at < tree_file.facts.functions; at += 1u)
    {
        if (EMBED_CALL(calls_by_name, InspectTreeCallsCtx, .facts = &tree_file.facts, .at = at, .name = args->name,
                       .size = args->size))
        {
            return 1;
        }
    }
    return 0;
}

// two files in the order of their paths, compared in their cells up to the shorter path's 0
EMBED_INLINE int calls_paths_order(const InspectTreeCallsCtx *args)
{
    CliTreeFile one;
    CliTreeFile other;
    CLI_TREE_FILE_READ(args->file, &one);
    CLI_TREE_FILE_READ(args->other, &other);
    const unsigned long long shorter = (one.path_size < other.path_size) ? one.path_size : other.path_size;
    const embed_iword order =
        g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){.cell = one.facts.cell,
                                                             .offset = CLI_TREE_PATH_AT,
                                                             .other_cell = other.facts.cell,
                                                             .other_offset = CLI_TREE_PATH_AT,
                                                             .bytes = (embed_word)(shorter + 1ULL)});
    return (order < 0) ? -1 : ((order > 0) ? 1 : 0);
}

// two reached functions in the order of their paths, then their places
static int calls_reached_order(const void *one, const void *other)
{
    const unsigned int left = *(const unsigned int *)one;
    const unsigned int right = *(const unsigned int *)other;
    const int paths = EMBED_CALL(calls_paths_order, InspectTreeCallsCtx, .file = s_reached_file[left],
                                 .other = s_reached_file[right]);
    if (paths != 0)
    {
        return paths;
    }
    if (s_reached_at[left] < s_reached_at[right])
    {
        return -1;
    }
    if (s_reached_at[left] > s_reached_at[right])
    {
        return 1;
    }
    return 0;
}

// two files in the order of their paths
static int calls_path_order(const void *one, const void *other)
{
    return EMBED_CALL(calls_paths_order, InspectTreeCallsCtx, .file = *(const unsigned int *)one,
                      .other = *(const unsigned int *)other);
}

// the functions a call of `size` bytes of `name` in the file at `file` reaches, into `s_reached_*`: their count
static unsigned int calls_reach(const InspectTreeCallsCtx *args)
{
    unsigned int count = 0u;
    CliTreeFile own;
    CLI_TREE_FILE_READ(args->file, &own);
    for (unsigned int at = 0u; at < own.facts.functions; at += 1u)
    {
        if (EMBED_CALL(calls_by_name, InspectTreeCallsCtx, .facts = &own.facts, .at = at, .name = args->name,
                       .size = args->size))
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
        CliTreeFile tree_file;
        CLI_TREE_FILE_READ(other, &tree_file);
        if (!tree_file.used || (other == args->file) || (tree_file.python != own.python))
        {
            continue;
        }
        for (unsigned int at = 0u; at < tree_file.facts.functions; at += 1u)
        {
            if (EMBED_CALL(calls_by_name, InspectTreeCallsCtx, .facts = &tree_file.facts, .at = at, .name = args->name,
                           .size = args->size))
            {
                s_reached_file[count] = other;
                s_reached_at[count] = at;
                s_order[count] = count;
                count += 1u;
            }
        }
    }
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_order, .count = count, .size = sizeof(s_order[0]),
               .order = calls_reached_order);
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
EMBED_INLINE unsigned char *calls_callees_alloc(const InspectTreeCallsCtx *args)
{
    const unsigned long long numbers = (5ULL * args->sites + 5ULL) * sizeof(unsigned int);
    unsigned char *const cell = g_cli_guard->temporary_buf_alloc(
        numbers + ((2ULL * args->sites + 2ULL) * sizeof(CliInspectSpan)), g_cli_inspect_owner);
    if (cell != NULL)
    {
        s_callee_file = (unsigned int *)(void *)cell;
        s_callee_at = s_callee_file + args->sites + 1ULL;
        s_callee_sites = s_callee_at + args->sites + 1ULL;
        s_callee_first = s_callee_sites + args->sites + 1ULL;
        s_site_callee = s_callee_first + args->sites + 1ULL;
        s_site = (CliInspectSpan *)(void *)(cell + numbers);
        s_grouped = s_site + args->sites + 1ULL;
    }
    return cell;
}

// the name of the call at `at` of the function `function` of the facts cell `facts` read into `name`, and the
// functions it reaches, into `s_reached_*`: their count
EMBED_INLINE unsigned int calls_call_reach(const InspectTreeCallsCtx *args)
{
    CliInspectName *const name = args->record;
    EMBED_CALL(calls_record, InspectTreeCallsCtx, .facts = args->facts,
               .offset = CLI_FACTS_NAME_AT(args->facts, args->function->call_first + args->at), .record = name,
               .bytes = sizeof(*name));
    char text[CLI_TREE_PATH_BYTES];
    if (name->name.size >= sizeof(text))
    {
        return 0u;
    }
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->facts->cell,
                                                            .offset = CLI_FACTS_TEXT_AT(args->facts, name->name.at),
                                                            .dst = text,
                                                            .bytes = name->name.size});
    return EMBED_CALL(calls_reach, InspectTreeCallsCtx, .file = args->file, .name = text, .size = name->name.size);
}

// the file at `file` passed to `each` as the function at `at` of it, with `count` sites from `grouped`
EMBED_INLINE void calls_hand(const InspectTreeCallsCtx *args)
{
    CliTreeFile tree_file;
    CLI_TREE_FILE_READ(args->file, &tree_file);
    char path[CLI_TREE_PATH_BYTES];
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = tree_file.facts.cell, .offset = CLI_TREE_PATH_AT, .dst = path, .bytes = tree_file.path_size + 1ULL});
    EMBED_CALL(args->each, CliInspectReachedCfg, .context = args->context, .path = path, .facts = &tree_file.facts,
               .function = args->at, .sites = args->grouped, .count = (unsigned int)args->sites);
}

// the callees of the function at `at` of the file at `file`, `sites` calls counted, passed to `each`
EMBED_INLINE void calls_callees_dispatch(const InspectTreeCallsCtx *args)
{
    unsigned char *const cell = EMBED_CALL(calls_callees_alloc, InspectTreeCallsCtx, .sites = args->sites_max);
    if (cell == NULL)
    {
        return;
    }
    CliTreeFile tree_file;
    CLI_TREE_FILE_READ(args->file, &tree_file);
    CliInspectFunction function;
    EMBED_CALL(calls_record, InspectTreeCallsCtx, .facts = &tree_file.facts,
               .offset = CLI_FACTS_FUNCTION_AT(&tree_file.facts, args->at), .record = &function,
               .bytes = sizeof(function));
    unsigned int callees = 0u;
    unsigned int sites = 0u;
    for (unsigned int call = 0u; call < function.calls; call += 1u)
    {
        CliInspectName name;
        const unsigned int count = EMBED_CALL(calls_call_reach, InspectTreeCallsCtx, .facts = &tree_file.facts,
                                              .function = &function, .at = call, .file = args->file, .record = &name);
        for (unsigned int one = 0u; one < count; one += 1u)
        {
            const unsigned int callee = EMBED_CALL(calls_callee, InspectTreeCallsCtx, .file = s_reached_file[one],
                                                   .at = s_reached_at[one], .callees = &callees);
            s_site_callee[sites] = callee;
            s_site[sites].from = name.from;
            s_site[sites].to = name.to;
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
        EMBED_CALL(calls_hand, InspectTreeCallsCtx, .file = s_callee_file[callee], .at = s_callee_at[callee],
                   .grouped = s_grouped + s_callee_first[callee], .sites = s_callee_sites[callee], .each = args->each,
                   .context = args->context);
    }
    g_cli_guard->persistent_buf_release(cell, g_cli_inspect_owner);
}

void cli_inspect_tree_callees(const CliInspectTreePathCfg *args)
{
    const unsigned int file =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}));
    if (file == CLI_TREE_NONE)
    {
        return;
    }
    CliTreeFile tree_file;
    CLI_TREE_FILE_READ(file, &tree_file);
    unsigned long long functions = 0ULL;
    unsigned long long calls_max = 0ULL;
    EMBED_CALL(calls_count, InspectTreeCallsCtx, .functions = &functions, .calls = &calls_max);
    unsigned int *const reached =
        (args->at < tree_file.facts.functions)
            ? EMBED_CALL(calls_reached_alloc, InspectTreeCallsCtx, .functions_value = functions)
            : NULL;
    if (reached != NULL)
    {
        // the calls counted by what each reaches, then the callees' cell taken at that count
        CliInspectFunction function;
        EMBED_CALL(calls_record, InspectTreeCallsCtx, .facts = &tree_file.facts,
                   .offset = CLI_FACTS_FUNCTION_AT(&tree_file.facts, args->at), .record = &function,
                   .bytes = sizeof(function));
        unsigned long long sites = 0ULL;
        for (unsigned int call = 0u; call < function.calls; call += 1u)
        {
            CliInspectName name;
            sites += EMBED_CALL(calls_call_reach, InspectTreeCallsCtx, .facts = &tree_file.facts, .function = &function,
                                .at = call, .file = file, .record = &name);
        }
        EMBED_CALL(calls_callees_dispatch, InspectTreeCallsCtx, .file = file, .at = args->at, .sites_max = sites,
                   .each = args->each, .context = args->context);
    }
    g_cli_guard->persistent_buf_release(reached, g_cli_inspect_owner);
}

// whether a call of `size` bytes of `name` in the file at `other` reaches the file at `file`: in its own file where
// that declares the name, in the other files of its language where it does not
EMBED_INLINE int calls_reaches(const InspectTreeCallsCtx *args)
{
    if (EMBED_CALL(calls_declares, InspectTreeCallsCtx, .file = args->other, .name = args->name, .size = args->size))
    {
        return args->other == args->file;
    }
    CliTreeFile one;
    CliTreeFile other;
    CLI_TREE_FILE_READ(args->file, &one);
    CLI_TREE_FILE_READ(args->other, &other);
    return (args->other != args->file) && (other.python == one.python);
}

// the calls the function at `at` of the facts cell `facts` makes to the `size` bytes of `name`, their spans into
// `grouped`: their count
EMBED_INLINE unsigned int calls_sites(const InspectTreeCallsCtx *args)
{
    CliInspectFunction function;
    EMBED_CALL(calls_record, InspectTreeCallsCtx, .facts = args->facts,
               .offset = CLI_FACTS_FUNCTION_AT(args->facts, args->at), .record = &function, .bytes = sizeof(function));
    unsigned int sites = 0u;
    for (unsigned int call = 0u; call < function.calls; call += 1u)
    {
        CliInspectName one;
        EMBED_CALL(calls_record, InspectTreeCallsCtx, .facts = args->facts,
                   .offset = CLI_FACTS_NAME_AT(args->facts, function.call_first + call), .record = &one,
                   .bytes = sizeof(one));
        if (EMBED_CALL(calls_text_is, InspectTreeCallsCtx, .facts = args->facts, .text = one.name, .name = args->name,
                       .size = args->size))
        {
            args->grouped[sites].from = one.from;
            args->grouped[sites].to = one.to;
            sites += 1u;
        }
    }
    return sites;
}

void cli_inspect_tree_callers(const CliInspectTreePathCfg *args)
{
    const unsigned int file =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}));
    if (file == CLI_TREE_NONE)
    {
        return;
    }
    CliTreeFile tree_file;
    CLI_TREE_FILE_READ(file, &tree_file);
    if (args->at >= tree_file.facts.functions)
    {
        return;
    }
    // the function's name read out of its cell
    CliInspectFunction function;
    EMBED_CALL(calls_record, InspectTreeCallsCtx, .facts = &tree_file.facts,
               .offset = CLI_FACTS_FUNCTION_AT(&tree_file.facts, args->at), .record = &function,
               .bytes = sizeof(function));
    char name[CLI_TREE_PATH_BYTES];
    if (function.name.size >= sizeof(name))
    {
        return;
    }
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = tree_file.facts.cell,
                            .offset = CLI_FACTS_TEXT_AT(&tree_file.facts, function.name.at),
                            .dst = name,
                            .bytes = function.name.size});
    const unsigned long long size = function.name.size;
    // the files in the order of their paths, and the calls of one caller, a cell sized to the files and the max calls
    // one function makes
    unsigned long long functions = 0ULL;
    unsigned long long calls_max = 0ULL;
    EMBED_CALL(calls_count, InspectTreeCallsCtx, .functions = &functions, .calls = &calls_max);
    const unsigned long long numbers = (g_cli_tree_files + 1ULL) * sizeof(unsigned int);
    unsigned char *const cell =
        g_cli_guard->temporary_buf_alloc(numbers + ((calls_max + 1ULL) * sizeof(CliInspectSpan)), g_cli_inspect_owner);
    if (cell == NULL)
    {
        return;
    }
    unsigned int *const sorted_files = (unsigned int *)(void *)cell;
    CliInspectSpan *const grouped = (CliInspectSpan *)(void *)(cell + ((numbers + 7ULL) & ~7ULL));
    unsigned int count = 0u;
    for (unsigned int other = 0u; other < g_cli_tree_files; other += 1u)
    {
        CliTreeFile record;
        CLI_TREE_FILE_READ(other, &record);
        if (record.used)
        {
            sorted_files[count] = other;
            count += 1u;
        }
    }
    EMBED_CALL(cli_sort, CliSortCfg, .base = sorted_files, .count = count, .size = sizeof(sorted_files[0]),
               .order = calls_path_order);
    for (unsigned int sorted = 0u; sorted < count; sorted += 1u)
    {
        const unsigned int other = sorted_files[sorted];
        CliTreeFile read;
        CLI_TREE_FILE_READ(other, &read);
        const int reaches =
            EMBED_CALL(calls_reaches, InspectTreeCallsCtx, .other = other, .file = file, .name = name, .size = size);
        for (unsigned int index = 0u; reaches && (index < read.facts.functions); index += 1u)
        {
            const unsigned int sites = EMBED_CALL(calls_sites, InspectTreeCallsCtx, .facts = &read.facts, .at = index,
                                                  .name = name, .size = size, .grouped = grouped);
            if (sites > 0u)
            {
                EMBED_CALL(calls_hand, InspectTreeCallsCtx, .file = other, .at = index, .grouped = grouped,
                           .sites = sites, .each = args->each, .context = args->context);
            }
        }
    }
    g_cli_guard->persistent_buf_release(cell, g_cli_inspect_owner);
}
