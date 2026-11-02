// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_signature.c: the signature of the call the cursor is in, from the tree: the call found off the leaves of the
// lines the page hands, its arguments and each one's type, every declaration of its name in the tree with its head
// read off its own file's leaves, each checked against the arguments, the best fit first. No language is read here but
// by its plugin's grammar
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

#define CLI_SYMBOLS_ARGUMENTS 32u
// The call the cursor is in: its name's leaf, its bracket's, the argument the cursor stands in, and each argument's
// first leaf, the leaf after its last, and its type
typedef struct
{
    unsigned int name;
    unsigned int open;
    unsigned int active;
    unsigned int arguments;
    unsigned int first[CLI_SYMBOLS_ARGUMENTS];
    unsigned int end[CLI_SYMBOLS_ARGUMENTS];
    CliSymbolType type[CLI_SYMBOLS_ARGUMENTS];
} CliSymbolCall;
// A signature worked out, in a cell of its own: the call, the heads of its name's declarations, how many, the places
// of those read in order of fit, how many, the tree's top, the path and line of a name's declaration sought, and the
// id of the language the call is read in
typedef struct
{
    CliSymbolCall call;
    CliSymbolHead head[CLI_SYMBOLS_HEADS];
    unsigned int heads;
    unsigned int order[CLI_SYMBOLS_HEADS];
    unsigned int ordered;
    char root[CLI_PATH_MAX_LEN];
    char needle[CLI_PATH_MAX_LEN];
    unsigned int needle_line;
    char language[CLI_NAME_MAX_LEN];
} CliSymbolSignature;
// how many lines above the cursor's the call may have opened on, the most arguments read, and the most bytes of an
// argument's text written
#define CLI_SYMBOLS_CALL_LINES 24u
#define CLI_SYMBOLS_ARGUMENT_TEXT 80ull

typedef struct
{
    const CliSymbolsLanguages *languages_read;
    const CliSymbolSignature *signature_read;
    const char *path;
    const char *text;
    unsigned long long size;
    unsigned int line;
    unsigned int col;
    const CliLeafRead *read;
    unsigned long long cursor;
    CliSymbolCall *call;
    unsigned int first;
    unsigned int end;
    CliSymbolsLanguages *languages;
    CliSymbolSignature *signature;
    const CliTreeBranch *branch;
    char *scratch;
    const char *name;
    unsigned long long length;
    CliSymbolType *type;
    unsigned int argument;
    VerbaLinea *out;
    CellOwner *owner;
} SymbolsSignatureCtx;
// whether the file at `path` opens in the language the call is read in
EMBED_INLINE int cli_signature_language_is(const SymbolsSignatureCtx *args)
{
    const CliSymbolsLanguages *languages = args->languages_read;
    const char *path = args->path;
    char id[CLI_NAME_MAX_LEN];
    return EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                      .languages_read = &languages->languages, .path = path, .id_out = id) &&
           cellul.eq(&(CatenaFinitaCfg){.src = id, .other = args->signature_read->language, .cap = CLI_CELLS_POOL});
}

// the byte of `size` bytes of `text` the place `line`, `col` stands at, `size` where the text has no such line
EMBED_INLINE unsigned long long cli_signature_cursor(const SymbolsSignatureCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int line = args->line;
    unsigned int col = args->col;
    unsigned long long start = 0ull;
    for (unsigned int at = 0u; at < line; at += 1u)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + start, .val = '\n', .bytes = size - start});
        if ((newline == NULL) || (start >= size))
        {
            return size;
        }
        // a newline found lies in the text, at or past `start`
        start = (unsigned long long)(newline - text) + 1ull;
    }
    unsigned long long end = start;
    end = (unsigned int)EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = end,
                                   .size = size, .set = "\n\r");
    unsigned long long byte = 0ull;
    unsigned int units = 0u;
    return start + EMBED_CALL(cli_symbols_size_internal.byte_at, CliSymbolsSizeCtx, .line = text + start,
                              .size = end - start, .col = col, .byte = &byte, .at = &units);
}

// the call the cursor at byte `cursor` of line `line` stands in, walked back over the leaves before it, each as its
// grammar names it: its bracket the first that opens at depth 0, a name named a call right before it, the separators at
// that depth counted: 1, or 0 where a bracket opens at depth 0 with no call before it, a statement ends at depth 0, or
// the lines' reach comes first
EMBED_INLINE int cli_signature_call(const SymbolsSignatureCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int leaf = 0u;
    while ((leaf < read->count) && (read->leaf[leaf].from < args->cursor))
    {
        leaf += 1u;
    }
    unsigned int depth = 0u;
    args->call->active = 0u;
    while (leaf > 0u)
    {
        leaf -= 1u;
        if ((read->leaf[leaf].line + CLI_SYMBOLS_CALL_LINES) <= args->line)
        {
            return 0;
        }
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_CLOSE))
        {
            depth += 1u;
        }
        else if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                            .roles = CLI_LEAF_OPEN) &&
                 (depth > 0u))
        {
            depth -= 1u;
        }
        else if ((depth == 0u) && EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                             .leaf = leaf, .roles = CLI_LEAF_STATEMENT))
        {
            return 0;
        }
        else if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                            .roles = CLI_LEAF_OPEN))
        {
            unsigned int name = leaf;
            while ((name > 0u) && ((read->leaf[name - 1u].is & CLI_LEAF_BLANK) != 0u))
            {
                name -= 1u;
            }
            if ((name == 0u) || !EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                            .leaf = name - 1u, .roles = CLI_LEAF_NAME | CLI_LEAF_CALL))
            {
                return 0;
            }
            args->call->name = name - 1u;
            args->call->open = leaf;
            return 1;
        }
        else if ((depth == 0u) && EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                             .leaf = leaf, .roles = CLI_LEAF_SEPARATOR))
        {
            args->call->active += 1u;
        }
    }
    return 0;
}

// the argument of the leaves from `first` to before `end` put after the call's others, the blank and comment leaves at
// either end of it left off
EMBED_INLINE void cli_signature_argument_append(const SymbolsSignatureCtx *args)
{
    unsigned int first = args->first;
    unsigned int end = args->end;
    const unsigned long long passed = CLI_LEAF_BLANK | CLI_LEAF_COMMENT;
    while ((first < end) && ((args->read->leaf[first].is & passed) != 0u))
    {
        first += 1u;
    }
    while ((end > first) && ((args->read->leaf[end - 1u].is & passed) != 0u))
    {
        end -= 1u;
    }
    if (args->call->arguments < CLI_SYMBOLS_ARGUMENTS)
    {
        args->call->first[args->call->arguments] = first;
        args->call->end[args->call->arguments] = end;
        args->call->arguments += 1u;
    }
}

// the call's arguments from its bracket on, each the leaves between separators at its depth, to the bracket that closes
// it or the last leaf of the cursor's line
EMBED_INLINE void cli_signature_arguments(const SymbolsSignatureCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int depth = 0u;
    unsigned int first = args->call->open + 1u;
    unsigned int leaf = args->call->open + 1u;
    args->call->arguments = 0u;
    for (; (leaf < read->count) && (read->leaf[leaf].line <= args->line); leaf += 1u)
    {
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_OPEN))
        {
            depth += 1u;
        }
        else if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                            .roles = CLI_LEAF_CLOSE) &&
                 (depth == 0u))
        {
            break;
        }
        else if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                            .roles = CLI_LEAF_CLOSE))
        {
            depth -= 1u;
        }
        else if ((depth == 0u) && EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                             .leaf = leaf, .roles = CLI_LEAF_SEPARATOR))
        {
            EMBED_CALL(cli_signature_argument_append, SymbolsSignatureCtx, .read = read, .call = args->call,
                       .first = first, .end = leaf);
            first = leaf + 1u;
        }
    }
    EMBED_CALL(cli_signature_argument_append, SymbolsSignatureCtx, .read = read, .call = args->call, .first = first,
               .end = leaf);
}

// whether a stored file is one of the call's language, its cell copied into `scratch` where it is
EMBED_INLINE int signature_file_read(const SymbolsSignatureCtx *args)
{
    const CliTreeBranch *branch = args->branch;
    char *scratch = args->scratch;
    if (!branch->used)
    {
        return 0;
    }
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_symbols_tree_branch_path.branch_path_read, CliSymbolsTreeBranchPathCfg, .branch = branch,
               .path_out = path);
    const int spoken = EMBED_CALL(cli_signature_language_is, SymbolsSignatureCtx, .languages_read = args->languages,
                                  .signature_read = args->signature, .path = path);
    if (spoken)
    {
        EMBED_CALL(cli_symbols_tree_branch.branch_copy, CliSymbolsTreeBranchCfg, .branch = branch, .scratch = scratch);
    }
    return spoken;
}

// the type of the tree's first declaration of `name`, `length` bytes, in a file of the call's language, read off the
// leaves of its own line
EMBED_INLINE void cli_signature_tree_type(const SymbolsSignatureCtx *args)
{
    CliSymbolsLanguages *languages = args->languages;
    unsigned long long length = args->length;
    CliSymbolType *type = args->type;
    int seen = 0;
    char *const scratch = cli_branch_copy_alloc();
    for (unsigned int file = 0u; !seen && (scratch != NULL) && (file < g_cli_branch_count); file += 1u)
    {
        const CliTreeBranch *const branch = &g_cli_branches[file];
        const int spoken = EMBED_CALL(signature_file_read, SymbolsSignatureCtx, .languages = languages,
                                      .signature = args->signature, .branch = branch, .scratch = scratch);
        for (unsigned int each = 0u; !seen && spoken && (each < branch->count); each += 1u)
        {
            const CliDeclaration *const symbol =
                &EMBED_CALL(cli_symbols_tree_branch_scratch.branch_copy_declarations, CliSymbolsTreeBranchScratchCfg,
                            .scratch_read = scratch)[each];
            seen = (symbol->length == length) &&
                   EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = scratch + symbol->name,
                              .other_pointer = args->name, .size = length);
            if (seen)
            {
                const char *const path =
                    EMBED_CALL(cli_symbols_tree_branch_scratch.branch_copy_path, CliSymbolsTreeBranchScratchCfg,
                               .branch = branch, .scratch_read = scratch);
                const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
                memor.cpy(&(MemoriaCfg){.dst = args->signature->needle, .src = path, .bytes = (size_t)size + 1u});
                args->signature->needle_line = symbol->line;
            }
        }
    }
    EMBED_CALL(cli_symbols_tree_branch.branch_copy_free, CliSymbolsTreeBranchCfg, .scratch = scratch);
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->signature->root, .relative = args->signature->needle,
               .joined = full);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = seen ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = full,
                                                         .size_pointer = &size, .owner = &g_cli_code_cells)
                                            : NULL;
    if (bytes == NULL)
    {
        return;
    }
    const char *const text = (const char *)bytes;
    unsigned long long start = 0ull;
    for (unsigned int at = 0u; (at < args->signature->needle_line) && (start < size); at += 1u)
    {
        start = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = start, .size = size).next;
    }
    unsigned long long end = start;
    end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = end, .size = size,
                     .set = "\n");
    CliLeafRead read;
    if (EMBED_CALL(cli_symbols_path_internal.leaves_save, CliSymbolsPathCtx, .languages = languages,
                   .path = args->signature->needle, .state = "root", .text = text + start, .size = end - start,
                   .read = &read))
    {
        for (unsigned int leaf = 0u; leaf < read.count; leaf += 1u)
        {
            unsigned long long size_of = 0ull;
            const char *const named = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                                 .read_read = &read, .leaf = leaf, .length_pointer = &size_of);
            if (((read.leaf[leaf].is & CLI_LEAF_DECLARES) != 0u) && (size_of == length) &&
                EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = named, .other_pointer = args->name,
                           .size = length))
            {
                EMBED_CALL(cli_symbols_read_leaf_type_internal.type_get, CliSymbolsReadLeafTypeCtx, .read_read = &read,
                           .leaf = leaf, .type = type);
                break;
            }
        }
    }
    EMBED_CALL(cli_symbols_text_internal.leaves_free, CliSymbolsTextCtx, .read = &read);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_code_cells);
}

// the type of the argument at `argument`: a literal's where every leaf of it is a number's or every one a string's,
// and for one name its declaration's, the last parameter or local of that name before the call among the leaves
// handed, else the tree's; none for any other
EMBED_INLINE void cli_signature_type(const SymbolsSignatureCtx *args)
{
    const CliLeafRead *read = args->read;
    CliSymbolCall *const call = &args->signature->call;
    CliSymbolType *const type = &call->type[args->argument];
    type->text[0] = '\0';
    type->is = 0ull;
    const unsigned int first = call->first[args->argument];
    const unsigned int last = call->end[args->argument];
    unsigned long long is = CLI_LEAF_NUMBER | CLI_LEAF_STRING;
    for (unsigned int leaf = first; leaf < last; leaf += 1u)
    {
        is &= read->leaf[leaf].is;
    }
    if ((first < last) && ((is & (CLI_LEAF_NUMBER | CLI_LEAF_STRING)) != 0u))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = type->text, .src = ((is & CLI_LEAF_NUMBER) != 0u) ? "number" : "string", .bytes = 7u});
        type->is = is & (CLI_LEAF_NUMBER | CLI_LEAF_STRING);
        return;
    }
    if (((last - first) != 1u) || ((read->leaf[first].is & CLI_LEAF_NAME) == 0u))
    {
        return;
    }
    unsigned long long length = 0ull;
    const char *const name = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                        .read_read = read, .leaf = first, .length_pointer = &length);
    for (unsigned int leaf = call->name; leaf > 0u; leaf -= 1u)
    {
        unsigned long long size = 0ull;
        const char *const text = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                            .read_read = read, .leaf = leaf - 1u, .length_pointer = &size);
        if (((read->leaf[leaf - 1u].is & (CLI_LEAF_PARAMETER | CLI_LEAF_LOCAL)) != 0u) && (size == length) &&
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = text, .other_pointer = name, .size = length))
        {
            EMBED_CALL(cli_symbols_read_leaf_type_internal.type_get, CliSymbolsReadLeafTypeCtx, .read_read = read,
                       .leaf = leaf - 1u, .type = type);
            return;
        }
    }
    EMBED_CALL(cli_signature_tree_type, SymbolsSignatureCtx, .languages = args->languages, .signature = args->signature,
               .name = name, .length = length, .type = type);
}

// the heads of every declaration of the call's name, `length` bytes, in the tree's files of the call's language,
// CLI_SYMBOLS_HEADS at most, each matched against the call's arguments, and the places of those read put in order of
// fit: those whose count fits first, then those with fewer arguments that differ
EMBED_INLINE void cli_signature_heads(const SymbolsSignatureCtx *args)
{
    CliSymbolsLanguages *languages = args->languages;
    const char *name = args->name;
    unsigned long long length = args->length;
    args->signature->heads = 0u;
    char *const scratch = cli_branch_copy_alloc();
    for (unsigned int file = 0u; (scratch != NULL) && (file < g_cli_branch_count); file += 1u)
    {
        const CliTreeBranch *const branch = &g_cli_branches[file];
        const int spoken = EMBED_CALL(signature_file_read, SymbolsSignatureCtx, .languages = languages,
                                      .signature = args->signature, .branch = branch, .scratch = scratch);
        for (unsigned int each = 0u; spoken && (each < branch->count); each += 1u)
        {
            const CliDeclaration *const symbol =
                &EMBED_CALL(cli_symbols_tree_branch_scratch.branch_copy_declarations, CliSymbolsTreeBranchScratchCfg,
                            .scratch_read = scratch)[each];
            if ((args->signature->heads < CLI_SYMBOLS_HEADS) && (symbol->length == length) &&
                EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = scratch + symbol->name,
                           .other_pointer = name, .size = length))
            {
                CliSymbolHead *const head = &args->signature->head[args->signature->heads];
                const char *const path =
                    EMBED_CALL(cli_symbols_tree_branch_scratch.branch_copy_path, CliSymbolsTreeBranchScratchCfg,
                               .branch = branch, .scratch_read = scratch);
                const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
                memor.cpy(&(MemoriaCfg){.dst = head->path, .src = path, .bytes = (size_t)size + 1u});
                head->line = symbol->line;
                args->signature->heads += 1u;
            }
        }
    }
    EMBED_CALL(cli_symbols_tree_branch.branch_copy_free, CliSymbolsTreeBranchCfg, .scratch = scratch);
    const CliSymbolCall *const call = &args->signature->call;
    unsigned int supplied = 0u;
    for (unsigned int argument = 0u; argument < call->arguments; argument += 1u)
    {
        supplied += (call->end[argument] > call->first[argument]) ? 1u : 0u;
    }
    args->signature->ordered = 0u;
    for (unsigned int at = 0u; at < args->signature->heads; at += 1u)
    {
        CliSymbolHead *const head = &args->signature->head[at];
        if (!EMBED_CALL(cli_symbols_languages_internal.head_read, CliSymbolsLanguagesCtx, .languages = languages,
                        .root = args->signature->root, .name = name, .length = length, .head = head))
        {
            continue;
        }
        head->differing = 0u;
        for (unsigned int argument = 0u; (argument < call->arguments) && (argument < head->params); argument += 1u)
        {
            if (EMBED_CALL(cli_symbols_argument_internal.type_fits, CliSymbolsArgumentCtx,
                           .argument = &call->type[argument], .parameter = &head->type[argument]) == 0)
            {
                head->differs[head->differing] = argument;
                head->differing += 1u;
            }
        }
        const int reached = (call->active < head->params) || ((head->params == 0u) && (call->active == 0u));
        head->counts = head->variadic || ((supplied <= head->params) && reached);
        // its place put among those of the heads read before it, after each that fits as well or better
        unsigned int place = args->signature->ordered;
        while (place > 0u)
        {
            const CliSymbolHead *const before = &args->signature->head[args->signature->order[place - 1u]];
            const int better = (head->counts && !before->counts) ||
                               ((head->counts == before->counts) && (head->differing < before->differing));
            if (!better)
            {
                break;
            }
            args->signature->order[place] = args->signature->order[place - 1u];
            place -= 1u;
        }
        args->signature->order[place] = at;
        args->signature->ordered += 1u;
    }
}

// the first declaration the call may be of as a panel in one cell of `owner`, with the call's parameter and those the
// arguments differ from: the cell, or NULL where no declaration is known or the cells cannot meet it
EMBED_INLINE CliSignaturePanel *cli_signature_panel_alloc(const SymbolsSignatureCtx *args)
{
    if (args->signature_read->ordered == 0u)
    {
        return NULL;
    }
    const CliSymbolHead *const head = &args->signature_read->head[args->signature_read->order[0]];
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = head->label, .cap = sizeof(head->label)});
    CliSignaturePanel *const panel =
        g_cli_cells->persistent_buf_alloc(sizeof(*panel) + (head->params * sizeof(CliSignatureParameter)) +
                                              (head->differing * sizeof(*panel->differs)) + length + 1ull,
                                          args->owner);
    if (panel == NULL)
    {
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = panel, .val = 0, .bytes = sizeof(*panel)});
    panel->parameter = (CliSignatureParameter *)(void *)(panel + 1);
    panel->differs = (unsigned int *)(void *)(panel->parameter + head->params);
    char *const text = (char *)(void *)(panel->differs + head->differing);
    memor.cpy(&(MemoriaCfg){.dst = text, .src = head->label, .bytes = length});
    text[length] = '\0';
    panel->label = text;
    panel->length = length;
    panel->doc = text + length;
    panel->parameters = head->params;
    for (unsigned int param = 0u; param < head->params; param += 1u)
    {
        panel->parameter[param] = (CliSignatureParameter){head->span[param][0], head->span[param][1]};
    }
    panel->differing = head->differing;
    for (unsigned int differs = 0u; differs < head->differing; differs += 1u)
    {
        panel->differs[differs] = head->differs[differs];
    }
    const unsigned int active = args->signature_read->call.active;
    // a parameter's place is far inside what an int represents
    panel->active = (active < head->params) ? (int)active : -1;
    return panel;
}

CliSignaturePanel *cli_symbols_signature_alloc(const CliSymbolsPathTextSizeOutCfg *args)
{
    const char *path = args->path;
    CliSymbolsLanguages *const languages = cli_symbols_languages_for_call();
    CliSymbolSignature *const signature =
        (languages != NULL) ? g_cli_cells->temporary_buf_alloc(sizeof(CliSymbolSignature), &g_cli_code_cells) : NULL;
    if (signature == NULL)
    {
        EMBED_CALL(cli_symbols.languages_free, CliSymbolsCfg, .read = languages);
        mmgr_line_put(args->err_msg, "the cells cannot meet a signature's reading");
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = signature->root, .src = g_cli_tree_root, .bytes = CLI_PATH_MAX_LEN});
    signature->language[0] = '\0';
    (void)EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                     .languages_read = &languages->languages, .path = path, .id_out = signature->language);
    CliLeafRead read;
    const int saved =
        EMBED_CALL(cli_symbols_path_internal.leaves_save, CliSymbolsPathCtx, .languages = languages, .path = path,
                   .state = args->state, .text = args->text, .size = args->size, .read = &read);
    CliSymbolCall *const call = &signature->call;
    CliSignaturePanel *panel = NULL;
    const int called =
        saved && EMBED_CALL(cli_signature_call, SymbolsSignatureCtx, .read = &read,
                            .cursor = EMBED_CALL(cli_signature_cursor, SymbolsSignatureCtx, .text = args->text,
                                                 .size = args->size, .line = args->line, .col = args->col),
                            .line = args->line, .call = call);
    if (called)
    {
        EMBED_CALL(cli_signature_arguments, SymbolsSignatureCtx, .read = &read, .line = args->line, .call = call);
        for (unsigned int argument = 0u; argument < call->arguments; argument += 1u)
        {
            EMBED_CALL(cli_signature_type, SymbolsSignatureCtx, .languages = languages, .read = &read,
                       .signature = signature, .argument = argument);
        }
        unsigned long long length = 0ull;
        const char *const name = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                            .read_read = &read, .leaf = call->name, .length_pointer = &length);
        EMBED_CALL(cli_signature_heads, SymbolsSignatureCtx, .languages = languages, .signature = signature,
                   .name = name, .length = length);
        panel = EMBED_CALL(cli_signature_panel_alloc, SymbolsSignatureCtx, .signature_read = signature,
                           .owner = args->owner);
    }
    const int refused = read.refused;
    EMBED_CALL(cli_symbols_text_internal.leaves_free, CliSymbolsTextCtx, .read = &read);
    g_cli_cells->persistent_buf_release(signature, &g_cli_code_cells);
    EMBED_CALL(cli_symbols.languages_free, CliSymbolsCfg, .read = languages);
    if (refused)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the leaves of the lines handed");
    }
    return panel;
}
