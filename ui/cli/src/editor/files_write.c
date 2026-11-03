// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_write.c: files and folders made in the tree, and a file's text written beside it and moved over it, so a failed
// write leaves the file as it was
#include "../text/text.h"
#include "archive.h"
#include "files.h"

typedef struct
{
    const char *path;
    VerbaLinea *err_msg;
} FilesWriteCtx;

// that the path is there already, reported
EMBED_INLINE void cli_there_already(const FilesWriteCtx *args)
{
    mmgr_line_put(args->err_msg, args->path);
    mmgr_line_put(args->err_msg, " is there already");
}

// that the path could not be written, reported
EMBED_INLINE void cli_unwritten(const FilesWriteCtx *args)
{
    mmgr_line_put(args->err_msg, args->path);
    mmgr_line_put(args->err_msg, ": it could not be written");
}

int cli_files_create_file(const CliFilesRootErrMsgTextCfg *args)
{
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->root, .relative = args->file, .joined = path,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path) ||
        EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = path))
    {
        EMBED_CALL(cli_there_already, FilesWriteCtx, .path = args->file, .err_msg = args->err_msg);
        return 0;
    }
    char parent[CLI_PATH_MAX_LEN];
    if ((EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = parent) &&
         !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = parent)) ||
        !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "",
                    .size = 0ULL))
    {
        EMBED_CALL(cli_unwritten, FilesWriteCtx, .path = args->file, .err_msg = args->err_msg);
        return 0;
    }
    return 1;
}

int cli_files_create_folder(const CliFilesRootOutDirCfg *args)
{
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->root, .relative = args->dir, .joined = path,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path) ||
        EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = path))
    {
        EMBED_CALL(cli_there_already, FilesWriteCtx, .path = args->dir, .err_msg = args->err_msg);
        return 0;
    }
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path))
    {
        EMBED_CALL(cli_unwritten, FilesWriteCtx, .path = args->dir, .err_msg = args->err_msg);
        return 0;
    }
    return 1;
}

int cli_files_write(const CliFilesRootErrMsgTextCfg *args)
{
    const char *root = args->root;
    const char *file = args->file;
    VerbaLinea *err_msg = args->err_msg;
    // a file within an archive is saved back into it
    char archive[CLI_PATH_MAX_LEN];
    char inner[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_archive_root.split, CliArchiveRootCfg, .root = root, .path = file, .file = archive,
                   .inner = inner) &&
        (inner[0] != '\0'))
    {
        return EMBED_CALL(cli_archive_size_data.write, CliArchiveSizeDataCfg, .root = root, .path = file,
                          .data = (const unsigned char *)args->text, .size = args->length, .err_msg = err_msg);
    }
    char path[CLI_PATH_MAX_LEN];
    char adjacent[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = file, .joined = path,
                    .err_msg = err_msg) ||
        !EMBED_CALL(cli_files_path_err_msg.saving_adjacent, CliFilesPathErrMsgCfg, .path = path, .file = file,
                    .adjacent = adjacent, .err_msg = err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = adjacent,
                    .bytes = args->text, .size = args->length))
    {
        const unsigned int error = cli_system_error();
        EMBED_CALL(cli_unwritten, FilesWriteCtx, .path = file, .err_msg = err_msg);
        EMBED_CALL(cli_error_internal.system_reason, CliErrorCtx, .error = error, .err_msg = err_msg);
        return 0;
    }
    return EMBED_CALL(cli_files_path.save, CliFilesPathCfg, .adjacent_read = adjacent, .path = path, .file = file,
                      .err_msg = err_msg);
}

int cli_files_saving_adjacent(const CliFilesPathErrMsgCfg *args)
{
    char parent[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->path, .out = parent))
    {
        mmgr_line_put(args->err_msg, args->file);
        mmgr_line_put(args->err_msg, " has no folder");
        return 0;
    }
    const char *name = args->path + cellul.len(&(CatenaFinitaCfg){.src = parent, .cap = CLI_CELLS_POOL});
    name += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = name, .at = 0ULL,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .set = "/\\");
    if (name[0] == '\0')
    {
        mmgr_line_put(args->err_msg, args->file);
        mmgr_line_put(args->err_msg, " has no name");
        return 0;
    }
    VerbaLinea line = {args->adjacent, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, parent);
    mmgr_line_put(&line, "/.");
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, ".saving");
    if (mmgr_line_finish(&line) == 0ULL)
    {
        EMBED_CALL(cli_unwritten, FilesWriteCtx, .path = args->file, .err_msg = args->err_msg);
        return 0;
    }
    return 1;
}

int cli_files_save(const CliFilesPathCfg *args)
{
    if (!EMBED_CALL(query_from_internal.file_rename, QueryFromCtx, .from = args->adjacent_read, .to = args->path))
    {
        const unsigned int error = cli_system_error();
        EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = args->adjacent_read);
        EMBED_CALL(cli_unwritten, FilesWriteCtx, .path = args->file, .err_msg = args->err_msg);
        EMBED_CALL(cli_error_internal.system_reason, CliErrorCtx, .error = error, .err_msg = args->err_msg);
        return 0;
    }
    return 1;
}
