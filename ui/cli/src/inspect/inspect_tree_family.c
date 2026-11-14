// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_family.c: each Python class's family: its ancestors in the tree, itself first, whether all of them are
// in it, and its descendants, itself among them; what each family sets; and each attribute a method reads of `self`
// that no class of its family sets. A class is numbered by the files in the tree's order and its place among its
// file's classes. The families are worked out in two cells of the working alone, one sized to the classes counted and
// the bases they name, one to the ancestors a walk of them counts, then written into two movable cells of the same
// layout that stand until the tree changes, each list read out of them a word at a time. A class and a name are read
// out of a file's facts cell through the guard, by the file's token. A base that adds no attribute a class reads of
// `self` is not followed
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "inspect_tree_internal.h"

// the most bytes a class's or an attribute's name is read into on the stack
#define CLI_TREE_FAMILY_NAME_BYTES 1024u

// The names a family sets, each once, in a table sized to the most names one family sets: a bucket's round, and the
// name it contains, where it stands in the facts cell of the class that sets it
typedef struct
{
    unsigned int round;
    unsigned int size;
    const CarcerMovableEntry *cell;
    unsigned long long at;
} CliTreeSupplied;
// the bases that add no attribute a class reads of `self`
static const char *const s_bare_bases[7] = {"object",         "ABC",      "abc.ABC",        "Generic",
                                            "typing.Generic", "Protocol", "typing.Protocol"};

// The families worked out: each file's first class's number, and each class's file and place among its file's
// classes; each class's bases in the tree, whether all it names are, and whether it is a Python file's; its ancestors,
// itself first, whether all are in the tree, and whether one sets attributes by name; its descendants; the round a
// class was last seen in, the walk's waiting classes, and the round. While the families are worked out each list is
// a part of a cell of the working, `laid` and `laid_ancestors`, which the pointers name; once they stand the lists are
// in the movable cells `tables_cell` and `ancestors_cell` at the places `*_at`, the pointers NULL
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
    unsigned char *laid;
    unsigned long long laid_bytes;
    unsigned char *laid_ancestors;
    unsigned long long laid_ancestors_bytes;
    const CarcerMovableEntry *tables_cell;
    const CarcerMovableEntry *ancestors_cell;
    unsigned long long class_file_at;
    unsigned long long class_at_at;
    unsigned long long python_at;
    unsigned long long ancestor_first_at;
    unsigned long long ancestors_at;
    unsigned long long known_at;
    unsigned long long dynamic_at;
    unsigned long long descendant_first_at;
    unsigned long long descendants_at;
    unsigned long long ancestor_at;
    unsigned long long descendant_at;
} CliTreeFamilies;

static CliTreeFamilies s_family;

void cli_tree_families_free(void)
{
    g_cli_guard->persistent_buf_release(s_family.laid, g_cli_inspect_owner);
    g_cli_guard->persistent_buf_release(s_family.laid_ancestors, g_cli_inspect_owner);
    if (s_family.tables_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_family.tables_cell, g_cli_inspect_owner);
    }
    if (s_family.ancestors_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_family.ancestors_cell, g_cli_inspect_owner);
    }
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
    CliInspectSupplied supplied;
    const char *path;
    const char *class_name;
    const CarcerMovableEntry *cell;
    unsigned long long table_at;
    unsigned int index;
    CliTreeFile *tree_file;
    CliInspectClass *class_of;
    const CliFactsCell *facts;
    CliInspectText text;
    char *out;
    unsigned long long out_bytes;
} InspectTreeFamilyCtx;

// the word `index` of the list at `table_at` in the movable cell `cell`, and the byte `index` of the list there
EMBED_INLINE unsigned int family_word(const InspectTreeFamilyCtx *args)
{
    unsigned int word = 0u;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = args->cell,
                            .offset = args->table_at + ((unsigned long long)args->index * sizeof(word)),
                            .dst = &word,
                            .bytes = sizeof(word)});
    return word;
}

EMBED_INLINE unsigned char family_byte(const InspectTreeFamilyCtx *args)
{
    unsigned char byte = 0u;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = args->cell, .offset = args->table_at + (unsigned long long)args->index, .dst = &byte, .bytes = 1ULL});
    return byte;
}

// the file of the class numbered `number`, and its place among its file's classes: out of the lists as they are laid
// while the families are worked out, else out of their movable cell
EMBED_INLINE unsigned int family_class_file(const InspectTreeFamilyCtx *args)
{
    return (s_family.class_file != NULL) ? s_family.class_file[args->number]
                                         : EMBED_CALL(family_word, InspectTreeFamilyCtx, .cell = s_family.tables_cell,
                                                      .table_at = s_family.class_file_at, .index = args->number);
}

EMBED_INLINE unsigned int family_class_at(const InspectTreeFamilyCtx *args)
{
    return (s_family.class_at != NULL) ? s_family.class_at[args->number]
                                       : EMBED_CALL(family_word, InspectTreeFamilyCtx, .cell = s_family.tables_cell,
                                                    .table_at = s_family.class_at_at, .index = args->number);
}

// the record of the class numbered `number`'s file read into `tree_file`, and the class itself read into `class_of`
// out of that file's facts cell
EMBED_INLINE void family_class(const InspectTreeFamilyCtx *args)
{
    CLI_TREE_FILE_READ(EMBED_CALL(family_class_file, InspectTreeFamilyCtx, .number = args->number), args->tree_file);
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = args->tree_file->facts.cell,
        .offset = CLI_FACTS_CLASS_AT(&args->tree_file->facts,
                                     EMBED_CALL(family_class_at, InspectTreeFamilyCtx, .number = args->number)),
        .dst = args->class_of,
        .bytes = sizeof(*args->class_of)});
}

// the name at `index` of the facts cell `facts` read into `class_of`'s place: its name text
EMBED_INLINE CliInspectName family_name(const InspectTreeFamilyCtx *args)
{
    CliInspectName name;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->facts->cell,
                                                            .offset = CLI_FACTS_NAME_AT(args->facts, args->index),
                                                            .dst = &name,
                                                            .bytes = sizeof(name)});
    return name;
}

// the text `text` of the facts cell `facts` read into `out`, `out_bytes` long, a 0 after it: 1, or 0 where it does
// not fit
EMBED_INLINE int family_text(const InspectTreeFamilyCtx *args)
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

// whether the text `text` of the facts cell `facts` is `size` bytes of `name`
EMBED_INLINE int family_text_is(const InspectTreeFamilyCtx *args)
{
    return (args->text.size == args->size) && (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){
                                                   .cell = args->facts->cell,
                                                   .offset = CLI_FACTS_TEXT_AT(args->facts, args->text.at),
                                                   .src = args->name,
                                                   .bytes = (embed_word)args->size}) == 0);
}

// the word `index` of a class's list at `table_at` in the tables' cell, and the byte there; the word `index` of a
// list at `table_at` in the ancestors' cell
EMBED_INLINE unsigned int family_table_word(const InspectTreeFamilyCtx *args)
{
    return EMBED_CALL(family_word, InspectTreeFamilyCtx, .cell = s_family.tables_cell, .table_at = args->table_at,
                      .index = args->index);
}

EMBED_INLINE unsigned char family_table_byte(const InspectTreeFamilyCtx *args)
{
    return EMBED_CALL(family_byte, InspectTreeFamilyCtx, .cell = s_family.tables_cell, .table_at = args->table_at,
                      .index = args->index);
}

EMBED_INLINE unsigned int family_ancestor_word(const InspectTreeFamilyCtx *args)
{
    return EMBED_CALL(family_word, InspectTreeFamilyCtx, .cell = s_family.ancestors_cell, .table_at = args->table_at,
                      .index = args->index);
}

// whether a base `size` bytes long is one that adds no attribute
EMBED_INLINE int family_bare(const InspectTreeFamilyCtx *args)
{
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_bare_bases[at], .cap = CLI_POOL_BYTES}) == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_bare_bases[at], .other = args->base, .bytes = (embed_word)args->size}) ==
             0))
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
    *at += (args->bytes + 7ULL) & ~7ULL;
    return part;
}

// the classes and the bases they name counted, and the cell of their lists taken: 1, or 0 where it cannot be
static int family_class_cell(void)
{
    unsigned int classes = 0u;
    unsigned long long links = 0ULL;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        CliTreeFile tree_file;
        CLI_TREE_FILE_READ(file, &tree_file);
        for (unsigned int at = 0u; tree_file.used && (at < tree_file.facts.classes); at += 1u)
        {
            CliInspectClass one;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = tree_file.facts.cell,
                                                                    .offset = CLI_FACTS_CLASS_AT(&tree_file.facts, at),
                                                                    .dst = &one,
                                                                    .bytes = sizeof(one)});
            classes += 1u;
            links += tree_file.python ? one.bases : 0u;
        }
    }
    const unsigned long long words = (11ULL * classes) + g_cli_tree_files + links + 16ULL;
    const unsigned long long bytes = (words * sizeof(unsigned int)) + (4ULL * (classes + 8ULL));
    unsigned char *at = g_cli_guard->temporary_buf_alloc(bytes, g_cli_inspect_owner);
    s_family.laid = at;
    s_family.laid_bytes = bytes;
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
        CliTreeFile tree_file;
        CLI_TREE_FILE_READ(file, &tree_file);
        for (unsigned int at = 0u; tree_file.used && (at < tree_file.facts.classes); at += 1u)
        {
            s_family.class_file[classes] = file;
            s_family.class_at[classes] = at;
            s_family.python[classes] = (unsigned char)tree_file.python;
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
        CliTreeFile tree_file;
        CliInspectClass one;
        EMBED_CALL(family_class, InspectTreeFamilyCtx, .number = number, .tree_file = &tree_file, .class_of = &one);
        for (unsigned int at = 0u; at < one.bases; at += 1u)
        {
            const CliInspectName base =
                EMBED_CALL(family_name, InspectTreeFamilyCtx, .facts = &tree_file.facts, .index = one.base_first + at);
            char text[CLI_TREE_FAMILY_NAME_BYTES];
            if (!EMBED_CALL(family_text, InspectTreeFamilyCtx, .facts = &tree_file.facts, .text = base.name,
                            .out = text, .out_bytes = sizeof(text)))
            {
                s_family.all[number] = 0u;
                continue;
            }
            if (EMBED_CALL(family_bare, InspectTreeFamilyCtx, .base = text, .size = base.name.size))
            {
                continue;
            }
            unsigned int found_file = 0u;
            unsigned int found_class = 0u;
            const unsigned int found =
                EMBED_CALL(cli_inspect_tree_size_internal.tree_base_get, CliInspectTreeSizeCtx, .file = file,
                           .base = text, .size = base.name.size, .depth = 0u, .found_file = &found_file,
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
        CliTreeFile tree_file;
        CliInspectClass one;
        EMBED_CALL(family_class, InspectTreeFamilyCtx, .number = s_family.ancestor[args->used + at],
                   .tree_file = &tree_file, .class_of = &one);
        s_family.dynamic[args->key] |= (unsigned char)(one.dynamic != 0);
    }
    return args->used + count;
}

// the lists as they are laid written into two movable cells of the same layout, each list's place in them kept, the
// cells of the working given back and the pointers into them dropped: 1, or 0 where the cells cannot meet them
static int family_stored(void)
{
    const unsigned char *const laid = s_family.laid;
    const unsigned char *const laid_ancestors = s_family.laid_ancestors;
    s_family.tables_cell = g_cli_guard->movable_buf_alloc(s_family.laid_bytes, g_cli_inspect_owner);
    s_family.ancestors_cell = g_cli_guard->movable_buf_alloc(s_family.laid_ancestors_bytes, g_cli_inspect_owner);
    if ((s_family.tables_cell == NULL) || (s_family.ancestors_cell == NULL))
    {
        return 0;
    }
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_family.tables_cell, .offset = 0ULL, .src = laid, .bytes = s_family.laid_bytes});
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_family.ancestors_cell,
                                                             .offset = 0ULL,
                                                             .src = laid_ancestors,
                                                             .bytes = s_family.laid_ancestors_bytes});
    // each list's place is where its pointer stands past its cell's start
    s_family.class_file_at = (unsigned long long)((const unsigned char *)s_family.class_file - laid);
    s_family.class_at_at = (unsigned long long)((const unsigned char *)s_family.class_at - laid);
    s_family.python_at = (unsigned long long)(s_family.python - laid);
    s_family.ancestor_first_at = (unsigned long long)((const unsigned char *)s_family.ancestor_first - laid);
    s_family.ancestors_at = (unsigned long long)((const unsigned char *)s_family.ancestors - laid);
    s_family.known_at = (unsigned long long)(s_family.known - laid);
    s_family.dynamic_at = (unsigned long long)(s_family.dynamic - laid);
    s_family.descendant_first_at = (unsigned long long)((const unsigned char *)s_family.descendant_first - laid);
    s_family.descendants_at = (unsigned long long)((const unsigned char *)s_family.descendants - laid);
    s_family.ancestor_at = (unsigned long long)((const unsigned char *)s_family.ancestor - laid_ancestors);
    s_family.descendant_at = (unsigned long long)((const unsigned char *)s_family.descendant - laid_ancestors);
    g_cli_guard->persistent_buf_release(s_family.laid, g_cli_inspect_owner);
    g_cli_guard->persistent_buf_release(s_family.laid_ancestors, g_cli_inspect_owner);
    s_family.laid = NULL;
    s_family.laid_ancestors = NULL;
    s_family.class_first = NULL;
    s_family.class_file = NULL;
    s_family.class_at = NULL;
    s_family.base_first = NULL;
    s_family.bases = NULL;
    s_family.base = NULL;
    s_family.all = NULL;
    s_family.python = NULL;
    s_family.ancestor_first = NULL;
    s_family.ancestors = NULL;
    s_family.ancestor = NULL;
    s_family.known = NULL;
    s_family.dynamic = NULL;
    s_family.descendant_first = NULL;
    s_family.descendants = NULL;
    s_family.descendant = NULL;
    s_family.seen = NULL;
    s_family.waiting = NULL;
    return 1;
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
    unsigned long long total = 0ULL;
    for (unsigned int key = 0u; key < s_family.classes; key += 1u)
    {
        total = EMBED_CALL(family_ancestors_get, InspectTreeFamilyCtx, .key = key, .used = 0u, .store = 0) + total;
    }
    const unsigned long long ancestors_bytes = (2ULL * total + 2ULL) * sizeof(unsigned int);
    unsigned int *const ancestors = g_cli_guard->temporary_buf_alloc(ancestors_bytes, g_cli_inspect_owner);
    s_family.laid_ancestors = (unsigned char *)(void *)ancestors;
    s_family.laid_ancestors_bytes = ancestors_bytes;
    if (ancestors == NULL)
    {
        cli_tree_families_free();
        return;
    }
    s_family.ancestor = ancestors;
    s_family.descendant = ancestors + total + 1ULL;
    memor.set(&(MemoriaCfg){.dst = s_family.seen, .val = 0, .bytes = s_family.classes * sizeof(s_family.seen[0])});
    s_family.round = 0u;
    unsigned int used = 0u;
    for (unsigned int key = 0u; key < s_family.classes; key += 1u)
    {
        used = EMBED_CALL(family_ancestors_get, InspectTreeFamilyCtx, .key = key, .used = used, .store = 1);
    }
    family_descendants();
    if (!family_stored())
    {
        cli_tree_families_free();
        return;
    }
    s_family.current = 1;
}

// whether the class numbered `number` sets `size` bytes of `name`
EMBED_INLINE int family_sets(const InspectTreeFamilyCtx *args)
{
    CliTreeFile tree_file;
    CliInspectClass one;
    EMBED_CALL(family_class, InspectTreeFamilyCtx, .number = args->number, .tree_file = &tree_file, .class_of = &one);
    for (unsigned int at = 0u; at < one.has; at += 1u)
    {
        const CliInspectName has =
            EMBED_CALL(family_name, InspectTreeFamilyCtx, .facts = &tree_file.facts, .index = one.has_first + at);
        if (EMBED_CALL(family_text_is, InspectTreeFamilyCtx, .facts = &tree_file.facts, .text = has.name,
                       .name = args->name, .size = args->size))
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
    if (!EMBED_CALL(family_table_byte, InspectTreeFamilyCtx, .table_at = s_family.known_at, .index = args->lower) ||
        EMBED_CALL(family_table_byte, InspectTreeFamilyCtx, .table_at = s_family.dynamic_at, .index = args->lower))
    {
        return -1;
    }
    const unsigned int ancestors =
        EMBED_CALL(family_table_word, InspectTreeFamilyCtx, .table_at = s_family.ancestors_at, .index = args->lower);
    const unsigned int first = EMBED_CALL(family_table_word, InspectTreeFamilyCtx,
                                          .table_at = s_family.ancestor_first_at, .index = args->lower);
    for (unsigned int at = 0u; at < ancestors; at += 1u)
    {
        if (EMBED_CALL(family_sets, InspectTreeFamilyCtx,
                       .number = EMBED_CALL(family_ancestor_word, InspectTreeFamilyCtx,
                                            .table_at = s_family.ancestor_at, .index = first + at),
                       .name = args->name, .size = args->size))
        {
            return 1;
        }
    }
    return 0;
}

// whether a read of the class `class_of` of the file `tree_file` at `read`, `size` bytes of `name`, names what is to
// be passed by: a name of Python's own, or one an earlier read named
EMBED_INLINE int family_pass(const InspectTreeFamilyCtx *args)
{
    const CliFactsCell *const facts = &args->tree_file->facts;
    if ((args->size >= 2u) && (memor.cmp(&(MemoriaCfg){.src = args->name, .other = "__", .bytes = 2u}) == 0) &&
        (memor.cmp(&(MemoriaCfg){.src = args->name + args->size - 2u, .other = "__", .bytes = 2u}) == 0))
    {
        return 1;
    }
    for (unsigned int earlier = 0u; earlier < args->read; earlier += 1u)
    {
        const CliInspectName other = EMBED_CALL(family_name, InspectTreeFamilyCtx, .facts = facts,
                                                .index = args->class_of->read_first + earlier);
        if (EMBED_CALL(family_text_is, InspectTreeFamilyCtx, .facts = facts, .text = other.name, .name = args->name,
                       .size = args->size))
        {
            return 1;
        }
    }
    return 0;
}

// the attributes the class numbered `key` reads of `self` that no class of its family sets, each handed to `each`
EMBED_INLINE void family_attributes_get(const InspectTreeFamilyCtx *args)
{
    CliTreeFile tree_file;
    CliInspectClass one;
    EMBED_CALL(family_class, InspectTreeFamilyCtx, .number = args->key, .tree_file = &tree_file, .class_of = &one);
    char class_name[CLI_TREE_FAMILY_NAME_BYTES];
    if (!EMBED_CALL(family_text, InspectTreeFamilyCtx, .facts = &tree_file.facts, .text = one.name, .out = class_name,
                    .out_bytes = sizeof(class_name)))
    {
        return;
    }
    for (unsigned int read = 0u; read < one.reads; read += 1u)
    {
        const CliInspectName name =
            EMBED_CALL(family_name, InspectTreeFamilyCtx, .facts = &tree_file.facts, .index = one.read_first + read);
        char text[CLI_TREE_FAMILY_NAME_BYTES];
        if (!EMBED_CALL(family_text, InspectTreeFamilyCtx, .facts = &tree_file.facts, .text = name.name, .out = text,
                        .out_bytes = sizeof(text)))
        {
            continue;
        }
        if (EMBED_CALL(family_pass, InspectTreeFamilyCtx, .tree_file = &tree_file, .class_of = &one, .read = read,
                       .name = text, .size = name.name.size))
        {
            continue;
        }
        int found = 0;
        const unsigned int descendants = EMBED_CALL(family_table_word, InspectTreeFamilyCtx,
                                                    .table_at = s_family.descendants_at, .index = args->key);
        const unsigned int first = EMBED_CALL(family_table_word, InspectTreeFamilyCtx,
                                              .table_at = s_family.descendant_first_at, .index = args->key);
        for (unsigned int at = 0u; (found == 0) && (at < descendants); at += 1u)
        {
            const int sets = EMBED_CALL(family_reads_set, InspectTreeFamilyCtx,
                                        .lower = EMBED_CALL(family_ancestor_word, InspectTreeFamilyCtx,
                                                            .table_at = s_family.descendant_at, .index = first + at),
                                        .name = text, .size = name.name.size);
            found = (sets != 0) ? sets : found;
        }
        if (found != 0)
        {
            continue;
        }
        char msg_buf[4096];
        VerbaLinea message = {msg_buf, sizeof(msg_buf), 0ULL};
        mmgr_line_ch(&message, '`');
        mmgr_line_put_n(&message, class_name, one.name.size);
        mmgr_line_put(&message, "` has no `");
        mmgr_line_put_n(&message, text, name.name.size);
        mmgr_line_put(&message, "`: neither it, a class it comes from, nor a class that comes from it sets `");
        mmgr_line_put_n(&message, text, name.name.size);
        mmgr_line_put(&message, "`.");
        if (mmgr_line_ok(&message))
        {
            EMBED_CALL(args->each, CliTreeFindingCfg, .context = args->context,
                       .file = EMBED_CALL(family_class_file, InspectTreeFamilyCtx, .number = args->key),
                       .from = name.from, .to = name.to, .code = "unknown-attribute", .message = &message);
        }
    }
}

void cli_tree_attributes(const CliInspectTreeCtx *args)
{
    cli_tree_families();
    for (unsigned int key = 0u; s_family.current && (key < s_family.classes); key += 1u)
    {
        if (EMBED_CALL(family_table_byte, InspectTreeFamilyCtx, .table_at = s_family.python_at, .index = key) &&
            EMBED_CALL(family_table_byte, InspectTreeFamilyCtx, .table_at = s_family.known_at, .index = key))
        {
            EMBED_CALL(family_attributes_get, InspectTreeFamilyCtx, .key = key, .each = args->each,
                       .context = args->context);
        }
    }
}

// the names the class numbered `key`'s family sets, counted with each name as often as a class of it sets it
EMBED_INLINE unsigned long long family_supplied_count(const InspectTreeFamilyCtx *args)
{
    unsigned long long count = 0ULL;
    const unsigned int descendants =
        EMBED_CALL(family_table_word, InspectTreeFamilyCtx, .table_at = s_family.descendants_at, .index = args->key);
    const unsigned int first = EMBED_CALL(family_table_word, InspectTreeFamilyCtx,
                                          .table_at = s_family.descendant_first_at, .index = args->key);
    for (unsigned int lower = 0u; lower < descendants; lower += 1u)
    {
        const unsigned int below = EMBED_CALL(family_ancestor_word, InspectTreeFamilyCtx,
                                              .table_at = s_family.descendant_at, .index = first + lower);
        const unsigned int ancestors =
            EMBED_CALL(family_table_word, InspectTreeFamilyCtx, .table_at = s_family.ancestors_at, .index = below);
        const unsigned int above =
            EMBED_CALL(family_table_word, InspectTreeFamilyCtx, .table_at = s_family.ancestor_first_at, .index = below);
        for (unsigned int up = 0u; up < ancestors; up += 1u)
        {
            CliTreeFile tree_file;
            CliInspectClass one;
            EMBED_CALL(family_class, InspectTreeFamilyCtx,
                       .number = EMBED_CALL(family_ancestor_word, InspectTreeFamilyCtx,
                                            .table_at = s_family.ancestor_at, .index = above + up),
                       .tree_file = &tree_file, .class_of = &one);
            count += one.has;
        }
    }
    return count;
}

// the text `text` of the facts cell `facts` put among the names of the round `round` in `table`, `buckets` of them:
// 1 where it was not among them
EMBED_INLINE int family_supplied_append(const InspectTreeFamilyCtx *args)
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
            (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){.cell = args->table[bucket].cell,
                                                                  .offset = args->table[bucket].at,
                                                                  .src = args->name,
                                                                  .bytes = args->size_u32}) == 0))
        {
            return 0;
        }
        bucket = (bucket + 1u) & (args->buckets - 1u);
    }
    args->table[bucket] = (CliTreeSupplied){args->round, args->size_u32, args->facts->cell,
                                            CLI_FACTS_TEXT_AT(args->facts, args->text.at)};
    return 1;
}

// what each class the class `number` comes from sets, each attribute first stored in `table` this `round` handed to
// `supplied` with the `path` and `class_name` of the class it is supplied to
EMBED_INLINE void family_supplied_above(const InspectTreeFamilyCtx *args)
{
    const unsigned int below = args->number;
    const unsigned int ancestors =
        EMBED_CALL(family_table_word, InspectTreeFamilyCtx, .table_at = s_family.ancestors_at, .index = below);
    const unsigned int above =
        EMBED_CALL(family_table_word, InspectTreeFamilyCtx, .table_at = s_family.ancestor_first_at, .index = below);
    for (unsigned int up = 0u; up < ancestors; up += 1u)
    {
        const unsigned int one = EMBED_CALL(family_ancestor_word, InspectTreeFamilyCtx,
                                            .table_at = s_family.ancestor_at, .index = above + up);
        CliTreeFile tree_file;
        CliInspectClass sets;
        EMBED_CALL(family_class, InspectTreeFamilyCtx, .number = one, .tree_file = &tree_file, .class_of = &sets);
        for (unsigned int at = 0u; at < sets.has; at += 1u)
        {
            const CliInspectName name =
                EMBED_CALL(family_name, InspectTreeFamilyCtx, .facts = &tree_file.facts, .index = sets.has_first + at);
            char text[CLI_TREE_FAMILY_NAME_BYTES];
            if (!EMBED_CALL(family_text, InspectTreeFamilyCtx, .facts = &tree_file.facts, .text = name.name,
                            .out = text, .out_bytes = sizeof(text)))
            {
                continue;
            }
            if (EMBED_CALL(family_supplied_append, InspectTreeFamilyCtx, .table = args->table, .buckets = args->buckets,
                           .round = args->round, .name = text, .size_u32 = name.name.size, .facts = &tree_file.facts,
                           .text = name.name))
            {
                EMBED_CALL(args->supplied, CliInspectSuppliedCfg, .context = args->context, .path = args->path,
                           .class_name = args->class_name, .attribute = text);
            }
        }
    }
}

void cli_inspect_tree_supplied(const CliInspectTreeContextEachSuppliedCfg *args)
{
    cli_tree_families();
    // the table's buckets twice the most names one family sets, rounded up to a power of two
    unsigned long long most = 0ULL;
    for (unsigned int key = 0u; s_family.current && (key < s_family.classes); key += 1u)
    {
        const unsigned long long count =
            EMBED_CALL(family_table_byte, InspectTreeFamilyCtx, .table_at = s_family.python_at, .index = key)
                ? EMBED_CALL(family_supplied_count, InspectTreeFamilyCtx, .key = key)
                : 0ULL;
        most = (count > most) ? count : most;
    }
    unsigned int buckets = 16u;
    while ((buckets < (2ULL * most)) && (buckets < 0x80000000u))
    {
        buckets <<= 1u;
    }
    CliTreeSupplied *const table =
        s_family.current ? g_cli_guard->temporary_buf_alloc(buckets * sizeof(CliTreeSupplied), g_cli_inspect_owner)
                         : NULL;
    if (table != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = table, .val = 0, .bytes = buckets * sizeof(CliTreeSupplied)});
    }
    unsigned int round = 0u;
    for (unsigned int key = 0u; (table != NULL) && (key < s_family.classes); key += 1u)
    {
        if (!EMBED_CALL(family_table_byte, InspectTreeFamilyCtx, .table_at = s_family.python_at, .index = key))
        {
            continue;
        }
        round += 1u;
        CliTreeFile tree_file;
        CliInspectClass one;
        EMBED_CALL(family_class, InspectTreeFamilyCtx, .number = key, .tree_file = &tree_file, .class_of = &one);
        char path[CLI_TREE_PATH_BYTES];
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = tree_file.facts.cell,
                                                                .offset = CLI_TREE_PATH_AT,
                                                                .dst = path,
                                                                .bytes = tree_file.path_size + 1ULL});
        char class_name[CLI_TREE_FAMILY_NAME_BYTES];
        if (!EMBED_CALL(family_text, InspectTreeFamilyCtx, .facts = &tree_file.facts, .text = one.name,
                        .out = class_name, .out_bytes = sizeof(class_name)))
        {
            continue;
        }
        const unsigned int descendants =
            EMBED_CALL(family_table_word, InspectTreeFamilyCtx, .table_at = s_family.descendants_at, .index = key);
        const unsigned int first =
            EMBED_CALL(family_table_word, InspectTreeFamilyCtx, .table_at = s_family.descendant_first_at, .index = key);
        for (unsigned int lower = 0u; lower < descendants; lower += 1u)
        {
            EMBED_CALL(family_supplied_above, InspectTreeFamilyCtx,
                       .number = EMBED_CALL(family_ancestor_word, InspectTreeFamilyCtx,
                                            .table_at = s_family.descendant_at, .index = first + lower),
                       .table = table, .buckets = buckets, .round = round, .supplied = args->each_inspect_supplied,
                       .context = args->context, .path = path, .class_name = class_name);
        }
    }
    g_cli_guard->persistent_buf_release(table, g_cli_inspect_owner);
}
