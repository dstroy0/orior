// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_file_test.c: running a file's check: a path is quoted for the shell, every run line of the manifest closes its
// names, and a language without one is refused
#include "../../../../../ui/cli/src/runs/run_file.h"
#include "../../../../../ui/cli/src/toolchains/toolchains.h"
#include "cli_check.h"

int main(void)
{
    char quoted[256];
    CliLine line = {quoted, sizeof(quoted), 0ull};
    cli_run_file_quoted(&line, "a b/it's.py");
    cli_check_text(quoted, line.at, "'a b/it'\\''s.py'", "a path is quoted for the shell");
    static CliJson s_json;
    const unsigned int manifest = cli_toolchains_manifest(&s_json, 0u, 0ull);
    int closed = cli_json_is(&s_json, manifest, CFG_JSON_ARRAY);
    for (unsigned int tool = closed ? s_json.node[manifest].first : CLI_JSON_NONE; tool != CLI_JSON_NONE;
         tool = s_json.node[tool].next)
    {
        const unsigned int runs = cli_json_member(&s_json, tool, "runs");
        const unsigned int first =
            cli_json_is(&s_json, runs, CFG_JSON_OBJECT) ? s_json.node[runs].first : CLI_JSON_NONE;
        for (unsigned int run = first; run != CLI_JSON_NONE; run = s_json.node[run].next)
        {
            const char *const text = cli_json_string(&s_json, run, NULL);
            int open = 0;
            for (const char *at = (text != NULL) ? text : ""; *at != '\0'; at += 1)
            {
                open += (*at == '{') ? 1 : ((*at == '}') ? -1 : 0);
            }
            closed &= (open == 0);
        }
    }
    cli_json_given(&s_json);
    cli_check(closed, "every run line of the manifest closes each name it opens");
    char why[1024];
    char ran[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {ran, sizeof(ran), 0ull};
    const int refused = !cli_run_file_line(".", "a.json", "json", &out, &said);
    (void)cli_line_closed(&said);
    cli_check(
        refused &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = why, .cap = CLI_CELLS_POOL, .other = "no way to run", .other_cap = CLI_CELLS_POOL}) != NULL),
        "a language with no run line is refused");
    return cli_checked();
}
