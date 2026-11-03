// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_save.c: the tabs' saves, as save.js saves them. A tab has changes not saved while its view stands away
// from its save point, which a save puts as it is asked for and loses where the write fails. A save is two pieces of
// work on the files' lane, the controller's thread between them reading whether the text moved while the lane read it.
// The first writes the text beside its file: a text read whole line by line from the document into the file writer's
// stage, a paged text through its pages, the file it wrote read for its index. The second, where the text stood still,
// puts what was written in the file's place: the file writer's on the lane, and a paged text's on the controller's
// thread, its file moved and its new index stored in one step, so no line is read of the file between the two. Where
// the text moved, the second removes what was written, the file stands as it was, and the save is asked again. No piece
// of a save runs on the controller's thread; one the lane cannot take fails the save, the file standing as it was
#include "controller_internal.h"
#include "controller_work.h"

#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"
#include "../editor/pages.h"
#include "../editor/view.h"

// the pieces of a save, in turn: the text written beside its file, and what was written put in the file's place
#define CLI_SAVE_BESIDE 0
#define CLI_SAVE_PLACE 1

// A save under way: the tab's key and the file's whole path, each after it in its cell; the text's reading; the file
// writer's file and whether it was begun; a paged text's index read of the file written beside, NULL for a text read
// whole; whether the text is paged; the piece it runs next; whether the text was written beside the file, moved while
// it was read, and was put in the file's place; and why not where it was not
typedef struct
{
    char *key;
    char *full;
    CliDocumentReading reading;
    CliFileWriting file;
    int begun;
    struct CliPaged *read;
    int paged;
    int piece;
    int written;
    int moved;
    int placed;
    char why[CLI_ERR_MSG_MAX_LEN];
} CliModelWriting;

// the stage the file writer writes a text through, the files' lane running one piece at a time
static char s_stage[CLI_STAGE_BYTES + 1u];

typedef struct
{
    CliModelWriting *writing;
    VerbaLinea *err_msg;
} ControllerSaveCtx;

// the save's why as a line, a piece after the first writing after what the first wrote
EMBED_INLINE VerbaLinea save_why(const ControllerSaveCtx *args)
{
    const VerbaLinea why = {args->writing->why, sizeof(args->writing->why),
                            cellul.len(&(CatenaFinitaCfg){.src = args->writing->why, .cap = sizeof(args->writing->why)})};
    return why;
}

// the text read whole written beside its file through the file writer's stage, the stage's last bytes written too: 1,
// or 0 with why written into `err_msg`
EMBED_INLINE int save_beside(const ControllerSaveCtx *args)
{
    CliModelWriting *const writing = args->writing;
    writing->begun =
        EMBED_CALL(cli_path_file_internal.file_begin, CliPathFileCtx, .file = &writing->file, .path = writing->full);
    if (!writing->begun)
    {
        mmgr_line_put(args->err_msg, writing->full);
        mmgr_line_put(args->err_msg, ": it could not be begun beside its place");
        return 0;
    }
    VerbaLinea line =
        EMBED_CALL(cli_file_stage_internal.file_line, CliFileStageCtx, .file = &writing->file, .stage = s_stage);
    const int put = EMBED_CALL(cli_document_reading.put, CliDocumentReadingCfg, .reading = &writing->reading,
                               .out = &line, .err_msg = args->err_msg);
    const int flushed = put && ((line.at == 0u) || mmgr_line_flush(&line)) && mmgr_line_ok(&line);
    if (put && !flushed)
    {
        mmgr_line_put(args->err_msg, writing->full);
        mmgr_line_put(args->err_msg, ": it could not be written");
    }
    return flushed;
}

// what runs on the files' lane: the first piece writes the text beside its file; the second puts the file writer's in
// the file's place where the text stood still, else removes it, and removes a paged text's, which is put in place on
// the controller's thread
static void save_run(void *context)
{
    CliModelWriting *const writing = (CliModelWriting *)context;
    VerbaLinea err_msg = EMBED_CALL(save_why, ControllerSaveCtx, .writing = writing);
    if ((writing->piece == CLI_SAVE_BESIDE) && writing->paged)
    {
        writing->read = EMBED_CALL(cli_pages_path.save_beside_alloc, CliPagesPathCfg, .key = writing->key,
                                   .path = writing->full, .err_msg = &err_msg);
        writing->written = writing->read != NULL;
    }
    else if (writing->piece == CLI_SAVE_BESIDE)
    {
        writing->written = EMBED_CALL(save_beside, ControllerSaveCtx, .writing = writing, .err_msg = &err_msg);
    }
    else if (writing->paged)
    {
        (void)EMBED_CALL(cli_pages_place.save_place, CliPagesPlaceCfg, .read = writing->read, .whole = 0,
                         .err_msg = &err_msg);
        writing->read = NULL;
    }
    else
    {
        const int whole = writing->written && !writing->moved;
        writing->placed = EMBED_CALL(cli_file_internal.file_end, CliFileCtx, .file = &writing->file, .whole = whole);
        if (whole && !writing->placed)
        {
            mmgr_line_put(&err_msg, writing->full);
            mmgr_line_put(&err_msg, ": it could not be put in its place");
        }
    }
    (void)mmgr_line_finish(&err_msg);
}

static void save_done(void *context);

// `writing` posted to the files' lane for its next piece: 1, or 0 where the lane cannot take it
EMBED_INLINE int save_post(const ControllerSaveCtx *args)
{
    return EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_FILES, .run = save_run,
                      .done = save_done, .context = args->writing, .busy = 1);
}

// the place of the tab whose text the save writes, g_cli_model.tabs where it was closed meanwhile
EMBED_INLINE unsigned int save_tab_at(const ControllerSaveCtx *args)
{
    unsigned int at = 0u;
    while ((at < g_cli_model.tabs) &&
           !cellul.eq(&(CatenaFinitaCfg){
               .src = g_cli_model.tab[at]->key, .other = args->writing->key, .cap = CLI_PATH_MAX_LEN}))
    {
        at += 1u;
    }
    return at;
}

// a save ended: asked again of its tab where the text moved while it was read, the file standing as it was; else a
// break for the notices waiting where the file was put in its place, or a failure's notice and the tab's save point
// lost where it was not
EMBED_INLINE void save_finish(const ControllerSaveCtx *args)
{
    CliModelWriting *const writing = args->writing;
    const unsigned int at = EMBED_CALL(save_tab_at, ControllerSaveCtx, .writing = writing);
    if (writing->moved)
    {
        if (at < g_cli_model.tabs)
        {
            EMBED_CALL(cli_model_save.tab_save, CliModelCtx, .at = at);
        }
    }
    else if (writing->placed)
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

// what comes back on the controller's thread. After the first piece the reading ends, which finds whether the text
// moved: a paged text written and still is put in its place here, its file moved and its index stored at once; what
// else was written beside the file goes to the second piece; and a save with nothing beside the file ends
static void save_done(void *context)
{
    CliModelWriting *const writing = (CliModelWriting *)context;
    if (writing->piece == CLI_SAVE_BESIDE)
    {
        writing->moved = EMBED_CALL(cli_document_reading.end, CliDocumentReadingCfg, .reading = &writing->reading);
        writing->piece = CLI_SAVE_PLACE;
        if ((writing->read != NULL) && !writing->moved)
        {
            VerbaLinea err_msg = EMBED_CALL(save_why, ControllerSaveCtx, .writing = writing);
            writing->placed = EMBED_CALL(cli_pages_place.save_place, CliPagesPlaceCfg, .read = writing->read,
                                         .whole = 1, .err_msg = &err_msg);
            writing->read = NULL;
            (void)mmgr_line_finish(&err_msg);
            // a tab closed while its save ran leaves no view to read the pages stored in place
            if (EMBED_CALL(save_tab_at, ControllerSaveCtx, .writing = writing) == g_cli_model.tabs)
            {
                EMBED_CALL(cli_pages.release, CliPagesCfg, .key = writing->key);
            }
        }
        else if ((writing->begun || (writing->read != NULL)) &&
                 EMBED_CALL(save_post, ControllerSaveCtx, .writing = writing))
        {
            return;
        }
        else if (writing->begun || (writing->read != NULL))
        {
            // the lane cannot take the second piece: what was written stands beside the file, the file as it was, and
            // the save fails rather than run again into the same refusal
            if (writing->read != NULL)
            {
                EMBED_CALL(cli_pages_place.read_free, CliPagesPlaceCfg, .read = writing->read);
                writing->read = NULL;
            }
            writing->moved = 0;
            writing->placed = 0;
            VerbaLinea err_msg = EMBED_CALL(save_why, ControllerSaveCtx, .writing = writing);
            mmgr_line_put(&err_msg, writing->full);
            mmgr_line_put(&err_msg, ": it could not be put in its place, the files' lane taking no more work");
            (void)mmgr_line_finish(&err_msg);
        }
    }
    EMBED_CALL(save_finish, ControllerSaveCtx, .writing = writing);
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
    if (!EMBED_CALL(cli_document_reading.begin, CliDocumentReadingCfg, .key = writing->key,
                    .reading = &writing->reading))
    {
        g_cli_cells->persistent_buf_release(writing, &g_cli_controller_cells);
        return;
    }
    unsigned long long lines = 0ULL;
    int whole = 0;
    writing->paged = EMBED_CALL(cli_pages.count, CliPagesCfg, .key = writing->key, .lines = &lines, .whole = &whole);
    if (!EMBED_CALL(save_post, ControllerSaveCtx, .writing = writing))
    {
        (void)EMBED_CALL(cli_document_reading.end, CliDocumentReadingCfg, .reading = &writing->reading);
        g_cli_cells->persistent_buf_release(writing, &g_cli_controller_cells);
        EMBED_CALL(cli_model_notices.post, CliModelCtx,
                   .path = "The file could not be saved: the files' lane takes no more work.", .on = 1);
        return;
    }
    EMBED_CALL(cli_view_table.saved, CliViewCfg, .key = tab->key);
    tab->closing = 0;
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_TABS);
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
