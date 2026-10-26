// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run.c: a query run (qry_member.h): its folder, its .qry, its writer's process, whether it was joined, and the files
// it read and may leave. Every process a loop starts reaches the run by QRY. A run's .qry contains everything the run
// did and every file it ran against, as it read each one and as it left each one it changed. As a run ends only the
// newest QRY_RETAINED runs of its folder are retained, 64 where it is not set
#include "../query_internal.h"

// the folder the course's runs are stored in (course.c)
#define QUERY_COURSE_FOLDER "build/engine/course/qry"

// the folder `path` stands in written into `folder`, the top of the tree where it names none
static void query_parent_of(const char *path, const char *root, char *folder)
{
    const char *slash = NULL;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        slash = ((*at == '/') || (*at == '\\')) ? at : slash;
    }
    VerbaLinea line = {folder, QUERY_PATH_MAX_LEN, 0ull};
    if (slash != NULL)
    {
        mmgr_line_put_n(&line, path, (unsigned long long)(slash - path));
    }
    else
    {
        mmgr_line_put(&line, root);
    }
    mmgr_line_finish(&line);
}

// the cells of the files a run read and may leave given back, as the run ends and before a run begins again in its
// room
static void query_run_paths_given(QueryRun *run)
{
    query_paths_given(&run->read);
    query_paths_given(&run->left);
}

// the run's writer named: its program built where stale, its .qry and the top of the tree
static int query_run_writer(QueryRun *run, const char *root, const char *qry, const QueryPrograms *programs,
                            VerbaLinea *err_msg)
{
    memor.set(&(MemoriaCfg){.dst = run, .val = 0, .bytes = sizeof(*run)});
    if (!query_tool_built(root, programs, "orior_qry_writer", run->writer.binary, sizeof(run->writer.binary), err_msg))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = run->writer.qry,
                            .src = qry,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = qry, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = run->writer.top,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    run->writer.path = programs->run_path(programs->context);
    return 1;
}

int query_run_joining(QueryRun *run, const char *root, const char *qry, const QueryPrograms *programs,
                      VerbaLinea *err_msg)
{
    query_run_paths_given(run);
    if (!query_run_writer(run, root, qry, programs, err_msg))
    {
        return 0;
    }
    if (!query_writer_runs(&run->writer) || !query_writer_joined(&run->writer))
    {
        mmgr_line_put(err_msg, "no writer runs ");
        mmgr_line_put(err_msg, qry);
        return 0;
    }
    query_parent_of(qry, root, run->folder);
    run->joined = 1;
    return 1;
}

int query_run_started(QueryRun *run, const char *root, const char *folder, const QueryPrograms *programs,
                      QueryContext *context, VerbaLinea *err_msg)
{
    (void)context;
    query_run_paths_given(run);
    char qry_path[QUERY_PATH_MAX_LEN];
    if (query_environment_read("QRY", qry_path, sizeof(qry_path)))
    {
        QueryRun joined;
        if (query_run_writer(&joined, root, qry_path, programs, err_msg) && query_writer_runs(&joined.writer) &&
            query_writer_joined(&joined.writer))
        {
            *run = joined;
            query_parent_of(qry_path, root, run->folder);
            run->joined = 1;
            return 1;
        }
        err_msg->at = 0ull;
    }
    if (!query_folder_made(folder))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, " is no folder");
        return 0;
    }
    char name[128];
    query_next_name(folder, name, sizeof(name));
    char qry[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {qry, sizeof(qry), 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    char full[QUERY_PATH_MAX_LEN];
    if ((mmgr_line_finish(&line) == 0ull) || !query_path_full(qry, full, sizeof(full)) ||
        !query_run_writer(run, root, full, programs, err_msg))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = run->folder,
                            .src = folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
    if (!query_writer_start(&run->writer, &run->process, err_msg))
    {
        return 0;
    }
    if (!query_writer_made(&run->writer))
    {
        mmgr_line_put(err_msg, "the run's writer did not start");
        return 0;
    }
    if (!query_writer_joined(&run->writer))
    {
        query_writer_stop(&run->writer, err_msg);
        mmgr_line_put(err_msg, "the run's buffer could not be opened");
        return 0;
    }
    return 1;
}

// the base name of `path`
static const char *query_base(const char *path)
{
    const char *base = path;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        base = ((*at == '/') || (*at == '\\')) ? (at + 1) : base;
    }
    return base;
}

// A run carried from: its path and when it last changed
typedef struct
{
    const char *path;
    unsigned long long changed;
} QueryCarriedRun;

// the runs of `folder`'s and of `course`, where `course` is given, put among those carried from, newest first by when
// each last changed, in a cell of one place for each: their count, the cell into `carried`, NULL where none are or
// the cell could not be taken
static unsigned int query_carried_runs(const QueryRuns *folder, const QueryRuns *course, QueryCarriedRun **carried)
{
    const unsigned int count = folder->count + course->count;
    *carried = (count > 0u) ? g_cli_cells->persistent_buf_alloc((unsigned long long)count * sizeof(QueryCarriedRun),
                                                                &g_query_cells)
                            : NULL;
    if (*carried == NULL)
    {
        return 0u;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const path = (at < folder->count) ? folder->path[at] : course->path[at - folder->count];
        (*carried)[at] = (QueryCarriedRun){path, 0ull};
        query_modified(path, &(*carried)[at].changed);
    }
    // newest first: a pass run alone after a course carries what the course left
    for (unsigned int at = 1u; at < count; at += 1u)
    {
        for (unsigned int back = at; (back > 0u) && ((*carried)[back - 1u].changed < (*carried)[back].changed);
             back -= 1u)
        {
            const QueryCarriedRun swap = (*carried)[back];
            (*carried)[back] = (*carried)[back - 1u];
            (*carried)[back - 1u] = swap;
        }
    }
    return count;
}

int query_run_carried(QueryRun *run, const char *folder, const char *const *files, unsigned int count,
                      const char *crystal, VerbaLinea *err_msg)
{
    char course[QUERY_PATH_MAX_LEN];
    VerbaLinea course_line = {course, sizeof(course), 0ull};
    mmgr_line_put(&course_line, run->writer.top);
    mmgr_line_ch(&course_line, '/');
    mmgr_line_put(&course_line, QUERY_COURSE_FOLDER);
    mmgr_line_finish(&course_line);
    char full_folder[QUERY_PATH_MAX_LEN];
    char full_course[QUERY_PATH_MAX_LEN];
    const int course_apart =
        !query_path_full(folder, full_folder, sizeof(full_folder)) ||
        !query_path_full(course, full_course, sizeof(full_course)) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = full_folder, .other = full_course, .cap = CLI_CELLS_POOL}));
    QueryRuns folder_runs;
    QueryRuns course_runs = {NULL, NULL, 0u};
    (void)query_runs_taken(folder, &folder_runs);
    if (course_apart)
    {
        (void)query_runs_taken(course, &course_runs);
    }
    QueryCarriedRun *carried_from = NULL;
    const unsigned int runs = query_carried_runs(&folder_runs, &course_runs, &carried_from);
    static const char *s_missing[QUERY_LEAVES_MOST];
    for (unsigned int at = 0u; at < runs; at += 1u)
    {
        unsigned int missing = 0u;
        for (unsigned int file = 0u; (file < count) && (missing < QUERY_LEAVES_MOST); file += 1u)
        {
            const unsigned char *bytes = NULL;
            unsigned long long size = 0ull;
            if (!query_writer_latest(&run->writer, query_base(files[file]), &bytes, &size))
            {
                s_missing[missing] = query_base(files[file]);
                missing += 1u;
            }
        }
        if (missing == 0u)
        {
            break;
        }
        char named[QUERY_PATH_MAX_LEN];
        char full_named[QUERY_PATH_MAX_LEN];
        char full_qry[QUERY_PATH_MAX_LEN];
        if (!query_opened(carried_from[at].path, crystal, named, sizeof(named)) ||
            (query_path_full(named, full_named, sizeof(full_named)) &&
             query_path_full(run->writer.qry, full_qry, sizeof(full_qry)) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = full_named, .other = full_qry, .cap = CLI_CELLS_POOL}))))
        {
            continue;
        }
        char carried[QUERY_LINE_MAX_LEN];
        VerbaLinea carried_line = {carried, sizeof(carried), 0ull};
        query_writer_carry(&run->writer, named, s_missing, missing, &carried_line);
    }
    g_cli_cells->persistent_buf_release(carried_from, &g_query_cells);
    query_runs_given(&course_runs);
    query_runs_given(&folder_runs);
    for (unsigned int file = 0u; file < count; file += 1u)
    {
        const unsigned char *bytes = NULL;
        unsigned long long size = 0ull;
        if (query_writer_latest(&run->writer, query_base(files[file]), &bytes, &size) || !query_is_file(files[file]))
        {
            continue;
        }
        const unsigned char *const mapped = qry_member_file_mapped(files[file], &size);
        if (mapped == NULL)
        {
            mmgr_line_put(err_msg, files[file]);
            mmgr_line_put(err_msg, ": it could not be read");
            return 0;
        }
        const int appended =
            query_writer_append(&run->writer, query_base(files[file]), mapped, size, QRY_ORDINARY, err_msg);
        qry_member_file_unmapped(mapped, size);
        if (!appended)
        {
            return 0;
        }
    }
    return 1;
}

int query_run_read(QueryRun *run, const char *const *paths, unsigned int count, VerbaLinea *err_msg)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (!query_is_file(paths[at]))
        {
            continue;
        }
        // a run that has the file already, a step before this one in the course having read or written it, retains
        // its own: the disk's copy is older than the run's until the run ends and writes it
        char name[QUERY_PATH_MAX_LEN];
        query_named(run->writer.top, paths[at], name, sizeof(name));
        const unsigned char *bytes = NULL;
        unsigned long long size = 0ull;
        if (!query_writer_latest(&run->writer, name, &bytes, &size))
        {
            const unsigned char *const mapped = qry_member_file_mapped(paths[at], &size);
            if (mapped == NULL)
            {
                mmgr_line_put(err_msg, paths[at]);
                mmgr_line_put(err_msg, ": it could not be read");
                return 0;
            }
            const int appended = query_writer_append(&run->writer, name, mapped, size, QRY_ORDINARY, err_msg);
            qry_member_file_unmapped(mapped, size);
            if (!appended)
            {
                return 0;
            }
        }
        if (query_paths_room(&run->read, QUERY_LEAVES_MOST))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = run->read.path[run->read.count],
                              .src = paths[at],
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}) + 1u});
            run->read.count += 1u;
        }
    }
    return 1;
}

void query_run_left(QueryRun *run, const char *const *paths, unsigned int count)
{
    for (unsigned int at = 0u; (at < count) && query_paths_room(&run->left, QUERY_LEAVES_MOST); at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = run->left.path[run->left.count],
                                .src = paths[at],
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}) + 1u});
        run->left.count += 1u;
    }
}

int query_run_stopped(QueryRun *run, VerbaLinea *err_msg)
{
    query_run_paths_given(run);
    // this process is a member of the run no more before the writer is requested to stop
    const unsigned long long process = run->process;
    run->process = 0ull;
    if (process == 0ull)
    {
        query_writer_parted(&run->writer);
        return 1;
    }
    query_writer_parted(&run->writer);
    const int stopped = query_writer_stop(&run->writer, err_msg);
    QueryProcess waited = {process, 0ull};
    query_process_wait(&waited);
    query_runs_pruned(run->folder, query_retain_max());
    return stopped;
}

int query_run_ended(QueryRun *run, QueryLeave leave, QueryPrompter prompter, QueryContext *context, VerbaLinea *err_msg)
{
    for (unsigned int at = 0u; at < run->left.count; at += 1u)
    {
        char left[QUERY_LINE_MAX_LEN];
        VerbaLinea left_line = {left, sizeof(left), 0ull};
        mmgr_line_put(&left_line, "  ");
        if (!query_writer_left(&run->writer, run->left.path[at], &left_line) && (mmgr_line_finish(&left_line) != 0ull))
        {
            query_print(context, left);
        }
    }
    query_paths_given(&run->left);
    if (!query_leaves_settled(run, leave, prompter, context, err_msg))
    {
        query_run_paths_given(run);
        return 0;
    }
    return query_run_stopped(run, err_msg);
}
