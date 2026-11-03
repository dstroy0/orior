// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_advice.c: what a build that failed can be reported of its cause, where its lines show one orior_cli
// knows, and what a run of the program a build made can be reported of how it ended: on Windows, the codes the system
// ends a program with that cannot load its DLLs; and a frozen Python program's module that is not in it, as one
// PyInstaller found nowhere or as one the program imports by its name as it runs, which PyInstaller cannot see
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "executables_internal.h"

typedef struct
{
    QuerySink sink;
    const char *text;
    const CliLines *lines;
    const char *mark;
    const char *id;
    char *script;
} ExecutablesAdviceCtx;

// a line printed through `sink`
EMBED_INLINE void cli_print(const ExecutablesAdviceCtx *args)
{
    EMBED_CALL(args->sink.print, CliEachLineSizeCfg, .context = args->sink.context, .line = args->text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}));
}

// whether a line of the build contains `mark`
EMBED_INLINE int cli_has(const ExecutablesAdviceCtx *args)
{
    const CliLines *lines = args->lines;
    for (unsigned int at = 0u; at < lines->count; at += 1u)
    {
        if (cellul.find(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_runner_lines_index.lines_at, CliRunnerLinesIndexCfg,
                                                             .lines_read = lines, .index = at),
                                           .cap = CLI_CELLS_POOL,
                                           .other = args->mark,
                                           .other_cap = CLI_CELLS_POOL}) != NULL)
        {
            return 1;
        }
    }
    return 0;
}

// the script of an executable's job id into `script`, sized for CLI_PATH_MAX_LEN, and its name: NULL where the id
// is no executable's job's
EMBED_INLINE const char *cli_id_parts(const ExecutablesAdviceCtx *args)
{
    const char *const colon =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->id,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}), .byte = ':');
    if ((!cellul.starts(&(CatenaFinitaCfg){.src = args->id, .other = "executable/", .cap = CLI_CELLS_POOL})) ||
        (colon == NULL) || (colon < (args->id + 11)))
    {
        return NULL;
    }
    VerbaLinea line = {args->script, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put_n(&line, args->id + 11, (unsigned long long)(colon - (args->id + 11)));
    mmgr_line_finish(&line);
    return colon + 1;
}

void cli_executables_advice(const CliExecutablesRootCfg *args)
{
    const CliLines *lines = args->lines;
    char script[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_id_parts, ExecutablesAdviceCtx, .id = args->id, .script = script) == NULL)
    {
        return;
    }
    const char *const base =
        (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = script,
                    .length_value = cellul.len(&(CatenaFinitaCfg){.src = script, .cap = CLI_CELLS_POOL}),
                    .byte = '/') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = script,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = script, .cap = CLI_CELLS_POOL}),
                          .byte = '/') +
               1)
            : script;
    const unsigned int kind =
        EMBED_CALL(cli_executables_name_out_folder_internal.kind_get, CliExecutablesNameOutFolderCtx, .name = base);
    char print_buf[CLI_PATH_MAX_LEN * 2u];
    if ((kind == CLI_KIND_DOTNET) && (EMBED_CALL(cli_has, ExecutablesAdviceCtx, .lines = lines, .mark = "NU1100") ||
                                      EMBED_CALL(cli_has, ExecutablesAdviceCtx, .lines = lines, .mark = "NU1101")))
    {
        const char *const arguments[5] = {"nuget", "list", "source", "--format", "short"};
        CliRunResult *const run_result = cli_run_result_alloc();
        char why[CLI_PATH_MAX_LEN];
        VerbaLinea failed = {why, sizeof(why), 0ULL};
        const int list_entries =
            EMBED_CALL(cli_executables_name_internal.output, CliExecutablesNameCtx, .tool = "dotnet", .name = "dotnet",
                       .request = arguments, .count = 5u, .folder = NULL, .run_result = run_result, .err_msg = &failed);
        const char *from = run_result->out;
        unsigned long long length = run_result->out_size;
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &from,
                   .length_pointer = &length);
        if (list_entries &&
            ((length == 0ULL) || (cellul.find(&(CatenaFinitaCfg){.src = run_result->out,
                                                                 .cap = CLI_CELLS_POOL,
                                                                 .other = "No sources found",
                                                                 .other_cap = CLI_CELLS_POOL}) != NULL)))
        {
            EMBED_CALL(cli_print, ExecutablesAdviceCtx, .sink = args->sink,
                       .text =
                           "NuGet has no package source to fetch the build's packages from; `dotnet nuget add source "
                           "https://api.nuget.org/v3/index.json -n nuget.org` adds nuget.org");
        }
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    }
    if (kind == CLI_KIND_CARGO)
    {
        // the target Rust's standard library is missing for, as the line that reports so names it
        const char *target = NULL;
        unsigned long long target_length = 0ULL;
        for (unsigned int at = 0u; (target == NULL) && (at < lines->count); at += 1u)
        {
            const char *const line =
                EMBED_CALL(cli_runner_lines_index.lines_at, CliRunnerLinesIndexCfg, .lines_read = lines, .index = at);
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
        VerbaLinea line = {print_buf, sizeof(print_buf), 0ULL};
        if (target != NULL)
        {
            mmgr_line_put(&line, "Rust's standard library for ");
            mmgr_line_put_n(&line, target, target_length);
            mmgr_line_put(&line, " is not installed: `rustup target add ");
            mmgr_line_put_n(&line, target, target_length);
            mmgr_line_put(&line, "` installs it");
            mmgr_line_finish(&line);
            EMBED_CALL(cli_print, ExecutablesAdviceCtx, .sink = args->sink, .text = print_buf);
        }
        else if ((EMBED_CALL(cli_has, ExecutablesAdviceCtx, .lines = lines, .mark = "linker `") &&
                  (EMBED_CALL(cli_has, ExecutablesAdviceCtx, .lines = lines, .mark = "not found") ||
                   EMBED_CALL(cli_has, ExecutablesAdviceCtx, .lines = lines, .mark = "failed"))) ||
                 EMBED_CALL(cli_has, ExecutablesAdviceCtx, .lines = lines, .mark = "linking with `"))
        {
            EMBED_CALL(cli_print, ExecutablesAdviceCtx, .sink = args->sink,
                       .text =
                           "linking for the target chosen needs that system's linker: the musl targets link here with "
                           "rust-lld, and a Linux GNU or macOS target links with cargo-zigbuild where it is installed");
        }
    }
    if ((kind == CLI_KIND_PYINSTALLER) &&
        EMBED_CALL(cli_has, ExecutablesAdviceCtx, .lines = lines, .mark = "No module named PyInstaller"))
    {
        char python[CLI_PATH_MAX_LEN];
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea failed = {why, sizeof(why), 0ULL};
        if (!EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "python", .name = "python",
                        .path = python, .err_msg = &failed))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = python,
                              .src = "python",
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = "python", .cap = CLI_CELLS_POOL}) + 1u});
        }
        VerbaLinea line = {print_buf, sizeof(print_buf), 0ULL};
        mmgr_line_put(&line, "PyInstaller is not in the Python that runs it, ");
        mmgr_line_put(&line, python);
        mmgr_line_put(&line, "; `\"");
        mmgr_line_put(&line, python);
        mmgr_line_put(&line, "\" -m pip install pyinstaller` installs it there");
        mmgr_line_finish(&line);
        EMBED_CALL(cli_print, ExecutablesAdviceCtx, .sink = args->sink, .text = print_buf);
    }
}

char *cli_pyinstaller_warn(const CliExecutablesSpecCtx *args)
{
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->spec,
                    .out = args->folder_out))
    {
        memor.cpy(&(MemoriaCfg){.dst = args->folder_out,
                                .src = ".",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
    }
    const char *const base =
        (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->spec,
                    .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->spec, .cap = CLI_CELLS_POOL}),
                    .byte = '/') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->spec,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->spec, .cap = CLI_CELLS_POOL}),
                          .byte = '/') +
               1)
        : (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->spec,
                      .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->spec, .cap = CLI_CELLS_POOL}),
                      .byte = '\\') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->spec,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->spec, .cap = CLI_CELLS_POOL}),
                          .byte = '\\') +
               1)
            : args->spec;
    const char *dot = NULL;
    const unsigned long long dot_end =
        (base[0] != '\0')
            ? EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = base + 1,
                         .at = cellul.len(&(CatenaFinitaCfg){.src = base + 1, .cap = CLI_CELLS_POOL}), .set = ".")
            : 0ULL;
    dot = (dot_end != 0ULL) ? (base + dot_end) : NULL;
    char stem[CLI_NAME_MAX_LEN];
    VerbaLinea stemmed = {stem, sizeof(stem), 0ULL};
    mmgr_line_put_n(&stemmed, base,
                    (dot != NULL) ? (unsigned long long)(dot - base)
                                  : cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}));
    mmgr_line_finish(&stemmed);
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, args->folder_out);
    mmgr_line_put(&line, "/build/");
    mmgr_line_put(&line, stem);
    mmgr_line_put(&line, "/warn-");
    mmgr_line_put(&line, stem);
    mmgr_line_put(&line, ".txt");
    return (mmgr_line_finish(&line) != 0ULL)
               ? EMBED_CALL(cli_executables_walk_internal.text_alloc, CliExecutablesWalkCtx, .path = path)
               : NULL;
}

void cli_pyinstaller_missing(const CliExecutablesTextEachCtx *args)
{
    const char *text = args->text;
    static const char s_head[] = "missing module named ";
    static const char s_by[] = " - imported by ";
    for (const char *line = text; line[0] != '\0';)
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_CELLS_POOL}), .set = "\n");
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
                EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx,
                           .text_pointer = &module, .length_pointer = &module_length);
                {
                    const unsigned long long lead =
                        EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = module,
                                   .at = 0ULL, .size = module_length, .set = "'");
                    module += lead;
                    module_length -= lead;
                }
                module_length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = module,
                                           .at = module_length, .set = "'");
                const char *const importers = by + sizeof(s_by) - 1u;
                EMBED_CALL(args->each_callback, CliEachModuleImportersCfg, .context = args->context, .module = module,
                           .module_length = module_length, .importers = importers,
                           .importers_length = (unsigned long long)(end - importers));
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

static void cli_unfound_each(const CliEachModuleImportersCfg *args)
{
    CliUnfound *const unfound = (CliUnfound *)args->context;
    unfound->found =
        unfound->found ||
        ((args->module_length == unfound->length) &&
         (memor.cmp(&(MemoriaCfg){.src = args->module, .other = unfound->module, .bytes = args->module_length}) ==
          0)) ||
        ((args->module_length == unfound->top_length) &&
         (memor.cmp(&(MemoriaCfg){.src = args->module, .other = unfound->top, .bytes = args->module_length}) == 0));
}

// The fullest module name the Python files beside a spec write out in quotes that starts with `module`, as
// `importlib.import_module("email.mime.text")` does for `email.mime`
typedef struct
{
    const char *folder;
    const char *module;
    unsigned long long length;
    char best[CLI_NAME_MAX_LEN];
} CliFullest;

static void cli_fullest_each(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    CliFullest *const fullest = (CliFullest *)args->context;
    const unsigned long long name_length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    char path[CLI_PATH_MAX_LEN];
    if (args->is_folder || (name_length < 3ULL) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + name_length - 3ULL, .other = ".py", .cap = CLI_CELLS_POOL})) ||
        !EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx,
                    .folder = fullest->folder, .name = name, .out = path))
    {
        return;
    }
    char *const text = EMBED_CALL(cli_executables_walk_internal.text_alloc, CliExecutablesWalkCtx, .path = path);
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
            unsigned long long letter = 0ULL;
            while (named && (letter < length))
            {
                const char one = piece[letter];
                named = ((one >= 'a') && (one <= 'z')) || ((one >= 'A') && (one <= 'Z')) ||
                        ((one >= '0') && (one <= '9')) || (one == '_') || (one == '.') || ((unsigned char)one >= 0x80u);
                letter += 1ULL;
            }
            const int starts =
                (length >= fullest->length) &&
                (memor.cmp(&(MemoriaCfg){.src = piece, .other = fullest->module, .bytes = fullest->length}) == 0) &&
                ((length == fullest->length) || (piece[fullest->length] == '.'));
            if (named && starts &&
                (length > cellul.len(&(CatenaFinitaCfg){.src = fullest->best, .cap = CLI_CELLS_POOL})))
            {
                VerbaLinea line = {fullest->best, sizeof(fullest->best), 0ULL};
                mmgr_line_put_n(&line, piece, length);
                mmgr_line_finish(&line);
            }
            at = (end != NULL)
                     ? cellul.chr(&(CatenaFinitaCfg){.src = end + 1, .cap = CLI_CELLS_POOL, .byte = s_quotes[quote]})
                     : NULL;
        }
    }
    g_cli_cells->persistent_buf_release(text, &g_cli_executables_cells);
}

void cli_executables_run_result_advice(const CliExecutablesRootCfg *args)
{
    const CliLines *lines = args->lines;
    static CliFullest s_fullest;
    char print_buf[CLI_PATH_MAX_LEN * 2u];
#if defined(_WIN32)
    // the exit code read as the unsigned number Windows writes it as
    const unsigned int status = args->has_code ? (unsigned int)args->code : 0u;
    const char *const reason =
        (status == 0xC0000135u)   ? "a DLL it loads was found nowhere (0xC0000135)"
        : (status == 0xC000007Bu) ? "a DLL it loads is built for another processor, or is damaged (0xC000007B)"
        : (status == 0xC0000139u) ? "a DLL it loads lacks a function it calls, another version than the one it was "
                                    "built with (0xC0000139)"
                                  : NULL;
    if (reason != NULL)
    {
        VerbaLinea line = {print_buf, sizeof(print_buf), 0ULL};
        mmgr_line_put(&line, "the program did not start: ");
        mmgr_line_put(&line, reason);
        mmgr_line_put(&line, "; the build's end shows which DLLs it loads from where");
        mmgr_line_finish(&line);
        EMBED_CALL(cli_print, ExecutablesAdviceCtx, .sink = args->sink, .text = print_buf);
    }
#else
#endif
    char spec[CLI_PATH_MAX_LEN];
    const unsigned long long spec_length =
        (EMBED_CALL(cli_id_parts, ExecutablesAdviceCtx, .id = args->id, .script = spec) != NULL)
            ? cellul.len(&(CatenaFinitaCfg){.src = spec, .cap = CLI_CELLS_POOL})
            : 0ULL;
    if ((spec_length < 5ULL) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = spec + spec_length - 5ULL, .other = ".spec", .cap = CLI_CELLS_POOL})))
    {
        return;
    }
    char path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = args->root,
               .name = spec, .out = path);
    char *const warned = EMBED_CALL(cli_executables_spec_internal.pyinstaller_warn, CliExecutablesSpecCtx, .spec = path,
                                    .folder_out = folder);
    const char *const warn = (warned != NULL) ? warned : "";
    for (unsigned int at = 0u; at < lines->count; at += 1u)
    {
        const char *const rest =
            cellul.find(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_runner_lines_index.lines_at, CliRunnerLinesIndexCfg,
                                                             .lines_read = lines, .index = at),
                                           .cap = CLI_CELLS_POOL,
                                           .other = "No module named ",
                                           .other_cap = CLI_CELLS_POOL});
        if (rest == NULL)
        {
            continue;
        }
        const char *module = rest + 16;
        unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = module, .cap = CLI_CELLS_POOL});
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &module,
                   .length_pointer = &length);
        {
            const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                       .text = module, .at = 0ULL, .size = length, .set = "'");
            module += lead;
            length -= lead;
        }
        length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = module, .at = length,
                            .set = "'");
        {
            const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                       .text = module, .at = 0ULL, .size = length, .set = "\"");
            module += lead;
            length -= lead;
        }
        length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = module, .at = length,
                            .set = "\"");
        if (length == 0ULL)
        {
            continue;
        }
        unsigned long long top = 0ULL;
        top = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = module, .at = top,
                         .size = length, .set = ".");
        CliUnfound unfound = {module, length, module, top, 0};
        EMBED_CALL(cli_executables_text_each_internal.pyinstaller_missing, CliExecutablesTextEachCtx, .text = warn,
                   .each_callback = cli_unfound_each, .context = &unfound);
        VerbaLinea line = {print_buf, sizeof(print_buf), 0ULL};
        if (unfound.found)
        {
            mmgr_line_put_n(&line, module, length);
            mmgr_line_put(&line,
                          " is not in the frozen program: PyInstaller found it nowhere in the Python that ran it; "
                          "install it there and build again");
        }
        else
        {
            s_fullest.folder = folder;
            s_fullest.module = module;
            s_fullest.length = length;
            VerbaLinea best = {s_fullest.best, sizeof(s_fullest.best), 0ULL};
            mmgr_line_put_n(&best, module, length);
            mmgr_line_finish(&best);
            EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder,
                       .each = cli_fullest_each, .context_bytess = &s_fullest);
            mmgr_line_put(&line, s_fullest.best);
            mmgr_line_put(&line, " is a module the frozen program imports by its name as it runs, which PyInstaller "
                                 "cannot see: add '");
            mmgr_line_put(&line, s_fullest.best);
            mmgr_line_put(&line, "' to hiddenimports in ");
            mmgr_line_put(&line, spec);
        }
        mmgr_line_finish(&line);
        EMBED_CALL(cli_print, ExecutablesAdviceCtx, .sink = args->sink, .text = print_buf);
    }
    g_cli_cells->persistent_buf_release(warned, &g_cli_executables_cells);
}
