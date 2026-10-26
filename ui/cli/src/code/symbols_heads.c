// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_heads.c: types and heads read off leaves: a declaration's type from the leaves about its name, whether an
// argument's type fits a parameter's, and a declaration's head from its own file, each parameter's span and type. No
// language is read here but by its plugin's grammar
#include "../command_line/cells.h"
#include "../text/text.h"
#include "symbols_internal.h"

// whether the leaf is a bracket within a type, as its grammar names it: one that opens, 1, one that closes, -1, or
// neither, 0; a region's bracket and a parameter list's are none
static int cli_type_bracket(const CliLeafRead *read, unsigned int leaf)
{
    const unsigned long long is = read->leaf[leaf].is;
    if ((is & (CLI_LEAF_REGION | CLI_LEAF_PARAMETER)) != 0u)
    {
        return 0;
    }
    return cli_leaf_has(read, leaf, CLI_LEAF_OPEN) ? 1 : (cli_leaf_has(read, leaf, CLI_LEAF_CLOSE) ? -1 : 0);
}

// whether the leaf bounds a type at the depth it starts at, as its grammar names it: a separator, a statement's end,
// an assignment, a region's bracket or a parameter list's
static int cli_type_bound(const CliLeafRead *read, unsigned int leaf)
{
    const unsigned long long bounds = CLI_LEAF_SEPARATOR | CLI_LEAF_STATEMENT | CLI_LEAF_ASSIGN | CLI_LEAF_REGION;
    const unsigned long long is = read->leaf[leaf].is;
    const int parameters = ((is & CLI_LEAF_PARAMETER) != 0u) && ((is & (CLI_LEAF_OPEN | CLI_LEAF_CLOSE)) != 0u);
    return (((is & bounds) != 0u) || parameters) && ((is & (CLI_LEAF_STRING | CLI_LEAF_COMMENT)) == 0u);
}

// the leaf put after the type's text at `*at`, a space before it where it and the one put before it both name types
static void cli_type_put(const CliLeafRead *read, unsigned int leaf, int spaced, CliSymbolType *type,
                         unsigned long long *at)
{
    unsigned long long length = 0ull;
    const char *const text = cli_leaf_text(read, leaf, &length);
    if (spaced && ((*at + 1ull) < CLI_SYMBOLS_TYPE))
    {
        type->text[*at] = ' ';
        *at += 1ull;
    }
    const unsigned long long room = CLI_SYMBOLS_TYPE - 1ull - *at;
    const unsigned long long copied = (length < room) ? length : room;
    // a type's text is under CLI_SYMBOLS_TYPE bytes, which a size_t holds
    memor.cpy(&(MemoriaCfg){.dst = type->text + *at, .src = text, .bytes = (size_t)copied});
    *at += copied;
    type->text[*at] = '\0';
    type->is |= read->leaf[leaf].is & (CLI_LEAF_OF_NUMBER | CLI_LEAF_OF_STRING | CLI_LEAF_MEMORY);
}

// the type of the leaves from `from` to before `to`, the leaf `name` passed over: those that name a type or a pointer,
// and the brackets and separators within a type's own brackets
static void cli_type_between(const CliLeafRead *read, unsigned int from, unsigned int to, unsigned int name,
                             CliSymbolType *type)
{
    unsigned long long at = 0ull;
    unsigned int depth = 0u;
    int named = 0;
    type->text[0] = '\0';
    type->is = 0ull;
    for (unsigned int leaf = from; leaf < to; leaf += 1u)
    {
        const unsigned long long is = read->leaf[leaf].is;
        const int bracket = cli_type_bracket(read, leaf);
        const int opens = bracket > 0;
        const int closes = bracket < 0;
        const int inside = (depth > 0u) && (closes || cli_leaf_has(read, leaf, CLI_LEAF_SEPARATOR));
        if ((leaf == name) || (((is & (CLI_LEAF_TYPE | CLI_LEAF_MEMORY)) == 0u) && !opens && !inside))
        {
            continue;
        }
        depth = opens ? (depth + 1u) : (closes && (depth > 0u)) ? (depth - 1u) : depth;
        const int names = (is & CLI_LEAF_TYPE) != 0u;
        cli_type_put(read, leaf, named && names, type, &at);
        named = names;
    }
}

// the literal of the leaves from `from` on its line to a bracket, a separator, a statement's end or the line's end,
// past one assignment first: every leaf of it a number's or every one a string's, as `type`
static void cli_type_literal(const CliLeafRead *read, unsigned int from, CliSymbolType *type)
{
    const unsigned int line = (from < read->count) ? read->leaf[from].line : 0u;
    const unsigned long long ends = CLI_LEAF_OPEN | CLI_LEAF_CLOSE | CLI_LEAF_SEPARATOR | CLI_LEAF_STATEMENT;
    unsigned int leaf = from;
    unsigned long long is = CLI_LEAF_NUMBER | CLI_LEAF_STRING;
    int seen = 0;
    int passed = 0;
    for (; (leaf < read->count) && (read->leaf[leaf].line == line) && ((read->leaf[leaf].is & ends) == 0u);
         leaf += 1u)
    {
        const unsigned long long one = read->leaf[leaf].is;
        if ((one & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u)
        {
            continue;
        }
        if (!seen && !passed && cli_leaf_has(read, leaf, CLI_LEAF_ASSIGN))
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
        memor.cpy(&(MemoriaCfg){.dst = type->text, .src = kind, .bytes = 7u});
        type->is = is & (CLI_LEAF_NUMBER | CLI_LEAF_STRING);
    }
}

void cli_symbols_type_of(const CliLeafRead *read, unsigned int leaf, CliSymbolType *type)
{
    const unsigned int line = read->leaf[leaf].line;
    unsigned int from = leaf;
    unsigned int depth = 0u;
    while ((from > 0u) && (read->leaf[from - 1u].line == line))
    {
        const unsigned int before = from - 1u;
        const int bracket = cli_type_bracket(read, before);
        if (bracket < 0)
        {
            depth += 1u;
        }
        else if ((depth > 0u) && (bracket > 0))
        {
            depth -= 1u;
        }
        else if ((depth == 0u) && ((bracket > 0) || cli_type_bound(read, before)))
        {
            break;
        }
        from = before;
    }
    unsigned int to = leaf + 1u;
    depth = 0u;
    while ((to < read->count) && (read->leaf[to].line == line))
    {
        const int bracket = cli_type_bracket(read, to);
        if (bracket > 0)
        {
            depth += 1u;
        }
        else if ((depth > 0u) && (bracket < 0))
        {
            depth -= 1u;
        }
        else if ((depth == 0u) && ((bracket < 0) || cli_type_bound(read, to)))
        {
            break;
        }
        to += 1u;
    }
    cli_type_between(read, from, to, leaf, type);
    if (type->text[0] == '\0')
    {
        cli_type_literal(read, leaf + 1u, type);
    }
}

// whether two types' texts are the same with their spaces set aside
static int cli_types_same(const char *one, const char *other)
{
    unsigned int left = 0u;
    unsigned int right = 0u;
    for (;;)
    {
        while (one[left] == ' ')
        {
            left += 1u;
        }
        while (other[right] == ' ')
        {
            right += 1u;
        }
        if (one[left] != other[right])
        {
            return 0;
        }
        if (one[left] == '\0')
        {
            return 1;
        }
        left += 1u;
        right += 1u;
    }
}

int cli_symbols_type_fits(const CliSymbolType *argument, const CliSymbolType *parameter)
{
    if ((argument->text[0] == '\0') || (parameter->text[0] == '\0'))
    {
        return -1;
    }
    const unsigned long long numbers = CLI_LEAF_NUMBER | CLI_LEAF_OF_NUMBER;
    const unsigned long long strings = CLI_LEAF_STRING | CLI_LEAF_OF_STRING;
    const int pointer = (parameter->is & CLI_LEAF_MEMORY) != 0u;
    if ((argument->is & CLI_LEAF_NUMBER) != 0u)
    {
        return ((parameter->is & numbers) != 0u) && !pointer;
    }
    if ((argument->is & CLI_LEAF_STRING) != 0u)
    {
        return (parameter->is & strings) != 0u;
    }
    // a parameter typed by its default's literal alone takes an argument of a type of the same kind
    if ((parameter->is & CLI_LEAF_NUMBER) != 0u)
    {
        return ((argument->is & numbers) != 0u) && ((argument->is & CLI_LEAF_MEMORY) == 0u);
    }
    if ((parameter->is & CLI_LEAF_STRING) != 0u)
    {
        return (argument->is & strings) != 0u;
    }
    return cli_types_same(argument->text, parameter->text);
}

// the parameter of the leaves from `first` to before `end` put after the head's others: its span, from its first
// leaf's first byte to its last leaf's last, its name's span, the leaf its grammar names a parameter, and its type.
// One holding a leaf its grammar names `rest` takes more after the others and is not counted; one of no name and no
// type is none
static void cli_head_param(const CliLeafRead *read, unsigned int first, unsigned int end, CliSymbolHead *head)
{
    const unsigned long long passed = CLI_LEAF_BLANK | CLI_LEAF_COMMENT;
    while ((first < end) && ((read->leaf[first].is & passed) != 0u))
    {
        first += 1u;
    }
    unsigned int last = end;
    while ((last > first) && ((read->leaf[last - 1u].is & passed) != 0u))
    {
        last -= 1u;
    }
    if ((first == last) || (head->params == CLI_SYMBOLS_PARAMS))
    {
        return;
    }
    for (unsigned int leaf = first; leaf < last; leaf += 1u)
    {
        if ((read->leaf[leaf].is & CLI_LEAF_REST) != 0u)
        {
            head->variadic = 1;
            return;
        }
    }
    unsigned int name = first;
    while ((name < last) && ((read->leaf[name].is & CLI_LEAF_PARAMETER) == 0u))
    {
        name += 1u;
    }
    CliSymbolType *const type = &head->type[head->params];
    if (name < last)
    {
        cli_symbols_type_of(read, name, type);
    }
    else
    {
        cli_type_between(read, first, last, last, type);
    }
    if ((name == last) && (type->text[0] == '\0'))
    {
        return;
    }
    // a head is read from no more than CLI_SYMBOLS_HEAD_LINES lines, far under 4 GiB
    head->span[head->params][0] = (unsigned int)read->leaf[first].from;
    head->span[head->params][1] = (unsigned int)read->leaf[last - 1u].to;
    head->named[head->params][0] = (name < last) ? (unsigned int)read->leaf[name].from : head->span[head->params][0];
    head->named[head->params][1] = (name < last) ? (unsigned int)read->leaf[name].to : head->span[head->params][0];
    head->params += 1u;
}

// the head's label from its first line's first letter to the end of the bracket at `close`, each line break and the
// white space after it one space, and each parameter's span and its name's, held as their bytes, made the label's in
// UTF-16 units
static void cli_head_label(const CliLeafRead *read, unsigned int close, CliSymbolHead *head)
{
    unsigned int bytes[CLI_SYMBOLS_PARAMS][2];
    unsigned int name_bytes[CLI_SYMBOLS_PARAMS][2];
    memor.cpy(&(MemoriaCfg){.dst = bytes, .src = head->span, .bytes = sizeof(bytes)});
    memor.cpy(&(MemoriaCfg){.dst = name_bytes, .src = head->named, .bytes = sizeof(name_bytes)});
    const char *const text = read->text;
    const unsigned long long to = read->leaf[close].to;
    unsigned long long byte = 0ull;
    while ((byte < to) && ((text[byte] == ' ') || (text[byte] == '\t')))
    {
        byte += 1ull;
    }
    unsigned int at = 0u;
    unsigned int units = 0u;
    while ((byte < to) && ((at + 5u) < CLI_SYMBOLS_LABEL))
    {
        for (unsigned int param = 0u; param < head->params; param += 1u)
        {
            head->span[param][0] = (bytes[param][0] == byte) ? units : head->span[param][0];
            head->span[param][1] = (bytes[param][1] == byte) ? units : head->span[param][1];
            head->named[param][0] = (name_bytes[param][0] == byte) ? units : head->named[param][0];
            head->named[param][1] = (name_bytes[param][1] == byte) ? units : head->named[param][1];
        }
        if ((text[byte] == '\r') || (text[byte] == '\n'))
        {
            const unsigned long long broken = byte;
            while ((byte < to) && ((text[byte] == ' ') || (text[byte] == '\t') || (text[byte] == '\r') ||
                                   (text[byte] == '\n')))
            {
                byte += 1ull;
            }
            // a span that ends within the white space ends before its one space
            for (unsigned int param = 0u; param < head->params; param += 1u)
            {
                const int within = (bytes[param][1] > broken) && (bytes[param][1] < byte);
                head->span[param][1] = within ? units : head->span[param][1];
            }
            head->label[at] = ' ';
            at += 1u;
            units += 1u;
            continue;
        }
        const unsigned char lead = (unsigned char)text[byte];
        const unsigned int length = (lead < 0x80u) ? 1u : (lead < 0xe0u) ? 2u : (lead < 0xf0u) ? 3u : 4u;
        for (unsigned int more = 0u; (more < length) && (byte < to); more += 1u)
        {
            head->label[at] = text[byte];
            at += 1u;
            byte += 1ull;
        }
        units += (length == 4u) ? 2u : 1u;
    }
    head->label[at] = '\0';
    for (unsigned int param = 0u; param < head->params; param += 1u)
    {
        head->span[param][1] = (bytes[param][1] >= byte) ? units : head->span[param][1];
        head->named[param][1] = (name_bytes[param][1] >= byte) ? units : head->named[param][1];
    }
}

// the head of the declaration of `name`, `length` bytes, on the first line of the leaves read: its parameters, each
// the leaves between separators at its bracket's depth, to the bracket that closes it, and its label: 1, or 0 where no
// leaf on that line declares it or no bracket opens after it before a region's bracket, a statement's end or an
// assignment, or its bracket is not closed; each as its grammar names it
static int cli_head_params(const CliLeafRead *read, const char *name, unsigned long long length, CliSymbolHead *head)
{
    unsigned int at = 0u;
    for (; (at < read->count) && (read->leaf[at].line == 0u); at += 1u)
    {
        unsigned long long size = 0ull;
        const char *const text = cli_leaf_text(read, at, &size);
        if (((read->leaf[at].is & CLI_LEAF_DECLARES) != 0u) && (size == length) && cli_text_same(text, name, size))
        {
            break;
        }
    }
    if ((at == read->count) || (read->leaf[at].line != 0u))
    {
        return 0;
    }
    unsigned int open = at + 1u;
    while ((open < read->count) && !cli_leaf_has(read, open, CLI_LEAF_OPEN))
    {
        if (cli_leaf_has(read, open, CLI_LEAF_STATEMENT) || cli_leaf_has(read, open, CLI_LEAF_ASSIGN))
        {
            return 0;
        }
        open += 1u;
    }
    if ((open < read->count) && cli_leaf_has(read, open, CLI_LEAF_REGION))
    {
        return 0;
    }
    head->params = 0u;
    head->variadic = 0;
    unsigned int depth = 0u;
    unsigned int first = open + 1u;
    unsigned int close = read->count;
    for (unsigned int leaf = open + 1u; (leaf < read->count) && (close == read->count); leaf += 1u)
    {
        const int separates = (depth == 0u) && cli_leaf_has(read, leaf, CLI_LEAF_SEPARATOR);
        if (cli_leaf_has(read, leaf, CLI_LEAF_OPEN))
        {
            depth += 1u;
        }
        else if (cli_leaf_has(read, leaf, CLI_LEAF_CLOSE) && (depth > 0u))
        {
            depth -= 1u;
        }
        else if (cli_leaf_has(read, leaf, CLI_LEAF_CLOSE) || separates)
        {
            cli_head_param(read, first, leaf, head);
            first = leaf + 1u;
            close = separates ? read->count : leaf;
        }
    }
    if ((open >= read->count) || (close == read->count))
    {
        return 0;
    }
    cli_head_label(read, close, head);
    return 1;
}

int cli_symbols_head_read(CliSymbolsLanguages *languages, const char *root, const char *name,
                          unsigned long long length, CliSymbolHead *head)
{
    char full[CLI_PATH_MAX_LEN];
    cli_full(root, head->path, full);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_read(full, &size);
    if (bytes == NULL)
    {
        return 0;
    }
    const char *const text = (const char *)bytes;
    unsigned long long start = 0ull;
    unsigned int counted = 0u;
    while ((counted < head->line) && (start < size))
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + start, .val = '\n', .bytes = size - start});
        // a newline found lies in the text, at or past `start`
        start = (newline != NULL) ? ((unsigned long long)(newline - text) + 1ull) : size;
        counted += 1u;
    }
    unsigned long long end = start;
    for (unsigned int lines = 0u; (end < size) && (lines < CLI_SYMBOLS_HEAD_LINES); lines += 1u)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + end, .val = '\n', .bytes = size - end});
        end = (newline != NULL) ? ((unsigned long long)(newline - text) + 1ull) : size;
    }
    CliLeafRead read;
    const int kept = (counted == head->line) &&
                     cli_symbols_leaves_kept(languages, head->path, "root", text + start, end - start, &read);
    const int found = kept && cli_head_params(&read, name, length, head);
    if (counted == head->line)
    {
        cli_symbols_leaves_given(&read);
    }
    cli_file_let_go(bytes, size);
    return found;
}
