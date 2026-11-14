// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_head.c: a file as the last commit has it read from git on a worker and stored beside its text in the document
// store, where the store's diff reads it for the changes of the open file
#include "git.h"

#include "../command_line/pool.h"
#include "../editor/document_internal.h"
#include "../servers/servers_internal.h"

// the most bytes of the file as the last commit has it a read takes
#define GIT_HEAD_MAX (1ULL << 20u)

// A read lined up for a worker: the tree's root, the file's path and the key its text is stored under
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char key[CLI_NAME_MAX_LEN];
} GitHeadRead;

// the store told nothing of a change: the head alone is stored by the call
static void head_stored_each(const CliEachHunkCfg *args)
{
    (void)args;
}

// the worker's read: the head's text lives through this call alone, a cell of the temporary tier; a file the last
// commit does not contain is stored as none
static void head_read_job(const CliServersJobCfg *args)
{
    if (args->size < sizeof(GitHeadRead))
    {
        return;
    }
    GitHeadRead read;
    memor.cpy(&(MemoriaCfg){.dst = &read, .src = args->job, .bytes = sizeof(read)});
    char *const scratch = g_cli_guard->temporary_buf_alloc(GIT_HEAD_MAX, g_cli_git_owner);
    if (scratch == NULL)
    {
        return;
    }
    VerbaLinea out = {scratch, GIT_HEAD_MAX, 0ULL};
    const int tracked = EMBED_CALL(cli_git_root_file_stage.head_text, CliGitRootFileStageCfg, .root = read.root,
                                   .file = read.path, .out = &out) &&
                        mmgr_line_ok(&out);
    (void)EMBED_CALL(cli_document_out_key_head_input.changes, CliDocumentOutKeyHeadInputCfg, .key = read.key,
                     .head_given = 1, .head = tracked ? scratch : NULL, .head_size = tracked ? out.at : 0ULL,
                     .each = head_stored_each, .context = NULL);
    g_cli_guard->persistent_buf_release(scratch, g_cli_git_owner);
    cli_document_colored_wake();
}

void cli_git_head_later(const char *root, const char *path, const char *key)
{
    if ((root == NULL) || (path == NULL) || (key == NULL) ||
        EMBED_CALL(cli_document.has_head, CliDocumentCfg, .key = key))
    {
        return;
    }
    GitHeadRead read;
    memor.set(&(MemoriaCfg){.dst = &read, .val = 0, .bytes = sizeof(read)});
    memor.cpy(&(MemoriaCfg){.dst = read.root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_PATH_MAX_LEN - 1u})});
    memor.cpy(&(MemoriaCfg){.dst = read.path,
                            .src = path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN - 1u})});
    memor.cpy(&(MemoriaCfg){.dst = read.key,
                            .src = key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_NAME_MAX_LEN - 1u})});
    (void)EMBED_CALL(cli_servers_run_internal.later, CliServersRunCtx, .run = head_read_job, .job = &read,
                     .size = sizeof(read));
}
