// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages.c: the window's pages through the CLI's page operations
#include "pages.h"

#include "../../cli/src/command_line/cli_internal.h"

static void *pages_reserve(unsigned long long bytes)
{
    return EMBED_CALL(cli_size_bytes_internal.pages_reserve, CliSizeBytesCtx, .size = bytes);
}

static int pages_commit(void *at, unsigned long long bytes)
{
    return EMBED_CALL(cli_size_bytes_internal.pages_commit, CliSizeBytesCtx, .bytes_out = at, .size = bytes);
}

static void pages_free(void *at, unsigned long long bytes)
{
    EMBED_CALL(cli_size_bytes_internal.pages_free, CliSizeBytesCtx, .bytes_out = at, .size = bytes);
}

const GuiBlockPages g_gui_pages = {pages_reserve, pages_commit, pages_free};
