// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_problems.c: the problems: each file's diagnostics, worst first, a file replaced whole as its report comes
// again. The servers and checkers report on threads of their own: a report is copied into one cell and pushed on a list
// any thread pushes to and the window's thread alone takes whole, by a compare and swap and an exchange, so no thread
// waits on another. A press on a row closes a file's rows or names the file and the place a diagnostic stands at; the
// tree's check is started and stopped here
#include "servers.h"

#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../text/text.h"

#include <stdlib.h>

// the severity that counts as an error
#define CLI_PROBLEMS_ERROR 1u

// One file's diagnostics in one cell: the next file on the list; its path from the tree's top and where its name
// starts; its diagnostics, each message its first line alone; how many are errors; and whether its rows are closed
typedef struct ServersProblemFile
{
    struct ServersProblemFile *next;
    const char *path;
    unsigned long long path_length;
    unsigned long long name;
    CliServerDiagnostic *item;
    unsigned int count;
    unsigned int errors;
    int closed;
} ServersProblemFile;

// The problems: the reports pushed and not yet taken, a pointer in the word; the files in the order they last came, and
// the last; the check's progress as it was last reported and as it was last taken; how many errors stand; and what
// wakes the window's thread as a report comes
typedef struct
{
    _Alignas(64) embed_atomic_u64 pushed;
    _Alignas(64) embed_atomic_u64 check_done;
    embed_atomic_u64 check_total;
    ServersProblemFile *first;
    ServersProblemFile *last;
    unsigned long long done;
    unsigned long long total;
    unsigned int errors;
    void (*woken)(void);
} ServersProblems;

static ServersProblems s_problems;

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
    ServersProblemFile *const file = g_cli_cells->persistent_buf_alloc(
        sizeof(*file) + (args->count * sizeof(CliServerDiagnostic)) + bytes, &g_cli_servers_cells);
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
        file->next = (ServersProblemFile *)(uintptr_t)head;
    } while (!EMBED_ATOMIC_CAS_64(&s_problems.pushed, &head, (embed_u64)(uintptr_t)file));
    if (s_problems.woken != NULL)
    {
        s_problems.woken();
    }
}

// from any thread: how far the tree's check has gone
static void problems_check_moved(const CliServerCheckCfg *args)
{
    EMBED_ATOMIC_STORE(&s_problems.check_done, args->done);
    EMBED_ATOMIC_STORE(&s_problems.check_total, args->total);
    if (s_problems.woken != NULL)
    {
        s_problems.woken();
    }
}

CliServersSink cli_servers_problems_sink(const char *root)
{
    CliServersSink sink;
    memor.set(&(MemoriaCfg){.dst = &sink, .val = 0, .bytes = sizeof(sink)});
    sink.diagnostics = problems_reported;
    sink.check_moved = problems_check_moved;
    VerbaLinea tree = {sink.tree, sizeof(sink.tree), 0ULL};
    mmgr_line_put(&tree, (root != NULL) ? root : "");
    (void)mmgr_line_finish(&tree);
    return sink;
}

void cli_servers_problems_begin(void (*woken)(void))
{
    memor.set(&(MemoriaCfg){.dst = &s_problems, .val = 0, .bytes = sizeof(s_problems)});
    s_problems.woken = woken;
}

// the file whose path is `length` bytes of `path` taken off the files with its errors: it, or NULL where none stands
static ServersProblemFile *problems_file_take(const char *path, unsigned long long length)
{
    ServersProblemFile *before = NULL;
    for (ServersProblemFile *file = s_problems.first; file != NULL; file = file->next)
    {
        if ((file->path_length == length) &&
            (memor.cmp(&(MemoriaCfg){.src = file->path, .other = path, .bytes = length}) == 0))
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

int cli_servers_problems_step(void)
{
    int did = 0;
    // the list is pushed newest first: turned over, each file's reports take their places in the order they came
    ServersProblemFile *pushed = (ServersProblemFile *)(uintptr_t)EMBED_ATOMIC_EXCHANGE_64(&s_problems.pushed, 0ULL);
    ServersProblemFile *oldest = NULL;
    while (pushed != NULL)
    {
        ServersProblemFile *const next = pushed->next;
        pushed->next = oldest;
        oldest = pushed;
        pushed = next;
    }
    while (oldest != NULL)
    {
        ServersProblemFile *const file = oldest;
        oldest = oldest->next;
        g_cli_cells->persistent_buf_release(problems_file_take(file->path, file->path_length), &g_cli_servers_cells);
        did = 1;
        if (file->count == 0u)
        {
            g_cli_cells->persistent_buf_release(file, &g_cli_servers_cells);
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
        did = 1;
    }
    return did;
}

void cli_servers_problems_end(void)
{
    cli_servers_stop_check();
    (void)cli_servers_problems_step();
    while (s_problems.first != NULL)
    {
        ServersProblemFile *const file = s_problems.first;
        s_problems.first = file->next;
        g_cli_cells->persistent_buf_release(file, &g_cli_servers_cells);
    }
    s_problems.last = NULL;
}

CliProblemsCounts cli_servers_problems_counts(void)
{
    CliProblemsCounts counts = {0u, 0u, s_problems.errors, s_problems.done, s_problems.total, 0u};
    for (const ServersProblemFile *file = s_problems.first; file != NULL; file = file->next)
    {
        counts.files += 1u;
        counts.items += file->count;
        counts.rows += 1u + (file->closed ? 0u : file->count);
    }
    return counts;
}

int cli_servers_problems_checking(void)
{
    return (s_problems.total != 0ULL) && (s_problems.done < s_problems.total);
}

int cli_servers_problems_row(unsigned int at, CliProblemRow *row)
{
    unsigned int first = 0u;
    for (const ServersProblemFile *file = s_problems.first; file != NULL; file = file->next)
    {
        const unsigned int under = file->closed ? 0u : file->count;
        if (at == first)
        {
            *row = (CliProblemRow){1,
                                   file->path,
                                   file->path + file->name,
                                   (file->name != 0ULL) ? (file->name - 1ULL) : 0ULL,
                                   file->count,
                                   file->closed,
                                   0u,
                                   "",
                                   0ULL,
                                   0ULL};
            return 1;
        }
        if (at < (first + 1u + under))
        {
            const CliServerDiagnostic *const item = &file->item[at - first - 1u];
            *row = (CliProblemRow){
                0, file->path, "", 0ULL, 0u, 0, item->severity, item->message, item->from_line, item->from_col};
            return 1;
        }
        first += 1u + under;
    }
    return 0;
}

int cli_servers_problems_span(const char *path, unsigned int at, CliProblemSpan *span)
{
    for (const ServersProblemFile *file = s_problems.first; (path != NULL) && (file != NULL); file = file->next)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = file->path, .other = path, .cap = CLI_PATH_MAX_LEN}))
        {
            if (at >= file->count)
            {
                return 0;
            }
            const CliServerDiagnostic *const item = &file->item[at];
            *span = (CliProblemSpan){item->severity, item->from_line, item->from_col, item->to_line, item->to_col};
            return 1;
        }
    }
    return 0;
}

int cli_servers_problems_press(unsigned int at, CliProblemRow *row)
{
    unsigned int first = 0u;
    for (ServersProblemFile *file = s_problems.first; file != NULL; file = file->next)
    {
        if (at == first)
        {
            file->closed = !file->closed;
            return 0;
        }
        first += 1u + (file->closed ? 0u : file->count);
    }
    return cli_servers_problems_row(at, row) && !row->is_file;
}

void cli_servers_problems_check(const char *root)
{
    if (cli_servers_problems_checking())
    {
        cli_servers_stop_check();
        return;
    }
    const CliFiles *const files =
        (root != NULL) ? EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = root) : NULL;
    if (files == NULL)
    {
        return;
    }
    const char **const paths = g_cli_cells->persistent_buf_alloc(
        ((unsigned long long)files->count + 1ULL) * sizeof(*paths), &g_cli_servers_cells);
    if (paths != NULL)
    {
        for (unsigned int at = 0u; at < files->count; at += 1u)
        {
            paths[at] = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at);
        }
        const CliServersSink sink = cli_servers_problems_sink(root);
        (void)EMBED_CALL(cli_servers_root_sink_count.check_tree, CliServersRootSinkCountCfg, .root = root,
                         .files = paths, .count = files->count, .sink = &sink);
        g_cli_cells->persistent_buf_release(paths, &g_cli_servers_cells);
    }
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
}
