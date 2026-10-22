// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shape_test.c: the structural search's check: an expression placeholder takes a call whole and its text is written
// back, a name placeholder takes one name and one named twice takes the same both times, statements and layout match
// in JavaScript past spaces and comments, and an expression last in a pattern takes the rest of its statement while
// strings are one token
#include "../../../../../ui/cli/src/code/shape_internal.h"
#include "../../../../../ui/cli/src/text/edits.h"
#include "cli_check.h"

// the matches found, their spans and written texts kept
static CliTextEdit s_edits[64];
static char s_with[1u << 14u];
static unsigned int s_count;
static unsigned int s_used;
static char s_after[1u << 14u];

// a match kept
static void shape_kept(void *context, const CliShapeFound *found)
{
    (void)context;
    if ((s_count >= 64u) || (s_used + found->with_size > sizeof(s_with)))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_with + s_used,
                            .src = found->has_with ? found->with : "",
                            .bytes = found->has_with ? found->with_size : 0ull});
    s_edits[s_count] =
        (CliTextEdit){found->from, found->to, s_with + s_used, found->has_with ? found->with_size : 0ull};
    // the texts kept are below the room's size, which an unsigned int holds
    s_used += (unsigned int)s_edits[s_count].size;
    s_count += 1u;
}

// the matches of `pattern` in `src`, written with `template_text` where it is not NULL: how many
static unsigned int shape_search(const char *language, const char *src, const char *pattern, const char *template_text)
{
    s_count = 0u;
    s_used = 0u;
    cli_shape_search(language, src, cellul.len(&(CatenaFinitaCfg){.src = src, .cap = CLI_CELLS_POOL}), pattern,
                     template_text, shape_kept, NULL);
    return s_count;
}

// `src` with each match of `pattern` written as `template_text`, checked against `wanted`
static void shape_replaced(const char *language, const char *src, const char *pattern, const char *template_text,
                           const char *wanted, const char *what)
{
    (void)shape_search(language, src, pattern, template_text);
    CliLine after = {s_after, sizeof(s_after), 0ull};
    (void)cli_text_applied(src, cellul.len(&(CatenaFinitaCfg){.src = src, .cap = CLI_CELLS_POOL}), s_edits, s_count,
                           &after);
    cli_check_text(s_after, after.at, wanted, what);
}

int main(void)
{
    static const char s_calls[] = "x = len( items ) + len(other(1, 2))\nprint(len(z))\n";
    const unsigned int count = shape_search("python", s_calls, "len($a$)", NULL);
    cli_check((count == 3u) && (s_edits[0].from.line == 0u) && (s_edits[0].from.col == 4u) &&
                  (s_edits[0].to.col == 16u) && (s_edits[1].from.line == 0u) && (s_edits[1].from.col == 19u) &&
                  (s_edits[1].to.col == 35u) && (s_edits[2].from.line == 1u) && (s_edits[2].from.col == 6u) &&
                  (s_edits[2].to.col == 12u),
              "an expression placeholder takes a call whole");
    shape_replaced("python", s_calls, "len($a$)", "count($a$)",
                   "x = count(items) + count(other(1, 2))\nprint(count(z))\n", "and its text is written back");
    static const char s_names[] = "a = a + 1\nb = c + 1\nd.e = d.e + 1\n";
    shape_replaced("python", s_names, "$x:name$ = $x:name$ + 1", "$x$ += 1", "a += 1\nb = c + 1\nd.e = d.e + 1\n",
                   "a name placeholder takes one name, the same both times");
    shape_replaced("python", s_names, "$x$ = $x$ + 1", "$x$ += 1", "a += 1\nb = c + 1\nd.e += 1\n",
                   "an expression named twice takes the same both times");
    static const char s_script[] = "if (ready) {\n  start(); // go\n  log(\"started\");\n}\nif(done){finish();}\n";
    cli_check(shape_search("javascript", s_script, "if ($c$) { $body:stmt$ }", NULL) == 2u,
              "statements match past spaces and comments");
    shape_replaced("javascript", s_script, "if ($c$) { $body:stmt$ }", "when($c$, () => { $body$ })",
                   "when(ready, () => { start(); // go\n  log(\"started\"); })\nwhen(done, () => { finish(); })\n",
                   "and are written back as they stood");
    static const char s_total[] = "total = a + b * c\nname = \"a + b\"\n";
    shape_replaced("python", s_total, "total = $v$", "total = ($v$)", "total = (a + b * c)\nname = \"a + b\"\n",
                   "an expression last in a pattern takes the rest of its statement");
    cli_check(shape_search("python", s_total, "a + b", NULL) == 1u, "and a string is one token");
    // a search of texts of a hundred bytes holds a room of their tokens, far below a fixed room of 2^22
    cli_check((g_cli_shape_cells.holds == 0ull) && (g_cli_shape_cells.hw != 0ull) &&
                  (g_cli_shape_cells.hw < (1ull << 16u)),
              "a search's room is sized to its code and pattern, and given back");
    return cli_checked();
}
