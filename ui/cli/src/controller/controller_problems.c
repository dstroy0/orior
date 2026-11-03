// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_problems.c: the problems: each file's diagnostics, worst first, a file replaced whole as its report comes
// again. The servers and checkers report on threads of their own: a report is copied into one cell and pushed on a list
// any thread pushes to and the controller's thread alone takes whole, by a compare and swap and an exchange, so no
// thread waits on another. A press on a row closes a file's rows or opens the file at a diagnostic's place; a press on
// the check's mark starts or stops the tree's check
#include "controller_internal.h"
#include "controller_work.h"

#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../servers/servers.h"
#include "../text/text.h"

#include <stdlib.h>

// the severity that counts as an error
#define CLI_PROBLEMS_ERROR 1u

// One file's diagnostics in one cell: the next file on the list; its path from the tree's top and where its name
// starts; its diagnostics, each message its first line alone; how many are errors; and whether its rows are closed
typedef struct ControllerProblemFile
{
    struct ControllerProblemFile *next;
    const char *path;
    unsigned long long path_length;
    unsigned long long name;
    CliServerDiagnostic *item;
    unsigned int count;
    unsigned int errors;
    int closed;
} ControllerProblemFile;

// The problems: the reports pushed and not yet taken, a pointer in the word; the files in the order they last came, and
// the last; the check's progress as it was last reported and as the model read it; and how many errors stand
typedef struct
{
    _Alignas(64) embed_atomic_u64 pushed;
    _Alignas(64) embed_atomic_u64 check_done;
    embed_atomic_u64 check_total;
    ControllerProblemFile *first;
    ControllerProblemFile *last;
    unsigned long long done;
    unsigned long long total;
    unsigned int errors;
} ControllerProblems;

static ControllerProblems s_problems;

// The tree's check being started: the tree, after it in its cell
typedef struct
{
    const char *root;
} ControllerCheckAsk;

typedef struct
{
    ControllerProblemFile *file;
    const char *path;
    unsigned long long length;
} ControllerProblemsCtx;

// two diagnostics in the order shown: the worse first, then the earlier
static int problems_order(const void *one, const void *other)
{
    const CliServerDiagnostic *const first = (const CliServerDiagnostic *)one;
    const CliServerDiagnostic *const second = (const CliServerDiagnostic *)other;
    if (first->severity != second->severity)
    {
        return (first->severity < second->severity) ? -1 : 1;
    }
    if (first->from_line != second->from_line)
    {
        return (first->from_line < second->from_line) ? -1 : 1;
    }
    if (first->from_col != second->from_col)
    {
        return (first->from_col < second->from_col) ? -1 : 1;
    }
    return 0;
}

// from any thread: a file's report copied into one cell and pushed; the path's name found from its end
static void problems_reported(const CliServerDiagnosticsCfg *args)
{
    const unsigned long long path_length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN});
    unsigned long long bytes = path_length + 1ULL;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        bytes += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->item[at].message,
                            .at = 0ULL, .size = args->item[at].message_length, .set = "\n") +
                 1ULL;
    }
    ControllerProblemFile *const file = g_cli_cells->persistent_buf_alloc(
        sizeof(*file) + (args->count * sizeof(CliServerDiagnostic)) + bytes, &g_cli_controller_cells);
    if (file == NULL)
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = file, .val = 0, .bytes = sizeof(*file)});
    file->item = (CliServerDiagnostic *)(void *)(file + 1);
    char *text = (char *)(void *)(file->item + args->count);
    memor.cpy(&(MemoriaCfg){.dst = text, .src = args->path, .bytes = path_length + 1ULL});
    file->path = text;
    file->path_length = path_length;
    file->name = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = text, .at = path_length,
                            .set = "/\\");
    text += path_length + 1ULL;
    file->count = args->count;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const unsigned long long first_line =
            EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->item[at].message,
                       .at = 0ULL, .size = args->item[at].message_length, .set = "\n");
        file->item[at] = args->item[at];
        memor.cpy(&(MemoriaCfg){.dst = text, .src = args->item[at].message, .bytes = first_line});
        text[first_line] = '\0';
        file->item[at].message = text;
        file->item[at].message_length = first_line;
        text += first_line + 1ULL;
        file->errors += (args->item[at].severity == CLI_PROBLEMS_ERROR) ? 1u : 0u;
    }
    qsort(file->item, file->count, sizeof(*file->item), problems_order);
    // a pointer fits the 64-bit word the list's head stands in
    embed_u64 head = EMBED_ATOMIC_LOAD(&s_problems.pushed);
    do
    {
        file->next = (ControllerProblemFile *)(uintptr_t)head;
    } while (!EMBED_ATOMIC_CAS_64(&s_problems.pushed, &head, (embed_u64)(uintptr_t)file));
    cli_controller_wake();
}

// from any thread: how far the tree's check has gone
static void problems_check_moved(const CliServerCheckCfg *args)
{
    EMBED_ATOMIC_STORE(&s_problems.check_done, args->done);
    EMBED_ATOMIC_STORE(&s_problems.check_total, args->total);
    cli_controller_wake();
}

CliServersSink cli_model_problems_sink(const CliModelCtx *args)
{
    CliServersSink sink;
    memor.set(&(MemoriaCfg){.dst = &sink, .val = 0, .bytes = sizeof(sink)});
    sink.diagnostics = problems_reported;
    sink.check_moved = problems_check_moved;
    VerbaLinea tree = {sink.tree, sizeof(sink.tree), 0ULL};
    mmgr_line_put(&tree, args->path);
    (void)mmgr_line_finish(&tree);
    return sink;
}

void cli_model_problems_begin(const CliModelCtx *args)
{
    (void)args;
    memor.set(&(MemoriaCfg){.dst = &s_problems, .val = 0, .bytes = sizeof(s_problems)});
}

// the file whose path is `length` bytes of `path` taken off the files with its errors: it, or NULL where none stands
EMBED_INLINE ControllerProblemFile *problems_file_take(const ControllerProblemsCtx *args)
{
    ControllerProblemFile *before = NULL;
    for (ControllerProblemFile *file = s_problems.first; file != NULL; file = file->next)
    {
        if ((file->path_length == args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = file->path, .other = args->path, .bytes = args->length}) == 0))
        {
            if (before != NULL)
            {
                before->next = file->next;
            }
            else
            {
                s_problems.first = file->next;
            }
            s_problems.last = (s_problems.last == file) ? before : s_problems.last;
            s_problems.errors -= file->errors;
            return file;
        }
        before = file;
    }
    return NULL;
}

int cli_model_problems_step(const CliModelCtx *args)
{
    (void)args;
    int did = 0;
    // the list is pushed newest first: turned over, each file's reports take their places in the order they came
    ControllerProblemFile *pushed =
        (ControllerProblemFile *)(uintptr_t)EMBED_ATOMIC_EXCHANGE_64(&s_problems.pushed, 0ULL);
    ControllerProblemFile *oldest = NULL;
    while (pushed != NULL)
    {
        ControllerProblemFile *const next = pushed->next;
        pushed->next = oldest;
        oldest = pushed;
        pushed = next;
    }
    while (oldest != NULL)
    {
        ControllerProblemFile *const file = oldest;
        oldest = oldest->next;
        ControllerProblemFile *const had =
            EMBED_CALL(problems_file_take, ControllerProblemsCtx, .path = file->path, .length = file->path_length);
        g_cli_cells->persistent_buf_release(had, &g_cli_controller_cells);
        did = 1;
        if (file->count == 0u)
        {
            g_cli_cells->persistent_buf_release(file, &g_cli_controller_cells);
            continue;
        }
        file->next = NULL;
        if (s_problems.last != NULL)
        {
            s_problems.last->next = file;
        }
        else
        {
            s_problems.first = file;
        }
        s_problems.last = file;
        s_problems.errors += file->errors;
    }
    const unsigned long long done = EMBED_ATOMIC_LOAD(&s_problems.check_done);
    const unsigned long long total = EMBED_ATOMIC_LOAD(&s_problems.check_total);
    if ((done != s_problems.done) || (total != s_problems.total))
    {
        s_problems.done = done;
        s_problems.total = total;
        g_cli_model.checking = (total != 0ULL) && (done < total);
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS);
        did = 1;
    }
    if (did)
    {
        g_cli_model.problems_marked = s_problems.errors > 0u;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_PROBLEMS) | CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS);
    }
    return did;
}

void cli_model_problems_end(const CliModelCtx *args)
{
    (void)args;
    cli_servers_stop_check();
    (void)EMBED_CALL(cli_model_problems.step, CliModelCtx, .at = 0u);
    while (s_problems.first != NULL)
    {
        ControllerProblemFile *const file = s_problems.first;
        s_problems.first = file->next;
        g_cli_cells->persistent_buf_release(file, &g_cli_controller_cells);
    }
    s_problems.last = NULL;
}

void cli_model_problems_press(const CliModelCtx *args)
{
    unsigned int row = 0u;
    for (ControllerProblemFile *file = s_problems.first; file != NULL; file = file->next)
    {
        if (args->at == row)
        {
            file->closed = !file->closed;
            g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_PROBLEMS);
            return;
        }
        const unsigned int under = file->closed ? 0u : file->count;
        if (args->at < (row + 1u + under))
        {
            const CliServerDiagnostic *const item = &file->item[args->at - row - 1u];
            EMBED_CALL(cli_model.file_open, CliModelCtx, .path = file->path, .placed = 1, .line = item->from_line,
                       .col = item->from_col);
            return;
        }
        row += 1u + under;
    }
}

// what runs on the servers' lane: the tree's files laid flat and handed to its check
static void problems_check_run(void *context)
{
    const ControllerCheckAsk *const ask = (const ControllerCheckAsk *)context;
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = ask->root);
    if (files == NULL)
    {
        return;
    }
    const char **const paths = g_cli_cells->persistent_buf_alloc(
        ((unsigned long long)files->count + 1ULL) * sizeof(*paths), &g_cli_controller_cells);
    if (paths != NULL)
    {
        for (unsigned int at = 0u; at < files->count; at += 1u)
        {
            paths[at] = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at);
        }
        const CliServersSink sink = EMBED_CALL(cli_model_problems.sink, CliModelCtx, .path = ask->root);
        (void)EMBED_CALL(cli_servers_root_sink_count.check_tree, CliServersRootSinkCountCfg, .root = ask->root,
                         .files = paths, .count = files->count, .sink = &sink);
        g_cli_cells->persistent_buf_release(paths, &g_cli_controller_cells);
    }
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
}

// what runs on the servers' lane: the tree's check stopped
static void problems_stop_run(void *context)
{
    (void)context;
    cli_servers_stop_check();
}

static void problems_ask_release(void *context)
{
    g_cli_cells->persistent_buf_release(context, &g_cli_controller_cells);
}

void cli_model_problems_check(const CliModelCtx *args)
{
    (void)args;
    if (g_cli_model.checking)
    {
        EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_SERVERS,
                   .run = problems_stop_run, .done = problems_ask_release, .context = NULL);
        return;
    }
    if (g_cli_model.root == NULL)
    {
        return;
    }
    const unsigned long long size =
        cellul.len(&(CatenaFinitaCfg){.src = g_cli_model.root, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    ControllerCheckAsk *const ask = g_cli_cells->persistent_buf_alloc(sizeof(*ask) + size, &g_cli_controller_cells);
    if (ask == NULL)
    {
        return;
    }
    char *const root = (char *)(void *)(ask + 1);
    memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_model.root, .bytes = size});
    ask->root = root;
    EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_SERVERS, .run = problems_check_run,
               .done = problems_ask_release, .context = ask, .busy = 1);
}

void *cli_model_problems_alloc(const CliModelCtx *args)
{
    (void)args;
    const char *const shown = (g_cli_model.active >= 0) ? g_cli_model.tab[g_cli_model.active]->path : NULL;
    unsigned int files = 0u;
    unsigned int items = 0u;
    unsigned int rows = 0u;
    unsigned int spans = 0u;
    unsigned long long bytes = 0ULL;
    for (const ControllerProblemFile *file = s_problems.first; file != NULL; file = file->next)
    {
        files += 1u;
        items += file->count;
        rows += 1u;
        bytes += (2ULL * file->path_length) + 2ULL;
        const int displayed = (shown != NULL) &&
                              cellul.eq(&(CatenaFinitaCfg){.src = file->path, .other = shown, .cap = CLI_PATH_MAX_LEN});
        spans += displayed ? file->count : 0u;
        for (unsigned int at = 0u; !file->closed && (at < file->count); at += 1u)
        {
            rows += 1u;
            bytes += file->item[at].message_length + 1ULL;
        }
    }
    CliProblems *const problems = g_cli_cells->persistent_buf_alloc(sizeof(*problems) + (rows * sizeof(CliProblemRow)) +
                                                                        (spans * sizeof(CliProblemSpan)) + bytes,
                                                                    &g_cli_controller_cells);
    if (problems == NULL)
    {
        return NULL;
    }
    CliProblemRow *const row = (CliProblemRow *)(void *)(problems + 1);
    CliProblemSpan *const span = (CliProblemSpan *)(void *)(row + rows);
    char *text = (char *)(void *)(span + spans);
    *problems =
        (CliProblems){files, items, s_problems.errors, s_problems.done, s_problems.total, rows, row, spans, span};
    unsigned int row_at = 0u;
    unsigned int span_at = 0u;
    for (const ControllerProblemFile *file = s_problems.first; file != NULL; file = file->next)
    {
        CliProblemRow *const head = &row[row_at];
        memor.set(&(MemoriaCfg){.dst = head, .val = 0, .bytes = sizeof(*head)});
        head->is_file = 1;
        head->count = file->count;
        head->closed = file->closed;
        memor.cpy(&(MemoriaCfg){.dst = text, .src = file->path, .bytes = file->path_length + 1ULL});
        head->path = text;
        head->name = text + file->name;
        text += file->path_length + 1ULL;
        const unsigned long long folder = (file->name != 0ULL) ? (file->name - 1ULL) : 0ULL;
        memor.cpy(&(MemoriaCfg){.dst = text, .src = file->path, .bytes = folder});
        text[folder] = '\0';
        head->folder = text;
        text += file->path_length + 1ULL;
        row_at += 1u;
        const int displayed = (shown != NULL) &&
                              cellul.eq(&(CatenaFinitaCfg){.src = file->path, .other = shown, .cap = CLI_PATH_MAX_LEN});
        for (unsigned int at = 0u; at < file->count; at += 1u)
        {
            const CliServerDiagnostic *const item = &file->item[at];
            if (displayed)
            {
                span[span_at] =
                    (CliProblemSpan){item->severity, item->from_line, item->from_col, item->to_line, item->to_col};
                span_at += 1u;
            }
            if (file->closed)
            {
                continue;
            }
            CliProblemRow *const one = &row[row_at];
            memor.set(&(MemoriaCfg){.dst = one, .val = 0, .bytes = sizeof(*one)});
            memor.cpy(&(MemoriaCfg){.dst = text, .src = item->message, .bytes = item->message_length});
            text[item->message_length] = '\0';
            one->severity = item->severity;
            one->message = text;
            one->line = item->from_line;
            one->col = item->from_col;
            text += item->message_length + 1ULL;
            row_at += 1u;
        }
    }
    return problems;
}
