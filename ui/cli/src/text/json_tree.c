// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_tree.c: a JSON tree read from its text, its nodes made, found, put, taken out, copied and compared
#include "json_tree.h"
#include "text.h"

// no key: a node that is no object's member
#define CLI_JSON_NO_KEY 0xffffffffffffffffull

void cli_json_begun(CliJson *json, CliJsonNode *node, unsigned int nodes_most, char *text, unsigned long long text_most)
{
    json->node = node;
    json->nodes = 0u;
    json->nodes_most = nodes_most;
    json->text = text;
    json->used = 0ull;
    json->text_most = text_most;
    json->full = 0;
    json->grows = 0;
    json->what = NULL;
}

// `length` bytes of `text` kept among the tree's texts, a NUL after them: where they start, or 0 where they do not fit
static unsigned long long cli_kept(CliJson *json, const char *text, unsigned long long length)
{
    if (!cli_json_room(json, 0u, length + 1ull))
    {
        return 0ull;
    }
    const unsigned long long at = json->used;
    memor.cpy(&(MemoriaCfg){.dst = json->text + at, .src = text, .bytes = length});
    json->text[at + length] = '\0';
    json->used += length + 1ull;
    return at;
}

unsigned int cli_json_made(CliJson *json, CfgJsonKind kind)
{
    if (!cli_json_room(json, 1u, 0ull))
    {
        return CLI_JSON_NONE;
    }
    const unsigned int made = json->nodes;
    const CliJsonNode node = {kind, CLI_JSON_NO_KEY, 0ull, 0ull, CLI_JSON_NONE, CLI_JSON_NONE, CLI_JSON_NONE, 0u};
    json->node[made] = node;
    json->nodes += 1u;
    return made;
}

unsigned int cli_json_string_made(CliJson *json, const char *text, unsigned long long length)
{
    const unsigned int made = cli_json_made(json, CFG_JSON_STRING);
    if (made != CLI_JSON_NONE)
    {
        json->node[made].text = cli_kept(json, text, length);
        json->node[made].length = json->full ? 0ull : length;
    }
    return made;
}

unsigned int cli_json_number_made(CliJson *json, const char *text)
{
    const unsigned int made = cli_json_made(json, CFG_JSON_NUMBER);
    if (made != CLI_JSON_NONE)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
        json->node[made].text = cli_kept(json, text, length);
        json->node[made].length = json->full ? 0ull : length;
    }
    return made;
}

// the child of `parent` whose key is `name`, and the child before it into `before`, CLI_JSON_NONE where it is first
static unsigned int cli_keyed(const CliJson *json, unsigned int parent, const char *name, unsigned int *before)
{
    *before = CLI_JSON_NONE;
    if ((parent == CLI_JSON_NONE) || (json->node[parent].kind != CFG_JSON_OBJECT))
    {
        return CLI_JSON_NONE;
    }
    for (unsigned int child = json->node[parent].first; child != CLI_JSON_NONE; child = json->node[child].next)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = json->text + json->node[child].key, .other = name, .cap = CLI_CELLS_POOL}))
        {
            return child;
        }
        *before = child;
    }
    return CLI_JSON_NONE;
}

unsigned int cli_json_member(const CliJson *json, unsigned int object, const char *name)
{
    unsigned int before = CLI_JSON_NONE;
    return cli_keyed(json, object, name, &before);
}

unsigned int cli_json_item(const CliJson *json, unsigned int array, unsigned int index)
{
    if ((array == CLI_JSON_NONE) || (json->node[array].kind != CFG_JSON_ARRAY))
    {
        return CLI_JSON_NONE;
    }
    unsigned int child = json->node[array].first;
    for (unsigned int at = 0u; (child != CLI_JSON_NONE) && (at < index); at += 1u)
    {
        child = json->node[child].next;
    }
    return child;
}

const char *cli_json_string(const CliJson *json, unsigned int node, unsigned long long *length)
{
    if ((node == CLI_JSON_NONE) || (json->node[node].kind != CFG_JSON_STRING))
    {
        return NULL;
    }
    if (length != NULL)
    {
        *length = json->node[node].length;
    }
    return json->text + json->node[node].text;
}

int cli_json_is(const CliJson *json, unsigned int node, CfgJsonKind kind)
{
    return (node != CLI_JSON_NONE) && (json->node[node].kind == kind);
}

// `value` put after the last child of `parent`
static void cli_put_last(CliJson *json, unsigned int parent, unsigned int value)
{
    CliJsonNode *const node = &json->node[parent];
    json->node[value].next = CLI_JSON_NONE;
    if (node->last == CLI_JSON_NONE)
    {
        node->first = value;
    }
    else
    {
        json->node[node->last].next = value;
    }
    node->last = value;
    node->count += 1u;
}

// `value` put as the member of `object` whose key starts at `key` in the tree's texts
static void cli_put_keyed(CliJson *json, unsigned int object, unsigned long long key, unsigned int value)
{
    if ((object == CLI_JSON_NONE) || (value == CLI_JSON_NONE) || (json->node[object].kind != CFG_JSON_OBJECT))
    {
        return;
    }
    unsigned int before = CLI_JSON_NONE;
    const unsigned int had = cli_keyed(json, object, json->text + key, &before);
    json->node[value].key = key;
    if (had == CLI_JSON_NONE)
    {
        cli_put_last(json, object, value);
        return;
    }
    json->node[value].next = json->node[had].next;
    if (before == CLI_JSON_NONE)
    {
        json->node[object].first = value;
    }
    else
    {
        json->node[before].next = value;
    }
    if (json->node[object].last == had)
    {
        json->node[object].last = value;
    }
}

void cli_json_set(CliJson *json, unsigned int object, const char *name, unsigned int value)
{
    unsigned int before = CLI_JSON_NONE;
    const unsigned int had = cli_keyed(json, object, name, &before);
    const unsigned long long key =
        (had != CLI_JSON_NONE)
            ? json->node[had].key
            : cli_kept(json, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    if (!json->full)
    {
        cli_put_keyed(json, object, key, value);
    }
}

void cli_json_remove(CliJson *json, unsigned int object, const char *name)
{
    unsigned int before = CLI_JSON_NONE;
    const unsigned int had = cli_keyed(json, object, name, &before);
    if (had == CLI_JSON_NONE)
    {
        return;
    }
    if (before == CLI_JSON_NONE)
    {
        json->node[object].first = json->node[had].next;
    }
    else
    {
        json->node[before].next = json->node[had].next;
    }
    if (json->node[object].last == had)
    {
        json->node[object].last = before;
    }
    json->node[object].count -= 1u;
}

void cli_json_append(CliJson *json, unsigned int array, unsigned int value)
{
    if ((array != CLI_JSON_NONE) && (value != CLI_JSON_NONE) && (json->node[array].kind == CFG_JSON_ARRAY))
    {
        json->node[value].key = CLI_JSON_NO_KEY;
        cli_put_last(json, array, value);
    }
}

void cli_json_sorted(CliJson *json, unsigned int object)
{
    if (!cli_json_is(json, object, CFG_JSON_OBJECT))
    {
        return;
    }
    // each member taken off the list in turn and put back in its place by key, a list kept in order
    unsigned int sorted = CLI_JSON_NONE;
    unsigned int child = json->node[object].first;
    while (child != CLI_JSON_NONE)
    {
        const unsigned int next = json->node[child].next;
        const char *const key = json->text + json->node[child].key;
        if ((sorted == CLI_JSON_NONE) || (cli_text_order(key, json->text + json->node[sorted].key) < 0))
        {
            json->node[child].next = sorted;
            sorted = child;
        }
        else
        {
            unsigned int before = sorted;
            while ((json->node[before].next != CLI_JSON_NONE) &&
                   (cli_text_order(json->text + json->node[json->node[before].next].key, key) <= 0))
            {
                before = json->node[before].next;
            }
            json->node[child].next = json->node[before].next;
            json->node[before].next = child;
        }
        child = next;
    }
    json->node[object].first = sorted;
    unsigned int last = sorted;
    while ((last != CLI_JSON_NONE) && (json->node[last].next != CLI_JSON_NONE))
    {
        last = json->node[last].next;
    }
    json->node[object].last = last;
}

unsigned int cli_json_copy(CliJson *json, unsigned int node)
{
    if (node == CLI_JSON_NONE)
    {
        return CLI_JSON_NONE;
    }
    const unsigned int made = cli_json_made(json, json->node[node].kind);
    if (made == CLI_JSON_NONE)
    {
        return made;
    }
    // a text is never changed once kept, so a copy shares it
    json->node[made].text = json->node[node].text;
    json->node[made].length = json->node[node].length;
    for (unsigned int child = json->node[node].first; child != CLI_JSON_NONE; child = json->node[child].next)
    {
        const unsigned int copied = cli_json_copy(json, child);
        if (copied == CLI_JSON_NONE)
        {
            return CLI_JSON_NONE;
        }
        cli_put_last(json, made, copied);
        json->node[copied].key = json->node[child].key;
    }
    return made;
}

unsigned int cli_json_copy_from(CliJson *to, const CliJson *from, unsigned int node)
{
    if (node == CLI_JSON_NONE)
    {
        return CLI_JSON_NONE;
    }
    const CliJsonNode *const held = &from->node[node];
    const unsigned int made = cli_json_made(to, held->kind);
    if (made == CLI_JSON_NONE)
    {
        return made;
    }
    if ((held->kind == CFG_JSON_STRING) || (held->kind == CFG_JSON_NUMBER))
    {
        to->node[made].text = cli_kept(to, from->text + held->text, held->length);
        to->node[made].length = to->full ? 0ull : held->length;
    }
    for (unsigned int child = held->first; child != CLI_JSON_NONE; child = from->node[child].next)
    {
        const unsigned int copied = cli_json_copy_from(to, from, child);
        if (copied == CLI_JSON_NONE)
        {
            return CLI_JSON_NONE;
        }
        if (held->kind == CFG_JSON_OBJECT)
        {
            const char *const key = from->text + from->node[child].key;
            const unsigned long long at =
                cli_kept(to, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
            if (to->full)
            {
                return CLI_JSON_NONE;
            }
            cli_put_keyed(to, made, at, copied);
        }
        else
        {
            cli_put_last(to, made, copied);
        }
    }
    return made;
}

int cli_json_same(const CliJson *json, unsigned int one, const CliJson *other_json, unsigned int other)
{
    if ((one == CLI_JSON_NONE) || (other == CLI_JSON_NONE))
    {
        return one == other;
    }
    const CliJsonNode *const left = &json->node[one];
    const CliJsonNode *const right = &other_json->node[other];
    if ((left->kind != right->kind) || (left->count != right->count))
    {
        return 0;
    }
    if ((left->kind == CFG_JSON_STRING) || (left->kind == CFG_JSON_NUMBER))
    {
        return (left->length == right->length) && (memor.cmp(&(MemoriaCfg){.src = json->text + left->text,
                                                                           .other = other_json->text + right->text,
                                                                           .bytes = left->length}) == 0);
    }
    unsigned int pair = right->first;
    for (unsigned int child = left->first; child != CLI_JSON_NONE; child = json->node[child].next)
    {
        const unsigned int match = (left->kind == CFG_JSON_OBJECT)
                                       ? cli_json_member(other_json, other, json->text + json->node[child].key)
                                       : pair;
        if (!cli_json_same(json, child, other_json, match))
        {
            return 0;
        }
        pair = (pair != CLI_JSON_NONE) ? other_json->node[pair].next : pair;
    }
    return 1;
}

// the value at token `token` of `text` made a node, all under it with it
static unsigned int cli_converted(CliJson *json, const char *text, const CfgJsonToken *tokens, unsigned int token)
{
    const CfgJsonToken *const at = &tokens[token];
    const unsigned int made = cli_json_made(json, at->kind);
    if (made == CLI_JSON_NONE)
    {
        return made;
    }
    if (at->kind == CFG_JSON_STRING)
    {
        // a string's letters as kept are at most its letters as written
        if (!cli_json_room(json, 0u, (unsigned long long)(at->end - at->start) + 1ull))
        {
            return CLI_JSON_NONE;
        }
        const unsigned long long room = json->text_most - json->used;
        if (!cfg_json_string(text, at, json->text + json->used, (size_t)room))
        {
            json->full = 1;
            return CLI_JSON_NONE;
        }
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = json->text + json->used, .cap = CLI_CELLS_POOL});
        json->node[made].text = json->used;
        json->node[made].length = length;
        json->used += length + 1ull;
    }
    else if (at->kind == CFG_JSON_NUMBER)
    {
        json->node[made].text = cli_kept(json, text + at->start, at->end - at->start);
        json->node[made].length = at->end - at->start;
    }
    else if (at->kind == CFG_JSON_OBJECT)
    {
        unsigned int key = token + 1u;
        for (unsigned int member = 0u; member < at->count; member += 1u)
        {
            const unsigned int value = cli_converted(json, text, tokens, key + 1u);
            const unsigned int named = cli_converted(json, text, tokens, key);
            if ((value == CLI_JSON_NONE) || (named == CLI_JSON_NONE))
            {
                return CLI_JSON_NONE;
            }
            // the key's node stands for its text alone, and is let go of
            cli_put_keyed(json, made, json->node[named].text, value);
            json->nodes -= (named == (json->nodes - 1u)) ? 1u : 0u;
            key = tokens[key + 1u].next;
        }
    }
    else if (at->kind == CFG_JSON_ARRAY)
    {
        unsigned int item = token + 1u;
        for (unsigned int each = 0u; each < at->count; each += 1u)
        {
            const unsigned int value = cli_converted(json, text, tokens, item);
            if (value == CLI_JSON_NONE)
            {
                return CLI_JSON_NONE;
            }
            cli_put_last(json, made, value);
            item = tokens[item].next;
        }
    }
    return made;
}

unsigned int cli_json_read(CliJson *json, const char *text, unsigned long long size, CfgJsonToken *tokens,
                           unsigned int tokens_most, CliLine *said)
{
    CfgJsonParse parse = {0u, 0u, 0u, NULL};
    if (!cfg_json_parse_metadata(text, (size_t)size, tokens, tokens_most, &parse))
    {
        cli_line_text(said, (parse.reason != NULL) ? parse.reason : "it is not JSON");
        cli_line_text(said, " at line ");
        cli_line_decimal(said, parse.line, 1u);
        cli_line_text(said, " column ");
        cli_line_decimal(said, parse.column, 1u);
        return CLI_JSON_NONE;
    }
    const unsigned int root = cli_converted(json, text, tokens, 0u);
    if ((root == CLI_JSON_NONE) || json->full)
    {
        cli_line_text(said, "it holds more than the tree it is read into keeps");
        return CLI_JSON_NONE;
    }
    return root;
}
