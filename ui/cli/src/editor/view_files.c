// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// view_files.c: the files open as views, which are the window's tabs. A file is read where it is opened, its text
// stored whole or paged under a key of its path's own and its grammar applied, and its view opened on the text at the
// editor's size. The session is the views' paths in order, a line each, the shown one's first, saved in orior's folder
// under a key of the tree's own
#include "view.h"

#include "../command_line/pool.h"
#include "../plugins/plugins.h"
#include "../text/text.h"
#include "files.h"

// the head of the key a file's text is stored under, and the key of orior's folder the session stands under, the
// tree's folder after it
#define CLI_VIEW_KEY_HEAD "window:"
#define CLI_VIEW_SESSION_KEY "window.session."

typedef struct
{
    const char *root;
    const char *path;
    const char *key;
    char *relative;
    unsigned long long length;
    unsigned int rows;
    CliPosition *place;
} ViewFilesCtx;

// the line and the column the `length` bytes at `path` end in, each a colon and digits counted from 1, as a place
// counted from 0 into `place`, a line alone standing at the line's start: how many bytes stand before them, `length`
// where the bytes end in no colon and digits
EMBED_INLINE unsigned long long view_files_place(const ViewFilesCtx *args)
{
    unsigned long long length = args->length;
    unsigned long long numbers[2] = {0ULL, 0ULL};
    unsigned int found = 0u;
    int reading = 1;
    while (reading && (found < 2u))
    {
        const unsigned long long after = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                    .text = args->path, .at = length, .set = ":");
        reading = (after > 1ULL) && (after < length) &&
                  (EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->path,
                              .at = after, .size = length, .classes = CLI_TEXT_DIGITS, .set = "") == length);
        if (reading)
        {
            numbers[found] = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = args->path, .at = after,
                                        .size = length, .end = NULL);
            found += 1u;
            length = after - 1ULL;
        }
    }
    // the last number read is the column where two were read, and the line where one was
    const unsigned long long line = (found == 2u) ? numbers[1] : numbers[0];
    const unsigned long long col = (found == 2u) ? numbers[0] : 1ULL;
    *args->place = (CliPosition){(line != 0ULL) ? (line - 1ULL) : 0ULL, (col != 0ULL) ? (col - 1ULL) : 0ULL};
    return length;
}

// the bytes of `path` from the tree's top where it is a whole path inside the tree, into `relative`, sized for
// CLI_PATH_MAX_LEN: how many, a path that is not whole being from the tree's top already
EMBED_INLINE unsigned long long view_files_relative(const ViewFilesCtx *args)
{
    const char *const path = args->path;
    const int whole = (path[0] == '/') || (path[0] == '\\') || ((path[0] != '\0') && (path[1] == ':'));
    char full[CLI_PATH_MAX_LEN];
    if (whole && (args->root != NULL) &&
        EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = path, .full = full,
                   .capacity = sizeof(full)))
    {
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .path = full, .root = args->root,
                   .relative_out = args->relative);
    }
    else
    {
        VerbaLinea line = {args->relative, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, path);
        (void)mmgr_line_finish(&line);
    }
    return cellul.len(&(CatenaFinitaCfg){.src = args->relative, .cap = CLI_PATH_MAX_LEN});
}

// the view whose path is `length` bytes at `path`, or -1
EMBED_INLINE int view_files_by_path(const ViewFilesCtx *args)
{
    const unsigned int count = cli_view_count();
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const path = cli_view_path_at(at);
        if ((cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) == args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = path, .other = args->path, .bytes = args->length}) == 0))
        {
            // the views are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// the text under `key` set to color by the grammar of the language its path is of
EMBED_INLINE void view_files_colored(const ViewFilesCtx *args)
{
    char id[CLI_NAME_MAX_LEN];
    char grammar[CLI_NAME_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliLanguages languages;
    EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &languages);
    const int known = EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                 .languages_read = &languages, .path = args->path, .id_out = id);
    g_cli_guard->persistent_buf_release(languages.pair, g_cli_plugins_owner);
    *(&languages) = (CliLanguages){NULL, 0u, 0u};
    grammar[0] = '\0';
    if (known && !EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = id, .key = grammar,
                             .err_msg = &err_msg))
    {
        grammar[0] = '\0';
    }
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = args->key,
                     .grammar = grammar, .named = 0);
}

int cli_view_file_open(const char *root, const char *path, unsigned int rows, unsigned int columns,
                       const CliPosition *place, VerbaLinea *err_msg)
{
    // the path less the line and the column it ends in, which put the cursor where no place is passed
    char path_read[CLI_PATH_MAX_LEN];
    VerbaLinea copied = {path_read, sizeof(path_read), 0ULL};
    mmgr_line_put(&copied, path);
    if (mmgr_line_finish(&copied) == 0ULL)
    {
        mmgr_line_put(err_msg, "the path is longer than a path is read");
        return -1;
    }
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = path_read, .cap = sizeof(path_read)});
    CliPosition read = {0ULL, 0ULL};
    const unsigned long long before =
        EMBED_CALL(view_files_place, ViewFilesCtx, .path = path_read, .length = whole, .place = &read);
    path_read[before] = '\0';
    const CliPosition *const placed_at = (place != NULL) ? place : ((before != whole) ? &read : NULL);
    char relative[CLI_PATH_MAX_LEN];
    const unsigned long long length =
        EMBED_CALL(view_files_relative, ViewFilesCtx, .root = root, .path = path_read, .relative = relative);
    int at = EMBED_CALL(view_files_by_path, ViewFilesCtx, .path = relative, .length = length);
    char key[CLI_PATH_MAX_LEN + sizeof(CLI_VIEW_KEY_HEAD)];
    VerbaLinea keyed = {key, sizeof(key), 0ULL};
    mmgr_line_put(&keyed, CLI_VIEW_KEY_HEAD);
    mmgr_line_put(&keyed, relative);
    if ((at < 0) && (mmgr_line_finish(&keyed) != 0ULL))
    {
        const unsigned int opened = EMBED_CALL(cli_files_opening.open, CliFilesOpenCfg, .root = root, .file = relative,
                                               .key = key, .err_msg = err_msg);
        if ((opened != CLI_FILES_OPEN_WHOLE) && (opened != CLI_FILES_OPEN_PAGED))
        {
            return -1;
        }
        if (opened == CLI_FILES_OPEN_PAGED)
        {
            (void)EMBED_CALL(cli_document_key_from.page, CliDocumentKeyFromCfg, .key = key, .from = 0ULL,
                             .to = rows + 1ULL);
        }
        else
        {
            (void)EMBED_CALL(cli_document_key_from.cache, CliDocumentKeyFromCfg, .key = key, .from = 0ULL,
                             .to = rows + 1ULL);
        }
        EMBED_CALL(view_files_colored, ViewFilesCtx, .key = key, .path = relative);
        if (!EMBED_CALL(cli_view_table.open, CliViewCfg, .key = key, .path = relative))
        {
            EMBED_CALL(cli_document.release, CliDocumentCfg, .key = key);
            mmgr_line_put(err_msg, relative);
            mmgr_line_put(err_msg, ": no view could be opened on it");
            return -1;
        }
        const CliViewInput sized = {.type = CLI_VIEW_SIZE, .rows = rows, .columns = columns};
        (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &sized, .err_msg = err_msg);
        // the view opened stands last in the table, which is fewer than an int counts
        at = (int)cli_view_count() - 1;
    }
    if (at < 0)
    {
        return -1;
    }
    // the view is among the views
    cli_view_show((unsigned int)at);
    if (placed_at != NULL)
    {
        const CliViewInput placed = {.type = CLI_VIEW_POSITION, .row = placed_at->line, .column = placed_at->col};
        (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = cli_view_key((unsigned int)at),
                         .input = &placed, .err_msg = err_msg);
    }
    return at;
}

void cli_view_file_close(unsigned int at)
{
    const char *const shown = cli_view_key(at);
    if (shown == NULL)
    {
        return;
    }
    char key[CLI_PATH_MAX_LEN + sizeof(CLI_VIEW_KEY_HEAD)];
    VerbaLinea keyed = {key, sizeof(key), 0ULL};
    mmgr_line_put(&keyed, shown);
    (void)mmgr_line_finish(&keyed);
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = key);
}

void cli_view_session_save(const char *root)
{
    char folder[CLI_PATH_MAX_LEN];
    if ((root == NULL) || !EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        return;
    }
    const unsigned int count = cli_view_count();
    const int shown = cli_view_shown();
    unsigned long long bytes = 2ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = cli_view_path_at(at), .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    }
    // the view shown is among the views
    const char *const shown_path = (shown >= 0) ? cli_view_path_at((unsigned int)shown) : "";
    bytes += cellul.len(&(CatenaFinitaCfg){.src = shown_path, .cap = CLI_PATH_MAX_LEN});
    char *const cell = g_cli_guard->temporary_buf_alloc(bytes, g_cli_views_owner);
    if (cell == NULL)
    {
        return;
    }
    VerbaLinea text = {cell, bytes, 0ULL};
    mmgr_line_put(&text, shown_path);
    mmgr_line_ch(&text, '\n');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&text, cli_view_path_at(at));
        mmgr_line_ch(&text, '\n');
    }
    (void)mmgr_line_finish(&text);
    char key[CLI_PATH_MAX_LEN + sizeof(CLI_VIEW_SESSION_KEY)];
    VerbaLinea keyed = {key, sizeof(key), 0ULL};
    mmgr_line_put(&keyed, CLI_VIEW_SESSION_KEY);
    mmgr_line_put(&keyed, root);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (mmgr_line_finish(&keyed) != 0ULL)
    {
        (void)cli_state_value_save(
            &(CliStateValueCfg){.folder = folder, .key = key, .value = cell, .err_msg = &err_msg});
    }
    g_cli_guard->persistent_buf_release(cell, g_cli_views_owner);
}

void cli_view_session_restore(const char *root, const CliStateEntries *entries, unsigned int rows, unsigned int columns)
{
    char key[CLI_PATH_MAX_LEN + sizeof(CLI_VIEW_SESSION_KEY)];
    VerbaLinea keyed = {key, sizeof(key), 0ULL};
    mmgr_line_put(&keyed, CLI_VIEW_SESSION_KEY);
    mmgr_line_put(&keyed, (root != NULL) ? root : "");
    const char *const session =
        ((entries != NULL) && (mmgr_line_finish(&keyed) != 0ULL)) ? cli_state_entry_value(entries, key) : NULL;
    if (session == NULL)
    {
        return;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = session, .cap = CLI_POOL_BYTES});
    const unsigned long long shown_end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                    .text = session, .at = 0ULL, .size = size, .set = "\n");
    for (unsigned long long at = shown_end + 1ULL; at < size;)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = session, .at = at, .size = size, .set = "\n");
        char path[CLI_PATH_MAX_LEN];
        VerbaLinea line = {path, sizeof(path), 0ULL};
        mmgr_line_put_n(&line, session + at, end - at);
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        if ((end > at) && (mmgr_line_finish(&line) != 0ULL))
        {
            (void)cli_view_file_open(root, path, rows, columns, NULL, &err_msg);
        }
        at = end + 1ULL;
    }
    char shown[CLI_PATH_MAX_LEN];
    VerbaLinea line = {shown, sizeof(shown), 0ULL};
    mmgr_line_put_n(&line, session, shown_end);
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if ((shown_end != 0ULL) && (mmgr_line_finish(&line) != 0ULL))
    {
        (void)cli_view_file_open(root, shown, rows, columns, NULL, &err_msg);
    }
}
