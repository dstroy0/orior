// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carry.c: the files of orior's own folder that travel, packed and sent to a file or a folder, and read back from one
#include "../code/git.h"
#include "../command_line/cells.h"
#include "../deploy/devcontainer.h"
#include "../text/text.h"
#include "carry_internal.h"

// the most a file of the reader's that travels holds
#define CLI_CARRY_LARGEST (16ull << 20u)

// the files that travel, each a file or a folder of orior's own folder, by its place there; and the settings that
// name this machine's own and stay on it, each a key or the start of keys that ends in `.`: the part of settings.json
// for this machine alone, how programs are signed here, the environments of the trees here, the memory the window may
// take here, and the file or folder the settings are carried through
static const char *const s_carried[7] = {"settings.json", "menus.json",       "user.css",  "themes/",
                                         "plugins/",      "plugin_settings/", "languages/"};
static const char *const s_machine[6] = {"machine",       "signing.",   "environment.",
                                         "memory-budget", "cores-kept", "carry.place"};

void cli_carried_clear(CliCarried *carried)
{
    for (unsigned int at = 0u; at < carried->count; at += 1u)
    {
        cli_cell_given(carried->value[at]);
        cli_cell_given(carried->place[at]);
        carried->value[at] = NULL;
        carried->place[at] = NULL;
    }
    carried->count = 0u;
}

unsigned int cli_carried_find(const CliCarried *carried, const char *place)
{
    for (unsigned int at = 0u; at < carried->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = carried->place[at], .other = place, .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    return carried->count;
}

const char *cli_carried_place(const CliCarried *carried, unsigned int at)
{
    return carried->place[at];
}

const char *cli_carried_value(const CliCarried *carried, unsigned int at)
{
    return carried->value[at];
}

void cli_carry_room_given(CliCarryRoom *room)
{
    cli_json_given(&room->json);
}

void cli_carry_room_begun(CliCarryRoom *room)
{
    cli_json_begun(&room->json, NULL, 0u, NULL, 0ull);
}

// the cells a carry's work took given back, its paths and its room, and its lock
static void carry_released(CliCarried *paths, CliCarryRoom *room, CliLock *lock)
{
    cli_carried_clear(paths);
    cli_carry_room_given(room);
    cli_lock_give(lock);
}

// `size` bytes of `text` in a cell of their own, a 0 after them: the cell, or NULL where none could be taken
static char *cli_carried_cell(const char *text, unsigned long long size, const char *what)
{
    char *const cell = (size < CLI_CARRIED_TEXT) ? cli_cell_taken(size + 1ull, what) : NULL;
    if (cell != NULL)
    {
        // a size below CLI_CARRIED_TEXT fits a size_t
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = text, .bytes = (size_t)size});
        cell[size] = '\0';
    }
    return cell;
}

int cli_carried_put(CliCarried *carried, const char *place, const char *value, unsigned long long size)
{
    unsigned int at = 0u;
    while ((at < carried->count) && (cli_text_order(carried->place[at], place) < 0))
    {
        at += 1u;
    }
    const int there = (at < carried->count) &&
                      (cellul.eq(&(CatenaFinitaCfg){.src = carried->place[at], .other = place, .cap = CLI_CELLS_POOL}));
    if (!there && (carried->count >= CLI_CARRIED_MOST))
    {
        return 0;
    }
    char *const pooled_value = cli_carried_cell(value, size, "a carried file's text");
    char *const pooled_place =
        there ? carried->place[at]
              : cli_carried_cell(place, cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}),
                                 "a carried place");
    if ((pooled_place == NULL) || (pooled_value == NULL))
    {
        cli_cell_given(pooled_value);
        if (!there)
        {
            cli_cell_given(pooled_place);
        }
        return 0;
    }
    if (there)
    {
        cli_cell_given(carried->value[at]);
    }
    else
    {
        for (unsigned int move = carried->count; move > at; move -= 1u)
        {
            carried->place[move] = carried->place[move - 1u];
            carried->value[move] = carried->value[move - 1u];
            carried->size[move] = carried->size[move - 1u];
        }
        carried->count += 1u;
    }
    carried->place[at] = pooled_place;
    carried->value[at] = pooled_value;
    carried->size[at] = size;
    return 1;
}

int cli_carry_machine(const char *key)
{
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_machine[at], .cap = CLI_CELLS_POOL});
        const int start = (s_machine[at][length - 1ull] == '.') && cli_text_same(key, s_machine[at], length);
        if ((cellul.eq(&(CatenaFinitaCfg){.src = key, .other = s_machine[at], .cap = CLI_CELLS_POOL})) || start)
        {
            return 1;
        }
    }
    return 0;
}

// `size` bytes of settings text made plain, which is never longer than the text, and read into the room's tree, a
// fresh one in a cell sized to it with `nodes_more` nodes and `text_more` bytes over where `fresh` is 1, else beside
// what it holds: the object read, an empty one where it is no object, CLI_JSON_NONE where the tree has no cell
static unsigned int carry_plain_read(CliCarryRoom *room, const char *text, unsigned long long size, int fresh,
                                     unsigned int nodes_more, unsigned long long text_more)
{
    char *const plain_text = cli_cell_taken(size + 1ull, "a settings file's plain text");
    CliLine plain = {plain_text, (plain_text != NULL) ? (size + 1ull) : 0ull, 0ull};
    if (plain_text != NULL)
    {
        cli_plain_json(text, size, &plain);
    }
    char why[CLI_NAME_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    unsigned int root = CLI_JSON_NONE;
    if (fresh)
    {
        cli_json_given(&room->json);
        root = (plain_text != NULL) ? cli_json_taken(&room->json, plain_text, plain.at, nodes_more, text_more, &unread)
                                    : CLI_JSON_NONE;
        if (room->json.node == NULL)
        {
            (void)cli_json_held(&room->json, nodes_more, text_more, "a settings tree");
        }
    }
    else if (plain_text != NULL)
    {
        const unsigned int tokens_most = cli_json_tokens_counted(plain_text, plain.at) + 1u;
        CfgJsonToken *const tokens =
            cli_cell_taken((unsigned long long)tokens_most * sizeof(*tokens), "a settings file's tokens");
        root = (tokens != NULL) ? cli_json_read(&room->json, plain_text, plain.at, tokens, tokens_most, &unread)
                                : CLI_JSON_NONE;
        cli_cell_given(tokens);
    }
    cli_cell_given(plain_text);
    if (room->json.node == NULL)
    {
        return CLI_JSON_NONE;
    }
    return cli_json_is(&room->json, root, CFG_JSON_OBJECT) ? root : cli_json_made(&room->json, CFG_JSON_OBJECT);
}

unsigned int cli_carry_settings_of(CliCarryRoom *room, const char *text, unsigned long long size,
                                   unsigned long long beside)
{
    // a text read beside makes no more nodes than its bytes, and no more text than twice them with a 0 after each,
    // and a copy of what it read takes as much again; the four over are the empty objects either side may be made
    return carry_plain_read(room, text, size, 1, (unsigned int)((2ull * beside) + 4ull), (4ull * beside) + 64ull);
}

unsigned int cli_carry_settings_beside(CliCarryRoom *room, const char *text, unsigned long long size)
{
    return carry_plain_read(room, text, size, 0, 0u, 0ull);
}

void cli_carry_settings_written(CliCarryRoom *room, unsigned int object, CliLine *out)
{
    cli_json_sorted(&room->json, object);
    cli_json_write(&room->json, object, out, 2u, 0);
    cli_line_character(out, '\n');
}

int cli_carry_allowed(const char *place)
{
    const char *part = place;
    for (const char *at = place;; at += 1)
    {
        if ((at[0] == '/') || (at[0] == '\0'))
        {
            // the part lies before `at`, its end at or past its start
            const unsigned long long size = (unsigned long long)(at - part);
            if ((size == 0ull) || ((size == 1ull) && (part[0] == '.')) ||
                ((size == 2ull) && (part[0] == '.') && (part[1] == '.')))
            {
                return 0;
            }
            if (at[0] == '\0')
            {
                break;
            }
            part = at + 1;
        }
    }
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_carried[at], .cap = CLI_CELLS_POOL});
        const int under = (s_carried[at][length - 1ull] == '/') && cli_text_same(place, s_carried[at], length);
        if ((cellul.eq(&(CatenaFinitaCfg){.src = place, .other = s_carried[at], .cap = CLI_CELLS_POOL})) || under)
        {
            return 1;
        }
    }
    return 0;
}

// A walk of a folder whose files travel: the folder, the place its files start with, and the files found
typedef struct
{
    const char *folder;
    const char *under;
    CliCarried *found;
} CliUnder;

static void cli_under_each(void *context, const char *name, int is_folder)
{
    const CliUnder *const walk = (const CliUnder *)context;
    if (name[0] == '.')
    {
        return;
    }
    char path[CLI_PATH_LONGEST];
    char place[CLI_PATH_LONGEST];
    cli_full(walk->folder, name, path);
    CliLine line = {place, sizeof(place), 0ull};
    cli_line_text(&line, walk->under);
    cli_line_text(&line, name);
    if (is_folder)
    {
        cli_line_character(&line, '/');
    }
    if (cli_line_closed(&line) == 0ull)
    {
        return;
    }
    if (is_folder)
    {
        const CliUnder below = {path, place, walk->found};
        query_folder_each(path, cli_under_each, (void *)&below);
        return;
    }
    cli_carried_put(walk->found, place, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
}

void cli_carry_files(const char *folder, CliCarried *found)
{
    cli_carried_clear(found);
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        char path[CLI_PATH_LONGEST];
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_carried[at], .cap = CLI_CELLS_POOL});
        if (s_carried[at][length - 1ull] == '/')
        {
            char dir[CLI_NAME_LONGEST];
            memor.cpy(&(MemoriaCfg){
                .dst = dir,
                .src = s_carried[at],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_carried[at], .cap = CLI_CELLS_POOL}) + 1u});
            dir[length - 1ull] = '\0';
            cli_full(folder, dir, path);
            const CliUnder walk = {path, s_carried[at], found};
            query_folder_each(path, cli_under_each, (void *)&walk);
            continue;
        }
        cli_full(folder, s_carried[at], path);
        if (query_is_file(path))
        {
            cli_carried_put(found, s_carried[at], path,
                            cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        }
    }
}

int cli_carry_pack(const char *home, CliCarried *packed, CliLine *said)
{
    static CliCarried s_paths;
    static CliCarryRoom s_room;
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    cli_carry_room_begun(&s_room);
    cli_carried_clear(packed);
    cli_carry_files(home, &s_paths);
    for (unsigned int at = 0u; at < s_paths.count; at += 1u)
    {
        const char *const place = cli_carried_place(&s_paths, at);
        const char *const path = cli_carried_value(&s_paths, at);
        unsigned long long size = 0ull;
        if (cli_file_size(path, &size) && (size > CLI_CARRY_LARGEST))
        {
            cli_line_text(said, path);
            cli_line_text(said, " is over 16 MB, the most a file that travels holds");
            carry_released(&s_paths, &s_room, &s_lock);
            return 0;
        }
        const unsigned char *const bytes = cli_file_held_utf8(path, &size);
        if (bytes == NULL)
        {
            continue;
        }
        if (cellul.eq(&(CatenaFinitaCfg){.src = place, .other = "settings.json", .cap = CLI_CELLS_POOL}))
        {
            const unsigned int object = cli_carry_settings_of(&s_room, (const char *)bytes, size, 0ull);
            for (unsigned int member = (object != CLI_JSON_NONE) ? s_room.json.node[object].first : CLI_JSON_NONE;
                 member != CLI_JSON_NONE;)
            {
                const unsigned int next = s_room.json.node[member].next;
                const char *const key = s_room.json.text + s_room.json.node[member].key;
                if (cli_carry_machine(key))
                {
                    cli_json_remove(&s_room.json, object, key);
                }
                member = next;
            }
            CliLine written;
            if ((object != CLI_JSON_NONE) &&
                cli_line_growing(&written, CLI_CARRY_WRITTEN_FIRST, "a settings file written"))
            {
                cli_carry_settings_written(&s_room, object, &written);
                if (cli_line_fits(&written))
                {
                    cli_carried_put(packed, place, written.out, written.at);
                }
                cli_cell_given(written.out);
            }
        }
        else
        {
            cli_carried_put(packed, place, (const char *)bytes, size);
        }
        cli_file_let_go(bytes, size);
    }
    carry_released(&s_paths, &s_room, &s_lock);
    return 1;
}

// whether the repository at `folder` has a remote
static int cli_has_remote(const char *folder)
{
    static const char *const s_remote[1] = {"remote"};
    char remotes[CLI_NAME_LONGEST * 4u];
    char why[CLI_NAME_LONGEST];
    CliLine out = {remotes, sizeof(remotes), 0ull};
    CliLine unrun = {why, sizeof(why), 0ull};
    return cli_git_run(folder, s_remote, 1u, &out, &unrun) && (out.at > 0ull);
}

int cli_carry_read(const char *from, CliCarried *there, CliLine *said)
{
    static CliCarried s_paths;
    static CliCarryRoom s_room;
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    cli_carry_room_begun(&s_room);
    cli_carried_clear(there);
    if (query_is_folder(from))
    {
        char git[CLI_PATH_LONGEST];
        cli_full(from, ".git", git);
        char words[CLI_NAME_LONGEST * 4u];
        CliLine pulled = {words, sizeof(words), 0ull};
        if ((query_is_file(git) || query_is_folder(git)) && cli_has_remote(from) && !cli_git_pull(from, &pulled, said))
        {
            carry_released(&s_paths, &s_room, &s_lock);
            return 0;
        }
        cli_carry_files(from, &s_paths);
        for (unsigned int at = 0u; at < s_paths.count; at += 1u)
        {
            unsigned long long size = 0ull;
            const unsigned char *const bytes = cli_file_held_utf8(cli_carried_value(&s_paths, at), &size);
            if (bytes != NULL)
            {
                cli_carried_put(there, cli_carried_place(&s_paths, at), (const char *)bytes, size);
                cli_file_let_go(bytes, size);
            }
        }
        carry_released(&s_paths, &s_room, &s_lock);
        return 1;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held_utf8(from, &size);
    if (bytes == NULL)
    {
        cli_line_text(said, from);
        cli_line_text(said, ": it could not be read");
        carry_released(&s_paths, &s_room, &s_lock);
        return 0;
    }
    char why[CLI_NAME_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_taken(&s_room.json, (const char *)bytes, size, 0u, 0ull, &unread);
    cli_file_let_go(bytes, size);
    const char *const mark = cli_json_string(&s_room.json, cli_json_member(&s_room.json, root, "orior"), NULL);
    const unsigned int files = cli_json_member(&s_room.json, root, "files");
    int read = (root != CLI_JSON_NONE) && (mark != NULL) &&
               (cellul.eq(&(CatenaFinitaCfg){.src = mark, .other = CLI_CARRY_MARK, .cap = CLI_CELLS_POOL})) &&
               cli_json_is(&s_room.json, files, CFG_JSON_OBJECT);
    for (unsigned int member = read ? s_room.json.node[files].first : CLI_JSON_NONE; member != CLI_JSON_NONE;
         member = s_room.json.node[member].next)
    {
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(&s_room.json, member, &length);
        const char *const place = s_room.json.text + s_room.json.node[member].key;
        read = read && (text != NULL);
        if ((text != NULL) && cli_carry_allowed(place))
        {
            cli_carried_put(there, place, text, length);
        }
    }
    if (!read)
    {
        cli_line_text(said, from);
        cli_line_text(said, (root == CLI_JSON_NONE) ? ": it is no JSON" : " holds no settings orior_cli carried");
    }
    carry_released(&s_paths, &s_room, &s_lock);
    return read;
}
