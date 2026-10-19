// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editorconfig.h: a file's indentation as `.editorconfig` files set it (editorconfig.c): each from the file's folder
// up, until one says `root = true` or the disk's root, the nearer over the farther, and in each the last section whose
// pattern names the file over those before it. Only indentation is read: `indent_style`, `indent_size`, which `tab`
// makes the tab's width, and `tab_width`.
//
// A section's pattern names a file by its name wherever it stands, or by its path from the folder of its
// `.editorconfig` where it has a `/`. `*` is any letters of a name, `**` any letters and folders, `?` one letter,
// `[abc]` and `[!abc]` one letter of or not of those, `{a,b}` either text, and `{1..3}` any whole number between
#ifndef EDITORCONFIG_H
#define EDITORCONFIG_H

#include "../command_line/cli_internal.h"

// The indentation found: tabs where `tabs` is 1, spaces where it is 0, and -1 where nothing set it; the width of a
// step in `size` where `sized` is 1
typedef struct
{
    int tabs;
    int sized;
    unsigned int size;
} CliIndent;

// the indentation the `.editorconfig` files above the file at `path` give it
void cli_indent(const char *path, CliIndent *indent);
// whether a section's pattern, `size` bytes, names the file at `relative` from the `.editorconfig`'s folder, whose
// name is `name` (editorconfig_names.c)
int cli_editorconfig_names(const char *pattern, unsigned long long size, const char *relative, const char *name);

#endif
