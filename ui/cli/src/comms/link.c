// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link.c: the window's one link to a tree on another machine: made on a thread of its own and made again as it
// drops, sooner at first and then every 30 s; the server's answers handed to the calls waiting on them by id and its
// events handed on; and how it stands reported as the event "link"
#include "../command_line/callback_cfg.h"
#include "../servers/lsp.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "link_internal.h"

CliLink g_cli_link;
char *g_cli_link_line;
static char s_state_json[CLI_LINK_STANDING];
// the link's state, and the server's input, which a writer takes first
CliLock g_cli_link_lock = CLI_LOCK_INIT;
CliLock g_cli_link_writing = CLI_LOCK_INIT;
CliWake g_cli_link_changed = CLI_WAKE_INIT;

typedef struct
{
    const char *state;
    const char *detail;
    unsigned long long size;
    VerbaLinea *out;
    const char *line;
    unsigned long long *id;
    CliLinkCall *call;
    int whole;
} LinkCtx;
// the link's state as `{state, machine, address, message}` written into `out`
EMBED_INLINE void link_stand(const LinkCtx *args)
{
    VerbaLinea *out = args->out;
    char text[CLI_PATH_MAX_LEN * 3u];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_put(out, "{\"state\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->state,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->state, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"machine\":");
    EMBED_CALL(cli_link_out.machine, CliLinkOutCfg, .address = &g_cli_link.address, .out = &line);
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = text,
               .length = line.at);
    mmgr_line_put(out, ",\"address\":");
    line.at = 0ull;
    EMBED_CALL(cli_link_out.address_text, CliLinkOutCfg, .address = &g_cli_link.address, .out = &line);
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = text,
               .length = line.at);
    mmgr_line_put(out, ",\"message\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)args->detail, .length = args->size);
    mmgr_line_ch(out, '}');
}

// the link's state reported and stored as how it stands; only the store's thread reports
void cli_link_state_set(const CliLinkStateCtx *args)
{
    VerbaLinea state_json = {s_state_json, sizeof(s_state_json), 0ull};
    EMBED_CALL(link_stand, LinkCtx, .state = args->state, .detail = args->detail, .size = args->size,
               .out = &state_json);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_link_lock);
    memor.cpy(&(MemoriaCfg){.dst = g_cli_link.standing, .src = s_state_json, .bytes = (size_t)state_json.at});
    g_cli_link.standing_size = state_json.at;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_link_lock);
    g_cli_link.emit(g_cli_link.context, "link", s_state_json, state_json.at);
}

// what preparing the server reports, reported as the link's state
void cli_link_state(const CliLinkStatedCfg *args)
{
    const char *state = args->state;
    const char *detail = args->detail;
    EMBED_CALL(cli_link_state_internal.state_set, CliLinkStateCtx, .state = state, .detail = detail,
               .size = cellul.len(&(CatenaFinitaCfg){.src = detail, .cap = CLI_CELLS_POOL}));
}

// what the transport prints on its errors read until it ends, its last CLI_LINK_STDERR_TAIL_LEN bytes retained
void cli_link_errors_read(void *context)
{
    (void)context;
    char piece[4096];
    for (;;)
    {
        const unsigned long long count =
            EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx,
                       .pipe_handle = g_cli_link.spawned.err, .piece = piece, .room = sizeof(piece));
        if (count == 0ull)
        {
            break;
        }
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_link_lock);
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            if (g_cli_link.stderr_tail_size == CLI_LINK_STDERR_TAIL_LEN)
            {
                unsigned long long cut = 1ull;
                while ((cut < g_cli_link.stderr_tail_size) &&
                       (((unsigned char)g_cli_link.stderr_tail[cut] & 0xc0u) == 0x80u))
                {
                    cut += 1ull;
                }
                memor.move_down(&(MemoriaCfg){.dst = g_cli_link.stderr_tail,
                                              .src = g_cli_link.stderr_tail + cut,
                                              .bytes = (size_t)(g_cli_link.stderr_tail_size - cut)});
                g_cli_link.stderr_tail_size -= cut;
            }
            g_cli_link.stderr_tail[g_cli_link.stderr_tail_size] = piece[at];
            g_cli_link.stderr_tail_size += 1ull;
        }
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_link_lock);
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_link_lock);
    g_cli_link.reading_errors = 0;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &g_cli_link_changed);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_link_lock);
}

// the end of what the transport printed, trimmed, into `out`
void cli_link_stderr_write(const CliLinkCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_link_lock);
    unsigned long long start = 0ull;
    unsigned long long end = g_cli_link.stderr_tail_size;
    start = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = g_cli_link.stderr_tail,
                       .at = start, .size = end, .set = CLI_TEXT_WHITE);
    end = start + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg,
                             .text = g_cli_link.stderr_tail + start, .at = end - start, .set = CLI_TEXT_WHITE);
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)g_cli_link.stderr_tail + start, .length = end - start);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_link_lock);
}

// the next line the server wrote, its line's end taken off, into g_cli_link_line, `*size` bytes, `*whole` 0 where it
// was longer than g_cli_link_line fits: 1, or 0 at the output's end
int cli_link_line(const CliLinkSizeWholeCtx *args)
{
    unsigned long long *size = args->size_pointer;
    int *whole = args->whole;
    *size = 0ull;
    *whole = 1;
    int any = 0;
    for (;;)
    {
        if (g_cli_link.piece_at == g_cli_link.piece_end)
        {
            const unsigned long long count = EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx,
                                                        .pipe_handle = g_cli_link.spawned.out,
                                                        .piece = g_cli_link.piece, .room = sizeof(g_cli_link.piece));
            if (count == 0ull)
            {
                return any;
            }
            g_cli_link.piece_at = 0ull;
            g_cli_link.piece_end = count;
        }
        any = 1;
        const char *const from = g_cli_link.piece + g_cli_link.piece_at;
        const char *const newline = memor.chr(
            &(MemoriaCfg){.src = from, .val = '\n', .bytes = (size_t)(g_cli_link.piece_end - g_cli_link.piece_at)});
        const unsigned long long take =
            (newline != NULL) ? (unsigned long long)(newline - from) : (g_cli_link.piece_end - g_cli_link.piece_at);
        const unsigned long long fits = (take < CLI_LINK_LINE - *size) ? take : (CLI_LINK_LINE - *size);
        memor.cpy(&(MemoriaCfg){.dst = g_cli_link_line + *size, .src = from, .bytes = (size_t)fits});
        *size += fits;
        *whole = *whole && (fits == take);
        g_cli_link.piece_at += take;
        if (newline != NULL)
        {
            g_cli_link.piece_at += 1u;
            *size -= (*whole && (*size > 0u) && (g_cli_link_line[*size - 1u] == '\r')) ? 1u : 0u;
            return 1;
        }
    }
}

// the id a line gives as an unsigned integer into `id`: 1, or 0 where it gives none
EMBED_INLINE int link_id(const LinkCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long *id = args->id;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    if (!EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->line, .size_value = size, .name = "id",
                    .start = &start, .length_pointer = &length) ||
        (length == 0u))
    {
        return 0;
    }
    *id = 0ull;
    for (unsigned long long at = start; at < start + length; at += 1u)
    {
        const char letter = args->line[at];
        if ((letter < '0') || (letter > '9') ||
            (*id > (0xffffffffffffffffull - (unsigned long long)(letter - '0')) / 10u))
        {
            return 0;
        }
        *id = (*id * 10u) + (unsigned long long)(letter - '0');
    }
    return (length == 1u) || (args->line[start] != '0');
}

// the call waiting on `id` answered by the line, the lock acquired: its answer, or its failure
EMBED_INLINE void link_answer(const LinkCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    char error[CLI_ERR_MSG_MAX_LEN];
    args->call->done = 2;
    if (!args->whole)
    {
        mmgr_line_put(args->call->err_msg, "the answer is larger than orior_cli supports");
    }
    else if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->line, .size_value = size, .name = "error",
                        .start = &start, .length_pointer = &length))
    {
        const int text = EMBED_CALL(cli_lsp_room.string, CliLspRoomCfg, .text = args->line + start, .length = length,
                                    .out_room = error, .room = sizeof(error));
        mmgr_line_put(args->call->err_msg, text ? error : "the call failed");
    }
    else
    {
        args->call->done = 1;
        if (!EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->line, .size_value = size, .name = "ok",
                        .start = &start, .length_pointer = &length) ||
            !EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = args->line + start, .size = length,
                        .out = args->call->out))
        {
            mmgr_line_put(args->call->out, "null");
        }
    }
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &g_cli_link_changed);
}

// a line the server wrote handed on: an event to the link's teller, an answer to the call waiting on its id
void cli_link_recv(const CliLinkSizeCtx *args)
{
    VerbaLinea none = {NULL, 0ull, 0ull};
    if (args->whole_value && !EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = g_cli_link_line,
                                         .size = args->size, .out = &none))
    {
        return;
    }
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    char event[CLI_NAME_MAX_LEN];
    if (args->whole_value &&
        EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = g_cli_link_line, .size_value = args->size,
                   .name = "event", .start = &start, .length_pointer = &length) &&
        EMBED_CALL(cli_lsp_room.string, CliLspRoomCfg, .text = g_cli_link_line + start, .length = length,
                   .out_room = event, .room = sizeof(event)))
    {
        const int body =
            EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = g_cli_link_line, .size_value = args->size,
                       .name = "body", .start = &start, .length_pointer = &length);
        g_cli_link.emit(g_cli_link.context, event, body ? (g_cli_link_line + start) : "null", body ? length : 4u);
        return;
    }
    unsigned long long id = 0ull;
    if (!EMBED_CALL(link_id, LinkCtx, .line = g_cli_link_line, .size = args->size, .id = &id))
    {
        return;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_link_lock);
    for (unsigned int at = 0u; at < CLI_LINK_CALLS; at += 1u)
    {
        CliLinkCall *const call = &g_cli_link.calls[at];
        if (call->used && !call->done && (call->id == id))
        {
            EMBED_CALL(link_answer, LinkCtx, .call = call, .line = g_cli_link_line, .size = args->size,
                       .whole = args->whole_value);
            break;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_link_lock);
}

void cli_link_path_word(const CliLinkOutPathCfg *args)
{
    char slashed[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = slashed,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) + 1u});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = slashed,
               .size = cellul.len(&(CatenaFinitaCfg){.src = slashed, .cap = CLI_CELLS_POOL}), .from = '\\', .to = '/');
    EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = slashed,
               .size = cellul.len(&(CatenaFinitaCfg){.src = slashed, .cap = CLI_CELLS_POOL}), .out = args->out);
}
