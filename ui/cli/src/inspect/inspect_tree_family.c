// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_family.c: each Python class's family: its ancestors in the tree, itself first, whether all of them are
// in it, and its descendants, itself among them; what each family sets; and each attribute a method reads of `self`
// that no class of its family sets. A class is numbered by the files in the index's order and its place among its
// file's classes. The families are held in two cells: one sized to the classes counted and the bases they name, one to
// the ancestors a walk of them counts, both given back as the index changes. A base that adds no attribute a class
// reads of `self` is not followed
#include "../command_line/cells.h"
#include "inspect_tree_internal.h"

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

void cli_tree_families_given(void)
{
    cli_cell_given(s_family.class_cell);
    cli_cell_given(s_family.ancestor_cell);
    memor.set(&(MemoriaCfg){.dst = &s_family, .val = 0, .bytes = sizeof(s_family)});
}

// the class numbered `number`
static const CliInspectClass *family_class(unsigned int number)
{
    return &CLI_TREE_FILE(s_family.class_file[number]).facts.class_of[s_family.class_at[number]];
}

// the facts of the class numbered `number`'s file
static const CliFacts *family_facts(unsigned int number)
{
    return &CLI_TREE_FILE(s_family.class_file[number]).facts;
}

// whether a base `size` bytes long is one that adds no attribute
static int family_bare(const char *base, unsigned long long size)
{
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_bare_bases[at], .cap = CLI_CELLS_POOL}) == size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_bare_bases[at], .other = base, .bytes = (size_t)size}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// the next part of a cell, `bytes` long, taken from `*at`, its start kept on a word's boundary: its start
static void *family_part(unsigned char **at, unsigned long long bytes)
{
    void *const part = *at;
    *at += (bytes + 7ull) & ~7ull;
    return part;
}

// the classes and the bases they name counted, and the cell of their lists taken: 1, or 0 where it cannot be
static int family_class_cell(void)
{
    unsigned int classes = 0u;
    unsigned long long links = 0ull;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        const CliTreeFile *const held = &CLI_TREE_FILE(file);
        for (unsigned int at = 0u; held->used && (at < held->facts.classes); at += 1u)
        {
            classes += 1u;
            links += held->python ? held->facts.class_of[at].bases : 0u;
        }
    }
    const unsigned long long words = (11ull * classes) + g_cli_tree_files + links + 16ull;
    const unsigned long long bytes = (words * sizeof(unsigned int)) + (4ull * (classes + 8ull));
    unsigned char *at = cli_cell_taken(bytes, "the tree's classes' families");
    s_family.class_cell = at;
    if (at == NULL)
    {
        return 0;
    }
    s_family.classes = classes;
    s_family.class_first = family_part(&at, (unsigned long long)g_cli_tree_files * sizeof(unsigned int));
    s_family.class_file = family_part(&at, classes * sizeof(unsigned int));
    s_family.class_at = family_part(&at, classes * sizeof(unsigned int));
    s_family.base_first = family_part(&at, classes * sizeof(unsigned int));
    s_family.bases = family_part(&at, classes * sizeof(unsigned int));
    s_family.base = family_part(&at, links * sizeof(unsigned int));
    s_family.ancestor_first = family_part(&at, classes * sizeof(unsigned int));
    s_family.ancestors = family_part(&at, classes * sizeof(unsigned int));
    s_family.descendant_first = family_part(&at, classes * sizeof(unsigned int));
    s_family.descendants = family_part(&at, classes * sizeof(unsigned int));
    s_family.seen = family_part(&at, classes * sizeof(unsigned int));
    s_family.waiting = family_part(&at, classes * sizeof(unsigned int));
    s_family.all = family_part(&at, classes);
    s_family.python = family_part(&at, classes);
    s_family.known = family_part(&at, classes);
    s_family.dynamic = family_part(&at, classes);
    return 1;
}

// each class numbered, by the files in the index's order
static void family_numbered(void)
{
    unsigned int classes = 0u;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        s_family.class_first[file] = classes;
        const CliTreeFile *const held = &CLI_TREE_FILE(file);
        for (unsigned int at = 0u; held->used && (at < held->facts.classes); at += 1u)
        {
            s_family.class_file[classes] = file;
            s_family.class_at[classes] = at;
            s_family.python[classes] = (unsigned char)held->python;
            classes += 1u;
        }
    }
}

// the number of the class at `at` of the file at `file`, CLI_TREE_NONE where it has none
static unsigned int family_number(unsigned int file, unsigned int at)
{
    const unsigned int number = s_family.class_first[file] + at;
    return ((number < s_family.classes) && (s_family.class_file[number] == file) && (s_family.class_at[number] == at))
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
        const CliFacts *const facts = family_facts(number);
        const CliInspectClass *const one = family_class(number);
        for (unsigned int at = 0u; at < one->bases; at += 1u)
        {
            const CliInspectName *const base = &facts->name[one->base_first + at];
            const char *const text = facts->text + base->name.at;
            if (family_bare(text, base->name.size))
            {
                continue;
            }
            unsigned int found_file = 0u;
            unsigned int found_class = 0u;
            const unsigned int found = cli_tree_base_of(file, text, base->name.size, 0u, &found_file, &found_class)
                                           ? family_number(found_file, found_class)
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

// the ancestors of the class numbered `key` walked, from `used` in the ancestors kept, each kept where `kept` is 1 and
// only counted where it is 0: their count after
static unsigned int family_ancestors_of(unsigned int key, unsigned int used, int kept)
{
    s_family.round += 1u;
    s_family.seen[key] = s_family.round;
    s_family.known[key] = 1u;
    s_family.dynamic[key] = 0u;
    if (kept)
    {
        s_family.ancestor_first[key] = used;
        s_family.ancestor[used] = key;
    }
    unsigned int count = 1u;
    unsigned int waiting = 1u;
    s_family.waiting[0] = key;
    while (waiting > 0u)
    {
        waiting -= 1u;
        const unsigned int one = s_family.waiting[waiting];
        if (!s_family.python[one])
        {
            s_family.known[key] = 0u;
            continue;
        }
        s_family.known[key] &= s_family.all[one];
        for (unsigned int at = 0u; at < s_family.bases[one]; at += 1u)
        {
            const unsigned int base = s_family.base[s_family.base_first[one] + at];
            if (s_family.seen[base] == s_family.round)
            {
                continue;
            }
            s_family.seen[base] = s_family.round;
            if (kept)
            {
                s_family.ancestor[used + count] = base;
            }
            count += 1u;
            s_family.waiting[waiting] = base;
            waiting += 1u;
        }
    }
    s_family.ancestors[key] = count;
    for (unsigned int at = 0u; kept && (at < count); at += 1u)
    {
        s_family.dynamic[key] |= (unsigned char)(family_class(s_family.ancestor[used + at])->dynamic != 0);
    }
    return used + count;
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
    cli_tree_families_given();
    if (!family_class_cell())
    {
        return;
    }
    family_numbered();
    family_bases();
    // the ancestors counted by one walk, their cell taken at that count, and kept by a second
    memor.set(&(MemoriaCfg){.dst = s_family.seen, .val = 0, .bytes = s_family.classes * sizeof(s_family.seen[0])});
    unsigned long long total = 0ull;
    for (unsigned int key = 0u; key < s_family.classes; key += 1u)
    {
        total = family_ancestors_of(key, 0u, 0) + total;
    }
    unsigned int *const ancestors =
        cli_cell_taken((2ull * total + 2ull) * sizeof(unsigned int), "the tree's classes' ancestors");
    s_family.ancestor_cell = ancestors;
    if (ancestors == NULL)
    {
        cli_tree_families_given();
        return;
    }
    s_family.ancestor = ancestors;
    s_family.descendant = ancestors + total + 1ull;
    memor.set(&(MemoriaCfg){.dst = s_family.seen, .val = 0, .bytes = s_family.classes * sizeof(s_family.seen[0])});
    s_family.round = 0u;
    unsigned int used = 0u;
    for (unsigned int key = 0u; key < s_family.classes; key += 1u)
    {
        used = family_ancestors_of(key, used, 1);
    }
    family_descendants();
    s_family.current = 1;
}

// whether the class numbered `number` sets `size` bytes of `name`
static int family_sets(unsigned int number, const char *name, unsigned long long size)
{
    const CliFacts *const facts = family_facts(number);
    const CliInspectClass *const one = family_class(number);
    for (unsigned int at = 0u; at < one->has; at += 1u)
    {
        const CliInspectText has = facts->name[one->has_first + at].name;
        if ((has.size == size) &&
            (memor.cmp(&(MemoriaCfg){.src = facts->text + has.at, .other = name, .bytes = (size_t)size}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// whether the family of the class numbered `lower` sets `name`, -1 where it is not all in the tree or one of it sets
// attributes by name
static int family_reads_set(unsigned int lower, const char *name, unsigned long long size)
{
    if (!s_family.known[lower] || s_family.dynamic[lower])
    {
        return -1;
    }
    for (unsigned int at = 0u; at < s_family.ancestors[lower]; at += 1u)
    {
        if (family_sets(s_family.ancestor[s_family.ancestor_first[lower] + at], name, size))
        {
            return 1;
        }
    }
    return 0;
}

// whether a read of the class numbered `key` at `read` names what is to be passed by: a name of Python's own, or one
// an earlier read named
static int family_passed(unsigned int key, unsigned int read)
{
    const CliFacts *const facts = family_facts(key);
    const CliInspectClass *const one = family_class(key);
    const CliInspectText name = facts->name[one->read_first + read].name;
    const char *const text = facts->text + name.at;
    if ((name.size >= 2u) && (memor.cmp(&(MemoriaCfg){.src = text, .other = "__", .bytes = 2u}) == 0) &&
        (memor.cmp(&(MemoriaCfg){.src = text + name.size - 2u, .other = "__", .bytes = 2u}) == 0))
    {
        return 1;
    }
    for (unsigned int earlier = 0u; earlier < read; earlier += 1u)
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
static void family_attributes_of(unsigned int key, CliTreeSaid each, void *context)
{
    const CliFacts *const facts = family_facts(key);
    const CliInspectClass *const one = family_class(key);
    for (unsigned int read = 0u; read < one->reads; read += 1u)
    {
        if (family_passed(key, read))
        {
            continue;
        }
        const CliInspectName *const name = &facts->name[one->read_first + read];
        const char *const text = facts->text + name->name.at;
        int found = 0;
        for (unsigned int at = 0u; (found == 0) && (at < s_family.descendants[key]); at += 1u)
        {
            const int sets =
                family_reads_set(s_family.descendant[s_family.descendant_first[key] + at], text, name->name.size);
            found = (sets != 0) ? sets : found;
        }
        if (found != 0)
        {
            continue;
        }
        char said[4096];
        CliLine message = {said, sizeof(said), 0ull};
        cli_line_character(&message, '`');
        cli_line_counted(&message, facts->text + one->name.at, one->name.size);
        cli_line_text(&message, "` has no `");
        cli_line_counted(&message, text, name->name.size);
        cli_line_text(&message, "`: neither it, a class it comes from, nor a class that comes from it sets `");
        cli_line_counted(&message, text, name->name.size);
        cli_line_text(&message, "`.");
        if (cli_line_fits(&message))
        {
            each(context, s_family.class_file[key], name->from, name->to, "unknown-attribute", &message);
        }
    }
}

void cli_tree_attributes(CliTreeSaid each, void *context)
{
    cli_tree_families();
    for (unsigned int key = 0u; s_family.current && (key < s_family.classes); key += 1u)
    {
        if (s_family.python[key] && s_family.known[key])
        {
            family_attributes_of(key, each, context);
        }
    }
}

// The names a family sets, each once, in a table sized to the most names one family sets: a bucket's round, and the
// name it holds
typedef struct
{
    unsigned int round;
    unsigned int size;
    const char *name;
} CliTreeSupplied;

// the names the class numbered `key`'s family sets, counted with each name as often as a class of it sets it
static unsigned long long family_supplied_count(unsigned int key)
{
    unsigned long long count = 0ull;
    for (unsigned int lower = 0u; lower < s_family.descendants[key]; lower += 1u)
    {
        const unsigned int below = s_family.descendant[s_family.descendant_first[key] + lower];
        for (unsigned int up = 0u; up < s_family.ancestors[below]; up += 1u)
        {
            count += family_class(s_family.ancestor[s_family.ancestor_first[below] + up])->has;
        }
    }
    return count;
}

// `size` bytes of `name` put among the names of the round `round` in `table`, `buckets` of them: 1 where it was not
// among them
static int family_supplied_put(CliTreeSupplied *table, unsigned int buckets, unsigned int round, const char *name,
                               unsigned int size)
{
    unsigned int bucket = 2166136261u;
    for (unsigned int at = 0u; at < size; at += 1u)
    {
        bucket = (bucket ^ (unsigned char)name[at]) * 16777619u;
    }
    bucket &= buckets - 1u;
    while (table[bucket].round == round)
    {
        if ((table[bucket].size == size) &&
            (memor.cmp(&(MemoriaCfg){.src = table[bucket].name, .other = name, .bytes = size}) == 0))
        {
            return 0;
        }
        bucket = (bucket + 1u) & (buckets - 1u);
    }
    table[bucket] = (CliTreeSupplied){round, size, name};
    return 1;
}

void cli_inspect_tree_supplied(CliInspectSupplied each, void *context)
{
    cli_lock_take(&g_cli_tree_lock);
    cli_tree_families();
    // the table's buckets twice the most names one family sets, rounded up to a power of two
    unsigned long long most = 0ull;
    for (unsigned int key = 0u; s_family.current && (key < s_family.classes); key += 1u)
    {
        const unsigned long long count = s_family.python[key] ? family_supplied_count(key) : 0ull;
        most = (count > most) ? count : most;
    }
    unsigned int buckets = 16u;
    while ((buckets < (2ull * most)) && (buckets < 0x80000000u))
    {
        buckets <<= 1u;
    }
    CliTreeSupplied *const table =
        s_family.current ? cli_cell_taken(buckets * sizeof(CliTreeSupplied), "the names a family sets") : NULL;
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
        const char *const class_name = family_facts(key)->text + family_class(key)->name.at;
        for (unsigned int lower = 0u; lower < s_family.descendants[key]; lower += 1u)
        {
            const unsigned int below = s_family.descendant[s_family.descendant_first[key] + lower];
            for (unsigned int up = 0u; up < s_family.ancestors[below]; up += 1u)
            {
                const unsigned int one = s_family.ancestor[s_family.ancestor_first[below] + up];
                const CliFacts *const facts = family_facts(one);
                const CliInspectClass *const sets = family_class(one);
                for (unsigned int at = 0u; at < sets->has; at += 1u)
                {
                    const CliInspectText name = facts->name[sets->has_first + at].name;
                    if (family_supplied_put(table, buckets, round, facts->text + name.at, name.size))
                    {
                        each(context, path, class_name, facts->text + name.at);
                    }
                }
            }
        }
    }
    cli_cell_given(table);
    cli_lock_give(&g_cli_tree_lock);
}
