// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_write.c: files and folders made in the tree, and a file's text written beside it and moved over it, so a failed
// write leaves the file as it was
#include "archive.h"
#include "files.h"

// that the path is there already, said
static void cli_there_already(const char *path, CliLine *said)
{
    cli_line_text(said, path);
    cli_line_text(said, " is there already");
}

// that the path could not be written, said
static void cli_unwritten(const char *path, CliLine *said)
{
    cli_line_text(said, path);
    cli_line_text(said, ": it could not be written");
}

int cli_files_create_file(const char *root, const char *file, CliLine *said)
{
    char path[CLI_PATH_LONGEST];
    if (!cli_inside(root, file, path, said))
    {
        return 0;
    }
    if (query_is_file(path) || query_is_folder(path))
    {
        cli_there_already(file, said);
        return 0;
    }
    char parent[CLI_PATH_LONGEST];
    if ((cli_path_parent(path, parent) && !query_folder_made(parent)) || !query_file_written(path, "", 0ull))
    {
        cli_unwritten(file, said);
        return 0;
    }
    return 1;
}

int cli_files_create_folder(const char *root, const char *dir, CliLine *said)
{
    char path[CLI_PATH_LONGEST];
    if (!cli_inside(root, dir, path, said))
    {
        return 0;
    }
    if (query_is_file(path) || query_is_folder(path))
    {
        cli_there_already(dir, said);
        return 0;
    }
    if (!query_folder_made(path))
    {
        cli_unwritten(dir, said);
        return 0;
    }
    return 1;
}

int cli_files_write(const char *root, const char *file, const char *text, unsigned long long length, CliLine *said)
{
    // a file within an archive is saved back into it
    char archive[CLI_PATH_LONGEST];
    char inner[CLI_PATH_LONGEST];
    if (cli_archive_split(root, file, archive, inner) && (inner[0] != '\0'))
    {
        return cli_archive_write(root, file, (const unsigned char *)text, length, said);
    }
    char path[CLI_PATH_LONGEST];
    char beside[CLI_PATH_LONGEST];
    if (!cli_inside(root, file, path, said) || !cli_files_saving_beside(path, file, beside, said))
    {
        return 0;
    }
    if (!query_file_written(beside, text, length))
    {
        const unsigned int error = cli_system_error();
        cli_unwritten(file, said);
        cli_system_reason(error, said);
        return 0;
    }
    return cli_files_saved(beside, path, file, said);
}

int cli_files_saving_beside(const char *path, const char *file, char *beside, CliLine *said)
{
    char parent[CLI_PATH_LONGEST];
    if (!cli_path_parent(path, parent))
    {
        cli_line_text(said, file);
        cli_line_text(said, " has no folder");
        return 0;
    }
    const char *name = path + cellul.len(&(CatenaFinitaCfg){.src = parent, .cap = CLI_CELLS_POOL});
    while ((name[0] == '/') || (name[0] == '\\'))
    {
        name += 1;
    }
    if (name[0] == '\0')
    {
        cli_line_text(said, file);
        cli_line_text(said, " has no name");
        return 0;
    }
    CliLine line = {beside, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, parent);
    cli_line_text(&line, "/.");
    cli_line_text(&line, name);
    cli_line_text(&line, ".saving");
    if (cli_line_closed(&line) == 0ull)
    {
        cli_unwritten(file, said);
        return 0;
    }
    return 1;
}

int cli_files_saved(const char *beside, const char *path, const char *file, CliLine *said)
{
    if (!query_file_renamed(beside, path))
    {
        const unsigned int error = cli_system_error();
        query_removed(beside);
        cli_unwritten(file, said);
        cli_system_reason(error, said);
        return 0;
    }
    return 1;
}
