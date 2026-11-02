// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugin_catalog_install.c: a plugin of a catalog installed: its package fetched, its hash checked where the
// catalog gives one, its plugin.json checked against the entry's id and version, and its files written into a folder of
// its own beside the one they take the place of, then moved into place
#include "../text/digest.h"
#include "plugin_catalog_internal.h"

#include "../text/text.h"

typedef struct
{
    const char *name;
    const char *from;
    const char *id;
    const char *version;
    const char *sha256;
    VerbaLinea *err_msg;
    const char *staged;
    unsigned int files;
} PluginCatalogInstallCtx;
