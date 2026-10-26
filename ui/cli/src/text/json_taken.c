// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_taken.c: a JSON tree in a cell of its own, sized to the text it is read from: its tokens counted before the
// read, the nodes and texts one cell, the tokens a cell given back as the read ends
#include "json_tree.h"

#include "../command_line/cells.h"

// whether a letter goes on a number or a literal: a digit, a letter, or a sign, a point or an exponent's sign
static int json_word_letter(char letter)
{
    return ((letter >= '0') && (letter <= '9')) || ((letter >= 'a') && (letter <= 'z')) ||
           ((letter >= 'A') && (letter <= 'Z')) || (letter == '.') || (letter == '+') || (letter == '-');
}

unsigned int cli_json_tokens_counted(const char *text, unsigned long long size)
{
    unsigned int count = 0u;
    unsigned long long at = 0ull;
    while (at < size)
    {
        const char letter = text[at];
        if (letter == '"')
        {
            at += 1ull;
            while ((at < size) && (text[at] != '"'))
            {
                at += (text[at] == '\\') ? 2ull : 1ull;
            }
            at += 1ull;
            count += 1u;
        }
        else if ((letter == '{') || (letter == '['))
        {
            at += 1ull;
            count += 1u;
        }
        else if (json_word_letter(letter))
        {
            while ((at < size) && json_word_letter(text[at]))
            {
                at += 1ull;
            }
            count += 1u;
        }
        else
        {
            at += 1ull;
        }
    }
    return count;
}

int cli_json_reserved(CliJson *json, unsigned int nodes_most, unsigned long long text_most, const char *what)
{
    const unsigned long long nodes_bytes = (unsigned long long)nodes_most * sizeof(CliJsonNode);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(nodes_bytes + text_most + 1ull, NULL);
    cli_json_begun(json, (CliJsonNode *)cell, (cell != NULL) ? nodes_most : 0u,
                   (cell != NULL) ? (char *)(cell + nodes_bytes) : NULL, (cell != NULL) ? text_most + 1ull : 0ull);
    json->what = what;
    return cell != NULL;
}

int cli_json_growing(CliJson *json, unsigned int nodes_first, unsigned long long text_first, const char *what)
{
    const int reserved = cli_json_reserved(json, nodes_first, text_first, what);
    json->grows = reserved;
    return reserved;
}

int cli_json_room(CliJson *json, unsigned int nodes, unsigned long long text)
{
    const unsigned long long nodes_wanted = (unsigned long long)json->nodes + nodes;
    const unsigned long long text_wanted = json->used + text;
    if ((nodes_wanted <= json->nodes_most) && (text_wanted <= json->text_most))
    {
        return 1;
    }
    // a node's index is an unsigned int's, so a tree past one's reach is full
    if (!json->grows || (nodes_wanted > 0xffffffffull))
    {
        json->full = 1;
        return 0;
    }
    const unsigned long long nodes_twice = 2ull * json->nodes_most;
    const unsigned long long text_twice = 2ull * json->text_most;
    const unsigned int nodes_most = (unsigned int)((nodes_twice > nodes_wanted) ? nodes_twice : nodes_wanted);
    const unsigned long long text_most = (text_twice > text_wanted) ? text_twice : text_wanted;
    const unsigned long long nodes_bytes = (unsigned long long)nodes_most * sizeof(CliJsonNode);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(nodes_bytes + text_most, NULL);
    if (cell == NULL)
    {
        json->full = 1;
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = cell, .src = json->node, .bytes = (size_t)json->nodes * sizeof(CliJsonNode)});
    memor.cpy(&(MemoriaCfg){.dst = cell + nodes_bytes, .src = json->text, .bytes = (size_t)json->used});
    g_cli_cells->persistent_buf_release(json->node, NULL);
    json->node = (CliJsonNode *)cell;
    json->nodes_most = nodes_most;
    json->text = (char *)(cell + nodes_bytes);
    json->text_most = text_most;
    return 1;
}

void cli_json_emptied(CliJson *json)
{
    json->nodes = 0u;
    json->used = 0ull;
    json->full = 0;
}

unsigned int cli_json_taken(CliJson *json, const char *text, unsigned long long size, unsigned int nodes_more,
                            unsigned long long text_more, VerbaLinea *err_msg)
{
    const unsigned int tokens_most = cli_json_tokens_counted(text, size) + 1u;
    CfgJsonToken *const tokens =
        g_cli_cells->persistent_buf_alloc((unsigned long long)tokens_most * sizeof(CfgJsonToken), NULL);
    if ((tokens == NULL) ||
        !cli_json_reserved(json, tokens_most + nodes_more, size + text_more, "a tree's nodes and texts"))
    {
        g_cli_cells->persistent_buf_release(tokens, NULL);
        mmgr_line_put(err_msg, "the cells a tree is read into could not be taken");
        return CLI_JSON_NONE;
    }
    const unsigned int root = cli_json_read(json, text, size, tokens, tokens_most, err_msg);
    g_cli_cells->persistent_buf_release(tokens, NULL);
    return root;
}

void cli_json_extent(const CliJson *json, unsigned int node, unsigned int *nodes, unsigned long long *text)
{
    if ((node == CLI_JSON_NONE) || (node >= json->nodes))
    {
        return;
    }
    const CliJsonNode *const one = &json->node[node];
    *nodes += 1u;
    *text += ((one->kind == CFG_JSON_STRING) || (one->kind == CFG_JSON_NUMBER)) ? (one->length + 1ull) : 0ull;
    for (unsigned int child = one->first; (child != CLI_JSON_NONE) && (child < json->nodes);
         child = json->node[child].next)
    {
        // a member's key is stored again where the member is copied
        *text +=
            (one->kind == CFG_JSON_OBJECT)
                ? (cellul.len(&(CatenaFinitaCfg){.src = json->text + json->node[child].key, .cap = CLI_CELLS_POOL}) +
                   1ull)
                : 0ull;
        cli_json_extent(json, child, nodes, text);
    }
}

unsigned int cli_json_file_taken(CliJson *json, const char *path, unsigned int nodes_more, unsigned long long text_more,
                                 VerbaLinea *err_msg)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_read(path, &size);
    if (bytes == NULL)
    {
        cli_json_begun(json, NULL, 0u, NULL, 0ull);
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be read");
        return CLI_JSON_NONE;
    }
    // the tree stores its texts of its own, so the file is let go as the read ends
    const unsigned int root = cli_json_taken(json, (const char *)bytes, size, nodes_more, text_more, err_msg);
    cli_file_let_go(bytes, size);
    return root;
}

void cli_json_given(CliJson *json)
{
    g_cli_cells->persistent_buf_release(json->node, NULL);
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
}
