// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_test.c: the patterns' check: characters, classes and quantifiers; sticky at a place with anchors and
// boundaries; alternation, groups, lookahead and laziness; backreferences and case; Unicode's properties under the u
// flag; what is not read is refused; a string of any length is taken a pass at a time; a repetition taken a pass at a
// time matches as backtracking does; one that goes deeper gives up before the stack runs out; and a runaway pattern
// gives up. Each runs on the program's own thread, its stack the system's, which holds more than a match may take
#include "../../../../../ui/cli/src/text/regexp.h"
#include "cli_check.h"

static CliRegexpRoom s_room;
static CliRegexp s_regexp;
static unsigned int s_letters[1u << 18u];
static unsigned long long s_offset[(1u << 18u) + 1u];
static CliRegexpSpan s_groups[CLI_REGEXP_GROUPS + 1u];
static char s_text[1u << 18u];
static char s_found[1u << 18u];

// `source` with `flags` read: 1, or 0 where it is refused
static int regexp_read(const char *source, const char *flags)
{
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    cli_regexp_roomed(&s_regexp, &s_room);
    return cli_regexp_read(&s_regexp, source, flags, &said);
}

// `text` decoded into s_letters, the byte each letter starts at into s_offset: the count of letters
static unsigned long long regexp_letters(const char *text)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    const unsigned long long count =
        cli_regexp_letters(text, size, s_letters, sizeof(s_letters) / sizeof(s_letters[0]));
    unsigned long long letter = 0ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        if ((((unsigned char)text[at]) & 0xc0u) != 0x80u)
        {
            s_offset[letter] = at;
            letter += 1ull;
        }
    }
    s_offset[count] = size;
    return count;
}

// the match of `source` with `flags` at letter `from` of `text`, into s_found: 1 with the letters it takes into
// `taken`, or 0 where none matches there
static int regexp_at(const char *source, const char *flags, const char *text, unsigned long long from,
                     unsigned long long *taken)
{
    const unsigned long long count = regexp_letters(text);
    if (!regexp_read(source, flags) || !cli_regexp_match_into(&s_regexp, s_letters, count, from, s_groups))
    {
        return 0;
    }
    const unsigned long long start = s_offset[s_groups[0].from];
    const unsigned long long end = s_offset[s_groups[0].to];
    memor.cpy(&(MemoriaCfg){.dst = s_found, .src = text + start, .bytes = end - start});
    s_found[end - start] = '\0';
    *taken = s_groups[0].to - s_groups[0].from;
    return 1;
}

// the match of `source` with `flags` at letter `from` of `text` checked against `wanted`, NULL where none should match
static void regexp_is(const char *source, const char *flags, const char *text, unsigned long long from,
                      const char *wanted)
{
    unsigned long long taken = 0ull;
    const int matched = regexp_at(source, flags, text, from, &taken);
    char what[1024];
    CliLine line = {what, sizeof(what), 0ull};
    cli_line_character(&line, '/');
    cli_line_text(&line, source);
    cli_line_text(&line, "/ at ");
    cli_line_text(&line, text);
    (void)cli_line_closed(&line);
    cli_check(
        (wanted == NULL)
            ? !matched
            : (matched && (cellul.eq(&(CatenaFinitaCfg){.src = s_found, .other = wanted, .cap = CLI_CELLS_POOL}))),
        what);
}

// each group of `source`'s match at the start of `text`, joined by `|`, `-` for one that took no part, against `wanted`
static void regexp_groups(const char *source, const char *text, const char *wanted)
{
    const unsigned long long count = regexp_letters(text);
    char out[256];
    CliLine line = {out, sizeof(out), 0ull};
    if (regexp_read(source, "") && cli_regexp_match_into(&s_regexp, s_letters, count, 0ull, s_groups))
    {
        for (unsigned int group = 0u; group <= s_regexp.groups; group += 1u)
        {
            cli_line_text(&line, (group > 0u) ? "|" : "");
            if (s_groups[group].set)
            {
                const unsigned long long start = s_offset[s_groups[group].from];
                cli_line_counted(&line, text + start, s_offset[s_groups[group].to] - start);
            }
            else
            {
                cli_line_character(&line, '-');
            }
        }
    }
    cli_check_text(out, line.at, wanted, source);
}

// characters, classes and quantifiers
static void regexp_characters(void)
{
    regexp_is("\\d+", "", "123abc", 0ull, "123");
    regexp_is("[a-z_][\\w$]*", "", "abc_1$ x", 0ull, "abc_1$");
    regexp_is("[^\\]\\\\]+", "", "ab\\]", 0ull, "ab");
    regexp_is("0[xX][\\da-fA-F_]+[uUlLzZ]*", "", "0xFFul;", 0ull, "0xFFul");
    regexp_is("#[\\da-fA-F]{3,8}\\b", "", "#fff;", 0ull, "#fff");
    regexp_is("#[\\da-fA-F]{3,8}\\b", "", "#ffg", 0ull, NULL);
    regexp_is("a{2}", "", "aaa", 0ull, "aa");
    regexp_is("a{2,}", "", "aaaa", 0ull, "aaaa");
    regexp_is("a{,2}", "", "a{,2}", 0ull, "a{,2}");
    regexp_is("\\.", "", "x", 0ull, NULL);
    regexp_is(".*", "", "ab\ncd", 0ull, "ab");
    regexp_is("A\\x42\\u{43}", "", "ABC", 0ull, "ABC");
}

// sticky at a place, with anchors and boundaries
static void regexp_sticky(void)
{
    regexp_is("\\w+", "", "  word", 2ull, "word");
    regexp_is("\\w+", "", "  word", 0ull, NULL);
    regexp_is("^\\s*:\\w+", "", "  :label", 0ull, "  :label");
    regexp_is("^:\\w+", "", "x:label", 1ull, NULL);
    regexp_is("\\/\\/.*$", "", "a // note", 2ull, "// note");
    regexp_is("$", "", "ab", 2ull, "");
    regexp_is("\\bif\\b", "", "if(", 0ull, "if");
    regexp_is("\\bif\\b", "", "iff", 0ull, NULL);
}

// alternation, groups, lookahead and laziness
static void regexp_alternation(void)
{
    regexp_is("(?:\\d[\\d_']*)?\\.?\\d[\\d_']*(?:[eE][\\-+]?\\d+)?", "", "3.25e-4 ", 0ull, "3.25e-4");
    regexp_is("[\\w-]+(?=\\s*:)", "", "color : red", 0ull, "color");
    regexp_is("[\\w-]+(?=\\s*:)", "", "color red", 0ull, NULL);
    regexp_is("\\w+(?!\\()", "", "call(", 0ull, "cal");
    regexp_is("cat|category", "", "category", 0ull, "cat");
    regexp_is("<.*?>", "", "<a><b>", 0ull, "<a>");
    regexp_is("<.*>", "", "<a><b>", 0ull, "<a><b>");
    regexp_is("(ab)+", "", "ababx", 0ull, "abab");
    regexp_is("(a|ab)(c|bcd)(d*)", "", "abcd", 0ull, "abcd");
    regexp_is("(a*)*b", "", "aaab", 0ull, "aaab");
    regexp_is("(x?)+$", "", "", 0ull, "");
    regexp_is("[]a", "", "a", 0ull, NULL);
    regexp_is("[^]+", "", "a]b", 0ull, "a]b");
    regexp_groups("(\\w+)\\s(\\w+)", "one two", "one two|one|two");
    regexp_groups("(a)|(b)", "b", "b|-|b");
}

// backreferences and case, and Unicode's properties under the u flag
static void regexp_backreferences(void)
{
    regexp_is("[rR](['\"])(-*)[\\[({].*?[\\])}]\\2\\1", "", "r\"-(x)-\" tail", 0ull, "r\"-(x)-\"");
    regexp_is("(['\"]).*?\\1", "", "'a\"b' c", 0ull, "'a\"b'");
    regexp_is("select", "i", "SELECT x", 0ull, "SELECT");
    regexp_is("[a-z]+", "i", "MiXed1", 0ull, "MiXed");
    regexp_is("select", "", "SELECT", 0ull, NULL);
    regexp_is("[\\p{L}_][\\p{L}\\p{N}_']*", "u", "\xCE\xB1\xCE\xB2_1' x", 0ull, "\xCE\xB1\xCE\xB2_1'");
    regexp_is("\\P{N}+", "u", "ab3", 0ull, "ab");
    regexp_is("\\p{Lu}\\p{Ll}+", "u", "\xC3\x8Atre!", 0ull, "\xC3\x8Atre");
    regexp_is("\\p{L}", "", "p{L}", 0ull, "p{L}");
}

// what is not read is refused
static void regexp_refused(void)
{
    cli_check(!regexp_read("(?<=a)b", ""), "lookbehind is refused");
    cli_check(!regexp_read("(?<name>a)", ""), "a named group is refused");
    cli_check(!regexp_read("\\p{Script=Greek}", "u"), "a property not read is refused");
    cli_check(!regexp_read("(a", ""), "a group not closed is refused");
    cli_check(!regexp_read("a)", ""), "a group not opened is refused");
    cli_check(!regexp_read("*a", ""), "a quantifier of nothing is refused");
}

// a string of any length is taken a pass at a time
static void regexp_string(void)
{
    static const char s_string[] = "\"(?:[^\"\\\\]|\\\\.)*\"";
    static const char s_keyed[] = "\"(?:[^\"\\\\]|\\\\.)*\"(?=\\s*:)";
    regexp_is(s_string, "", "\"a\\\"b\\\\\" rest", 0ull, "\"a\\\"b\\\\\"");
    regexp_is(s_keyed, "", "\"key\" : 1", 0ull, "\"key\"");
    regexp_is(s_keyed, "", "\"value\", 1", 0ull, NULL);
    CliLine line = {s_text, sizeof(s_text), 0ull};
    cli_line_character(&line, '"');
    for (unsigned int at = 0u; at < 200000u; at += 1u)
    {
        cli_line_character(&line, ((at % 997u) == 0u) ? '\\' : 'x');
    }
    cli_line_text(&line, "\", next");
    (void)cli_line_closed(&line);
    unsigned long long taken = 0ull;
    cli_check(regexp_at(s_string, "", s_text, 0ull, &taken) && (taken == 200002ull),
              "the whole string, its escapes taken with the character after each");
}

// a repetition taken a pass at a time matches as backtracking does
static void regexp_repetition(void)
{
    regexp_is("(?:ab|c)*", "", "ababcx", 0ull, "ababc");
    regexp_is("(?:ab|c)+?c", "", "abcc", 0ull, "abc");
    regexp_is("(?:ab|c){2}", "", "ababab", 0ull, "abab");
    regexp_is("(?:ab|c){2,3}c", "", "ababcc", 0ull, "ababcc");
    regexp_is("(?:ab|cd)*e", "", "ababcde", 0ull, "ababcde");
    regexp_is("(?:ab|cd)+", "", "ac", 0ull, NULL);
    regexp_is("(?:ab|a)*", "", "aba", 0ull, "aba");
    regexp_is("(?:a|b)+", "i", "AbAc", 0ull, "AbA");
    regexp_is("(?:\\d|x)+", "", "1x2y", 0ull, "1x2");
}

// `count` letters a and the text `after` into s_text
static void regexp_run(unsigned int count, const char *after)
{
    memor.set(&(MemoriaCfg){.dst = s_text, .val = 'a', .bytes = count});
    memor.cpy(&(MemoriaCfg){.dst = s_text + count,
                            .src = after,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = after, .cap = CLI_CELLS_POOL}) + 1u});
}

int main(void)
{
    regexp_characters();
    regexp_sticky();
    regexp_alternation();
    regexp_backreferences();
    regexp_refused();
    regexp_string();
    regexp_repetition();
    unsigned long long taken = 0ull;
    regexp_run(100000u, "!");
    cli_check(!regexp_at("(?:a|ab)*!", "", s_text, 0ull, &taken), "a repetition that goes deeper gives up");
    regexp_run(20u, "!");
    cli_check(regexp_at("(?:a|ab)*!", "", s_text, 0ull, &taken) && (taken == 21ull), "inside its stack it matches");
    regexp_run(40u, "!");
    cli_check(!regexp_at("(a|aa)+$", "", s_text, 0ull, &taken), "a runaway pattern gives up");
    return cli_checked();
}
