// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_bridge.c: the bridge at the command line: its keys listed, each with how many pairs it has and how many
// of them are open and closed, those that hold a word where one is given; and one key shown whole, its pairs with
// their verdicts and notes, the name each .klm gives it, and each ruleset's entries of it, every line with the file and
// line it is read from
#include "../editor/bridge.h"
#include "../text/text.h"
#include "cells.h"
#include "command_line_internal.h"

// The command's room, a cell taken as it starts and given back as it ends: the bridge's tree, in a cell of its own,
// and a line said
typedef struct
{
    CliJson json;
    char line[1u << 16u];
} CliBridgeRoom;

static CliBridgeRoom *s_room;

// a string member of a node, the empty text where it has none
static const char *bridge_text(unsigned int node, const char *name)
{
    const char *const text = cli_json_string(&s_room->json, cli_json_member(&s_room->json, node, name), NULL);
    return (text != NULL) ? text : "";
}

// `{path}:{line}` written, the line counted from 1, of a node's `line`, counted from 0
static void bridge_at(CliLine *out, const char *path, unsigned int node)
{
    const unsigned int line = cli_json_member(&s_room->json, node, "line");
    unsigned long long number = 0ull;
    if (cli_json_is(&s_room->json, line, CFG_JSON_NUMBER))
    {
        const CliJsonNode *const one = &s_room->json.node[line];
        for (unsigned long long at = 0ull; at < one->length; at += 1ull)
        {
            const char digit = s_room->json.text[one->text + at];
            if ((digit < '0') || (digit > '9'))
            {
                break;
            }
            number = number * 10ull + (unsigned long long)(digit - '0');
        }
    }
    cli_line_text(out, path);
    cli_line_character(out, ':');
    cli_line_decimal(out, number + 1ull, 1u);
}

// the line written, its spaces at the end not
static void bridge_out(const CliLine *line)
{
    const unsigned long long size = cli_line_fits(line) ? line->at : 0ull;
    cli_out_line(line->out, cli_text_trimmed_end(line->out, size));
}

// how many pairs a key has, and how many of them are open and closed, the empty text for a key with none
static void bridge_tally(unsigned int key, CliLine *out)
{
    const unsigned int pairs = cli_json_member(&s_room->json, key, "pairs");
    const unsigned int count = cli_json_is(&s_room->json, pairs, CFG_JSON_ARRAY) ? s_room->json.node[pairs].count : 0u;
    if (count == 0u)
    {
        return;
    }
    unsigned int open = 0u;
    unsigned int closed = 0u;
    for (unsigned int pair = s_room->json.node[pairs].first; pair != CLI_JSON_NONE; pair = s_room->json.node[pair].next)
    {
        const unsigned int verdict = cli_json_member(&s_room->json, pair, "verdict");
        if (cli_json_is(&s_room->json, verdict, CFG_JSON_OBJECT))
        {
            open += (cellul.eq(&(CatenaFinitaCfg){
                        .src = bridge_text(verdict, "word"), .other = "open", .cap = CLI_CELLS_POOL}))
                        ? 1u
                        : 0u;
            closed += (cellul.eq(&(CatenaFinitaCfg){
                          .src = bridge_text(verdict, "word"), .other = "closed", .cap = CLI_CELLS_POOL}))
                          ? 1u
                          : 0u;
        }
    }
    cli_line_decimal(out, count, 1u);
    cli_line_text(out, " pairs, ");
    cli_line_decimal(out, open, 1u);
    cli_line_text(out, " open, ");
    cli_line_decimal(out, closed, 1u);
    cli_line_text(out, " closed");
}

// the key's pairs, each with its verdict and its notes
static void bridge_pairs(const char *path, const char *name, unsigned int key)
{
    CliLine line = {s_room->line, sizeof(s_room->line), 0ull};
    cli_line_text(&line, name);
    cli_line_text(&line, "  ");
    bridge_at(&line, path, key);
    bridge_out(&line);
    const unsigned int pairs = cli_json_member(&s_room->json, key, "pairs");
    if (!cli_json_is(&s_room->json, pairs, CFG_JSON_ARRAY) || (s_room->json.node[pairs].count == 0u))
    {
        cli_out("  no pairs");
        return;
    }
    for (unsigned int pair = s_room->json.node[pairs].first; pair != CLI_JSON_NONE; pair = s_room->json.node[pair].next)
    {
        line.at = 0ull;
        cli_line_text(&line, "  ");
        cli_line_text(&line, bridge_text(pair, "first"));
        cli_line_text(&line, " ~ ");
        cli_line_text(&line, bridge_text(pair, "second"));
        cli_line_text(&line, "  ");
        const unsigned int verdict = cli_json_member(&s_room->json, pair, "verdict");
        if (cli_json_is(&s_room->json, verdict, CFG_JSON_OBJECT))
        {
            static char s_verdict[1u << 12u];
            CliLine said = {s_verdict, sizeof(s_verdict), 0ull};
            cli_line_text(&said, bridge_text(verdict, "word"));
            cli_line_character(&said, ' ');
            cli_line_text(&said, bridge_text(verdict, "rest"));
            unsigned long long from = 0ull;
            unsigned long long to = 0ull;
            cli_text_trimmed(s_verdict, cli_line_fits(&said) ? said.at : 0ull, &from, &to);
            cli_line_counted(&line, s_verdict + from, to - from);
        }
        cli_line_text(&line, "  ");
        bridge_at(&line, path, pair);
        bridge_out(&line);
        const unsigned int notes = cli_json_member(&s_room->json, pair, "notes");
        const unsigned int first =
            cli_json_is(&s_room->json, notes, CFG_JSON_ARRAY) ? s_room->json.node[notes].first : CLI_JSON_NONE;
        for (unsigned int note = first; note != CLI_JSON_NONE; note = s_room->json.node[note].next)
        {
            line.at = 0ull;
            cli_line_text(&line, "    ");
            cli_line_text(&line, bridge_text(note, "word"));
            cli_line_character(&line, ' ');
            cli_line_text(&line, bridge_text(note, "rest"));
            bridge_out(&line);
        }
    }
}

// the name each .klm gives the key, with its kind and what breaks it
static void bridge_maps(unsigned int bridge, const char *name)
{
    const unsigned int maps = cli_json_member(&s_room->json, bridge, "maps");
    const unsigned int first =
        cli_json_is(&s_room->json, maps, CFG_JSON_ARRAY) ? s_room->json.node[maps].first : CLI_JSON_NONE;
    int any = 0;
    for (unsigned int map = first; map != CLI_JSON_NONE; map = s_room->json.node[map].next)
    {
        const unsigned int names = cli_json_member(&s_room->json, map, "names");
        const unsigned int named_first =
            cli_json_is(&s_room->json, names, CFG_JSON_OBJECT) ? s_room->json.node[names].first : CLI_JSON_NONE;
        for (unsigned int entry = named_first; entry != CLI_JSON_NONE; entry = s_room->json.node[entry].next)
        {
            const char *const named = s_room->json.text + s_room->json.node[entry].key;
            const char *const key = bridge_text(entry, "key");
            if ((!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = name, .cap = CLI_CELLS_POOL})) &&
                (!cellul.eq(&(CatenaFinitaCfg){.src = named, .other = name, .cap = CLI_CELLS_POOL})))
            {
                continue;
            }
            if (!any)
            {
                cli_out("maps");
                any = 1;
            }
            CliLine line = {s_room->line, sizeof(s_room->line), 0ull};
            cli_line_text(&line, "  ");
            cli_line_text(&line, bridge_text(map, "language"));
            cli_line_text(&line, "  ");
            cli_line_text(&line, named);
            if ((key[0] != '\0') && (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = named, .cap = CLI_CELLS_POOL})))
            {
                cli_line_text(&line, " -> ");
                cli_line_text(&line, key);
            }
            const unsigned int kind = cli_json_member(&s_room->json, entry, "kind");
            if (cli_json_is(&s_room->json, kind, CFG_JSON_OBJECT))
            {
                cli_line_text(&line, "  ");
                cli_line_text(&line, bridge_text(kind, "rest"));
            }
            const unsigned int breaks = cli_json_member(&s_room->json, entry, "breaks");
            const unsigned int broken_first =
                cli_json_is(&s_room->json, breaks, CFG_JSON_ARRAY) ? s_room->json.node[breaks].first : CLI_JSON_NONE;
            for (unsigned int broken = broken_first; broken != CLI_JSON_NONE; broken = s_room->json.node[broken].next)
            {
                cli_line_text(&line, "  breaks ");
                cli_line_text(&line, bridge_text(broken, "rest"));
            }
            cli_line_text(&line, "  ");
            bridge_at(&line, bridge_text(map, "path"), entry);
            cli_out_line(s_room->line, cli_line_fits(&line) ? line.at : 0ull);
        }
    }
}

// each ruleset's entries of the key's name, each with its kind
static void bridge_rulesets(unsigned int bridge, const char *name)
{
    const unsigned int rulesets = cli_json_member(&s_room->json, bridge, "rulesets");
    const unsigned int first =
        cli_json_is(&s_room->json, rulesets, CFG_JSON_ARRAY) ? s_room->json.node[rulesets].first : CLI_JSON_NONE;
    int any = 0;
    for (unsigned int ruleset = first; ruleset != CLI_JSON_NONE; ruleset = s_room->json.node[ruleset].next)
    {
        const char *const path = bridge_text(ruleset, "path");
        const char *const slash =
            cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
        const char *const file = (slash != NULL) ? slash + 1 : path;
        const unsigned int entries =
            cli_json_member(&s_room->json, cli_json_member(&s_room->json, ruleset, "entries"), name);
        const unsigned int entry_first =
            cli_json_is(&s_room->json, entries, CFG_JSON_ARRAY) ? s_room->json.node[entries].first : CLI_JSON_NONE;
        for (unsigned int entry = entry_first; entry != CLI_JSON_NONE; entry = s_room->json.node[entry].next)
        {
            if (!any)
            {
                cli_out("rulesets");
                any = 1;
            }
            CliLine line = {s_room->line, sizeof(s_room->line), 0ull};
            cli_line_text(&line, "  ");
            cli_line_text(&line, file);
            cli_line_text(&line, "  ");
            cli_line_text(&line, bridge_text(entry, "kind"));
            cli_line_text(&line, "  ");
            bridge_at(&line, path, entry);
            cli_out_line(s_room->line, cli_line_fits(&line) ? line.at : 0ull);
        }
    }
}

static int bridge_words(const char *root, const char *word)
{
    const unsigned int bridge = cli_bridge_read(&s_room->json, root);
    const unsigned int klq = cli_json_member(&s_room->json, bridge, "klq");
    if (!cli_json_is(&s_room->json, klq, CFG_JSON_OBJECT))
    {
        cli_err("no Lstar.klq in " CLI_BRIDGE_DIR);
        return CLI_NO_CODE;
    }
    const unsigned int keys = cli_json_member(&s_room->json, klq, "keys");
    if (word != NULL)
    {
        const unsigned int key = cli_json_member(&s_room->json, keys, word);
        if (key != CLI_JSON_NONE)
        {
            bridge_pairs(bridge_text(klq, "path"), word, key);
            bridge_maps(bridge, word);
            bridge_rulesets(bridge, word);
            return 0;
        }
    }
    const unsigned int first =
        cli_json_is(&s_room->json, keys, CFG_JSON_OBJECT) ? s_room->json.node[keys].first : CLI_JSON_NONE;
    unsigned long long pad = 0ull;
    unsigned int count = 0u;
    for (unsigned int key = first; key != CLI_JSON_NONE; key = s_room->json.node[key].next)
    {
        const char *const name = s_room->json.text + s_room->json.node[key].key;
        if ((word == NULL) || cli_holds_word(name, word))
        {
            const unsigned long long letters =
                cli_text_letters(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
            pad = (letters > pad) ? letters : pad;
            count += 1u;
        }
    }
    if (count == 0u)
    {
        cli_err("no key holds that word");
        return CLI_NO_CODE;
    }
    for (unsigned int key = first; key != CLI_JSON_NONE; key = s_room->json.node[key].next)
    {
        const char *const name = s_room->json.text + s_room->json.node[key].key;
        if ((word != NULL) && !cli_holds_word(name, word))
        {
            continue;
        }
        CliLine line = {s_room->line, sizeof(s_room->line), 0ull};
        cli_line_text(&line, name);
        for (unsigned long long letters =
                 cli_text_letters(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
             letters < pad; letters += 1ull)
        {
            cli_line_character(&line, ' ');
        }
        cli_line_text(&line, "  ");
        bridge_tally(key, &line);
        bridge_out(&line);
    }
    return 0;
}

int cli_bridge_words(const char *root, const char *word)
{
    s_room = cli_cell_taken(sizeof(*s_room), "a showing of the bridge");
    if (s_room == NULL)
    {
        cli_err("a showing of the bridge could not be taken");
        return CLI_NO_CODE;
    }
    const int code = bridge_words(root, word);
    cli_json_given(&s_room->json);
    cli_cell_given(s_room);
    s_room = NULL;
    return code;
}
