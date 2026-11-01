// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// keyed_test.c: the keepers' check: the bytes every key and the text beside it list to are the bytes they write
#include "../../../../../ui/cli/src/command_line/cells.h"
#include "../../../../../ui/cli/src/text/keyed.h"
#include "cli_check.h"

// a key and the text beside it put on the line that is `context`, each ended by a 0
static void keyed_listed_put(void *context, const char *key, const char *beside, const char *text,
                             unsigned long long size)
{
    (void)text;
    (void)size;
    VerbaLinea *const line = context;
    (void)mmgr_line_put_n(line, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1ull);
    (void)mmgr_line_put_n(line, beside, cellul.len(&(CatenaFinitaCfg){.src = beside, .cap = CLI_CELLS_POOL}) + 1ull);
}

// the size a keeper lists to, and whether a line of that size and its 0 holds what it lists
static void keyed_listed_is(const CliKeyed *keyed, unsigned long long wanted, const char *what)
{
    const unsigned long long size = cli_keyed_listed_size(keyed);
    cli_check(size == wanted, what);
    char text[256];
    VerbaLinea line = {text, size + 1ull, 0ull};
    cli_keyed_each(keyed, keyed_listed_put, &line);
    cli_check(mmgr_line_ok(&line) && (line.at == size), "and a line of that size and its 0 holds them");
    VerbaLinea short_line = {text, (size > 0ull) ? size : 1ull, 0ull};
    cli_keyed_each(keyed, keyed_listed_put, &short_line);
    cli_check((size == 0ull) || !mmgr_line_ok(&short_line), "and a line a byte shorter does not");
}

static void keyed_listed(void)
{
    static CliKeyed s_keyed;
    cli_keyed_begun(&s_keyed, "a check's texts", 16u);
    keyed_listed_is(&s_keyed, 0ull, "nothing stored lists to nothing");
    (void)cli_keyed_put(&s_keyed, "one", "beside", "text", 4ull);
    (void)cli_keyed_put(&s_keyed, "three", "", "", 0ull);
    keyed_listed_is(&s_keyed, 18ull, "each key and the text beside it count, each with its 0, and no text");
    (void)cli_keyed_put(&s_keyed, "one", "b", "a longer text than it was", 25ull);
    keyed_listed_is(&s_keyed, 13ull, "an entry put again counts as it now stands");
    cli_keyed_remove(&s_keyed, "three");
    keyed_listed_is(&s_keyed, 6ull, "an entry taken out no longer counts");
    cli_keyed_clear(&s_keyed);
    keyed_listed_is(&s_keyed, 0ull, "nor does any once the keeper is emptied");
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
    keyed_listed();
    return cli_checked();
}
