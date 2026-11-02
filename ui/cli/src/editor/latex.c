// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex.c: a LaTeX file made into pages: the engine its text names, the programs found, the make run in orior's own
// folder for the file, and what the make left written as the preview reads it
#include "../command_line/cells.h"
#include "../text/digest.h"
#include "../text/text.h"
#include "archive.h"
#include "latex_internal.h"

CliLatexRoom *g_cli_latex;
CliLock g_cli_latex_lock = CLI_LOCK_INIT;

int cli_latex_lock(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_latex_lock);
    g_cli_latex = g_cli_cells->persistent_buf_alloc(sizeof(*g_cli_latex), &g_cli_editor_cells);
    if (g_cli_latex == NULL)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_latex_lock);
    }
    return g_cli_latex != NULL;
}

void cli_latex_release(void)
{
    g_cli_cells->persistent_buf_release(g_cli_latex, &g_cli_editor_cells);
    g_cli_latex = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_latex_lock);
}

typedef struct
{
    const char *path;
    unsigned long long *start;
    unsigned long long *size;
    const char *name;
    char *program;
    const char *program_read;
    const char *const *request;
    unsigned int count;
    const char *dir;
    int *ended;
    VerbaLinea *err_msg;
    const char *log;
    unsigned long long size_value;
    const char *engine;
    VerbaLinea *out;
    const char *job;
    const char *extension;
    char *path_out;
    const char *root;
    const char *text;
    unsigned long long length;
    const char *file;
    const char *folder;
} LatexCtx;
// the stem of the file at `path`, its name without its last extension, as its start and its size
EMBED_INLINE void latex_stem(const LatexCtx *args)
{
    unsigned long long *start = args->start;
    unsigned long long *size = args->size;
    unsigned long long from = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    const unsigned long long end = from;
#if defined(_WIN32)
    from = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path, .at = from,
                      .set = "/\\");
#else
    from = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path, .at = from,
                      .set = "/");
#endif
    unsigned long long dot = end;
    dot = (from + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path + from,
                             .at = dot - from, .set = "."));
    // a name with no dot, or whose only dot opens it, is its own stem
    *start = from;
    *size = ((dot > from + 1ull) ? dot - 1ull : end) - from;
}

void cli_latex_job_get(const CliLatexFileCtx *args)
{
    unsigned long long start = 0ull;
    unsigned long long size = 0ull;
    EMBED_CALL(latex_stem, LatexCtx, .path = args->file_read, .start = &start, .size = &size);
    int plain = (size > 0ull);
    for (unsigned long long at = start; plain && (at < start + size); at += 1ull)
    {
        const char letter = args->file_read[at];
        plain = ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
                ((letter >= '0') && (letter <= '9')) || (letter == '-') || (letter == '_');
    }
    if (!plain)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->job_out, .src = "preview", .bytes = 8u});
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->job_out, .src = args->file_read + start, .bytes = size});
    args->job_out[size] = '\0';
}

// the program `name` as orior_cli finds it into `program`, sized for CLI_PATH_MAX_LEN: in TeX's folder or Poppler's
// as File, Toolchains finds them, then on the PATH orior's runs get. 1, or 0 where it is in none
static int latex_program(const LatexCtx *args)
{
    const char *name = args->name;
    CliJson manifest_json;
    CliJson *const json = &manifest_json;
    unsigned int count = 0u;
    char (*const path)[CLI_PATH_MAX_LEN] = cli_toolchains_path_taken(&count, &g_cli_editor_cells);
    const char (*const folders)[CLI_PATH_MAX_LEN] = (const char (*)[CLI_PATH_MAX_LEN])path;
    const char *const names[1] = {name};
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                             .json_out = json, .nodes_more = 0u, .text_more = 0ull);
    for (unsigned int index = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = manifest, .index = index) != CLI_JSON_NONE;
         index += 1u)
    {
        const unsigned int tool = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = manifest, .index = index);
        const char *const id =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = tool, .name = "id"),
                       .length_pointer = NULL);
        if ((id == NULL) || ((!cellul.eq(&(CatenaFinitaCfg){.src = id, .other = "tex", .cap = CLI_CELLS_POOL})) &&
                             (!cellul.eq(&(CatenaFinitaCfg){.src = id, .other = "poppler", .cap = CLI_CELLS_POOL}))))
        {
            continue;
        }
        EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = json, .tool = tool, .path_callback = folders,
                   .count = count, .found = &g_cli_latex->found);
        if ((g_cli_latex->found.folder[0] != '\0') &&
            EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = g_cli_latex->found.folder,
                       .names = names, .count = 1u, .path = args->program))
        {
            EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = json);
            g_cli_cells->persistent_buf_release(path, &g_cli_editor_cells);
            return 1;
        }
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = json);
    int found = 0;
    for (unsigned int at = 0u; !found && (at < count); at += 1u)
    {
        found = EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = folders[at], .names = names,
                           .count = 1u, .path = args->program);
    }
    g_cli_cells->persistent_buf_release(path, &g_cli_editor_cells);
    return found;
}

// `program` run in `dir` with `args`, `count` of them, with the PATH orior's runs get and the room's TEXINPUTS, its
// output let go, 1 into `ended` where it ended well: 1, or 0 with why given where it did not start or ran past
// CLI_LATEX_PATIENCE and was stopped
EMBED_INLINE int latex_run(const LatexCtx *args)
{
    unsigned int count = args->count;
    int *ended = args->ended;
    VerbaLinea *err_msg = args->err_msg;
    const char *words[8];
    words[0] = args->program_read;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        words[at + 1u] = args->request[at];
    }
    words[count + 1u] = NULL;
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = g_cli_latex->path,
               .size = CLI_PATH_TEXT);
    const CliSetting settings[2] = {{"PATH", g_cli_latex->path}, {"TEXINPUTS", g_cli_latex->inputs}};
    CliRan *const ran = cli_ran_take();
    const int started = EMBED_CALL(cli_words_internal.ran_within, CliWordsCtx, .words = words, .folder = args->dir,
                                   .settings = settings, .count = 2u, .input = NULL, .length = 0ull,
                                   .milliseconds_u64 = CLI_LATEX_PATIENCE, .ran = ran, .err_msg = err_msg);
    const int stopped = started && ran->stopped;
    *ended = started && !stopped && (ran->code == 0ll);
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    if (stopped)
    {
        unsigned long long start = 0ull;
        unsigned long long size = 0ull;
        EMBED_CALL(latex_stem, LatexCtx, .path = args->program_read, .start = &start, .size = &size);
        mmgr_line_put_n(err_msg, args->program_read + start, size);
        mmgr_line_put(err_msg, " took longer than ");
        mmgr_line_decimal(err_msg, CLI_LATEX_PATIENCE / 1000ull, 1u);
        mmgr_line_put(err_msg, " s and was stopped");
        return 0;
    }
    return started;
}

// the last four lines of a log that are not blank, joined by "\n", written as the one error of a make that failed and
// named none, the file made its file, or that the engine made no pages and wrote no log
EMBED_INLINE void latex_tail_write(const LatexCtx *args)
{
    unsigned long long size = args->size_value;
    VerbaLinea *out = args->out;
    unsigned long long starts[4];
    unsigned long long sizes[4];
    unsigned long long count = 0ull;
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    while (EMBED_CALL(cli_latex_text_internal.tex_line, CliLatexTextCtx, .text = args->log, .length = size, .at = &at,
                      .start = &start, .size_pointer = &length))
    {
        unsigned long long letter = 0ull;
        while ((letter < length) &&
               EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = args->log[start + letter]))
        {
            letter += 1ull;
        }
        if (letter == length)
        {
            continue;
        }
        starts[count % 4ull] = start;
        sizes[count % 4ull] = length;
        count += 1ull;
    }
    mmgr_line_put(out, "{\"file\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"line\":null,\"text\":\"");
    if (count == 0ull)
    {
        EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)args->engine,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->engine, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, " made no pages and wrote no log");
    }
    for (unsigned long long line = (count > 4ull) ? count - 4ull : 0ull; line < count; line += 1ull)
    {
        if (line != ((count > 4ull) ? count - 4ull : 0ull))
        {
            mmgr_line_put(out, "\\n");
        }
        EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)args->log + starts[line % 4ull], .length = sizes[line % 4ull]);
    }
    mmgr_line_put(out, "\"}");
}

// the file `name` of the make's folder `dir` into `path`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void latex_file(const LatexCtx *args)
{
    char name[CLI_PATH_MAX_LEN];
    VerbaLinea line = {name, sizeof(name), 0ull};
    mmgr_line_put(&line, args->job);
    mmgr_line_put(&line, args->extension);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->dir, .relative = name, .joined = args->path_out);
}

// the make in `dir` under the room's lock, as cli_latex_make_in specifies
EMBED_INLINE int latex_init(const LatexCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    const char *text = args->text;
    unsigned long long length = args->length;
    const char *file = args->file;
    const char *folder = args->folder;
    const char *dir = args->dir;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned long long began = cli_clock_milliseconds();
    const char *const engine = EMBED_CALL(cli_latex.engine_get, CliLatexCfg, .text = text, .length = length);
    char tex[CLI_PATH_MAX_LEN];
    char cairo[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(latex_program, LatexCtx, .name = engine, .program = tex))
    {
        mmgr_line_put(err_msg, engine);
        mmgr_line_put(err_msg, " is not found: File, Toolchains opens TeX's install page or takes its folder");
        return 0;
    }
    if (!EMBED_CALL(latex_program, LatexCtx, .name = "pdftocairo", .program = cairo))
    {
        mmgr_line_put(err_msg,
                      "pdftocairo is not found: File, Toolchains opens Poppler's install page or takes its folder");
        return 0;
    }
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = dir))
    {
        mmgr_line_put(err_msg, dir);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    char job[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_latex_file_internal.job_get, CliLatexFileCtx, .file_read = file, .job_out = job);
    char made[CLI_PATH_MAX_LEN];
    char pdf[CLI_PATH_MAX_LEN];
    EMBED_CALL(latex_file, LatexCtx, .dir = dir, .job = job, .extension = ".tex", .path_out = made);
    EMBED_CALL(latex_file, LatexCtx, .dir = dir, .job = job, .extension = ".pdf", .path_out = pdf);
    if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = made, .bytes = text,
                    .size = length))
    {
        mmgr_line_put(err_msg, dir);
        mmgr_line_put(err_msg, ": it could not be written");
        return 0;
    }
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = pdf);
#if defined(_WIN32)
    const char *const between = ";";
#else
    const char *const between = ":";
#endif
    VerbaLinea inputs = {g_cli_latex->inputs, CLI_PATH_TEXT, 0ull};
    mmgr_line_put(&inputs, ".");
    mmgr_line_put(&inputs, between);
    mmgr_line_put(&inputs, folder);
    mmgr_line_put(&inputs, between);
    mmgr_line_finish(&inputs);
    if ((inputs.at >= CLI_PATH_TEXT) ||
        !EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "TEXINPUTS",
                    .value_out = g_cli_latex->inputs + inputs.at, .room = CLI_PATH_TEXT - inputs.at))
    {
        g_cli_latex->inputs[(inputs.at < CLI_PATH_TEXT) ? inputs.at : CLI_PATH_TEXT - 1ull] = '\0';
    }
    CliTexNames names;
    EMBED_CALL(cli_latex_names_root_internal.tex_names_init, CliLatexNamesRootCtx, .names_out = &names, .root = root,
               .dir = dir, .job = job, .path = path);
    // The text is named by its path from the make's folder, which no search on TEXINPUTS takes: the file saved in the
    // tree under the same name is never made in its place
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ull};
    mmgr_line_put(&own_line, "./");
    mmgr_line_put(&own_line, job);
    mmgr_line_put(&own_line, ".tex");
    mmgr_line_finish(&own_line);
    const char *const arguments[5] = {"-interaction=nonstopmode", "-halt-on-error", "-file-line-error", "-synctex=1",
                                      own};
    int ended = 0;
    if (!EMBED_CALL(latex_run, LatexCtx, .program_read = tex, .request = arguments, .count = 5u, .dir = dir,
                    .ended = &ended, .err_msg = err_msg))
    {
        return 0;
    }
    const unsigned long long from = out->at;
    mmgr_line_put(out, "{\"engine\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = engine,
               .length = cellul.len(&(CatenaFinitaCfg){.src = engine, .cap = CLI_CELLS_POOL}));
    if (!ended || !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = pdf))
    {
        mmgr_line_put(out, ",\"defs\":\"\",\"pages\":[],\"boxes\":[],\"errors\":[");
        char log_path[CLI_PATH_MAX_LEN];
        EMBED_CALL(latex_file, LatexCtx, .dir = dir, .job = job, .extension = ".log", .path_out = log_path);
        unsigned long long size = 0ull;
        const unsigned char *const log = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = log_path,
                                                    .size_pointer = &size, .owner = &g_cli_editor_cells);
        const char *const read = (log != NULL) ? (const char *)log : "";
        const unsigned long long read_size = size;
        if (EMBED_CALL(cli_latex_names_log_internal.tex_errors_write, CliLatexNamesLogCtx, .log = read,
                       .size = read_size, .names = &names, .out = out) == 0u)
        {
            EMBED_CALL(latex_tail_write, LatexCtx, .log = read, .size_value = read_size, .path = path, .engine = engine,
                       .out = out);
        }
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = log, .owner = &g_cli_editor_cells);
        mmgr_line_put(out, "],\"ms\":");
        mmgr_line_decimal(out, cli_clock_milliseconds() - began, 1u);
        mmgr_line_ch(out, '}');
        return 1;
    }
    char pdf_name[CLI_PATH_MAX_LEN];
    char svg_name[CLI_PATH_MAX_LEN];
    VerbaLinea pdf_line = {pdf_name, sizeof(pdf_name), 0ull};
    VerbaLinea svg_line = {svg_name, sizeof(svg_name), 0ull};
    mmgr_line_put(&pdf_line, job);
    mmgr_line_put(&pdf_line, ".pdf");
    mmgr_line_finish(&pdf_line);
    mmgr_line_put(&svg_line, job);
    mmgr_line_put(&svg_line, ".svg");
    mmgr_line_finish(&svg_line);
    const char *const drawing[3] = {"-svg", pdf_name, svg_name};
    if (!EMBED_CALL(latex_run, LatexCtx, .program_read = cairo, .request = drawing, .count = 3u, .dir = dir,
                    .ended = &ended, .err_msg = err_msg))
    {
        out->at = from;
        return 0;
    }
    if (!ended)
    {
        mmgr_line_put(err_msg, "pdftocairo could not draw the pages ");
        mmgr_line_put(err_msg, engine);
        mmgr_line_put(err_msg, " made");
        out->at = from;
        return 0;
    }
    char drawn[CLI_PATH_MAX_LEN];
    EMBED_CALL(latex_file, LatexCtx, .dir = dir, .job = job, .extension = ".svg", .path_out = drawn);
    unsigned long long svg_size = 0ull;
    const unsigned char *const svg = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = drawn,
                                                .size_pointer = &svg_size, .owner = &g_cli_editor_cells);
    const int whole = (svg != NULL) && EMBED_CALL(cli_json_tree_size.utf8_whole, CliJsonTreeSizeCfg,
                                                  .bytes = (const char *)svg, .size = svg_size);
    mmgr_line_ch(out, ',');
    const int fits = whole && EMBED_CALL(cli_latex_size_internal.tex_pages_write, CliLatexSizeCtx,
                                         .svg = (const char *)svg, .size = svg_size, .out = out);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = svg, .owner = &g_cli_editor_cells);
    if (!fits)
    {
        mmgr_line_put(err_msg, drawn);
        mmgr_line_put(err_msg, whole ? " is larger than orior_cli supports" : ": it could not be read");
        out->at = from;
        return 0;
    }
    mmgr_line_put(out, ",\"boxes\":");
    char sync_path[CLI_PATH_MAX_LEN];
    EMBED_CALL(latex_file, LatexCtx, .dir = dir, .job = job, .extension = ".synctex.gz", .path_out = sync_path);
    unsigned long long zipped_size = 0ull;
    const unsigned char *const zipped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = sync_path,
                                                   .size_pointer = &zipped_size, .owner = &g_cli_editor_cells);
    unsigned long long sync_size = 0ull;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unzipped = {why, sizeof(why), 0ull};
    if ((zipped != NULL) &&
        EMBED_CALL(cli_archive_bytes.gunzip, CliArchiveBytesCfg, .bytes = zipped, .size = zipped_size,
                   .out = g_cli_latex->sync, .room = CLI_LATEX_SYNC, .out_size = &sync_size, .err_msg = &unzipped))
    {
        EMBED_CALL(cli_latex_names_internal.tex_boxes_write, CliLatexNamesCtx, .sync = (const char *)g_cli_latex->sync,
                   .size = sync_size, .names = &names, .out = out);
    }
    else
    {
        mmgr_line_put(out, "[]");
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = zipped, .owner = &g_cli_editor_cells);
    mmgr_line_put(out, ",\"errors\":[],\"ms\":");
    mmgr_line_decimal(out, cli_clock_milliseconds() - began, 1u);
    mmgr_line_ch(out, '}');
    return 1;
}

int cli_latex_make_in(const CliLatexCfg *args)
{
    char file[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->root, .relative = args->path, .joined = file,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = file, .out = folder))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, " has no folder");
        return 0;
    }
    char digest[80];
    VerbaLinea hex = {digest, sizeof(digest), 0ull};
    EMBED_CALL(cli_digest_bytes.sha256_text, CliDigestBytesCfg, .bytes = file,
               .size = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), .text = &hex);
    digest[16] = '\0';
    char dir[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->out_dir, .relative = digest, .joined = dir);
    if (!cli_latex_lock())
    {
        mmgr_line_put(args->err_msg, "the make could not be read: the cells cannot meet it");
        return 0;
    }
    const int made = EMBED_CALL(latex_init, LatexCtx, .root = args->root, .path = args->path, .text = args->text,
                                .length = args->length, .file = file, .folder = folder, .dir = dir, .out = args->out,
                                .err_msg = args->err_msg);
    cli_latex_release();
    return made;
}

int cli_latex_make(const CliLatexCfg *args)
{
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        mmgr_line_put(args->err_msg, "orior's own folder is not known");
        return 0;
    }
    char out_dir[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "previews", .joined = out_dir);
    return EMBED_CALL(cli_latex.make_in, CliLatexCfg, .root = args->root, .path = args->path, .text = args->text,
                      .length = args->length, .out_dir = out_dir, .out = args->out, .err_msg = args->err_msg);
}
