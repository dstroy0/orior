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
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;

CliRequestsRoom *cli_requests_room_taken(void)
{
    cli_lock_take(&s_lock);
    while (s_rooms_used == CLI_REQUESTS_ROOMS)
    {
        cli_wake_wait(&s_wake, &s_lock);
    }
    CliRequestsRoom *room = g_cli_cells->persistent_buf_alloc(sizeof(CliRequestsRoom), NULL);
    if (room != NULL)
    {
        room->unpacked = NULL;
        room->unpacked_room = 0ull;
        const int made = cli_growing_line_made(&room->body, CLI_REQUESTS_BODY_FIRST) &&
                         cli_growing_line_made(&room->raw, CLI_REQUESTS_BODY_FIRST);
        if (!made)
        {
            g_cli_cells->persistent_buf_release(room->raw.out, NULL);
            g_cli_cells->persistent_buf_release(room->body.out, NULL);
            g_cli_cells->persistent_buf_release(room, NULL);
            room = NULL;
        }
    }
    s_rooms_used += (room != NULL) ? 1u : 0u;
    cli_lock_give(&s_lock);
    return room;
}

void cli_requests_room_given(CliRequestsRoom *room)
{
    if (room == NULL)
    {
        return;
    }
    g_cli_cells->persistent_buf_release(room->unpacked, NULL);
    g_cli_cells->persistent_buf_release(room->raw.out, NULL);
    g_cli_cells->persistent_buf_release(room->body.out, NULL);
    g_cli_cells->persistent_buf_release(room, NULL);
    cli_lock_take(&s_lock);
    s_rooms_used -= 1u;
    cli_wake_all(&s_wake);
    cli_lock_give(&s_lock);
}

// the answer made one that reports what failed, at the hop's address
static void send_failed(CliRequestsRoom *room, const char *failed, unsigned long long size)
{
    CliRequestsAnswer *const answer = &room->answer;
    answer->at = cli_wall_milliseconds();
    memor.cpy(&(MemoriaCfg){.dst = answer->url, .src = room->hop.url, .bytes = sizeof(answer->url)});
    answer->status = 0u;
    answer->reason.size = 0ull;
    answer->version.size = 0ull;
    answer->headers = 0u;
    answer->head_used = 0ull;
    answer->body = (const unsigned char *)room->raw.out;
    answer->size = 0ull;
    answer->cut = 0;
    answer->binary = 0;
    memor.set(&(MemoriaCfg){.dst = &answer->times, .val = 0, .bytes = sizeof(answer->times)});
    const unsigned long long copy_len = (size < sizeof(answer->failed)) ? size : (sizeof(answer->failed) - 1u);
    memor.cpy(&(MemoriaCfg){.dst = answer->failed, .src = failed, .bytes = (size_t)copy_len});
    answer->failed[copy_len] = '\0';
    answer->has_failed = 1;
}

// whether the hop goes on to where the answer's Location specifies, and the hop made the one that goes there where it
// does
static int send_redirected(const CliRequestsSent *sent, CliRequestsRoom *room)
{
    CliRequestsAnswer *const answer = &room->answer;
    const unsigned int status = answer->status;
    const CliRequestsText *const location = cli_requests_header(answer, "Location");
    const int moved = (status == 301u) || (status == 302u) || (status == 303u) || (status == 307u) || (status == 308u);
    if (!sent->redirect || answer->has_failed || !moved || (answer->redirects == CLI_REQUESTS_REDIRECTS) ||
        (location == NULL))
    {
        return 0;
    }
    char next_url[CLI_REQUESTS_ADDRESS];
    VerbaLinea next = {next_url, sizeof(next_url), 0ull};
    cli_requests_joined(room->hop.url, answer->head + location->at, &next);
    if (mmgr_line_finish(&next) == 0ull)
    {
        return 0;
    }
    answer->redirect_status[answer->redirects] = status;
    memor.cpy(&(MemoriaCfg){
        .dst = answer->redirect_url[answer->redirects], .src = room->hop.url, .bytes = sizeof(room->hop.url)});
    answer->redirects += 1u;
    memor.cpy(&(MemoriaCfg){.dst = room->hop.url, .src = next_url, .bytes = sizeof(room->hop.url)});
    if ((status == 303u) ||
        (((status == 301u) || (status == 302u)) &&
         (cellul.eq(&(CatenaFinitaCfg){.src = room->hop.method, .other = "POST", .cap = CLI_CELLS_POOL}))))
    {
        memor.cpy(&(MemoriaCfg){.dst = room->hop.method, .src = "GET", .bytes = 4u});
        room->hop.body_size = 0ull;
        unsigned int header_count = 0u;
        for (unsigned int at = 0u; at < room->hop.headers; at += 1u)
        {
            const char *const name = room->hop.header_name[at];
            const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
            const int dropped = ((size == 12u) && cli_requests_contains(name, size, "content-type")) ||
                                ((size == 14u) && cli_requests_contains(name, size, "content-length"));
            if (!dropped && (header_count != at))
            {
                memor.cpy(&(MemoriaCfg){.dst = room->hop.header_name[header_count],
                                        .src = room->hop.header_name[at],
                                        .bytes = sizeof(room->hop.header_name[at])});
                memor.cpy(&(MemoriaCfg){.dst = room->hop.header_value[header_count],
                                        .src = room->hop.header_value[at],
                                        .bytes = sizeof(room->hop.header_value[at])});
            }
            header_count += dropped ? 0u : 1u;
        }
        room->hop.headers = header_count;
    }
    return 1;
}

void cli_requests_send(const CliRequestsSent *sent, int verify, CliRequestsRoom *room)
{
    const unsigned long long began = cli_clock_microseconds();
    if (&room->hop != sent)
    {
        memor.cpy(&(MemoriaCfg){.dst = &room->hop, .src = sent, .bytes = sizeof(room->hop)});
    }
    room->answer.redirects = 0u;
    do
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        room->answer.has_failed = 0;
        if (!cli_requests_once(room, verify, &err_msg))
        {
            send_failed(room, why, err_msg.at);
        }
    } while (send_redirected(sent, room));
    room->answer.times.total = (double)((cli_clock_microseconds() - began + 50u) / 100u) / 10.0;
}

int cli_requests_sent_at(const char *root, const char *path, const char *text, unsigned long long size,
                         unsigned long long line, const char *env, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliRequestsRoom *const room = cli_requests_room_taken();
    if (room == NULL)
    {
        mmgr_line_put(err_msg, "the request's room could not be taken");
        return 0;
    }
    char key[CLI_REQUESTS_ADDRESS + CLI_REQUESTS_NAME];
    VerbaLinea key_line = {key, sizeof(key), 0ull};
    int sent = cli_requests_ready(root, path, text, size, line, env, &room->sent, &room->body, &key_line, err_msg);
    if (sent && !mmgr_line_ok(&key_line))
    {
        mmgr_line_put(err_msg, "the request is larger than orior_cli supports");
        sent = 0;
    }
    if (sent)
    {
        cli_requests_send(&room->sent, cli_requests_verifies(root, path, env), room);
        sent = cli_requests_history_save(root, path, key, key_line.at, &room->answer, &room->sent, err_msg);
    }
    if (sent)
    {
        cli_requests_answer_written(&room->answer, &room->sent, 0xffffffffffffffffull, out);
    }
    cli_requests_room_given(room);
    return sent;
}

int cli_requests_get(const char *address, unsigned int *status, VerbaLinea *reason, int *cut, VerbaLinea *body,
                     VerbaLinea *err_msg)
{
    CliRequestsRoom *const room = cli_requests_room_taken();
    if (room == NULL)
    {
        mmgr_line_put(err_msg, "the request's room could not be taken");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = &room->sent, .val = 0, .bytes = sizeof(room->sent)});
    memor.cpy(&(MemoriaCfg){.dst = room->sent.method, .src = "GET", .bytes = 4u});
    VerbaLinea url = {room->sent.url, sizeof(room->sent.url), 0ull};
    mmgr_line_put(&url, address);
    room->body.at = 0ull;
    room->sent.body = room->body.out;
    room->sent.redirect = 1;
    const int fits = (mmgr_line_finish(&url) != 0ull);
    if (fits)
    {
        cli_requests_send(&room->sent, 1, room);
    }
    const CliRequestsAnswer *const answer = &room->answer;
    const int answered = fits && !answer->has_failed;
    if (!fits)
    {
        mmgr_line_put(err_msg, "the request is larger than orior_cli supports");
    }
    else if (!answered)
    {
        mmgr_line_put(err_msg, answer->failed);
    }
    else
    {
        *status = answer->status;
        *cut = answer->cut;
        cli_lossy(reason, (const unsigned char *)answer->head + answer->reason.at, answer->reason.size);
        if (answer->binary)
        {
            mmgr_line_put_n(body, (const char *)answer->body, answer->size);
        }
        else
        {
            cli_lossy(body, answer->body, answer->size);
        }
    }
    cli_requests_room_given(room);
    return answered;
}
