// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recent.c: the files opened last and the commands run last. A list is one cell of its entries, each ended by a 0,
// with the offset each starts at beside it, read from the entry orior's folder stores it under the first time it is
// asked and written back there as a push changes it, a line an entry
#include "recent.h"

#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"
#include "state.h"

// the key each list is stored under in orior's folder
static const char *const s_recent_keys[CLI_RECENT_LISTS] = {"recent.files", "recent.commands"};

// A list: its entries in one movable cell, each ended by a 0, NULL where it has none, and the cell's bytes; the
// offset each starts at; how many; and whether it was read from orior's folder
typedef struct
{
    const CarcerMovableEntry *text;
    unsigned long long bytes;
    unsigned int start[CLI_RECENT_MOST];
    unsigned int count;
    int read;
} RecentList;

static RecentList s_recent[CLI_RECENT_LISTS];

typedef struct
{
    unsigned int list;
    const char *text;
    unsigned long long length;
    const CellOwner *owner;
} RecentCtx;

// the list `list`'s entries read out of their cell into a temporary cell of `owner`, which the caller gives back; NULL
// where the list has none or the cell could not be taken
EMBED_INLINE char *recent_text_copy(const RecentCtx *args)
{
    const RecentList *const list = &s_recent[args->list];
    char *const copy = (list->text != NULL) ? g_cli_guard->temporary_buf_alloc(list->bytes, args->owner) : NULL;
    if (copy != NULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = list->text, .offset = 0ULL, .dst = copy, .bytes = list->bytes});
    }
    return copy;
}

// the list `list` laid anew from `length` bytes of `text`, its entries a line each, an empty line none, at most
// CLI_RECENT_MOST of them, in a cell sized to them with their ends; the list as it was where the cells cannot meet it
EMBED_INLINE void recent_laid(const RecentCtx *args)
{
    RecentList *const list = &s_recent[args->list];
    unsigned long long bytes = 0ULL;
    unsigned int count = 0u;
    for (unsigned long long at = 0ULL; (at < args->length) && (count < CLI_RECENT_MOST);)
    {
        unsigned long long end = at;
        while ((end < args->length) && (args->text[end] != '\n'))
        {
            end += 1ULL;
        }
        bytes += (end > at) ? (end - at) + 1ULL : 0ULL;
        count += (end > at) ? 1u : 0u;
        at = end + 1ULL;
    }
    // the entries laid in a temporary cell, then written whole into a movable cell of the list's own
    char *const cell = (bytes != 0ULL) ? g_cli_guard->temporary_buf_alloc(bytes, g_cli_machine_owner) : NULL;
    const CarcerMovableEntry *const kept =
        (cell != NULL) ? g_cli_guard->movable_buf_alloc(bytes, g_cli_machine_owner) : NULL;
    if ((bytes != 0ULL) && (kept == NULL))
    {
        g_cli_guard->persistent_buf_release(cell, g_cli_machine_owner);
        return;
    }
    unsigned int laid = 0u;
    unsigned long long used = 0ULL;
    for (unsigned long long at = 0ULL; (at < args->length) && (laid < count);)
    {
        unsigned long long end = at;
        while ((end < args->length) && (args->text[end] != '\n'))
        {
            end += 1ULL;
        }
        if (end > at)
        {
            memor.cpy(&(MemoriaCfg){.dst = cell + used, .src = args->text + at, .bytes = end - at});
            cell[used + (end - at)] = '\0';
            // the cell is below CLI_RECENT_MOST entries of a path's length, far inside an unsigned int
            list->start[laid] = (unsigned int)used;
            used += (end - at) + 1ULL;
            laid += 1u;
        }
        at = end + 1ULL;
    }
    if (kept != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = kept, .offset = 0ULL, .src = cell, .bytes = bytes});
    }
    g_cli_guard->persistent_buf_release(cell, g_cli_machine_owner);
    (void)g_cli_guard->movable_buf_release(list->text, g_cli_machine_owner);
    list->text = kept;
    list->bytes = bytes;
    list->count = laid;
}

// the list `list` read from orior's folder the first time it is asked
EMBED_INLINE void recent_read(const RecentCtx *args)
{
    RecentList *const list = &s_recent[args->list];
    if (list->read)
    {
        return;
    }
    list->read = 1;
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        return;
    }
    CliStateEntries entries =
        cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = folder, .owner = g_cli_machine_owner});
    // the list's text read out of the entries' cell into a cell of this call
    const unsigned long long size = cli_state_entry_size(&entries, s_recent_keys[args->list]);
    char *const value =
        (size != CLI_STATE_NONE) ? g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_machine_owner) : NULL;
    if ((value != NULL) && (cli_state_entry_value(&entries, s_recent_keys[args->list], value, size + 1ULL) != NULL))
    {
        EMBED_CALL(recent_laid, RecentCtx, .list = args->list, .text = value, .length = size);
    }
    g_cli_guard->persistent_buf_release(value, g_cli_machine_owner);
    cli_state_entries_release(&entries);
}

void cli_recent_push(const CliRecentCfg *args)
{
    if ((args->list >= CLI_RECENT_LISTS) || (args->entry == NULL))
    {
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->entry, .cap = CLI_PATH_MAX_LEN});
    const int broken =
        cellul.find(&(CatenaFinitaCfg){.src = args->entry, .cap = length, .other = "\n", .other_cap = 1ULL}) != NULL;
    if ((length == 0ULL) || broken)
    {
        return;
    }
    EMBED_CALL(recent_read, RecentCtx, .list = args->list);
    const RecentList *const list = &s_recent[args->list];
    // the entries read out, then the entry first and every other entry of the list in its order, a line each, for
    // this push alone
    char *const text = EMBED_CALL(recent_text_copy, RecentCtx, .list = args->list, .owner = g_cli_machine_owner);
    const unsigned int count = (text != NULL) ? list->count : 0u;
    unsigned long long bytes = length + 1ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = text + list->start[at], .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    }
    char *const joined = g_cli_guard->temporary_buf_alloc(bytes + CLI_ERR_MSG_MAX_LEN, g_cli_machine_owner);
    if (joined == NULL)
    {
        g_cli_guard->persistent_buf_release(text, g_cli_machine_owner);
        return;
    }
    VerbaLinea line = {joined, bytes, 0ULL};
    mmgr_line_put(&line, args->entry);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const one = text + list->start[at];
        if (!cellul.eq(&(CatenaFinitaCfg){.src = one, .other = args->entry, .cap = CLI_PATH_MAX_LEN}))
        {
            mmgr_line_ch(&line, '\n');
            mmgr_line_put(&line, one);
        }
    }
    const unsigned long long written = mmgr_line_finish(&line);
    EMBED_CALL(recent_laid, RecentCtx, .list = args->list, .text = joined, .length = written);
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {joined + bytes, CLI_ERR_MSG_MAX_LEN, 0ULL};
    if ((written != 0ULL) && EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        (void)cli_state_value_save(&(CliStateValueCfg){
            .folder = folder, .key = s_recent_keys[args->list], .value = joined, .err_msg = &err_msg});
    }
    g_cli_guard->persistent_buf_release(joined, g_cli_machine_owner);
    g_cli_guard->persistent_buf_release(text, g_cli_machine_owner);
}

unsigned int cli_recent_list(const CliRecentCfg *args)
{
    *args->cell_out = NULL;
    if (args->list >= CLI_RECENT_LISTS)
    {
        return 0u;
    }
    EMBED_CALL(recent_read, RecentCtx, .list = args->list);
    const RecentList *const list = &s_recent[args->list];
    // the entries read out into the caller's cell, the rows standing in it
    char *const text = EMBED_CALL(recent_text_copy, RecentCtx, .list = args->list, .owner = args->owner);
    *args->cell_out = text;
    const unsigned int count = (text == NULL) ? 0u : (list->count < args->most) ? list->count : args->most;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        args->rows_out[at] = text + list->start[at];
    }
    return count;
}
