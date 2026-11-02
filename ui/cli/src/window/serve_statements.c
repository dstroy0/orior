// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_statements.c: the statements of a text the database window has open, read with no connection: the one at a
// place of the text, and every one, each as its kind of database splits them
#include "../databases/db.h"
#include "serve_internal.h"

int cli_serve_db_statement_at(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const text = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "text", .err_msg = err_msg);
    unsigned long long offset = 0ull;
    const char *const kind = (text != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "kind", .err_msg = err_msg)
                                            : NULL;
    if ((kind == NULL) || !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments,
                                      .key = "offset", .value = &offset, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_db_out_size.statement_at, CliDbOutSizeCfg, .text = text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), .offset = offset,
               .kind = kind, .out = out);
    return 1;
}

int cli_serve_db_statements(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const text = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "text", .err_msg = err_msg);
    const char *const kind = (text != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "kind", .err_msg = err_msg)
                                            : NULL;
    if (kind == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_db_out_size.statements, CliDbOutSizeCfg, .text = text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), .kind = kind, .out = out);
    return 1;
}
