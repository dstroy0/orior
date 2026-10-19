// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure_test.c: the structural compare's check: tokens are names, numbers, strings and signs; layout alone is no
// change; a line moved reads as moved and a name changed everywhere as renamed; a block moved takes its closing sign
// along; a token changed is marked where it stands; and a name changed two ways is not renamed
#include "../../../../../ui/cli/src/code/structure.h"
#include "../../../../../ui/cli/src/code/structure_internal.h"
#include "cli_check.h"

static CliSide s_side;
static char s_out[1u << 16u];
static const CliJson *s_json;

// `then` compared with `now`, read into the shared tree: its node
static unsigned int structure_compared(const char *then, const char *now)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_structure_compare(then, cellul.len(&(CatenaFinitaCfg){.src = then, .cap = CLI_CELLS_POOL}), now,
                          cellul.len(&(CatenaFinitaCfg){.src = now, .cap = CLI_CELLS_POOL}), &out);
    unsigned int found = CLI_JSON_NONE;
    s_json = cli_check_parsed(s_out, out.at, &found);
    return found;
}

// a member of the compare written compact, checked against `wanted`
static void structure_member(unsigned int found, const char *name, const char *wanted, const char *what)
{
    char text[512];
    CliLine line = {text, sizeof(text), 0ull};
    cli_json_write(s_json, cli_json_member(s_json, found, name), &line, 0u, 0);
    cli_check_text(text, line.at, wanted, what);
}

// the whole number at `at` of an array, -1 where it is none
static long long structure_at(unsigned int array, unsigned int at)
{
    const unsigned int number = cli_json_item(s_json, array, at);
    long long value = cli_json_is(s_json, number, CFG_JSON_NUMBER) ? 0ll : -1ll;
    for (unsigned long long digit = 0ull; (value >= 0ll) && (digit < s_json->node[number].length); digit += 1ull)
    {
        value = value * 10ll + (long long)(s_json->text[s_json->node[number].text + digit] - '0');
    }
    return value;
}

// whether every mark of the array `name`, where `line` is not -1 those on that line, is of `kind`
static int structure_marks_all(unsigned int found, const char *name, long long line, long long kind)
{
    const unsigned int marks = cli_json_member(s_json, found, name);
    int all = 1;
    for (unsigned int at = 0u; cli_json_item(s_json, marks, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int mark = cli_json_item(s_json, marks, at);
        all &= ((line != -1ll) && (structure_at(mark, 0u) != line)) || (structure_at(mark, 3u) == kind);
    }
    return all;
}

// tokens are names, numbers, strings and signs
static void structure_tokens(void)
{
    static const char s_text[] = "let x_1 = 3.5 + \"a \\\" b\";\n  \xC3\xA9(y)";
    static const char *const s_wanted[9] = {"let", "x_1", "=", "3.5", "+", "\"a \\\" b\"", ";", "\xC3\xA9", "("};
    cli_check(cli_structure_lex(s_text, sizeof(s_text) - 1u, &s_side) && (s_side.tokens == 11u), "the text lexes");
    int named = 1;
    for (unsigned int at = 0u; named && (at < 9u); at += 1u)
    {
        const CliToken *const token = &s_side.token[at];
        named = ((token->end - token->start) ==
                 cellul.len(&(CatenaFinitaCfg){.src = s_wanted[at], .cap = CLI_CELLS_POOL})) &&
                (memor.cmp(&(MemoriaCfg){
                     .src = s_text + token->start, .other = s_wanted[at], .bytes = token->end - token->start}) == 0);
    }
    const CliToken *const last = &s_side.token[s_side.tokens - 1u];
    cli_check(named, "tokens are names, numbers, strings and signs");
    cli_check((last->line == 1u) && (last->from == 5u) && (last->to == 6u), "each placed in UTF-16 on its line");
}

int main(void)
{
    structure_tokens();
    unsigned int found = structure_compared("f(a, b);\nreturn x;\n", "f(\n    a,\n    b\n);\n\nreturn   x;\n");
    structure_member(found, "left_marks", "[]", "layout alone marks no token on the left");
    structure_member(found, "right_marks", "[]", "nor on the right");
    const unsigned int right = cli_json_member(s_json, found, "right");
    int kinds = 1;
    for (unsigned int at = 0u; cli_json_item(s_json, right, at) != CLI_JSON_NONE; at += 1u)
    {
        const long long kind = structure_at(right, at);
        kinds &= (kind == CLI_SAME) || (kind == CLI_RESHAPED);
    }
    const unsigned int reshaped = cli_json_member(s_json, found, "reshaped");
    cli_check(kinds && cli_json_is(s_json, reshaped, CFG_JSON_NUMBER) &&
                  (s_json->text[s_json->node[reshaped].text] != '0'),
              "its lines are the same or reshaped, some reshaped");
    found = structure_compared("fn one() {}\nfn two() {}\nlet count = count + 1;\nprint(count);\n",
                               "fn two() {}\nlet total = total + 1;\nprint(total);\nfn one() {}\n");
    structure_member(found, "moved", "1", "a line moved reads as moved");
    const unsigned int left = cli_json_member(s_json, found, "left");
    const unsigned int right_kinds = cli_json_member(s_json, found, "right");
    cli_check((structure_at(left, 0u) == CLI_MOVED) && (structure_at(right_kinds, 3u) == CLI_MOVED), "on both sides");
    structure_member(found, "renames", "[[\"count\",\"total\",3]]", "a name changed everywhere is renamed");
    cli_check((structure_at(right_kinds, 1u) == CLI_RENAMED) && (structure_at(right_kinds, 2u) == CLI_RENAMED) &&
                  structure_marks_all(found, "right_marks", 1ll, CLI_RENAMED),
              "and its lines and tokens read as renamed");
    found = structure_compared("function one() {\n  return 1;\n}\nfunction two() {\n  return 2;\n}\n",
                               "function two() {\n  return 2;\n}\nfunction one() {\n  return 1;\n}\n");
    structure_member(found, "moved", "3", "a block moved is moved whole");
    const unsigned int block_left = cli_json_member(s_json, found, "left");
    const unsigned int block_right = cli_json_member(s_json, found, "right");
    unsigned int moved_right = 0u;
    for (unsigned int at = 0u; cli_json_item(s_json, block_right, at) != CLI_JSON_NONE; at += 1u)
    {
        moved_right += (structure_at(block_right, at) == CLI_MOVED) ? 1u : 0u;
    }
    // every closing sign is alike: the one that moved on the right is whichever the lines kept leave
    cli_check((structure_at(block_left, 0u) == CLI_MOVED) && (structure_at(block_left, 1u) == CLI_MOVED) &&
                  (structure_at(block_left, 2u) == CLI_MOVED) && (moved_right == 3u),
              "its closing sign along");
    cli_check(structure_marks_all(found, "left_marks", -1ll, CLI_MOVED) &&
                  structure_marks_all(found, "right_marks", -1ll, CLI_MOVED),
              "and each of its tokens is marked moved");
    found = structure_compared("let a = 1;\n", "let a = 2;\n");
    structure_member(found, "left_marks", "[[0,8,9,1]]", "a token changed is marked where it stands");
    structure_member(found, "right_marks", "[[0,8,9,1]]", "on both sides");
    structure_member(found, "rows", "[[0,0],[1,1]]", "the lines stand side by side");
    structure_member(found, "starts", "[0]", "the change starts at its row");
    structure_member(found, "renames", "[]", "and nothing is renamed");
    found = structure_compared("a(x);\nb(x);\n", "a(y);\nb(z);\n");
    structure_member(found, "renames", "[]", "a name changed two ways is not renamed");
    cli_check(structure_marks_all(found, "right_marks", -1ll, CLI_CHANGED), "but changed");
    return cli_checked();
}
