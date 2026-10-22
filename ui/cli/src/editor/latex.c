// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex.c: a LaTeX file made into pages: the engine its text names, the programs found, the make run in orior's own
// folder for the file, and what the make left written as the preview reads it
#include "../command_line/cells.h"
#include "../text/digest.h"
#include "archive.h"
#include "latex_internal.h"

CliLatexRoom *g_cli_latex;
CliLock g_cli_latex_lock = CLI_LOCK_INIT;

int cli_latex_held(void)
{
    cli_lock_take(&g_cli_latex_lock);
    g_cli_latex = cli_cell_taken(sizeof(*g_cli_latex), "a LaTeX make read");
    if (g_cli_latex == NULL)
    {
        cli_lock_give(&g_cli_latex_lock);
    }
    return g_cli_latex != NULL;
}

void cli_latex_released(void)
{
    cli_cell_given(g_cli_latex);
    g_cli_latex = NULL;
    cli_lock_give(&g_cli_latex_lock);
}

// the stem of the file at `path`, its name without its last extension, as its start and its size
static void latex_stem(const char *path, unsigned long long *start, unsigned long long *size)
{
    unsigned long long from = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    const unsigned long long end = from;
    while ((from > 0ull) && (path[from - 1ull] != '/')
#if defined(_WIN32)
           && (path[from - 1ull] != '\\')
#endif
    )
    {
        from -= 1ull;
    }
    unsigned long long dot = end;
    while ((dot > from) && (path[dot - 1ull] != '.'))
    {
        dot -= 1ull;
    }
    // a name with no dot, or whose only dot opens it, is its own stem
    *start = from;
    *size = ((dot > from + 1ull) ? dot - 1ull : end) - from;
}

void cli_latex_job_of(const char *file, char *job)
{
    unsigned long long start = 0ull;
    unsigned long long size = 0ull;
    latex_stem(file, &start, &size);
    int plain = (size > 0ull);
    for (unsigned long long at = start; plain && (at < start + size); at += 1ull)
    {
        const char letter = file[at];
        plain = ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
                ((letter >= '0') && (letter <= '9')) || (letter == '-') || (letter == '_');
    }
    if (!plain)
    {
        memor.cpy(&(MemoriaCfg){.dst = job, .src = "preview", .bytes = 8u});
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = job, .src = file + start, .bytes = size});
    job[size] = '\0';
}

// the program `name` as orior_cli finds it into `program`, which holds CLI_PATH_LONGEST: in TeX's folder or Poppler's as
// File, Toolchains finds them, then on the PATH orior's runs get. 1, or 0 where it is in none
static int latex_program(const char *name, char *program)
{
    CliJson manifest_json;
    CliJson *const json = &manifest_json;
    unsigned int count = 0u;
    char (*const path)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&count);
    const char (*const folders)[CLI_PATH_LONGEST] = (const char (*)[CLI_PATH_LONGEST])path;
    const char *const names[1] = {name};
    const unsigned int manifest = cli_toolchains_manifest(json, 0u, 0ull);
    for (unsigned int index = 0u; cli_json_item(json, manifest, index) != CLI_JSON_NONE; index += 1u)
    {
        const unsigned int tool = cli_json_item(json, manifest, index);
        const char *const id = cli_json_string(json, cli_json_member(json, tool, "id"), NULL);
        if ((id == NULL) || ((!cellul.eq(&(CatenaFinitaCfg){.src = id, .other = "tex", .cap = CLI_CELLS_POOL})) &&
                             (!cellul.eq(&(CatenaFinitaCfg){.src = id, .other = "poppler", .cap = CLI_CELLS_POOL}))))
        {
            continue;
        }
        cli_toolchains_find(json, tool, folders, count, &g_cli_latex->found);
        if ((g_cli_latex->found.folder[0] != '\0') &&
            cli_toolchains_program_in(g_cli_latex->found.folder, names, 1u, program))
        {
            cli_json_given(json);
            cli_cell_given(path);
            return 1;
        }
    }
    cli_json_given(json);
    int found = 0;
    for (unsigned int at = 0u; !found && (at < count); at += 1u)
    {
        found = cli_toolchains_program_in(folders[at], names, 1u, program);
    }
    cli_cell_given(path);
    return found;
}

// `program` run in `dir` with `args`, `count` of them, with the PATH orior's runs get and the room's TEXINPUTS, its
// output let go, 1 into `ended` where it ended well: 1, or 0 with why said where it did not start or ran past
// CLI_LATEX_PATIENCE and was stopped
static int latex_ran(const char *program, const char *const *args, unsigned int count, const char *dir, int *ended,
                     CliLine *said)
{
    const char *words[8];
    words[0] = program;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        words[at + 1u] = args[at];
    }
    words[count + 1u] = NULL;
    cli_toolchains_run_path(g_cli_latex->path, CLI_PATH_TEXT);
    const CliSetting settings[2] = {{"PATH", g_cli_latex->path}, {"TEXINPUTS", g_cli_latex->inputs}};
    CliRan *const ran = cli_ran_taken();
    const int started = cli_ran_within(words, dir, settings, 2u, NULL, 0ull, CLI_LATEX_PATIENCE, ran, said);
    const int stopped = started && ran->stopped;
    *ended = started && !stopped && (ran->code == 0ll);
    cli_ran_given(ran);
    if (stopped)
    {
        unsigned long long start = 0ull;
        unsigned long long size = 0ull;
        latex_stem(program, &start, &size);
        cli_line_counted(said, program + start, size);
        cli_line_text(said, " took longer than ");
        cli_line_decimal(said, CLI_LATEX_PATIENCE / 1000ull, 1u);
        cli_line_text(said, " s and was stopped");
        return 0;
    }
    return started;
}

// the last four lines of a log that are not blank, joined by "\n", written as the one error of a make that failed and
// named none, the file made its file, or that the engine made no pages and wrote no log
static void latex_tail_written(const char *log, unsigned long long size, const char *path, const char *engine,
                               CliLine *out)
{
    unsigned long long starts[4];
    unsigned long long sizes[4];
    unsigned long long kept = 0ull;
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    while (cli_tex_line(log, size, &at, &start, &length))
    {
        unsigned long long letter = 0ull;
        while ((letter < length) && cli_tex_white(log[start + letter]))
        {
            letter += 1ull;
        }
        if (letter == length)
        {
            continue;
        }
        starts[kept % 4ull] = start;
        sizes[kept % 4ull] = length;
        kept += 1ull;
    }
    cli_line_text(out, "{\"file\":");
    cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"line\":null,\"text\":\"");
    if (kept == 0ull)
    {
        cli_json_escaped_lossy(out, (const unsigned char *)engine,
                               cellul.len(&(CatenaFinitaCfg){.src = engine, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, " made no pages and wrote no log");
    }
    for (unsigned long long line = (kept > 4ull) ? kept - 4ull : 0ull; line < kept; line += 1ull)
    {
        if (line != ((kept > 4ull) ? kept - 4ull : 0ull))
        {
            cli_line_text(out, "\\n");
        }
        cli_json_escaped_lossy(out, (const unsigned char *)log + starts[line % 4ull], sizes[line % 4ull]);
    }
    cli_line_text(out, "\"}");
}

// the file `name` of the make's folder `dir` into `path`, which holds CLI_PATH_LONGEST
static void latex_file(const char *dir, const char *job, const char *extension, char *path)
{
    char name[CLI_PATH_LONGEST];
    CliLine line = {name, sizeof(name), 0ull};
    cli_line_text(&line, job);
    cli_line_text(&line, extension);
    cli_line_closed(&line);
    cli_full(dir, name, path);
}

// the make in `dir` under the room's lock, as cli_latex_make_in says
static int latex_made(const char *root, const char *path, const char *text, unsigned long long length, const char *file,
                      const char *folder, const char *dir, CliLine *out, CliLine *said)
{
    const unsigned long long began = cli_clock_milliseconds();
    const char *const engine = cli_latex_engine_of(text, length);
    char tex[CLI_PATH_LONGEST];
    char cairo[CLI_PATH_LONGEST];
    if (!latex_program(engine, tex))
    {
        cli_line_text(said, engine);
        cli_line_text(said, " is not found: File, Toolchains opens TeX's install page or takes its folder");
        return 0;
    }
    if (!latex_program("pdftocairo", cairo))
    {
        cli_line_text(said,
                      "pdftocairo is not found: File, Toolchains opens Poppler's install page or takes its folder");
        return 0;
    }
    if (!query_folder_made(dir))
    {
        cli_line_text(said, dir);
        cli_line_text(said, ": it could not be made");
        return 0;
    }
    char job[CLI_PATH_LONGEST];
    cli_latex_job_of(file, job);
    char made[CLI_PATH_LONGEST];
    char pdf[CLI_PATH_LONGEST];
    latex_file(dir, job, ".tex", made);
    latex_file(dir, job, ".pdf", pdf);
    if (!query_file_written(made, text, length))
    {
        cli_line_text(said, dir);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    (void)query_removed(pdf);
#if defined(_WIN32)
    const char *const between = ";";
#else
    const char *const between = ":";
#endif
    CliLine inputs = {g_cli_latex->inputs, CLI_PATH_TEXT, 0ull};
    cli_line_text(&inputs, ".");
    cli_line_text(&inputs, between);
    cli_line_text(&inputs, folder);
    cli_line_text(&inputs, between);
    cli_line_closed(&inputs);
    if ((inputs.at >= CLI_PATH_TEXT) ||
        !query_environment_read("TEXINPUTS", g_cli_latex->inputs + inputs.at, CLI_PATH_TEXT - inputs.at))
    {
        g_cli_latex->inputs[(inputs.at < CLI_PATH_TEXT) ? inputs.at : CLI_PATH_TEXT - 1ull] = '\0';
    }
    CliTexNames names;
    cli_tex_names_made(&names, root, dir, job, path);
    // The text is named by its path from the make's folder, which no search on TEXINPUTS takes: the file saved in the
    // tree under the same name is never made in its place
    char own[CLI_PATH_LONGEST];
    CliLine own_line = {own, sizeof(own), 0ull};
    cli_line_text(&own_line, "./");
    cli_line_text(&own_line, job);
    cli_line_text(&own_line, ".tex");
    cli_line_closed(&own_line);
    const char *const args[5] = {"-interaction=nonstopmode", "-halt-on-error", "-file-line-error", "-synctex=1", own};
    int ended = 0;
    if (!latex_ran(tex, args, 5u, dir, &ended, said))
    {
        return 0;
    }
    const unsigned long long from = out->at;
    cli_line_text(out, "{\"engine\":");
    cli_json_quoted(out, engine, cellul.len(&(CatenaFinitaCfg){.src = engine, .cap = CLI_CELLS_POOL}));
    if (!ended || !query_is_file(pdf))
    {
        cli_line_text(out, ",\"defs\":\"\",\"pages\":[],\"boxes\":[],\"errors\":[");
        char log_path[CLI_PATH_LONGEST];
        latex_file(dir, job, ".log", log_path);
        unsigned long long size = 0ull;
        const unsigned char *const log = cli_file_held(log_path, &size);
        const char *const read = (log != NULL) ? (const char *)log : "";
        const unsigned long long read_size = (log != NULL) ? size : 0ull;
        if (cli_tex_errors_written(read, read_size, &names, out) == 0u)
        {
            latex_tail_written(read, read_size, path, engine, out);
        }
        if (log != NULL)
        {
            cli_file_let_go(log, size);
        }
        cli_line_text(out, "],\"ms\":");
        cli_line_decimal(out, cli_clock_milliseconds() - began, 1u);
        cli_line_character(out, '}');
        return 1;
    }
    char pdf_name[CLI_PATH_LONGEST];
    char svg_name[CLI_PATH_LONGEST];
    CliLine pdf_line = {pdf_name, sizeof(pdf_name), 0ull};
    CliLine svg_line = {svg_name, sizeof(svg_name), 0ull};
    cli_line_text(&pdf_line, job);
    cli_line_text(&pdf_line, ".pdf");
    cli_line_closed(&pdf_line);
    cli_line_text(&svg_line, job);
    cli_line_text(&svg_line, ".svg");
    cli_line_closed(&svg_line);
    const char *const drawing[3] = {"-svg", pdf_name, svg_name};
    if (!latex_ran(cairo, drawing, 3u, dir, &ended, said))
    {
        out->at = from;
        return 0;
    }
    if (!ended)
    {
        cli_line_text(said, "pdftocairo could not draw the pages ");
        cli_line_text(said, engine);
        cli_line_text(said, " made");
        out->at = from;
        return 0;
    }
    char drawn[CLI_PATH_LONGEST];
    latex_file(dir, job, ".svg", drawn);
    unsigned long long svg_size = 0ull;
    const unsigned char *const svg = cli_file_held(drawn, &svg_size);
    const int whole = (svg != NULL) && cli_utf8_whole((const char *)svg, svg_size);
    cli_line_character(out, ',');
    const int fits = whole && cli_tex_pages_written((const char *)svg, svg_size, out);
    if (svg != NULL)
    {
        cli_file_let_go(svg, svg_size);
    }
    if (!fits)
    {
        cli_line_text(said, drawn);
        cli_line_text(said, whole ? " is larger than orior_cli holds" : ": it could not be read");
        out->at = from;
        return 0;
    }
    cli_line_text(out, ",\"boxes\":");
    char sync_path[CLI_PATH_LONGEST];
    latex_file(dir, job, ".synctex.gz", sync_path);
    unsigned long long zipped_size = 0ull;
    const unsigned char *const zipped = cli_file_held(sync_path, &zipped_size);
    unsigned long long sync_size = 0ull;
    char why[CLI_SAID_LONGEST];
    CliLine unzipped = {why, sizeof(why), 0ull};
    if ((zipped != NULL) && cli_gunzip(zipped, zipped_size, g_cli_latex->sync, CLI_LATEX_SYNC, &sync_size, &unzipped))
    {
        cli_tex_boxes_written((const char *)g_cli_latex->sync, sync_size, &names, out);
    }
    else
    {
        cli_line_text(out, "[]");
    }
    if (zipped != NULL)
    {
        cli_file_let_go(zipped, zipped_size);
    }
    cli_line_text(out, ",\"errors\":[],\"ms\":");
    cli_line_decimal(out, cli_clock_milliseconds() - began, 1u);
    cli_line_character(out, '}');
    return 1;
}

int cli_latex_make_in(const char *root, const char *path, const char *text, unsigned long long length, const char *kept,
                      CliLine *out, CliLine *said)
{
    char file[CLI_PATH_LONGEST];
    if (!cli_inside(root, path, file, said))
    {
        return 0;
    }
    char folder[CLI_PATH_LONGEST];
    if (!cli_path_parent(file, folder))
    {
        cli_line_text(said, path);
        cli_line_text(said, " has no folder");
        return 0;
    }
    char digest[80];
    CliLine hex = {digest, sizeof(digest), 0ull};
    cli_sha256_text(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), &hex);
    digest[16] = '\0';
    char dir[CLI_PATH_LONGEST];
    cli_full(kept, digest, dir);
    if (!cli_latex_held())
    {
        cli_line_text(said, "the make could not be read: the cells cannot meet it");
        return 0;
    }
    const int made = latex_made(root, path, text, length, file, folder, dir, out, said);
    cli_latex_released();
    return made;
}

int cli_latex_make(const char *root, const char *path, const char *text, unsigned long long length, CliLine *out,
                   CliLine *said)
{
    char home[CLI_PATH_LONGEST];
    if (!cli_home_folder(home))
    {
        cli_line_text(said, "orior's own folder is not known");
        return 0;
    }
    char kept[CLI_PATH_LONGEST];
    cli_full(home, "previews", kept);
    return cli_latex_make_in(root, path, text, length, kept, out, said);
}
