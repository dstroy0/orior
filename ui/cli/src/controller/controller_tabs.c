// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_tabs.c: the files open as tabs. A file is read on the files' lane, its text stored and its grammar applied
// there, and its view opens on the controller's thread as that comes back. The session is the tabs' paths in order, a
// line each, the tab shown's first, saved in orior's folder under a key of the tree's own
#include "controller_internal.h"
#include "controller_work.h"

#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../editor/view.h"
#include "../machine/state.h"
#include "../plugins/plugins.h"
#include "../servers/servers.h"
#include "../servers/servers_internal.h"
#include "../text/text.h"

// the head of the key a file's text is stored under; the keys of orior's folder the session and the strip's group stand
// under, the tree's root after the session's
#define CLI_MODEL_KEY_HEAD "window:"
#define CLI_MODEL_SESSION_KEY "window.session."
#define CLI_MODEL_GROUP_KEY "panes.group"

// A file being opened on the files' lane: its path, key and tree after it in its cell, the rows the walk starts with,
// what the opening left, and the text as the file stands
typedef struct
{
    const char *path;
    const char *key;
    const char *root;
    unsigned int rows;
    unsigned int opened;
    CliModelSavedText saved;
} CliModelOpening;

// A value being saved on the files' lane, its key and value after it in its cell
typedef struct
{
    const char *key;
    const char *value;
} CliModelSaving;

typedef struct
{
    const char *path;
    unsigned long long length;
    const char *key;
    const char *value;
    char *relative;
    unsigned int at;
} ControllerTabsCtx;

// the tab whose path is `length` bytes at `path`, or -1
EMBED_INLINE int tabs_by_path(const ControllerTabsCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_model.tabs; at += 1u)
    {
        const char *const path = g_cli_model.tab[at]->path;
        if ((cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) == args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = path, .other = args->path, .bytes = args->length}) == 0))
        {
            // the tabs are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// the tab whose key is `key`, or -1
EMBED_INLINE int tabs_by_key(const ControllerTabsCtx *args)
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

const char *cli_model_key(const CliModelCtx *args)
{
    (void)args;
    return (g_cli_model.active >= 0) ? g_cli_model.tab[g_cli_model.active]->key : NULL;
}

void cli_model_shown_dirty(const CliModelCtx *args)
{
    (void)args;
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_TABS) | CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS) |
                         CLI_MODEL_DIRTY(CLI_SNAPSHOT_FRAME) | CLI_MODEL_DIRTY(CLI_SNAPSHOT_MAP) |
                         CLI_MODEL_DIRTY(CLI_SNAPSHOT_NAMES) | CLI_MODEL_DIRTY(CLI_SNAPSHOT_EXPLORER) |
                         CLI_MODEL_DIRTY(CLI_SNAPSHOT_PROBLEMS);
}

// what runs on the files' lane: the file read and stored under its key, and its grammar applied
static void tabs_open_run(void *context)
{
    CliModelOpening *const opening = (CliModelOpening *)context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    opening->opened = EMBED_CALL(cli_files_opening.open, CliFilesOpenCfg, .root = opening->root, .file = opening->path,
                                 .key = opening->key, .err_msg = &err_msg);
    if ((opening->opened != CLI_FILES_OPEN_WHOLE) && (opening->opened != CLI_FILES_OPEN_PAGED))
    {
        return;
    }
    if (opening->opened == CLI_FILES_OPEN_PAGED)
    {
        (void)EMBED_CALL(cli_document_key_from.page, CliDocumentKeyFromCfg, .key = opening->key, .from = 0ULL,
                         .to = opening->rows + 1ULL);
    }
    else
    {
        (void)EMBED_CALL(cli_document_key_from.cache, CliDocumentKeyFromCfg, .key = opening->key, .from = 0ULL,
                         .to = opening->rows + 1ULL);
        EMBED_CALL(cli_model_saved_text.read, CliModelSavedTextCfg, .key = opening->key, .text = &opening->saved,
                   .digest = 1);
    }
    char id[CLI_NAME_MAX_LEN];
    char grammar[CLI_NAME_MAX_LEN];
    CliLanguages languages;
    EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &languages);
    const int known = EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                 .languages_read = &languages, .path = opening->path, .id_out = id);
    EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = &languages);
    grammar[0] = '\0';
    err_msg.at = 0ULL;
    if (known && !EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = id, .key = grammar,
                             .err_msg = &err_msg))
    {
        grammar[0] = '\0';
    }
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = opening->key,
                     .grammar = grammar, .named = 0);
}

// what comes back on the controller's thread: the view opened on the text at the editor's size, where its tab still
// stands
static void tabs_open_done(void *context)
{
    CliModelOpening *const opening = (CliModelOpening *)context;
    const int at = EMBED_CALL(tabs_by_key, ControllerTabsCtx, .key = opening->key);
    const int text = (opening->opened == CLI_FILES_OPEN_WHOLE) || (opening->opened == CLI_FILES_OPEN_PAGED);
    if ((at < 0) && text)
    {
        EMBED_CALL(cli_document.release, CliDocumentCfg, .key = opening->key);
    }
    if ((at >= 0) && text && EMBED_CALL(cli_view_table.open, CliViewCfg, .key = opening->key, .path = opening->path))
    {
        CliModelTab *const tab = g_cli_model.tab[at];
        tab->viewed = 1;
        // the file stands at the state the view opened in
        tab->paged = opening->opened == CLI_FILES_OPEN_PAGED;
        tab->saved = opening->saved;
        tab->saved.state = EMBED_CALL(cli_view_table.state, CliViewCfg, .key = opening->key);
        tab->compared_state = tab->saved.state;
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        const CliViewInput sized = {.kind = CLI_VIEW_SIZE, .rows = g_cli_model.rows, .columns = g_cli_model.columns};
        (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = tab->key, .input = &sized,
                         .err_msg = &err_msg);
        EMBED_CALL(cli_model.server_serve, CliModelCtx, .at = (unsigned int)at);
        if (at == g_cli_model.active)
        {
            if (g_cli_model.position_pending)
            {
                const CliViewInput placed = {.kind = CLI_VIEW_POSITION,
                                             .row = g_cli_model.position_wanted.line,
                                             .column = g_cli_model.position_wanted.col};
                g_cli_model.position_pending = 0;
                (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = tab->key, .input = &placed,
                                 .err_msg = &err_msg);
            }
            EMBED_CALL(cli_model.shown_dirty, CliModelCtx, .at = 0u);
        }
    }
    g_cli_cells->persistent_buf_release(opening, &g_cli_controller_cells);
}

// the bytes of `path` made the tree's where it is a whole path inside the tree, into `relative`, sized for
// CLI_PATH_MAX_LEN: how many, a path that is not whole being from the tree's top already
EMBED_INLINE unsigned long long tabs_relative(const ControllerTabsCtx *args)
{
    const char *const path = args->path;
    const int whole = (path[0] == '/') || (path[0] == '\\') || ((path[0] != '\0') && (path[1] == ':'));
    VerbaLinea line = {args->relative, CLI_PATH_MAX_LEN, 0ULL};
    char full[CLI_PATH_MAX_LEN];
    if (whole && (g_cli_model.root != NULL) &&
        EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = path, .full = full,
                   .capacity = sizeof(full)))
    {
        EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = g_cli_model.root,
                   .path = full, .out = &line);
    }
    else
    {
        mmgr_line_put(&line, path);
    }
    return mmgr_line_finish(&line);
}

// the tabs' list grown to hold one more: 1, or 0 where the cells cannot meet it
EMBED_INLINE int tabs_room(const ControllerTabsCtx *args)
{
    (void)args;
    if (g_cli_model.tabs < g_cli_model.tabs_capacity)
    {
        return 1;
    }
    const unsigned int capacity = (g_cli_model.tabs_capacity != 0u) ? (2u * g_cli_model.tabs_capacity) : 16u;
    CliModelTab **const grown = g_cli_cells->persistent_buf_alloc(capacity * sizeof(*grown), &g_cli_controller_cells);
    if (grown == NULL)
    {
        return 0;
    }
    if (g_cli_model.tab != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = g_cli_model.tab, .bytes = g_cli_model.tabs * sizeof(*grown)});
    }
    g_cli_cells->persistent_buf_release(g_cli_model.tab, &g_cli_controller_cells);
    g_cli_model.tab = grown;
    g_cli_model.tabs_capacity = capacity;
    return 1;
}

// the tab `at` made the one shown, the one shown before it saved
EMBED_INLINE void tabs_show(const ControllerTabsCtx *args)
{
    // the tabs are fewer than an int counts
    const int at = (int)args->at;
    g_cli_model.last_active =
        ((g_cli_model.active >= 0) && (g_cli_model.active != at)) ? g_cli_model.active : g_cli_model.last_active;
    g_cli_model.active = at;
    g_cli_model.map_said = 0;
    g_cli_model.shown += 1ULL;
    EMBED_CALL(cli_model.panels_hide, CliModelCtx, .at = 0u);
    EMBED_CALL(cli_model.shown_dirty, CliModelCtx, .at = 0u);
}

void cli_model_tab_show(const CliModelCtx *args)
{
    if (args->at >= g_cli_model.tabs)
    {
        return;
    }
    EMBED_CALL(tabs_show, ControllerTabsCtx, .at = args->at);
    EMBED_CALL(cli_model.session_save, CliModelCtx, .at = 0u);
}

void cli_model_file_open(const CliModelCtx *args)
{
    char relative[CLI_PATH_MAX_LEN];
    const unsigned long long length =
        EMBED_CALL(tabs_relative, ControllerTabsCtx, .path = args->path, .relative = relative);
    g_cli_model.position_pending = args->placed;
    g_cli_model.position_wanted = (CliPosition){args->line, args->col};
    const int open = EMBED_CALL(tabs_by_path, ControllerTabsCtx, .path = relative, .length = length);
    if (open >= 0)
    {
        // the tabs are fewer than an int counts
        const CliModelTab *const tab = g_cli_model.tab[open];
        EMBED_CALL(tabs_show, ControllerTabsCtx, .at = (unsigned int)open);
        if (tab->viewed && g_cli_model.position_pending)
        {
            char why[CLI_ERR_MSG_MAX_LEN];
            VerbaLinea err_msg = {why, sizeof(why), 0ULL};
            const CliViewInput placed = {.kind = CLI_VIEW_POSITION, .row = args->line, .column = args->col};
            g_cli_model.position_pending = 0;
            (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = tab->key, .input = &placed,
                             .err_msg = &err_msg);
        }
        EMBED_CALL(cli_model.session_save, CliModelCtx, .at = 0u);
        return;
    }
    const unsigned long long head = sizeof(CLI_MODEL_KEY_HEAD) - 1ULL;
    CliModelTab *const tab =
        EMBED_CALL(tabs_room, ControllerTabsCtx, .at = 0u)
            ? g_cli_cells->persistent_buf_alloc(sizeof(*tab) + (2ULL * (length + 1ULL)) + head, &g_cli_controller_cells)
            : NULL;
    const unsigned long long root_size =
        (g_cli_model.root != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = g_cli_model.root, .cap = CLI_PATH_MAX_LEN})
                                   : 0ULL;
    CliModelOpening *const opening =
        (tab != NULL)
            ? g_cli_cells->persistent_buf_alloc(sizeof(*opening) + (2ULL * (length + 1ULL)) + head + root_size + 1ULL,
                                                &g_cli_controller_cells)
            : NULL;
    if (opening == NULL)
    {
        g_cli_cells->persistent_buf_release(tab, &g_cli_controller_cells);
        return;
    }
    memor.set(&(MemoriaCfg){.dst = tab, .val = 0, .bytes = sizeof(*tab)});
    tab->path = (char *)(void *)(tab + 1);
    tab->key = tab->path + length + 1ULL;
    memor.cpy(&(MemoriaCfg){.dst = tab->path, .src = relative, .bytes = length + 1ULL});
    memor.cpy(&(MemoriaCfg){.dst = tab->key, .src = CLI_MODEL_KEY_HEAD, .bytes = head});
    memor.cpy(&(MemoriaCfg){.dst = tab->key + head, .src = relative, .bytes = length + 1ULL});
    g_cli_model.tab[g_cli_model.tabs] = tab;
    g_cli_model.tabs += 1u;
    char *const copies = (char *)(void *)(opening + 1);
    memor.cpy(&(MemoriaCfg){.dst = copies, .src = tab->path, .bytes = length + 1ULL});
    memor.cpy(&(MemoriaCfg){.dst = copies + length + 1ULL, .src = tab->key, .bytes = head + length + 1ULL});
    char *const root_copy = copies + (2ULL * (length + 1ULL)) + head;
    if (g_cli_model.root != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = root_copy, .src = g_cli_model.root, .bytes = root_size});
    }
    root_copy[root_size] = '\0';
    *opening = (CliModelOpening){copies, copies + length + 1ULL, root_copy, g_cli_model.rows, CLI_FILES_OPEN_REFUSED};
    EMBED_CALL(tabs_show, ControllerTabsCtx, .at = g_cli_model.tabs - 1u);
    EMBED_CALL(cli_model.session_save, CliModelCtx, .at = 0u);
    EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_FILES, .run = tabs_open_run,
               .done = tabs_open_done, .context = opening, .busy = 1);
}

void cli_model_tab_close(const CliModelCtx *args)
{
    if (args->at >= g_cli_model.tabs)
    {
        return;
    }
    CliModelTab *const tab = g_cli_model.tab[args->at];
    // a tab with changes not saved closes on a second close, as tabs.js closes one
    if (tab->changed && !tab->closing)
    {
        tab->closing = 1;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_TABS);
        return;
    }
    if (tab->viewed)
    {
        EMBED_CALL(cli_view_table.close, CliViewCfg, .key = tab->key);
    }
    EMBED_CALL(cli_model.server_close, CliModelCtx, .at = args->at);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = tab->key);
    g_cli_cells->persistent_buf_release(tab, &g_cli_controller_cells);
    for (unsigned int after = args->at + 1u; after < g_cli_model.tabs; after += 1u)
    {
        g_cli_model.tab[after - 1u] = g_cli_model.tab[after];
    }
    g_cli_model.tabs -= 1u;
    // the tabs are fewer than an int counts
    const int closed = (int)args->at;
    if (g_cli_model.last_active > closed)
    {
        g_cli_model.last_active = g_cli_model.last_active - 1;
    }
    else if (g_cli_model.last_active == closed)
    {
        g_cli_model.last_active = -1;
    }
    else
    {
        g_cli_model.last_active = g_cli_model.last_active;
    }
    if (g_cli_model.tabs == 0u)
    {
        g_cli_model.active = -1;
        g_cli_model.session_known = 1;
        EMBED_CALL(cli_model.panels_hide, CliModelCtx, .at = 0u);
        EMBED_CALL(cli_model.editing, CliModelCtx, .on = 0);
        EMBED_CALL(cli_model.shown_dirty, CliModelCtx, .at = 0u);
    }
    else if (closed == g_cli_model.active)
    {
        g_cli_model.active = -1;
        EMBED_CALL(tabs_show, ControllerTabsCtx,
                   .at = (args->at < g_cli_model.tabs) ? args->at : g_cli_model.tabs - 1u);
    }
    else
    {
        g_cli_model.active -= (closed < g_cli_model.active) ? 1 : 0;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_TABS);
    }
    EMBED_CALL(cli_model.session_save, CliModelCtx, .at = 0u);
}

void cli_model_tabs_close(const CliModelCtx *args)
{
    (void)args;
    // each tab closed once, the last first, so those before it stand where they stood: a changed one stands, its close
    // waiting on a second
    for (unsigned int at = g_cli_model.tabs; at > 0u; at -= 1u)
    {
        EMBED_CALL(cli_model.tab_close, CliModelCtx, .at = at - 1u);
    }
}

void cli_model_tab_last(const CliModelCtx *args)
{
    (void)args;
    if ((g_cli_model.last_active >= 0) && ((unsigned int)g_cli_model.last_active < g_cli_model.tabs))
    {
        EMBED_CALL(cli_model.tab_show, CliModelCtx, .at = (unsigned int)g_cli_model.last_active);
    }
}

// what runs on the files' lane: the value saved
static void tabs_save_run(void *context)
{
    const CliModelSaving *const saving = (const CliModelSaving *)context;
    char folder[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        (void)cli_state_value_save(
            &(CliStateValueCfg){.folder = folder, .key = saving->key, .value = saving->value, .err_msg = &err_msg});
    }
}

static void tabs_save_done(void *context)
{
    g_cli_cells->persistent_buf_release(context, &g_cli_controller_cells);
}

void cli_model_value_save(const CliModelCtx *args)
{
    const unsigned long long key_size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) + 1ULL;
    const unsigned long long value_size =
        cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = CLI_CELLS_POOL}) + 1ULL;
    CliModelSaving *const saving =
        g_cli_cells->persistent_buf_alloc(sizeof(*saving) + key_size + value_size, &g_cli_controller_cells);
    if (saving == NULL)
    {
        return;
    }
    char *const key = (char *)(void *)(saving + 1);
    char *const value = key + key_size;
    memor.cpy(&(MemoriaCfg){.dst = key, .src = args->path, .bytes = key_size});
    memor.cpy(&(MemoriaCfg){.dst = value, .src = args->value, .bytes = value_size});
    *saving = (CliModelSaving){key, value};
    if (args->on)
    {
        // the window is ending: saved here, the lanes having gone
        tabs_save_run(saving);
        tabs_save_done(saving);
        return;
    }
    EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_FILES, .run = tabs_save_run,
               .done = tabs_save_done, .context = saving);
}

// the session's text in a cell of the controller's, the tab shown's path and then every tab's, a line each; NULL where
// the cells cannot meet it
EMBED_INLINE char *tabs_session_text(const ControllerTabsCtx *args)
{
    (void)args;
    unsigned long long bytes = 2ULL;
    for (unsigned int at = 0u; at < g_cli_model.tabs; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = g_cli_model.tab[at]->path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    }
    bytes +=
        (g_cli_model.active >= 0)
            ? cellul.len(&(CatenaFinitaCfg){.src = g_cli_model.tab[g_cli_model.active]->path, .cap = CLI_PATH_MAX_LEN})
            : 0ULL;
    char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_controller_cells);
    if (cell != NULL)
    {
        VerbaLinea text = {cell, bytes, 0ULL};
        mmgr_line_put(&text, (g_cli_model.active >= 0) ? g_cli_model.tab[g_cli_model.active]->path : "");
        mmgr_line_ch(&text, '\n');
        for (unsigned int at = 0u; at < g_cli_model.tabs; at += 1u)
        {
            mmgr_line_put(&text, g_cli_model.tab[at]->path);
            mmgr_line_ch(&text, '\n');
        }
        (void)mmgr_line_finish(&text);
    }
    return cell;
}

void cli_model_session_save(const CliModelCtx *args)
{
    if (!g_cli_model.restored || (g_cli_model.root == NULL) || (!g_cli_model.session_known && (g_cli_model.tabs == 0u)))
    {
        return;
    }
    char key[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea keyed = {key, sizeof(key), 0ULL};
    mmgr_line_put(&keyed, CLI_MODEL_SESSION_KEY);
    mmgr_line_put(&keyed, g_cli_model.root);
    char *const session = EMBED_CALL(tabs_session_text, ControllerTabsCtx, .at = 0u);
    if ((session != NULL) && (mmgr_line_finish(&keyed) != 0ULL))
    {
        EMBED_CALL(cli_model.value_save, CliModelCtx, .path = key, .value = session, .on = args->on);
    }
    g_cli_cells->persistent_buf_release(session, &g_cli_controller_cells);
    const CliTool *const tool = (g_cli_model.tool >= 0) ? &g_cli_tools[g_cli_model.tool] : NULL;
    if ((tool != NULL) && tool->side)
    {
        EMBED_CALL(cli_model.value_save, CliModelCtx, .path = CLI_MODEL_GROUP_KEY, .value = tool->name, .on = args->on);
    }
}

void cli_model_session_restore(const CliModelCtx *args)
{
    const CliStateEntries *const entries = (const CliStateEntries *)args->input;
    const char *const group = cli_state_entry_value(entries, CLI_MODEL_GROUP_KEY);
    for (unsigned int at = 0u; (group != NULL) && (at < g_cli_tools_count); at += 1u)
    {
        if (g_cli_tools[at].side &&
            cellul.eq(&(CatenaFinitaCfg){.src = g_cli_tools[at].name, .other = group, .cap = 32ULL}))
        {
            // the tools are fewer than an int counts
            g_cli_model.tool = (int)at;
            g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS);
            EMBED_CALL(cli_model.explorer_show, CliModelCtx, .at = 0u);
        }
    }
    char key[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea keyed = {key, sizeof(key), 0ULL};
    mmgr_line_put(&keyed, CLI_MODEL_SESSION_KEY);
    mmgr_line_put(&keyed, (g_cli_model.root != NULL) ? g_cli_model.root : "");
    const char *const session = (mmgr_line_finish(&keyed) != 0ULL) ? cli_state_entry_value(entries, key) : NULL;
    g_cli_model.session_known = session != NULL;
    if (session != NULL)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = session, .cap = CLI_CELLS_POOL});
        const unsigned long long shown_end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                        .text = session, .at = 0ULL, .size = size, .set = "\n");
        for (unsigned long long at = shown_end + 1ULL; at < size;)
        {
            const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                      .text = session, .at = at, .size = size, .set = "\n");
            char path[CLI_PATH_MAX_LEN];
            VerbaLinea line = {path, sizeof(path), 0ULL};
            mmgr_line_put_n(&line, session + at, end - at);
            if ((end > at) && (mmgr_line_finish(&line) != 0ULL))
            {
                EMBED_CALL(cli_model.file_open, CliModelCtx, .path = path);
            }
            at = end + 1ULL;
        }
        char shown[CLI_PATH_MAX_LEN];
        VerbaLinea line = {shown, sizeof(shown), 0ULL};
        mmgr_line_put_n(&line, session, shown_end);
        if ((shown_end != 0ULL) && (mmgr_line_finish(&line) != 0ULL))
        {
            EMBED_CALL(cli_model.file_open, CliModelCtx, .path = shown);
        }
    }
    g_cli_model.restored = 1;
}
