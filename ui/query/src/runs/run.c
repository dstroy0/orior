// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run.c: a query run (qry_member.h): its folder, its .qry, its writer's process, whether it was joined, and the files
// it read and may leave. Every process a loop starts reaches the run by QRY. A run's .qry contains everything the run
// did and every file it ran against, as it read each one and as it left each one it changed. As a run ends only the
// newest QRY_RETAINED runs of its folder are retained, 64 where it is not set
#include "../../../cli/src/text/text.h"
#include "../query_internal.h"

// A run carried from: its path and when it last changed
typedef struct
{
    const char *path;
    unsigned long long changed;
} QueryTransferredRun;
// the folder the course's runs are stored in (course.c)
#define QUERY_COURSE_FOLDER "build/engine/course/qry"

typedef struct
{
    const char *path;
    const char *root;
    char *folder;
    QueryRun *run;
    const char *qry;
    const QueryPrograms *programs;
    VerbaLinea *err_msg;
    const QueryRuns *folder_runs_pointer;
    const QueryRuns *course;
    QueryTransferredRun **carried;
} RunCtx;
// the folder `path` stands in written into `folder`, the top of the tree where it names none
EMBED_INLINE void query_parent_get(const RunCtx *args)
{
    const char *slash = NULL;
    const unsigned long long slash_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    slash = (slash_end != 0ULL) ? (args->path + (slash_end - 1ULL)) : slash;
    VerbaLinea line = {args->folder, QUERY_PATH_MAX_LEN, 0ULL};
    if (slash != NULL)
    {
        mmgr_line_put_n(&line, args->path, (unsigned long long)(slash - args->path));
    }
    else
    {
        mmgr_line_put(&line, args->root);
    }
    mmgr_line_finish(&line);
}

// the cells of the files a run read and may leave given back, as the run ends and before a run begins again in its
// capacity
EMBED_INLINE void query_run_paths_free(const RunCtx *args)
{
    EMBED_CALL(query_paths_internal.paths_free, QueryPathsCtx, .paths = &args->run->read);
    EMBED_CALL(query_paths_internal.paths_free, QueryPathsCtx, .paths = &args->run->left);
}

// the cells of the files a run reads and may leave taken at their most
EMBED_INLINE void query_run_paths_alloc(const RunCtx *args)
{
    (void)EMBED_CALL(query_paths_internal.paths_alloc, QueryPathsCtx, .paths = &args->run->read,
                     .count = QUERY_LEAVES_MAX);
    (void)EMBED_CALL(query_paths_internal.paths_alloc, QueryPathsCtx, .paths = &args->run->left,
                     .count = QUERY_LEAVES_MAX);
}

// the run's writer named: its program built where stale, its .qry and the top of the tree
EMBED_INLINE int query_run_writer(const RunCtx *args)
{
    const char *root = args->root;
    const QueryPrograms *programs = args->programs;
    VerbaLinea *err_msg = args->err_msg;
    memor.set(&(MemoriaCfg){.dst = args->run, .val = 0, .bytes = sizeof(*args->run)});
    if (!EMBED_CALL(query_name_capacity_internal.tool_build, QueryNameCapacityCtx, .root = root, .programs = programs,
                    .name = "orior_qry_writer", .binary = args->run->writer.binary,
                    .capacity = sizeof(args->run->writer.binary), .err_msg = err_msg))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->run->writer.qry,
                            .src = args->qry,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->qry, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = args->run->writer.top,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    args->run->writer.path = programs->run_path(programs->context);
    return 1;
}

int query_run_joining(const QueryErrMsgRootCtx *args)
{
    EMBED_CALL(query_run_paths_free, RunCtx, .run = args->run);
    if (!EMBED_CALL(query_run_writer, RunCtx, .run = args->run, .root = args->root, .qry = args->qry,
                    .programs = args->programs, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(query_writer_internal.writer_runs, QueryWriterCtx, .writer = &args->run->writer) ||
        !EMBED_CALL(query_err_msg_writer_internal.writer_join, QueryErrMsgWriterCtx, .writer_out = &args->run->writer))
    {
        mmgr_line_put(args->err_msg, "no writer runs ");
        mmgr_line_put(args->err_msg, args->qry);
        return 0;
    }
    EMBED_CALL(query_parent_get, RunCtx, .path = args->qry, .root = args->root, .folder = args->run->folder);
    args->run->joined = 1;
    EMBED_CALL(query_run_paths_alloc, RunCtx, .run = args->run);
    return 1;
}

int query_run_start(const QueryFolderContextCtx *args)
{
    QueryRun *run = args->run;
    EMBED_CALL(query_run_paths_free, RunCtx, .run = run);
    char qry_path[QUERY_PATH_MAX_LEN];
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "QRY", .value_out = qry_path,
                   .capacity = sizeof(qry_path)))
    {
        QueryRun joined;
        if (EMBED_CALL(query_run_writer, RunCtx, .run = &joined, .root = args->root, .qry = qry_path,
                       .programs = args->programs, .err_msg = args->err_msg) &&
            EMBED_CALL(query_writer_internal.writer_runs, QueryWriterCtx, .writer = &joined.writer) &&
            EMBED_CALL(query_err_msg_writer_internal.writer_join, QueryErrMsgWriterCtx, .writer_out = &joined.writer))
        {
            *run = joined;
            EMBED_CALL(query_parent_get, RunCtx, .path = qry_path, .root = args->root, .folder = run->folder);
            run->joined = 1;
            EMBED_CALL(query_run_paths_alloc, RunCtx, .run = run);
            return 1;
        }
        args->err_msg->at = 0ULL;
    }
    if (!query_folder_context_internal.folder_init(args))
    {
        mmgr_line_put(args->err_msg, args->folder);
        mmgr_line_put(args->err_msg, " is no folder");
        return 0;
    }
    char name[128];
    EMBED_CALL(query_folder_capacity_internal.next_name, QueryFolderCapacityCtx, .folder = args->folder,
               .name_out = name, .capacity = sizeof(name));
    char qry[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {qry, sizeof(qry), 0ULL};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    char full[QUERY_PATH_MAX_LEN];
    if ((mmgr_line_finish(&line) == 0ULL) ||
        !EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = qry, .full = full,
                    .capacity = sizeof(full)) ||
        !EMBED_CALL(query_run_writer, RunCtx, .run = run, .root = args->root, .qry = full, .programs = args->programs,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = run->folder,
                            .src = args->folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_CELLS_POOL}) + 1u});
    if (!EMBED_CALL(query_writer_internal.writer_start, QueryWriterCtx, .writer = &run->writer,
                    .process_u64s = &run->process, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(query_writer_internal.writer_init, QueryWriterCtx, .writer = &run->writer))
    {
        mmgr_line_put(args->err_msg, "the run's writer did not start");
        return 0;
    }
    if (!EMBED_CALL(query_err_msg_writer_internal.writer_join, QueryErrMsgWriterCtx, .writer_out = &run->writer))
    {
        EMBED_CALL(query_err_msg_writer_internal.writer_stop, QueryErrMsgWriterCtx, .writer_out = &run->writer,
                   .err_msg = args->err_msg);
        mmgr_line_put(args->err_msg, "the run's buffer could not be opened");
        return 0;
    }
    EMBED_CALL(query_run_paths_alloc, RunCtx, .run = run);
    return 1;
}

// the base name of `path`
EMBED_INLINE const char *query_base(const RunCtx *args)
{
    const char *base = args->path;
    const unsigned long long base_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    base = (base_end != 0ULL) ? (args->path + base_end) : base;
    return base;
}

// the runs of `folder`'s and of `course`, where `course` is given, put among those carried from, newest first by when
// each last changed, in a cell of one place for each: their count, the cell into `carried`, NULL where none are or
// the cell could not be taken
EMBED_INLINE unsigned int query_transferred_runs(const RunCtx *args)
{
    QueryTransferredRun **carried = args->carried;
    const unsigned int count = args->folder_runs_pointer->count + args->course->count;
    *carried = (count > 0u) ? g_cli_cells->persistent_buf_alloc((unsigned long long)count * sizeof(QueryTransferredRun),
                                                                &g_query_cells)
                            : NULL;
    if (*carried == NULL)
    {
        return 0u;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const path = (at < args->folder_runs_pointer->count)
                                     ? args->folder_runs_pointer->path[at]
                                     : args->course->path[at - args->folder_runs_pointer->count];
        (*carried)[at] = (QueryTransferredRun){path, 0ULL};
        EMBED_CALL(query_path_seconds_internal.modify, QueryPathSecondsCtx, .path = path,
                   .seconds = &(*carried)[at].changed);
    }
    // newest first: a pass run alone after a course carries what the course left
    for (unsigned int at = 1u; at < count; at += 1u)
    {
        for (unsigned int back = at; (back > 0u) && ((*carried)[back - 1u].changed < (*carried)[back].changed);
             back -= 1u)
        {
            const QueryTransferredRun swap = (*carried)[back];
            (*carried)[back] = (*carried)[back - 1u];
            (*carried)[back - 1u] = swap;
        }
    }
    return count;
}

int query_run_machine_transfer(const QueryCountFolderCtx *args)
{
    char course[QUERY_PATH_MAX_LEN];
    VerbaLinea course_line = {course, sizeof(course), 0ULL};
    mmgr_line_put(&course_line, args->run->writer.top);
    mmgr_line_ch(&course_line, '/');
    mmgr_line_put(&course_line, QUERY_COURSE_FOLDER);
    mmgr_line_finish(&course_line);
    char full_folder[QUERY_PATH_MAX_LEN];
    char full_course[QUERY_PATH_MAX_LEN];
    const int course_apart =
        !EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = args->folder,
                    .full = full_folder, .capacity = sizeof(full_folder)) ||
        !EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = course, .full = full_course,
                    .capacity = sizeof(full_course)) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = full_folder, .other = full_course, .cap = CLI_CELLS_POOL}));
    QueryRuns folder_runs;
    QueryRuns course_runs = {NULL, NULL, 0u};
    (void)EMBED_CALL(query_folder_runs_internal.runs_alloc, QueryFolderRunsCtx, .folder = args->folder,
                     .runs = &folder_runs);
    if (course_apart)
    {
        (void)EMBED_CALL(query_folder_runs_internal.runs_alloc, QueryFolderRunsCtx, .folder = course,
                         .runs = &course_runs);
    }
    QueryTransferredRun *transferred_from = NULL;
    const unsigned int runs = EMBED_CALL(query_transferred_runs, RunCtx, .folder_runs_pointer = &folder_runs,
                                         .course = &course_runs, .carried = &transferred_from);
    static const char *s_missing[QUERY_LEAVES_MAX];
    for (unsigned int at = 0u; at < runs; at += 1u)
    {
        unsigned int missing = 0u;
        for (unsigned int file = 0u; (file < args->count) && (missing < QUERY_LEAVES_MAX); file += 1u)
        {
            const unsigned char *bytes = NULL;
            unsigned long long size = 0ULL;
            if (!EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &args->run->writer,
                            .name = EMBED_CALL(query_base, RunCtx, .path = args->files[file]), .bytes_u8s = &bytes,
                            .size_pointer = &size))
            {
                s_missing[missing] = EMBED_CALL(query_base, RunCtx, .path = args->files[file]);
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
        if (!EMBED_CALL(query_capacity_qry_path_internal.open, QueryCapacityQryPathCtx,
                        .qry_path = transferred_from[at].path, .crystal = args->crystal, .named = named,
                        .capacity = sizeof(named)) ||
            (EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = named, .full = full_named,
                        .capacity = sizeof(full_named)) &&
             EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = args->run->writer.qry,
                        .full = full_qry, .capacity = sizeof(full_qry)) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = full_named, .other = full_qry, .cap = CLI_CELLS_POOL}))))
        {
            continue;
        }
        char carried[QUERY_LINE_MAX_LEN];
        VerbaLinea transferred_line = {carried, sizeof(carried), 0ULL};
        EMBED_CALL(query_count_name_internal.writer_machine_transfer, QueryCountNameCtx, .writer = &args->run->writer,
                   .from = named, .names = s_missing, .count = missing, .err_msg = &transferred_line);
    }
    g_cli_cells->persistent_buf_release(transferred_from, &g_query_cells);
    EMBED_CALL(query_folder_runs_internal.runs_free, QueryFolderRunsCtx, .runs = &course_runs);
    EMBED_CALL(query_folder_runs_internal.runs_free, QueryFolderRunsCtx, .runs = &folder_runs);
    for (unsigned int file = 0u; file < args->count; file += 1u)
    {
        const unsigned char *bytes = NULL;
        unsigned long long size = 0ULL;
        if (EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &args->run->writer,
                       .name = EMBED_CALL(query_base, RunCtx, .path = args->files[file]), .bytes_u8s = &bytes,
                       .size_pointer = &size) ||
            !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->files[file]))
        {
            continue;
        }
        const unsigned char *const mapped = qry_member_file_mapped(args->files[file], &size);
        if (mapped == NULL)
        {
            mmgr_line_put(args->err_msg, args->files[file]);
            mmgr_line_put(args->err_msg, ": it could not be read");
            return 0;
        }
        const int appended =
            EMBED_CALL(query_name_bytes_internal.writer_append, QueryNameBytesCtx, .writer = &args->run->writer,
                       .name = EMBED_CALL(query_base, RunCtx, .path = args->files[file]), .bytes = mapped, .size = size,
                       .priority = QRY_ORDINARY, .err_msg = args->err_msg);
        qry_member_file_unmapped(mapped, size);
        if (!appended)
        {
            return 0;
        }
    }
    return 1;
}

int query_run_read(const QueryCountErrMsgCtx *args)
{
    unsigned int count = args->count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->paths_letters[at]))
        {
            continue;
        }
        // a run that has the file already, a step before this one in the course having read or written it, retains
        // its own: the disk's copy is older than the run's until the run ends and writes it
        char name[QUERY_PATH_MAX_LEN];
        EMBED_CALL(query_capacity_path_name_internal.by_name, QueryCapacityPathNameCtx, .top = args->run->writer.top,
                   .path = args->paths_letters[at], .name_out = name, .capacity = sizeof(name));
        const unsigned char *bytes = NULL;
        unsigned long long size = 0ULL;
        if (!EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &args->run->writer,
                        .name = name, .bytes_u8s = &bytes, .size_pointer = &size))
        {
            const unsigned char *const mapped = qry_member_file_mapped(args->paths_letters[at], &size);
            if (mapped == NULL)
            {
                mmgr_line_put(args->err_msg, args->paths_letters[at]);
                mmgr_line_put(args->err_msg, ": it could not be read");
                return 0;
            }
            const int appended = EMBED_CALL(query_name_bytes_internal.writer_append, QueryNameBytesCtx,
                                            .writer = &args->run->writer, .name = name, .bytes = mapped, .size = size,
                                            .priority = QRY_ORDINARY, .err_msg = args->err_msg);
            qry_member_file_unmapped(mapped, size);
            if (!appended)
            {
                return 0;
            }
        }
        if (args->run->read.count < args->run->read.capacity)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = args->run->read.path[args->run->read.count],
                .src = args->paths_letters[at],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->paths_letters[at], .cap = CLI_CELLS_POOL}) + 1u});
            args->run->read.count += 1u;
        }
    }
    return 1;
}

void query_run_leave(const QueryCountErrMsgCtx *args)
{
    unsigned int count = args->count;
    for (unsigned int at = 0u; (at < count) && (args->run->left.count < args->run->left.capacity); at += 1u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = args->run->left.path[args->run->left.count],
            .src = args->paths_letters[at],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->paths_letters[at], .cap = CLI_CELLS_POOL}) + 1u});
        args->run->left.count += 1u;
    }
}

int query_run_stop(const QueryContextErrMsgRunCtx *args)
{
    EMBED_CALL(query_run_paths_free, RunCtx, .run = args->run);
    // this process is a member of the run no more before the writer is requested to stop
    const unsigned long long process = args->run->process;
    args->run->process = 0ULL;
    if (process == 0ULL)
    {
        EMBED_CALL(query_err_msg_writer_internal.writer_part, QueryErrMsgWriterCtx, .writer_out = &args->run->writer);
        return 1;
    }
    EMBED_CALL(query_err_msg_writer_internal.writer_part, QueryErrMsgWriterCtx, .writer_out = &args->run->writer);
    const int stopped = EMBED_CALL(query_err_msg_writer_internal.writer_stop, QueryErrMsgWriterCtx,
                                   .writer_out = &args->run->writer, .err_msg = args->err_msg);
    QueryProcess waited = {process, 0ULL};
    EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &waited);
    EMBED_CALL(query_folder_max_internal.runs_prune, QueryFolderMaxCtx, .folder = args->run->folder,
               .most = query_retain_max());
    return stopped;
}

int query_run_end(const QueryContextErrMsgRunCtx *args)
{
    for (unsigned int at = 0u; at < args->run->left.count; at += 1u)
    {
        char left[QUERY_LINE_MAX_LEN];
        VerbaLinea left_line = {left, sizeof(left), 0ULL};
        mmgr_line_put(&left_line, "  ");
        if (!EMBED_CALL(query_path_bytes_size_internal.writer_leave, QueryPathBytesSizeCtx,
                        .writer = &args->run->writer, .path = args->run->left.path[at], .err_msg = &left_line) &&
            (mmgr_line_finish(&left_line) != 0ULL))
        {
            EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = args->context, .text = left);
        }
    }
    EMBED_CALL(query_paths_internal.paths_free, QueryPathsCtx, .paths = &args->run->left);
    if (!EMBED_CALL(query_context_err_msg_internal.leaves_settle, QueryContextErrMsgCtx, .run_read = args->run,
                    .leave = args->leave, .prompter = args->prompter, .context = args->context,
                    .err_msg = args->err_msg))
    {
        EMBED_CALL(query_run_paths_free, RunCtx, .run = args->run);
        return 0;
    }
    return query_context_err_msg_run_internal.run_stop(args);
}
