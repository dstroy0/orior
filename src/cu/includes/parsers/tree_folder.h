// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tree_folder.h: a folder of the tree named from the folder of a source file, with no `..` in it, since the run's
// writer files no path that climbs
#ifndef TREE_FOLDER_H
#define TREE_FOLDER_H

#include <string>

// The folder `rest` names from the folder `ups` folders above the one holding the source file `file`, its separators
// `/`: `file` itself where it holds no separator, so that the folder is read from where the process stands
static inline std::string tree_folder_from(const std::string &file, unsigned int ups, const char *rest)
{
    std::string folder = file;
    for (char &each : folder)
    {
        each = (each == '\\') ? '/' : each;
    }
    size_t slash = folder.find_last_of('/');
    if (slash == std::string::npos)
    {
        std::string climbed;
        for (unsigned int up = 0u; up < ups; up += 1u)
        {
            climbed += "../";
        }
        return climbed + rest;
    }
    folder.resize(slash);
    for (unsigned int up = 0u; up < ups; up += 1u)
    {
        slash = folder.find_last_of('/');
        folder.resize((slash == std::string::npos) ? 0u : slash);
    }
    return folder + "/" + rest;
}

#endif
