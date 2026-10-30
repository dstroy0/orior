// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_usages.c: the usages of the name at a place in a file of the tree, read off the leaves of the tree's
// branches. A local's or a parameter's are the leaves of its name in the region of the function that declares it; any
// other name's are the leaves of it, in every file the tree reads, whose definition in effect is the one in effect at
// the place: the define in effect, else the first declaration of the name the file's walk met, so a function a header
// declares and a file defines is one function, and a static of the same name in another file is another. Each file
// holding the name is walked once, its leaves of the name noted as the walk passes them, and one that a local of the
// function it stands in shadows set aside. A name, a local, a parameter, a region's brackets and a statement's end
// are known by the roles their grammar's tokens name; a function's region by its head declaring a function
#include "../command_line/cells.h"
#include "../command_line/file_writing.h"
#include "../text/text.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// the bytes the notes, their texts, the definitions' paths, the regions and the tree's paths are each begun in,
// doubling as they fill
#define CLI_USAGES_FIRST 4096u

// One leaf of the name a walk noted: its line, its span in UTF-16 units, its line's text, a place among the texts, and
// how many bytes; the definition in effect there, its path a place among the paths, CLI_JSON_NONE for none, and its
// line; whether the function region it stands in declares the name a local or a parameter; and that region's first
// leaf, CLI_JSON_NONE where it stands in none
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

// The usages being read: the name and its length; each walk's notes, their lines' texts and the definitions' paths,
// and the regions of the branch noted last, each a growing line; that branch's leaves; and whether the cells refused
// a note
typedef struct
{
    const char *name;
    unsigned long long length;
    VerbaLinea notes;
    VerbaLinea texts;
    VerbaLinea paths;
    VerbaLinea regions;
    const CliLeafRead *read;
    unsigned int noted;
    unsigned int region_count;
    int refused;
} CliUsages;

// the note at `at` of those read
static CliUsageNoted usages_note_at(const CliUsages *usages, unsigned int at)
{
    CliUsageNoted one;
    memor.cpy(&(MemoriaCfg){.dst = &one, .src = usages->notes.out + ((size_t)at * sizeof(one)), .bytes = sizeof(one)});
    return one;
}

// the region at `at` of the branch noted last
static CliUsageRegion *usages_region_at(const CliUsages *usages, unsigned int at)
{
    // the regions were written one after another into a cell taken for them, on its own boundary
    return (CliUsageRegion *)(void *)(usages->regions.out + ((size_t)at * sizeof(CliUsageRegion)));
}

// the top-level regions of `read`, each a bracket its grammar names a region's opening, at no depth, to the one that
// closes it, its head the statement before it: after the last statement's end or region at no depth, or the last
// directive's line; a function's where its head declares a function
static void usages_regions_read(CliUsages *usages, const CliLeafRead *read)
{
    usages->regions.at = 0u;
    usages->region_count = 0u;
    usages->read = read;
    unsigned int depth = 0u;
    unsigned int statement = 0u;
    unsigned int open = 0u;
    unsigned int directive_line = CLI_SLICE_NOWHERE;
    for (unsigned int leaf = 0u; leaf < read->count; leaf += 1u)
    {
        if (cli_leaf_has(read, leaf, CLI_LEAF_REGION | CLI_LEAF_OPEN))
        {
            open = (depth == 0u) ? leaf : open;
            depth += 1u;
            continue;
        }
        if (cli_leaf_has(read, leaf, CLI_LEAF_REGION | CLI_LEAF_CLOSE) && (depth > 0u))
        {
            depth -= 1u;
            if (depth == 0u)
            {
                int function = 0;
                for (unsigned int head = statement; !function && (head < open); head += 1u)
                {
                    function = cli_leaf_has(read, head, CLI_LEAF_DECLARES | CLI_LEAF_FUNCTION);
                }
                const CliUsageRegion region = {statement, open, leaf + 1u, function, -1};
                mmgr_line_put_n(&usages->regions, (const char *)&region, sizeof(region));
                usages->region_count += 1u;
                statement = leaf + 1u;
            }
            continue;
        }
        directive_line = ((read->leaf[leaf].is & CLI_LEAF_DIRECTIVE) != 0u) ? read->leaf[leaf].line : directive_line;
        if ((depth == 0u) &&
            ((read->leaf[leaf].line == directive_line) || cli_leaf_has(read, leaf, CLI_LEAF_STATEMENT)))
        {
            statement = leaf + 1u;
        }
    }
    usages->refused |= !mmgr_line_ok(&usages->regions);
}

// the place among the regions of the one the leaf `leaf` stands in, CLI_JSON_NONE where it stands in none
static unsigned int usages_region_of(const CliUsages *usages, unsigned int leaf)
{
    unsigned int low = 0u;
    unsigned int high = usages->region_count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (usages_region_at(usages, middle)->end <= leaf)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return ((low < usages->region_count) && (usages_region_at(usages, low)->head <= leaf)) ? low : CLI_JSON_NONE;
}

// whether the function region at `at` declares the name a local or a parameter, read once
static int usages_region_declares(CliUsages *usages, const CliLeafRead *read, unsigned int at)
{
    CliUsageRegion *const region = usages_region_at(usages, at);
    if (region->declares < 0)
    {
        region->declares = 0;
        for (unsigned int leaf = region->head; region->function && !region->declares && (leaf < region->end);
             leaf += 1u)
        {
            unsigned long long length = 0ull;
            const char *const text = cli_leaf_text(read, leaf, &length);
            region->declares = ((read->leaf[leaf].is & (CLI_LEAF_LOCAL | CLI_LEAF_PARAMETER)) != 0u) &&
                               ((read->leaf[leaf].is & CLI_LEAF_NAME) != 0u) && (length == usages->length) &&
                               cli_text_same(text, usages->name, length);
        }
    }
    return region->declares;
}

// a leaf of the name a walk passes noted with the definition in effect there, its line's text and its span, and
// whether a local of the function region it stands in shadows it
static void usages_noted(void *context, const CliLeafRead *read, unsigned int leaf, const char *path, unsigned int line)
{
    CliUsages *const usages = (CliUsages *)context;
    if (usages->read != read)
    {
        usages_regions_read(usages, read);
    }
    const unsigned int region = usages_region_of(usages, leaf);
    const int local = (region != CLI_JSON_NONE) && usages_region_declares(usages, read, region);
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
    end -= ((end > start) && (read->text[end - 1ull] == '\r')) ? 1ull : 0ull;
    // a line's units are below 2^32
    const unsigned int units = (unsigned int)cli_text_units(read->text + start, from - start);
    const unsigned int width = (unsigned int)cli_text_units(read->text + from, read->leaf[leaf].to - from);
    const CliUsageNoted one = {read->leaf[leaf].line,
                               units,
                               units + width,
                               (region != CLI_JSON_NONE) ? usages_region_at(usages, region)->head : CLI_JSON_NONE,
                               usages->texts.at,
                               end - start,
                               (path != NULL) ? (unsigned int)usages->paths.at : CLI_JSON_NONE,
                               line,
                               local};
    mmgr_line_put_n(&usages->texts, read->text + start, (size_t)(end - start));
    if (path != NULL)
    {
        mmgr_line_put(&usages->paths, path);
        mmgr_line_ch(&usages->paths, '\0');
    }
    mmgr_line_put_n(&usages->notes, (const char *)&one, sizeof(one));
    usages->noted += 1u;
    usages->refused |= !mmgr_line_ok(&usages->notes) || !mmgr_line_ok(&usages->texts) || !mmgr_line_ok(&usages->paths);
}

// the branch at `path`, `size` bytes of `text`, walked to its end, each of its leaves of the name noted in place of
// those noted before: 1, or 0 with why given where the cells cannot meet the walk
static int usages_walked(CliUsages *usages, const char *path, const char *text, unsigned long long size,
                         VerbaLinea *err_msg)
{
    usages->notes.at = 0u;
    usages->texts.at = 0u;
    usages->paths.at = 0u;
    usages->noted = 0u;
    usages->read = NULL;
    CliEffect *const effect = cli_effect_made(path, usages->name, 0, err_msg);
    if (effect == NULL)
    {
        return 0;
    }
    effect->start_text = text;
    effect->start_size = size;
    effect->noted = usages_noted;
    effect->noted_context = usages;
    cli_effect_taken(effect, path, CLI_EFFECT_GIVEN);
    const int refused = effect->refused || usages->refused;
    cli_effect_given_back(effect);
    if (refused)
    {
        mmgr_line_put(err_msg, "the cells cannot meet the usages' walk");
    }
    return !refused;
}

// whether the note `one` has the definition `defined`, a path, NULL for none, at `line`
static int usages_same(const CliUsages *usages, const CliUsageNoted *one, const char *defined, unsigned int line)
{
    if ((one->defined == CLI_JSON_NONE) || (defined == NULL))
    {
        return 0;
    }
    return (one->defined_line == line) &&
           cellul.eq(
               &(CatenaFinitaCfg){.src = usages->paths.out + one->defined, .other = defined, .cap = CLI_CELLS_POOL});
}

// each note of the branch at `path` handed on as a usage where it stands in the region `region` of a local, else where
// a local does not shadow it and its definition is `defined` at `line`
static void usages_handed(const CliUsages *usages, const char *path, unsigned int region, const char *defined,
                          unsigned int line, CliUsage each, void *context)
{
    for (unsigned int at = 0u; at < usages->noted; at += 1u)
    {
        const CliUsageNoted one = usages_note_at(usages, at);
        const int used = (region != CLI_JSON_NONE) ? (one.local && (one.region == region))
                                                   : (!one.local && usages_same(usages, &one, defined, line));
        if (used)
        {
            each(context, path, one.line, one.from, one.to, usages->texts.out + one.text, one.length);
        }
    }
}

// the first note of the branch at `path` handed on as the definition: the first in the region `region` of a local, else
// the first on the line `line` whose definition is `defined` at `line`; whether one was
static int usages_first_handed(const CliUsages *usages, const char *path, unsigned int region, const char *defined,
                               unsigned int line, CliUsage each, void *context)
{
    for (unsigned int at = 0u; at < usages->noted; at += 1u)
    {
        const CliUsageNoted one = usages_note_at(usages, at);
        const int used = (region != CLI_JSON_NONE)
                             ? (one.local && (one.region == region))
                             : (!one.local && (one.line == line) && usages_same(usages, &one, defined, line));
        if (used)
        {
            each(context, path, one.line, one.from, one.to, usages->texts.out + one.text, one.length);
            return 1;
        }
    }
    return 0;
}

// the text of the tree's file at `path`, `held`'s, else the disk's mapped, its size into `size`, `*mapped` 1 where it
// is the disk's: NULL where neither has it
static char *usages_text(const char *path, CliHeld held, void *held_context, unsigned long long *size, int *mapped)
{
    char *const given = (held != NULL) ? held(held_context, path, size) : NULL;
    *mapped = given == NULL;
    if (given != NULL)
    {
        return given;
    }
    char full[CLI_PATH_MAX_LEN];
    cli_lock_take(&g_cli_symbols_lock);
    cli_full(g_cli_symbols_root, path, full);
    cli_lock_give(&g_cli_symbols_lock);
    // a text read mapped is read and never written
    return (char *)cli_file_read(full, size);
}

// a text `usages_text` gave let go
static void usages_text_given(char *text, unsigned long long size, int mapped)
{
    if (mapped && (text != NULL))
    {
        cli_file_let_go((const unsigned char *)text, size);
    }
    else
    {
        cli_cell_release(text, NULL);
    }
}

// the name the leaf at `line`, `col` of `size` bytes of `text` names, read in the language of the file at `path`, into
// `name`, sized for CLI_NAME_MAX_LEN: its length, 0 where the place names no name
static unsigned long long usages_name_at(const char *path, const char *text, unsigned long long size, unsigned int line,
                                         unsigned int col, char *name)
{
    CliSymbolsLanguages *const languages = cli_symbols_languages_read();
    CliLeafRead read;
    const int leaved = (languages != NULL) && cli_symbols_leaves_kept(languages, path, "root", text, size, &read);
    unsigned long long length = 0ull;
    unsigned int leaf = 0u;
    while (leaved && (leaf < read.count) && (read.leaf[leaf].line < line))
    {
        leaf += 1u;
    }
    if (leaved && (leaf < read.count) && (read.leaf[leaf].line == line))
    {
        const unsigned long long start = read.leaf[leaf].from;
        unsigned long long end = start;
        while ((end < size) && (text[end] != '\n'))
        {
            end += 1ull;
        }
        unsigned long long byte = 0ull;
        unsigned int units = 0u;
        const unsigned long long at = start + cli_symbols_byte_at(text + start, end - start, col, &byte, &units);
        // the leaf the place stands in, else the one it ends where that one is a name
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
        const char *const word = named ? cli_leaf_text(&read, leaf, &length) : "";
        length = (length < CLI_NAME_MAX_LEN) ? length : 0ull;
        // a name below CLI_NAME_MAX_LEN bytes
        memor.cpy(&(MemoriaCfg){.dst = name, .src = word, .bytes = (size_t)length});
    }
    name[length] = '\0';
    if (leaved)
    {
        cli_symbols_leaves_given(&read);
    }
    cli_symbols_languages_given(languages);
    return length;
}

// the paths of the tree's files the tree reads, but `path` and the mounts', each with a 0 after, into `paths`
static void usages_tree_paths(const char *path, VerbaLinea *paths)
{
    cli_lock_take(&g_cli_symbols_lock);
    for (unsigned int at = 0u; at < g_cli_symbol_file_count; at += 1u)
    {
        const char *const one = g_cli_symbol_files[at].used ? cli_symbol_file_path(&g_cli_symbol_files[at]) : NULL;
        if ((one != NULL) && (one[0] != '@') &&
            !cellul.eq(&(CatenaFinitaCfg){.src = one, .other = path, .cap = CLI_CELLS_POOL}))
        {
            mmgr_line_put(paths, one);
            mmgr_line_ch(paths, '\0');
        }
    }
    cli_lock_give(&g_cli_symbols_lock);
}

// the usages read across the tree's files but `path`, each holding the name walked, the notes whose definition is
// `defined` at `line` handed on: 1, or 0 with why given
static int usages_across(CliUsages *usages, const char *path, const char *defined, unsigned int line, CliHeld held,
                         void *held_context, CliUsage each, void *context, VerbaLinea *err_msg)
{
    VerbaLinea paths = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    if (!cli_growing_line_made(&paths, CLI_USAGES_FIRST))
    {
        mmgr_line_put(err_msg, "the cells cannot meet the usages' walk");
        return 0;
    }
    usages_tree_paths(path, &paths);
    int walked = mmgr_line_ok(&paths);
    for (unsigned long long at = 0ull; walked && (at < paths.at);
         at += cellul.len(&(CatenaFinitaCfg){.src = paths.out + at, .cap = CLI_CELLS_POOL}) + 1ull)
    {
        const char *const one = paths.out + at;
        unsigned long long size = 0ull;
        int mapped = 0;
        char *const text = usages_text(one, held, held_context, &size, &mapped);
        unsigned long long found = 0ull;
        // a file whose bytes hold the name nowhere holds no leaf of it, and is not walked
        if ((text != NULL) && cli_text_find(text, size, usages->name, usages->length, &found))
        {
            walked = usages_walked(usages, one, text, size, err_msg);
            usages_handed(usages, one, CLI_JSON_NONE, defined, line, each, context);
        }
        usages_text_given(text, size, mapped);
    }
    cli_cell_release(paths.out, NULL);
    return walked;
}

// the leaf of the name on the line `line` of the tree's file `defined`, the definition in effect at the place asked
// about in the file at `path`, handed on, that file walked where it is not `path`: 1, 0 where its walk notes none
// there, as for a define the compile database gives, or -1 with why given
static int usages_definition_handed(CliUsages *usages, const char *path, const char *defined, unsigned int line,
                                    CliHeld held, void *held_context, CliUsage each, void *context, VerbaLinea *err_msg)
{
    if (!cellul.eq(&(CatenaFinitaCfg){.src = path, .other = defined, .cap = CLI_CELLS_POOL}))
    {
        unsigned long long size = 0ull;
        int mapped = 0;
        char *const text = usages_text(defined, held, held_context, &size, &mapped);
        if (text == NULL)
        {
            return 0;
        }
        const int walked = usages_walked(usages, defined, text, size, err_msg);
        usages_text_given(text, size, mapped);
        if (!walked)
        {
            return -1;
        }
    }
    return usages_first_handed(usages, defined, CLI_JSON_NONE, defined, line, each, context);
}

// the usages read in the file at `path`, its `size` bytes of `text`, of the name `name`, and across the tree where
// `across` is 1, or the definition in effect alone where `definition` is 1: 1, 0 where the place's leaf is no note or
// its name has no definition, or -1 with why given
static int usages_read(CliUsages *usages, const char *path, const char *text, unsigned long long size,
                       unsigned int line, unsigned int col, int across, int definition, CliHeld held,
                       void *held_context, CliUsage each, void *context, VerbaLinea *err_msg)
{
    if (!usages_walked(usages, path, text, size, err_msg))
    {
        return -1;
    }
    unsigned int place = 0u;
    CliUsageNoted at = {0u, 0u, 0u, CLI_JSON_NONE, 0ull, 0ull, CLI_JSON_NONE, 0u, 0};
    while (place < usages->noted)
    {
        at = usages_note_at(usages, place);
        if ((at.line == line) && (at.from <= col) && (col <= at.to))
        {
            break;
        }
        place += 1u;
    }
    if (place == usages->noted)
    {
        return 0;
    }
    if (at.local && definition)
    {
        return usages_first_handed(usages, path, at.region, NULL, 0u, each, context);
    }
    if (at.local)
    {
        usages_handed(usages, path, at.region, NULL, 0u, each, context);
        return 1;
    }
    if (at.defined == CLI_JSON_NONE)
    {
        return 0;
    }
    // the definition is copied out of the notes, which the walks across the tree write over
    char defined[CLI_PATH_MAX_LEN];
    VerbaLinea defined_line = {defined, sizeof(defined), 0ull};
    mmgr_line_put(&defined_line, usages->paths.out + at.defined);
    (void)mmgr_line_finish(&defined_line);
    if (definition)
    {
        return usages_definition_handed(usages, path, defined, at.defined_line, held, held_context, each, context,
                                        err_msg);
    }
    usages_handed(usages, path, CLI_JSON_NONE, defined, at.defined_line, each, context);
    if (across && !usages_across(usages, path, defined, at.defined_line, held, held_context, each, context, err_msg))
    {
        return -1;
    }
    return 1;
}

// the usages of the name at `line`, `col` of the tree's file at `path`, or its definition in effect alone where
// `definition` is 1, as cli_symbols_usages and cli_symbols_definition answer
static int usages_asked(const char *path, unsigned int line, unsigned int col, int across, int definition, CliHeld held,
                        void *held_context, CliUsage each, void *context, VerbaLinea *err_msg)
{
    unsigned long long size = 0ull;
    int mapped = 0;
    char *const text = usages_text(path, held, held_context, &size, &mapped);
    char name[CLI_NAME_MAX_LEN];
    const unsigned long long length = (text != NULL) ? usages_name_at(path, text, size, line, col, name) : 0ull;
    CliUsages usages = {name,
                        length,
                        {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                        {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                        {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                        {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                        NULL,
                        0u,
                        0u,
                        0};
    const int made = (length != 0ull) && cli_growing_line_made(&usages.notes, CLI_USAGES_FIRST) &&
                     cli_growing_line_made(&usages.texts, CLI_USAGES_FIRST) &&
                     cli_growing_line_made(&usages.paths, CLI_USAGES_FIRST) &&
                     cli_growing_line_made(&usages.regions, CLI_USAGES_FIRST);
    int read = 0;
    if (made)
    {
        read = usages_read(&usages, path, text, size, line, col, across, definition, held, held_context, each, context,
                           err_msg);
    }
    else if (length != 0ull)
    {
        mmgr_line_put(err_msg, "the cells cannot meet the usages' walk");
        read = -1;
    }
    cli_cell_release(usages.notes.out, NULL);
    cli_cell_release(usages.texts.out, NULL);
    cli_cell_release(usages.paths.out, NULL);
    cli_cell_release(usages.regions.out, NULL);
    usages_text_given(text, size, mapped);
    return read;
}

int cli_symbols_usages(const char *path, unsigned int line, unsigned int col, int across, CliHeld held,
                       void *held_context, CliUsage each, void *context, VerbaLinea *err_msg)
{
    return usages_asked(path, line, col, across, 0, held, held_context, each, context, err_msg);
}

int cli_symbols_definition(const char *path, unsigned int line, unsigned int col, CliHeld held, void *held_context,
                           CliUsage each, void *context, VerbaLinea *err_msg)
{
    return usages_asked(path, line, col, 0, 1, held, held_context, each, context, err_msg);
}
