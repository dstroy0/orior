// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_gather.c: the database gathered from the tree's builds: each CMake build's entries from its file API
// reply, and each Meson build's compile_commands.json, written together where they changed, and read for the
// compilers they name
#include "../command_line/cells.h"
#include "../text/text.h"
#include "compile_db_internal.h"

CliGathering g_cli_gather;

// the gathering's cells given back, those it has; and taken: 1, or 0 with none kept where the cells cannot meet them
static void gather_cells_given(void)
{
    cli_cell_given(g_cli_gather.written);
    cli_cell_given(g_cli_gather.reply_text);
    cli_cell_given(g_cli_gather.reply_nodes);
    cli_cell_given(g_cli_gather.tokens);
    cli_cell_given(g_cli_gather.text);
    cli_cell_given(g_cli_gather.nodes);
    g_cli_gather.written = NULL;
    g_cli_gather.reply_text = NULL;
    g_cli_gather.reply_nodes = NULL;
    g_cli_gather.tokens = NULL;
    g_cli_gather.text = NULL;
    g_cli_gather.nodes = NULL;
}

static int gather_cells_taken(void)
{
    g_cli_gather.nodes = cli_cell_taken(CLI_GATHER_NODES * sizeof(CliJsonNode), "the compile entries' nodes");
    g_cli_gather.text = cli_cell_taken(CLI_GATHER_TEXT, "the compile entries' texts");
    g_cli_gather.tokens = cli_cell_taken(CLI_GATHER_NODES * sizeof(CfgJsonToken), "the compile entries' tokens");
    g_cli_gather.reply_nodes = cli_cell_taken(CLI_REPLY_NODES * sizeof(CliJsonNode), "a CMake reply's nodes");
    g_cli_gather.reply_text = cli_cell_taken(CLI_REPLY_TEXT, "a CMake reply's texts");
    g_cli_gather.written = cli_cell_taken(CLI_GATHER_WRITTEN, "the compile database written");
    const int taken = (g_cli_gather.nodes != NULL) && (g_cli_gather.text != NULL) && (g_cli_gather.tokens != NULL) &&
                      (g_cli_gather.reply_nodes != NULL) && (g_cli_gather.reply_text != NULL) &&
                      (g_cli_gather.written != NULL);
    if (!taken)
    {
        gather_cells_given();
    }
    return taken;
}

// a database of the entries `entries`, given to clangd from `folder` where one is given, with the compilers they name
static void cli_of_entries(const char *folder, unsigned int entries, CliDatabase *database)
{
    CliJson *const json = &g_cli_gather.json;
    database->has_folder = folder != NULL;
    memor.cpy(&(MemoriaCfg){
        .dst = database->folder,
        .src = (folder != NULL) ? folder : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (folder != NULL) ? folder : "", .cap = CLI_CELLS_POOL}) + 1u});
    database->compilers = 0u;
    database->msvc = 0;
    for (unsigned int at = 0u; cli_json_item(json, entries, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int entry = cli_json_item(json, entries, at);
        char compiler[CLI_PATH_LONGEST];
        const unsigned int arguments = cli_json_member(json, entry, "arguments");
        const char *const first = cli_json_string(json, cli_json_item(json, arguments, 0u), NULL);
        unsigned long long size = 0ull;
        const char *const command = cli_json_string(json, cli_json_member(json, entry, "command"), &size);
        int found = 0;
        if (cli_json_is(json, arguments, CFG_JSON_ARRAY))
        {
            found = first != NULL;
            memor.cpy(&(MemoriaCfg){
                .dst = compiler,
                .src = (first != NULL) ? first : "",
                .bytes =
                    cellul.len(&(CatenaFinitaCfg){.src = (first != NULL) ? first : "", .cap = CLI_CELLS_POOL}) + 1u});
        }
        else if (command != NULL)
        {
            found = cli_compile_first_word(command, size, compiler);
        }
        if (!found)
        {
            continue;
        }
        for (char *letter = compiler; letter[0] != '\0'; letter += 1)
        {
            letter[0] = (letter[0] == '\\') ? '/' : letter[0];
        }
        // the compilers kept in order, each once
        unsigned int place = 0u;
        while ((place < database->compilers) && (cli_text_order(database->compiler[place], compiler) < 0))
        {
            place += 1u;
        }
        if (((place < database->compilers) &&
             (cellul.eq(
                 &(CatenaFinitaCfg){.src = database->compiler[place], .other = compiler, .cap = CLI_CELLS_POOL}))) ||
            (database->compilers >= CLI_COMPILERS))
        {
            continue;
        }
        for (unsigned int move = database->compilers; move > place; move -= 1u)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = database->compiler[move],
                .src = database->compiler[move - 1u],
                .bytes =
                    cellul.len(&(CatenaFinitaCfg){.src = database->compiler[move - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        }
        memor.cpy(&(MemoriaCfg){.dst = database->compiler[place],
                                .src = compiler,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = compiler, .cap = CLI_CELLS_POOL}) + 1u});
        database->compilers += 1u;
        database->msvc = database->msvc || cli_compile_db_is_cl(compiler);
    }
}

// a project's entries kept where it has none yet or these were configured later
static void cli_project_put(const char *project, unsigned long long time, unsigned int entries)
{
    unsigned int at = 0u;
    while ((at < g_cli_gather.projects) &&
           (!cellul.eq(
               &(CatenaFinitaCfg){.src = g_cli_gather.project[at].project, .other = project, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    if ((at == g_cli_gather.projects) && (g_cli_gather.projects < CLI_GATHER_PROJECTS))
    {
        memor.cpy(&(MemoriaCfg){.dst = g_cli_gather.project[at].project,
                                .src = project,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = project, .cap = CLI_CELLS_POOL}) + 1u});
        g_cli_gather.project[at].time = time;
        g_cli_gather.project[at].entries = entries;
        g_cli_gather.projects += 1u;
    }
    else if ((at < g_cli_gather.projects) && (g_cli_gather.project[at].time < time))
    {
        g_cli_gather.project[at].time = time;
        g_cli_gather.project[at].entries = entries;
    }
}

// the entries written where they differ from the file there
static void cli_written(const char *path, unsigned int entries)
{
    CliLine out = {g_cli_gather.written, CLI_GATHER_WRITTEN, 0ull};
    cli_json_write(&g_cli_gather.json, entries, &out, 2u, 0);
    if (!cli_line_fits(&out))
    {
        return;
    }
    unsigned long long size = 0ull;
    const unsigned char *const there = qry_member_file_mapped(path, &size);
    const int same = (there != NULL) && (size == out.at) &&
                     (memor.cmp(&(MemoriaCfg){.src = there, .other = g_cli_gather.written, .bytes = size}) == 0);
    if (there != NULL)
    {
        qry_member_file_unmapped(there, size);
    }
    char folder[CLI_PATH_LONGEST];
    if (!same && cli_path_parent(path, folder))
    {
        query_folder_made(folder);
        query_file_written(path, g_cli_gather.written, out.at);
    }
}

// the gathering itself, its cells taken: the database's compilers and folder
static void gather_held(const char *root, const CliInputs *inputs, CliDatabase *database);

void cli_compile_gather(const char *root, const CliInputs *inputs, CliDatabase *database)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    if (gather_cells_taken())
    {
        gather_held(root, inputs, database);
        gather_cells_given();
    }
    else
    {
        database->has_folder = 0;
        database->folder[0] = '\0';
        database->compilers = 0u;
        database->msvc = 0;
    }
    cli_lock_give(&s_lock);
}

static void gather_held(const char *root, const CliInputs *inputs, CliDatabase *database)
{
    CliJson *const json = &g_cli_gather.json;
    cli_json_begun(json, g_cli_gather.nodes, CLI_GATHER_NODES, g_cli_gather.text, CLI_GATHER_TEXT);
    if (inputs->own)
    {
        const unsigned int own =
            cli_compile_json_read(json, inputs->file[0].path, g_cli_gather.tokens, CLI_GATHER_NODES);
        cli_of_entries(NULL, own, database);
        return;
    }
    // each CMake project's newest configuration, by the project's own folder, and each Meson build's
    g_cli_gather.projects = 0u;
    for (unsigned int index = 0u; index < inputs->count; index += 1u)
    {
        const CliStamped *const file = &inputs->file[index];
        const char *name = file->path;
        for (const char *at = file->path; at[0] != '\0'; at += 1)
        {
            name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
        }
        char project[CLI_PATH_LONGEST] = "";
        unsigned int entries = CLI_JSON_NONE;
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "compile_commands.json", .cap = CLI_CELLS_POOL}))
        {
            char folder[CLI_PATH_LONGEST];
            entries = cli_compile_json_read(json, file->path, g_cli_gather.tokens, CLI_GATHER_NODES);
            entries = cli_json_is(json, entries, CFG_JSON_ARRAY) ? entries : cli_json_made(json, CFG_JSON_ARRAY);
            if (!cli_path_parent(file->path, folder) || !cli_path_parent(folder, project))
            {
                project[0] = '\0';
            }
        }
        else
        {
            entries = cli_compile_cmake_entries(file->path, project);
        }
        if (entries != CLI_JSON_NONE)
        {
            cli_project_put(project, file->time, entries);
        }
    }
    // the projects by their folders, their entries one after another
    for (unsigned int at = 1u; at < g_cli_gather.projects; at += 1u)
    {
        const CliProject held = g_cli_gather.project[at];
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(g_cli_gather.project[place - 1u].project, held.project) > 0))
        {
            g_cli_gather.project[place] = g_cli_gather.project[place - 1u];
            place -= 1u;
        }
        g_cli_gather.project[place] = held;
    }
    const unsigned int all = cli_json_made(json, CFG_JSON_ARRAY);
    for (unsigned int at = 0u; at < g_cli_gather.projects; at += 1u)
    {
        const unsigned int entries = g_cli_gather.project[at].entries;
        for (unsigned int item = 0u; cli_json_item(json, entries, item) != CLI_JSON_NONE; item += 1u)
        {
            cli_json_append(json, all, cli_json_copy(json, cli_json_item(json, entries, item)));
        }
    }
    if (cli_json_item(json, all, 0u) == CLI_JSON_NONE)
    {
        database->has_folder = 0;
        database->folder[0] = '\0';
        database->compilers = 0u;
        database->msvc = 0;
        return;
    }
    char path[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    cli_full(root, CLI_COMPILE_WRITTEN, path);
    cli_written(path, all);
    cli_of_entries(cli_path_parent(path, folder) ? folder : NULL, all, database);
}
