// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_send.c: a request sent and its answer read, a redirect followed up to ten times where the request follows
// them, a 303, or a 301 or 302 of a POST, going on as a GET with no body; the request at a line of a file sent and its
// answer stored; and a GET for orior's own reading. Two rooms contain what a request sends and reads, a third request
// waiting for one
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

// the most requests sent at once, each in a room that is a cell of its own; how many are, their lock, and the wake of
// a request waiting for one to end
#define CLI_REQUESTS_ROOMS 2u

static unsigned int s_rooms_used;
static CliWake s_wake = CLI_WAKE_INIT;

CliRequestsRoom *cli_requests_room_take(void)
{
    while (s_rooms_used == CLI_REQUESTS_ROOMS)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
    }
    CliRequestsRoom *room = g_cli_cells->persistent_buf_alloc(sizeof(CliRequestsRoom), &g_cli_requests_cells);
    if (room != NULL)
    {
        room->unpacked = NULL;
        room->unpacked_room = 0ull;
        char *const body_cell = g_cli_cells->temporary_buf_alloc(CLI_REQUESTS_BODY + 1ull, &g_cli_requests_cells);
        char *const raw_cell = g_cli_cells->temporary_buf_alloc(CLI_REQUESTS_BODY + 1ull, &g_cli_requests_cells);
        room->body = mmgr_line_over(body_cell, CLI_REQUESTS_BODY + 1ull);
        room->raw = mmgr_line_over(raw_cell, CLI_REQUESTS_BODY + 1ull);
        if ((body_cell == NULL) || (raw_cell == NULL))
        {
            g_cli_cells->persistent_buf_release(room->raw.out, &g_cli_requests_cells);
            g_cli_cells->persistent_buf_release(room->body.out, &g_cli_requests_cells);
            g_cli_cells->persistent_buf_release(room, &g_cli_requests_cells);
            room = NULL;
        }
    }
    s_rooms_used += (room != NULL) ? 1u : 0u;
    return room;
}

void cli_requests_room_free(const CliRequestsSizeErrMsgCtx *args)
{
    if (args->room == NULL)
    {
        return;
    }
    g_cli_cells->persistent_buf_release(args->room->unpacked, &g_cli_requests_cells);
    // what a request sent and what came back may carry a secret, and the pool leaves a cell as it is
    mmgr_zero_buf(args->room->raw.out, args->room->raw.at);
    mmgr_zero_buf(args->room->body.out, args->room->body.at);
    g_cli_cells->persistent_buf_release(args->room->raw.out, &g_cli_requests_cells);
    g_cli_cells->persistent_buf_release(args->room->body.out, &g_cli_requests_cells);
    mmgr_zero_buf(args->room, sizeof(*args->room));
    g_cli_cells->persistent_buf_release(args->room, &g_cli_requests_cells);
    s_rooms_used -= 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
}

typedef struct
{
    CliRequestsRoom *room;
    const char *failed;
    unsigned long long size;
    const CliRequestsSent *sent;
} RequestsSendCtx;
// the answer made one that reports what failed, at the hop's address
EMBED_INLINE void send_fail(const RequestsSendCtx *args)
{
    unsigned long long size = args->size;
    CliRequestsAnswer *const answer = &args->room->answer;
    answer->at = cli_wall_milliseconds();
    memor.cpy(&(MemoriaCfg){.dst = answer->url, .src = args->room->hop.url, .bytes = sizeof(answer->url)});
    answer->status = 0u;
    answer->reason.size = 0ull;
    answer->version.size = 0ull;
    answer->headers = 0u;
    answer->head_used = 0ull;
    answer->body = (const unsigned char *)args->room->raw.out;
    answer->size = 0ull;
    answer->cut = 0;
    answer->binary = 0;
    memor.set(&(MemoriaCfg){.dst = &answer->times, .val = 0, .bytes = sizeof(answer->times)});
    const unsigned long long copy_len = (size < sizeof(answer->failed)) ? size : (sizeof(answer->failed) - 1u);
    memor.cpy(&(MemoriaCfg){.dst = answer->failed, .src = args->failed, .bytes = (size_t)copy_len});
    answer->failed[copy_len] = '\0';
    answer->has_failed = 1;
}

// whether the hop goes on to where the answer's Location specifies, and the hop made the one that goes there where it
// does
EMBED_INLINE int send_redirect(const RequestsSendCtx *args)
{
    CliRequestsAnswer *const answer = &args->room->answer;
    const unsigned int status = answer->status;
    const CliRequestsText *const location = EMBED_CALL(cli_requests_name_answer_internal.header,
                                                       CliRequestsNameAnswerCtx, .answer = answer, .name = "Location");
    const int moved = (status == 301u) || (status == 302u) || (status == 303u) || (status == 307u) || (status == 308u);
    if (!args->sent->redirect || answer->has_failed || !moved || (answer->redirects == CLI_REQUESTS_REDIRECTS) ||
        (location == NULL))
    {
        return 0;
    }
    char next_url[CLI_REQUESTS_ADDRESS];
    VerbaLinea next = {next_url, sizeof(next_url), 0ull};
    EMBED_CALL(cli_requests_out_base_internal.join, CliRequestsOutBaseCtx, .base_text = args->room->hop.url,
               .location = answer->head + location->at, .out = &next);
    if (mmgr_line_finish(&next) == 0ull)
    {
        return 0;
    }
    answer->redirect_status[answer->redirects] = status;
    memor.cpy(&(MemoriaCfg){.dst = answer->redirect_url[answer->redirects],
                            .src = args->room->hop.url,
                            .bytes = sizeof(args->room->hop.url)});
    answer->redirects += 1u;
    memor.cpy(&(MemoriaCfg){.dst = args->room->hop.url, .src = next_url, .bytes = sizeof(args->room->hop.url)});
    if ((status == 303u) ||
        (((status == 301u) || (status == 302u)) &&
         (cellul.eq(&(CatenaFinitaCfg){.src = args->room->hop.method, .other = "POST", .cap = CLI_CELLS_POOL}))))
    {
        memor.cpy(&(MemoriaCfg){.dst = args->room->hop.method, .src = "GET", .bytes = 4u});
        args->room->hop.body_size = 0ull;
        unsigned int header_count = 0u;
        for (unsigned int at = 0u; at < args->room->hop.headers; at += 1u)
        {
            const char *const name = args->room->hop.header_name[at];
            const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
            const int dropped = ((size == 12u) && EMBED_CALL(cli_requests_text_size_needle_internal.contains,
                                                             CliRequestsTextSizeNeedleCtx, .text = name, .size = size,
                                                             .needle = "content-type")) ||
                                ((size == 14u) && EMBED_CALL(cli_requests_text_size_needle_internal.contains,
                                                             CliRequestsTextSizeNeedleCtx, .text = name, .size = size,
                                                             .needle = "content-length"));
            if (!dropped && (header_count != at))
            {
                memor.cpy(&(MemoriaCfg){.dst = args->room->hop.header_name[header_count],
                                        .src = args->room->hop.header_name[at],
                                        .bytes = sizeof(args->room->hop.header_name[at])});
                memor.cpy(&(MemoriaCfg){.dst = args->room->hop.header_value[header_count],
                                        .src = args->room->hop.header_value[at],
                                        .bytes = sizeof(args->room->hop.header_value[at])});
            }
            header_count += dropped ? 0u : 1u;
        }
        args->room->hop.headers = header_count;
    }
    return 1;
}

void cli_requests_send(const CliRequestsSentCtx *args)
{
    const unsigned long long began = cli_clock_microseconds();
    if (&args->room->hop != args->sent)
    {
        memor.cpy(&(MemoriaCfg){.dst = &args->room->hop, .src = args->sent, .bytes = sizeof(args->room->hop)});
    }
    args->room->answer.redirects = 0u;
    do
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        args->room->answer.has_failed = 0;
        if (!EMBED_CALL(cli_requests_err_msg_room_internal.once, CliRequestsErrMsgRoomCtx, .room = args->room,
                        .verify = args->verify, .err_msg = &err_msg))
        {
            EMBED_CALL(send_fail, RequestsSendCtx, .room = args->room, .failed = why, .size = err_msg.at);
        }
    } while (EMBED_CALL(send_redirect, RequestsSendCtx, .sent = args->sent, .room = args->room));
    args->room->answer.times.total = (double)((cli_clock_microseconds() - began + 50u) / 100u) / 10.0;
}

int cli_requests_sent_at(const CliRequestsTextCfg *args)
{
    CliRequestsRoom *const room = cli_requests_room_take();
    if (room == NULL)
    {
        mmgr_line_put(args->err_msg, "the request's room could not be taken");
        return 0;
    }
    char key[CLI_REQUESTS_ADDRESS + CLI_REQUESTS_NAME];
    VerbaLinea key_line = {key, sizeof(key), 0ull};
    int sent = EMBED_CALL(cli_requests_text_internal.ready, CliRequestsTextCtx, .root = args->root, .path = args->path,
                          .text = args->text, .size = args->size, .line_u64 = args->line, .env = args->env,
                          .sent_out = &room->sent, .body = &room->body, .key = &key_line, .err_msg = args->err_msg);
    if (sent && !mmgr_line_ok(&key_line))
    {
        mmgr_line_put(args->err_msg, "the request is larger than orior_cli supports");
        sent = 0;
    }
    if (sent)
    {
        EMBED_CALL(cli_requests_sent_internal.send, CliRequestsSentCtx, .sent = &room->sent,
                   .verify = EMBED_CALL(cli_requests_err_msg_root_internal.verifies, CliRequestsErrMsgRootCtx,
                                        .root = args->root, .path = args->path, .env = args->env),
                   .room = room);
        sent = EMBED_CALL(cli_requests_err_msg_internal.history_save, CliRequestsErrMsgCtx, .root = args->root,
                          .path = args->path, .key_text = key, .key_size = key_line.at, .answer = &room->answer,
                          .sent = &room->sent, .err_msg = args->err_msg);
    }
    if (sent)
    {
        EMBED_CALL(cli_requests_out_internal.answer_write, CliRequestsOutCtx, .answer = &room->answer,
                   .sent = &room->sent, .most = 0xffffffffffffffffull, .out = args->out);
    }
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = room);
    return sent;
}

int cli_requests_get(const CliRequestsAddressCfg *args)
{
    unsigned int *status = args->status;
    int *cut = args->cut;
    VerbaLinea *body = args->body;
    CliRequestsRoom *const room = cli_requests_room_take();
    if (room == NULL)
    {
        mmgr_line_put(args->err_msg, "the request's room could not be taken");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = &room->sent, .val = 0, .bytes = sizeof(room->sent)});
    memor.cpy(&(MemoriaCfg){.dst = room->sent.method, .src = "GET", .bytes = 4u});
    VerbaLinea url = {room->sent.url, sizeof(room->sent.url), 0ull};
    mmgr_line_put(&url, args->address);
    room->body.at = 0ull;
    room->sent.body = room->body.out;
    room->sent.redirect = 1;
    const int fits = (mmgr_line_finish(&url) != 0ull);
    if (fits)
    {
        EMBED_CALL(cli_requests_sent_internal.send, CliRequestsSentCtx, .sent = &room->sent, .verify = 1, .room = room);
    }
    const CliRequestsAnswer *const answer = &room->answer;
    const int answered = fits && !answer->has_failed;
    if (!fits)
    {
        mmgr_line_put(args->err_msg, "the request is larger than orior_cli supports");
    }
    else if (!answered)
    {
        mmgr_line_put(args->err_msg, answer->failed);
    }
    else
    {
        *status = answer->status;
        *cut = answer->cut;
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->reason,
                   .text_bytes = (const unsigned char *)answer->head + answer->reason.at,
                   .length = answer->reason.size);
        if (answer->binary)
        {
            mmgr_line_put_n(body, (const char *)answer->body, answer->size);
        }
        else
        {
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = body, .text_bytes = answer->body,
                       .length = answer->size);
        }
    }
    EMBED_CALL(cli_requests_size_err_msg_internal.room_free, CliRequestsSizeErrMsgCtx, .room = room);
    return answered;
}
