// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_find.c: the adapter a language is debugged by, the first toolchain found of those that have one for it, and a
// program built for debugging: by the CMake or Meson build that makes it, or by the `builds` of the toolchain that
// builds its language, into build/debug/
#include "../command_line/cells.h"
#include "../programs/executables.h"
#include "../toolchains/toolchains.h"
#include "debug_internal.h"

#include "../text/text.h"

// the most toolchains one search names, and the most lines of a failed build told
#define CLI_DEBUG_NAMED 64u
#define CLI_DEBUG_TAIL 12u

// The PATH's folders, a cell of a search's, a tool found, and the lock they and a search's toolchains are held by
static char (*s_folders)[CLI_PATH_LONGEST];
static CliFound s_found;
static CliLock s_lock = CLI_LOCK_INIT;

// a search's toolchains and the PATH's folders given back, and the lock
static void find_released(CliJson *manifest)
{
    cli_json_given(manifest);
    cli_cell_given(s_folders);
    s_folders = NULL;
    cli_lock_give(&s_lock);
}

// whether the array `array` of `json` holds the string `text`
static int find_holds(const CliJson *json, unsigned int array, const char *text)
{
    for (unsigned int at = 0u; cli_json_item(json, array, at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const one = cli_json_string(json, cli_json_item(json, array, at), NULL);
        if ((one != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = one, .other = text, .cap = CLI_CELLS_POOL})))
        {
            return 1;
        }
    }
    return 0;
}

// the names of the toolchains `named`, `count` of them, joined by " or " into `out`
static void find_names_joined(const CliJson *json, const unsigned int *named, unsigned int count, CliLine *out)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? " or " : "");
        cli_line_text(out, cli_debug_text(json, cli_json_member(json, named[at], "name"), ""));
    }
}

// the toolchains `named`, `count` of them, with the one of id `ahead` put first, the others in their order
static void find_ahead_first(const CliJson *json, unsigned int *named, unsigned int *words, unsigned int count,
                             const char *ahead)
{
    unsigned int first = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = cli_debug_text(json, cli_json_member(json, named[at], "id"), ""),
                                          .other = ahead,
                                          .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        const unsigned int tool = named[at];
        const unsigned int word = words[at];
        for (unsigned int moved = at; moved > first; moved -= 1u)
        {
            named[moved] = named[moved - 1u];
            words[moved] = words[moved - 1u];
        }
        named[first] = tool;
        words[first] = word;
        first += 1u;
    }
}

int cli_debug_adapter(const char *language, const char *ahead, CliJson *json, unsigned int *spec, char *tool,
                      char *tool_id, char *program, CliLine *said)
{
    unsigned int named[CLI_DEBUG_NAMED];
    unsigned int specs[CLI_DEBUG_NAMED];
    unsigned int count = 0u;
    cli_lock_take(&s_lock);
    CliJson manifest_json;
    const unsigned int manifest = cli_toolchains_manifest(&manifest_json, 0u, 0ull);
    const char *const system = cli_toolchains_system();
    for (unsigned int at = 0u;
         (cli_json_item(&manifest_json, manifest, at) != CLI_JSON_NONE) && (count < CLI_DEBUG_NAMED); at += 1u)
    {
        const unsigned int one = cli_json_item(&manifest_json, manifest, at);
        const unsigned int debugger = cli_json_member(&manifest_json, one, "debugger");
        const unsigned int systems = cli_json_member(&manifest_json, debugger, "systems");
        const int on_system = (cli_json_item(&manifest_json, systems, 0u) == CLI_JSON_NONE) ||
                              find_holds(&manifest_json, systems, system);
        if (cli_json_is(&manifest_json, debugger, CFG_JSON_OBJECT) &&
            find_holds(&manifest_json, cli_json_member(&manifest_json, debugger, "languages"), language) && on_system)
        {
            named[count] = one;
            specs[count] = debugger;
            count += 1u;
        }
    }
    if (count == 0u)
    {
        find_released(&manifest_json);
        cli_line_text(said, "orior has no debugger for ");
        cli_line_text(said, language);
        return 0;
    }
    find_ahead_first(&manifest_json, named, specs, count, ahead);
    unsigned int folders = 0u;
    s_folders = cli_toolchains_path_taken(&folders);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_toolchains_find(&manifest_json, named[at], (const char (*)[CLI_PATH_LONGEST])s_folders, folders, &s_found);
        const char *const names[1] = {
            cli_debug_text(&manifest_json, cli_json_member(&manifest_json, specs[at], "program"), "")};
        if ((s_found.folder[0] != '\0') && cli_toolchains_program_in(s_found.folder, names, 1u, program))
        {
            *spec = cli_json_copy_from(json, &manifest_json, specs[at]);
            memor.cpy(&(MemoriaCfg){
                .dst = tool,
                .src = cli_debug_text(&manifest_json, cli_json_member(&manifest_json, named[at], "name"), ""),
                .bytes =
                    cellul.len(&(CatenaFinitaCfg){
                        .src = cli_debug_text(&manifest_json, cli_json_member(&manifest_json, named[at], "name"), ""),
                        .cap = CLI_CELLS_POOL}) +
                    1u});
            memor.cpy(&(MemoriaCfg){
                .dst = tool_id,
                .src = cli_debug_text(&manifest_json, cli_json_member(&manifest_json, named[at], "id"), ""),
                .bytes =
                    cellul.len(&(CatenaFinitaCfg){
                        .src = cli_debug_text(&manifest_json, cli_json_member(&manifest_json, named[at], "id"), ""),
                        .cap = CLI_CELLS_POOL}) +
                    1u});
            find_released(&manifest_json);
            return 1;
        }
    }
    find_names_joined(&manifest_json, named, count, said);
    find_released(&manifest_json);
    cli_line_text(said, " debugs it, and is not found: File, Toolchains opens its install page or takes its folder");
    return 0;
}

// the last lines of what a build wrote that are not blank, `most` of them at most, joined by "\n" into `out`
static void find_tail(const char *text, unsigned long long size, unsigned int most, CliLine *out)
{
    unsigned long long starts[CLI_DEBUG_TAIL];
    unsigned long long ends[CLI_DEBUG_TAIL];
    unsigned int kept = 0u;
    for (unsigned long long start = 0ull; start < size;)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + start, .val = '\n', .bytes = size - start});
        const unsigned long long next = (newline != NULL) ? (unsigned long long)(newline - text) + 1ull : size;
        unsigned long long end = (newline != NULL) ? next - 1ull : size;
        end = ((end > start) && (text[end - 1ull] == '\r')) ? end - 1ull : end;
        int blank = 1;
        for (unsigned long long at = start; blank && (at < end); at += 1ull)
        {
            blank =
                (cellul.chr(&(CatenaFinitaCfg){.src = " \t\f\v\r", .cap = CLI_CELLS_POOL, .byte = text[at]}) != NULL);
        }
        if (!blank)
        {
            const unsigned int place = (kept < most) ? kept : most - 1u;
            for (unsigned int moved = 0u; (kept >= most) && (moved + 1u < most); moved += 1u)
            {
                starts[moved] = starts[moved + 1u];
                ends[moved] = ends[moved + 1u];
            }
            starts[place] = start;
            ends[place] = end;
            kept = (kept < most) ? kept + 1u : kept;
        }
        start = next;
    }
    for (unsigned int at = 0u; at < kept; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "\n" : "");
        cli_lossy(out, (const unsigned char *)text + starts[at], ends[at] - starts[at]);
    }
}

int cli_debug_build_program(const CliProgram *made, char *program, CliLine *said)
{
    char tool[CLI_PATH_LONGEST];
    if (!cli_executables_program(made->tool, made->tool, tool, said))
    {
        return 0;
    }
    char path[CLI_PATH_TEXT];
    cli_toolchains_run_path(path, sizeof(path));
    const CliSetting settings[1] = {{"PATH", path}};
    const int meson = (cellul.eq(&(CatenaFinitaCfg){.src = made->tool, .other = "meson", .cap = CLI_CELLS_POOL}));
    const char *const meson_words[6] = {tool, "compile", "-C", made->build, made->target, NULL};
    const char *const cmake_words[9] = {tool,       "--build",    made->build, "--target", made->target,
                                        "--config", made->config, NULL,        NULL};
    char why[CLI_SAID_LONGEST];
    CliLine unsaid = {why, sizeof(why), 0ull};
    CliRan *const ran = cli_ran_taken();
    if (!cli_ran(meson ? meson_words : cmake_words, NULL, settings, 1u, ran, &unsaid))
    {
        cli_ran_given(ran);
        cli_line_text(said, tool);
        cli_line_text(said, ": ");
        cli_line_counted(said, unsaid.out, unsaid.at);
        return 0;
    }
    if (ran->code != 0ll)
    {
        // what the build wrote to its output, then to its errors, as one text, a cell of their size
        char *const both = cli_cell_taken(ran->out_size + ran->err_size + 1ull, "a failed build's output");
        cli_line_text(said, "The build of ");
        cli_line_text(said, made->target);
        cli_line_text(said, " failed");
        if (both != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = both, .src = ran->out, .bytes = ran->out_size});
            memor.cpy(&(MemoriaCfg){.dst = both + ran->out_size, .src = ran->err, .bytes = ran->err_size});
            cli_line_text(said, ":\n");
            find_tail(both, ran->out_size + ran->err_size, CLI_DEBUG_TAIL, said);
        }
        cli_cell_given(both);
        cli_ran_given(ran);
        return 0;
    }
    cli_ran_given(ran);
    memor.cpy(&(MemoriaCfg){.dst = program,
                            .src = made->program,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = made->program, .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
}

// the extension of the file `path` names, lowercase, into `out`, which holds CLI_NAME_LONGEST, empty where it has
// none; and its name without its extension into `stem`, which holds CLI_PATH_LONGEST
static void find_name_parts(const char *path, char *out, char *stem)
{
    const char *name = path;
    for (const char *letter = path; *letter != '\0'; letter += 1)
    {
        name = ((*letter == '/') || (*letter == '\\')) ? letter + 1 : name;
    }
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    const int has = (dot != NULL) && (dot != name);
    CliLine extension = {out, CLI_NAME_LONGEST, 0ull};
    for (const char *letter = has ? dot + 1 : ""; *letter != '\0'; letter += 1)
    {
        const char one = *letter;
        cli_line_character(&extension, ((one >= 'A') && (one <= 'Z')) ? (char)(one - 'A' + 'a') : one);
    }
    cli_line_closed(&extension);
    CliLine stem_line = {stem, CLI_PATH_LONGEST, 0ull};
    cli_line_counted(&stem_line, name,
                     has ? (unsigned long long)(dot - name)
                         : cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    cli_line_closed(&stem_line);
}

// the words of the build of the toolchain `named` found, at `program`, filled in: the file's path, the program it
// makes and the toolchain's program, and each other "{name}" the toolchain's program of that name in its folder, put
// into `words`: 1, or 0 with why said
static int find_build_words(const CliJson *json, unsigned int named, unsigned int words_node, const char *path,
                            const char *out, const char *program, CliWords *words, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    if (!cli_path_parent(program, folder))
    {
        folder[0] = '\0';
    }
    for (unsigned int at = 0u; cli_json_item(json, words_node, at) != CLI_JSON_NONE; at += 1u)
    {
        unsigned long long length = 0ull;
        const char *const word = cli_json_string(json, cli_json_item(json, words_node, at), &length);
        const char *const text = (word != NULL) ? word : "";
        if (cellul.eq(&(CatenaFinitaCfg){.src = text, .other = "{path}", .cap = CLI_CELLS_POOL}))
        {
            cli_words_put(words, path);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = text, .other = "{out}", .cap = CLI_CELLS_POOL}))
        {
            cli_words_put(words, out);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = text, .other = "{program}", .cap = CLI_CELLS_POOL}))
        {
            cli_words_put(words, program);
        }
        else if ((length >= 2ull) && (text[0] == '{') && (text[length - 1ull] == '}'))
        {
            char name[CLI_NAME_LONGEST];
            char found[CLI_PATH_LONGEST];
            CliLine name_line = {name, sizeof(name), 0ull};
            cli_line_counted(&name_line, text + 1, length - 2ull);
            cli_line_closed(&name_line);
            const char *const names[1] = {name};
            if (!cli_toolchains_program_in(folder, names, 1u, found))
            {
                cli_line_text(said, cli_debug_text(json, cli_json_member(json, named, "name"), ""));
                cli_line_text(said, " has no ");
                cli_line_text(said, name);
                cli_line_text(said, " in ");
                cli_line_text(said, folder);
                return 0;
            }
            cli_words_put(words, found);
        }
        else
        {
            cli_words_put(words, text);
        }
    }
    return 1;
}

int cli_debug_build(const char *root, const char *path, const char *language, const char *ahead, char *program,
                    int *built, CliLine *said)
{
    char extension[CLI_NAME_LONGEST];
    char stem[CLI_PATH_LONGEST];
    char keys[2][CLI_NAME_LONGEST];
    find_name_parts(path, extension, stem);
    CliLine key = {keys[0], sizeof(keys[0]), 0ull};
    cli_line_text(&key, language);
    cli_line_character(&key, '.');
    cli_line_text(&key, extension);
    cli_line_closed(&key);
    memor.cpy(&(MemoriaCfg){.dst = keys[1],
                            .src = language,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = language, .cap = CLI_CELLS_POOL}) + 1u});
    unsigned int named[CLI_DEBUG_NAMED];
    unsigned int words_of[CLI_DEBUG_NAMED];
    unsigned int count = 0u;
    *built = 0;
    cli_lock_take(&s_lock);
    CliJson json;
    const unsigned int manifest = cli_toolchains_manifest(&json, 0u, 0ull);
    for (unsigned int one = 0u; one < 2u; one += 1u)
    {
        for (unsigned int at = 0u; (cli_json_item(&json, manifest, at) != CLI_JSON_NONE) && (count < CLI_DEBUG_NAMED);
             at += 1u)
        {
            const unsigned int tool = cli_json_item(&json, manifest, at);
            const unsigned int words = cli_json_member(&json, cli_json_member(&json, tool, "builds"), keys[one]);
            if (words != CLI_JSON_NONE)
            {
                named[count] = tool;
                words_of[count] = words;
                count += 1u;
            }
        }
    }
    if (count == 0u)
    {
        find_released(&json);
        return 1;
    }
    find_ahead_first(&json, named, words_of, count, ahead);
    unsigned int folders = 0u;
    s_folders = cli_toolchains_path_taken(&folders);
    unsigned int chosen = count;
    char compiler[CLI_PATH_LONGEST];
    for (unsigned int at = 0u; (at < count) && (chosen == count); at += 1u)
    {
        cli_toolchains_find(&json, named[at], (const char (*)[CLI_PATH_LONGEST])s_folders, folders, &s_found);
        if (s_found.has_program)
        {
            chosen = at;
            memor.cpy(&(MemoriaCfg){
                .dst = compiler,
                .src = s_found.program,
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_found.program, .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
    if (chosen == count)
    {
        find_names_joined(&json, named, count, said);
        find_released(&json);
        cli_line_text(said,
                      " builds it, and is not found: File, Toolchains opens its install page or takes its folder");
        return 0;
    }
    char out_folder[CLI_PATH_LONGEST];
    char build_folder[CLI_PATH_LONGEST];
    cli_full(root, "build", build_folder);
    cli_full(build_folder, "debug", out_folder);
    if (!query_folder_made(out_folder))
    {
        find_released(&json);
        cli_line_text(said, "build/debug: the folder could not be made");
        return 0;
    }
    char out[CLI_PATH_LONGEST];
#if defined(_WIN32)
    // the stem's own extension, where it has one, gives way to .exe, as a program's name is set on Windows
    char *const dot = cli_text_last(stem, cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_CELLS_POOL}), '.');
    if ((dot != NULL) && (dot != stem))
    {
        *dot = '\0';
    }
    CliLine exe = {stem + cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_CELLS_POOL}),
                   CLI_PATH_LONGEST - cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_CELLS_POOL}), 0ull};
    cli_line_text(&exe, ".exe");
    cli_line_closed(&exe);
#endif
    cli_full(out_folder, stem, out);
    static CliWords s_words;
    static CliLock s_words_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_words_lock);
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    const int filled = find_build_words(&json, named[chosen], words_of[chosen], path, out, compiler, &s_words, said);
    find_released(&json);
    if (filled && (s_words.count == 0u))
    {
        cli_line_text(said, "the build names no program");
    }
    if (!filled || (s_words.count == 0u))
    {
        cli_lock_give(&s_words_lock);
        return 0;
    }
    char run_path[CLI_PATH_TEXT];
    cli_toolchains_run_path(run_path, sizeof(run_path));
    const CliSetting settings[1] = {{"PATH", run_path}};
    char why[CLI_SAID_LONGEST];
    CliLine unsaid = {why, sizeof(why), 0ull};
    CliRan *const ran = cli_ran_taken();
    const int ran_well = cli_ran_within(s_words.word, root, settings, 1u, NULL, 0ull, CLI_DEBUG_BUILDING, ran, &unsaid);
    if (!ran_well)
    {
        cli_line_text(said, s_words.word[0]);
        cli_line_text(said, ": ");
        cli_line_counted(said, unsaid.out, unsaid.at);
    }
    cli_lock_give(&s_words_lock);
    if (ran_well && ran->stopped)
    {
        cli_line_text(said, "the build took longer than 300 s");
    }
    else if (ran_well && (ran->code != 0ll))
    {
        unsigned long long length = 0ull;
        cli_line_text(said, "the build for debugging failed:\n");
        // its errors, then its output, trimmed at both ends
        const unsigned long long start = said->at;
        cli_lossy(said, (const unsigned char *)ran->err, ran->err_size);
        cli_lossy(said, (const unsigned char *)ran->out, ran->out_size);
        while ((said->at > start) &&
               (cellul.chr(&(CatenaFinitaCfg){
                    .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = said->out[said->at - 1ull]}) != NULL))
        {
            said->at -= 1ull;
        }
        while ((start + length < said->at) &&
               (cellul.chr(&(CatenaFinitaCfg){
                    .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = said->out[start + length]}) != NULL))
        {
            length += 1ull;
        }
        memor.move_down(&(MemoriaCfg){
            .dst = said->out + start, .src = said->out + start + length, .bytes = said->at - start - length});
        said->at -= length;
    }
    const int done = ran_well && !ran->stopped && (ran->code == 0ll);
    cli_ran_given(ran);
    if (done)
    {
        memor.cpy(&(MemoriaCfg){.dst = program,
                                .src = out,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = out, .cap = CLI_CELLS_POOL}) + 1u});
        *built = 1;
    }
    return done;
}
