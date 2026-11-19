// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sessions.c: the sessions one window holds, each a far end started over a link, and the far end itself, which sends
// what it reads to the window on its host
#include "sessions.h"
#include "../machine/reach.h"
#include "link.h"

// the bytes a frame's length takes
#define SESSIONS_HEAD 4ULL
// the bytes an answer's body takes at most: its id, the mark and the text, bounded by what the door's answer carries
#define SESSIONS_ANSWER (CLI_ERR_MSG_MAX_LEN + 64ULL)

// One session: the address it was joined with, the far end started over its link, and the answer read from it so far,
// `got` bytes of the head and body, whole once `answered` is 1
typedef struct
{
    char address[CLI_PATH_MAX_LEN];
    CliSpawned child;
    unsigned long long got;
    int answered;
    unsigned char answer[SESSIONS_HEAD + SESSIONS_ANSWER];
} CliSession;

// The sessions: a cell of `count` records; the request in flight, its id and the moment the sender stops waiting
// for the answers; and the answer waiting to be given, `pending` while one is, `done` 1 where every host ran it
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned int count;
    int pending;
    int sending;
    int done;
    unsigned long long id;
    unsigned long long until;
    char text[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea line;
} SessionsCtx;

typedef struct
{
    unsigned int at;
    CliSession *session;
    const CliSession *session_read;
    unsigned long long pipe_handle;
    unsigned char *bytes;
    const unsigned char *bytes_read;
    unsigned long long size;
    const char *root;
    VerbaLinea *out;
    int done;
} SessionsOpCtx;

static SessionsCtx s_sessions;

// the record at `at` read into `session`, and `session_read` written at `at`
EMBED_INLINE void session_read(const SessionsOpCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_sessions.cell,
                                                            .offset = (embed_word)args->at * sizeof(CliSession),
                                                            .dst = args->session,
                                                            .bytes = sizeof(CliSession)});
}

EMBED_INLINE void session_write(const SessionsOpCtx *args)
{
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_sessions.cell,
                                                             .offset = (embed_word)args->at * sizeof(CliSession),
                                                             .src = args->session_read,
                                                             .bytes = sizeof(CliSession)});
}

// `size` written as a frame's head, little-endian, and read back from one
EMBED_INLINE void sessions_head_write(const SessionsOpCtx *args)
{
    for (unsigned int at = 0u; at < (unsigned int)SESSIONS_HEAD; at += 1u)
    {
        // each byte of the length in turn, the low one first
        args->bytes[at] = (unsigned char)((args->size >> (8u * at)) & 0xffULL);
    }
}

EMBED_INLINE unsigned long long sessions_head_read(const SessionsOpCtx *args)
{
    unsigned long long size = 0ULL;
    for (unsigned int at = 0u; at < (unsigned int)SESSIONS_HEAD; at += 1u)
    {
        size |= (unsigned long long)args->bytes_read[at] << (8u * at);
    }
    return size;
}

// `size` bytes written to `pipe_handle` as one frame: 1, or 0 where the far end stopped reading
EMBED_INLINE int sessions_frame_write(const SessionsOpCtx *args)
{
    unsigned char head[SESSIONS_HEAD];
    EMBED_CALL(sessions_head_write, SessionsOpCtx, .bytes = head, .size = args->size);
    return EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = args->pipe_handle,
                      .bytes = head, .size = SESSIONS_HEAD) &&
           EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = args->pipe_handle,
                      .bytes = args->bytes_read, .size = args->size);
}

// `size` bytes read whole from `pipe_handle` into `bytes`, waiting as they come: 1, or 0 where the pipe ended first
EMBED_INLINE int sessions_read_whole(const SessionsOpCtx *args)
{
    unsigned long long got = 0ULL;
    while (got < args->size)
    {
        const unsigned long long piece = EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read,
                                                    CliCapacityPipeHandleCtx, .pipe_handle = args->pipe_handle,
                                                    .piece = (char *)args->bytes + got, .capacity = args->size - got);
        if (piece == 0ULL)
        {
            return 0;
        }
        got += piece;
    }
    return 1;
}

// the session at `at` ended: its far end killed and waited on, its pipes let go
EMBED_INLINE void session_end(const SessionsOpCtx *args)
{
    CliSession session;
    EMBED_CALL(session_read, SessionsOpCtx, .at = args->at, .session = &session);
    EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &session.child);
    (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &session.child);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &session.child);
}

// the record of the session on `address`, or the count where there is none
EMBED_INLINE unsigned int session_find(const CliSessionsCfg *args)
{
    CliSession session;
    for (unsigned int at = 0u; at < s_sessions.count; at += 1u)
    {
        EMBED_CALL(session_read, SessionsOpCtx, .at = at, .session = &session);
        if (cellul.eq(&(CatenaFinitaCfg){.src = session.address, .other = args->address, .cap = CLI_POOL_BYTES}))
        {
            return at;
        }
    }
    return s_sessions.count;
}

// the answer waiting begun: its text started over, `done` as given
EMBED_INLINE void sessions_answer_begin(const SessionsOpCtx *args)
{
    s_sessions.line = (VerbaLinea){s_sessions.text, sizeof(s_sessions.text), 0ULL};
    s_sessions.pending = 1;
    s_sessions.sending = 0;
    s_sessions.done = args->done;
}

void cli_sessions_refused(const CliSessionsCfg *args)
{
    EMBED_CALL(sessions_answer_begin, SessionsOpCtx, .done = 0);
    mmgr_line_put_n(&s_sessions.line, args->err_msg->out, args->err_msg->at);
}

int cli_sessions_join_words(const CliSessionsCfg *args)
{
    if (s_sessions.pending)
    {
        mmgr_line_put(args->err_msg, "an answer is still gathered");
        return 0;
    }
    if (args->address == NULL)
    {
        mmgr_line_put(args->err_msg, "a host's address is needed");
        return 0;
    }
    if (EMBED_CALL(session_find, CliSessionsCfg, .address = args->address) < s_sessions.count)
    {
        mmgr_line_put(args->err_msg, "a session is open on ");
        mmgr_line_put(args->err_msg, args->address);
        return 0;
    }
    CliSession session;
    memor.set(&(MemoriaCfg){.dst = &session, .val = 0, .bytes = sizeof(session)});
    VerbaLinea address = {session.address, sizeof(session.address), 0ULL};
    mmgr_line_put(&address, args->address);
    if (mmgr_line_finish(&address) == 0ULL)
    {
        mmgr_line_put(args->err_msg, "the address does not fit a session's");
        return 0;
    }
    // the words end at a NULL, as a spawn reads them; the count is the settings', and there are none
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn_piped_as, CliWordsErrMsgCtx, .priority = CLI_PRIORITY_WORK,
                    .spawned = &session.child, .words = args->words, .count = 0u, .folder = NULL, .settings = NULL,
                    .detached = 0, .err_msg = args->err_msg))
    {
        return 0;
    }
    const CarcerMovableEntry *const grown = g_cli_guard->movable_buf_alloc(
        (unsigned long long)(s_sessions.count + 1u) * sizeof(CliSession), g_cli_comms_owner);
    if (grown == NULL)
    {
        EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &session.child);
        (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &session.child);
        EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &session.child);
        mmgr_line_put(args->err_msg, "no cell holds another session");
        return 0;
    }
    if (s_sessions.cell != NULL)
    {
        CliSession kept;
        for (unsigned int at = 0u; at < s_sessions.count; at += 1u)
        {
            EMBED_CALL(session_read, SessionsOpCtx, .at = at, .session = &kept);
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = grown,
                                                                     .offset = (embed_word)at * sizeof(CliSession),
                                                                     .src = &kept,
                                                                     .bytes = sizeof(CliSession)});
        }
        (void)g_cli_guard->movable_buf_release(s_sessions.cell, g_cli_comms_owner);
    }
    s_sessions.cell = grown;
    EMBED_CALL(session_write, SessionsOpCtx, .at = s_sessions.count, .session_read = &session);
    s_sessions.count += 1u;
    EMBED_CALL(sessions_answer_begin, SessionsOpCtx, .done = 1);
    mmgr_line_put(&s_sessions.line, "joined ");
    mmgr_line_put(&s_sessions.line, args->address);
    return 1;
}

int cli_sessions_join(const CliSessionsCfg *args)
{
    if (args->address == NULL)
    {
        mmgr_line_put(args->err_msg, "a host's address is needed");
        return 0;
    }
    CliLinkAddress address;
    if (!EMBED_CALL(cli_link_text.address_parse, CliLinkTextCfg, .text = args->address, .address_out = &address,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    // the far end's line on the host, given the tree the far window is open on where the address names one
    char line[CLI_PATH_MAX_LEN * 2u + 16u];
    VerbaLinea relay = {line, sizeof(line), 0ULL};
    mmgr_line_put(&relay, "imp sessions relay");
    if (address.folder[0] != '\0')
    {
        mmgr_line_ch(&relay, ' ');
        EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = address.folder,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = address.folder, .cap = CLI_POOL_BYTES}), .out = &relay);
    }
    if (mmgr_line_finish(&relay) == 0ULL)
    {
        mmgr_line_put(args->err_msg, "the address does not fit the relay's line");
        return 0;
    }
    static CliWords s_words;
    EMBED_CALL(cli_link_address_table.runs, CliLinkAddressCfg, .address = &address, .line = line, .terminal = 0,
               .words = &s_words);
    if (s_words.full)
    {
        mmgr_line_put(args->err_msg, "the link's words do not fit");
        return 0;
    }
    return EMBED_CALL(cli_sessions.join_words, CliSessionsCfg, .address = args->address, .words = s_words.word_text,
                      .err_msg = args->err_msg);
}

int cli_sessions_leave(const CliSessionsCfg *args)
{
    if (s_sessions.pending)
    {
        mmgr_line_put(args->err_msg, "an answer is still gathered");
        return 0;
    }
    if (args->address == NULL)
    {
        mmgr_line_put(args->err_msg, "a host's address is needed");
        return 0;
    }
    const unsigned int found = EMBED_CALL(session_find, CliSessionsCfg, .address = args->address);
    if (found == s_sessions.count)
    {
        mmgr_line_put(args->err_msg, "no session is open on ");
        mmgr_line_put(args->err_msg, args->address);
        return 0;
    }
    EMBED_CALL(session_end, SessionsOpCtx, .at = found);
    // the records after it move down one place
    CliSession moved;
    for (unsigned int at = found + 1u; at < s_sessions.count; at += 1u)
    {
        EMBED_CALL(session_read, SessionsOpCtx, .at = at, .session = &moved);
        EMBED_CALL(session_write, SessionsOpCtx, .at = at - 1u, .session_read = &moved);
    }
    s_sessions.count -= 1u;
    if (s_sessions.count == 0u)
    {
        (void)g_cli_guard->movable_buf_release(s_sessions.cell, g_cli_comms_owner);
        s_sessions.cell = NULL;
    }
    EMBED_CALL(sessions_answer_begin, SessionsOpCtx, .done = 1);
    mmgr_line_put(&s_sessions.line, "left ");
    mmgr_line_put(&s_sessions.line, args->address);
    return 1;
}

void cli_sessions_list(const CliSessionsCfg *args)
{
    VerbaLinea *const out = (args->out != NULL) ? args->out : &s_sessions.line;
    if (args->out == NULL)
    {
        EMBED_CALL(sessions_answer_begin, SessionsOpCtx, .done = 1);
    }
    if (s_sessions.count == 0u)
    {
        mmgr_line_put(out, "no sessions");
        return;
    }
    CliSession session;
    for (unsigned int at = 0u; at < s_sessions.count; at += 1u)
    {
        EMBED_CALL(session_read, SessionsOpCtx, .at = at, .session = &session);
        if (at != 0u)
        {
            mmgr_line_ch(out, '\n');
        }
        mmgr_line_put(out, session.address);
        long long code = 0LL;
        if (EMBED_CALL(cli_spawned_internal.spawn_poll, CliSpawnedCtx, .spawned = &session.child, .code = &code) != 0)
        {
            mmgr_line_put(out, " ended");
        }
    }
}

int cli_sessions_send(const CliSessionsCfg *args)
{
    if (s_sessions.pending)
    {
        mmgr_line_put(args->err_msg, "an answer is still gathered");
        return 0;
    }
    if (s_sessions.count == 0u)
    {
        mmgr_line_put(args->err_msg, "no session is open");
        return 0;
    }
    if ((args->menu == NULL) || (args->command == NULL))
    {
        mmgr_line_put(args->err_msg, "a menu and a command are needed");
        return 0;
    }
    static char s_body[CLI_SESSIONS_FRAME];
    VerbaLinea body = {s_body, sizeof(s_body), 0ULL};
    s_sessions.id += 1ULL;
    mmgr_line_decimal(&body, s_sessions.id, 1u);
    mmgr_line_ch(&body, '\0');
    mmgr_line_put(&body, args->menu);
    mmgr_line_ch(&body, '\0');
    mmgr_line_put(&body, args->command);
    mmgr_line_ch(&body, '\0');
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(&body, args->request[at]);
        mmgr_line_ch(&body, '\0');
    }
    if (!mmgr_line_ok(&body))
    {
        mmgr_line_put(args->err_msg, "the command does not fit a frame");
        return 0;
    }
    EMBED_CALL(sessions_answer_begin, SessionsOpCtx, .done = 1);
    s_sessions.sending = 1;
    s_sessions.until = cli_clock_milliseconds() + CLI_SESSIONS_WAIT_MS;
    CliSession session;
    for (unsigned int at = 0u; at < s_sessions.count; at += 1u)
    {
        EMBED_CALL(session_read, SessionsOpCtx, .at = at, .session = &session);
        session.got = 0ULL;
        // a far end that stopped reading has ended: its line in the answer says so, with no wait for it
        session.answered = !EMBED_CALL(sessions_frame_write, SessionsOpCtx, .pipe_handle = session.child.in,
                                       .bytes_read = (const unsigned char *)s_body, .size = body.at);
        EMBED_CALL(session_write, SessionsOpCtx, .at = at, .session_read = &session);
    }
    return 1;
}

int cli_sessions_pending(void)
{
    return s_sessions.pending;
}

// what waits on the session's output read into its answer, and whether the answer is whole: 1 where it is, or where
// the far end ended and its output is empty
EMBED_INLINE int session_gathered(const SessionsOpCtx *args)
{
    CliSession *const session = args->session;
    for (;;)
    {
        const unsigned long long waiting =
            EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_waiting, CliCapacityPipeHandleCtx,
                       .pipe_handle = session->child.out, .capacity = 0ULL, .piece = NULL);
        const unsigned long long room = sizeof(session->answer) - session->got;
        if ((waiting == 0ULL) || (room == 0ULL))
        {
            break;
        }
        const unsigned long long piece = EMBED_CALL(
            cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx, .pipe_handle = session->child.out,
            .piece = (char *)session->answer + session->got, .capacity = (waiting < room) ? waiting : room);
        if (piece == 0ULL)
        {
            break;
        }
        session->got += piece;
    }
    if (session->got >= SESSIONS_HEAD)
    {
        const unsigned long long size = EMBED_CALL(sessions_head_read, SessionsOpCtx, .bytes_read = session->answer);
        if ((size > SESSIONS_ANSWER) || (session->got >= SESSIONS_HEAD + size))
        {
            return 1;
        }
    }
    long long code = 0LL;
    return EMBED_CALL(cli_spawned_internal.spawn_poll, CliSpawnedCtx, .spawned = &session->child, .code = &code) != 0;
}

// a session's line of the answer: its address, then what its host answered, or that it gave none
EMBED_INLINE void session_reported(const SessionsOpCtx *args)
{
    const CliSession *const session = args->session_read;
    VerbaLinea *const out = &s_sessions.line;
    if (out->at != 0ULL)
    {
        mmgr_line_ch(out, '\n');
    }
    mmgr_line_put(out, session->address);
    const unsigned long long size = (session->got >= SESSIONS_HEAD)
                                        ? EMBED_CALL(sessions_head_read, SessionsOpCtx, .bytes_read = session->answer)
                                        : 0ULL;
    const int whole = (size != 0ULL) && (size <= SESSIONS_ANSWER) && (session->got >= SESSIONS_HEAD + size);
    if (!whole)
    {
        mmgr_line_put(out, " gave no answer");
        s_sessions.done = 0;
        return;
    }
    // the body: the id, a 0, the mark and the text
    const char *const body = (const char *)session->answer + SESSIONS_HEAD;
    const unsigned long long id = cellul.len(&(CatenaFinitaCfg){.src = body, .cap = size});
    if (id + 1ULL >= size)
    {
        mmgr_line_put(out, " gave no answer");
        s_sessions.done = 0;
        return;
    }
    const int ran = body[id + 1ULL] == CLI_REACH_DONE;
    s_sessions.done = s_sessions.done && ran;
    mmgr_line_put(out, ran ? ": " : " refused: ");
    mmgr_line_put_n(out, body + id + 2ULL, size - id - 2ULL);
}

int cli_sessions_step(const CliSessionsCfg *args)
{
    if (!s_sessions.pending)
    {
        return 0;
    }
    if (s_sessions.sending)
    {
        int whole = 1;
        CliSession session;
        for (unsigned int at = 0u; at < s_sessions.count; at += 1u)
        {
            EMBED_CALL(session_read, SessionsOpCtx, .at = at, .session = &session);
            if (!session.answered)
            {
                session.answered = EMBED_CALL(session_gathered, SessionsOpCtx, .session = &session);
                EMBED_CALL(session_write, SessionsOpCtx, .at = at, .session_read = &session);
            }
            whole = whole && session.answered;
        }
        if (!whole && (cli_clock_milliseconds() < s_sessions.until))
        {
            return 0;
        }
        for (unsigned int at = 0u; at < s_sessions.count; at += 1u)
        {
            EMBED_CALL(session_read, SessionsOpCtx, .at = at, .session = &session);
            EMBED_CALL(session_reported, SessionsOpCtx, .session_read = &session);
        }
    }
    (void)mmgr_line_finish(&s_sessions.line);
    mmgr_line_put(args->out, s_sessions.text);
    *args->done = s_sessions.done;
    s_sessions.pending = 0;
    s_sessions.sending = 0;
    return 1;
}

void cli_sessions_close(void)
{
    for (unsigned int at = 0u; at < s_sessions.count; at += 1u)
    {
        EMBED_CALL(session_end, SessionsOpCtx, .at = at);
    }
    if (s_sessions.cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_sessions.cell, g_cli_comms_owner);
    }
    s_sessions.cell = NULL;
    s_sessions.count = 0u;
    s_sessions.pending = 0;
    s_sessions.sending = 0;
}

// `size` bytes of `bytes_read` put on the reply `out`, trimmed of the white at both ends, and cut to what fits
EMBED_INLINE void relay_text_put(const SessionsOpCtx *args)
{
    const char *const text = (const char *)args->bytes_read;
    unsigned long long from = 0ULL;
    unsigned long long to = args->size;
    while ((from < to) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\n\r", .cap = CLI_POOL_BYTES, .byte = text[from]}) != NULL))
    {
        from += 1ULL;
    }
    while ((to > from) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\n\r", .cap = CLI_POOL_BYTES, .byte = text[to - 1ULL]}) != NULL))
    {
        to -= 1ULL;
    }
    VerbaLinea *const out = args->out;
    const unsigned long long fits = (out->capacity > out->at + 1ULL) ? out->capacity - out->at - 1ULL : 0ULL;
    mmgr_line_put_n(out, text + from, ((to - from) < fits) ? (to - from) : fits);
}

// One request answered: its fields cut at their 0s, and the command run as this program runs one from the command
// line, `imp [--root tree] <menu> <command> [words]`, in a process of its own. Where a window is open on the host that
// process sends the command through its door; where none is, impero runs it here as the terminal can, or opens the
// window to run it. The answer's mark is the exit code's, its text what the process printed, cut to what a frame of
// the answer's carries, and written as a frame to `pipe_handle`
EMBED_INLINE void relay_answered(const SessionsOpCtx *args)
{
    const char *const body = (const char *)args->bytes_read;
    // the program's own path, a root word and its folder, the fields, and the NULL a run ends at
    static const char *s_word[CLI_WORDS_MAX + CLI_REACH_HEAD_FIELDS + 4u];
    static char s_program[CLI_PATH_MAX_LEN];
    unsigned int fields = 0u;
    unsigned long long at = 0ULL;
    const char *id = "0";
    unsigned int words = 0u;
    s_word[words] = s_program;
    words += 1u;
    if (args->root != NULL)
    {
        s_word[words] = "--root";
        s_word[words + 1u] = args->root;
        words += 2u;
    }
    while ((fields < CLI_WORDS_MAX + CLI_REACH_HEAD_FIELDS) && (at < args->size))
    {
        if (fields == 0u)
        {
            id = body + at;
        }
        else
        {
            s_word[words] = body + at;
            words += 1u;
        }
        fields += 1u;
        at += cellul.len(&(CatenaFinitaCfg){.src = body + at, .cap = args->size - at}) + 1ULL;
    }
    s_word[words] = NULL;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int framed = (fields >= CLI_REACH_HEAD_FIELDS) && (at <= args->size) && (body[args->size - 1ULL] == '\0');
    CliRunResult *run_result = NULL;
    int ran = 0;
    if (!framed)
    {
        mmgr_line_put(&err_msg, "the request's fields are not whole");
    }
    else if (!EMBED_CALL(cli_path_capacity_internal.program_own, CliPathCapacityCtx, .path_out = s_program,
                         .capacity_bytes = sizeof(s_program)))
    {
        mmgr_line_put(&err_msg, "impero finds no path of its own to run");
    }
    else
    {
        run_result = cli_run_result_alloc();
        ran = (run_result != NULL) &&
              EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = s_word, .folder = NULL, .settings = NULL,
                         .count = 0u, .run_result = run_result, .err_msg = &err_msg);
    }
    const int done = ran && (run_result->code == 0LL);
    static char s_reply[SESSIONS_ANSWER];
    VerbaLinea reply = {s_reply, sizeof(s_reply), 0ULL};
    mmgr_line_put(&reply, id);
    mmgr_line_ch(&reply, '\0');
    mmgr_line_ch(&reply, done ? CLI_REACH_DONE : CLI_REACH_REFUSED);
    if (ran)
    {
        // what it printed: its output where it ran well, else its errors, or its output where the errors are empty
        const int errors = !done && (run_result->err_size != 0ULL);
        EMBED_CALL(relay_text_put, SessionsOpCtx, .out = &reply,
                   .bytes_read = (const unsigned char *)(errors ? run_result->err : run_result->out),
                   .size = errors ? run_result->err_size : run_result->out_size);
    }
    else
    {
        EMBED_CALL(relay_text_put, SessionsOpCtx, .out = &reply, .bytes_read = (const unsigned char *)why,
                   .size = err_msg.at);
    }
    if (run_result != NULL)
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    }
    (void)EMBED_CALL(sessions_frame_write, SessionsOpCtx, .pipe_handle = args->pipe_handle,
                     .bytes_read = (const unsigned char *)s_reply, .size = mmgr_line_ok(&reply) ? reply.at : 0ULL);
}

int cli_sessions_relay(const CliSessionsCfg *args)
{
    static unsigned char s_body[CLI_SESSIONS_FRAME];
    const unsigned long long in = cli_standard_input();
    const unsigned long long out = cli_standard_output();
    for (;;)
    {
        unsigned char head[SESSIONS_HEAD];
        if (!EMBED_CALL(sessions_read_whole, SessionsOpCtx, .pipe_handle = in, .bytes = head, .size = SESSIONS_HEAD))
        {
            return 0;
        }
        const unsigned long long size = EMBED_CALL(sessions_head_read, SessionsOpCtx, .bytes_read = head);
        if ((size == 0ULL) || (size > sizeof(s_body)))
        {
            // a frame no request fits is the end of what this far end reads
            return 0;
        }
        if (!EMBED_CALL(sessions_read_whole, SessionsOpCtx, .pipe_handle = in, .bytes = s_body, .size = size))
        {
            return 0;
        }
        EMBED_CALL(relay_answered, SessionsOpCtx, .root = args->root, .pipe_handle = out, .bytes_read = s_body,
                   .size = size);
    }
}
