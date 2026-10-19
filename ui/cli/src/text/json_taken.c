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

int cli_json_held(CliJson *json, unsigned int nodes_most, unsigned long long text_most, const char *what)
{
    const unsigned long long nodes_bytes = (unsigned long long)nodes_most * sizeof(CliJsonNode);
    unsigned char *const cell = cli_cell_taken(nodes_bytes + text_most + 1ull, what);
    cli_json_begun(json, (CliJsonNode *)cell, (cell != NULL) ? nodes_most : 0u,
                   (cell != NULL) ? (char *)(cell + nodes_bytes) : NULL, (cell != NULL) ? text_most + 1ull : 0ull);
    return cell != NULL;
}

unsigned int cli_json_taken(CliJson *json, const char *text, unsigned long long size, unsigned int nodes_more,
                            unsigned long long text_more, CliLine *said)
{
    const unsigned int tokens_most = cli_json_tokens_counted(text, size) + 1u;
    CfgJsonToken *const tokens =
        cli_cell_taken((unsigned long long)tokens_most * sizeof(CfgJsonToken), "a tree's tokens");
    if ((tokens == NULL) ||
        !cli_json_held(json, tokens_most + nodes_more, size + text_more, "a tree's nodes and texts"))
    {
        cli_cell_given(tokens);
        cli_line_text(said, "the cells a tree is read into could not be taken");
        return CLI_JSON_NONE;
    }
    const unsigned int root = cli_json_read(json, text, size, tokens, tokens_most, said);
    cli_cell_given(tokens);
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
        // a member's key is kept again where the member is copied
        *text +=
            (one->kind == CFG_JSON_OBJECT)
                ? (cellul.len(&(CatenaFinitaCfg){.src = json->text + json->node[child].key, .cap = CLI_CELLS_POOL}) +
                   1ull)
                : 0ull;
        cli_json_extent(json, child, nodes, text);
    }
}

unsigned int cli_json_file_taken(CliJson *json, const char *path, unsigned int nodes_more, unsigned long long text_more,
                                 CliLine *said)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    if (bytes == NULL)
    {
        cli_json_begun(json, NULL, 0u, NULL, 0ull);
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be read");
        return CLI_JSON_NONE;
    }
    // the tree keeps its texts of its own, so the file is let go as the read ends
    const unsigned int root = cli_json_taken(json, (const char *)bytes, size, nodes_more, text_more, said);
    qry_member_file_unmapped(bytes, size);
    return root;
}

void cli_json_given(CliJson *json)
{
    cli_cell_given(json->node);
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
}
