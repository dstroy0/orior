// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// font_families.h: a family's font files found in the system's font folders, regular, bold, italic and bold italic, and
// the monospace fonts impero draws with (font_families.c)
#ifndef PRINT_FONT_FAMILIES_H
#define PRINT_FONT_FAMILIES_H
#include "embed_dispatch_layout.h"
#include "print.h"

typedef struct CliFontFamiliesCfg CliFontFamiliesCfg;

// the first family of `families`, `count` of them, whose regular face the system's font folders hold, each family its
// files regular, bold, italic and bold italic, an empty name for a face it lacks: each face's path into `paths`, which
// is sized for four of CLI_PATH_MAX_LEN, empty where the family or the face is not found; and the monospace fonts
// impero draws with, the first found of those the editor uses and those each system has
void cli_font_find(const CliFontFamiliesCfg *args);
void cli_font_monospace(const CliFontFamiliesCfg *args);

struct CliFontFamiliesCfg
{
    const char *const (*families)[4];
    unsigned int count;
    char (*paths)[CLI_PATH_MAX_LEN];
};

typedef struct
{
    void (*find)(const CliFontFamiliesCfg *args);
    void (*monospace)(const CliFontFamiliesCfg *args);
} CliFontFamiliesNs;
EMBED_TABLE_LAYOUT(CliFontFamiliesNs, find, monospace);
EMBED_TABLE_STORAGE CliFontFamiliesNs cli_font_families EMBED_UNUSED = {
    .find = cli_font_find,
    .monospace = cli_font_monospace,
};

#endif
