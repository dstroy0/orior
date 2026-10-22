// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// writer.c: the run's writer (qry_writer.c) from this process. The writer's process is started with the run's .qry
// and the top of the tree, and stopped through the run: this process, a member of it, asks it to stop. Every record
// appended or read goes through the run this process is a member of (qry_member.h): an append is one write into this
// process's ring, a read is the record where the writer put it. A program's output goes to the run as it comes, each
// piece a record of `<name>.console`, and to this process's own output
#include "../query_internal.h"

// the environment every program of the run runs with: QRY, QRY_TOP and the PATH, set on this process, which every
// process it starts inherits
static void query_writer_environment(const QueryWriter *writer)
{
    query_environment_set("QRY", writer->qry);
    query_environment_set("QRY_TOP", writer->top);
    query_environment_set("PATH", writer->path);
}

int query_writer_start(const QueryWriter *writer, unsigned long long *process, CliLine *said)
{
    query_writer_environment(writer);
    const char *const words[4] = {writer->binary, writer->qry, writer->top, NULL};
    QueryProcess started;
    if (!query_process_start(&started, words, NULL, QUERY_OUTPUT_NONE))
    {
        cli_line_text(said, "the run's writer did not start: ");
        cli_line_text(said, writer->binary);
        return 0;
    }
    *process = started.process;
    return 1;
}

int query_writer_runs(const QueryWriter *writer)
{
    return qry_member_writer_runs(writer->qry);
}

int query_writer_made(const QueryWriter *writer)
{
    for (unsigned int step = 0u; step < QUERY_READY_STEPS; step += 1u)
    {
        if (query_writer_runs(writer))
        {
            // the line naming the run's database, said on the console of whatever started the run, for a reader of
            // the run to find it by
            char line[QUERY_PATH_LONGEST + 64u];
            CliLine made = {line, sizeof(line), 0ull};
            cli_line_text(&made, "  orior_qry_writer: the run of ");
            cli_line_text(&made, writer->qry);
            cli_line_text(&made, " is made\n");
            if (cli_line_fits(&made))
            {
                query_terminal_written(0, line, made.at);
            }
            return 1;
        }
        query_paused(10u);
    }
    return 0;
}

int query_writer_joined(QueryWriter *writer)
{
    writer->member = qry_member_joined(writer->qry) != NULL;
    return writer->member;
}

// 1 where this process is a member of `writer`'s run now
static int query_writer_member_now(const QueryWriter *writer)
{
    const char *const path = qry_member_path();
    return (path != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = writer->qry, .cap = CLI_CELLS_POOL}));
}

int query_writer_member_held(const QueryWriter *writer)
{
    // a process is a member of one run at a time: a writer that joined its run, whose place another run took since,
    // joins it again
    return writer->member && (query_writer_member_now(writer) || (qry_member_joined(writer->qry) != NULL));
}

void query_writer_parted(QueryWriter *writer)
{
    if (writer->member && query_writer_member_now(writer))
    {
        qry_member_parted();
    }
    writer->member = 0;
}

int query_writer_stop(QueryWriter *writer, CliLine *said)
{
    if (query_writer_member_held(writer))
    {
        qry_member_stopped();
        return 1;
    }
    if (qry_member_joined(writer->qry) == NULL)
    {
        cli_line_text(said, "no writer runs ");
        cli_line_text(said, writer->qry);
        return 0;
    }
    qry_member_stopped();
    qry_member_parted();
    return 1;
}

// `what: this process is a member of no run` said where it is none: 1 where it is one
static int query_writer_member(const QueryWriter *writer, const char *what, CliLine *said)
{
    const int held = query_writer_member_held(writer);
    if (!held)
    {
        cli_line_text(said, what);
        cli_line_text(said, ": this process is a member of no run");
    }
    return held;
}

int query_writer_append(const QueryWriter *writer, const char *name, const void *bytes, unsigned long long size,
                        unsigned int priority, CliLine *said)
{
    if (!query_writer_member(writer, name, said))
    {
        return 0;
    }
    if (qry_member_append(name, bytes, size, priority) == 0ull)
    {
        cli_line_text(said, name);
        cli_line_text(said, " was not appended to the run");
        return 0;
    }
    // taken by the writer before this returns, so that what is appended is there for the next read and the next
    // program
    qry_member_settled();
    return 1;
}

int query_writer_file(const QueryWriter *writer, const char *path, const void *bytes, unsigned long long size,
                      CliLine *said)
{
    if (!query_writer_member(writer, path, said))
    {
        return 0;
    }
    if (qry_member_filed(path, bytes, size) == 0ull)
    {
        cli_line_text(said, path);
        cli_line_text(said, " was not written through the run's writer");
        return 0;
    }
    qry_member_settled();
    return 1;
}

int query_writer_left(const QueryWriter *writer, const char *path, CliLine *said)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    if (bytes == NULL)
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    const int filed = query_writer_file(writer, path, bytes, size, said);
    qry_member_file_unmapped(bytes, size);
    return filed;
}

int query_writer_tee(const QueryWriter *writer, QueryContext *context, const char *name, const char *const *words,
                     long long *code, CliLine *said)
{
    char console[QRY_NAME_LONGEST + 1u];
    CliLine console_line = {console, sizeof(console), 0ull};
    cli_line_text(&console_line, name);
    cli_line_text(&console_line, ".console");
    if ((cli_line_closed(&console_line) == 0ull) || !query_writer_member(writer, name, said))
    {
        return 0;
    }
    query_writer_environment(writer);
    QueryProcess child;
    if (!query_process_start(&child, words, context->root, QUERY_OUTPUT_PIPED))
    {
        cli_line_text(said, words[0]);
        cli_line_text(said, ": it could not be started");
        return 0;
    }
    static char s_piece[65536];
    for (;;)
    {
        const unsigned long long got = query_process_read(&child, s_piece, sizeof(s_piece));
        if (got == 0ull)
        {
            break;
        }
        query_terminal_written(0, s_piece, got);
        qry_member_append(console, s_piece, got, QRY_ORDINARY);
    }
    *code = query_process_wait(&child);
    qry_member_settled();
    return 1;
}
