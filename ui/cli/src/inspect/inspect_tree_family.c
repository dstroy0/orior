// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_family.c: each Python class's family: its ancestors in the tree, itself first, whether all of them are
// in it, and its descendants, itself among them; what each family sets; and each attribute a method reads of `self`
// that no class of its family sets. A class is numbered by the files in the tree's order and its place among its
// file's classes. The families are stored in two cells: one sized to the classes counted and the bases they name, one
// to the ancestors a walk of them counts, both given back as the tree changes. A base that adds no attribute a class
// reads of `self` is not followed
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "inspect_tree_internal.h"

// The names a family sets, each once, in a table sized to the most names one family sets: a bucket's round, and the
// name it contains
typedef struct
{
    unsigned int round;
    unsigned int size;
    const char *name;
} CliTreeSupplied;
// the bases that add no attribute a class reads of `self`
static const char *const s_bare_bases[7] = {"object",         "ABC",      "abc.ABC",        "Generic",
                                            "typing.Generic", "Protocol", "typing.Protocol"};

// The families worked out: each file's first class's number, and each class's file and place among its file's
// classes; each class's bases in the tree, whether all it names are, and whether it is a Python file's; its ancestors,
// itself first, whether all are in the tree, and whether one sets attributes by name; its descendants; the round a
// class was last seen in, the walk's waiting classes, and the round. Each list a part of the cell its count is known
// for
typedef struct
{
    int current;
    unsigned int classes;
    unsigned int *class_first;
    unsigned int *class_file;
    unsigned int *class_at;
    unsigned int *base_first;
    unsigned int *bases;
    unsigned int *base;
    unsigned char *all;
    unsigned char *python;
    unsigned int *ancestor_first;
    unsigned int *ancestors;
    unsigned int *ancestor;
    unsigned char *known;
    unsigned char *dynamic;
    unsigned int *descendant_first;
    unsigned int *descendants;
    unsigned int *descendant;
    unsigned int *seen;
    unsigned int *waiting;
    unsigned int round;
    void *class_cell;
    void *ancestor_cell;
} CliTreeFamilies;

static CliTreeFamilies s_family;

void cli_tree_families_free(void)
{
    g_cli_cells->persistent_buf_release(s_family.class_cell, &g_cli_inspect_cells);
    g_cli_cells->persistent_buf_release(s_family.ancestor_cell, &g_cli_inspect_cells);
    memor.set(&(MemoriaCfg){.dst = &s_family, .val = 0, .bytes = sizeof(s_family)});
}

typedef struct
{
    unsigned int number;
    const char *base;
    unsigned long long size;
    unsigned char **at;
    unsigned long long bytes;
    unsigned int file;
    unsigned int at_u32;
    unsigned int key;
    unsigned int used;
    int store;
    const char *name;
    unsigned int lower;
    unsigned int read;
    CliTreeFindingFn each;
    void *context;
    CliTreeSupplied *table;
    unsigned int buckets;
    unsigned int round;
    unsigned int size_u32;
} InspectTreeFamilyCtx;
// the class numbered `number`
EMBED_INLINE const CliInspectClass *family_class(const InspectTreeFamilyCtx *args)
{
    return &CLI_TREE_FILE(s_family.class_file[args->number]).facts.class_of[s_family.class_at[args->number]];
}

// the facts of the class numbered `number`'s file
EMBED_INLINE const CliFacts *family_facts(const InspectTreeFamilyCtx *args)
{
    return &CLI_TREE_FILE(s_family.class_file[args->number]).facts;
}

// whether a base `size` bytes long is one that adds no attribute
EMBED_INLINE int family_bare(const InspectTreeFamilyCtx *args)
{
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_bare_bases[at], .cap = CLI_CELLS_POOL}) == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_bare_bases[at], .other = args->base, .bytes = (size_t)args->size}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// the next part of a cell, `bytes` long, taken from `*at`, its start aligned to a word's boundary: its start
EMBED_INLINE void *family_part(const InspectTreeFamilyCtx *args)
{
    unsigned char **at = args->at;
    void *const part = *at;
    *at += (args->bytes + 7ull) & ~7ull;
    return part;
}

// the classes and the bases they name counted, and the cell of their lists taken: 1, or 0 where it cannot be
static int family_class_cell(void)
{
    unsigned int classes = 0u;
    unsigned long long links = 0ull;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        const CliTreeFile *const tree_file = &CLI_TREE_FILE(file);
        for (unsigned int at = 0u; tree_file->used && (at < tree_file->facts.classes); at += 1u)
        {
            classes += 1u;
            links += tree_file->python ? tree_file->facts.class_of[at].bases : 0u;
        }
    }
    const unsigned long long words = (11ull * classes) + g_cli_tree_files + links + 16ull;
    const unsigned long long bytes = (words * sizeof(unsigned int)) + (4ull * (classes + 8ull));
    unsigned char *at = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_inspect_cells);
    s_family.class_cell = at;
    if (at == NULL)
    {
        return 0;
    }
    s_family.classes = classes;
    s_family.class_first = EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at,
                                      .bytes = (unsigned long long)g_cli_tree_files * sizeof(unsigned int));
    s_family.class_file =
        EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes * sizeof(unsigned int));
    s_family.class_at =
        EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes * sizeof(unsigned int));
    s_family.base_first =
        EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes * sizeof(unsigned int));
    s_family.bases = EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes * sizeof(unsigned int));
    s_family.base = EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = links * sizeof(unsigned int));
    s_family.ancestor_first =
        EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes * sizeof(unsigned int));
    s_family.ancestors =
        EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes * sizeof(unsigned int));
    s_family.descendant_first =
        EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes * sizeof(unsigned int));
    s_family.descendants =
        EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes * sizeof(unsigned int));
    s_family.seen = EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes * sizeof(unsigned int));
    s_family.waiting =
        EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes * sizeof(unsigned int));
    s_family.all = EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes);
    s_family.python = EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes);
    s_family.known = EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes);
    s_family.dynamic = EMBED_CALL(family_part, InspectTreeFamilyCtx, .at = &at, .bytes = classes);
    return 1;
}

// each class numbered, by the files in the tree's order
static void family_on_number(void)
{
    unsigned int classes = 0u;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        s_family.class_first[file] = classes;
        const CliTreeFile *const tree_file = &CLI_TREE_FILE(file);
        for (unsigned int at = 0u; tree_file->used && (at < tree_file->facts.classes); at += 1u)
        {
            s_family.class_file[classes] = file;
            s_family.class_at[classes] = at;
            s_family.python[classes] = (unsigned char)tree_file->python;
            classes += 1u;
        }
    }
}

// the number of the class at `at` of the file at `file`, CLI_TREE_NONE where it has none
EMBED_INLINE unsigned int family_number(const InspectTreeFamilyCtx *args)
{
    const unsigned int number = s_family.class_first[args->file] + args->at_u32;
    return ((number < s_family.classes) && (s_family.class_file[number] == args->file) &&
            (s_family.class_at[number] == args->at_u32))
               ? number
               : CLI_TREE_NONE;
}

// each Python class's bases in the tree, and whether all it names are
static void family_bases(void)
{
    unsigned int links = 0u;
    for (unsigned int number = 0u; number < s_family.classes; number += 1u)
    {
        s_family.base_first[number] = links;
        s_family.bases[number] = 0u;
        s_family.all[number] = 1u;
        if (!s_family.python[number])
        {
            continue;
        }
        const unsigned int file = s_family.class_file[number];
        const CliFacts *const facts = EMBED_CALL(family_facts, InspectTreeFamilyCtx, .number = number);
        const CliInspectClass *const one = EMBED_CALL(family_class, InspectTreeFamilyCtx, .number = number);
        for (unsigned int at = 0u; at < one->bases; at += 1u)
        {
            const CliInspectName *const base = &facts->name[one->base_first + at];
            const char *const text = facts->text + base->name.at;
            if (EMBED_CALL(family_bare, InspectTreeFamilyCtx, .base = text, .size = base->name.size))
            {
                continue;
            }
            unsigned int found_file = 0u;
            unsigned int found_class = 0u;
            const unsigned int found =
                EMBED_CALL(cli_inspect_tree_size_internal.tree_base_get, CliInspectTreeSizeCtx, .file = file,
                           .base = text, .size = base->name.size, .depth = 0u, .found_file = &found_file,
                           .found_class = &found_class)
                    ? EMBED_CALL(family_number, InspectTreeFamilyCtx, .file = found_file, .at_u32 = found_class)
                    : CLI_TREE_NONE;
            if (found == CLI_TREE_NONE)
            {
                s_family.all[number] = 0u;
                continue;
            }
            s_family.base[links] = found;
            links += 1u;
            s_family.bases[number] += 1u;
        }
    }
}

// the ancestors of the class numbered `key` walked, from `used` in the ancestors stored, each stored where `store` is 1
// and only counted where it is 0: their count after
static unsigned int family_ancestors_get(const InspectTreeFamilyCtx *args)
{
    s_family.round += 1u;
    s_family.seen[args->key] = s_family.round;
    s_family.known[args->key] = 1u;
    s_family.dynamic[args->key] = 0u;
    if (args->store)
    {
        s_family.ancestor_first[args->key] = args->used;
        s_family.ancestor[args->used] = args->key;
    }
    unsigned int count = 1u;
    unsigned int waiting = 1u;
    s_family.waiting[0] = args->key;
    while (waiting > 0u)
    {
        waiting -= 1u;
        const unsigned int one = s_family.waiting[waiting];
        if (!s_family.python[one])
        {
            s_family.known[args->key] = 0u;
            continue;
        }
        s_family.known[args->key] &= s_family.all[one];
        for (unsigned int at = 0u; at < s_family.bases[one]; at += 1u)
        {
            const unsigned int base = s_family.base[s_family.base_first[one] + at];
            if (s_family.seen[base] == s_family.round)
            {
                continue;
            }
            s_family.seen[base] = s_family.round;
            if (args->store)
            {
                s_family.ancestor[args->used + count] = base;
            }
            count += 1u;
            s_family.waiting[waiting] = base;
            waiting += 1u;
        }
    }
    s_family.ancestors[args->key] = count;
    for (unsigned int at = 0u; args->store && (at < count); at += 1u)
    {
        s_family.dynamic[args->key] |=
            (unsigned char)(EMBED_CALL(family_class, InspectTreeFamilyCtx, .number = s_family.ancestor[args->used + at])
                                ->dynamic != 0);
    }
    return args->used + count;
}

// each class's descendants, from its ancestors'
static void family_descendants(void)
{
    memor.set(&(MemoriaCfg){
        .dst = s_family.descendants, .val = 0, .bytes = s_family.classes * sizeof(s_family.descendants[0])});
    for (unsigned int key = 0u; key < s_family.classes; key += 1u)
    {
        for (unsigned int at = 0u; at < s_family.ancestors[key]; at += 1u)
        {
            s_family.descendants[s_family.ancestor[s_family.ancestor_first[key] + at]] += 1u;
        }
    }
    unsigned int first = 0u;
    for (unsigned int number = 0u; number < s_family.classes; number += 1u)
    {
        s_family.descendant_first[number] = first;
        first += s_family.descendants[number];
        s_family.descendants[number] = 0u;
    }
    for (unsigned int key = 0u; key < s_family.classes; key += 1u)
    {
        for (unsigned int at = 0u; at < s_family.ancestors[key]; at += 1u)
        {
            const unsigned int ancestor = s_family.ancestor[s_family.ancestor_first[key] + at];
            s_family.descendant[s_family.descendant_first[ancestor] + s_family.descendants[ancestor]] = key;
            s_family.descendants[ancestor] += 1u;
        }
    }
}

void cli_tree_families(void)
{
    if (s_family.current)
    {
        return;
    }
    cli_tree_families_free();
    if (!family_class_cell())
    {
        return;
    }
    family_on_number();
    family_bases();
    // the ancestors counted by one walk, their cell taken at that count, and stored by a second
    memor.set(&(MemoriaCfg){.dst = s_family.seen, .val = 0, .bytes = s_family.classes * sizeof(s_family.seen[0])});
    unsigned long long total = 0ull;
    for (unsigned int key = 0u; key < s_family.classes; key += 1u)
    {
        total = EMBED_CALL(family_ancestors_get, InspectTreeFamilyCtx, .key = key, .used = 0u, .store = 0) + total;
    }
    unsigned int *const ancestors =
        g_cli_cells->persistent_buf_alloc((2ull * total + 2ull) * sizeof(unsigned int), &g_cli_inspect_cells);
    s_family.ancestor_cell = ancestors;
    if (ancestors == NULL)
    {
        cli_tree_families_free();
        return;
    }
    s_family.ancestor = ancestors;
    s_family.descendant = ancestors + total + 1ull;
    memor.set(&(MemoriaCfg){.dst = s_family.seen, .val = 0, .bytes = s_family.classes * sizeof(s_family.seen[0])});
    s_family.round = 0u;
    unsigned int used = 0u;
    for (unsigned int key = 0u; key < s_family.classes; key += 1u)
    {
        used = EMBED_CALL(family_ancestors_get, InspectTreeFamilyCtx, .key = key, .used = used, .store = 1);
    }
    family_descendants();
    s_family.current = 1;
}

// whether the class numbered `number` sets `size` bytes of `name`
EMBED_INLINE int family_sets(const InspectTreeFamilyCtx *args)
{
    const CliFacts *const facts = family_facts(args);
    const CliInspectClass *const one = family_class(args);
    for (unsigned int at = 0u; at < one->has; at += 1u)
    {
        const CliInspectText has = facts->name[one->has_first + at].name;
        if ((has.size == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = facts->text + has.at, .other = args->name, .bytes = (size_t)args->size}) ==
             0))
        {
            return 1;
        }
    }
    return 0;
}

// whether the family of the class numbered `lower` sets `name`, -1 where it is not all in the tree or one of it sets
// attributes by name
EMBED_INLINE int family_reads_set(const InspectTreeFamilyCtx *args)
{
    if (!s_family.known[args->lower] || s_family.dynamic[args->lower])
    {
        return -1;
    }
    for (unsigned int at = 0u; at < s_family.ancestors[args->lower]; at += 1u)
    {
        if (EMBED_CALL(family_sets, InspectTreeFamilyCtx,
                       .number = s_family.ancestor[s_family.ancestor_first[args->lower] + at], .name = args->name,
                       .size = args->size))
        {
            return 1;
        }
    }
    return 0;
}

// whether a read of the class numbered `key` at `read` names what is to be passed by: a name of Python's own, or one
// an earlier read named
EMBED_INLINE int family_pass(const InspectTreeFamilyCtx *args)
{
    const CliFacts *const facts = EMBED_CALL(family_facts, InspectTreeFamilyCtx, .number = args->key);
    const CliInspectClass *const one = EMBED_CALL(family_class, InspectTreeFamilyCtx, .number = args->key);
    const CliInspectText name = facts->name[one->read_first + args->read].name;
    const char *const text = facts->text + name.at;
    if ((name.size >= 2u) && (memor.cmp(&(MemoriaCfg){.src = text, .other = "__", .bytes = 2u}) == 0) &&
        (memor.cmp(&(MemoriaCfg){.src = text + name.size - 2u, .other = "__", .bytes = 2u}) == 0))
    {
        return 1;
    }
    for (unsigned int earlier = 0u; earlier < args->read; earlier += 1u)
    {
        const CliInspectText other = facts->name[one->read_first + earlier].name;
        if ((other.size == name.size) &&
            (memor.cmp(&(MemoriaCfg){.src = facts->text + other.at, .other = text, .bytes = name.size}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// the attributes the class numbered `key` reads of `self` that no class of its family sets, each handed to `each`
EMBED_INLINE void family_attributes_get(const InspectTreeFamilyCtx *args)
{
    const CliFacts *const facts = EMBED_CALL(family_facts, InspectTreeFamilyCtx, .number = args->key);
    const CliInspectClass *const one = EMBED_CALL(family_class, InspectTreeFamilyCtx, .number = args->key);
    for (unsigned int read = 0u; read < one->reads; read += 1u)
    {
        if (EMBED_CALL(family_pass, InspectTreeFamilyCtx, .key = args->key, .read = read))
        {
            continue;
        }
        const CliInspectName *const name = &facts->name[one->read_first + read];
        const char *const text = facts->text + name->name.at;
        int found = 0;
        for (unsigned int at = 0u; (found == 0) && (at < s_family.descendants[args->key]); at += 1u)
        {
            const int sets = EMBED_CALL(family_reads_set, InspectTreeFamilyCtx,
                                        .lower = s_family.descendant[s_family.descendant_first[args->key] + at],
                                        .name = text, .size = name->name.size);
            found = (sets != 0) ? sets : found;
        }
        if (found != 0)
        {
            continue;
        }
        char msg_buf[4096];
        VerbaLinea message = {msg_buf, sizeof(msg_buf), 0ull};
        mmgr_line_ch(&message, '`');
        mmgr_line_put_n(&message, facts->text + one->name.at, one->name.size);
        mmgr_line_put(&message, "` has no `");
        mmgr_line_put_n(&message, text, name->name.size);
        mmgr_line_put(&message, "`: neither it, a class it comes from, nor a class that comes from it sets `");
        mmgr_line_put_n(&message, text, name->name.size);
        mmgr_line_put(&message, "`.");
        if (mmgr_line_ok(&message))
        {
            EMBED_CALL(args->each, CliTreeFindingCfg, .context = args->context, .file = s_family.class_file[args->key],
                       .from = name->from, .to = name->to, .code = "unknown-attribute", .message = &message);
        }
    }
}

void cli_tree_attributes(const CliInspectTreeCtx *args)
{
    cli_tree_families();
    for (unsigned int key = 0u; s_family.current && (key < s_family.classes); key += 1u)
    {
        if (s_family.python[key] && s_family.known[key])
        {
            EMBED_CALL(family_attributes_get, InspectTreeFamilyCtx, .key = key, .each = args->each,
                       .context = args->context);
        }
    }
}

// the names the class numbered `key`'s family sets, counted with each name as often as a class of it sets it
EMBED_INLINE unsigned long long family_supplied_count(const InspectTreeFamilyCtx *args)
{
    unsigned long long count = 0ull;
    for (unsigned int lower = 0u; lower < s_family.descendants[args->key]; lower += 1u)
    {
        const unsigned int below = s_family.descendant[s_family.descendant_first[args->key] + lower];
        for (unsigned int up = 0u; up < s_family.ancestors[below]; up += 1u)
        {
            count += EMBED_CALL(family_class, InspectTreeFamilyCtx,
                                .number = s_family.ancestor[s_family.ancestor_first[below] + up])
                         ->has;
        }
    }
    return count;
}

// `size` bytes of `name` put among the names of the round `round` in `table`, `buckets` of them: 1 where it was not
// among them
EMBED_INLINE int family_supplied_put(const InspectTreeFamilyCtx *args)
{
    unsigned int bucket = 2166136261u;
    for (unsigned int at = 0u; at < args->size_u32; at += 1u)
    {
        bucket = (bucket ^ (unsigned char)args->name[at]) * 16777619u;
    }
    bucket &= args->buckets - 1u;
    while (args->table[bucket].round == args->round)
    {
        if ((args->table[bucket].size == args->size_u32) &&
            (memor.cmp(&(MemoriaCfg){.src = args->table[bucket].name, .other = args->name, .bytes = args->size_u32}) ==
             0))
        {
            return 0;
        }
        bucket = (bucket + 1u) & (args->buckets - 1u);
    }
    args->table[bucket] = (CliTreeSupplied){args->round, args->size_u32, args->name};
    return 1;
}

void cli_inspect_tree_supplied(const CliInspectTreeContextEachSuppliedCfg *args)
{
    cli_tree_families();
    // the table's buckets twice the most names one family sets, rounded up to a power of two
    unsigned long long most = 0ull;
    for (unsigned int key = 0u; s_family.current && (key < s_family.classes); key += 1u)
    {
        const unsigned long long count =
            s_family.python[key] ? EMBED_CALL(family_supplied_count, InspectTreeFamilyCtx, .key = key) : 0ull;
        most = (count > most) ? count : most;
    }
    unsigned int buckets = 16u;
    while ((buckets < (2ull * most)) && (buckets < 0x80000000u))
    {
        buckets <<= 1u;
    }
    CliTreeSupplied *const table =
        s_family.current ? g_cli_cells->temporary_buf_alloc(buckets * sizeof(CliTreeSupplied), &g_cli_inspect_cells)
                         : NULL;
    if (table != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = table, .val = 0, .bytes = buckets * sizeof(CliTreeSupplied)});
    }
    unsigned int round = 0u;
    for (unsigned int key = 0u; (table != NULL) && (key < s_family.classes); key += 1u)
    {
        if (!s_family.python[key])
        {
            continue;
        }
        round += 1u;
        const char *const path = CLI_TREE_FILE(s_family.class_file[key]).path;
        const char *const class_name = EMBED_CALL(family_facts, InspectTreeFamilyCtx, .number = key)->text +
                                       EMBED_CALL(family_class, InspectTreeFamilyCtx, .number = key)->name.at;
        for (unsigned int lower = 0u; lower < s_family.descendants[key]; lower += 1u)
        {
            const unsigned int below = s_family.descendant[s_family.descendant_first[key] + lower];
            for (unsigned int up = 0u; up < s_family.ancestors[below]; up += 1u)
            {
                const unsigned int one = s_family.ancestor[s_family.ancestor_first[below] + up];
                const CliFacts *const facts = EMBED_CALL(family_facts, InspectTreeFamilyCtx, .number = one);
                const CliInspectClass *const sets = EMBED_CALL(family_class, InspectTreeFamilyCtx, .number = one);
                for (unsigned int at = 0u; at < sets->has; at += 1u)
                {
                    const CliInspectText name = facts->name[sets->has_first + at].name;
                    if (EMBED_CALL(family_supplied_put, InspectTreeFamilyCtx, .table = table, .buckets = buckets,
                                   .round = round, .name = facts->text + name.at, .size_u32 = name.size))
                    {
                        EMBED_CALL(args->each_inspect_supplied, CliInspectSuppliedCfg, .context = args->context,
                                   .path = path, .class_name = class_name, .attribute = facts->text + name.at);
                    }
                }
            }
        }
    }
    g_cli_cells->persistent_buf_release(table, &g_cli_inspect_cells);
}
