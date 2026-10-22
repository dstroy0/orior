// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_chain_text.c: a chain of settings as the window reads it and hands it back, and a chain's tree given back
#include "../text/text.h"
#include "settings_internal.h"

void cli_settings_chain_written(const CliChain *chain, CliLine *out)
{
    cli_line_text(out, "{\"files\":[");
    for (unsigned int at = 0u; at < chain->files; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? ",{\"file\":" : "{\"file\":");
        cli_json_quoted(out, chain->file[at],
                        cellul.len(&(CatenaFinitaCfg){.src = chain->file[at], .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"values\":");
        cli_json_write(&chain->json, chain->values[at], out, 0u, 0);
        cli_line_character(out, '}');
    }
    cli_line_text(out, "],\"problems\":[");
    for (unsigned int at = 0u; at < chain->problems; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, chain->problem[at],
                        cellul.len(&(CatenaFinitaCfg){.src = chain->problem[at], .cap = CLI_CELLS_POOL}));
    }
    cli_line_text(out, "]}");
}

int cli_settings_chain_read(const char *text, unsigned long long length, CliChain *chain, CliLine *said)
{
    cli_json_begun(&chain->json, NULL, 0u, NULL, 0ull);
    chain->files = 0u;
    chain->problems = 0u;
    const unsigned int root = cli_json_taken(&chain->json, text, length, 0u, 0ull, said);
    if (!cli_json_is(&chain->json, root, CFG_JSON_OBJECT))
    {
        return 0;
    }
    const unsigned int files = cli_json_member(&chain->json, root, "files");
    for (unsigned int item = cli_json_is(&chain->json, files, CFG_JSON_ARRAY) ? chain->json.node[files].first
                                                                              : CLI_JSON_NONE;
         (item != CLI_JSON_NONE) && (chain->files < CLI_SETTINGS_DEEPEST); item = chain->json.node[item].next)
    {
        const char *const file = cli_json_string(&chain->json, cli_json_member(&chain->json, item, "file"), NULL);
        const unsigned int values = cli_json_member(&chain->json, item, "values");
        if ((file == NULL) || !cli_json_is(&chain->json, values, CFG_JSON_OBJECT))
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = chain->file[chain->files],
                                .src = file,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u});
        chain->values[chain->files] = values;
        chain->files += 1u;
    }
    const unsigned int problems = cli_json_member(&chain->json, root, "problems");
    for (unsigned int item = cli_json_is(&chain->json, problems, CFG_JSON_ARRAY) ? chain->json.node[problems].first
                                                                                 : CLI_JSON_NONE;
         (item != CLI_JSON_NONE) && (chain->problems < CLI_SETTINGS_PROBLEMS); item = chain->json.node[item].next)
    {
        const char *const problem = cli_json_string(&chain->json, item, NULL);
        if (problem != NULL)
        {
            CliLine line = {chain->problem[chain->problems], CLI_PATH_LONGEST * 2u, 0ull};
            cli_line_text(&line, problem);
            cli_line_closed(&line);
            chain->problems += 1u;
        }
    }
    return 1;
}

void cli_settings_chain_given(CliChain *chain)
{
    cli_json_given(&chain->json);
    chain->files = 0u;
    chain->problems = 0u;
}
