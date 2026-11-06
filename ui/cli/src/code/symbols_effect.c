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
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// One condition open: whether its region is active, whether one of its arms was taken, and whether the region it
// opens in is active
typedef struct
{
    int active;
    int reserved;
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
// A branch being walked: its leaves, its conditions open and how many, the lines of the declarations of the name sought
// in it and how many; and for a slice's walk, every declaration its leaves name, read as they are, in a cell at their
// most, how many and the capacity, and its place among the branches read whole
typedef struct
{
    CliLeafRead read;
    CliEffectCondition condition[CLI_EFFECT_DEPTH];
    unsigned int open;
    unsigned int needle[CLI_EFFECT_NEEDLE];
    unsigned int needles;
    CliEffectDeclared *declared;
    unsigned int declareds;
    unsigned int declared_capacity;
    unsigned int read_at;
} CliEffectBranch;
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
// the bytes the walk's texts are taken with, and the capacity its macros and branches walked are taken with, the same
#define CLI_EFFECT_TEXT_CAPACITY (8ULL << 20u)

// A branch a slice's walk reads: the walk and the branch
typedef struct
{
    CliEffect *effect;
    CliEffectBranch *branch;
} CliEffectDeclaring;

const char *cli_effect_text(const CliSymbolsCtx *args)
{
    return args->effect_read->text.out + args->at_value;
}

unsigned int cli_effect_append(const CliSymbolsTextLengthCtx *args)
{
    // a walk's texts are far under 4 GiB
    const unsigned int at = (unsigned int)args->effect->text.at;
    mmgr_line_put_n(&args->effect->text, args->text, args->length);
    mmgr_line_ch(&args->effect->text, '\0');
    args->effect->refused |= !mmgr_line_ok(&args->effect->text);
    return mmgr_line_ok(&args->effect->text) ? at : CLI_JSON_NONE;
}

typedef struct
{
    CliEffect *effect;
    const CliEffectSeen *seen;
    const CliLeafRead *read;
    unsigned int leaf;
    const CliEffectMet *met;
    const CliEffect *effect_read;
    const char *whole;
    char *out;
    const char *path;
    const char *folder;
    unsigned long long folder_length;
    const char *named;
    unsigned long long length;
    int angled;
    unsigned int upto;
    unsigned int depth;
    CliEffectBranch *branch;
    unsigned int role;
    unsigned int from;
    unsigned int to;
    unsigned int path_u32;
    unsigned int read_at;
    unsigned int named_u32;
    unsigned int end;
    const unsigned char *bytes;
    unsigned long long size;
    int from_disk;
    unsigned int first;
    unsigned int *declared_at;
} SymbolsEffectCtx;
// what a slice's walk met put in the walk's cell after what it met before: its place among them, or CLI_JSON_NONE
// where the walk is no slice's, or the cells cannot meet it and the walk is refused
EMBED_INLINE unsigned int cli_effect_visit(const SymbolsEffectCtx *args)
{
    if (!args->effect->slicing)
    {
        return CLI_JSON_NONE;
    }
    if (args->effect->seens >= args->effect->seen_capacity)
    {
        args->effect->refused = 1;
        return CLI_JSON_NONE;
    }
    args->effect->seen[args->effect->seens] = *args->seen;
    args->effect->seens += 1u;
    return args->effect->seens - 1u;
}

unsigned int cli_effect_macro(const CliSymbolsNameCtx *args)
{
    for (unsigned int at = args->effect_read->macros; at > 0u; at -= 1u)
    {
        const char *const named =
            EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx, .effect_read = args->effect_read,
                       .at_value = args->effect_read->macro[at - 1u].name);
        if ((cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_POOL_BYTES}) == args->length) &&
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = named, .other_pointer = args->name,
                       .size = args->length))
        {
            return args->effect_read->macro[at - 1u].undefined ? CLI_JSON_NONE : (at - 1u);
        }
    }
    return CLI_JSON_NONE;
}

// the leaf `leaf` of `read` put in the walk's cell of value leaves after those before it, its text put among the walk's
// texts: 1, or 0 with the walk refused where the cells cannot meet it
EMBED_INLINE int cli_effect_value_leaf_append(const SymbolsEffectCtx *args)
{
    CliEffect *effect = args->effect;
    const CliLeafRead *read = args->read;
    unsigned int leaf = args->leaf;
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                        .read_read = read, .leaf = leaf, .length_pointer = &length);
    const unsigned int put = (effect->value_leaves < effect->value_leaf_capacity)
                                 ? EMBED_CALL(cli_symbols_text_length_internal.effect_append, CliSymbolsTextLengthCtx,
                                              .effect = effect, .text = text, .length = length)
                                 : CLI_JSON_NONE;
    if (put == CLI_JSON_NONE)
    {
        effect->refused = 1;
        return 0;
    }
    const CliLeaf *const one = &read->leaf[leaf];
    // a leaf's text is far under 4 GiB
    effect->value_leaf[effect->value_leaves] =
        (CliEffectValueLeaf){one->is, put, (unsigned int)length, one->binds, one->base, one->does};
    effect->value_leaves += 1u;
    return 1;
}

// a macro met put after those met before, its value's leaves put in the walk's cell of value leaves but its blanks,
// comments and the leaves that carry its line on to the next
static void cli_effect_intersect(const SymbolsEffectCtx *args)
{
    CliEffect *effect = args->effect;
    const unsigned int named =
        (effect->macros < effect->macro_capacity)
            ? EMBED_CALL(cli_symbols_text_length_internal.effect_append, CliSymbolsTextLengthCtx, .effect = effect,
                         .text = args->met->name, .length = args->met->name_length)
            : CLI_JSON_NONE;
    const unsigned int valued =
        (named != CLI_JSON_NONE)
            ? EMBED_CALL(cli_symbols_text_length_internal.effect_append, CliSymbolsTextLengthCtx, .effect = effect,
                         .text = args->met->value, .length = args->met->value_length)
            : CLI_JSON_NONE;
    if (valued == CLI_JSON_NONE)
    {
        effect->refused = 1;
        return;
    }
    // the parameters' names put one after another, each ended by its zero, the first's place theirs
    unsigned int params = valued;
    for (unsigned int at = 0u; at < args->met->params; at += 1u)
    {
        const unsigned int put =
            EMBED_CALL(cli_symbols_text_length_internal.effect_append, CliSymbolsTextLengthCtx, .effect = effect,
                       .text = args->met->param[at], .length = args->met->param_length[at]);
        params = (at == 0u) ? put : params;
    }
    const unsigned int first = effect->value_leaves;
    const unsigned long long passed = CLI_LEAF_BLANK | CLI_LEAF_COMMENT | CLI_LEAF_CONTINUED;
    for (unsigned int leaf = args->met->first; !effect->refused && (leaf < args->met->end); leaf += 1u)
    {
        if ((args->met->read->leaf[leaf].is & passed) == 0u)
        {
            (void)EMBED_CALL(cli_effect_value_leaf_append, SymbolsEffectCtx, .effect = effect, .read = args->met->read,
                             .leaf = leaf);
        }
    }
    if (effect->refused)
    {
        return;
    }
    effect->macro[effect->macros] = (CliEffectMacro){named,
                                                     valued,
                                                     args->met->path,
                                                     args->met->line,
                                                     args->met->undefined,
                                                     args->met->called,
                                                     params,
                                                     args->met->params,
                                                     args->met->rest,
                                                     first,
                                                     effect->value_leaves - first,
                                                     args->met->seen};
    effect->macros += 1u;
}

// One define the compilation database gives the branch the walk starts from, met before its first line: its value's
// leaves read by the grammar of the branch's language from its first state
static void cli_effect_free(const CliDefineEachCfg *args)
{
    CliEffect *const effect = (CliEffect *)args->context;
    const unsigned int path =
        EMBED_CALL(cli_symbols_text_length_internal.effect_append, CliSymbolsTextLengthCtx, .effect = effect,
                   .text = args->define->node,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->define->node, .cap = CLI_POOL_BYTES}));
    if (path == CLI_JSON_NONE)
    {
        return;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->define->value, .cap = CLI_POOL_BYTES});
    CliLeafRead read;
    // a value no grammar reads has no leaves, and a condition reads it as none
    const int leaved =
        EMBED_CALL(cli_symbols_path_internal.leaves_save, CliSymbolsPathCtx, .languages = effect->languages,
                   .path = effect->start, .state = "root", .text = args->define->value, .size = size, .read = &read);
    CliEffectMet met = {.seen = CLI_JSON_NONE,
                        .name = args->define->name,
                        .name_length = cellul.len(&(CatenaFinitaCfg){.src = args->define->name, .cap = CLI_POOL_BYTES}),
                        .value = args->define->value,
                        .value_length = size,
                        .read = &read,
                        .first = 0u,
                        .end = leaved ? read.count : 0u,
                        .called = args->define->called,
                        .params = (args->define->params < CLI_DEFINE_PARAMS) ? args->define->params : CLI_DEFINE_PARAMS,
                        .rest = args->define->rest,
                        .path = path,
                        .line = CLI_EFFECT_INPUT,
                        .undefined = 0};
    for (unsigned int at = 0u; at < met.params; at += 1u)
    {
        met.param[at] = args->define->param[at];
        met.param_length[at] = cellul.len(&(CatenaFinitaCfg){.src = args->define->param[at], .cap = CLI_POOL_BYTES});
    }
    EMBED_CALL(cli_effect_intersect, SymbolsEffectCtx, .effect = effect, .met = &met);
    g_cli_guard->persistent_buf_release(read.leaf, g_cli_leaves_owner);
    read.leaf = NULL;
    read.count = 0u;
    read.capacity = 0u;
}

// the whole path `whole`, its slashes `/`, as the tree holds it into `out`, sized for CLI_PATH_MAX_LEN: from the
// tree's top where it lies inside it, else under the name of the mount that holds it; 1, or 0 where neither does
EMBED_INLINE int cli_effect_tree_path(const SymbolsEffectCtx *args)
{
    const char *names[CLI_MOUNTS_MAX];
    const char *paths[CLI_MOUNTS_MAX];
    const unsigned int mounts = EMBED_CALL(cli_capacity_names_internal.mounts, CliCapacityNamesCtx, .names = names,
                                           .paths_read = paths, .capacity_u32 = CLI_MOUNTS_MAX);
    for (unsigned int at = 0u; at <= mounts; at += 1u)
    {
        // the tree's top first, then each mount
        const char *const top = (at == 0u) ? args->effect_read->root : paths[at - 1u];
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = top, .cap = CLI_POOL_BYTES});
        int under = length != 0ULL;
        unsigned long long each = 0ULL;
        while (under && (each < length))
        {
            under = ((top[each] == '\\') ? '/' : top[each]) == ((args->whole[each] == '\\') ? '/' : args->whole[each]);
            each += 1ULL;
        }
        under = under && ((args->whole[length] == '/') || (args->whole[length] == '\0'));
        if (!under)
        {
            continue;
        }
        VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL, NULL, NULL, 0ULL};
        if (at != 0u)
        {
            mmgr_line_ch(&line, '@');
            mmgr_line_put(&line, names[at - 1u]);
            mmgr_line_put(&line, (args->whole[length] == '/') ? "/" : "");
        }
        mmgr_line_put(&line, args->whole + length + ((args->whole[length] == '/') ? 1u : 0u));
        return mmgr_line_finish(&line) != 0ULL || (args->whole[length] == '\0');
    }
    return 0;
}

// One include folder or file of the branch the walk starts from, put in the walk's cell as the tree holds it where it
// does
static void cli_effect_include_intersect(const CliEachPathKindCfg *args)
{
    CliEffect *const effect = (CliEffect *)args->context;
    char tree_path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_effect_tree_path, SymbolsEffectCtx, .effect_read = effect, .whole = args->path,
                    .out = tree_path))
    {
        return;
    }
    const unsigned int put =
        (effect->includes < effect->include_capacity)
            ? EMBED_CALL(cli_symbols_text_length_internal.effect_append, CliSymbolsTextLengthCtx, .effect = effect,
                         .text = tree_path,
                         .length = cellul.len(&(CatenaFinitaCfg){.src = tree_path, .cap = CLI_POOL_BYTES}))
            : CLI_JSON_NONE;
    if (put == CLI_JSON_NONE)
    {
        effect->refused = 1;
        return;
    }
    effect->include[effect->includes] = (CliEffectInclude){put, args->kind};
    effect->includes += 1u;
}

// `path` made whole into `out`, sized for CLI_PATH_MAX_LEN: each `.` left off and each `..` taking the folder before
// it away; 0 where a `..` leaves the tree's top
EMBED_INLINE int cli_effect_path_init(const SymbolsEffectCtx *args)
{
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN});
    unsigned long long at = 0ULL;
    unsigned long long length = 0ULL;
    while (at < whole)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = args->path, .at = at, .size = whole, .set = "/");
        const unsigned long long size = end - at;
        if ((size == 2ULL) && (memor.cmp(&(MemoriaCfg){.src = args->path + at, .other = "..", .bytes = 2u}) == 0))
        {
            if (length == 0ULL)
            {
                return 0;
            }
            length = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->out,
                                .at = length, .set = "/");
            length -= (length > 0ULL) ? 1ULL : 0ULL;
        }
        else if ((size != 0ULL) && !((size == 1ULL) && (args->path[at] == '.')) &&
                 ((length + size + 2ULL) < CLI_PATH_MAX_LEN))
        {
            if (length > 0ULL)
            {
                args->out[length] = '/';
                length += 1ULL;
            }
            memor.cpy(&(MemoriaCfg){.dst = args->out + length, .src = args->path + at, .bytes = (size_t)size});
            length += size;
        }
        // past the slash that ended the piece, or past the end where none did
        at = end + 1ULL;
    }
    args->out[length] = '\0';
    return 1;
}

// whether `folder`, the tree's path of a folder, and `named`, `length` bytes, joined and made whole into `out` are a
// file of the tree; called under the lock
EMBED_INLINE int cli_effect_tree_file(const SymbolsEffectCtx *args)
{
    char joined[CLI_PATH_MAX_LEN];
    VerbaLinea line = {joined, sizeof(joined), 0ULL, NULL, NULL, 0ULL};
    mmgr_line_put_n(&line, args->folder, args->folder_length);
    mmgr_line_put(&line, ((args->folder_length != 0ULL) && (args->folder[args->folder_length - 1u] != '/')) ? "/" : "");
    mmgr_line_put_n(&line, args->named, args->length);
    return (mmgr_line_finish(&line) != 0ULL) &&
           EMBED_CALL(cli_effect_path_init, SymbolsEffectCtx, .path = joined, .out = args->out) &&
           (EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = args->out) != CLI_BRANCHES_MAX);
}

// the file of the tree an include of `named`, `length` bytes, in the branch at `path` reaches into `out`, sized for
// CLI_PATH_MAX_LEN, each a file the tree holds, its own or a mount's: beside the branch where it is named in quotes,
// then in each include folder of the branch the walk starts from in the order a compiler searches them, a folder for
// quotes alone passed over for one in angle brackets, then from the tree's top, else the first of the tree's files
// whose path ends with it; 1, or 0 where none does
EMBED_INLINE int cli_effect_include(const SymbolsEffectCtx *args)
{
    const CliEffect *effect = args->effect_read;
    const char *path = args->path;
    unsigned long long length = args->length;
    const char *const slash =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}), .byte = '/');
    // the branch's folder runs to its last slash, at or past its start
    int found =
        !args->angled && EMBED_CALL(cli_effect_tree_file, SymbolsEffectCtx, .folder = path,
                                    .folder_length = (slash != NULL) ? (unsigned long long)(slash - path) : 0ULL,
                                    .named = args->named, .length = length, .out = args->out);
    for (unsigned int at = 0u; !found && (at < effect->includes); at += 1u)
    {
        const CliEffectInclude *const include = &effect->include[at];
        const char *const folder = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx, .effect_read = effect,
                                              .at_value = include->path);
        found = (include->kind != CLI_INCLUDE_FORCED) && (!args->angled || (include->kind != CLI_INCLUDE_QUOTE)) &&
                EMBED_CALL(cli_effect_tree_file, SymbolsEffectCtx, .folder = folder,
                           .folder_length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES}),
                           .named = args->named, .length = length, .out = args->out);
    }
    found = found || EMBED_CALL(cli_effect_tree_file, SymbolsEffectCtx, .folder = "", .folder_length = 0ULL,
                                .named = args->named, .length = length, .out = args->out);
    unsigned int file = 0u;
    while (!found && (file < g_cli_branch_count))
    {
        const CliTreeBranch *const branch = &g_cli_branches[file];
        char stored_path[CLI_PATH_MAX_LEN] = {'\0'};
        if (branch->used)
        {
            EMBED_CALL(cli_symbols_tree_branch_path.branch_path_read, CliSymbolsTreeBranchPathCfg, .branch = branch,
                       .path_out = stored_path);
        }
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = stored_path, .cap = CLI_POOL_BYTES});
        const int ends = (size > length) && (stored_path[size - length - 1u] == '/') &&
                         EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = stored_path + size - length,
                                    .other_pointer = args->named, .size = length);
        if (ends && (size < CLI_PATH_MAX_LEN))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->out, .src = stored_path, .bytes = (size_t)size + 1u});
            found = 1;
        }
        file += 1u;
    }
    return found;
}

static void cli_effect_walk(const SymbolsEffectCtx *args);

// whether the branch at `path` was walked already, and where it was not, it put among those walked
EMBED_INLINE int cli_effect_on_walk(const SymbolsEffectCtx *args)
{
    CliEffect *effect = args->effect;
    for (unsigned int at = 0u; at < effect->walks; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx,
                                                           .effect_read = effect, .at_value = effect->walked[at]),
                                         .other = args->path,
                                         .cap = CLI_POOL_BYTES}))
        {
            return 1;
        }
    }
    const unsigned int put =
        (effect->walks < effect->walk_capacity)
            ? EMBED_CALL(cli_symbols_text_length_internal.effect_append, CliSymbolsTextLengthCtx, .effect = effect,
                         .text = args->path,
                         .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}))
            : CLI_JSON_NONE;
    if (put == CLI_JSON_NONE)
    {
        effect->refused = 1;
        return 1;
    }
    effect->walked[effect->walks] = put;
    effect->walks += 1u;
    return 0;
}

// the lines of the declarations of the name sought the tree holds in the branch at `path`, in the order of their lines;
// a walk that notes the name's leaves reads them off the branch's own leaves as it reads them
EMBED_INLINE void cli_effect_find(const SymbolsEffectCtx *args)
{
    const char *path = args->path;
    CliEffectBranch *branch = args->branch;
    branch->needles = 0u;
    branch->declared =
        args->effect->slicing
            ? g_cli_guard->temporary_buf_alloc(CLI_EFFECT_DECLARED_MAX * sizeof(CliEffectDeclared), g_cli_code_owner)
            : NULL;
    branch->declareds = 0u;
    branch->declared_capacity = (branch->declared != NULL) ? CLI_EFFECT_DECLARED_MAX : 0u;
    if (args->effect->noted != NULL)
    {
        return;
    }
    const unsigned int file = EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = path);
    const CliTreeBranch *const stored = (file != CLI_BRANCHES_MAX) ? &g_cli_branches[file] : NULL;
    char *const scratch = (stored != NULL) ? cli_branch_copy_alloc() : NULL;
    if (scratch != NULL)
    {
        EMBED_CALL(cli_symbols_tree_branch.branch_copy, CliSymbolsTreeBranchCfg, .branch = stored, .scratch = scratch);
    }
    const unsigned int count = (scratch != NULL) ? stored->count : 0u;
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        const CliDeclaration *const symbol = &EMBED_CALL(cli_symbols_tree_branch_scratch.branch_copy_declarations,
                                                         CliSymbolsTreeBranchScratchCfg, .scratch_read = scratch)[each];
        if ((symbol->length == args->effect->length) && (branch->needles < CLI_EFFECT_NEEDLE) &&
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = scratch + symbol->name,
                       .other_pointer = args->effect->name, .size = args->effect->length))
        {
            branch->needle[branch->needles] = symbol->line;
            branch->needles += 1u;
        }
    }
    g_cli_guard->persistent_buf_release(scratch, g_cli_code_owner);
}

// a declaration the leaves of a branch a slice's walk or a noting walk reads name, as the leaves are read: a branch's
// own text, the editor's or one the tree has not read yet, declares what its leaves name. For a noting walk, its line
// among those of the name sought; for a slice's walk, put after those before it
static void cli_effect_declaring(const CliDeclaredCfg *args)
{
    unsigned long long length = args->length;
    CliEffectDeclaring *const declaring = (CliEffectDeclaring *)args->context;
    CliEffect *const effect = declaring->effect;
    CliEffectBranch *const branch = declaring->branch;
    if ((effect->length != 0ULL) && (length == effect->length) && (branch->needles < CLI_EFFECT_NEEDLE) &&
        EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->name, .other_pointer = effect->name,
                   .size = length))
    {
        branch->needle[branch->needles] = args->line;
        branch->needles += 1u;
    }
    if (!effect->slicing)
    {
        return;
    }
    if (branch->declareds >= branch->declared_capacity)
    {
        effect->refused = 1;
        return;
    }
    const unsigned int named = EMBED_CALL(cli_symbols_text_length_internal.effect_append, CliSymbolsTextLengthCtx,
                                          .effect = effect, .text = args->name, .length = length);
    const unsigned int kinded = EMBED_CALL(
        cli_symbols_text_length_internal.effect_append, CliSymbolsTextLengthCtx, .effect = effect, .text = args->kind,
        .length = cellul.len(&(CatenaFinitaCfg){.src = args->kind, .cap = CLI_POOL_BYTES}));
    if ((named != CLI_JSON_NONE) && (kinded != CLI_JSON_NONE))
    {
        // a name a grammar's token covers is below 2^32 bytes
        branch->declared[branch->declareds] = (CliEffectDeclared){args->line, named, (unsigned int)length, kinded};
        branch->declareds += 1u;
    }
}

// the leaf after the last of the directive whose role is the leaf `role`: the end of its line, and of each line after
// while the line before it ends with a leaf its grammar names `continued`
EMBED_INLINE unsigned int cli_effect_directive_end(const SymbolsEffectCtx *args)
{
    unsigned int leaf = args->role;
    for (unsigned int lines = 0u; lines < CLI_EFFECT_DIRECTIVE_LINES; lines += 1u)
    {
        const unsigned int line = args->read->leaf[leaf].line;
        unsigned int last = leaf;
        while (((leaf + 1u) < args->read->count) && (args->read->leaf[leaf + 1u].line == line))
        {
            leaf += 1u;
            last = ((args->read->leaf[leaf].is & CLI_LEAF_BLANK) == 0u) ? leaf : last;
        }
        const int continued = (args->read->leaf[last].is & CLI_LEAF_CONTINUED) != 0u;
        if (!continued || ((leaf + 1u) >= args->read->count))
        {
            break;
        }
        leaf += 1u;
    }
    return leaf + 1u;
}

// the first leaf from `from` to before `to` none of whose blank or comment it is, `to` where all are
EMBED_INLINE unsigned int cli_effect_next(const SymbolsEffectCtx *args)
{
    unsigned int from = args->from;
    while ((from < args->to) && ((args->read->leaf[from].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u))
    {
        from += 1u;
    }
    return from;
}

// the integer the leaves from `from` to before `to` give as a condition, its blanks, comments and the leaves that carry
// its line on to the next passed over
EMBED_INLINE long long cli_effect_judged(const SymbolsEffectCtx *args)
{
    CliEffect *effect = args->effect;
    const CliLeafRead *read = args->read;
    CliEffectToken *const token = g_cli_guard->temporary_buf_alloc(
        (unsigned long long)((args->to - args->from) + 1u) * sizeof(CliEffectToken), g_cli_code_owner);
    if (token == NULL)
    {
        effect->refused = 1;
        return 0LL;
    }
    unsigned int count = 0u;
    for (unsigned int leaf = args->from; leaf < args->to; leaf += 1u)
    {
        const CliLeaf *const one = &read->leaf[leaf];
        if ((one->is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT | CLI_LEAF_CONTINUED)) == 0u)
        {
            unsigned long long length = 0ULL;
            const char *const text = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                                .read_read = read, .leaf = leaf, .length_pointer = &length);
            token[count] = (CliEffectToken){text, length, one->is, one->binds, one->base, one->does};
            count += 1u;
        }
    }
    const long long value = EMBED_CALL(cli_symbols_effect_internal.effect_value, CliSymbolsEffectCtx, .effect = effect,
                                       .token = token, .count = count, .through = 0u);
    g_cli_guard->persistent_buf_release(token, g_cli_code_owner);
    return value;
}

// the define or undefine whose role is the leaf `role` and whose name is the leaf `named`, its leaves running from its
// line's first, `from`, to before `end`, met in the branch at `path`, a place among the texts, `read_at` among the
// branches a slice's walk read whole. A name a leaf its grammar names a parameter follows unnamed is called with
// arguments: that leaf opens its parameters and the next such closes them, each name between that its grammar names a
// parameter is one, and a leaf named `rest` among them takes the arguments past them. Its value is the leaves after,
// and its text the define's from the leaf after its name, white space at either end left off
EMBED_INLINE void cli_effect_define(const SymbolsEffectCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int path = args->path_u32;
    unsigned int end = args->end;
    const CliEffectSeen seen = {
        CLI_SEEN_MACRO, args->read_at, read->leaf[args->role].line, args->from, end, CLI_JSON_NONE, 0u, CLI_JSON_NONE};
    CliEffectMet met = {.seen = EMBED_CALL(cli_effect_visit, SymbolsEffectCtx, .effect = args->effect, .seen = &seen),
                        .read = read,
                        .path = path,
                        .line = read->leaf[args->role].line,
                        .undefined = (read->leaf[args->role].is & CLI_LEAF_UNDEF) != 0u};
    met.name = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx, .read_read = read,
                          .leaf = args->named_u32, .length_pointer = &met.name_length);
    // a leaf of the parameters' brackets is a parameter's and no name's nor the rest's
    const unsigned long long roles = CLI_LEAF_PARAMETER | CLI_LEAF_NAME | CLI_LEAF_REMAINDER;
    unsigned int leaf = args->named_u32 + 1u;
    met.called = (leaf < end) && ((read->leaf[leaf].is & roles) == CLI_LEAF_PARAMETER);
    if (met.called)
    {
        leaf += 1u;
        while ((leaf < end) && ((read->leaf[leaf].is & roles) != CLI_LEAF_PARAMETER))
        {
            const unsigned long long is = read->leaf[leaf].is;
            met.rest |= (is & CLI_LEAF_REMAINDER) != 0u;
            if (((is & roles) == (CLI_LEAF_PARAMETER | CLI_LEAF_NAME)) && (met.params < CLI_DEFINE_PARAMS))
            {
                met.param[met.params] =
                    EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx, .read_read = read,
                               .leaf = leaf, .length_pointer = &met.param_length[met.params]);
                met.params += 1u;
            }
            leaf += 1u;
        }
        leaf += (leaf < end) ? 1u : 0u;
    }
    met.first = leaf;
    met.end = met.undefined ? leaf : end;
    const unsigned long long passed = CLI_LEAF_BLANK | CLI_LEAF_COMMENT | CLI_LEAF_CONTINUED;
    const unsigned int first =
        EMBED_CALL(cli_effect_next, SymbolsEffectCtx, .read = read, .from = args->named_u32 + 1u, .to = end);
    unsigned int last = end;
    while ((last > first) && ((read->leaf[last - 1u].is & passed) != 0u))
    {
        last -= 1u;
    }
    const unsigned long long text_from = (first < last) ? read->leaf[first].from : 0ULL;
    met.value = read->text + text_from;
    met.value_length = (first < last) ? (read->leaf[last - 1u].to - text_from) : 0ULL;
    EMBED_CALL(cli_effect_intersect, SymbolsEffectCtx, .effect = args->effect, .met = &met);
}

// the condition the directive whose role is the leaf `role` opens put last among those of the branch, whether the name
// it asks about, `named` of `length` bytes, stands defined or its leaves to before `end` give a value that is not 0
EMBED_INLINE void cli_effect_condition_open(const SymbolsEffectCtx *args)
{
    CliEffect *effect = args->effect;
    CliEffectBranch *const branch = args->branch;
    const unsigned long long is = branch->read.leaf[args->role].is;
    const int active = (branch->open == 0u) || branch->condition[branch->open - 1u].active;
    const int defined = (args->length != 0ULL) &&
                        (EMBED_CALL(cli_symbols_name_internal.effect_macro, CliSymbolsNameCtx, .effect_read = effect,
                                    .name = args->named, .length = args->length) != CLI_JSON_NONE);
    int holds;
    if ((is & CLI_LEAF_IF) != 0u)
    {
        holds = EMBED_CALL(cli_effect_judged, SymbolsEffectCtx, .effect = effect, .read = &branch->read,
                           .from = args->role + 1u, .to = args->end) != 0LL;
    }
    else if ((is & CLI_LEAF_IFDEF) != 0u)
    {
        holds = defined;
    }
    else
    {
        holds = !defined;
    }
    if (branch->open < CLI_EFFECT_DEPTH)
    {
        branch->condition[branch->open] = (CliEffectCondition){active && holds, holds, active};
        branch->open += 1u;
    }
}

// the branch's last open condition turned or closed by the directive whose role is the leaf `role`: 1, or 0 where the
// directive does neither
EMBED_INLINE int cli_effect_condition_turn(const SymbolsEffectCtx *args)
{
    CliEffectBranch *const branch = args->branch;
    const unsigned long long is = branch->read.leaf[args->role].is;
    CliEffectCondition *const open = (branch->open > 0u) ? &branch->condition[branch->open - 1u] : NULL;
    if (((is & CLI_LEAF_ELIF) != 0u) && (open != NULL))
    {
        const int holds =
            !open->reserved && (EMBED_CALL(cli_effect_judged, SymbolsEffectCtx, .effect = args->effect,
                                           .read = &branch->read, .from = args->role + 1u, .to = args->end) != 0LL);
        open->active = open->outer && holds;
        open->reserved |= holds;
        return 1;
    }
    if (((is & CLI_LEAF_ELSE) != 0u) && (open != NULL))
    {
        open->active = open->outer && !open->reserved;
        open->reserved = 1;
        return 1;
    }
    if ((is & CLI_LEAF_ENDIF) != 0u)
    {
        branch->open -= (branch->open > 0u) ? 1u : 0u;
        return 1;
    }
    return 0;
}

// the include whose role is the leaf `role`, its first leaf after that `named_u32`: the file of the tree it reaches
// walked, or for a slice's walk an include of no file of the tree met. Its file is the leaf its grammar names the
// included, beside the branch where it names it so
EMBED_INLINE void cli_effect_directive_include(const SymbolsEffectCtx *args)
{
    CliEffect *effect = args->effect;
    const CliLeafRead *const read = &args->branch->read;
    unsigned int included = args->named_u32;
    while ((included < args->end) && ((read->leaf[included].is & CLI_LEAF_INCLUDED) == 0u))
    {
        included += 1u;
    }
    if (((read->leaf[args->role].is & CLI_LEAF_INCLUDE) == 0u) || (included >= args->end) ||
        (args->depth >= CLI_EFFECT_DEPTH))
    {
        return;
    }
    unsigned long long size = 0ULL;
    const char *const file = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                        .read_read = read, .leaf = included, .length_pointer = &size);
    const int angled = (read->leaf[included].is & CLI_LEAF_ADJACENT) == 0u;
    char reached[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_effect_include, SymbolsEffectCtx, .effect_read = effect,
                   .path = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx, .effect_read = effect,
                                      .at_value = args->path_u32),
                   .named = file, .length = size, .angled = angled, .out = reached))
    {
        EMBED_CALL(cli_effect_walk, SymbolsEffectCtx, .effect = effect, .path = reached, .upto = CLI_EFFECT_INPUT,
                   .depth = args->depth + 1u);
        return;
    }
    const CliEffectSeen seen = {
        CLI_SEEN_INCLUDE, args->branch->read_at, read->leaf[args->role].line, args->from, args->end, CLI_JSON_NONE, 0u,
        CLI_JSON_NONE};
    (void)EMBED_CALL(cli_effect_visit, SymbolsEffectCtx, .effect = effect, .seen = &seen);
}

// the directive whose role is the leaf `role`, its leaves running from its line's first, `from`, to before `end`,
// taken in the branch at `path`, a place among the texts: its conditions opened, turned and closed, and in an active
// region its defines and undefines met, its includes of the tree's files walked, and for a slice's walk an include of
// no file of the tree met
static void cli_effect_directive(const SymbolsEffectCtx *args)
{
    CliEffect *effect = args->effect;
    const CliLeafRead *const read = &args->branch->read;
    const unsigned long long is = read->leaf[args->role].is;
    const int active = (args->branch->open == 0u) || args->branch->condition[args->branch->open - 1u].active;
    const unsigned int named =
        EMBED_CALL(cli_effect_next, SymbolsEffectCtx, .read = read, .from = args->role + 1u, .to = args->end);
    unsigned long long length = 0ULL;
    const char *const name = (named < args->end)
                                 ? EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                              .read_read = read, .leaf = named, .length_pointer = &length)
                                 : "";
    if ((is & (CLI_LEAF_IF | CLI_LEAF_IFDEF | CLI_LEAF_IFNDEF)) != 0u)
    {
        EMBED_CALL(cli_effect_condition_open, SymbolsEffectCtx, .effect = effect, .branch = args->branch,
                   .role = args->role, .end = args->end, .named = name, .length = length);
        return;
    }
    if (EMBED_CALL(cli_effect_condition_turn, SymbolsEffectCtx, .effect = effect, .branch = args->branch,
                   .role = args->role, .end = args->end) ||
        !active || (named >= args->end))
    {
        return;
    }
    if ((is & (CLI_LEAF_DEFINE | CLI_LEAF_UNDEF)) != 0u)
    {
        EMBED_CALL(cli_effect_define, SymbolsEffectCtx, .effect = effect, .read = read, .path_u32 = args->path_u32,
                   .read_at = args->branch->read_at, .role = args->role, .from = args->from, .named_u32 = named,
                   .end = args->end);
        return;
    }
    EMBED_CALL(cli_effect_directive_include, SymbolsEffectCtx, .effect = effect, .branch = args->branch,
               .path_u32 = args->path_u32, .role = args->role, .from = args->from, .named_u32 = named, .end = args->end,
               .depth = args->depth);
}

// a branch a slice's walk read whole, its bytes and its leaves, put in the walk's cell after those read before: its
// place among them, or CLI_JSON_NONE, the walk refused, where the cells cannot meet it
EMBED_INLINE unsigned int cli_effect_branch_read_append(const SymbolsEffectCtx *args)
{
    if (args->effect->branches_read >= args->effect->branch_read_capacity)
    {
        args->effect->refused = 1;
        return CLI_JSON_NONE;
    }
    args->effect->branch_read[args->effect->branches_read] =
        (CliEffectBranchRead){args->path_u32, args->bytes, args->size, args->from_disk, *args->read};
    args->effect->branches_read += 1u;
    return args->effect->branches_read - 1u;
}

// each leaf from `first` to before `end` of the branch the walk starts from that names the name sought, neither a
// comment's nor a string's, handed on with the definition of the name in effect there as a usage knows it: the define
// in effect, else the first declaration of it the walk met, else none
EMBED_INLINE void cli_effect_noted_line(const SymbolsEffectCtx *args)
{
    CliEffect *effect = args->effect;
    const CliLeafRead *read = args->read;
    for (unsigned int leaf = args->first; (effect->noted != NULL) && (effect->length != 0ULL) && (leaf < args->end);
         leaf += 1u)
    {
        unsigned long long length = 0ULL;
        const char *const text = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                            .read_read = read, .leaf = leaf, .length_pointer = &length);
        if (((read->leaf[leaf].is & CLI_LEAF_NAME) == 0u) ||
            ((read->leaf[leaf].is & (CLI_LEAF_COMMENT | CLI_LEAF_STRING)) != 0u) || (length != effect->length) ||
            !EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = text, .other_pointer = effect->name,
                        .size = length))
        {
            continue;
        }
        const unsigned int macro = EMBED_CALL(cli_symbols_name_internal.effect_macro, CliSymbolsNameCtx,
                                              .effect_read = effect, .name = effect->name, .length = effect->length);
        const char *defined;
        if (macro != CLI_JSON_NONE)
        {
            defined = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx, .effect_read = effect,
                                 .at_value = effect->macro[macro].path);
        }
        else if (effect->first_found)
        {
            defined = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx, .effect_read = effect,
                                 .at_value = effect->first_path);
        }
        else
        {
            defined = NULL;
        }
        const unsigned int line = (macro != CLI_JSON_NONE) ? effect->macro[macro].line : effect->first_line;
        EMBED_CALL(effect->noted, CliEffectNotedCfg, .context = effect->noted_context, .read = read, .leaf = leaf,
                   .path = defined, .line = line);
    }
}

// the line of the branch's leaf `from` a needle of the walk names, from its first leaf to its last, put among the
// walk's texts with where it stands, the first such the walk found read as the name's first declaration. The branch's
// bytes are `bytes`
EMBED_INLINE void cli_effect_line_found(const SymbolsEffectCtx *args)
{
    CliEffect *effect = args->effect;
    const CliLeafRead *read = args->read;
    const unsigned int line = read->leaf[args->from].line;
    unsigned int last = args->from;
    while (((last + 1u) < read->count) && (read->leaf[last + 1u].line == line))
    {
        last += 1u;
    }
    const unsigned int first =
        EMBED_CALL(cli_effect_next, SymbolsEffectCtx, .read = read, .from = args->from, .to = last + 1u);
    const unsigned long long from = (first <= last) ? read->leaf[first].from : 0ULL;
    const unsigned int output =
        EMBED_CALL(cli_symbols_text_length_internal.effect_append, CliSymbolsTextLengthCtx, .effect = effect,
                   .text = (const char *)args->bytes + from, .length = read->leaf[last].to - from);
    effect->found_path = args->path_u32;
    effect->found_text = output;
    effect->found_line = line;
    effect->found = output != CLI_JSON_NONE;
    effect->first_path = effect->first_found ? effect->first_path : args->path_u32;
    effect->first_line = effect->first_found ? effect->first_line : line;
    effect->first_found = 1;
}

// the line of the branch's leaf `from`, which is no directive's, read: in an active region, each declaration on it met,
// `declared_at` moved past them whatever the region, and the line among the walk's texts for each needle naming it
EMBED_INLINE void cli_effect_line_met(const SymbolsEffectCtx *args)
{
    const CliEffectBranch *const branch = args->branch;
    const unsigned int line = args->read->leaf[args->from].line;
    const int active = (branch->open == 0u) || branch->condition[branch->open - 1u].active;
    unsigned int declared = *args->declared_at;
    for (; (declared < branch->declareds) && (branch->declared[declared].line == line); declared += 1u)
    {
        const CliEffectDeclared *const one = &branch->declared[declared];
        const CliEffectSeen seen = {CLI_SEEN_DECLARED, branch->read_at, line,     args->from, 0u,
                                    one->name,         one->length,     one->kind};
        if (active)
        {
            (void)EMBED_CALL(cli_effect_visit, SymbolsEffectCtx, .effect = args->effect, .seen = &seen);
        }
    }
    *args->declared_at = declared;
    for (unsigned int at = 0u; active && (at < branch->needles); at += 1u)
    {
        if (branch->needle[at] == line)
        {
            EMBED_CALL(cli_effect_line_found, SymbolsEffectCtx, .effect = args->effect, .read = args->read,
                       .from = args->from, .bytes = args->bytes, .path_u32 = args->path_u32);
        }
    }
}

// the leaves of the branch read in order to before line `upto`, a directive's line taken as one and every other line
// read, each handed where its lines go first and the walk ended there where it is refused
EMBED_INLINE void cli_effect_leaves_walk(const SymbolsEffectCtx *args)
{
    CliEffect *effect = args->effect;
    CliEffectBranch *const branch = args->branch;
    const CliLeafRead *const read = &branch->read;
    unsigned int leaf = 0u;
    unsigned int declared = 0u;
    while ((leaf < read->count) && (read->leaf[leaf].line < args->upto) && !effect->refused)
    {
        const unsigned int line = read->leaf[leaf].line;
        const unsigned int line_first = leaf;
        const int active_here = (branch->open == 0u) || branch->condition[branch->open - 1u].active;
        if ((args->depth == 0u) && (effect->reached != NULL) &&
            !EMBED_CALL(effect->reached, CliEffectReachedCfg, .context = effect->reached_context, .read = read,
                        .leaf = leaf, .active = active_here))
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
            leaf = EMBED_CALL(cli_effect_directive_end, SymbolsEffectCtx, .read = read, .role = role);
            EMBED_CALL(cli_effect_directive, SymbolsEffectCtx, .effect = effect, .branch = branch,
                       .path_u32 = args->path_u32, .role = role, .from = line_first, .end = leaf, .depth = args->depth);
        }
        else
        {
            EMBED_CALL(cli_effect_line_met, SymbolsEffectCtx, .effect = effect, .branch = branch, .read = read,
                       .from = leaf, .bytes = args->bytes, .path_u32 = args->path_u32, .declared_at = &declared);
            while ((leaf < read->count) && (read->leaf[leaf].line == line))
            {
                leaf += 1u;
            }
        }
        if ((args->depth == 0u) && active_here)
        {
            EMBED_CALL(cli_effect_noted_line, SymbolsEffectCtx, .effect = effect, .read = read, .first = line_first,
                       .end = leaf);
        }
    }
}

// the branch at `path` walked from its first line to before line `upto`, CLI_EFFECT_INPUT for all of it, once in
// a walk, its includes `depth` deep; for a slice's walk, its bytes and leaves stay in the walk's cell until the walk is
// given back, and each declaration its leaves name on a line of an active region is met; for a noting walk, each leaf
// of the name in the branch it starts from handed on as the walk passes its line; and each line of that branch handed
// where its lines go before the walk reads it, the walk ended there where it is refused
static void cli_effect_walk(const SymbolsEffectCtx *args)
{
    CliEffect *effect = args->effect;
    const char *path = args->path;
    if (effect->refused || EMBED_CALL(cli_effect_on_walk, SymbolsEffectCtx, .effect = effect, .path = path))
    {
        return;
    }
    const unsigned int path_at = effect->walked[effect->walks - 1u];
    CliEffectBranch *const branch = g_cli_guard->temporary_buf_alloc(sizeof(CliEffectBranch), g_cli_code_owner);
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = effect->root, .relative = path, .joined = full);
    // the branch the walk starts from is the editor's text where one is given, else every branch is the disk's
    const int from_disk = (effect->start_text == NULL) ||
                          !cellul.eq(&(CatenaFinitaCfg){.src = path, .other = effect->start, .cap = CLI_POOL_BYTES});
    unsigned long long size = from_disk ? 0ULL : effect->start_size;
    const unsigned char *bytes;
    if (branch == NULL)
    {
        bytes = NULL;
    }
    else if (from_disk)
    {
        bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = full, .size_pointer = &size,
                           .owner = g_cli_code_owner);
    }
    else
    {
        bytes = (const unsigned char *)effect->start_text;
    }
    if (bytes == NULL)
    {
        effect->refused |= branch == NULL;
        g_cli_guard->persistent_buf_release(branch, g_cli_code_owner);
        return;
    }
    branch->open = 0u;
    branch->read_at = CLI_JSON_NONE;
    EMBED_CALL(cli_effect_find, SymbolsEffectCtx, .effect = effect, .path = path, .branch = branch);
    const char *const text = (const char *)bytes;
    CliEffectDeclaring declaring = {effect, branch};
    const int declares = effect->slicing || (effect->noted != NULL);
    const int leaved =
        EMBED_CALL(cli_symbols_path_internal.leaves_declare, CliSymbolsPathCtx, .languages = effect->languages,
                   .path = path, .state = "root", .text = text, .size = size, .read = &branch->read,
                   .each = declares ? cli_effect_declaring : NULL, .context = declares ? &declaring : NULL);
    branch->read_at =
        (leaved && effect->slicing)
            ? EMBED_CALL(cli_effect_branch_read_append, SymbolsEffectCtx, .effect = effect, .path_u32 = path_at,
                         .bytes = bytes, .size = size, .from_disk = from_disk, .read = &branch->read)
            : CLI_JSON_NONE;
    if (leaved)
    {
        EMBED_CALL(cli_effect_leaves_walk, SymbolsEffectCtx, .effect = effect, .branch = branch, .path_u32 = path_at,
                   .bytes = bytes, .upto = args->upto, .depth = args->depth);
    }
    // a branch a slice's walk read whole stays in the walk's cell until the walk is given back
    if (branch->read_at == CLI_JSON_NONE)
    {
        g_cli_guard->persistent_buf_release(branch->read.leaf, g_cli_leaves_owner);
        branch->read.leaf = NULL;
        branch->read.count = 0u;
        branch->read.capacity = 0u;
        if (from_disk)
        {
            g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_code_owner));
        }
    }
    g_cli_guard->persistent_buf_release(branch->declared, g_cli_code_owner);
    g_cli_guard->persistent_buf_release(branch, g_cli_code_owner);
}

CliEffect *cli_effect_init(const CliSymbolsPathNameCtx *args)
{
    const char *name = args->name;
    int slicing = args->slicing;
    CliEffect *const effect = g_cli_guard->persistent_buf_alloc(sizeof(CliEffect), g_cli_code_owner);
    CliSymbolsLanguages *const languages = (effect != NULL) ? cli_symbols_languages_read() : NULL;
    // the walk's lists and texts in one cell, each at its most
    const unsigned long long branch_bytes = CLI_EFFECT_LIST_MAX * sizeof(CliEffectBranchRead);
    const unsigned long long macro_bytes = CLI_EFFECT_LIST_MAX * sizeof(CliEffectMacro);
    const unsigned long long seen_bytes = CLI_EFFECT_LIST_MAX * sizeof(CliEffectSeen);
    const unsigned long long leaf_bytes = CLI_EFFECT_VALUES_MAX * sizeof(CliEffectValueLeaf);
    const unsigned long long include_bytes = CLI_EFFECT_LIST_MAX * sizeof(CliEffectInclude);
    const unsigned long long walked_bytes = CLI_EFFECT_LIST_MAX * sizeof(unsigned int);
    const unsigned long long list_bytes =
        branch_bytes + macro_bytes + seen_bytes + leaf_bytes + include_bytes + walked_bytes;
    char *const cell = (languages != NULL)
                           ? g_cli_guard->temporary_buf_alloc(list_bytes + CLI_EFFECT_TEXT_CAPACITY, g_cli_code_owner)
                           : NULL;
    if (cell == NULL)
    {
        if (languages != NULL)
        {
            g_cli_guard->persistent_buf_release(languages->languages.pair, g_cli_plugins_owner);
            *(&languages->languages) = (CliLanguages){NULL, 0u, 0u};
        }
        g_cli_guard->persistent_buf_release(languages, g_cli_code_owner);
        g_cli_guard->persistent_buf_release(effect, g_cli_code_owner);
        mmgr_line_put(args->err_msg, "the cells cannot meet the walk");
        return NULL;
    }
    effect->text = (VerbaLinea){cell + list_bytes, CLI_EFFECT_TEXT_CAPACITY, 0ULL};
    effect->languages = languages;
    memor.cpy(&(MemoriaCfg){.dst = effect->root, .src = g_cli_tree_root, .bytes = CLI_PATH_MAX_LEN});
    effect->start = args->path;
    effect->start_text = NULL;
    effect->start_size = 0ULL;
    effect->branch_read = (CliEffectBranchRead *)(void *)cell;
    effect->branches_read = 0u;
    effect->branch_read_capacity = CLI_EFFECT_LIST_MAX;
    effect->macro = (CliEffectMacro *)(void *)(cell + branch_bytes);
    effect->macros = 0u;
    effect->macro_capacity = CLI_EFFECT_LIST_MAX;
    effect->seen = (CliEffectSeen *)(void *)(cell + branch_bytes + macro_bytes);
    effect->seens = 0u;
    effect->seen_capacity = CLI_EFFECT_LIST_MAX;
    effect->value_leaf = (CliEffectValueLeaf *)(void *)(cell + branch_bytes + macro_bytes + seen_bytes);
    effect->value_leaves = 0u;
    effect->value_leaf_capacity = CLI_EFFECT_VALUES_MAX;
    effect->include = (CliEffectInclude *)(void *)(cell + branch_bytes + macro_bytes + seen_bytes + leaf_bytes);
    effect->includes = 0u;
    effect->include_capacity = CLI_EFFECT_LIST_MAX;
    effect->walked =
        (unsigned int *)(void *)(cell + branch_bytes + macro_bytes + seen_bytes + leaf_bytes + include_bytes);
    effect->walks = 0u;
    effect->walk_capacity = CLI_EFFECT_LIST_MAX;
    effect->name = name;
    effect->length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES});
    effect->found = 0;
    effect->first_path = CLI_JSON_NONE;
    effect->first_line = 0u;
    effect->first_found = 0;
    effect->slicing = slicing;
    effect->noted = NULL;
    effect->noted_context = NULL;
    effect->reached = NULL;
    effect->reached_context = NULL;
    effect->refused = 0;
    return effect;
}

void cli_effect_alloc(const CliSymbolsPathEffectCtx *args)
{
    const char *path = args->path;
    EMBED_CALL(cli_compile_db_root.defines_get, CliCompileDbRootCfg, .root = args->effect->root, .path = path,
               .each_define_each = cli_effect_free, .context = args->effect);
    EMBED_CALL(cli_compile_db_root_path.includes_get, CliCompileDbRootPathCfg, .root = args->effect->root, .path = path,
               .each = cli_effect_include_intersect, .context = args->effect);
    // each file the branch includes before its first line walked before it, as its compiler reads them
    for (unsigned int at = 0u; at < args->effect->includes; at += 1u)
    {
        if (args->effect->include[at].kind == CLI_INCLUDE_FORCED)
        {
            EMBED_CALL(cli_effect_walk, SymbolsEffectCtx, .effect = args->effect,
                       .path = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx, .effect_read = args->effect,
                                          .at_value = args->effect->include[at].path),
                       .upto = CLI_EFFECT_INPUT, .depth = 1u);
        }
    }
    EMBED_CALL(cli_effect_walk, SymbolsEffectCtx, .effect = args->effect, .path = path, .upto = args->line_u32,
               .depth = 0u);
}

void cli_effect_input_back(const CliSymbolsTextLengthCtx *args)
{
    for (unsigned int at = 0u; at < args->effect->branches_read; at += 1u)
    {
        g_cli_guard->persistent_buf_release((&args->effect->branch_read[at].read)->leaf, g_cli_leaves_owner);
        (&args->effect->branch_read[at].read)->leaf = NULL;
        (&args->effect->branch_read[at].read)->count = 0u;
        (&args->effect->branch_read[at].read)->capacity = 0u;
        if (args->effect->branch_read[at].from_disk)
        {
            g_cli_guard->persistent_buf_release((void *)(uintptr_t)(args->effect->branch_read[at].bytes),
                                                (g_cli_code_owner));
        }
    }
    g_cli_guard->persistent_buf_release(args->effect->branch_read, g_cli_code_owner);
    if (args->effect->languages != NULL)
    {
        g_cli_guard->persistent_buf_release(args->effect->languages->languages.pair, g_cli_plugins_owner);
        *(&args->effect->languages->languages) = (CliLanguages){NULL, 0u, 0u};
    }
    g_cli_guard->persistent_buf_release(args->effect->languages, g_cli_code_owner);
    g_cli_guard->persistent_buf_release(args->effect, g_cli_code_owner);
}
