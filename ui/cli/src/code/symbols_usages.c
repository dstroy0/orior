// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_usages.c: the usages of the name at a place in a file of the tree, read off the leaves of the tree's
// branches. A local's or a parameter's are the leaves of its name in the region of the function that declares it; any
// other name's are the leaves of it, in every file the tree reads, whose definition in effect is the one in effect at
// the place: the define in effect, else the first declaration of the name the file's walk met, so a function a header
// declares and a file defines is one function, and a static of the same name in another file is another. Each file
// holding the name is walked once, its leaves of the name noted as the walk passes them, and one that a local of the
// function it is in shadows set aside. A name, a local, a parameter, a region's brackets and a statement's end
// are known by the roles their grammar's tokens name; a function's region by its head declaring a function
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/text.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// One leaf of the name a walk noted: its line, its span in UTF-16 units, its line's text, a place among the texts, and
// how many bytes; the definition in effect there, its path a place among the paths, CLI_JSON_NONE for none, and its
// line; whether the function region it is in declares the name a local or a parameter; and that region's first
// leaf, CLI_JSON_NONE where it is in none
typedef struct
{
    unsigned int line;
    unsigned int from;
    unsigned int to;
    unsigned int region;
    unsigned long long text;
    unsigned long long length;
    unsigned int defined;
    unsigned int defined_line;
    int local;
} CliUsageNoted;
// The usages being read: the name and its length; each walk's notes, their lines' texts and the definitions' paths, and
// the regions of the branch noted last, each a line; that branch's leaves; and whether the cells refused a note
typedef struct
{
    const char *name;
    unsigned long long length;
    CliGrownLine notes;
    CliGrownLine texts;
    CliGrownLine paths;
    CliGrownLine regions;
    const CliLeafRead *read;
    unsigned int noted;
    unsigned int region_count;
    int refused;
} CliUsages;
// the bytes the notes, their texts, the definitions' paths, the regions and the tree's paths are each written in
#define CLI_USAGES_CAPACITY (16ULL << 20u)

// One top-level region of a branch's leaves: the first leaf of the statement its opening bracket closes over, the
// bracket, the leaf after the bracket that closes it, whether its head declares a function, and whether it declares
// the name a local or a parameter, -1 until it is read
typedef struct
{
    unsigned int head;
    unsigned int open;
    unsigned int end;
    int function;
    int declares;
} CliUsageRegion;

typedef struct
{
    const CliUsages *usages;
    unsigned int at;
    CliUsages *usages_out;
    const CliLeafRead *read;
    unsigned int leaf;
    const char *path;
    const char *text;
    unsigned long long size;
    VerbaLinea *err_msg;
    const CliUsageNoted *one;
    const char *defined;
    unsigned int line;
    unsigned int region;
    CliUsage each;
    void *context;
    CliOpenText open_text;
    void *open_text_context;
    unsigned long long *size_pointer;
    int *mapped;
    char *text_out;
    int mapped_value;
    unsigned int col;
    char *name;
    CliGrownLine *paths;
    int across;
    int definition;
} SymbolsUsagesCtx;
// the note at `at` of those read
EMBED_INLINE CliUsageNoted usages_note_at(const SymbolsUsagesCtx *args)
{
    CliUsageNoted one;
    memor.cpy(&(MemoriaCfg){
        .dst = &one, .src = args->usages->notes.cell + ((embed_word)args->at * sizeof(one)), .bytes = sizeof(one)});
    return one;
}

// the region at `at` of the branch noted last
EMBED_INLINE CliUsageRegion *usages_region_at(const SymbolsUsagesCtx *args)
{
    // the regions were written one after another into a cell taken for them, on its own boundary
    return (CliUsageRegion *)(void *)(args->usages->regions.cell + ((embed_word)args->at * sizeof(CliUsageRegion)));
}

// the top-level regions of `read`, each a bracket its grammar names a region's opening, at no depth, to the one that
// closes it, its head the statement before it: after the last statement's end or region at no depth, or the last
// directive's line; a function's where its head declares a function
EMBED_INLINE void usages_regions_read(const SymbolsUsagesCtx *args)
{
    const CliLeafRead *read = args->read;
    cli_grown_reset(&args->usages_out->regions);
    args->usages_out->region_count = 0u;
    args->usages_out->read = read;
    unsigned int depth = 0u;
    unsigned int statement = 0u;
    unsigned int open = 0u;
    unsigned int directive_line = CLI_SLICE_NOWHERE;
    for (unsigned int leaf = 0u; leaf < read->count; leaf += 1u)
    {
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_REGION | CLI_LEAF_OPEN))
        {
            open = (depth == 0u) ? leaf : open;
            depth += 1u;
            continue;
        }
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_REGION | CLI_LEAF_CLOSE) &&
            (depth > 0u))
        {
            depth -= 1u;
            if (depth != 0u)
            {
                continue;
            }
            int function = 0;
            unsigned int head = statement;
            while (!function && (head < open))
            {
                function = EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                      .leaf = head, .roles = CLI_LEAF_DECLARES | CLI_LEAF_FUNCTION);
                head += 1u;
            }
            const CliUsageRegion region = {statement, open, leaf + 1u, function, -1};
            mmgr_line_put_n(&args->usages_out->regions.line, (const char *)&region, sizeof(region));
            args->usages_out->region_count += 1u;
            statement = leaf + 1u;
            continue;
        }
        directive_line = ((read->leaf[leaf].is & CLI_LEAF_DIRECTIVE) != 0u) ? read->leaf[leaf].line : directive_line;
        if ((depth == 0u) && ((read->leaf[leaf].line == directive_line) ||
                              EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                         .leaf = leaf, .roles = CLI_LEAF_STATEMENT)))
        {
            statement = leaf + 1u;
        }
    }
    args->usages_out->refused |= !mmgr_line_ok(&args->usages_out->regions.line);
}

// the place among the regions of the one the leaf `leaf` is in, CLI_JSON_NONE where it is in none
EMBED_INLINE unsigned int usages_region_get(const SymbolsUsagesCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = args->usages->region_count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (EMBED_CALL(usages_region_at, SymbolsUsagesCtx, .usages = args->usages, .at = middle)->end <= args->leaf)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return ((low < args->usages->region_count) &&
            (EMBED_CALL(usages_region_at, SymbolsUsagesCtx, .usages = args->usages, .at = low)->head <= args->leaf))
               ? low
               : CLI_JSON_NONE;
}

// whether the function region at `at` declares the name a local or a parameter, read once
EMBED_INLINE int usages_region_declares(const SymbolsUsagesCtx *args)
{
    const CliLeafRead *read = args->read;
    CliUsageRegion *const region =
        EMBED_CALL(usages_region_at, SymbolsUsagesCtx, .usages = args->usages_out, .at = args->at);
    if (region->declares < 0)
    {
        region->declares = 0;
        for (unsigned int leaf = region->head; region->function && !region->declares && (leaf < region->end);
             leaf += 1u)
        {
            unsigned long long length = 0ULL;
            const char *const text = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                                .read_read = read, .leaf = leaf, .length_pointer = &length);
            region->declares = ((read->leaf[leaf].is & (CLI_LEAF_LOCAL | CLI_LEAF_PARAMETER)) != 0u) &&
                               ((read->leaf[leaf].is & CLI_LEAF_NAME) != 0u) && (length == args->usages_out->length) &&
                               EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = text,
                                          .other_pointer = args->usages_out->name, .size = length);
        }
    }
    return region->declares;
}

// a leaf of the name a walk passes noted with the definition in effect there, its line's text and its span, and
// whether a local of the function region it is in shadows it
static void usages_note(const CliEffectNotedCfg *args)
{
    const CliLeafRead *read = args->read;
    unsigned int leaf = args->leaf;
    CliUsages *const usages = (CliUsages *)args->context;
    if (usages->read != read)
    {
        EMBED_CALL(usages_regions_read, SymbolsUsagesCtx, .usages_out = usages, .read = read);
    }
    const unsigned int region = EMBED_CALL(usages_region_get, SymbolsUsagesCtx, .usages = usages, .leaf = leaf);
    const int local = (region != CLI_JSON_NONE) && EMBED_CALL(usages_region_declares, SymbolsUsagesCtx,
                                                              .usages_out = usages, .read = read, .at = region);
    // the line runs from its first leaf to its last, its "\r" left off
    const unsigned long long from = read->leaf[leaf].from;
    unsigned int first = leaf;
    while ((first > 0u) && (read->leaf[first - 1u].line == read->leaf[leaf].line))
    {
        first -= 1u;
    }
    unsigned int last = leaf;
    while (((last + 1u) < read->count) && (read->leaf[last + 1u].line == read->leaf[leaf].line))
    {
        last += 1u;
    }
    const unsigned long long start = read->leaf[first].from;
    unsigned long long end = read->leaf[last].to;
    end -= ((end > start) && (read->text[end - 1ULL] == '\r')) ? 1ULL : 0ULL;
    // a line's units are below 2^32
    const unsigned int units = (unsigned int)EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg,
                                                        .text = read->text + start, .size = from - start);
    const unsigned int width = (unsigned int)EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg,
                                                        .text = read->text + from, .size = read->leaf[leaf].to - from);
    const CliUsageNoted one = {
        read->leaf[leaf].line,
        units,
        units + width,
        (region != CLI_JSON_NONE) ? EMBED_CALL(usages_region_at, SymbolsUsagesCtx, .usages = usages, .at = region)->head
                                  : CLI_JSON_NONE,
        cli_grown_size(&usages->texts),
        end - start,
        (args->path != NULL) ? (unsigned int)cli_grown_size(&usages->paths) : CLI_JSON_NONE,
        args->line,
        local};
    mmgr_line_put_n(&usages->texts.line, read->text + start, (embed_word)(end - start));
    if (args->path != NULL)
    {
        mmgr_line_put(&usages->paths.line, args->path);
        mmgr_line_ch(&usages->paths.line, '\0');
    }
    mmgr_line_put_n(&usages->notes.line, (const char *)&one, sizeof(one));
    usages->noted += 1u;
    usages->refused |=
        !mmgr_line_ok(&usages->notes.line) || !mmgr_line_ok(&usages->texts.line) || !mmgr_line_ok(&usages->paths.line);
}

// the branch at `path`, `size` bytes of `text`, walked to its end, each of its leaves of the name noted in place of
// those noted before: 1, or 0 with why given where the cells cannot meet the walk
EMBED_INLINE int usages_walk(const SymbolsUsagesCtx *args)
{
    const char *path = args->path;
    VerbaLinea *err_msg = args->err_msg;
    cli_grown_reset(&args->usages_out->notes);
    cli_grown_reset(&args->usages_out->texts);
    cli_grown_reset(&args->usages_out->paths);
    args->usages_out->noted = 0u;
    args->usages_out->read = NULL;
    CliEffect *const effect =
        EMBED_CALL(cli_symbols_path_name_internal.effect_init, CliSymbolsPathNameCtx, .path = path,
                   .name = args->usages_out->name, .slicing = 0, .err_msg = err_msg);
    if (effect == NULL)
    {
        return 0;
    }
    effect->start_text = args->text;
    effect->start_size = args->size;
    effect->noted = usages_note;
    effect->noted_context = args->usages_out;
    EMBED_CALL(cli_symbols_path_effect_internal.effect_alloc, CliSymbolsPathEffectCtx, .effect = effect, .path = path,
               .line_u32 = CLI_EFFECT_INPUT);
    const int refused = effect->refused || args->usages_out->refused;
    EMBED_CALL(cli_symbols_text_length_internal.effect_input_back, CliSymbolsTextLengthCtx, .effect = effect);
    if (refused)
    {
        mmgr_line_put(err_msg, "the cells cannot meet the usages' walk");
    }
    return !refused;
}

// whether the note `one` has the definition `defined`, a path, NULL for none, at `line`
EMBED_INLINE int usages_same(const SymbolsUsagesCtx *args)
{
    if ((args->one->defined == CLI_JSON_NONE) || (args->defined == NULL))
    {
        return 0;
    }
    return (args->one->defined_line == args->line) &&
           cellul.eq(&(CatenaFinitaCfg){
               .src = args->usages->paths.cell + args->one->defined, .other = args->defined, .cap = CLI_POOL_BYTES});
}

// each note of the branch at `path` passed on as a usage where it is in the region `region` of a local, else where
// a local does not shadow it and its definition is `defined` at `line`
EMBED_INLINE void usages_dispatch(const SymbolsUsagesCtx *args)
{
    for (unsigned int at = 0u; at < args->usages->noted; at += 1u)
    {
        const CliUsageNoted one = EMBED_CALL(usages_note_at, SymbolsUsagesCtx, .usages = args->usages, .at = at);
        const int used = (args->region != CLI_JSON_NONE)
                             ? (one.local && (one.region == args->region))
                             : (!one.local && EMBED_CALL(usages_same, SymbolsUsagesCtx, .usages = args->usages,
                                                         .one = &one, .defined = args->defined, .line = args->line));
        if (used)
        {
            EMBED_CALL(args->each, CliUsageCfg, .context = args->context, .path = args->path, .line = one.line,
                       .from = one.from, .to = one.to, .text = args->usages->texts.cell + one.text,
                       .length = one.length);
        }
    }
}

// the first note of the branch at `path` passed on as the definition: the first in the region `region` of a local, else
// the first on the line `line` whose definition is `defined` at `line`; whether one was
EMBED_INLINE int usages_first_dispatch(const SymbolsUsagesCtx *args)
{
    for (unsigned int at = 0u; at < args->usages->noted; at += 1u)
    {
        const CliUsageNoted one = EMBED_CALL(usages_note_at, SymbolsUsagesCtx, .usages = args->usages, .at = at);
        const int used = (args->region != CLI_JSON_NONE)
                             ? (one.local && (one.region == args->region))
                             : (!one.local && (one.line == args->line) &&
                                EMBED_CALL(usages_same, SymbolsUsagesCtx, .usages = args->usages, .one = &one,
                                           .defined = args->defined, .line = args->line));
        if (used)
        {
            EMBED_CALL(args->each, CliUsageCfg, .context = args->context, .path = args->path, .line = one.line,
                       .from = one.from, .to = one.to, .text = args->usages->texts.cell + one.text,
                       .length = one.length);
            return 1;
        }
    }
    return 0;
}

// the text of the tree's file at `path`, `open_text`'s, else the disk's mapped, its size into `size`, `*mapped` 1 where
// it is the disk's: NULL where neither has it
EMBED_INLINE char *usages_text(const SymbolsUsagesCtx *args)
{
    const char *path = args->path;
    unsigned long long *size = args->size_pointer;
    int *mapped = args->mapped;
    char *const supplied = (args->open_text != NULL)
                               ? EMBED_CALL(args->open_text, CliOpenTextCfg, .context = args->open_text_context,
                                            .path = path, .size = size)
                               : NULL;
    *mapped = supplied == NULL;
    if (supplied != NULL)
    {
        return supplied;
    }
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = g_cli_tree_root, .relative = path, .joined = full);
    // a text read mapped is read and never written
    return (char *)EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = full, .size_pointer = size,
                              .owner = g_cli_code_owner);
}

// the name the leaf at `line`, `col` of `size` bytes of `text` names, read in the language of the file at `path`, into
// `name`, sized for CLI_NAME_MAX_LEN: its length, 0 where the place names no name
EMBED_INLINE unsigned long long usages_name_at(const SymbolsUsagesCtx *args)
{
    const char *path = args->path;
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int line = args->line;
    unsigned int col = args->col;
    CliSymbolsLanguages *const languages = cli_symbols_languages_for_call();
    CliLeafRead read;
    const int leaved = (languages != NULL) &&
                       EMBED_CALL(cli_symbols_path_internal.leaves_save, CliSymbolsPathCtx, .languages = languages,
                                  .path = path, .state = "root", .text = text, .size = size, .read = &read);
    unsigned long long length = 0ULL;
    unsigned int leaf = 0u;
    while (leaved && (leaf < read.count) && (read.leaf[leaf].line < line))
    {
        leaf += 1u;
    }
    if (leaved && (leaf < read.count) && (read.leaf[leaf].line == line))
    {
        const unsigned long long start = read.leaf[leaf].from;
        unsigned long long end = start;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = end,
                         .size = size, .set = "\n");
        unsigned long long byte = 0ULL;
        unsigned int units = 0u;
        const unsigned long long at =
            start + EMBED_CALL(cli_symbols_size_internal.byte_at, CliSymbolsSizeCtx, .line = text + start,
                               .size = end - start, .col = col, .byte = &byte, .at = &units);
        // the leaf the place is in, else the one it ends where that one is a name
        while ((leaf < read.count) && (read.leaf[leaf].line == line) && (read.leaf[leaf].to <= at))
        {
            leaf += 1u;
        }
        const unsigned long long names = CLI_LEAF_NAME;
        const unsigned long long words = CLI_LEAF_COMMENT | CLI_LEAF_STRING;
        const int in = (leaf < read.count) && (read.leaf[leaf].line == line) && (read.leaf[leaf].from <= at) &&
                       ((read.leaf[leaf].is & names) != 0u) && ((read.leaf[leaf].is & words) == 0u);
        const int after = !in && (leaf > 0u) && (read.leaf[leaf - 1u].line == line) &&
                          (read.leaf[leaf - 1u].to == at) && ((read.leaf[leaf - 1u].is & names) != 0u) &&
                          ((read.leaf[leaf - 1u].is & words) == 0u);
        leaf -= after ? 1u : 0u;
        const int named = in || after;
        const char *const word_text = named
                                          ? EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                                       .read_read = &read, .leaf = leaf, .length_pointer = &length)
                                          : "";
        length = (length < CLI_NAME_MAX_LEN) ? length : 0ULL;
        // a name below CLI_NAME_MAX_LEN bytes
        memor.cpy(&(MemoriaCfg){.dst = args->name, .src = word_text, .bytes = (embed_word)length});
    }
    args->name[length] = '\0';
    if (leaved)
    {
        g_cli_guard->persistent_buf_release(read.leaf, g_cli_leaves_owner);
        read.leaf = NULL;
        read.count = 0u;
        read.capacity = 0u;
    }
    if (languages != NULL)
    {
        g_cli_guard->persistent_buf_release(languages->languages.pair, g_cli_plugins_owner);
        *(&languages->languages) = (CliLanguages){NULL, 0u, 0u};
    }
    g_cli_guard->persistent_buf_release(languages, g_cli_code_owner);
    return length;
}

// the paths of the tree's files the tree reads, but `path` and the mounts', each with a 0 after, into `paths`
EMBED_INLINE void usages_tree_paths(const SymbolsUsagesCtx *args)
{
    const char *path = args->path;
    for (unsigned int at = 0u; at < g_cli_branch_count; at += 1u)
    {
        char one[CLI_PATH_MAX_LEN];
        CliTreeBranch branch;
        EMBED_CALL(cli_symbols_tree_at.branch_read, CliSymbolsTreeAtCfg, .file = at, .branch_out = &branch);
        if (branch.used)
        {
            EMBED_CALL(cli_symbols_tree_branch_path.branch_path_read, CliSymbolsTreeBranchPathCfg, .branch = &branch,
                       .path_out = one);
        }
        if (branch.used && (one[0] != '@') &&
            !cellul.eq(&(CatenaFinitaCfg){.src = one, .other = path, .cap = CLI_POOL_BYTES}))
        {
            mmgr_line_put(&args->paths->line, one);
            mmgr_line_ch(&args->paths->line, '\0');
        }
    }
}

// the usages read across the tree's files but `path`, each holding the name walked, the notes whose definition is
// `defined` at `line` passed on: 1, or 0 with why given
EMBED_INLINE int usages_across(const SymbolsUsagesCtx *args)
{
    CliGrownLine paths;
    cli_grown_begin(&paths, CLI_USAGES_CAPACITY, g_cli_code_owner);
    if (paths.cell == NULL)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the usages' walk");
        return 0;
    }
    EMBED_CALL(usages_tree_paths, SymbolsUsagesCtx, .path = args->path, .paths = &paths);
    int walked = mmgr_line_ok(&paths.line);
    for (unsigned long long at = 0ULL; walked && (at < cli_grown_size(&paths));
         at += cellul.len(&(CatenaFinitaCfg){.src = paths.cell + at, .cap = CLI_POOL_BYTES}) + 1ULL)
    {
        const char *const one = paths.cell + at;
        unsigned long long size = 0ULL;
        int mapped = 0;
        char *const text =
            EMBED_CALL(usages_text, SymbolsUsagesCtx, .path = one, .open_text = args->open_text,
                       .open_text_context = args->open_text_context, .size_pointer = &size, .mapped = &mapped);
        unsigned long long found = 0ULL;
        // a file whose bytes hold the name nowhere holds no leaf of it, and is not walked
        if ((text != NULL) &&
            EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text, .length_value = size,
                       .required = args->usages_out->name, .size = args->usages_out->length, .at_pointer = &found))
        {
            walked = EMBED_CALL(usages_walk, SymbolsUsagesCtx, .usages_out = args->usages_out, .path = one,
                                .text = text, .size = size, .err_msg = args->err_msg);
            EMBED_CALL(usages_dispatch, SymbolsUsagesCtx, .usages = args->usages_out, .path = one,
                       .region = CLI_JSON_NONE, .defined = args->defined, .line = args->line, .each = args->each,
                       .context = args->context);
        }
        if (mapped && (text != NULL))
        {
            g_cli_guard->persistent_buf_release((void *)(embed_word)((const unsigned char *)text), (g_cli_code_owner));
        }
        else
        {
            g_cli_guard->persistent_buf_release(text, g_cli_code_owner);
        }
    }
    cli_grown_end(&paths);
    return walked;
}

// the leaf of the name on the line `line` of the tree's file `defined`, the definition in effect at the place asked
// about in the file at `path`, passed on, that file walked where it is not `path`: 1, 0 where its walk notes none
// there, as for a define the compile database gives, or -1 with why given
EMBED_INLINE int usages_definition_dispatch(const SymbolsUsagesCtx *args)
{
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->path, .other = args->defined, .cap = CLI_POOL_BYTES}))
    {
        unsigned long long size = 0ULL;
        int mapped = 0;
        char *const text =
            EMBED_CALL(usages_text, SymbolsUsagesCtx, .path = args->defined, .open_text = args->open_text,
                       .open_text_context = args->open_text_context, .size_pointer = &size, .mapped = &mapped);
        if (text == NULL)
        {
            return 0;
        }
        const int walked = EMBED_CALL(usages_walk, SymbolsUsagesCtx, .usages_out = args->usages_out,
                                      .path = args->defined, .text = text, .size = size, .err_msg = args->err_msg);
        if (mapped && (text != NULL))
        {
            g_cli_guard->persistent_buf_release((void *)(embed_word)((const unsigned char *)text), (g_cli_code_owner));
        }
        else
        {
            g_cli_guard->persistent_buf_release(text, g_cli_code_owner);
        }
        if (!walked)
        {
            return -1;
        }
    }
    return EMBED_CALL(usages_first_dispatch, SymbolsUsagesCtx, .usages = args->usages_out, .path = args->defined,
                      .region = CLI_JSON_NONE, .defined = args->defined, .line = args->line, .each = args->each,
                      .context = args->context);
}

// the usages read in the file at `path`, its `size` bytes of `text`, of the name `name`, and across the tree where
// `across` is 1, or the definition in effect alone where `definition` is 1: 1, 0 where the place's leaf is no note or
// its name has no definition, or -1 with why given
EMBED_INLINE int usages_read(const SymbolsUsagesCtx *args)
{
    if (!usages_walk(args))
    {
        return -1;
    }
    unsigned int place = 0u;
    CliUsageNoted at = {0u, 0u, 0u, CLI_JSON_NONE, 0ULL, 0ULL, CLI_JSON_NONE, 0u, 0};
    while (place < args->usages_out->noted)
    {
        at = EMBED_CALL(usages_note_at, SymbolsUsagesCtx, .usages = args->usages_out, .at = place);
        if ((at.line == args->line) && (at.from <= args->col) && (args->col <= at.to))
        {
            break;
        }
        place += 1u;
    }
    if (place == args->usages_out->noted)
    {
        return 0;
    }
    if (at.local && args->definition)
    {
        return EMBED_CALL(usages_first_dispatch, SymbolsUsagesCtx, .usages = args->usages_out, .path = args->path,
                          .region = at.region, .defined = NULL, .line = 0u, .each = args->each,
                          .context = args->context);
    }
    if (at.local)
    {
        EMBED_CALL(usages_dispatch, SymbolsUsagesCtx, .usages = args->usages_out, .path = args->path,
                   .region = at.region, .defined = NULL, .line = 0u, .each = args->each, .context = args->context);
        return 1;
    }
    if (at.defined == CLI_JSON_NONE)
    {
        return 0;
    }
    // the definition is copied out of the notes, which the walks across the tree write over
    char defined[CLI_PATH_MAX_LEN];
    VerbaLinea defined_line = {defined, sizeof(defined), 0ULL};
    mmgr_line_put(&defined_line, args->usages_out->paths.cell + at.defined);
    (void)mmgr_line_finish(&defined_line);
    if (args->definition)
    {
        return EMBED_CALL(usages_definition_dispatch, SymbolsUsagesCtx, .usages_out = args->usages_out,
                          .path = args->path, .defined = defined, .line = at.defined_line, .open_text = args->open_text,
                          .open_text_context = args->open_text_context, .each = args->each, .context = args->context,
                          .err_msg = args->err_msg);
    }
    EMBED_CALL(usages_dispatch, SymbolsUsagesCtx, .usages = args->usages_out, .path = args->path,
               .region = CLI_JSON_NONE, .defined = defined, .line = at.defined_line, .each = args->each,
               .context = args->context);
    if (args->across && !EMBED_CALL(usages_across, SymbolsUsagesCtx, .usages_out = args->usages_out, .path = args->path,
                                    .defined = defined, .line = at.defined_line, .open_text = args->open_text,
                                    .open_text_context = args->open_text_context, .each = args->each,
                                    .context = args->context, .err_msg = args->err_msg))
    {
        return -1;
    }
    return 1;
}

// the usages of the name at `line`, `col` of the tree's file at `path`, or its definition in effect alone where
// `definition` is 1, as cli_symbols_usages and cli_symbols_definition answer
static int usages_request(const SymbolsUsagesCtx *args)
{
    unsigned long long size = 0ULL;
    int mapped = 0;
    char *const text =
        EMBED_CALL(usages_text, SymbolsUsagesCtx, .path = args->path, .open_text = args->open_text,
                   .open_text_context = args->open_text_context, .size_pointer = &size, .mapped = &mapped);
    char name[CLI_NAME_MAX_LEN];
    const unsigned long long length =
        (text != NULL) ? EMBED_CALL(usages_name_at, SymbolsUsagesCtx, .path = args->path, .text = text, .size = size,
                                    .line = args->line, .col = args->col, .name = name)
                       : 0ULL;
    // the four lines begun where a name is at the place, each grown to what it holds
    CliUsages usages;
    memor.set(&(MemoriaCfg){.dst = &usages, .val = 0, .bytes = sizeof(usages)});
    usages.name = name;
    usages.length = length;
    if (length != 0ULL)
    {
        cli_grown_begin(&usages.notes, CLI_USAGES_CAPACITY, g_cli_code_owner);
        cli_grown_begin(&usages.texts, CLI_USAGES_CAPACITY, g_cli_code_owner);
        cli_grown_begin(&usages.paths, CLI_USAGES_CAPACITY, g_cli_code_owner);
        cli_grown_begin(&usages.regions, CLI_USAGES_CAPACITY, g_cli_code_owner);
    }
    const int made = (length != 0ULL) && (usages.notes.cell != NULL) && (usages.texts.cell != NULL) &&
                     (usages.paths.cell != NULL) && (usages.regions.cell != NULL);
    int read = 0;
    if (made)
    {
        read = EMBED_CALL(usages_read, SymbolsUsagesCtx, .usages_out = &usages, .path = args->path, .text = text,
                          .size = size, .line = args->line, .col = args->col, .across = args->across,
                          .definition = args->definition, .open_text = args->open_text,
                          .open_text_context = args->open_text_context, .each = args->each, .context = args->context,
                          .err_msg = args->err_msg);
    }
    else if (length != 0ULL)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the usages' walk");
        read = -1;
    }
    cli_grown_end(&usages.notes);
    cli_grown_end(&usages.texts);
    cli_grown_end(&usages.paths);
    cli_grown_end(&usages.regions);
    if (mapped && (text != NULL))
    {
        g_cli_guard->persistent_buf_release((void *)(embed_word)((const unsigned char *)text), (g_cli_code_owner));
    }
    else
    {
        g_cli_guard->persistent_buf_release(text, g_cli_code_owner);
    }
    return read;
}

int cli_symbols_usages(const CliSymbolsPathContextCfg *args)
{
    return EMBED_CALL(usages_request, SymbolsUsagesCtx, .path = args->path, .line = args->line, .col = args->col,
                      .across = args->across, .definition = 0, .open_text = args->open_text,
                      .open_text_context = args->open_text_context, .each = args->each_usage, .context = args->context,
                      .err_msg = args->err_msg);
}

int cli_symbols_definition(const CliSymbolsPathContextCfg *args)
{
    return EMBED_CALL(usages_request, SymbolsUsagesCtx, .path = args->path, .line = args->line, .col = args->col,
                      .across = 0, .definition = 1, .open_text = args->open_text,
                      .open_text_context = args->open_text_context, .each = args->each_usage, .context = args->context,
                      .err_msg = args->err_msg);
}
