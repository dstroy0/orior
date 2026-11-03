// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_heads.c: types and heads read off leaves: a declaration's type from the leaves about its name, whether an
// argument's type fits a parameter's, and a declaration's head from its own file, each parameter's span and type. No
// language is read here but by its plugin's grammar
#include "../command_line/cells.h"
#include "../text/text.h"
#include "symbols_internal.h"

typedef struct
{
    const CliLeafRead *read;
    unsigned int leaf;
    int spaced;
    CliSymbolType *type;
    unsigned long long *at;
    unsigned int from;
    unsigned int to;
    unsigned int name;
    const char *one;
    const char *other;
    unsigned int first;
    unsigned int end;
    CliSymbolHead *head;
    unsigned int close;
    const char *name_text;
    unsigned long long length;
} SymbolsHeadsCtx;

// whether the leaf is a bracket within a type, as its grammar names it: one that opens, 1, one that closes, -1, or
// neither, 0; a region's bracket and a parameter list's are none
EMBED_INLINE int cli_type_bracket(const SymbolsHeadsCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int leaf = args->leaf;
    const unsigned long long is = read->leaf[leaf].is;
    if ((is & (CLI_LEAF_REGION | CLI_LEAF_PARAMETER)) != 0u)
    {
        return 0;
    }
    if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                   .roles = CLI_LEAF_OPEN))
    {
        return 1;
    }
    if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                   .roles = CLI_LEAF_CLOSE))
    {
        return -1;
    }
    return 0;
}

// whether the leaf bounds a type at the depth it starts at, as its grammar names it: a separator, a statement's end,
// an assignment, a region's bracket or a parameter list's
EMBED_INLINE int cli_type_bind(const SymbolsHeadsCtx *args)
{
    const unsigned long long bounds = CLI_LEAF_SEPARATOR | CLI_LEAF_STATEMENT | CLI_LEAF_ASSIGN | CLI_LEAF_REGION;
    const unsigned long long is = args->read->leaf[args->leaf].is;
    const int parameters = ((is & CLI_LEAF_PARAMETER) != 0u) && ((is & (CLI_LEAF_OPEN | CLI_LEAF_CLOSE)) != 0u);
    return (((is & bounds) != 0u) || parameters) && ((is & (CLI_LEAF_STRING | CLI_LEAF_COMMENT)) == 0u);
}

// the leaf put after the type's text at `*at`, a space before it where it and the one put before it both name types
EMBED_INLINE void cli_type_append(const SymbolsHeadsCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int leaf = args->leaf;
    unsigned long long *at = args->at;
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                        .read_read = read, .leaf = leaf, .length_pointer = &length);
    if (args->spaced && ((*at + 1ULL) < CLI_SYMBOLS_TYPE))
    {
        args->type->text[*at] = ' ';
        *at += 1ULL;
    }
    const unsigned long long capacity = CLI_SYMBOLS_TYPE - 1ULL - *at;
    const unsigned long long copied = (length < capacity) ? length : capacity;
    // a type's text is under CLI_SYMBOLS_TYPE bytes, which a size_t holds
    memor.cpy(&(MemoriaCfg){.dst = args->type->text + *at, .src = text, .bytes = (size_t)copied});
    *at += copied;
    args->type->text[*at] = '\0';
    args->type->is |= read->leaf[leaf].is & (CLI_LEAF_OF_NUMBER | CLI_LEAF_OF_STRING | CLI_LEAF_MEMORY);
}

// the type of the leaves from `from` to before `to`, the leaf `name` passed over: those that name a type or a pointer,
// and the brackets and separators within a type's own brackets
EMBED_INLINE void cli_type_between(const SymbolsHeadsCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned long long at = 0ULL;
    unsigned int depth = 0u;
    int named = 0;
    args->type->text[0] = '\0';
    args->type->is = 0ULL;
    for (unsigned int leaf = args->from; leaf < args->to; leaf += 1u)
    {
        const unsigned long long is = read->leaf[leaf].is;
        const int bracket = EMBED_CALL(cli_type_bracket, SymbolsHeadsCtx, .read = read, .leaf = leaf);
        const int opens = bracket > 0;
        const int closes = bracket < 0;
        const int inside =
            (depth > 0u) && (closes || EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx,
                                                  .read_read = read, .leaf = leaf, .roles = CLI_LEAF_SEPARATOR));
        if ((leaf == args->name) || (((is & (CLI_LEAF_TYPE | CLI_LEAF_MEMORY)) == 0u) && !opens && !inside))
        {
            continue;
        }
        if (opens)
        {
            depth = depth + 1u;
        }
        else if (closes && (depth > 0u))
        {
            depth = depth - 1u;
        }
        const int names = (is & CLI_LEAF_TYPE) != 0u;
        EMBED_CALL(cli_type_append, SymbolsHeadsCtx, .read = read, .leaf = leaf, .spaced = named && names,
                   .type = args->type, .at = &at);
        named = names;
    }
}

// the literal of the leaves from `from` on its line to a bracket, a separator, a statement's end or the line's end,
// past one assignment first: every leaf of it a number's or every one a string's, as `type`
EMBED_INLINE void cli_type_literal(const SymbolsHeadsCtx *args)
{
    const CliLeafRead *read = args->read;
    const unsigned int line = (args->from < read->count) ? read->leaf[args->from].line : 0u;
    const unsigned long long ends = CLI_LEAF_OPEN | CLI_LEAF_CLOSE | CLI_LEAF_SEPARATOR | CLI_LEAF_STATEMENT;
    unsigned int leaf = args->from;
    unsigned long long is = CLI_LEAF_NUMBER | CLI_LEAF_STRING;
    int seen = 0;
    int passed = 0;
    for (; (leaf < read->count) && (read->leaf[leaf].line == line) && ((read->leaf[leaf].is & ends) == 0u); leaf += 1u)
    {
        const unsigned long long one = read->leaf[leaf].is;
        if ((one & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u)
        {
            continue;
        }
        if (!seen && !passed &&
            EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_ASSIGN))
        {
            passed = 1;
            continue;
        }
        is &= one;
        seen = 1;
    }
    if (seen && ((is & (CLI_LEAF_NUMBER | CLI_LEAF_STRING)) != 0u))
    {
        const char *const kind = ((is & CLI_LEAF_NUMBER) != 0u) ? "number" : "string";
        memor.cpy(&(MemoriaCfg){.dst = args->type->text, .src = kind, .bytes = 7u});
        args->type->is = is & (CLI_LEAF_NUMBER | CLI_LEAF_STRING);
    }
}

void cli_symbols_type_get(const CliSymbolsReadLeafTypeCtx *args)
{
    const unsigned int line = args->read_read->leaf[args->leaf].line;
    unsigned int from = args->leaf;
    unsigned int depth = 0u;
    while ((from > 0u) && (args->read_read->leaf[from - 1u].line == line))
    {
        const unsigned int before = from - 1u;
        const int bracket = EMBED_CALL(cli_type_bracket, SymbolsHeadsCtx, .read = args->read_read, .leaf = before);
        if (bracket < 0)
        {
            depth += 1u;
        }
        else if ((depth > 0u) && (bracket > 0))
        {
            depth -= 1u;
        }
        else if ((depth == 0u) &&
                 ((bracket > 0) || EMBED_CALL(cli_type_bind, SymbolsHeadsCtx, .read = args->read_read, .leaf = before)))
        {
            break;
        }
        from = before;
    }
    unsigned int to = args->leaf + 1u;
    depth = 0u;
    while ((to < args->read_read->count) && (args->read_read->leaf[to].line == line))
    {
        const int bracket = EMBED_CALL(cli_type_bracket, SymbolsHeadsCtx, .read = args->read_read, .leaf = to);
        if (bracket > 0)
        {
            depth += 1u;
        }
        else if ((depth > 0u) && (bracket < 0))
        {
            depth -= 1u;
        }
        else if ((depth == 0u) &&
                 ((bracket < 0) || EMBED_CALL(cli_type_bind, SymbolsHeadsCtx, .read = args->read_read, .leaf = to)))
        {
            break;
        }
        to += 1u;
    }
    EMBED_CALL(cli_type_between, SymbolsHeadsCtx, .read = args->read_read, .from = from, .to = to, .name = args->leaf,
               .type = args->type);
    if (args->type->text[0] == '\0')
    {
        EMBED_CALL(cli_type_literal, SymbolsHeadsCtx, .read = args->read_read, .from = args->leaf + 1u,
                   .type = args->type);
    }
}

// whether two types' texts are the same with their spaces set aside
EMBED_INLINE int cli_types_same(const SymbolsHeadsCtx *args)
{
    unsigned int left = 0u;
    unsigned int right = 0u;
    for (;;)
    {
        left = (unsigned int)EMBED_CALL(
            cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->one, .at = left,
            .size = left + cellul.len(&(CatenaFinitaCfg){.src = args->one + left, .cap = CLI_CELLS_POOL}), .set = " ");
        right = (unsigned int)EMBED_CALL(
            cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->other, .at = right,
            .size = right + cellul.len(&(CatenaFinitaCfg){.src = args->other + right, .cap = CLI_CELLS_POOL}),
            .set = " ");
        if (args->one[left] != args->other[right])
        {
            return 0;
        }
        if (args->one[left] == '\0')
        {
            return 1;
        }
        left += 1u;
        right += 1u;
    }
}

int cli_symbols_type_fits(const CliSymbolsArgumentCtx *args)
{
    if ((args->argument->text[0] == '\0') || (args->parameter->text[0] == '\0'))
    {
        return -1;
    }
    const unsigned long long numbers = CLI_LEAF_NUMBER | CLI_LEAF_OF_NUMBER;
    const unsigned long long strings = CLI_LEAF_STRING | CLI_LEAF_OF_STRING;
    const int pointer = (args->parameter->is & CLI_LEAF_MEMORY) != 0u;
    if ((args->argument->is & CLI_LEAF_NUMBER) != 0u)
    {
        return ((args->parameter->is & numbers) != 0u) && !pointer;
    }
    if ((args->argument->is & CLI_LEAF_STRING) != 0u)
    {
        return (args->parameter->is & strings) != 0u;
    }
    // a parameter typed by its default's literal alone takes an argument of a type of the same kind
    if ((args->parameter->is & CLI_LEAF_NUMBER) != 0u)
    {
        return ((args->argument->is & numbers) != 0u) && ((args->argument->is & CLI_LEAF_MEMORY) == 0u);
    }
    if ((args->parameter->is & CLI_LEAF_STRING) != 0u)
    {
        return (args->argument->is & strings) != 0u;
    }
    return EMBED_CALL(cli_types_same, SymbolsHeadsCtx, .one = args->argument->text, .other = args->parameter->text);
}

// the parameter of the leaves from `first` to before `end` put after the head's others: its span, from its first
// leaf's first byte to its last leaf's last, its name's span, the leaf its grammar names a parameter, and its type.
// One holding a leaf its grammar names `rest` takes more after the others and is not counted; one of no name and no
// type is none
EMBED_INLINE void cli_head_param(const SymbolsHeadsCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int first = args->first;
    const unsigned long long passed = CLI_LEAF_BLANK | CLI_LEAF_COMMENT;
    while ((first < args->end) && ((read->leaf[first].is & passed) != 0u))
    {
        first += 1u;
    }
    unsigned int last = args->end;
    while ((last > first) && ((read->leaf[last - 1u].is & passed) != 0u))
    {
        last -= 1u;
    }
    if ((first == last) || (args->head->params == CLI_SYMBOLS_PARAMS))
    {
        return;
    }
    for (unsigned int leaf = first; leaf < last; leaf += 1u)
    {
        if ((read->leaf[leaf].is & CLI_LEAF_REMAINDER) != 0u)
        {
            args->head->variadic = 1;
            return;
        }
    }
    unsigned int name = first;
    while ((name < last) && ((read->leaf[name].is & CLI_LEAF_PARAMETER) == 0u))
    {
        name += 1u;
    }
    CliSymbolType *const type = &args->head->type[args->head->params];
    if (name < last)
    {
        EMBED_CALL(cli_symbols_read_leaf_type_internal.type_get, CliSymbolsReadLeafTypeCtx, .read_read = read,
                   .leaf = name, .type = type);
    }
    else
    {
        EMBED_CALL(cli_type_between, SymbolsHeadsCtx, .read = read, .from = first, .to = last, .name = last,
                   .type = type);
    }
    if ((name == last) && (type->text[0] == '\0'))
    {
        return;
    }
    // a head is read from no more than CLI_SYMBOLS_HEAD_LINES lines, far under 4 GiB
    args->head->span[args->head->params][0] = (unsigned int)read->leaf[first].from;
    args->head->span[args->head->params][1] = (unsigned int)read->leaf[last - 1u].to;
    args->head->named[args->head->params][0] =
        (name < last) ? (unsigned int)read->leaf[name].from : args->head->span[args->head->params][0];
    args->head->named[args->head->params][1] =
        (name < last) ? (unsigned int)read->leaf[name].to : args->head->span[args->head->params][0];
    args->head->params += 1u;
}

// the head's label from its first line's first letter to the end of the bracket at `close`, each line break and the
// white space after it one space, and each parameter's span and its name's, stored as their bytes, made the label's in
// UTF-16 units
EMBED_INLINE void cli_head_label(const SymbolsHeadsCtx *args)
{
    unsigned int bytes[CLI_SYMBOLS_PARAMS][2];
    unsigned int name_bytes[CLI_SYMBOLS_PARAMS][2];
    memor.cpy(&(MemoriaCfg){.dst = bytes, .src = args->head->span, .bytes = sizeof(bytes)});
    memor.cpy(&(MemoriaCfg){.dst = name_bytes, .src = args->head->named, .bytes = sizeof(name_bytes)});
    const char *const text = args->read->text;
    const unsigned long long to = args->read->leaf[args->close].to;
    unsigned long long byte = 0ULL;
    byte = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = byte, .size = to,
                      .set = " \t");
    unsigned int at = 0u;
    unsigned int units = 0u;
    while ((byte < to) && ((at + 5u) < CLI_SYMBOLS_LABEL))
    {
        for (unsigned int param = 0u; param < args->head->params; param += 1u)
        {
            args->head->span[param][0] = (bytes[param][0] == byte) ? units : args->head->span[param][0];
            args->head->span[param][1] = (bytes[param][1] == byte) ? units : args->head->span[param][1];
            args->head->named[param][0] = (name_bytes[param][0] == byte) ? units : args->head->named[param][0];
            args->head->named[param][1] = (name_bytes[param][1] == byte) ? units : args->head->named[param][1];
        }
        if ((text[byte] == '\r') || (text[byte] == '\n'))
        {
            const unsigned long long broken = byte;
            byte = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = byte,
                              .size = to, .set = " \t\r\n");
            // a span that ends within the white space ends before its one space
            for (unsigned int param = 0u; param < args->head->params; param += 1u)
            {
                const int within = (bytes[param][1] > broken) && (bytes[param][1] < byte);
                args->head->span[param][1] = within ? units : args->head->span[param][1];
            }
            args->head->label[at] = ' ';
            at += 1u;
            units += 1u;
            continue;
        }
        const unsigned char lead = (unsigned char)text[byte];
        unsigned int length;
        if (lead < 0x80u)
        {
            length = 1u;
        }
        else if (lead < 0xe0u)
        {
            length = 2u;
        }
        else if (lead < 0xf0u)
        {
            length = 3u;
        }
        else
        {
            length = 4u;
        }
        unsigned int more = 0u;
        while ((more < length) && (byte < to))
        {
            args->head->label[at] = text[byte];
            at += 1u;
            byte += 1ULL;
            more += 1u;
        }
        units += (length == 4u) ? 2u : 1u;
    }
    args->head->label[at] = '\0';
    for (unsigned int param = 0u; param < args->head->params; param += 1u)
    {
        args->head->span[param][1] = (bytes[param][1] >= byte) ? units : args->head->span[param][1];
        args->head->named[param][1] = (name_bytes[param][1] >= byte) ? units : args->head->named[param][1];
    }
}

// the head of the declaration of `name`, `length` bytes, on the first line of the leaves read: its parameters, each
// the leaves between separators at its bracket's depth, to the bracket that closes it, and its label: 1, or 0 where no
// leaf on that line declares it or no bracket opens after it before a region's bracket, a statement's end or an
// assignment, or its bracket is not closed; each as its grammar names it
EMBED_INLINE int cli_head_params(const SymbolsHeadsCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned long long length = args->length;
    unsigned int at = 0u;
    for (; (at < read->count) && (read->leaf[at].line == 0u); at += 1u)
    {
        unsigned long long size = 0ULL;
        const char *const text = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                            .read_read = read, .leaf = at, .length_pointer = &size);
        if (((read->leaf[at].is & CLI_LEAF_DECLARES) != 0u) && (size == length) &&
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = text, .other_pointer = args->name_text,
                       .size = size))
        {
            break;
        }
    }
    if ((at == read->count) || (read->leaf[at].line != 0u))
    {
        return 0;
    }
    unsigned int open = at + 1u;
    while ((open < read->count) && !EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                               .leaf = open, .roles = CLI_LEAF_OPEN))
    {
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = open,
                       .roles = CLI_LEAF_STATEMENT) ||
            EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = open,
                       .roles = CLI_LEAF_ASSIGN))
        {
            return 0;
        }
        open += 1u;
    }
    if ((open < read->count) && EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                           .leaf = open, .roles = CLI_LEAF_REGION))
    {
        return 0;
    }
    args->head->params = 0u;
    args->head->variadic = 0;
    unsigned int depth = 0u;
    unsigned int first = open + 1u;
    unsigned int close = read->count;
    for (unsigned int leaf = open + 1u; (leaf < read->count) && (close == read->count); leaf += 1u)
    {
        const int separates = (depth == 0u) && EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx,
                                                          .read_read = read, .leaf = leaf, .roles = CLI_LEAF_SEPARATOR);
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_OPEN))
        {
            depth += 1u;
        }
        else if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                            .roles = CLI_LEAF_CLOSE) &&
                 (depth > 0u))
        {
            depth -= 1u;
        }
        else if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                            .roles = CLI_LEAF_CLOSE) ||
                 separates)
        {
            EMBED_CALL(cli_head_param, SymbolsHeadsCtx, .read = read, .first = first, .end = leaf, .head = args->head);
            first = leaf + 1u;
            close = separates ? read->count : leaf;
        }
    }
    if ((open >= read->count) || (close == read->count))
    {
        return 0;
    }
    EMBED_CALL(cli_head_label, SymbolsHeadsCtx, .read = read, .close = close, .head = args->head);
    return 1;
}

int cli_symbols_head_read(const CliSymbolsLanguagesCtx *args)
{
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = args->head->path, .joined = full);
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = full,
                                                  .size_pointer = &size, .owner = &g_cli_code_cells);
    if (bytes == NULL)
    {
        return 0;
    }
    const char *const text = (const char *)bytes;
    unsigned long long start = 0ULL;
    unsigned int counted = 0u;
    while ((counted < args->head->line) && (start < size))
    {
        start = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = start, .size = size).next;
        counted += 1u;
    }
    unsigned long long end = start;
    unsigned int lines = 0u;
    while ((end < size) && (lines < CLI_SYMBOLS_HEAD_LINES))
    {
        end = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = end, .size = size).next;
        lines += 1u;
    }
    CliLeafRead read;
    const int saved =
        (counted == args->head->line) &&
        EMBED_CALL(cli_symbols_path_internal.leaves_save, CliSymbolsPathCtx, .languages = args->languages,
                   .path = args->head->path, .state = "root", .text = text + start, .size = end - start, .read = &read);
    const int found = saved && EMBED_CALL(cli_head_params, SymbolsHeadsCtx, .read = &read, .name_text = args->name,
                                          .length = args->length, .head = args->head);
    if (counted == args->head->line)
    {
        EMBED_CALL(cli_symbols_text_internal.leaves_free, CliSymbolsTextCtx, .read = &read);
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_code_cells);
    return found;
}
