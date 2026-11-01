// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_taken.c: a JSON tree in a cell of its own, sized to the text it is read from: its tokens counted before the
// read, the nodes and texts one cell, the tokens a cell given back as the read ends
#include "json_tree.h"

#include "../command_line/cells.h"
#include "text.h"

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
            at = cli_text_to_any(text, at, size, "\"\\");
            while ((at < size) && (text[at] == '\\'))
            {
                at = cli_text_to_any(text, at + 2ull, size, "\"\\");
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

// the tree's room reserved as cli_json_reserved says, its cell taken for `owner` from the temporary tier where
// `temporary` is set, else from the persistent tier
static int json_reserved_in(CliJson *json, unsigned int nodes_most, unsigned long long text_most, const char *what,
                            CellOwner *owner, int temporary)
{
    const unsigned long long nodes_bytes = (unsigned long long)nodes_most * sizeof(CliJsonNode);
    const unsigned long long bytes = nodes_bytes + text_most + 1ull;
    unsigned char *const cell =
        temporary ? g_cli_cells->temporary_buf_alloc(bytes, owner) : g_cli_cells->persistent_buf_alloc(bytes, owner);
    if (cell != NULL)
    {
        cli_json_begun(json, (CliJsonNode *)cell, nodes_most, (char *)(cell + nodes_bytes), text_most + 1ull);
    }
    else
    {
        cli_json_begun(json, NULL, 0u, NULL, 0ull);
    }
    json->what = what;
    return cell != NULL;
}

int cli_json_reserved(CliJson *json, unsigned int nodes_most, unsigned long long text_most, const char *what,
                      CellOwner *owner)
{
    return json_reserved_in(json, nodes_most, text_most, what, owner, 0);
}

int cli_json_reserved_temporary(CliJson *json, unsigned int nodes_most, unsigned long long text_most, const char *what,
                                CellOwner *owner)
{
    return json_reserved_in(json, nodes_most, text_most, what, owner, 1);
}

int cli_json_room(CliJson *json, unsigned int nodes, unsigned long long text)
{
    const int fits =
        (((unsigned long long)json->nodes + nodes) <= json->nodes_most) && ((json->used + text) <= json->text_most);
    json->full = json->full || !fits;
    return fits;
}

void cli_json_emptied(CliJson *json)
{
    json->nodes = 0u;
    json->used = 0ull;
    json->full = 0;
}

// the tree read as cli_json_taken reads it, its own cell taken as json_reserved_in takes it
static unsigned int json_taken_in(CliJson *json, const char *text, unsigned long long size, unsigned int nodes_more,
                                  unsigned long long text_more, VerbaLinea *err_msg, CellOwner *owner, int temporary)
{
    // the tree begun empty before any take: a take refused leaves a tree with no cell for its holder to give back
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
    const unsigned int tokens_most = cli_json_tokens_counted(text, size) + 1u;
    CfgJsonToken *const tokens =
        g_cli_cells->temporary_buf_alloc((unsigned long long)tokens_most * sizeof(CfgJsonToken), owner);
    if ((tokens == NULL) || !json_reserved_in(json, tokens_most + nodes_more, size + text_more,
                                              "a tree's nodes and texts", owner, temporary))
    {
        g_cli_cells->persistent_buf_release(tokens, owner);
        mmgr_line_put(err_msg, "the cells a tree is read into could not be taken");
        return CLI_JSON_NONE;
    }
    const unsigned int root = cli_json_read(json, text, size, tokens, tokens_most, err_msg);
    g_cli_cells->persistent_buf_release(tokens, owner);
    return root;
}

unsigned int cli_json_taken(CliJson *json, const char *text, unsigned long long size, unsigned int nodes_more,
                            unsigned long long text_more, VerbaLinea *err_msg, CellOwner *owner)
{
    return json_taken_in(json, text, size, nodes_more, text_more, err_msg, owner, 0);
}

unsigned int cli_json_taken_temporary(CliJson *json, const char *text, unsigned long long size, unsigned int nodes_more,
                                      unsigned long long text_more, VerbaLinea *err_msg, CellOwner *owner)
{
    return json_taken_in(json, text, size, nodes_more, text_more, err_msg, owner, 1);
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
                                 VerbaLinea *err_msg, CellOwner *owner)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_taken(path, &size, &g_cli_text_cells);
    if (bytes == NULL)
    {
        cli_json_begun(json, NULL, 0u, NULL, 0ull);
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be read");
        return CLI_JSON_NONE;
    }
    // the tree stores its texts of its own, and the file is let go as the read ends
    const unsigned int root = cli_json_taken(json, (const char *)bytes, size, nodes_more, text_more, err_msg, owner);
    cli_file_given(bytes, &g_cli_text_cells);
    return root;
}

void cli_json_given(CliJson *json)
{
    // the tree given back as the owner its cell counts to
    g_cli_cells->persistent_buf_release(json->node, cli_cell_owner(json->node));
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
}
