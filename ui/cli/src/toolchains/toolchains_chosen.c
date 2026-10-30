// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_chosen.c: the folders the reader gave orior, by tool, stored in toolchains.json in orior's own folder; and
// what each tool's search found, cached in toolchains_found.json there, so a tool is searched for again only where what
// the search reads has changed
#include "toolchains_internal.h"

// A tool's search cached: its id, how it was found, as CliFound's `state` records, and its program, empty where it is
// missing
typedef struct
{
    char id[CLI_NAME_MAX_LEN];
    const char *state;
    char program[CLI_PATH_MAX_LEN];
} CliToolFound;

// The searches cached: what the search read as they were made, the PATH's folders, the manifest as built and the
// reader's own two files, as one hash; the tools, `count` of them in a cell of `most`; whether the file was read; the
// checks of every tool locking the cache, which is written once as the last lets it go; and whether it changed since
// it was written
typedef struct
{
    unsigned long long against;
    CliToolFound *tool;
    unsigned int count;
    unsigned int most;
    int read;
    unsigned int refs;
    int changed;
} CliToolsFound;

static CliToolsFound s_found;
static CliLock s_found_lock = CLI_LOCK_INIT;
static CellOwner s_found_cells = MMGR_CELL_OWNER("cli_toolchains_found");

// the states a search is cached with: those found by what the search reads, not by a variable or the tree
static const char *const s_found_states[4] = {"chosen", "path", "found", "missing"};

// the places the cache begins with
#define CLI_FOUND_FIRST 64u

// The folders the reader gave, by tool, read into a tree sized to them with room for one folder more, which
// cli_json_given gives back: the object of them
static unsigned int cli_chosen_read(CliJson *json, char *path)
{
    const int named = cli_toolchains_file("toolchains.json", path);
    return cli_toolchains_file_read(json, named ? path : "", 1u, CLI_PATH_MAX_LEN + CLI_NAME_MAX_LEN);
}

// the tree given back, and the lock with it
static void cli_chosen_released(CliJson *json)
{
    cli_json_given(json);
    cli_lock_give(&g_cli_toolchains_lock);
}

int cli_toolchains_chosen(const char *id, char *folder)
{
    CliJson json;
    char path[CLI_PATH_MAX_LEN];
    cli_lock_take(&g_cli_toolchains_lock);
    const unsigned int chosen = cli_chosen_read(&json, path);
    const char *const given = cli_json_string(&json, cli_json_member(&json, chosen, id), NULL);
    if (given != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = given,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = given, .cap = CLI_CELLS_POOL}) + 1u});
    }
    cli_chosen_released(&json);
    return given != NULL;
}

void cli_toolchains_chosen_each(void (*each)(void *context, const char *id, const char *folder), void *context)
{
    static char s_ids[256][CLI_NAME_MAX_LEN];
    static char s_folders[256][CLI_PATH_MAX_LEN];
    CliJson json;
    char path[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    cli_lock_take(&g_cli_toolchains_lock);
    const unsigned int chosen = cli_chosen_read(&json, path);
    cli_json_sorted(&json, chosen);
    for (unsigned int item = json.node[chosen].first; (item != CLI_JSON_NONE) && (count < 256u);
         item = json.node[item].next)
    {
        const char *const folder = cli_json_string(&json, item, NULL);
        if (folder != NULL)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = s_ids[count], .bytes = CLI_NAME_MAX_LEN, .src = json.text + json.node[item].key});
            memor.cpy(
                &(MemoriaCfg){.dst = s_folders[count],
                              .src = folder,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
            count += 1u;
        }
    }
    cli_chosen_released(&json);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        each(context, s_ids[at], s_folders[at]);
    }
}

int cli_toolchains_choose(const char *id, const char *folder, char *program, VerbaLinea *err_msg)
{
    CliJson manifest_json;
    char dir[CLI_PATH_MAX_LEN];
    cli_toolchains_trimmed(folder, dir, sizeof(dir));
    const unsigned int manifest = cli_toolchains_manifest(&manifest_json, 0u, 0ull);
    const unsigned int tool = cli_toolchains_tool(&manifest_json, manifest, id, err_msg);
    if (tool == CLI_JSON_NONE)
    {
        cli_json_given(&manifest_json);
        return 0;
    }
    if (!cli_toolchains_tool_program_in(&manifest_json, tool, dir, program))
    {
        mmgr_line_put(err_msg, dir);
        mmgr_line_put(err_msg, " contains none of ");
        const unsigned int programs = cli_json_member(&manifest_json, tool, "programs");
        for (unsigned int item = manifest_json.node[programs].first; item != CLI_JSON_NONE;
             item = manifest_json.node[item].next)
        {
            mmgr_line_put(err_msg, (item != manifest_json.node[programs].first) ? ", " : "");
            const char *const name = cli_json_string(&manifest_json, item, NULL);
            mmgr_line_put(err_msg, (name != NULL) ? name : "");
        }
        cli_json_given(&manifest_json);
        return 0;
    }
    cli_json_given(&manifest_json);
    CliJson json;
    char path[CLI_PATH_MAX_LEN];
    cli_lock_take(&g_cli_toolchains_lock);
    const unsigned int chosen = cli_chosen_read(&json, path);
    cli_json_set(&json, chosen, id,
                 cli_json_string_made(&json, dir, cellul.len(&(CatenaFinitaCfg){.src = dir, .cap = CLI_CELLS_POOL})));
    const int saved = (path[0] != '\0') && cli_toolchains_file_saved(&json, chosen, path, 1, err_msg);
    cli_chosen_released(&json);
    if ((path[0] == '\0') && !saved)
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own to store toolchains in");
    }
    return saved;
}

int cli_toolchains_forget(const char *id, VerbaLinea *err_msg)
{
    CliJson json;
    char path[CLI_PATH_MAX_LEN];
    cli_lock_take(&g_cli_toolchains_lock);
    const unsigned int chosen = cli_chosen_read(&json, path);
    int saved = 1;
    if (cli_json_member(&json, chosen, id) != CLI_JSON_NONE)
    {
        cli_json_remove(&json, chosen, id);
        saved = cli_toolchains_file_saved(&json, chosen, path, 1, err_msg);
    }
    cli_chosen_released(&json);
    return saved;
}

int cli_toolchains_chosen_program(const char *id, char *path)
{
    CliJson manifest_json;
    char folder[CLI_PATH_MAX_LEN];
    if (!cli_toolchains_chosen(id, folder))
    {
        return 0;
    }
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ull};
    const unsigned int tool =
        cli_toolchains_tool(&manifest_json, cli_toolchains_manifest(&manifest_json, 0u, 0ull), id, &err_msg);
    const int found = (tool != CLI_JSON_NONE) && cli_toolchains_tool_program_in(&manifest_json, tool, folder, path);
    cli_json_given(&manifest_json);
    return found;
}

// the hash `hash` carried on over `size` bytes at `bytes`, FNV-1a's
static unsigned long long found_hashed(unsigned long long hash, const void *bytes, unsigned long long size)
{
    const unsigned char *const at = bytes;
    for (unsigned long long one = 0ull; one < size; one += 1ull)
    {
        hash = (hash ^ at[one]) * 1099511628211ull;
    }
    return hash;
}

// what the search reads, as one hash: the manifest as built, the PATH's `count` folders, and the size and the time of
// the last change of the reader's own toolchains and of the folders the reader gave
static unsigned long long found_against(const char (*path)[CLI_PATH_MAX_LEN], unsigned int count)
{
    static unsigned long long s_manifest;
    // the manifest is built into the program, and is hashed once
    s_manifest = (s_manifest != 0ull)
                     ? s_manifest
                     : found_hashed(14695981039346656037ull, g_cli_toolchains_json, g_cli_toolchains_json_size);
    unsigned long long hash = s_manifest;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        hash =
            found_hashed(hash, path[at], cellul.len(&(CatenaFinitaCfg){.src = path[at], .cap = CLI_CELLS_POOL}) + 1u);
    }
    static const char *const s_files[2] = {"user_toolchains.json", "toolchains.json"};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        char file[CLI_PATH_MAX_LEN];
        unsigned long long stamp[2] = {0ull, 0ull};
        if (cli_toolchains_file(s_files[at], file))
        {
            (void)cli_file_stamp(file, &stamp[0], &stamp[1]);
        }
        hash = found_hashed(hash, stamp, sizeof(stamp));
    }
    return hash;
}

// the place of the tool cached of `id`, or the cache's count where none is. Under the cache's lock
static unsigned int found_place(const char *id)
{
    unsigned int place = 0u;
    while ((place < s_found.count) &&
           !cellul.eq(&(CatenaFinitaCfg){.src = s_found.tool[place].id, .other = id, .cap = CLI_NAME_MAX_LEN}))
    {
        place += 1u;
    }
    return place;
}

// tool `id` cached as found `state`, with `program`, in its place or after the others, the cache grown to fit it: 1, or
// 0 where the id is too long or the cellblock cannot meet the cache. Under the cache's lock
static int found_put(const char *id, const char *state, const char *program)
{
    const unsigned long long id_length = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_NAME_MAX_LEN});
    const unsigned long long program_length = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_PATH_MAX_LEN});
    const unsigned int place = found_place(id);
    if ((id_length >= CLI_NAME_MAX_LEN) || (program_length >= CLI_PATH_MAX_LEN))
    {
        return 0;
    }
    if (place == s_found.most)
    {
        const unsigned int most = (s_found.most == 0u) ? CLI_FOUND_FIRST : (s_found.most * 2u);
        CliToolFound *const grown =
            g_cli_cells->persistent_buf_alloc((unsigned long long)most * sizeof(*grown), &s_found_cells);
        if (grown == NULL)
        {
            return 0;
        }
        if (s_found.tool != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = s_found.tool, .bytes = s_found.count * sizeof(*grown)});
        }
        cli_cell_release(s_found.tool, &s_found_cells);
        s_found.tool = grown;
        s_found.most = most;
    }
    CliToolFound *const tool = &s_found.tool[place];
    memor.cpy(&(MemoriaCfg){.dst = tool->id, .src = id, .bytes = (size_t)id_length + 1u});
    memor.cpy(&(MemoriaCfg){.dst = tool->program, .src = program, .bytes = (size_t)program_length + 1u});
    tool->state = state;
    s_found.count += (place == s_found.count) ? 1u : 0u;
    s_found.changed = 1;
    return 1;
}

// the state a search is cached with that reads as `text`, or NULL where none does
static const char *found_state(const char *text)
{
    for (unsigned int at = 0u; (text != NULL) && (at < 4u); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = text, .other = s_found_states[at], .cap = CLI_NAME_MAX_LEN}))
        {
            return s_found_states[at];
        }
    }
    return NULL;
}

// the cache read from toolchains_found.json once, and emptied where what the search reads is not what it was as the
// cache was made. Under the cache's lock
static void found_current(unsigned long long against)
{
    if (!s_found.read)
    {
        s_found.read = 1;
        CliJson json;
        char path[CLI_PATH_MAX_LEN];
        const int named = cli_toolchains_file("toolchains_found.json", path);
        const unsigned int root_node = cli_toolchains_file_read(&json, named ? path : "", 0u, 0ull);
        const char *const hex = cli_json_string(&json, cli_json_member(&json, root_node, "against"), NULL);
        for (const char *at = hex; (at != NULL) && (*at != '\0'); at += 1)
        {
            // a hex digit read as its value, which an unsigned long long represents
            const unsigned long long digit =
                (*at >= 'a') ? (unsigned long long)(*at - 'a' + 10) : (unsigned long long)(*at - '0');
            s_found.against = (s_found.against << 4u) | (digit & 0xFull);
        }
        const unsigned int tools = cli_json_member(&json, root_node, "tools");
        for (unsigned int item = cli_json_is(&json, tools, CFG_JSON_OBJECT) ? json.node[tools].first : CLI_JSON_NONE;
             item != CLI_JSON_NONE; item = json.node[item].next)
        {
            const char *const state = found_state(cli_json_string(&json, cli_json_member(&json, item, "state"), NULL));
            const char *const program = cli_json_string(&json, cli_json_member(&json, item, "program"), NULL);
            if (state != NULL)
            {
                (void)found_put(json.text + json.node[item].key, state, (program != NULL) ? program : "");
            }
        }
        cli_json_given(&json);
        s_found.changed = 0;
    }
    if (s_found.against != against)
    {
        s_found.against = against;
        s_found.count = 0u;
        s_found.changed = 1;
    }
}

// the cache written to toolchains_found.json where it changed since it was, its folder made. Under the cache's lock
static void found_written(void)
{
    char path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    if (!s_found.changed || !cli_toolchains_file("toolchains_found.json", path))
    {
        return;
    }
    if (cli_home_folder(folder))
    {
        query_folder_made(folder);
    }
    char hex[17];
    for (unsigned int at = 0u; at < 16u; at += 1u)
    {
        hex[at] = "0123456789abcdef"[(s_found.against >> ((15u - at) * 4u)) & 0xFull];
    }
    hex[16] = '\0';
    CliFileWriting file;
    char piece[4096];
    if (!cli_file_begun(&file, path))
    {
        return;
    }
    VerbaLinea out = {piece, sizeof(piece), 0ull, cli_file_put, &file, 0ull};
    mmgr_line_put(&out, "{\n  \"against\": \"");
    mmgr_line_put(&out, hex);
    mmgr_line_put(&out, "\",\n  \"tools\": {");
    for (unsigned int at = 0u; at < s_found.count; at += 1u)
    {
        const CliToolFound *const tool = &s_found.tool[at];
        mmgr_line_put(&out, (at != 0u) ? ",\n    " : "\n    ");
        cli_json_quoted_lossy(&out, (const unsigned char *)tool->id,
                              cellul.len(&(CatenaFinitaCfg){.src = tool->id, .cap = CLI_NAME_MAX_LEN}));
        mmgr_line_put(&out, ": {\"state\": \"");
        mmgr_line_put(&out, tool->state);
        mmgr_line_put(&out, "\", \"program\": ");
        cli_json_quoted_lossy(&out, (const unsigned char *)tool->program,
                              cellul.len(&(CatenaFinitaCfg){.src = tool->program, .cap = CLI_PATH_MAX_LEN}));
        mmgr_line_ch(&out, '}');
    }
    mmgr_line_put(&out, "\n  }\n}\n");
    const int written = mmgr_line_flush(&out);
    s_found.changed = !cli_file_ended(&file, written);
}

int cli_toolchains_found_cached(const char *id, const char (*path)[CLI_PATH_MAX_LEN], unsigned int count,
                                CliFound *found)
{
    const unsigned long long against = found_against(path, count);
    cli_lock_take(&s_found_lock);
    found_current(against);
    const unsigned int place = found_place(id);
    const CliToolFound *const tool = (place < s_found.count) ? &s_found.tool[place] : NULL;
    // a program cached that is gone from its folder is searched for again
    const int cached = (tool != NULL) && ((tool->program[0] == '\0') || query_is_file(tool->program));
    if (cached)
    {
        found->state = tool->state;
        found->has_program = tool->program[0] != '\0';
        memor.cpy(&(MemoriaCfg){.dst = found->program, .src = tool->program, .bytes = CLI_PATH_MAX_LEN});
        memor.cpy(&(MemoriaCfg){.dst = found->folder, .src = tool->program, .bytes = CLI_PATH_MAX_LEN});
        char *cut = found->folder;
        for (char *at = found->folder; *at != '\0'; at += 1)
        {
            cut = ((*at == '/') || (*at == '\\')) ? at : cut;
        }
        *cut = '\0';
    }
    cli_lock_give(&s_found_lock);
    return cached;
}

void cli_toolchains_found_put(const char *id, const char (*path)[CLI_PATH_MAX_LEN], unsigned int count,
                              const CliFound *found)
{
    const char *const state = found_state(found->state);
    if (state == NULL)
    {
        return;
    }
    const unsigned long long against = found_against(path, count);
    cli_lock_take(&s_found_lock);
    found_current(against);
    (void)found_put(id, state, found->has_program ? found->program : "");
    if (s_found.refs == 0u)
    {
        found_written();
    }
    cli_lock_give(&s_found_lock);
}

void cli_toolchains_found_ref(void)
{
    cli_lock_take(&s_found_lock);
    s_found.refs += 1u;
    cli_lock_give(&s_found_lock);
}

void cli_toolchains_found_let(void)
{
    cli_lock_take(&s_found_lock);
    s_found.refs -= 1u;
    if (s_found.refs == 0u)
    {
        found_written();
    }
    cli_lock_give(&s_found_lock);
}
