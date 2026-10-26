// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_write.c: files and folders made in the tree, and a file's text written beside it and moved over it, so a failed
// write leaves the file as it was
#include "archive.h"
#include "files.h"

// that the path is there already, reported
static void cli_there_already(const char *path, VerbaLinea *err_msg)
{
    mmgr_line_put(err_msg, path);
    mmgr_line_put(err_msg, " is there already");
}

// that the path could not be written, reported
static void cli_unwritten(const char *path, VerbaLinea *err_msg)
{
    mmgr_line_put(err_msg, path);
    mmgr_line_put(err_msg, ": it could not be written");
}

int cli_files_create_file(const char *root, const char *file, VerbaLinea *err_msg)
{
    char path[CLI_PATH_MAX_LEN];
    if (!cli_inside(root, file, path, err_msg))
    {
        return 0;
    }
    if (query_is_file(path) || query_is_folder(path))
    {
        cli_there_already(file, err_msg);
        return 0;
    }
    char parent[CLI_PATH_MAX_LEN];
    if ((cli_path_parent(path, parent) && !query_folder_made(parent)) || !query_file_written(path, "", 0ull))
    {
        cli_unwritten(file, err_msg);
        return 0;
    }
    return 1;
}

int cli_files_create_folder(const char *root, const char *dir, VerbaLinea *err_msg)
{
    char path[CLI_PATH_MAX_LEN];
    if (!cli_inside(root, dir, path, err_msg))
    {
        return 0;
    }
    if (query_is_file(path) || query_is_folder(path))
    {
        cli_there_already(dir, err_msg);
        return 0;
    }
    if (!query_folder_made(path))
    {
        cli_unwritten(dir, err_msg);
        return 0;
    }
    return 1;
}

int cli_files_write(const char *root, const char *file, const char *text, unsigned long long length,
                    VerbaLinea *err_msg)
{
    // a file within an archive is saved back into it
    char archive[CLI_PATH_MAX_LEN];
    char inner[CLI_PATH_MAX_LEN];
    if (cli_archive_split(root, file, archive, inner) && (inner[0] != '\0'))
    {
        return cli_archive_write(root, file, (const unsigned char *)text, length, err_msg);
    }
    char path[CLI_PATH_MAX_LEN];
    char beside[CLI_PATH_MAX_LEN];
    if (!cli_inside(root, file, path, err_msg) || !cli_files_saving_beside(path, file, beside, err_msg))
    {
        return 0;
    }
    if (!query_file_written(beside, text, length))
    {
        const unsigned int error = cli_system_error();
        cli_unwritten(file, err_msg);
        cli_system_reason(error, err_msg);
        return 0;
    }
    return cli_files_saved(beside, path, file, err_msg);
}

int cli_files_saving_beside(const char *path, const char *file, char *beside, VerbaLinea *err_msg)
{
    char parent[CLI_PATH_MAX_LEN];
    if (!cli_path_parent(path, parent))
    {
        mmgr_line_put(err_msg, file);
        mmgr_line_put(err_msg, " has no folder");
        return 0;
    }
    const char *name = path + cellul.len(&(CatenaFinitaCfg){.src = parent, .cap = CLI_CELLS_POOL});
    while ((name[0] == '/') || (name[0] == '\\'))
    {
        name += 1;
    }
    if (name[0] == '\0')
    {
        mmgr_line_put(err_msg, file);
        mmgr_line_put(err_msg, " has no name");
        return 0;
    }
    VerbaLinea line = {beside, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, parent);
    mmgr_line_put(&line, "/.");
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, ".saving");
    if (mmgr_line_finish(&line) == 0ull)
    {
        cli_unwritten(file, err_msg);
        return 0;
    }
    return 1;
}

int cli_files_saved(const char *beside, const char *path, const char *file, VerbaLinea *err_msg)
{
    if (!query_file_renamed(beside, path))
    {
        const unsigned int error = cli_system_error();
        query_removed(beside);
        cli_unwritten(file, err_msg);
        cli_system_reason(error, err_msg);
        return 0;
    }
    return 1;
}
