// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_save.c: the tabs' saves, as save.js and tabs.js save them. A tab is changed while its text differs from
// its file as it opened or was last saved: the view's state the one the file stands at is no change; a paged text in
// any other state is changed; and a text read whole in another state is changed unless its lines, its line ends, its
// census and its SHA-256 all match the file's, so a letter typed and taken out again is no change, the digest taken
// once for each state and only where the rest match. A save writes the text on the files' lane, as the tree writes
// every file, a paged text's through the pages, a text whose lines read ending in a carriage return and a newline
// written with them
#include "controller_internal.h"
#include "controller_work.h"

#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../editor/pages.h"
#include "../editor/view.h"
#include "../text/digest.h"
#include "../text/text.h"

// A save under way on the files' lane: the tab's key, the tree and the file's path from the tree's top, each after it
// in its cell; the text, `size` bytes in a cell of its own, its lines joined by newlines, NULL for a paged text, which
// the pages write; the text as its file stands once written; whether the write was done, and why not where it was not
typedef struct
{
    char *key;
    char *root;
    char *path;
    char *text;
    unsigned long long size;
    int paged;
    CliModelSavedText saved;
    int written;
    char why[CLI_ERR_MSG_MAX_LEN];
} CliModelWriting;

typedef struct
{
    const char *key;
    const CliModelSavedText *saved_read;
    const CliModelSavedText *now_read;
} ControllerSaveCtx;

void cli_model_saved_text_read(const CliModelSavedTextCfg *args)
{
    CliModelSavedText *const text = args->text;
    CliShape shape = {0ULL, 0ULL};
    text->lines =
        EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = args->key, .shape = &shape) ? shape.lines : 0ULL;
    text->crlf = EMBED_CALL(cli_document.crlf, CliDocumentCfg, .key = args->key);
    if (!EMBED_CALL(cli_document.histogram, CliDocumentCfg, .key = args->key, .histogram = &text->census))
    {
        memor.set(&(MemoriaCfg){.dst = &text->census, .val = 0, .bytes = sizeof(text->census)});
    }
    text->digest_known = 0;
    if (!args->digest)
    {
        return;
    }
    unsigned long long size = 0ULL;
    char *const whole = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = args->key,
                                   .size_pointer = &size, .owner = &g_cli_controller_cells);
    if (whole != NULL)
    {
        EMBED_CALL(cli_digest_bytes_size.sha256_bytes, CliDigestBytesSizeCfg, .bytes = whole, .size = size,
                   .digest = text->digest);
        text->digest_known = 1;
        g_cli_cells->persistent_buf_release(whole, &g_cli_controller_cells);
    }
}

// whether the text `now_read` reads, its digest taken where the rest match, is the file `saved_read` stands at
EMBED_INLINE int save_same(const ControllerSaveCtx *args)
{
    const CliModelSavedText *const saved = args->saved_read;
    CliModelSavedText now = *args->now_read;
    if ((now.lines != saved->lines) || (now.crlf != saved->crlf) ||
        (memor.cmp(&(MemoriaCfg){.src = &now.census, .other = &saved->census, .bytes = sizeof(now.census)}) != 0))
    {
        return 0;
    }
    EMBED_CALL(cli_model_saved_text.read, CliModelSavedTextCfg, .key = args->key, .text = &now, .digest = 1);
    return now.digest_known &&
           (memor.cmp(&(MemoriaCfg){.src = now.digest, .other = saved->digest, .bytes = sizeof(now.digest)}) == 0);
}

void cli_model_tab_compare(const CliModelCtx *args)
{
    if (args->at >= g_cli_model.tabs)
    {
        return;
    }
    CliModelTab *const tab = g_cli_model.tab[args->at];
    const unsigned long long state = EMBED_CALL(cli_view_table.state, CliViewCfg, .key = tab->key);
    if (!tab->viewed || (state == tab->compared_state))
    {
        return;
    }
    int changed = state != tab->saved.state;
    if (changed && tab->saved.digest_known)
    {
        CliModelSavedText now;
        EMBED_CALL(cli_model_saved_text.read, CliModelSavedTextCfg, .key = tab->key, .text = &now, .digest = 0);
        changed =
            !EMBED_CALL(save_same, ControllerSaveCtx, .key = tab->key, .saved_read = &tab->saved, .now_read = &now);
    }
    tab->compared_state = state;
    if (changed != tab->changed)
    {
        tab->changed = changed;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_TABS);
    }
}

// what runs on the files' lane: the text written to its file, its line ends put back where they were a carriage return
// and a newline, and its digest taken
static void save_run(void *context)
{
    CliModelWriting *const writing = (CliModelWriting *)context;
    VerbaLinea err_msg = {writing->why, sizeof(writing->why), 0ULL};
    if (writing->paged)
    {
        writing->written = EMBED_CALL(cli_pages_path.save, CliPagesPathCfg, .key = writing->key, .path = writing->path,
                                      .err_msg = &err_msg);
        (void)mmgr_line_finish(&err_msg);
        return;
    }
    const char *text = writing->text;
    unsigned long long length = writing->size;
    char *ended = NULL;
    if (writing->saved.crlf)
    {
        // each newline a carriage return before it: the lines less one more bytes
        const unsigned long long breaks = (writing->saved.lines != 0ULL) ? (writing->saved.lines - 1ULL) : 0ULL;
        ended = g_cli_cells->persistent_buf_alloc(writing->size + breaks + 1ULL, &g_cli_controller_cells);
        if (ended == NULL)
        {
            mmgr_line_put(&err_msg, writing->path);
            mmgr_line_put(&err_msg, ": the cells cannot meet its text with its line ends");
            (void)mmgr_line_finish(&err_msg);
            return;
        }
        VerbaLinea line = {ended, writing->size + breaks + 1ULL, 0ULL};
        for (unsigned long long at = 0ULL; at < writing->size;)
        {
            const unsigned long long end =
                EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = writing->text, .at = at,
                           .size = writing->size, .set = "\n");
            mmgr_line_put_n(&line, writing->text + at, end - at);
            mmgr_line_put(&line, (end < writing->size) ? "\r\n" : "");
            at = end + 1ULL;
        }
        length = mmgr_line_finish(&line);
        text = ended;
    }
    writing->written = EMBED_CALL(cli_files_root_err_msg_text.write, CliFilesRootErrMsgTextCfg, .root = writing->root,
                                  .file = writing->path, .text = text, .length = length, .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    g_cli_cells->persistent_buf_release(ended, &g_cli_controller_cells);
    EMBED_CALL(cli_digest_bytes_size.sha256_bytes, CliDigestBytesSizeCfg, .bytes = writing->text, .size = writing->size,
               .digest = writing->saved.digest);
    writing->saved.digest_known = 1;
}

// the tab whose key is `key`, -1 for none
EMBED_INLINE int save_tab_by_key(const ControllerSaveCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_model.tabs; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_model.tab[at]->key, .other = args->key, .cap = CLI_PATH_MAX_LEN}))
        {
            // the tabs are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// what comes back on the controller's thread: the tab's file as it now stands, the tab no longer changed where its text
// stands there still, nor its close waiting, and the save a break for the notices waiting; a write that failed posted
// as a failure's notice, the tab left changed; and a save asked for meanwhile begun
static void save_done(void *context)
{
    CliModelWriting *const writing = (CliModelWriting *)context;
    if (writing->written)
    {
        EMBED_CALL(cli_model_notices.taken, CliModelCtx, .at = 0u);
    }
    else
    {
        EMBED_CALL(cli_model_notices.post, CliModelCtx,
                   .path = (writing->why[0] != '\0') ? writing->why : "The file could not be written.", .on = 1);
    }
    const int at = EMBED_CALL(save_tab_by_key, ControllerSaveCtx, .key = writing->key);
    if (at >= 0)
    {
        CliModelTab *const tab = g_cli_model.tab[at];
        const int again = tab->saving == CLI_MODEL_SAVING_AGAIN;
        tab->saving = 0;
        if (writing->written)
        {
            tab->saved = writing->saved;
            tab->saved.digest_known = tab->saved.digest_known && !writing->paged;
            tab->closing = 0;
            tab->compared_state = 0ULL;
            g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_TABS);
            // the tabs are fewer than an int counts
            EMBED_CALL(cli_model_save.tab_compare, CliModelCtx, .at = (unsigned int)at);
        }
        if (again)
        {
            // the tabs are fewer than an int counts
            EMBED_CALL(cli_model_save.tab_save, CliModelCtx, .at = (unsigned int)at);
        }
    }
    g_cli_cells->persistent_buf_release(writing->text, &g_cli_controller_cells);
    g_cli_cells->persistent_buf_release(writing, &g_cli_controller_cells);
}

void cli_model_tab_save(const CliModelCtx *args)
{
    if ((args->at >= g_cli_model.tabs) || (g_cli_model.root == NULL))
    {
        return;
    }
    CliModelTab *const tab = g_cli_model.tab[args->at];
    if (!tab->viewed)
    {
        return;
    }
    if (tab->saving != 0)
    {
        tab->saving = CLI_MODEL_SAVING_AGAIN;
        return;
    }
    const unsigned long long key_size = cellul.len(&(CatenaFinitaCfg){.src = tab->key, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long root_size =
        cellul.len(&(CatenaFinitaCfg){.src = g_cli_model.root, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long path_size =
        cellul.len(&(CatenaFinitaCfg){.src = tab->path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    CliModelWriting *const writing =
        g_cli_cells->persistent_buf_alloc(sizeof(*writing) + key_size + root_size + path_size, &g_cli_controller_cells);
    if (writing == NULL)
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = writing, .val = 0, .bytes = sizeof(*writing)});
    writing->key = (char *)(void *)(writing + 1);
    writing->root = writing->key + key_size;
    writing->path = writing->root + root_size;
    memor.cpy(&(MemoriaCfg){.dst = writing->key, .src = tab->key, .bytes = key_size});
    memor.cpy(&(MemoriaCfg){.dst = writing->root, .src = g_cli_model.root, .bytes = root_size});
    memor.cpy(&(MemoriaCfg){.dst = writing->path, .src = tab->path, .bytes = path_size});
    writing->paged = tab->paged;
    // the text and the state it stands in taken together, so the file stands at the state written
    EMBED_CALL(cli_model_saved_text.read, CliModelSavedTextCfg, .key = tab->key, .text = &writing->saved, .digest = 0);
    writing->saved.state = EMBED_CALL(cli_view_table.state, CliViewCfg, .key = tab->key);
    if (!writing->paged)
    {
        writing->text = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = tab->key,
                                   .size_pointer = &writing->size, .owner = &g_cli_controller_cells);
        if (writing->text == NULL)
        {
            g_cli_cells->persistent_buf_release(writing, &g_cli_controller_cells);
            return;
        }
    }
    tab->saving = CLI_MODEL_SAVING;
    EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_FILES, .run = save_run,
               .done = save_done, .context = writing);
}

void cli_model_tabs_save(const CliModelCtx *args)
{
    (void)args;
    for (unsigned int at = 0u; at < g_cli_model.tabs; at += 1u)
    {
        if (g_cli_model.tab[at]->changed)
        {
            EMBED_CALL(cli_model_save.tab_save, CliModelCtx, .at = at);
        }
    }
}

int cli_model_save_needs(const CliModelCtx *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->path, .other = "changed-active", .cap = CLI_LABEL_MAX_LEN}))
    {
        return (g_cli_model.active >= 0) && g_cli_model.tab[g_cli_model.active]->changed;
    }
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->path, .other = "changed", .cap = CLI_LABEL_MAX_LEN}))
    {
        return 0;
    }
    int changed = 0;
    for (unsigned int at = 0u; at < g_cli_model.tabs; at += 1u)
    {
        changed = changed || g_cli_model.tab[at]->changed;
    }
    return changed;
}
