// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tree_folder.h: a folder of the tree named from the folder of a source file, with no `..` in it, since the run's
// writer files no path that climbs
#ifndef TREE_FOLDER_H
#define TREE_FOLDER_H

#include "cellularum_laboro/cellularum_linea.h"
#include "verba_scribo/verba_linea.h"

#include <stddef.h>

// The folder `rest` names from the folder `ups` folders above the one that holds the source file `file`, into
// `folder`, `bytes` of it, its separators `/`. Where `file` holds no separator, the folder climbs from where the
// process is. 1 where the whole folder fit, else 0
static inline int tree_folder_from(const char *file, unsigned int ups, const char *rest, char *folder, size_t bytes)
{
    VerbaLinea line = mmgr_line_over(folder, bytes);
    size_t end = mmgr_text_length(file, (size_t)-1);
    size_t slash = end;
    for (size_t at = 0u; at < end; at += 1u)
    {
        slash = ((file[at] == '/') || (file[at] == '\\')) ? at : slash;
    }
    if (slash == end)
    {
        for (unsigned int up = 0u; up < ups; up += 1u)
        {
            mmgr_line_put(&line, "../");
        }
        mmgr_line_put(&line, rest);
        return (mmgr_line_finish(&line) != 0u) && (mmgr_line_ok(&line) != 0);
    }
    end = slash;
    for (unsigned int up = 0u; up < ups; up += 1u)
    {
        slash = 0u;
        for (size_t at = 0u; at < end; at += 1u)
        {
            slash = ((file[at] == '/') || (file[at] == '\\')) ? at : slash;
        }
        end = slash;
    }
    for (size_t at = 0u; at < end; at += 1u)
    {
        mmgr_line_ch(&line, (file[at] == '\\') ? '/' : file[at]);
    }
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, rest);
    return (mmgr_line_finish(&line) != 0u) && (mmgr_line_ok(&line) != 0);
}

#endif
