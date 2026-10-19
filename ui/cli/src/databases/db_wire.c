// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_wire.c: a database server's failure made of a message alone, and written as the reader reads it
#include "db_wire.h"

void cli_db_failure_said(CliDbFailure *failure, const char *message)
{
    memor.set(&(MemoriaCfg){.dst = failure, .val = 0, .bytes = sizeof(*failure)});
    memor.cpy(&(MemoriaCfg){.dst = failure->message,
                            .src = message,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL}) + 1u});
}

void cli_db_failure_written(const CliDbFailure *failure, CliLine *out)
{
    cli_line_text(out, failure->message);
    if (failure->code[0] != '\0')
    {
        cli_line_text(out, " (");
        cli_line_text(out, failure->code);
        cli_line_character(out, ')');
    }
    if (failure->detail[0] != '\0')
    {
        cli_line_character(out, '\n');
        cli_line_text(out, failure->detail);
    }
    if (failure->hint[0] != '\0')
    {
        cli_line_character(out, '\n');
        cli_line_text(out, failure->hint);
    }
}
