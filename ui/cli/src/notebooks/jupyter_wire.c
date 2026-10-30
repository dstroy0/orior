// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_wire.c: Jupyter's messages on the wire: the key a kernel signs with, the ids messages and connection files
// are made with, a header's time, a message's frames signed and sent whole in one write, and frames read back as the
// page reads a message. The shell is half duplex, as a kernel serves it: one request is in flight on it at a time, and
// one made while another is in flight is queued, signed and whole, and sent as the reply to the one before it comes
#include "../command_line/file_writing.h"
#include "../text/digest.h"
#include "jupyter_internal.h"

// the mark between a message's routing frames and its own, and the frames of its own: the signature, the header, the
// parent's header, the metadata and the content; and the bytes the shell's queue is begun in, doubling as it fills
#define CLI_KERNEL_MARK "<IDS|MSG>"
#define CLI_KERNEL_PARTS 5u
#define CLI_KERNEL_QUEUE_FIRST 4096u

static CliLock s_next_lock = CLI_LOCK_INIT;
static unsigned long long s_next = 1ull;

void cli_kernel_key_made(char *key)
{
    unsigned char bytes[32];
    if (!cli_random_bytes(bytes, sizeof(bytes)))
    {
        // where the system gives no randomness, the clocks, this process and a number of its own are hashed
        const unsigned long long seed[4] = {cli_wall_milliseconds(), cli_clock_milliseconds(), cli_process_own(),
                                            cli_kernel_next()};
        cli_sha256_bytes(seed, sizeof(seed), bytes);
    }
    VerbaLinea line = {key, 65ull, 0ull};
    static const char s_hex[17] = "0123456789abcdef";
    for (unsigned int at = 0u; at < sizeof(bytes); at += 1u)
    {
        mmgr_line_ch(&line, s_hex[bytes[at] >> 4u]);
        mmgr_line_ch(&line, s_hex[bytes[at] & 0x0fu]);
    }
    mmgr_line_finish(&line);
}

unsigned long long cli_kernel_next(void)
{
    cli_lock_take(&s_next_lock);
    const unsigned long long next = s_next;
    s_next += 1ull;
    cli_lock_give(&s_next_lock);
    return next;
}

void cli_kernel_id_made(const char *session, char *id)
{
    VerbaLinea line = {id, CLI_KERNEL_ID, 0ull};
    mmgr_line_put(&line, session);
    mmgr_line_ch(&line, '_');
    mmgr_line_decimal(&line, cli_kernel_next(), 1u);
    mmgr_line_finish(&line);
}

void cli_kernel_now_written(VerbaLinea *out)
{
    const unsigned long long milliseconds = cli_wall_milliseconds();
    const unsigned long long seconds = milliseconds / 1000ull;
    // the civil date of a day count from 1970, by eras of 400 years, as Howard Hinnant's days_from_civil inverts it
    const unsigned long long shifted = (seconds / 86400ull) + 719468ull;
    const unsigned long long era = shifted / 146097ull;
    const unsigned long long of_era = shifted - (era * 146097ull);
    const unsigned long long year_of_era =
        (of_era - (of_era / 1460ull) + (of_era / 36524ull) - (of_era / 146096ull)) / 365ull;
    const unsigned long long day_of_year =
        of_era - ((365ull * year_of_era) + (year_of_era / 4ull) - (year_of_era / 100ull));
    const unsigned long long month_index = ((5ull * day_of_year) + 2ull) / 153ull;
    const unsigned long long day = day_of_year - (((153ull * month_index) + 2ull) / 5ull) + 1ull;
    const unsigned long long month = (month_index < 10ull) ? month_index + 3ull : month_index - 9ull;
    const unsigned long long year = year_of_era + (era * 400ull) + ((month <= 2ull) ? 1ull : 0ull);
    const unsigned long long rest = seconds % 86400ull;
    mmgr_line_decimal(out, year, 4u);
    mmgr_line_ch(out, '-');
    mmgr_line_decimal(out, month, 2u);
    mmgr_line_ch(out, '-');
    mmgr_line_decimal(out, day, 2u);
    mmgr_line_ch(out, 'T');
    mmgr_line_decimal(out, rest / 3600ull, 2u);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, (rest % 3600ull) / 60ull, 2u);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, rest % 60ull, 2u);
    mmgr_line_ch(out, '.');
    // the wall clock reads milliseconds, written to the microsecond as Jupyter's headers are
    mmgr_line_decimal(out, (milliseconds % 1000ull) * 1000ull, 6u);
    mmgr_line_ch(out, 'Z');
}

// the HMAC by SHA-256 of `count` frames taken one after another, with the kernel's key, as RFC 2104 sets it out, in
// lowercase hex into `hex`, sized for 65
static void jupyter_signed(const char *key, const CliZmtpFrame *parts, unsigned int count, char *hex)
{
    unsigned char block[64];
    memor.set(&(MemoriaCfg){.dst = block, .val = 0, .bytes = sizeof(block)});
    const unsigned long long key_size = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
    if (key_size > sizeof(block))
    {
        cli_sha256_bytes(key, key_size, block);
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = block, .src = key, .bytes = key_size});
    }
    unsigned char pad[64];
    unsigned char inner[32];
    unsigned char outer[32];
    CliSha256 sha;
    for (unsigned int at = 0u; at < sizeof(pad); at += 1u)
    {
        pad[at] = (unsigned char)(block[at] ^ 0x36u);
    }
    cli_sha256_begun(&sha);
    cli_sha256_taken(&sha, pad, sizeof(pad));
    for (unsigned int part = 0u; part < count; part += 1u)
    {
        cli_sha256_taken(&sha, parts[part].bytes, parts[part].size);
    }
    cli_sha256_ended(&sha, inner);
    for (unsigned int at = 0u; at < sizeof(pad); at += 1u)
    {
        pad[at] = (unsigned char)(block[at] ^ 0x5cu);
    }
    cli_sha256_begun(&sha);
    cli_sha256_taken(&sha, pad, sizeof(pad));
    cli_sha256_taken(&sha, inner, sizeof(inner));
    cli_sha256_ended(&sha, outer);
    static const char s_hex[17] = "0123456789abcdef";
    for (unsigned int at = 0u; at < sizeof(outer); at += 1u)
    {
        hex[at * 2u] = s_hex[outer[at] >> 4u];
        hex[(at * 2u) + 1u] = s_hex[outer[at] & 0x0fu];
    }
    hex[64] = '\0';
}

// the request `id`, `size` bytes of `message` whole, put after those queued on the shell, under the shell's lock: 1, or
// 0 where the cells cannot meet it
static int jupyter_shell_queued(CliKernel *kernel, const char *id, const unsigned char *message,
                                unsigned long long size)
{
    if ((kernel->shell_queue.out == NULL) &&
        !cli_growing_line_made(&kernel->shell_queue, CLI_KERNEL_QUEUE_FIRST, &g_cli_notebook_cells))
    {
        return 0;
    }
    char named[CLI_KERNEL_ID];
    memor.set(&(MemoriaCfg){.dst = named, .val = 0, .bytes = sizeof(named)});
    VerbaLinea name = {named, sizeof(named), 0ull};
    mmgr_line_put(&name, id);
    mmgr_line_put_n(&kernel->shell_queue, named, sizeof(named));
    mmgr_line_put_n(&kernel->shell_queue, (const char *)&size, sizeof(size));
    // a message is below the cells' pool, which a size_t counts
    mmgr_line_put_n(&kernel->shell_queue, (const char *)message, (size_t)size);
    return mmgr_line_ok(&kernel->shell_queue);
}

void cli_kernel_shell_answered(CliKernel *kernel, const char *parent)
{
    cli_lock_take(&kernel->shell_lock);
    if ((kernel->shell_flight[0] != '\0') &&
        (cellul.eq(&(CatenaFinitaCfg){.src = kernel->shell_flight, .other = parent, .cap = CLI_CELLS_POOL})))
    {
        kernel->shell_flight[0] = '\0';
        // the next queued request sent, one the connection refuses passed over for the one after it
        while ((kernel->shell_flight[0] == '\0') && (kernel->shell_queue_from < kernel->shell_queue.at))
        {
            const char *const named = kernel->shell_queue.out + kernel->shell_queue_from;
            unsigned long long size = 0ull;
            memor.cpy(&(MemoriaCfg){.dst = &size, .src = named + CLI_KERNEL_ID, .bytes = sizeof(size)});
            const unsigned char *const message = (const unsigned char *)(named + CLI_KERNEL_ID + sizeof(size));
            if (cli_socket_written(&kernel->shell, message, size))
            {
                memor.cpy(&(MemoriaCfg){.dst = kernel->shell_flight, .src = named, .bytes = CLI_KERNEL_ID});
            }
            kernel->shell_queue_from += CLI_KERNEL_ID + sizeof(size) + size;
        }
        if (kernel->shell_queue_from >= kernel->shell_queue.at)
        {
            kernel->shell_queue_from = 0ull;
            kernel->shell_queue.at = 0u;
        }
    }
    cli_lock_give(&kernel->shell_lock);
}

int cli_kernel_frames_sent(CliKernel *kernel, const CliSocket *socket, CliLock *lock, const char *id,
                           const char *msg_type, const char *content, unsigned long long length)
{
    char header[2048];
    VerbaLinea line = {header, sizeof(header), 0ull};
    mmgr_line_put(&line, "{\"msg_id\":");
    cli_json_quoted(&line, id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&line, ",\"session\":");
    cli_json_quoted(&line, kernel->session,
                    cellul.len(&(CatenaFinitaCfg){.src = kernel->session, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&line, ",\"username\":\"orior\",\"date\":\"");
    cli_kernel_now_written(&line);
    mmgr_line_put(&line, "\",\"msg_type\":");
    cli_json_quoted(&line, msg_type, cellul.len(&(CatenaFinitaCfg){.src = msg_type, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&line, ",\"version\":\"5.3\"}");
    if (!mmgr_line_ok(&line))
    {
        return 0;
    }
    CliZmtpFrame frames[2u + 4u] = {
        {(const unsigned char *)CLI_KERNEL_MARK, sizeof(CLI_KERNEL_MARK) - 1ull},
        {NULL, 64ull},
        {(const unsigned char *)header, line.at},
        {(const unsigned char *)"{}", 2ull},
        {(const unsigned char *)"{}", 2ull},
        {(const unsigned char *)content, length},
    };
    char signature[72];
    jupyter_signed(kernel->key, frames + 2, 4u, signature);
    frames[1].bytes = (const unsigned char *)signature;
    unsigned long long size = 0ull;
    unsigned char *const message = cli_zmtp_message_made(frames, 6u, &size);
    if (message == NULL)
    {
        return 0;
    }
    const int shell = socket == &kernel->shell;
    cli_lock_take(lock);
    const int now = !shell || (kernel->shell_flight[0] == '\0');
    const int sent = now ? cli_socket_written(socket, message, size) : jupyter_shell_queued(kernel, id, message, size);
    if (shell && now && sent)
    {
        VerbaLinea flight = {kernel->shell_flight, sizeof(kernel->shell_flight), 0ull};
        mmgr_line_put(&flight, id);
        (void)mmgr_line_finish(&flight);
    }
    cli_lock_give(lock);
    cli_cell_release(message, &g_cli_notebook_cells);
    return sent;
}

// the string member `name` of a header `size` bytes long, read into the reader's room, written as JSON into `out`, ""
// where it has none, and into `copy`, sized for `room`, where it is given and fits: 1, or 0 where the header is no
// JSON
static int jupyter_header_member(CliKernelReader *reader, const CliZmtpFrame *header, const char *name, VerbaLinea *out,
                                 char *copy, unsigned long long room)
{
    CliJson json;
    cli_json_begun(&json, reader->nodes, CLI_KERNEL_HEADER_NODES, reader->text, CLI_KERNEL_HEADER_TEXT);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const char *const text = (const char *)header->bytes;
    const unsigned int root =
        cli_utf8_whole(text, header->size)
            ? cli_json_read(&json, text, header->size, reader->tokens, CLI_KERNEL_HEADER_NODES, &unread)
            : CLI_JSON_NONE;
    if (root == CLI_JSON_NONE)
    {
        return 0;
    }
    unsigned long long length = 0ull;
    const char *const value = cli_json_string(&json, cli_json_member(&json, root, name), &length);
    cli_json_quoted(out, (value != NULL) ? value : "", (value != NULL) ? length : 0ull);
    if (copy != NULL)
    {
        const int fits = (value != NULL) && (length < room) &&
                         (memor.chr(&(MemoriaCfg){.src = value, .val = '\0', .bytes = length}) == NULL);
        memor.cpy(&(MemoriaCfg){.dst = copy, .src = fits ? value : "", .bytes = fits ? length : 0ull});
        copy[fits ? length : 0ull] = '\0';
    }
    return 1;
}

int cli_kernel_message_written(CliKernelReader *reader, const CliZmtpFrame *frames, unsigned int count, VerbaLinea *out,
                               char *parent)
{
    const unsigned long long mark = sizeof(CLI_KERNEL_MARK) - 1ull;
    unsigned int at = 0u;
    while ((at < count) &&
           !((frames[at].size == mark) &&
             (memor.cmp(&(MemoriaCfg){.src = frames[at].bytes, .other = CLI_KERNEL_MARK, .bytes = mark}) == 0)))
    {
        at += 1u;
    }
    if ((at >= count) || (count - at - 1u < CLI_KERNEL_PARTS))
    {
        return 0;
    }
    const CliZmtpFrame *const parts = frames + at + 1u;
    const char *const key = reader->kernel->key;
    if (key[0] != '\0')
    {
        char signature[72];
        jupyter_signed(key, parts + 1, 4u, signature);
        if ((parts[0].size != 64ull) ||
            (memor.cmp(&(MemoriaCfg){.src = parts[0].bytes, .other = signature, .bytes = 64u}) != 0))
        {
            return 0;
        }
    }
    const unsigned long long from = out->at;
    mmgr_line_put(out, "{\"channel\":");
    cli_json_quoted(out, reader->channel,
                    cellul.len(&(CatenaFinitaCfg){.src = reader->channel, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"msg_type\":");
    const int header = jupyter_header_member(reader, &parts[1], "msg_type", out, NULL, 0ull);
    mmgr_line_put(out, ",\"msg_id\":");
    const int id = header && jupyter_header_member(reader, &parts[1], "msg_id", out, NULL, 0ull);
    mmgr_line_put(out, ",\"parent\":");
    const int parented = id && jupyter_header_member(reader, &parts[2], "msg_id", out, parent, CLI_KERNEL_ID);
    mmgr_line_put(out, ",\"content\":");
    const int whole = parented && cli_utf8_whole((const char *)parts[4].bytes, parts[4].size) &&
                      cli_json_compacted((const char *)parts[4].bytes, parts[4].size, out);
    mmgr_line_ch(out, '}');
    if (!whole || !mmgr_line_ok(out))
    {
        out->at = from;
        return 0;
    }
    return 1;
}
