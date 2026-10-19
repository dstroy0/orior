// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_dotnet.c: what a build that made its program can be told of it past what it loads: a .NET program that
// needs a .NET runtime a program opened on its own does not find, and the modules a PyInstaller build warns it did
// not find that the tree's own code imports, each with the hidden import that takes it in
#include "../text/json_tree.h"
#include "../text/text.h"
#include "executables_internal.h"

// the most places the .NET runtime is looked for, the most numbers of a version read, and the most nodes and bytes of a
// runtimeconfig.json read
#define CLI_DOTNET_PLACES 8u

#define CLI_DOTNET_NUMBERS 8u

#define CLI_DOTNET_NODES 4096u

#define CLI_DOTNET_BYTES (1u << 16u)

// a line said through `say`
static void cli_say(QuerySink say, const char *text)
{
    say.said(say.context, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the folders a program opened on its own looks in for the .NET runtime, DOTNET_ROOT where the system sets it, then
// where .NET installs for every user, into `places`: their count
static unsigned int cli_dotnet_places(char (*places)[CLI_PATH_LONGEST])
{
    unsigned int count = 0u;
    if (query_environment_read("DOTNET_ROOT", places[count], CLI_PATH_LONGEST))
    {
        count += 1u;
    }
#if defined(_WIN32)
    char files[CLI_PATH_LONGEST];
    if (query_environment_read("ProgramFiles", files, sizeof(files)) &&
        cli_executables_joined(files, "dotnet", places[count]))
    {
        count += 1u;
    }
#else
    char text[CLI_PATH_LONGEST];
    if (cli_text_read("/etc/dotnet/install_location", text, sizeof(text)))
    {
        const char *from = text;
        unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
        cli_scan_trim(&from, &length);
        cli_scan_copied(from, length, places[count], CLI_PATH_LONGEST);
        count += 1u;
    }
    static const char *const s_usual[3] = {"/usr/share/dotnet", "/usr/lib/dotnet", "/usr/local/share/dotnet"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = places[count],
                          .src = s_usual[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_usual[at], .cap = CLI_CELLS_POOL}) + 1u});
        count += 1u;
    }
#endif
    return count;
}

// a version's numbers, its parts between dots and dashes that are numbers, into `numbers`: their count
static unsigned int cli_numbers(const char *version, unsigned long long *numbers)
{
    unsigned int count = 0u;
    const char *part = version;
    while ((part[0] != '\0') && (count < CLI_DOTNET_NUMBERS))
    {
        const char *end = part;
        while ((end[0] != '\0') && (end[0] != '.') && (end[0] != '-'))
        {
            end += 1;
        }
        int number = end > part;
        unsigned long long value = 0ull;
        for (const char *digit = part; number && (digit < end); digit += 1)
        {
            number = (digit[0] >= '0') && (digit[0] <= '9') && (value <= 0xffffffffull);
            value = (value * 10ull) + (unsigned long long)(digit[0] - '0');
        }
        if (number && (value <= 0xffffffffull))
        {
            numbers[count] = value;
            count += 1u;
        }
        part = (end[0] != '\0') ? (end + 1) : end;
    }
    return count;
}

// A framework looked for among a place's versions of it: the numbers wanted, and whether one held is new enough
typedef struct
{
    unsigned long long wanted[CLI_DOTNET_NUMBERS];
    unsigned int wanted_count;
    int held;
} CliFramework;

// a version a place holds: one with the same first number as the one wanted and at least as new holds it
static void cli_version_each(void *context, const char *name, int is_folder)
{
    (void)is_folder;
    CliFramework *const framework = (CliFramework *)context;
    unsigned long long have[CLI_DOTNET_NUMBERS];
    const unsigned int count = cli_numbers(name, have);
    if ((count == 0u) || (framework->wanted_count == 0u) || (have[0] != framework->wanted[0]))
    {
        return;
    }
    // the numbers compared in turn, the longer newer where one runs out
    int order = 0;
    for (unsigned int at = 0u; (order == 0) && (at < count) && (at < framework->wanted_count); at += 1u)
    {
        order = (have[at] < framework->wanted[at]) ? -1 : (have[at] > framework->wanted[at]) ? 1 : 0;
    }
    order = (order != 0) ? order : ((count < framework->wanted_count) ? -1 : 0);
    framework->held = framework->held || (order >= 0);
}

// a framework a runtimeconfig.json names said where no place holds a version of it
static void cli_framework_told(const CliJson *json, unsigned int framework, char (*places)[CLI_PATH_LONGEST],
                               unsigned int place_count, const char *file, QuerySink say)
{
    const char *const name = cli_json_string(json, cli_json_member(json, framework, "name"), NULL);
    const char *const version = cli_json_string(json, cli_json_member(json, framework, "version"), NULL);
    if ((name == NULL) || (version == NULL))
    {
        return;
    }
    CliFramework wanted;
    wanted.wanted_count = cli_numbers(version, wanted.wanted);
    wanted.held = 0;
    for (unsigned int at = 0u; !wanted.held && (at < place_count); at += 1u)
    {
        char shared[CLI_PATH_LONGEST];
        char folder[CLI_PATH_LONGEST];
        if (cli_executables_joined(places[at], "shared", shared) && cli_executables_joined(shared, name, folder))
        {
            query_folder_each(folder, cli_version_each, &wanted);
        }
    }
    if (wanted.held)
    {
        return;
    }
    const char *const base =
        (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/') != NULL)
            ? (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/') + 1)
        : (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '\\') != NULL)
            ? (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '\\') + 1)
            : file;
    char said[CLI_PATH_LONGEST * (CLI_DOTNET_PLACES + 2u)];
    CliLine line = {said, sizeof(said), 0ull};
    cli_line_text(&line, base);
    cli_line_text(&line, " needs the .NET runtime ");
    cli_line_text(&line, name);
    cli_line_character(&line, ' ');
    cli_line_text(&line, version);
    cli_line_text(&line, ", which a program opened on its own looks for in ");
    for (unsigned int at = 0u; at < place_count; at += 1u)
    {
        cli_line_text(&line, (at != 0u) ? " and " : "");
        cli_line_text(&line, places[at]);
    }
    cli_line_text(&line, " and does not find: Publish as One File, the job's output one file, carries it inside the "
                         "program");
    cli_line_closed(&line);
    cli_say(say, said);
}

// what a .NET program that is no single file needs of the .NET runtime, said where a program opened on its own finds
// none: each framework its runtimeconfig.json names, by name and version, that no place holds a version of with the
// same first number and at least as new
static void cli_dotnet_missing(const char *file, QuerySink say)
{
    static CliJsonNode s_nodes[CLI_DOTNET_NODES];
    static CfgJsonToken s_tokens[CLI_DOTNET_NODES];
    static char s_file[CLI_DOTNET_BYTES];
    static char s_text[CLI_DOTNET_BYTES];
    static char s_places[CLI_DOTNET_PLACES][CLI_PATH_LONGEST];
    static CliLock s_lock = CLI_LOCK_INIT;
    // the program's path with its extension put as runtimeconfig.json
    char config[CLI_PATH_LONGEST];
    const char *const base =
        (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/') != NULL)
            ? (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/') + 1)
        : (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '\\') != NULL)
            ? (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '\\') + 1)
            : file;
    const char *const dot =
        (base[0] != '\0')
            ? cli_text_last(base + 1, cellul.len(&(CatenaFinitaCfg){.src = base + 1, .cap = CLI_CELLS_POOL}), '.')
            : NULL;
    CliLine named = {config, sizeof(config), 0ull};
    cli_line_counted(&named, file,
                     (dot != NULL) ? (unsigned long long)(dot - file)
                                   : cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
    cli_line_text(&named, ".runtimeconfig.json");
    if (cli_line_closed(&named) == 0ull)
    {
        return;
    }
    cli_lock_take(&s_lock);
    const unsigned long long size = qry_member_file_read(config, (unsigned char *)s_file, sizeof(s_file));
    CliJson json;
    cli_json_begun(&json, s_nodes, CLI_DOTNET_NODES, s_text, sizeof(s_text));
    char why[CLI_NAME_LONGEST];
    CliLine reason = {why, sizeof(why), 0ull};
    const unsigned int root = (size != QRY_MEMBER_UNREAD)
                                  ? cli_json_read(&json, s_file, size, s_tokens, CLI_DOTNET_NODES, &reason)
                                  : CLI_JSON_NONE;
    const unsigned int options = cli_json_member(&json, root, "runtimeOptions");
    if ((options == CLI_JSON_NONE) || (cli_json_member(&json, options, "includedFrameworks") != CLI_JSON_NONE))
    {
        cli_lock_give(&s_lock);
        return;
    }
    const unsigned int place_count = cli_dotnet_places(s_places);
    const unsigned int single = cli_json_member(&json, options, "framework");
    if (single != CLI_JSON_NONE)
    {
        cli_framework_told(&json, single, s_places, place_count, file, say);
    }
    const unsigned int many = cli_json_member(&json, options, "frameworks");
    for (unsigned int item = cli_json_is(&json, many, CFG_JSON_ARRAY) ? json.node[many].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json.node[item].next)
    {
        cli_framework_told(&json, item, s_places, place_count, file, say);
    }
    cli_lock_give(&s_lock);
}

// The tree's own code beside a spec, and what is said of the modules it imports that PyInstaller did not find
typedef struct
{
    const char *folder;
    QuerySink say;
} CliOwn;

// whether an importer is the tree's own: a script by its path under the spec's folder, or a module whose first part is
// a package or a Python file of that folder, or the script run
static int cli_is_own(const char *folder, const char *importer, unsigned long long length)
{
    const int script = (length > 3ull) &&
                       (memor.cmp(&(MemoriaCfg){.src = importer + length - 3ull, .other = ".py", .bytes = 3u}) == 0);
    const int absolute =
        (length > 0ull) && ((importer[0] == '/') || (importer[0] == '\\') || ((length > 1ull) && (importer[1] == ':')));
    if (script && absolute)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
        return (length >= size) && (memor.cmp(&(MemoriaCfg){.src = importer, .other = folder, .bytes = size}) == 0) &&
               ((length == size) || (importer[size] == '/') || (importer[size] == '\\'));
    }
    unsigned long long top = 0ull;
    while ((top < length) && (importer[top] != '.'))
    {
        top += 1ull;
    }
    if ((top == 8ull) && (memor.cmp(&(MemoriaCfg){.src = importer, .other = "__main__", .bytes = 8u}) == 0))
    {
        return 1;
    }
    char name[CLI_NAME_LONGEST];
    char path[CLI_PATH_LONGEST];
    char marked[CLI_PATH_LONGEST];
    cli_scan_copied(importer, top, name, sizeof(name));
    CliLine line = {name + top, sizeof(name) - top, 0ull};
    cli_line_text(&line, ".py");
    cli_line_closed(&line);
    if (cli_executables_joined(folder, name, path) && (query_is_file(path) || query_is_folder(path)))
    {
        return 1;
    }
    name[top] = '\0';
    return cli_executables_joined(folder, name, path) && query_is_folder(path) &&
           cli_executables_joined(path, "__init__.py", marked) && query_is_file(marked);
}

// a module PyInstaller did not find said where the first of its importers that is the tree's own, and imports it other
// than optionally, names it
static void cli_own_each(void *context, const char *module, unsigned long long module_length, const char *importers,
                         unsigned long long importers_length)
{
    const CliOwn *const own = (const CliOwn *)context;
    const char *one = importers;
    const char *const end = importers + importers_length;
    while (one < end)
    {
        // the importer up to the next `, `
        const char *const start = one;
        const char *stop = one;
        while ((stop < end) && !(((stop + 1) < end) && (stop[0] == ',') && (stop[1] == ' ')))
        {
            stop += 1;
        }
        one = (stop < end) ? (stop + 2) : end;
        // `name (kinds)`: the `)` at its end left off, then parted at its last ` (`
        const char *close = stop;
        while ((close > start) && (close[-1] == ')'))
        {
            close -= 1;
        }
        const char *split = NULL;
        for (const char *at = start; (at + 1) < close; at += 1)
        {
            split = ((at[0] == ' ') && (at[1] == '(')) ? at : split;
        }
        if (split == NULL)
        {
            continue;
        }
        const char *name = start;
        unsigned long long length = (unsigned long long)(split - start);
        cli_scan_trim(&name, &length);
        const char *const kinds = split + 2;
        const unsigned long long kinds_length = (unsigned long long)(close - kinds);
        int optional = 0;
        for (unsigned long long at = 0ull; !optional && ((at + 8ull) <= kinds_length); at += 1ull)
        {
            optional = memor.cmp(&(MemoriaCfg){.src = kinds + at, .other = "optional", .bytes = 8u}) == 0;
        }
        if (optional || !cli_is_own(own->folder, name, length))
        {
            continue;
        }
        // a script by its file's name, a module by its own
        const char *shown = name;
        const int script = (length > 3ull) &&
                           (memor.cmp(&(MemoriaCfg){.src = name + length - 3ull, .other = ".py", .bytes = 3u}) == 0);
        for (unsigned long long at = 0ull; script && (at < length); at += 1ull)
        {
            shown = ((name[at] == '/') || (name[at] == '\\')) ? (name + at + 1) : shown;
        }
        char said[CLI_PATH_LONGEST * 2u];
        CliLine line = {said, sizeof(said), 0ull};
        cli_line_counted(&line, module, module_length);
        cli_line_text(&line, ", which ");
        cli_line_counted(&line, shown, (unsigned long long)((name + length) - shown));
        cli_line_text(&line, " imports, is not in the frozen program: PyInstaller found it nowhere in the Python that "
                             "ran it; install it there and build again");
        cli_line_closed(&line);
        cli_say(own->say, said);
        return;
    }
}

void cli_executables_built_advice(const char *root, const char *id, const CliLines *lines, QuerySink say)
{
    static char s_warn[CLI_EXECUTABLES_BYTES];
    static CliLock s_lock = CLI_LOCK_INIT;
    const char *const colon = cli_text_last(id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}), ':');
    if ((!cellul.starts(&(CatenaFinitaCfg){.src = id, .other = "executable/", .cap = CLI_CELLS_POOL})) ||
        (colon == NULL) || (colon < (id + 11)))
    {
        return;
    }
    char script[CLI_PATH_LONGEST];
    CliLine scripted = {script, sizeof(script), 0ull};
    cli_line_counted(&scripted, id + 11, (unsigned long long)(colon - (id + 11)));
    cli_line_closed(&scripted);
    char path[CLI_PATH_LONGEST];
    cli_executables_joined(root, script, path);
    const char *const base =
        (cli_text_last(script, cellul.len(&(CatenaFinitaCfg){.src = script, .cap = CLI_CELLS_POOL}), '/') != NULL)
            ? (cli_text_last(script, cellul.len(&(CatenaFinitaCfg){.src = script, .cap = CLI_CELLS_POOL}), '/') + 1)
            : script;
    const unsigned int kind = cli_executables_kind_of(base);
    if (kind == CLI_KIND_DOTNET)
    {
        char file[CLI_PATH_LONGEST];
        char why[CLI_NAME_LONGEST];
        CliLine unnamed = {why, sizeof(why), 0ull};
        if (cli_dotnet_artifact(colon + 1, lines, file, &unnamed))
        {
            cli_dotnet_missing(file, say);
        }
    }
    else if (kind == CLI_KIND_PYINSTALLER)
    {
        cli_lock_take(&s_lock);
        CliOwn own = {"", say};
        char folder[CLI_PATH_LONGEST];
        cli_pyinstaller_warn(path, folder, s_warn, sizeof(s_warn));
        own.folder = folder;
        cli_pyinstaller_missing(s_warn, cli_own_each, &own);
        cli_lock_give(&s_lock);
    }
}
