// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_save.c: the tabs' saves, as save.js saves them. A tab has changes not saved while its view stands away
// from its save point. A save runs on the files' lane: a text read whole goes line by line from the document into the
// file writer's stage, which writes it beside the file and puts it in place once every byte is on the disk, and a paged
// text goes through the pages' own save. The save point is put as the save is asked for and lost where the write fails
#include "controller_internal.h"
#include "controller_work.h"

#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"
#include "../editor/pages.h"
#include "../editor/view.h"

// the stage the file writer writes a text through, the files' lane writing one file at a time
static char s_stage[CLI_STAGE_BYTES + 1u];

// A save under way on the files' lane: the tab's key and the file's whole path, each after it in its cell; whether the
// file was written; and why not where it was not
typedef struct
{
    char *key;
    char *full;
    int written;
    char why[CLI_ERR_MSG_MAX_LEN];
} CliModelWriting;

typedef struct
{
    const char *key;
    const char *full;
    VerbaLinea *err_msg;
} ControllerSaveCtx;

// the text read whole under `key` handed to the file writer for the file at `full`: 1, or 0 with why given
EMBED_INLINE int save_through_file_writer(const ControllerSaveCtx *args)
{
    CliFileWriting file;
    if (!EMBED_CALL(cli_path_file_internal.file_begin, CliPathFileCtx, .file = &file, .path = args->full))
    {
        mmgr_line_put(args->err_msg, args->full);
        mmgr_line_put(args->err_msg, ": it could not be begun beside its place");
        return 0;
    }
    VerbaLinea line = EMBED_CALL(cli_file_stage_internal.file_line, CliFileStageCtx, .file = &file, .stage = s_stage);
    if (!EMBED_CALL(cli_document_out.file_put, CliDocumentOutCfg, .key = args->key, .out = &line,
                    .err_msg = args->err_msg))
    {
        (void)EMBED_CALL(cli_file_internal.file_end, CliFileCtx, .file = &file, .whole = 0);
        return 0;
    }
    if (!EMBED_CALL(cli_line_internal.file_line_end, CliLineCtx, .file = &file, .line = &line))
    {
        mmgr_line_put(args->err_msg, args->full);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

// what runs on the files' lane: the text written to its file, a paged text's through the pages
static void save_run(void *context)
{
    CliModelWriting *const writing = (CliModelWriting *)context;
    VerbaLinea err_msg = {writing->why, sizeof(writing->why), 0ULL};
    unsigned long long lines = 0ULL;
    int whole = 0;
    writing->written =
        EMBED_CALL(cli_pages.count, CliPagesCfg, .key = writing->key, .lines = &lines, .whole = &whole)
            ? EMBED_CALL(cli_pages_path.save, CliPagesPathCfg, .key = writing->key, .path = writing->full,
                         .err_msg = &err_msg)
            : EMBED_CALL(save_through_file_writer, ControllerSaveCtx, .key = writing->key, .full = writing->full,
                         .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
}

// what comes back on the controller's thread: the save a break for the notices waiting, or a write that failed posted
// as a failure's notice and the tab's save point lost
static void save_done(void *context)
{
    CliModelWriting *const writing = (CliModelWriting *)context;
    if (writing->written)
    {
        EMBED_CALL(cli_model_notices.taken, CliModelCtx, .at = 0u);
    }
    else
    {
        EMBED_CALL(cli_view_table.unsaved, CliViewCfg, .key = writing->key);
        EMBED_CALL(cli_model_notices.post, CliModelCtx,
                   .path = (writing->why[0] != '\0') ? writing->why : "The file could not be written.", .on = 1);
    }
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_TABS);
    g_cli_cells->persistent_buf_release(writing, &g_cli_controller_cells);
}

void cli_model_tab_save(const CliModelCtx *args)
{
    if ((args->at >= g_cli_model.tabs) || (g_cli_model.root == NULL) || !g_cli_model.tab[args->at]->viewed)
    {
        return;
    }
    CliModelTab *const tab = g_cli_model.tab[args->at];
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = g_cli_model.root, .relative = tab->path, .joined = full);
    const unsigned long long key_size = cellul.len(&(CatenaFinitaCfg){.src = tab->key, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long full_size = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    CliModelWriting *const writing =
        g_cli_cells->persistent_buf_alloc(sizeof(*writing) + key_size + full_size, &g_cli_controller_cells);
    if (writing == NULL)
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = writing, .val = 0, .bytes = sizeof(*writing)});
    writing->key = (char *)(void *)(writing + 1);
    writing->full = writing->key + key_size;
    memor.cpy(&(MemoriaCfg){.dst = writing->key, .src = tab->key, .bytes = key_size});
    memor.cpy(&(MemoriaCfg){.dst = writing->full, .src = full, .bytes = full_size});
    EMBED_CALL(cli_view_table.saved, CliViewCfg, .key = tab->key);
    tab->closing = 0;
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_TABS);
    EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_FILES, .run = save_run,
               .done = save_done, .context = writing, .busy = 1);
}

void cli_model_tabs_save(const CliModelCtx *args)
{
    (void)args;
    for (unsigned int at = 0u; at < g_cli_model.tabs; at += 1u)
    {
        if (EMBED_CALL(cli_view_table.modified, CliViewCfg, .key = g_cli_model.tab[at]->key))
        {
            EMBED_CALL(cli_model_save.tab_save, CliModelCtx, .at = at);
        }
    }
}

int cli_model_save_needs(const CliModelCtx *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->path, .other = "changed-active", .cap = CLI_LABEL_MAX_LEN}))
    {
        return (g_cli_model.active >= 0) &&
               EMBED_CALL(cli_view_table.modified, CliViewCfg, .key = g_cli_model.tab[g_cli_model.active]->key);
    }
    int changed = 0;
    for (unsigned int at = 0u; at < g_cli_model.tabs; at += 1u)
    {
        changed = changed || EMBED_CALL(cli_view_table.modified, CliViewCfg, .key = g_cli_model.tab[at]->key);
    }
    return cellul.eq(&(CatenaFinitaCfg){.src = args->path, .other = "changed", .cap = CLI_LABEL_MAX_LEN}) && changed;
}
