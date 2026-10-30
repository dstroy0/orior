// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_effect.c: the definition of a name in effect at a line of a branch of the tree: a walk from the branch's
// first line to that line, the defines the compilation database gives the branch first and the files it includes
// before its first line walked before it, each include of a file of the tree, its own or a mount's, walked where it
// stands, found through the branch's include folders, and each branch once, each condition read off its leaves, and
// every define, undefine and declaration of the name met in an active region put in the walk's cells. The directives,
// a define's parameters and the file an include names are known by the roles their grammar's tokens name; nothing here
// reads a language's text
#include "../builds/compile_db.h"
#include "../command_line/cells.h"
#include "../command_line/file_writing.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// the bytes the walk's texts are begun with, doubling as they fill, and the room its macros and branches walked are
// begun with, the same
#define CLI_EFFECT_TEXT_FIRST 4096u
#define CLI_EFFECT_ROOM_FIRST 64u

// One condition open: whether its region is active, whether one of its arms was taken, and whether the region it
// opens in is active
typedef struct
{
    int active;
    int taken;
    int outer;
} CliEffectCondition;

// One declaration the leaves of a branch a slice's walk walks name: its line, the name it declares and its kind,
// places among the walk's texts, and the name's length
typedef struct
{
    unsigned int line;
    unsigned int name;
    unsigned int length;
    unsigned int kind;
} CliEffectDeclared;

// A branch being walked: its leaves, its conditions open and how many, the lines of the declarations of the name
// sought in it and how many; and for a slice's walk, every declaration its leaves name, read as they are, in a cell
// whose room doubles as they come, how many and the room, and its place among the branches read whole
typedef struct
{
    CliLeafRead read;
    CliEffectCondition condition[CLI_EFFECT_DEPTH];
    unsigned int open;
    unsigned int sought[CLI_EFFECT_SOUGHT];
    unsigned int soughts;
    CliEffectDeclared *declared;
    unsigned int declareds;
    unsigned int declared_room;
    unsigned int read_at;
} CliEffectBranch;

// A branch a slice's walk reads: the walk and the branch
typedef struct
{
    CliEffect *effect;
    CliEffectBranch *branch;
} CliEffectDeclaring;

const char *cli_effect_text(const CliEffect *effect, unsigned int at)
{
    return effect->text.out + at;
}

unsigned int cli_effect_put(CliEffect *effect, const char *text, unsigned long long length)
{
    // a walk's texts are far under 4 GiB
    const unsigned int at = (unsigned int)effect->text.at;
    mmgr_line_put_n(&effect->text, text, length);
    mmgr_line_ch(&effect->text, '\0');
    effect->refused |= !mmgr_line_ok(&effect->text);
    return mmgr_line_ok(&effect->text) ? at : CLI_JSON_NONE;
}

void *cli_effect_room(void *list, unsigned int count, unsigned int *room, unsigned long long each)
{
    if (count < *room)
    {
        return list;
    }
    const unsigned int grown = (*room == 0u) ? CLI_EFFECT_ROOM_FIRST : (2u * *room);
    void *const larger = g_cli_cells->persistent_buf_alloc((unsigned long long)grown * each, NULL);
    if (larger == NULL)
    {
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = larger, .src = list, .bytes = (size_t)(count * each)});
    cli_cell_release(list, NULL);
    *room = grown;
    return larger;
}

// what a slice's walk met put in the walk's cell after what it met before: its place among them, or CLI_JSON_NONE
// where the walk is no slice's, or the cells cannot meet it and the walk is refused
static unsigned int cli_effect_seen(CliEffect *effect, const CliEffectSeen *seen)
{
    if (!effect->slicing)
    {
        return CLI_JSON_NONE;
    }
    CliEffectSeen *const room = cli_effect_room(effect->seen, effect->seens, &effect->seen_room, sizeof(CliEffectSeen));
    if (room == NULL)
    {
        effect->refused = 1;
        return CLI_JSON_NONE;
    }
    effect->seen = room;
    effect->seen[effect->seens] = *seen;
    effect->seens += 1u;
    return effect->seens - 1u;
}

unsigned int cli_effect_macro(const CliEffect *effect, const char *name, unsigned long long length)
{
    for (unsigned int at = effect->macros; at > 0u; at -= 1u)
    {
        const char *const named = cli_effect_text(effect, effect->macro[at - 1u].name);
        if ((cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) == length) &&
            cli_text_same(named, name, length))
        {
            return effect->macro[at - 1u].undefined ? CLI_JSON_NONE : (at - 1u);
        }
    }
    return CLI_JSON_NONE;
}

// A macro met: its name and its value's text, each a text and its length; its value's leaves, those of `read` from
// `first` to before `end`; whether it is called with arguments, and if so its parameters' names, each a text and its
// length, how many, and whether it takes the arguments past them; the branch it was met in, a place among the walk's
// texts, its line, and whether it was undefined there; and its directive among what a slice's walk met
typedef struct
{
    unsigned int seen;
    const char *name;
    unsigned long long name_length;
    const char *value;
    unsigned long long value_length;
    const CliLeafRead *read;
    unsigned int first;
    unsigned int end;
    int called;
    const char *param[CLI_DEFINE_PARAMS];
    unsigned long long param_length[CLI_DEFINE_PARAMS];
    unsigned int params;
    int rest;
    unsigned int path;
    unsigned int line;
    int undefined;
} CliEffectMet;

// the leaf `leaf` of `read` put in the walk's cell of value leaves after those before it, its text put among the walk's
// texts: 1, or 0 with the walk refused where the cells cannot meet it
static int cli_effect_value_leaf_put(CliEffect *effect, const CliLeafRead *read, unsigned int leaf)
{
    CliEffectValueLeaf *const room =
        cli_effect_room(effect->value_leaf, effect->value_leaves, &effect->value_leaf_room, sizeof(CliEffectValueLeaf));
    unsigned long long length = 0ull;
    const char *const text = cli_leaf_text(read, leaf, &length);
    const unsigned int put = (room != NULL) ? cli_effect_put(effect, text, length) : CLI_JSON_NONE;
    if (put == CLI_JSON_NONE)
    {
        effect->refused = 1;
        return 0;
    }
    const CliLeaf *const one = &read->leaf[leaf];
    effect->value_leaf = room;
    // a leaf's text is far under 4 GiB
    effect->value_leaf[effect->value_leaves] =
        (CliEffectValueLeaf){one->is, put, (unsigned int)length, one->binds, one->base, one->does};
    effect->value_leaves += 1u;
    return 1;
}

// a macro met put after those met before, its value's leaves put in the walk's cell of value leaves but its blanks,
// comments and the leaves that carry its line on to the next
static void cli_effect_met(CliEffect *effect, const CliEffectMet *met)
{
    CliEffectMacro *const macros =
        cli_effect_room(effect->macro, effect->macros, &effect->macro_room, sizeof(CliEffectMacro));
    const unsigned int named = (macros != NULL) ? cli_effect_put(effect, met->name, met->name_length) : CLI_JSON_NONE;
    const unsigned int valued =
        (named != CLI_JSON_NONE) ? cli_effect_put(effect, met->value, met->value_length) : CLI_JSON_NONE;
    if (valued == CLI_JSON_NONE)
    {
        effect->refused = 1;
        return;
    }
    effect->macro = macros;
    // the parameters' names put one after another, each ended by its zero, the first's place theirs
    unsigned int params = valued;
    for (unsigned int at = 0u; at < met->params; at += 1u)
    {
        const unsigned int put = cli_effect_put(effect, met->param[at], met->param_length[at]);
        params = (at == 0u) ? put : params;
    }
    const unsigned int first = effect->value_leaves;
    const unsigned long long passed = CLI_LEAF_BLANK | CLI_LEAF_COMMENT | CLI_LEAF_CONTINUED;
    for (unsigned int leaf = met->first; !effect->refused && (leaf < met->end); leaf += 1u)
    {
        if ((met->read->leaf[leaf].is & passed) == 0u)
        {
            (void)cli_effect_value_leaf_put(effect, met->read, leaf);
        }
    }
    if (effect->refused)
    {
        return;
    }
    effect->macro[effect->macros] = (CliEffectMacro){named,
                                                     valued,
                                                     met->path,
                                                     met->line,
                                                     met->undefined,
                                                     met->called,
                                                     params,
                                                     met->params,
                                                     met->rest,
                                                     first,
                                                     effect->value_leaves - first,
                                                     met->seen};
    effect->macros += 1u;
}

// One define the compilation database gives the branch the walk starts from, met before its first line: its value's
// leaves read by the grammar of the branch's language from its first state
static void cli_effect_given(void *context, const CliDefineGiven *define)
{
    CliEffect *const effect = (CliEffect *)context;
    const unsigned int path = cli_effect_put(
        effect, define->node, cellul.len(&(CatenaFinitaCfg){.src = define->node, .cap = CLI_CELLS_POOL}));
    if (path == CLI_JSON_NONE)
    {
        return;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = define->value, .cap = CLI_CELLS_POOL});
    CliLeafRead read;
    // a value no grammar reads has no leaves, and a condition reads it as none
    const int leaved = cli_symbols_leaves_kept(effect->languages, effect->start, "root", define->value, size, &read);
    CliEffectMet met = {.seen = CLI_JSON_NONE,
                        .name = define->name,
                        .name_length = cellul.len(&(CatenaFinitaCfg){.src = define->name, .cap = CLI_CELLS_POOL}),
                        .value = define->value,
                        .value_length = size,
                        .read = &read,
                        .first = 0u,
                        .end = leaved ? read.count : 0u,
                        .called = define->called,
                        .params = (define->params < CLI_DEFINE_PARAMS) ? define->params : CLI_DEFINE_PARAMS,
                        .rest = define->rest,
                        .path = path,
                        .line = CLI_EFFECT_GIVEN,
                        .undefined = 0};
    for (unsigned int at = 0u; at < met.params; at += 1u)
    {
        met.param[at] = define->param[at];
        met.param_length[at] = cellul.len(&(CatenaFinitaCfg){.src = define->param[at], .cap = CLI_CELLS_POOL});
    }
    cli_effect_met(effect, &met);
    cli_symbols_leaves_given(&read);
}

// the whole path `whole`, its slashes `/`, as the tree holds it into `out`, sized for CLI_PATH_MAX_LEN: from the
// tree's top where it lies inside it, else under the name of the mount that holds it; 1, or 0 where neither does
static int cli_effect_tree_path(const CliEffect *effect, const char *whole, char *out)
{
    const char *names[CLI_MOUNTS_MOST];
    const char *paths[CLI_MOUNTS_MOST];
    const unsigned int mounts = cli_mounts(names, paths, CLI_MOUNTS_MOST);
    for (unsigned int at = 0u; at <= mounts; at += 1u)
    {
        // the tree's top first, then each mount
        const char *const top = (at == 0u) ? effect->root : paths[at - 1u];
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = top, .cap = CLI_CELLS_POOL});
        int under = length != 0ull;
        for (unsigned long long each = 0ull; under && (each < length); each += 1ull)
        {
            under = ((top[each] == '\\') ? '/' : top[each]) == ((whole[each] == '\\') ? '/' : whole[each]);
        }
        under = under && ((whole[length] == '/') || (whole[length] == '\0'));
        if (!under)
        {
            continue;
        }
        VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ull, NULL, NULL, 0ull};
        if (at != 0u)
        {
            mmgr_line_ch(&line, '@');
            mmgr_line_put(&line, names[at - 1u]);
            mmgr_line_put(&line, (whole[length] == '/') ? "/" : "");
        }
        mmgr_line_put(&line, whole + length + ((whole[length] == '/') ? 1u : 0u));
        return mmgr_line_finish(&line) != 0ull || (whole[length] == '\0');
    }
    return 0;
}

// One include folder or file of the branch the walk starts from, put in the walk's cell as the tree holds it where it
// does
static void cli_effect_include_met(void *context, const char *path, unsigned int kind)
{
    CliEffect *const effect = (CliEffect *)context;
    char tree_path[CLI_PATH_MAX_LEN];
    if (!cli_effect_tree_path(effect, path, tree_path))
    {
        return;
    }
    CliEffectInclude *const includes =
        cli_effect_room(effect->include, effect->includes, &effect->include_room, sizeof(CliEffectInclude));
    const unsigned int put =
        (includes != NULL)
            ? cli_effect_put(effect, tree_path, cellul.len(&(CatenaFinitaCfg){.src = tree_path, .cap = CLI_CELLS_POOL}))
            : CLI_JSON_NONE;
    if (put == CLI_JSON_NONE)
    {
        effect->refused = 1;
        return;
    }
    effect->include = includes;
    effect->include[effect->includes] = (CliEffectInclude){put, kind};
    effect->includes += 1u;
}

// `path` made whole into `out`, sized for CLI_PATH_MAX_LEN: each `.` left off and each `..` taking the folder before
// it away; 0 where a `..` leaves the tree's top
static int cli_effect_path_made(const char *path, char *out)
{
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    while (path[at] != '\0')
    {
        unsigned long long end = at;
        while ((path[end] != '\0') && (path[end] != '/'))
        {
            end += 1ull;
        }
        const unsigned long long size = end - at;
        if ((size == 2ull) && (path[at] == '.') && (path[at + 1u] == '.'))
        {
            if (length == 0ull)
            {
                return 0;
            }
            while ((length > 0ull) && (out[length - 1u] != '/'))
            {
                length -= 1ull;
            }
            length -= (length > 0ull) ? 1ull : 0ull;
        }
        else if ((size != 0ull) && !((size == 1ull) && (path[at] == '.')) &&
                 ((length + size + 2ull) < CLI_PATH_MAX_LEN))
        {
            if (length > 0ull)
            {
                out[length] = '/';
                length += 1ull;
            }
            memor.cpy(&(MemoriaCfg){.dst = out + length, .src = path + at, .bytes = (size_t)size});
            length += size;
        }
        at = (path[end] == '/') ? (end + 1ull) : end;
    }
    out[length] = '\0';
    return 1;
}

// whether `folder`, the tree's path of a folder, and `named`, `length` bytes, joined and made whole into `out` are a
// file of the tree; called under the lock
static int cli_effect_tree_file(const char *folder, unsigned long long folder_length, const char *named,
                                unsigned long long length, char *out)
{
    char joined[CLI_PATH_MAX_LEN];
    VerbaLinea line = {joined, sizeof(joined), 0ull, NULL, NULL, 0ull};
    mmgr_line_put_n(&line, folder, folder_length);
    mmgr_line_put(&line, ((folder_length != 0ull) && (folder[folder_length - 1u] != '/')) ? "/" : "");
    mmgr_line_put_n(&line, named, length);
    return (mmgr_line_finish(&line) != 0ull) && cli_effect_path_made(joined, out) &&
           (cli_symbols_file_of(out) != CLI_SYMBOL_FILES);
}

// the file of the tree an include of `named`, `length` bytes, in the branch at `path` reaches into `out`, sized for
// CLI_PATH_MAX_LEN, each a file the tree holds, its own or a mount's: beside the branch where it is named in quotes,
// then in each include folder of the branch the walk starts from in the order a compiler searches them, a folder for
// quotes alone passed over for one in angle brackets, then from the tree's top, else the first of the tree's files
// whose path ends with it; 1, or 0 where none does
static int cli_effect_included(const CliEffect *effect, const char *path, const char *named, unsigned long long length,
                               int angled, char *out)
{
    const char *const slash =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
    cli_lock_take(&g_cli_symbols_lock);
    // the branch's folder runs to its last slash, at or past its start
    int found = !angled && cli_effect_tree_file(path, (slash != NULL) ? (unsigned long long)(slash - path) : 0ull,
                                                named, length, out);
    for (unsigned int at = 0u; !found && (at < effect->includes); at += 1u)
    {
        const CliEffectInclude *const include = &effect->include[at];
        const char *const folder = cli_effect_text(effect, include->path);
        found = (include->kind != CLI_INCLUDE_FORCED) && (!angled || (include->kind != CLI_INCLUDE_QUOTE)) &&
                cli_effect_tree_file(folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}),
                                     named, length, out);
    }
    found = found || cli_effect_tree_file("", 0ull, named, length, out);
    for (unsigned int file = 0u; !found && (file < g_cli_symbol_file_count); file += 1u)
    {
        const CliSymbolFile *const branch = &g_cli_symbol_files[file];
        const char *const stored_path = branch->used ? cli_symbol_file_path(branch) : "";
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = stored_path, .cap = CLI_CELLS_POOL});
        const int ends = (size > length) && (stored_path[size - length - 1u] == '/') &&
                         cli_text_same(stored_path + size - length, named, length);
        if (ends && (size < CLI_PATH_MAX_LEN))
        {
            memor.cpy(&(MemoriaCfg){.dst = out, .src = stored_path, .bytes = (size_t)size + 1u});
            found = 1;
        }
    }
    cli_lock_give(&g_cli_symbols_lock);
    return found;
}

static void cli_effect_walk(CliEffect *effect, const char *path, unsigned int upto, unsigned int depth);

// whether the branch at `path` was walked already, and where it was not, it put among those walked
static int cli_effect_walked(CliEffect *effect, const char *path)
{
    for (unsigned int at = 0u; at < effect->walks; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_effect_text(effect, effect->walked[at]), .other = path, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    unsigned int *const walked =
        cli_effect_room(effect->walked, effect->walks, &effect->walk_room, sizeof(unsigned int));
    const unsigned int put =
        (walked != NULL)
            ? cli_effect_put(effect, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}))
            : CLI_JSON_NONE;
    if (put == CLI_JSON_NONE)
    {
        effect->refused = 1;
        return 1;
    }
    effect->walked = walked;
    effect->walked[effect->walks] = put;
    effect->walks += 1u;
    return 0;
}

// the lines of the declarations of the name sought the tree holds in the branch at `path`, in the order of their lines;
// a walk that notes the name's leaves reads them off the branch's own leaves as it reads them
static void cli_effect_sought(CliEffect *effect, const char *path, CliEffectBranch *branch)
{
    branch->soughts = 0u;
    branch->declared = NULL;
    branch->declareds = 0u;
    branch->declared_room = 0u;
    if (effect->noted != NULL)
    {
        return;
    }
    cli_lock_take(&g_cli_symbols_lock);
    const unsigned int file = cli_symbols_file_of(path);
    const CliSymbolFile *const stored = (file != CLI_SYMBOL_FILES) ? &g_cli_symbol_files[file] : NULL;
    const unsigned int count = (stored != NULL) ? stored->count : 0u;
    for (unsigned int each = 0u; (stored != NULL) && (each < count); each += 1u)
    {
        const CliSymbol *const symbol = &cli_symbol_file_symbols(stored)[each];
        if ((symbol->length == effect->length) && (branch->soughts < CLI_EFFECT_SOUGHT) &&
            cli_text_same(stored->cell + symbol->name, effect->name, effect->length))
        {
            branch->sought[branch->soughts] = symbol->line;
            branch->soughts += 1u;
        }
    }
    cli_lock_give(&g_cli_symbols_lock);
}

// a declaration the leaves of a branch a slice's walk or a noting walk reads name, as the leaves are read: a branch's
// own text, the editor's or one the tree has not read yet, declares what its leaves name. For a noting walk, its line
// among those of the name sought; for a slice's walk, put after those before it
static void cli_effect_declaring(void *context, const char *name, unsigned long long length, const char *kind,
                                 unsigned int line)
{
    CliEffectDeclaring *const declaring = (CliEffectDeclaring *)context;
    CliEffect *const effect = declaring->effect;
    CliEffectBranch *const branch = declaring->branch;
    if ((effect->length != 0ull) && (length == effect->length) && (branch->soughts < CLI_EFFECT_SOUGHT) &&
        cli_text_same(name, effect->name, length))
    {
        branch->sought[branch->soughts] = line;
        branch->soughts += 1u;
    }
    if (!effect->slicing)
    {
        return;
    }
    CliEffectDeclared *const room =
        cli_effect_room(branch->declared, branch->declareds, &branch->declared_room, sizeof(CliEffectDeclared));
    if (room == NULL)
    {
        effect->refused = 1;
        return;
    }
    branch->declared = room;
    const unsigned int named = cli_effect_put(effect, name, length);
    const unsigned int kinded =
        cli_effect_put(effect, kind, cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}));
    if ((named != CLI_JSON_NONE) && (kinded != CLI_JSON_NONE))
    {
        // a name a grammar's token covers is below 2^32 bytes
        branch->declared[branch->declareds] = (CliEffectDeclared){line, named, (unsigned int)length, kinded};
        branch->declareds += 1u;
    }
}

// the leaf after the last of the directive whose role is the leaf `role`: the end of its line, and of each line after
// while the line before it ends with a leaf its grammar names `continued`
static unsigned int cli_effect_directive_end(const CliLeafRead *read, unsigned int role)
{
    unsigned int leaf = role;
    for (unsigned int lines = 0u; lines < CLI_EFFECT_DIRECTIVE_LINES; lines += 1u)
    {
        const unsigned int line = read->leaf[leaf].line;
        unsigned int last = leaf;
        while (((leaf + 1u) < read->count) && (read->leaf[leaf + 1u].line == line))
        {
            leaf += 1u;
            last = ((read->leaf[leaf].is & CLI_LEAF_BLANK) == 0u) ? leaf : last;
        }
        const int continued = (read->leaf[last].is & CLI_LEAF_CONTINUED) != 0u;
        if (!continued || ((leaf + 1u) >= read->count))
        {
            break;
        }
        leaf += 1u;
    }
    return leaf + 1u;
}

// the first leaf from `from` to before `to` none of whose blank or comment it is, `to` where all are
static unsigned int cli_effect_next(const CliLeafRead *read, unsigned int from, unsigned int to)
{
    while ((from < to) && ((read->leaf[from].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u))
    {
        from += 1u;
    }
    return from;
}

// the integer the leaves from `from` to before `to` give as a condition, its blanks, comments and the leaves that carry
// its line on to the next passed over
static long long cli_effect_judged(CliEffect *effect, const CliLeafRead *read, unsigned int from, unsigned int to)
{
    CliEffectToken *const token =
        g_cli_cells->persistent_buf_alloc((unsigned long long)((to - from) + 1u) * sizeof(CliEffectToken), NULL);
    if (token == NULL)
    {
        effect->refused = 1;
        return 0ll;
    }
    unsigned int count = 0u;
    for (unsigned int leaf = from; leaf < to; leaf += 1u)
    {
        const CliLeaf *const one = &read->leaf[leaf];
        if ((one->is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT | CLI_LEAF_CONTINUED)) == 0u)
        {
            unsigned long long length = 0ull;
            const char *const text = cli_leaf_text(read, leaf, &length);
            token[count] = (CliEffectToken){text, length, one->is, one->binds, one->base, one->does};
            count += 1u;
        }
    }
    const long long value = cli_effect_value(effect, token, count, 0u);
    cli_cell_release(token, NULL);
    return value;
}

// the define or undefine whose role is the leaf `role` and whose name is the leaf `named`, its leaves running from its
// line's first, `from`, to before `end`, met in the branch at `path`, a place among the texts, `read_at` among the
// branches a slice's walk read whole. A name a leaf its grammar names a parameter follows unnamed is called with
// arguments: that leaf opens its parameters and the next such closes them, each name between that its grammar names a
// parameter is one, and a leaf named `rest` among them takes the arguments past them. Its value is the leaves after,
// and its text the define's from the leaf after its name, white space at either end left off
static void cli_effect_defined(CliEffect *effect, const CliLeafRead *read, unsigned int path, unsigned int read_at,
                               unsigned int role, unsigned int from, unsigned int named, unsigned int end)
{
    const CliEffectSeen seen = {CLI_SEEN_MACRO, read_at, read->leaf[role].line, from, end,
                                CLI_JSON_NONE,  0u,      CLI_JSON_NONE};
    CliEffectMet met = {.seen = cli_effect_seen(effect, &seen),
                        .read = read,
                        .path = path,
                        .line = read->leaf[role].line,
                        .undefined = (read->leaf[role].is & CLI_LEAF_UNDEF) != 0u};
    met.name = cli_leaf_text(read, named, &met.name_length);
    // a leaf of the parameters' brackets is a parameter's and no name's nor the rest's
    const unsigned long long roles = CLI_LEAF_PARAMETER | CLI_LEAF_NAME | CLI_LEAF_REST;
    unsigned int leaf = named + 1u;
    met.called = (leaf < end) && ((read->leaf[leaf].is & roles) == CLI_LEAF_PARAMETER);
    if (met.called)
    {
        leaf += 1u;
        while ((leaf < end) && ((read->leaf[leaf].is & roles) != CLI_LEAF_PARAMETER))
        {
            const unsigned long long is = read->leaf[leaf].is;
            met.rest |= (is & CLI_LEAF_REST) != 0u;
            if (((is & roles) == (CLI_LEAF_PARAMETER | CLI_LEAF_NAME)) && (met.params < CLI_DEFINE_PARAMS))
            {
                met.param[met.params] = cli_leaf_text(read, leaf, &met.param_length[met.params]);
                met.params += 1u;
            }
            leaf += 1u;
        }
        leaf += (leaf < end) ? 1u : 0u;
    }
    met.first = leaf;
    met.end = met.undefined ? leaf : end;
    const unsigned long long passed = CLI_LEAF_BLANK | CLI_LEAF_COMMENT | CLI_LEAF_CONTINUED;
    const unsigned int first = cli_effect_next(read, named + 1u, end);
    unsigned int last = end;
    while ((last > first) && ((read->leaf[last - 1u].is & passed) != 0u))
    {
        last -= 1u;
    }
    const unsigned long long text_from = (first < last) ? read->leaf[first].from : 0ull;
    met.value = read->text + text_from;
    met.value_length = (first < last) ? (read->leaf[last - 1u].to - text_from) : 0ull;
    cli_effect_met(effect, &met);
}

// the directive whose role is the leaf `role`, its leaves running from its line's first, `from`, to before `end`,
// taken in the branch at `path`, a place among the texts: its conditions opened, turned and closed, and in an active
// region its defines and undefines met, its includes of the tree's files walked, and for a slice's walk an include of
// no file of the tree met
static void cli_effect_directive(CliEffect *effect, CliEffectBranch *branch, unsigned int path, unsigned int role,
                                 unsigned int from, unsigned int end, unsigned int depth)
{
    const CliLeafRead *const read = &branch->read;
    const unsigned long long is = read->leaf[role].is;
    const int active = (branch->open == 0u) || branch->condition[branch->open - 1u].active;
    const unsigned int named = cli_effect_next(read, role + 1u, end);
    unsigned long long length = 0ull;
    const char *const name = (named < end) ? cli_leaf_text(read, named, &length) : "";
    if ((is & (CLI_LEAF_IF | CLI_LEAF_IFDEF | CLI_LEAF_IFNDEF)) != 0u)
    {
        const int defined = (length != 0ull) && (cli_effect_macro(effect, name, length) != CLI_JSON_NONE);
        const int holds = ((is & CLI_LEAF_IF) != 0u)      ? (cli_effect_judged(effect, read, role + 1u, end) != 0ll)
                          : ((is & CLI_LEAF_IFDEF) != 0u) ? defined
                                                          : !defined;
        if (branch->open < CLI_EFFECT_DEPTH)
        {
            branch->condition[branch->open] = (CliEffectCondition){active && holds, holds, active};
            branch->open += 1u;
        }
        return;
    }
    CliEffectCondition *const open = (branch->open > 0u) ? &branch->condition[branch->open - 1u] : NULL;
    if (((is & CLI_LEAF_ELIF) != 0u) && (open != NULL))
    {
        const int holds = !open->taken && (cli_effect_judged(effect, read, role + 1u, end) != 0ll);
        open->active = open->outer && holds;
        open->taken |= holds;
        return;
    }
    if (((is & CLI_LEAF_ELSE) != 0u) && (open != NULL))
    {
        open->active = open->outer && !open->taken;
        open->taken = 1;
        return;
    }
    if ((is & CLI_LEAF_ENDIF) != 0u)
    {
        branch->open -= (branch->open > 0u) ? 1u : 0u;
        return;
    }
    if (!active || (named >= end))
    {
        return;
    }
    if ((is & (CLI_LEAF_DEFINE | CLI_LEAF_UNDEF)) != 0u)
    {
        cli_effect_defined(effect, read, path, branch->read_at, role, from, named, end);
        return;
    }
    // an include's file is the leaf its grammar names the included, beside the branch where it names it so
    unsigned int included = named;
    while ((included < end) && ((read->leaf[included].is & CLI_LEAF_INCLUDED) == 0u))
    {
        included += 1u;
    }
    if (((is & CLI_LEAF_INCLUDE) != 0u) && (included < end) && (depth < CLI_EFFECT_DEPTH))
    {
        unsigned long long size = 0ull;
        const char *const file = cli_leaf_text(read, included, &size);
        const int angled = (read->leaf[included].is & CLI_LEAF_BESIDE) == 0u;
        char reached[CLI_PATH_MAX_LEN];
        if (cli_effect_included(effect, cli_effect_text(effect, path), file, size, angled, reached))
        {
            cli_effect_walk(effect, reached, CLI_EFFECT_GIVEN, depth + 1u);
            return;
        }
        const CliEffectSeen seen = {
            CLI_SEEN_INCLUDE, branch->read_at, read->leaf[role].line, from, end, CLI_JSON_NONE, 0u, CLI_JSON_NONE};
        (void)cli_effect_seen(effect, &seen);
    }
}

// a branch a slice's walk read whole, its bytes and its leaves, put in the walk's cell after those read before: its
// place among them, or CLI_JSON_NONE, the walk refused, where the cells cannot meet it
static unsigned int cli_effect_branch_read_put(CliEffect *effect, unsigned int path, const unsigned char *bytes,
                                               unsigned long long size, int from_disk, const CliLeafRead *read)
{
    CliEffectBranchRead *const room = cli_effect_room(effect->branch_read, effect->branches_read,
                                                      &effect->branch_read_room, sizeof(CliEffectBranchRead));
    if (room == NULL)
    {
        effect->refused = 1;
        return CLI_JSON_NONE;
    }
    effect->branch_read = room;
    effect->branch_read[effect->branches_read] = (CliEffectBranchRead){path, bytes, size, from_disk, *read};
    effect->branches_read += 1u;
    return effect->branches_read - 1u;
}

// each leaf from `first` to before `end` of the branch the walk starts from that names the name sought, neither a
// comment's nor a string's, handed on with the definition of the name in effect there as a usage knows it: the define
// in effect, else the first declaration of it the walk met, else none
static void cli_effect_noted_line(CliEffect *effect, const CliLeafRead *read, unsigned int first, unsigned int end)
{
    for (unsigned int leaf = first; (effect->noted != NULL) && (effect->length != 0ull) && (leaf < end); leaf += 1u)
    {
        unsigned long long length = 0ull;
        const char *const text = cli_leaf_text(read, leaf, &length);
        if (((read->leaf[leaf].is & CLI_LEAF_NAME) == 0u) ||
            ((read->leaf[leaf].is & (CLI_LEAF_COMMENT | CLI_LEAF_STRING)) != 0u) || (length != effect->length) ||
            !cli_text_same(text, effect->name, length))
        {
            continue;
        }
        const unsigned int macro = cli_effect_macro(effect, effect->name, effect->length);
        const char *const defined = (macro != CLI_JSON_NONE) ? cli_effect_text(effect, effect->macro[macro].path)
                                    : effect->first_found    ? cli_effect_text(effect, effect->first_path)
                                                             : NULL;
        const unsigned int line = (macro != CLI_JSON_NONE) ? effect->macro[macro].line : effect->first_line;
        effect->noted(effect->noted_context, read, leaf, defined, line);
    }
}

// the branch at `path` walked from its first line to before line `upto`, CLI_EFFECT_GIVEN for the whole of it, once in
// a walk, its includes `depth` deep; for a slice's walk, its bytes and leaves stay in the walk's cell until the walk is
// given back, and each declaration its leaves name on a line of an active region is met; for a noting walk, each leaf
// of the name in the branch it starts from handed on as the walk passes its line; and each line of that branch handed
// where its lines go before the walk reads it, the walk ended there where it is refused
static void cli_effect_walk(CliEffect *effect, const char *path, unsigned int upto, unsigned int depth)
{
    if (effect->refused || cli_effect_walked(effect, path))
    {
        return;
    }
    const unsigned int path_at = effect->walked[effect->walks - 1u];
    CliEffectBranch *const branch = g_cli_cells->persistent_buf_alloc(sizeof(CliEffectBranch), NULL);
    char full[CLI_PATH_MAX_LEN];
    cli_full(effect->root, path, full);
    // the branch the walk starts from is the editor's text where one is given, else every branch is the disk's
    const int from_disk = (effect->start_text == NULL) ||
                          !cellul.eq(&(CatenaFinitaCfg){.src = path, .other = effect->start, .cap = CLI_CELLS_POOL});
    unsigned long long size = from_disk ? 0ull : effect->start_size;
    const unsigned char *const bytes = (branch == NULL) ? NULL
                                       : from_disk      ? cli_file_read(full, &size)
                                                        : (const unsigned char *)effect->start_text;
    if (bytes == NULL)
    {
        effect->refused |= branch == NULL;
        cli_cell_release(branch, NULL);
        return;
    }
    branch->open = 0u;
    branch->read_at = CLI_JSON_NONE;
    cli_effect_sought(effect, path, branch);
    const char *const text = (const char *)bytes;
    CliEffectDeclaring declaring = {effect, branch};
    const int declares = effect->slicing || (effect->noted != NULL);
    const int leaved =
        cli_symbols_leaves_declared(effect->languages, path, "root", text, size, &branch->read,
                                    declares ? cli_effect_declaring : NULL, declares ? &declaring : NULL);
    branch->read_at = (leaved && effect->slicing)
                          ? cli_effect_branch_read_put(effect, path_at, bytes, size, from_disk, &branch->read)
                          : CLI_JSON_NONE;
    if (leaved)
    {
        const CliLeafRead *const read = &branch->read;
        unsigned int leaf = 0u;
        unsigned int declared = 0u;
        while ((leaf < read->count) && (read->leaf[leaf].line < upto) && !effect->refused)
        {
            const unsigned int line = read->leaf[leaf].line;
            const unsigned int line_first = leaf;
            const int active_here = (branch->open == 0u) || branch->condition[branch->open - 1u].active;
            if ((depth == 0u) && (effect->reached != NULL) &&
                !effect->reached(effect->reached_context, read, leaf, active_here))
            {
                break;
            }
            unsigned int role = leaf;
            while ((role < read->count) && (read->leaf[role].line == line) &&
                   ((read->leaf[role].is & CLI_LEAF_DIRECTIVE) == 0u) &&
                   ((read->leaf[role].is & (CLI_LEAF_BLANK | CLI_LEAF_KEYWORD)) != 0u))
            {
                role += 1u;
            }
            // a declaration on a line the walk passed over, or on a directive's, is none the walk meets
            while ((declared < branch->declareds) && (branch->declared[declared].line < line))
            {
                declared += 1u;
            }
            if ((role < read->count) && (read->leaf[role].line == line) &&
                ((read->leaf[role].is & CLI_LEAF_DIRECTIVE) != 0u))
            {
                const unsigned int end = cli_effect_directive_end(read, role);
                cli_effect_directive(effect, branch, path_at, role, leaf, end, depth);
                if ((depth == 0u) && active_here)
                {
                    cli_effect_noted_line(effect, read, line_first, end);
                }
                leaf = end;
                continue;
            }
            const int active = (branch->open == 0u) || branch->condition[branch->open - 1u].active;
            for (; (declared < branch->declareds) && (branch->declared[declared].line == line); declared += 1u)
            {
                const CliEffectDeclared *const one = &branch->declared[declared];
                const CliEffectSeen seen = {CLI_SEEN_DECLARED, branch->read_at, line,     leaf, 0u,
                                            one->name,         one->length,     one->kind};
                if (active)
                {
                    (void)cli_effect_seen(effect, &seen);
                }
            }
            for (unsigned int at = 0u; active && (at < branch->soughts); at += 1u)
            {
                if (branch->sought[at] != line)
                {
                    continue;
                }
                // the declaration's line, from its first leaf to its last, put among the walk's texts with where it
                // stands
                unsigned int last = leaf;
                while (((last + 1u) < read->count) && (read->leaf[last + 1u].line == line))
                {
                    last += 1u;
                }
                const unsigned int first = cli_effect_next(read, leaf, last + 1u);
                const unsigned long long from = (first <= last) ? read->leaf[first].from : 0ull;
                const unsigned int said = cli_effect_put(effect, text + from, read->leaf[last].to - from);
                effect->found_path = path_at;
                effect->found_text = said;
                effect->found_line = line;
                effect->found = said != CLI_JSON_NONE;
                effect->first_path = effect->first_found ? effect->first_path : path_at;
                effect->first_line = effect->first_found ? effect->first_line : line;
                effect->first_found = 1;
            }
            while ((leaf < read->count) && (read->leaf[leaf].line == line))
            {
                leaf += 1u;
            }
            if ((depth == 0u) && active_here)
            {
                cli_effect_noted_line(effect, read, line_first, leaf);
            }
        }
    }
    // a branch a slice's walk read whole stays in the walk's cell until the walk is given back
    if (branch->read_at == CLI_JSON_NONE)
    {
        cli_symbols_leaves_given(&branch->read);
        if (from_disk)
        {
            cli_file_let_go(bytes, size);
        }
    }
    cli_cell_release(branch->declared, NULL);
    cli_cell_release(branch, NULL);
}

CliEffect *cli_effect_made(const char *path, const char *name, int slicing, VerbaLinea *err_msg)
{
    CliEffect *const effect = g_cli_cells->persistent_buf_alloc(sizeof(CliEffect), NULL);
    CliSymbolsLanguages *const languages = (effect != NULL) ? cli_symbols_languages_read() : NULL;
    if ((languages == NULL) || !cli_growing_line_made(&effect->text, CLI_EFFECT_TEXT_FIRST))
    {
        cli_symbols_languages_given(languages);
        cli_cell_release(effect, NULL);
        mmgr_line_put(err_msg, "the cells cannot meet the walk");
        return NULL;
    }
    effect->languages = languages;
    cli_lock_take(&g_cli_symbols_lock);
    memor.cpy(&(MemoriaCfg){.dst = effect->root, .src = g_cli_symbols_root, .bytes = CLI_PATH_MAX_LEN});
    cli_lock_give(&g_cli_symbols_lock);
    effect->start = path;
    effect->start_text = NULL;
    effect->start_size = 0ull;
    effect->macro = NULL;
    effect->macros = 0u;
    effect->macro_room = 0u;
    effect->value_leaf = NULL;
    effect->value_leaves = 0u;
    effect->value_leaf_room = 0u;
    effect->walked = NULL;
    effect->walks = 0u;
    effect->walk_room = 0u;
    effect->include = NULL;
    effect->includes = 0u;
    effect->include_room = 0u;
    effect->name = name;
    effect->length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    effect->found = 0;
    effect->first_path = CLI_JSON_NONE;
    effect->first_line = 0u;
    effect->first_found = 0;
    effect->slicing = slicing;
    effect->branch_read = NULL;
    effect->branches_read = 0u;
    effect->branch_read_room = 0u;
    effect->seen = NULL;
    effect->seens = 0u;
    effect->seen_room = 0u;
    effect->noted = NULL;
    effect->noted_context = NULL;
    effect->reached = NULL;
    effect->reached_context = NULL;
    effect->refused = 0;
    return effect;
}

void cli_effect_taken(CliEffect *effect, const char *path, unsigned int line)
{
    cli_compile_db_defines_of(effect->root, path, cli_effect_given, effect);
    cli_compile_db_includes_of(effect->root, path, cli_effect_include_met, effect);
    // each file the branch includes before its first line walked before it, as its compiler reads them
    for (unsigned int at = 0u; at < effect->includes; at += 1u)
    {
        // the walk puts each branch's path among the texts, which move as they grow, so the file's is copied first
        const char *const included = cli_effect_text(effect, effect->include[at].path);
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = included, .cap = CLI_CELLS_POOL});
        if ((effect->include[at].kind == CLI_INCLUDE_FORCED) && (size < CLI_PATH_MAX_LEN))
        {
            char forced[CLI_PATH_MAX_LEN];
            memor.cpy(&(MemoriaCfg){.dst = forced, .src = included, .bytes = (size_t)size + 1u});
            cli_effect_walk(effect, forced, CLI_EFFECT_GIVEN, 1u);
        }
    }
    cli_effect_walk(effect, path, line, 0u);
}

void cli_effect_given_back(CliEffect *effect)
{
    for (unsigned int at = 0u; at < effect->branches_read; at += 1u)
    {
        cli_symbols_leaves_given(&effect->branch_read[at].read);
        if (effect->branch_read[at].from_disk)
        {
            cli_file_let_go(effect->branch_read[at].bytes, effect->branch_read[at].size);
        }
    }
    cli_cell_release(effect->branch_read, NULL);
    cli_cell_release(effect->seen, NULL);
    cli_cell_release(effect->text.out, NULL);
    cli_cell_release(effect->macro, NULL);
    cli_cell_release(effect->value_leaf, NULL);
    cli_cell_release(effect->walked, NULL);
    cli_cell_release(effect->include, NULL);
    cli_symbols_languages_given(effect->languages);
    cli_cell_release(effect, NULL);
}

int cli_symbols_effect(const char *path, unsigned int line, const char *name, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliEffect *const effect = cli_effect_made(path, name, 0, err_msg);
    if (effect == NULL)
    {
        return 0;
    }
    cli_effect_taken(effect, path, line);
    const unsigned int macro = cli_effect_macro(effect, name, effect->length);
    const int refused = effect->refused;
    if (!refused && ((macro != CLI_JSON_NONE) || effect->found))
    {
        const unsigned int where = (macro != CLI_JSON_NONE) ? effect->macro[macro].path : effect->found_path;
        const unsigned int at = (macro != CLI_JSON_NONE) ? effect->macro[macro].line : effect->found_line;
        const char *const said =
            cli_effect_text(effect, (macro != CLI_JSON_NONE) ? effect->macro[macro].value : effect->found_text);
        const char *const where_text = cli_effect_text(effect, where);
        mmgr_line_put(out, "{\"name\":");
        cli_json_quoted_lossy(out, (const unsigned char *)name, effect->length);
        mmgr_line_put(out, ",\"path\":");
        cli_json_quoted(out, where_text, cellul.len(&(CatenaFinitaCfg){.src = where_text, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"line\":");
        if (at == CLI_EFFECT_GIVEN)
        {
            mmgr_line_put(out, "null");
        }
        else
        {
            mmgr_line_decimal(out, at, 1u);
        }
        mmgr_line_put(out, ",\"text\":");
        cli_json_quoted_lossy(out, (const unsigned char *)said,
                              cellul.len(&(CatenaFinitaCfg){.src = said, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(out, '}');
    }
    else if (!refused)
    {
        mmgr_line_put(out, "null");
    }
    cli_effect_given_back(effect);
    if (refused)
    {
        mmgr_line_put(err_msg, "the cells cannot meet the walk");
    }
    return !refused;
}
