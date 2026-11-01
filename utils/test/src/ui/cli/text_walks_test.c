// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// text_walks_test.c: the walks over a text's bytes answer as a walk of one byte at a time does, from every place and
// over every length about a lane's and a word's width, and read nothing past the bounds they are given
#include "../../../../../ui/cli/src/text/text.h"
#include "cli_check.h"

#define WALKS_BYTES 300u

static char s_text[WALKS_BYTES + 1u];

static int walks_in(char byte, const char *set)
{
    for (const char *one = set; *one != '\0'; one += 1)
    {
        if (*one == byte)
        {
            return 1;
        }
    }
    return 0;
}

// the bytes laid so that a byte of the set falls at the places a lane's and a word's walk turn on
static void walks_laid(unsigned int every)
{
    for (unsigned int at = 0u; at < WALKS_BYTES; at += 1u)
    {
        s_text[at] = ((every != 0u) && ((at % every) == (every - 1u))) ? '\n' : ((at % 2u) != 0u) ? ' ' : 'x';
    }
    s_text[WALKS_BYTES] = '\n';
}

static int walks_agree(const char *set, unsigned int every)
{
    walks_laid(every);
    int agree = 1;
    for (unsigned int size = 0u; agree && (size <= WALKS_BYTES); size += (size < 80u) ? 1u : 37u)
    {
        for (unsigned int at = 0u; agree && (at <= size); at += (at < 80u) ? 1u : 29u)
        {
            unsigned int to = at;
            unsigned int past = at;
            unsigned int back = at;
            unsigned int back_past = at;
            while ((to < size) && !walks_in(s_text[to], set))
            {
                to += 1u;
            }
            while ((past < size) && walks_in(s_text[past], set))
            {
                past += 1u;
            }
            while ((back > 0u) && !walks_in(s_text[back - 1u], set))
            {
                back -= 1u;
            }
            while ((back_past > 0u) && walks_in(s_text[back_past - 1u], set))
            {
                back_past -= 1u;
            }
            agree = (cli_text_to_any(s_text, at, size, set) == to) && (cli_text_past_any(s_text, at, size, set) == past) &&
                    (cli_text_back_any(s_text, at, set) == back) && (cli_text_back_past(s_text, at, set) == back_past);
        }
    }
    return agree;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
    cli_check(walks_agree("\n", 0u), "a byte the text holds nowhere is walked to the end, and to the start");
    cli_check(walks_agree("\n", 7u), "one byte is found where it first and last stands");
    cli_check(walks_agree("\n", 97u), "and past a lane's width");
    cli_check(walks_agree(" x", 7u), "a set of two walks to the first and past the run of them");
    cli_check(walks_agree(CLI_TEXT_WHITE, 31u), "and the whitespace");
    cli_check(cli_text_to_any(s_text, 9u, 3u, "x") == 9u, "a place past the bound stands where it is");
    cli_check(cli_text_past_any(s_text, 9u, 3u, "x") == 9u, "for both walks forward");
    const CliTextLine first = cli_text_line_at("ab\r\ncd\n\ne\r", 0ull, 11ull);
    cli_check((first.stop == 3ull) && (first.cut == 2ull) && (first.next == 4ull),
              "a line stops at its newline, its carriage return left out of where it is cut");
    const CliTextLine second = cli_text_line_at("ab\r\ncd\n\ne\r", 4ull, 11ull);
    cli_check((second.stop == 6ull) && (second.cut == 6ull) && (second.next == 7ull),
              "a line with no carriage return is cut where it stops");
    const CliTextLine empty = cli_text_line_at("ab\r\ncd\n\ne\r", 7ull, 11ull);
    cli_check((empty.stop == 7ull) && (empty.cut == 7ull) && (empty.next == 8ull), "an empty line has no letters");
    const CliTextLine last = cli_text_line_at("ab\r\ncd\n\ne\r", 8ull, 11ull);
    cli_check((last.stop == 11ull) && (last.cut == 11ull) && (last.next == 11ull),
              "a last line with no newline stops at the end and keeps its carriage return");
    char copied[8];
    cli_text_copied_or(copied, "ab", "c");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = copied, .other = "ab", .cap = sizeof(copied)}), "a text is copied");
    cli_text_copied_or(copied, NULL, "c");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = copied, .other = "c", .cap = sizeof(copied)}),
              "and what stands for it where it is NULL");
    char upper[4] = "aZ9";
    cli_text_raised(upper, 3ull);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = upper, .other = "AZ9", .cap = sizeof(upper)}), "letters are raised");
    unsigned long long past = 0ull;
    cli_check((cli_text_number("  42x", 2ull, 5ull, &past) == 42ull) && (past == 4ull),
              "a number reads its digits and answers where they stop");
    cli_check(cli_text_to_pair("a--b", 0ull, 4ull, '-', 'b') == 2ull, "a pair is found where it stands");
    cli_check(cli_text_to_pair("a--b", 0ull, 4ull, 'x', 'y') == 4ull, "and the end where it stands nowhere");
    cli_check(cli_text_past_class("ab1 ", 0ull, 4ull, CLI_TEXT_LETTERS | CLI_TEXT_DIGITS, "") == 3ull,
              "a walk past classes stops at the first byte of neither");
    return cli_checked();
}
