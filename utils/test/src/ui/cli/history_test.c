// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// history_test.c: Local History's check: saves are kept newest first, without repeats, orior's own folder a folder
// of the run's
#include "../../../../../ui/cli/src/editor/history.h"
#include "cli_check.h"

int main(void)
{
    char home[CLI_PATH_LONGEST];
    cli_check_folder("history_home", home);
    (void)query_environment_set("ORIOR_HOME", home);
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    const int kept = cli_history_keep("/tree", "src/a.c", "zero", 4ull, "one", 3ull, &said) &&
                     cli_history_keep("/tree", "src/a.c", "one", 3ull, "one", 3ull, &said) &&
                     cli_history_keep("/tree", "src/a.c", "one", 3ull, "two", 3ull, &said);
    cli_check(kept, "three saves are kept");
    static char s_list[1u << 16u];
    CliLine list = {s_list, sizeof(s_list), 0ull};
    cli_history_list("/tree", "src/a.c", &list);
    unsigned int read = 0u;
    const CliJson *const json = cli_check_parsed(s_list, list.at, &read);
    static char s_texts[4096];
    CliLine texts = {s_texts, sizeof(s_texts), 0ull};
    for (unsigned int at = 0u; cli_json_item(json, read, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int when = cli_json_member(json, cli_json_item(json, read, at), "at");
        unsigned long long stamp = 0ull;
        for (unsigned long long digit = 0ull;
             cli_json_is(json, when, CFG_JSON_NUMBER) && (digit < json->node[when].length); digit += 1ull)
        {
            stamp = stamp * 10ull + (unsigned long long)(json->text[json->node[when].text + digit] - '0');
        }
        char text[64];
        CliLine one = {text, sizeof(text), 0ull};
        said.at = 0ull;
        (void)cli_history_read("/tree", "src/a.c", stamp, &one, &said);
        cli_line_text(&texts, (at > 0u) ? "," : "");
        cli_line_counted(&texts, text, cli_line_fits(&one) ? one.at : 0ull);
    }
    cli_check_text(s_texts, texts.at, "two,one,zero", "the saves read newest first, the repeat left out");
    list.at = 0ull;
    cli_history_list("/tree", "src/b.c", &list);
    cli_check_text(s_list, list.at, "[]", "a file never saved has no history");
    return cli_checked();
}
