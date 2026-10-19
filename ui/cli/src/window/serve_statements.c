// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_statements.c: the statements of a text the database window holds, read with no connection: the one at a place
// of the text, and every one, each as its kind of database splits them
#include "../databases/db.h"
#include "serve_internal.h"

int cli_serve_db_statement_at(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const text = cli_serve_string(args, "text", said);
    unsigned long long offset = 0ull;
    const char *const kind = (text != NULL) ? cli_serve_string(args, "kind", said) : NULL;
    if ((kind == NULL) || !cli_serve_whole(args, "offset", &offset, said))
    {
        return 0;
    }
    cli_db_statement_at(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), offset, kind, out);
    return 1;
}

int cli_serve_db_statements(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const text = cli_serve_string(args, "text", said);
    const char *const kind = (text != NULL) ? cli_serve_string(args, "kind", said) : NULL;
    if (kind == NULL)
    {
        return 0;
    }
    cli_db_statements(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), kind, out);
    return 1;
}
