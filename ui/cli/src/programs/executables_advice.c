// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_advice.c: what a build that failed can be told of its cause, where its lines show one orior_cli knows,
// and what a run of the program a build made can be told of how it ended: on Windows, the codes the system ends a
// program with that cannot load its DLLs; and a frozen Python program's module that is not in it, as one PyInstaller
// found nowhere or as one the program imports by its name as it runs, which PyInstaller cannot see
#include "../text/text.h"
#include "executables_internal.h"

// a line said through `say`
static void cli_say(QuerySink say, const char *text)
{
    say.said(say.context, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// whether a line of the build holds `mark`
static int cli_has(const CliLines *lines, const char *mark)
{
    for (unsigned int at = 0u; at < lines->count; at += 1u)
    {
        if (cellul.find(&(CatenaFinitaCfg){
                .src = cli_lines_at(lines, at), .cap = CLI_CELLS_POOL, .other = mark, .other_cap = CLI_CELLS_POOL}) !=
            NULL)
        {
            return 1;
        }
    }
    return 0;
}

// the script of an executable's job id into `script`, which holds CLI_PATH_LONGEST, and its name: NULL where the id
// is no executable's job's
static const char *cli_id_parts(const char *id, char *script)
{
    const char *const colon = cli_text_last(id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}), ':');
    if ((!cellul.starts(&(CatenaFinitaCfg){.src = id, .other = "executable/", .cap = CLI_CELLS_POOL})) ||
        (colon == NULL) || (colon < (id + 11)))
    {
        return NULL;
    }
    CliLine line = {script, CLI_PATH_LONGEST, 0ull};
    cli_line_counted(&line, id + 11, (unsigned long long)(colon - (id + 11)));
    cli_line_closed(&line);
    return colon + 1;
}

void cli_executables_advice(const char *id, const CliLines *lines, QuerySink say)
{
    char script[CLI_PATH_LONGEST];
    if (cli_id_parts(id, script) == NULL)
    {
        return;
    }
    const char *const base =
        (cli_text_last(script, cellul.len(&(CatenaFinitaCfg){.src = script, .cap = CLI_CELLS_POOL}), '/') != NULL)
            ? (cli_text_last(script, cellul.len(&(CatenaFinitaCfg){.src = script, .cap = CLI_CELLS_POOL}), '/') + 1)
            : script;
    const unsigned int kind = cli_executables_kind_of(base);
    char said[CLI_PATH_LONGEST * 2u];
    if ((kind == CLI_KIND_DOTNET) && (cli_has(lines, "NU1100") || cli_has(lines, "NU1101")))
    {
        const char *const args[5] = {"nuget", "list", "source", "--format", "short"};
        CliRan *const ran = cli_ran_taken();
        char why[CLI_PATH_LONGEST];
        CliLine failed = {why, sizeof(why), 0ull};
        const int listed = cli_executables_output("dotnet", "dotnet", args, 5u, NULL, ran, &failed);
        const char *from = ran->out;
        unsigned long long length = ran->out_size;
        cli_scan_trim(&from, &length);
        if (listed && ((length == 0ull) || (cellul.find(&(CatenaFinitaCfg){.src = ran->out,
                                                                           .cap = CLI_CELLS_POOL,
                                                                           .other = "No sources found",
                                                                           .other_cap = CLI_CELLS_POOL}) != NULL)))
        {
            cli_say(say, "NuGet has no package source to fetch the build's packages from; `dotnet nuget add source "
                         "https://api.nuget.org/v3/index.json -n nuget.org` adds nuget.org");
        }
        cli_ran_given(ran);
    }
    if (kind == CLI_KIND_CARGO)
    {
        // the target Rust's standard library is missing for, as the line that says so names it
        const char *target = NULL;
        unsigned long long target_length = 0ull;
        for (unsigned int at = 0u; (target == NULL) && (at < lines->count); at += 1u)
        {
            const char *const line = cli_lines_at(lines, at);
            const char *const opened = cellul.find(
                &(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .other = "the `", .other_cap = CLI_CELLS_POOL});
            if ((opened == NULL) || (cellul.find(&(CatenaFinitaCfg){.src = line,
                                                                    .cap = CLI_CELLS_POOL,
                                                                    .other = "target may not be installed",
                                                                    .other_cap = CLI_CELLS_POOL}) == NULL))
            {
                continue;
            }
            target = opened + 5;
            const char *const closed =
                cellul.chr(&(CatenaFinitaCfg){.src = target, .cap = CLI_CELLS_POOL, .byte = '`'});
            target_length = (closed != NULL) ? (unsigned long long)(closed - target)
                                             : cellul.len(&(CatenaFinitaCfg){.src = target, .cap = CLI_CELLS_POOL});
        }
        CliLine line = {said, sizeof(said), 0ull};
        if (target != NULL)
        {
            cli_line_text(&line, "Rust's standard library for ");
            cli_line_counted(&line, target, target_length);
            cli_line_text(&line, " is not installed: `rustup target add ");
            cli_line_counted(&line, target, target_length);
            cli_line_text(&line, "` installs it");
            cli_line_closed(&line);
            cli_say(say, said);
        }
        else if ((cli_has(lines, "linker `") && (cli_has(lines, "not found") || cli_has(lines, "failed"))) ||
                 cli_has(lines, "linking with `"))
        {
            cli_say(say, "linking for the target chosen needs that system's linker: the musl targets link here with "
                         "rust-lld, and a Linux GNU or macOS target links with cargo-zigbuild where it is installed");
        }
    }
    if ((kind == CLI_KIND_PYINSTALLER) && cli_has(lines, "No module named PyInstaller"))
    {
        char python[CLI_PATH_LONGEST];
        char why[CLI_NAME_LONGEST];
        CliLine failed = {why, sizeof(why), 0ull};
        if (!cli_executables_program("python", "python", python, &failed))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = python,
                              .src = "python",
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = "python", .cap = CLI_CELLS_POOL}) + 1u});
        }
        CliLine line = {said, sizeof(said), 0ull};
        cli_line_text(&line, "PyInstaller is not in the Python that runs it, ");
        cli_line_text(&line, python);
        cli_line_text(&line, "; `\"");
        cli_line_text(&line, python);
        cli_line_text(&line, "\" -m pip install pyinstaller` installs it there");
        cli_line_closed(&line);
        cli_say(say, said);
    }
}

char *cli_pyinstaller_warn(const char *spec, char *folder)
{
    if (!cli_path_parent(spec, folder))
    {
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = ".",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
    }
    const char *const base =
        (cli_text_last(spec, cellul.len(&(CatenaFinitaCfg){.src = spec, .cap = CLI_CELLS_POOL}), '/') != NULL)
            ? (cli_text_last(spec, cellul.len(&(CatenaFinitaCfg){.src = spec, .cap = CLI_CELLS_POOL}), '/') + 1)
        : (cli_text_last(spec, cellul.len(&(CatenaFinitaCfg){.src = spec, .cap = CLI_CELLS_POOL}), '\\') != NULL)
            ? (cli_text_last(spec, cellul.len(&(CatenaFinitaCfg){.src = spec, .cap = CLI_CELLS_POOL}), '\\') + 1)
            : spec;
    const char *dot = NULL;
    for (const char *at = base + 1; (base[0] != '\0') && (at[0] != '\0'); at += 1)
    {
        dot = (at[0] == '.') ? at : dot;
    }
    char stem[CLI_NAME_LONGEST];
    CliLine stemmed = {stem, sizeof(stem), 0ull};
    cli_line_counted(&stemmed, base,
                     (dot != NULL) ? (unsigned long long)(dot - base)
                                   : cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}));
    cli_line_closed(&stemmed);
    char path[CLI_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, folder);
    cli_line_text(&line, "/build/");
    cli_line_text(&line, stem);
    cli_line_text(&line, "/warn-");
    cli_line_text(&line, stem);
    cli_line_text(&line, ".txt");
    return (cli_line_closed(&line) != 0ull) ? cli_text_taken(path) : NULL;
}

void cli_pyinstaller_missing(const char *text,
                             void (*each)(void *context, const char *module, unsigned long long module_length,
                                          const char *importers, unsigned long long importers_length),
                             void *context)
{
    static const char s_head[] = "missing module named ";
    static const char s_by[] = " - imported by ";
    for (const char *line = text; line[0] != '\0';)
    {
        const char *stop = line;
        while ((stop[0] != '\0') && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *const end = ((stop > line) && (stop[-1] == '\r')) ? (stop - 1) : stop;
        const unsigned long long length = (unsigned long long)(end - line);
        if ((length > (sizeof(s_head) - 1u)) &&
            (memor.cmp(&(MemoriaCfg){.src = line, .other = s_head, .bytes = sizeof(s_head) - 1u}) == 0))
        {
            const char *const rest = line + sizeof(s_head) - 1u;
            const char *by = rest;
            while (((by + sizeof(s_by) - 1u) <= end) &&
                   (memor.cmp(&(MemoriaCfg){.src = by, .other = s_by, .bytes = sizeof(s_by) - 1u}) != 0))
            {
                by += 1;
            }
            if ((by + sizeof(s_by) - 1u) <= end)
            {
                const char *module = rest;
                unsigned long long module_length = (unsigned long long)(by - rest);
                cli_scan_trim(&module, &module_length);
                while ((module_length > 0ull) && (module[0] == '\''))
                {
                    module += 1;
                    module_length -= 1ull;
                }
                while ((module_length > 0ull) && (module[module_length - 1ull] == '\''))
                {
                    module_length -= 1ull;
                }
                const char *const importers = by + sizeof(s_by) - 1u;
                each(context, module, module_length, importers, (unsigned long long)(end - importers));
            }
        }
        line = (stop[0] != '\0') ? (stop + 1) : stop;
    }
}

// A module looked for among those PyInstaller did not find: its name, or its first part, and whether it is one
typedef struct
{
    const char *module;
    unsigned long long length;
    const char *top;
    unsigned long long top_length;
    int found;
} CliUnfound;

static void cli_unfound_each(void *context, const char *module, unsigned long long module_length, const char *importers,
                             unsigned long long importers_length)
{
    (void)importers;
    (void)importers_length;
    CliUnfound *const unfound = (CliUnfound *)context;
    unfound->found =
        unfound->found ||
        ((module_length == unfound->length) &&
         (memor.cmp(&(MemoriaCfg){.src = module, .other = unfound->module, .bytes = module_length}) == 0)) ||
        ((module_length == unfound->top_length) &&
         (memor.cmp(&(MemoriaCfg){.src = module, .other = unfound->top, .bytes = module_length}) == 0));
}

// The fullest module name the Python files beside a spec write out in quotes that starts with `module`, as
// `importlib.import_module("email.mime.text")` does for `email.mime`
typedef struct
{
    const char *folder;
    const char *module;
    unsigned long long length;
    char best[CLI_NAME_LONGEST];
} CliFullest;

static void cli_fullest_each(void *context, const char *name, int is_folder)
{
    CliFullest *const fullest = (CliFullest *)context;
    const unsigned long long name_length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    char path[CLI_PATH_LONGEST];
    if (is_folder || (name_length < 3ull) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + name_length - 3ull, .other = ".py", .cap = CLI_CELLS_POOL})) ||
        !cli_executables_joined(fullest->folder, name, path))
    {
        return;
    }
    char *const text = cli_text_taken(path);
    if (text == NULL)
    {
        return;
    }
    static const char s_quotes[2] = {'"', '\''};
    for (unsigned int quote = 0u; quote < 2u; quote += 1u)
    {
        // the pieces between each opening quote and the one that closes it
        const char *at = cellul.chr(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL, .byte = s_quotes[quote]});
        while (at != NULL)
        {
            const char *const piece = at + 1;
            const char *const end =
                cellul.chr(&(CatenaFinitaCfg){.src = piece, .cap = CLI_CELLS_POOL, .byte = s_quotes[quote]});
            const unsigned long long length = (end != NULL)
                                                  ? (unsigned long long)(end - piece)
                                                  : cellul.len(&(CatenaFinitaCfg){.src = piece, .cap = CLI_CELLS_POOL});
            int named = 1;
            for (unsigned long long letter = 0ull; named && (letter < length); letter += 1ull)
            {
                const char one = piece[letter];
                named = ((one >= 'a') && (one <= 'z')) || ((one >= 'A') && (one <= 'Z')) ||
                        ((one >= '0') && (one <= '9')) || (one == '_') || (one == '.') || ((unsigned char)one >= 0x80u);
            }
            const int starts =
                (length >= fullest->length) &&
                (memor.cmp(&(MemoriaCfg){.src = piece, .other = fullest->module, .bytes = fullest->length}) == 0) &&
                ((length == fullest->length) || (piece[fullest->length] == '.'));
            if (named && starts &&
                (length > cellul.len(&(CatenaFinitaCfg){.src = fullest->best, .cap = CLI_CELLS_POOL})))
            {
                CliLine line = {fullest->best, sizeof(fullest->best), 0ull};
                cli_line_counted(&line, piece, length);
                cli_line_closed(&line);
            }
            at = (end != NULL)
                     ? cellul.chr(&(CatenaFinitaCfg){.src = end + 1, .cap = CLI_CELLS_POOL, .byte = s_quotes[quote]})
                     : NULL;
        }
    }
    cli_cell_given_by(&g_cli_executables_cells, text);
}

void cli_executables_ran_advice(const char *root, const char *id, int has_code, long long code, const CliLines *lines,
                                QuerySink say)
{
    static CliFullest s_fullest;
    static CliLock s_lock = CLI_LOCK_INIT;
    char said[CLI_PATH_LONGEST * 2u];
#if defined(_WIN32)
    // the exit code read as the unsigned number Windows writes it as
    const unsigned int status = has_code ? (unsigned int)code : 0u;
    const char *const reason =
        (status == 0xC0000135u)   ? "a DLL it loads was found nowhere (0xC0000135)"
        : (status == 0xC000007Bu) ? "a DLL it loads is built for another processor, or is damaged (0xC000007B)"
        : (status == 0xC0000139u) ? "a DLL it loads lacks a function it calls, another version than the one it was "
                                    "built with (0xC0000139)"
                                  : NULL;
    if (reason != NULL)
    {
        CliLine line = {said, sizeof(said), 0ull};
        cli_line_text(&line, "the program did not start: ");
        cli_line_text(&line, reason);
        cli_line_text(&line, "; the build's end says which DLLs it loads from where");
        cli_line_closed(&line);
        cli_say(say, said);
    }
#else
    (void)has_code;
    (void)code;
#endif
    char spec[CLI_PATH_LONGEST];
    const unsigned long long spec_length =
        (cli_id_parts(id, spec) != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = spec, .cap = CLI_CELLS_POOL}) : 0ull;
    if ((spec_length < 5ull) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = spec + spec_length - 5ull, .other = ".spec", .cap = CLI_CELLS_POOL})))
    {
        return;
    }
    char path[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    cli_executables_joined(root, spec, path);
    cli_lock_take(&s_lock);
    char *const warned = cli_pyinstaller_warn(path, folder);
    const char *const warn = (warned != NULL) ? warned : "";
    for (unsigned int at = 0u; at < lines->count; at += 1u)
    {
        const char *const rest = cellul.find(&(CatenaFinitaCfg){.src = cli_lines_at(lines, at),
                                                                .cap = CLI_CELLS_POOL,
                                                                .other = "No module named ",
                                                                .other_cap = CLI_CELLS_POOL});
        if (rest == NULL)
        {
            continue;
        }
        const char *module = rest + 16;
        unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = module, .cap = CLI_CELLS_POOL});
        cli_scan_trim(&module, &length);
        while ((length > 0ull) && (module[0] == '\''))
        {
            module += 1;
            length -= 1ull;
        }
        while ((length > 0ull) && (module[length - 1ull] == '\''))
        {
            length -= 1ull;
        }
        while ((length > 0ull) && (module[0] == '"'))
        {
            module += 1;
            length -= 1ull;
        }
        while ((length > 0ull) && (module[length - 1ull] == '"'))
        {
            length -= 1ull;
        }
        if (length == 0ull)
        {
            continue;
        }
        unsigned long long top = 0ull;
        while ((top < length) && (module[top] != '.'))
        {
            top += 1ull;
        }
        CliUnfound unfound = {module, length, module, top, 0};
        cli_pyinstaller_missing(warn, cli_unfound_each, &unfound);
        CliLine line = {said, sizeof(said), 0ull};
        if (unfound.found)
        {
            cli_line_counted(&line, module, length);
            cli_line_text(&line,
                          " is not in the frozen program: PyInstaller found it nowhere in the Python that ran it; "
                          "install it there and build again");
        }
        else
        {
            s_fullest.folder = folder;
            s_fullest.module = module;
            s_fullest.length = length;
            CliLine best = {s_fullest.best, sizeof(s_fullest.best), 0ull};
            cli_line_counted(&best, module, length);
            cli_line_closed(&best);
            query_folder_each(folder, cli_fullest_each, &s_fullest);
            cli_line_text(&line, s_fullest.best);
            cli_line_text(&line, " is a module the frozen program imports by its name as it runs, which PyInstaller "
                                 "cannot see: add '");
            cli_line_text(&line, s_fullest.best);
            cli_line_text(&line, "' to hiddenimports in ");
            cli_line_text(&line, spec);
        }
        cli_line_closed(&line);
        cli_say(say, said);
    }
    cli_cell_given_by(&g_cli_executables_cells, warned);
    cli_lock_give(&s_lock);
}
