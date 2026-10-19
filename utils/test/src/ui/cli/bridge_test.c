// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bridge_test.c: the bridge's check: a pair keeps its verdict and every line under it, a map reads a name and its key
// apart, and the tree gives the bridge
#include "../../../../../ui/cli/src/editor/bridge.h"
#include "cli_check.h"

static CliJson s_json;

// the tree made empty in a cell of its own, the one before given back, room enough for a check's few lines
static CliJson *bridge_tree(void)
{
    cli_json_given(&s_json);
    cli_check(cli_json_held(&s_json, 1024u, 1ull << 16u, "a bridge check's tree"), "a check's tree is taken");
    return &s_json;
}

// a string member of a node, the empty text where it has none
static const char *bridge_text(unsigned int node, const char *name)
{
    const char *const text = cli_json_string(&s_json, cli_json_member(&s_json, node, name), NULL);
    return (text != NULL) ? text : "";
}

// whether the text `whole` ends in `end`
static int bridge_ends(const char *whole, const char *end)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = whole, .cap = CLI_CELLS_POOL});
    return (size >= cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL})) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = whole + size -
                                                cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}),
                                         .other = end,
                                         .cap = CLI_CELLS_POOL}));
}

// a pair keeps its verdict and every line under it
static void bridge_pair(void)
{
    static const char s_klq[] = "klq L*\nkey word_copy\npair word_copy thread_idx_x\nopen 3\nqualifier_coherence\n"
                                "concept_coherence 00ff\nkey word_set\ntext_identity a = b\nclosed @x 1->0\n";
    CliJson *const json = bridge_tree();
    const unsigned int read = cli_bridge_klq(json, "Lstar.klq", s_klq, sizeof(s_klq) - 1u);
    const unsigned int keys = cli_json_member(json, read, "keys");
    const unsigned int pair =
        cli_json_item(json, cli_json_member(json, cli_json_member(json, keys, "word_copy"), "pairs"), 0u);
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = bridge_text(pair, "first"), .other = "word_copy", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = bridge_text(pair, "second"), .other = "thread_idx_x", .cap = CLI_CELLS_POOL})),
              "a pair reads its two names");
    const unsigned int verdict = cli_json_member(json, pair, "verdict");
    cli_check(
        (cellul.eq(&(CatenaFinitaCfg){.src = bridge_text(verdict, "word"), .other = "open", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = bridge_text(verdict, "rest"), .other = "3", .cap = CLI_CELLS_POOL})),
        "and its verdict");
    const unsigned int notes = cli_json_member(json, pair, "notes");
    cli_check(cli_json_is(json, notes, CFG_JSON_ARRAY) && (json->node[notes].count == 2u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = bridge_text(cli_json_item(json, notes, 0u), "word"),
                                                .other = "qualifier_coherence",
                                                .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = bridge_text(cli_json_item(json, notes, 1u), "word"),
                                                .other = "concept_coherence",
                                                .cap = CLI_CELLS_POOL})),
              "and every other line under it as a note");
    const unsigned int set = cli_json_member(json, cli_json_member(json, keys, "word_set"), "pairs");
    cli_check(cli_json_is(json, set, CFG_JSON_ARRAY) && (json->node[set].count == 0u), "a key with no pair has none");
    const unsigned int identity = cli_json_item(json, cli_json_member(json, read, "identities"), 0u);
    cli_check(bridge_ends(bridge_text(identity, "rest"), "closed @x 1->0"), "a line under no pair is the key's");
}

// a map reads a name and its key apart
static void bridge_map(void)
{
    static const char s_klm[] = "klm cu\nkey lane_body lane_core\nkind lane_body nop\nbreaks a b\n";
    CliJson *const json = bridge_tree();
    const unsigned int read = cli_bridge_klm(json, "cu.klm", s_klm, sizeof(s_klm) - 1u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = bridge_text(read, "language"), .other = "cu", .cap = CLI_CELLS_POOL}),
              "a map names its language");
    const unsigned int names = cli_json_member(json, read, "names");
    const unsigned int body = cli_json_member(json, names, "lane_body");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = bridge_text(body, "key"), .other = "lane_core", .cap = CLI_CELLS_POOL}),
        "a name and its key are read apart");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = bridge_text(cli_json_member(json, body, "kind"), "rest"),
                                           .other = "nop",
                                           .cap = CLI_CELLS_POOL}),
              "a name keeps its kind");
    const unsigned int breaks = cli_json_member(json, cli_json_member(json, names, "a"), "breaks");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = bridge_text(cli_json_item(json, breaks, 0u), "rest"), .other = "b", .cap = CLI_CELLS_POOL}),
              "and what it breaks");
}

// the tree gives the bridge
static void bridge_tree_read(void)
{
    char root[CLI_PATH_LONGEST];
    if (!cli_tree_found(root))
    {
        cli_check(1, "no tree is found, and no bridge is read");
        return;
    }
    CliJson *const json = &s_json;
    cli_json_given(json);
    const unsigned int bridge = cli_bridge_read(json, root);
    const unsigned int klq = cli_json_member(json, bridge, "klq");
    cli_check(cli_json_member(json, cli_json_member(json, klq, "keys"), "lane_open") != CLI_JSON_NONE,
              "the tree's Lstar.klq holds lane_open");
    int mapped = 0;
    const unsigned int maps = cli_json_member(json, bridge, "maps");
    for (unsigned int at = 0u; cli_json_item(json, maps, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int map = cli_json_item(json, maps, at);
        mapped |= (cellul.eq(
                      &(CatenaFinitaCfg){.src = bridge_text(map, "language"), .other = "cu", .cap = CLI_CELLS_POOL})) &&
                  (cli_json_member(json, cli_json_member(json, map, "names"), "lane_open") != CLI_JSON_NONE);
    }
    cli_check(mapped, "the cu map names lane_open");
    int written = 0;
    const unsigned int rulesets = cli_json_member(json, bridge, "rulesets");
    for (unsigned int at = 0u; cli_json_item(json, rulesets, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int ruleset = cli_json_item(json, rulesets, at);
        written |= bridge_ends(bridge_text(ruleset, "path"), "cu.krs") &&
                   (cli_json_member(json, cli_json_member(json, ruleset, "entries"), "lane_open") != CLI_JSON_NONE);
    }
    cli_check(written, "the cu ruleset writes lane_open");
    cli_json_given(json);
}

int main(void)
{
    bridge_pair();
    bridge_map();
    bridge_tree_read();
    return cli_checked();
}
