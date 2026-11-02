// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// writer.c: the run's writer (qry_writer.c) from this process. The writer's process is started with the run's .qry
// and the top of the tree, and stopped through the run: this process, a member of it, requests it to stop. Every record
// appended or read goes through the run this process is a member of (qry_member.h): an append is one write into this
// process's ring, a read is the record where the writer put it. A program's output goes to the run as it comes, each
// piece a record of `<name>.console`, and to this process's own output
#include "../query_internal.h"

typedef struct
{
    const QueryWriter *writer;
    const char *what;
    VerbaLinea *err_msg;
} WriterCtx;

// the environment every program of the run runs with: QRY, QRY_TOP and the PATH, set on this process, which every
// process it starts inherits
EMBED_INLINE void query_writer_environment(const WriterCtx *args)
{
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "QRY", .value = args->writer->qry);
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "QRY_TOP", .value = args->writer->top);
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "PATH", .value = args->writer->path);
}

int query_writer_start(const QueryWriterCtx *args)
{
    unsigned long long *process = args->process_u64s;
    EMBED_CALL(query_writer_environment, WriterCtx, .writer = args->writer);
    const char *const words[4] = {args->writer->binary, args->writer->qry, args->writer->top, NULL};
    QueryProcess started;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &started, .words = words,
                    .folder = NULL, .output = QUERY_OUTPUT_NONE))
    {
        mmgr_line_put(args->err_msg, "the run's writer did not start: ");
        mmgr_line_put(args->err_msg, args->writer->binary);
        return 0;
    }
    *process = started.process;
    return 1;
}

int query_writer_runs(const QueryWriterCtx *args)
{
    return qry_member_writer_runs(args->writer->qry);
}

int query_writer_init(const QueryWriterCtx *args)
{
    for (unsigned int step = 0u; step < QUERY_READY_STEPS; step += 1u)
    {
        if (query_writer_internal.writer_runs(args))
        {
            // the line naming the run's database, printed on the console of whatever started the run, for a reader of
            // the run to find it by
            char line[QUERY_PATH_MAX_LEN + 64u];
            VerbaLinea made = {line, sizeof(line), 0ull};
            mmgr_line_put(&made, "  orior_qry_writer: the run of ");
            mmgr_line_put(&made, args->writer->qry);
            mmgr_line_put(&made, " is made\n");
            if (mmgr_line_ok(&made))
            {
                EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 0, .text = line,
                           .length = made.at);
            }
            return 1;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 10u);
    }
    return 0;
}

int query_writer_join(const QueryErrMsgWriterCtx *args)
{
    args->writer_out->member = qry_member_joined(args->writer_out->qry) != NULL;
    return args->writer_out->member;
}

// 1 where this process is a member of `writer`'s run now
EMBED_INLINE int query_writer_member_now(const WriterCtx *args)
{
    const char *const path = qry_member_path();
    return (path != NULL) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = args->writer->qry, .cap = CLI_CELLS_POOL}));
}

int query_writer_is_member(const QueryCountNameCtx *args)
{
    // a process is a member of one run at a time: a writer that joined its run, whose place another run took since,
    // joins it again
    return args->writer->member && (EMBED_CALL(query_writer_member_now, WriterCtx, .writer = args->writer) ||
                                    (qry_member_joined(args->writer->qry) != NULL));
}

void query_writer_part(const QueryErrMsgWriterCtx *args)
{
    if (args->writer_out->member && EMBED_CALL(query_writer_member_now, WriterCtx, .writer = args->writer_out))
    {
        qry_member_parted();
    }
    args->writer_out->member = 0;
}

int query_writer_stop(const QueryErrMsgWriterCtx *args)
{
    if (EMBED_CALL(query_count_name_internal.writer_is_member, QueryCountNameCtx, .writer = args->writer_out))
    {
        qry_member_stopped();
        return 1;
    }
    if (qry_member_joined(args->writer_out->qry) == NULL)
    {
        mmgr_line_put(args->err_msg, "no writer runs ");
        mmgr_line_put(args->err_msg, args->writer_out->qry);
        return 0;
    }
    qry_member_stopped();
    qry_member_parted();
    return 1;
}

// `what: this process is a member of no run` reported where it is none: 1 where it is one
EMBED_INLINE int query_writer_member(const WriterCtx *args)
{
    const QueryWriter *writer = args->writer;
    const int is_member = EMBED_CALL(query_count_name_internal.writer_is_member, QueryCountNameCtx, .writer = writer);
    if (!is_member)
    {
        mmgr_line_put(args->err_msg, args->what);
        mmgr_line_put(args->err_msg, ": this process is a member of no run");
    }
    return is_member;
}

int query_writer_append(const QueryNameBytesCtx *args)
{
    if (!EMBED_CALL(query_writer_member, WriterCtx, .writer = args->writer, .what = args->name,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (qry_member_append(args->name, args->bytes, args->size, args->priority) == 0ull)
    {
        mmgr_line_put(args->err_msg, args->name);
        mmgr_line_put(args->err_msg, " was not appended to the run");
        return 0;
    }
    // taken by the writer before this returns, so that what is appended is there for the next read and the next
    // program
    qry_member_settled();
    return 1;
}

int query_writer_file(const QueryPathBytesSizeCtx *args)
{
    if (!EMBED_CALL(query_writer_member, WriterCtx, .writer = args->writer, .what = args->path,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (qry_member_filed(args->path, args->bytes, args->size) == 0ull)
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, " was not written through the run's writer");
        return 0;
    }
    qry_member_settled();
    return 1;
}

int query_writer_leave(const QueryPathBytesSizeCtx *args)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(args->path, &size);
    if (bytes == NULL)
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": it could not be read");
        return 0;
    }
    const int filed =
        EMBED_CALL(query_path_bytes_size_internal.writer_file, QueryPathBytesSizeCtx, .writer = args->writer,
                   .path = args->path, .bytes = bytes, .size = size, .err_msg = args->err_msg);
    qry_member_file_unmapped(bytes, size);
    return filed;
}

int query_writer_tee(const QueryWordsNameCtx *args)
{
    long long *code = args->code;
    char console[QRY_NAME_MAX_LEN + 1u];
    VerbaLinea console_line = {console, sizeof(console), 0ull};
    mmgr_line_put(&console_line, args->name);
    mmgr_line_put(&console_line, ".console");
    if ((mmgr_line_finish(&console_line) == 0ull) || !EMBED_CALL(query_writer_member, WriterCtx, .writer = args->writer,
                                                                 .what = args->name, .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(query_writer_environment, WriterCtx, .writer = args->writer);
    QueryProcess child;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &child, .words = args->words,
                    .folder = args->context->root, .output = QUERY_OUTPUT_PIPED))
    {
        mmgr_line_put(args->err_msg, args->words[0]);
        mmgr_line_put(args->err_msg, ": it could not be started");
        return 0;
    }
    static char s_piece[65536];
    for (;;)
    {
        const unsigned long long got = EMBED_CALL(query_process_piece_internal.process_read, QueryProcessPieceCtx,
                                                  .process = &child, .piece = s_piece, .room_u64 = sizeof(s_piece));
        if (got == 0ull)
        {
            break;
        }
        EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 0, .text = s_piece, .length = got);
        qry_member_append(console, s_piece, got, QRY_ORDINARY);
    }
    *code = EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &child);
    qry_member_settled();
    return 1;
}
