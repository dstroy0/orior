// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_wire.c: Jupyter's messages on the wire: the key a kernel signs with, the ids messages and connection files
// are made with, a header's time, a message's frames signed and sent whole in one write, and frames read back as the
// page reads a message. The shell is half duplex, as a kernel serves it: one request is in flight on it at a time, and
// one made while another is in flight is queued, signed and whole, and sent as the reply to the one before it comes
#include "../text/digest.h"
#include "jupyter_internal.h"

// the mark between a message's routing frames and its own, and the frames of its own: the signature, the header, the
// parent's header, the metadata and the content; and the bytes the shell's queue is written in
#define CLI_KERNEL_MARK "<IDS|MSG>"
#define CLI_KERNEL_PARTS 5u
#define CLI_KERNEL_QUEUE_CAPACITY (16ULL << 20u)

static unsigned long long s_next = 1ULL;

void cli_kernel_key_init(const CliJupyterKeyCtx *args)
{
    unsigned char bytes[32];
    if (!EMBED_CALL(cli_size_bytes_internal.random_bytes, CliSizeBytesCtx, .bytes_out = bytes, .size = sizeof(bytes)))
    {
        // where the system gives no randomness, the clocks, this process and a number of its own are hashed
        const unsigned long long seed[4] = {cli_wall_milliseconds(), cli_clock_milliseconds(), cli_process_own(),
                                            cli_kernel_next()};
        EMBED_CALL(cli_digest_bytes_size.sha256_bytes, CliDigestBytesSizeCfg, .bytes = seed, .size = sizeof(seed),
                   .digest = bytes);
    }
    VerbaLinea line = {args->key, 65ULL, 0ULL};
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
    const unsigned long long next = s_next;
    s_next += 1ULL;
    return next;
}

void cli_kernel_id_init(const CliJupyterSessionCtx *args)
{
    VerbaLinea line = {args->id, CLI_KERNEL_ID, 0ULL};
    mmgr_line_put(&line, args->session);
    mmgr_line_ch(&line, '_');
    mmgr_line_decimal(&line, cli_kernel_next(), 1u);
    mmgr_line_finish(&line);
}

void cli_kernel_now_write(const CliJupyterOutCtx *args)
{
    const unsigned long long milliseconds = cli_wall_milliseconds();
    const unsigned long long seconds = milliseconds / 1000ULL;
    // the civil date of a day count from 1970, by eras of 400 years, as Howard Hinnant's days_from_civil inverts it
    const unsigned long long shifted = (seconds / 86400ULL) + 719468ULL;
    const unsigned long long era = shifted / 146097ULL;
    const unsigned long long of_era = shifted - (era * 146097ULL);
    const unsigned long long year_of_era =
        (of_era - (of_era / 1460ULL) + (of_era / 36524ULL) - (of_era / 146096ULL)) / 365ULL;
    const unsigned long long day_of_year =
        of_era - ((365ULL * year_of_era) + (year_of_era / 4ULL) - (year_of_era / 100ULL));
    const unsigned long long month_index = ((5ULL * day_of_year) + 2ULL) / 153ULL;
    const unsigned long long day = day_of_year - (((153ULL * month_index) + 2ULL) / 5ULL) + 1ULL;
    const unsigned long long month = (month_index < 10ULL) ? month_index + 3ULL : month_index - 9ULL;
    const unsigned long long year = year_of_era + (era * 400ULL) + ((month <= 2ULL) ? 1ULL : 0ULL);
    const unsigned long long rest = seconds % 86400ULL;
    mmgr_line_decimal(args->out, year, 4u);
    mmgr_line_ch(args->out, '-');
    mmgr_line_decimal(args->out, month, 2u);
    mmgr_line_ch(args->out, '-');
    mmgr_line_decimal(args->out, day, 2u);
    mmgr_line_ch(args->out, 'T');
    mmgr_line_decimal(args->out, rest / 3600ULL, 2u);
    mmgr_line_ch(args->out, ':');
    mmgr_line_decimal(args->out, (rest % 3600ULL) / 60ULL, 2u);
    mmgr_line_ch(args->out, ':');
    mmgr_line_decimal(args->out, rest % 60ULL, 2u);
    mmgr_line_ch(args->out, '.');
    // the wall clock reads milliseconds, written to the microsecond as Jupyter's headers are
    mmgr_line_decimal(args->out, (milliseconds % 1000ULL) * 1000ULL, 6u);
    mmgr_line_ch(args->out, 'Z');
}

typedef struct
{
    const char *key;
    const CliZmtpFrame *parts;
    unsigned int count;
    char *hex;
    CliKernel *kernel;
    const char *id;
    const unsigned char *message;
    unsigned long long size;
    CliKernelReader *reader;
    const CliZmtpFrame *header;
    const char *name;
    VerbaLinea *out;
    char *copy;
    unsigned long long capacity;
} JupyterWireCtx;
// the HMAC by SHA-256 of `count` frames taken one after another, with the kernel's key, as RFC 2104 sets it out, in
// lowercase hex into `hex`, sized for 65
static void jupyter_sign(const JupyterWireCtx *args)
{
    unsigned char block[64];
    memor.set(&(MemoriaCfg){.dst = block, .val = 0, .bytes = sizeof(block)});
    const unsigned long long key_size = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES});
    if (key_size > sizeof(block))
    {
        EMBED_CALL(cli_digest_bytes_size.sha256_bytes, CliDigestBytesSizeCfg, .bytes = args->key, .size = key_size,
                   .digest = block);
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = block, .src = args->key, .bytes = key_size});
    }
    unsigned char pad[64];
    unsigned char inner[32];
    unsigned char outer[32];
    CliSha256 sha;
    for (unsigned int at = 0u; at < sizeof(pad); at += 1u)
    {
        pad[at] = (unsigned char)(block[at] ^ 0x36u);
    }
    EMBED_CALL(cli_digest.sha256_begin, CliDigestCfg, .sha = &sha);
    EMBED_CALL(cli_digest_sha.sha256_alloc, CliDigestShaCfg, .sha = &sha, .bytes = pad, .size = sizeof(pad));
    for (unsigned int part = 0u; part < args->count; part += 1u)
    {
        EMBED_CALL(cli_digest_sha.sha256_alloc, CliDigestShaCfg, .sha = &sha, .bytes = args->parts[part].bytes,
                   .size = args->parts[part].size);
    }
    EMBED_CALL(cli_digest.sha256_end, CliDigestCfg, .sha = &sha, .digest = inner);
    for (unsigned int at = 0u; at < sizeof(pad); at += 1u)
    {
        pad[at] = (unsigned char)(block[at] ^ 0x5cu);
    }
    EMBED_CALL(cli_digest.sha256_begin, CliDigestCfg, .sha = &sha);
    EMBED_CALL(cli_digest_sha.sha256_alloc, CliDigestShaCfg, .sha = &sha, .bytes = pad, .size = sizeof(pad));
    EMBED_CALL(cli_digest_sha.sha256_alloc, CliDigestShaCfg, .sha = &sha, .bytes = inner, .size = sizeof(inner));
    EMBED_CALL(cli_digest.sha256_end, CliDigestCfg, .sha = &sha, .digest = outer);
    static const char s_hex[17] = "0123456789abcdef";
    for (unsigned int at = 0u; at < sizeof(outer); at += 1u)
    {
        args->hex[at * 2u] = s_hex[outer[at] >> 4u];
        args->hex[(at * 2u) + 1u] = s_hex[outer[at] & 0x0fu];
    }
    args->hex[64] = '\0';
}

// the request `id`, `size` bytes of `message` whole, put after those queued on the shell, under the shell's lock: 1, or
// 0 where the cells cannot meet it
EMBED_INLINE int jupyter_shell_queue(const JupyterWireCtx *args)
{
    if (args->kernel->shell_queue.out == NULL)
    {
        char *const queue_cell = g_cli_guard->persistent_buf_alloc(CLI_KERNEL_QUEUE_CAPACITY, g_cli_notebook_owner);
        args->kernel->shell_queue = mmgr_line_over(queue_cell, CLI_KERNEL_QUEUE_CAPACITY);
    }
    char named[CLI_KERNEL_ID];
    memor.set(&(MemoriaCfg){.dst = named, .val = 0, .bytes = sizeof(named)});
    VerbaLinea name = {named, sizeof(named), 0ULL};
    mmgr_line_put(&name, args->id);
    mmgr_line_put_n(&args->kernel->shell_queue, named, sizeof(named));
    mmgr_line_put_n(&args->kernel->shell_queue, (const char *)&args->size, sizeof(args->size));
    // a message is below the cells' pool, which a size_t counts
    mmgr_line_put_n(&args->kernel->shell_queue, (const char *)args->message, (size_t)args->size);
    return mmgr_line_ok(&args->kernel->shell_queue);
}

void cli_kernel_shell_answer(const CliJupyterKernelParentCtx *args)
{
    if ((args->kernel->shell_flight[0] != '\0') &&
        (cellul.eq(
            &(CatenaFinitaCfg){.src = args->kernel->shell_flight, .other = args->parent, .cap = CLI_POOL_BYTES})))
    {
        args->kernel->shell_flight[0] = '\0';
        // the next queued request sent, one the connection refuses passed over for the one after it
        while ((args->kernel->shell_flight[0] == '\0') &&
               (args->kernel->shell_queue_from < args->kernel->shell_queue.at))
        {
            const char *const named = args->kernel->shell_queue.out + args->kernel->shell_queue_from;
            unsigned long long size = 0ULL;
            memor.cpy(&(MemoriaCfg){.dst = &size, .src = named + CLI_KERNEL_ID, .bytes = sizeof(size)});
            const unsigned char *const message = (const unsigned char *)(named + CLI_KERNEL_ID + sizeof(size));
            if (EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &args->kernel->shell,
                           .bytes = message, .size = size))
            {
                memor.cpy(&(MemoriaCfg){.dst = args->kernel->shell_flight, .src = named, .bytes = CLI_KERNEL_ID});
            }
            args->kernel->shell_queue_from += CLI_KERNEL_ID + sizeof(size) + size;
        }
        if (args->kernel->shell_queue_from >= args->kernel->shell_queue.at)
        {
            args->kernel->shell_queue_from = 0ULL;
            args->kernel->shell_queue.at = 0u;
        }
    }
}

int cli_kernel_frames_send(const CliJupyterKernelCtx *args)
{
    char header[2048];
    VerbaLinea line = {header, sizeof(header), 0ULL};
    mmgr_line_put(&line, "{\"msg_id\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = args->id_read,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->id_read, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&line, ",\"session\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line,
               .text = args->kernel->session,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->kernel->session, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&line, ",\"username\":\"orior\",\"date\":\"");
    EMBED_CALL(cli_jupyter_out_internal.kernel_now_write, CliJupyterOutCtx, .out = &line);
    mmgr_line_put(&line, "\",\"msg_type\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = args->msg_type,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->msg_type, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&line, ",\"version\":\"5.3\"}");
    if (!mmgr_line_ok(&line))
    {
        return 0;
    }
    CliZmtpFrame frames[2u + 4u] = {
        {(const unsigned char *)CLI_KERNEL_MARK, sizeof(CLI_KERNEL_MARK) - 1ULL},
        {NULL, 64ULL},
        {(const unsigned char *)header, line.at},
        {(const unsigned char *)"{}", 2ULL},
        {(const unsigned char *)"{}", 2ULL},
        {(const unsigned char *)args->content, args->length},
    };
    char signature[72];
    EMBED_CALL(jupyter_sign, JupyterWireCtx, .key = args->kernel->key, .parts = frames + 2, .count = 4u,
               .hex = signature);
    frames[1].bytes = (const unsigned char *)signature;
    unsigned long long size = 0ULL;
    unsigned char *const message =
        EMBED_CALL(cli_zmtp.message_init, CliZmtpCfg, .frames = frames, .count = 6u, .size = &size);
    if (message == NULL)
    {
        return 0;
    }
    const int shell = args->socket == &args->kernel->shell;
    const int now = !shell || (args->kernel->shell_flight[0] == '\0');
    const int sent = now ? EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = args->socket,
                                      .bytes = message, .size = size)
                         : EMBED_CALL(jupyter_shell_queue, JupyterWireCtx, .kernel = args->kernel, .id = args->id_read,
                                      .message = message, .size = size);
    if (shell && now && sent)
    {
        VerbaLinea flight = {args->kernel->shell_flight, sizeof(args->kernel->shell_flight), 0ULL};
        mmgr_line_put(&flight, args->id_read);
        (void)mmgr_line_finish(&flight);
    }
    g_cli_guard->persistent_buf_release(message, g_cli_notebook_owner);
    return sent;
}

// the string member `name` of a header `size` bytes long, read into the reader's capacity, written as JSON into `out`,
// "" where it has none, and into `copy`, sized for `capacity`, where it is given and fits: 1, or 0 where the header is
// no JSON
EMBED_INLINE int jupyter_header_member(const JupyterWireCtx *args)
{
    const char *name = args->name;
    VerbaLinea *out = args->out;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = args->reader->nodes, .nodes_max = CLI_KERNEL_HEADER_NODES,
               .text_out = args->reader->text, .text_max = CLI_KERNEL_HEADER_TEXT);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    const char *const text = (const char *)args->header->bytes;
    const unsigned int root =
        EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = text, .size = args->header->size)
            ? EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = &json,
                         .text = text, .size = args->header->size, .tokens = args->reader->tokens,
                         .tokens_max = CLI_KERNEL_HEADER_NODES, .err_msg = &unread)
            : CLI_JSON_NONE;
    if (root == CLI_JSON_NONE)
    {
        return 0;
    }
    unsigned long long length = 0ULL;
    const char *const value =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = root, .name = name),
                   .length_pointer = &length);
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
               .text = (value != NULL) ? value : "", .length = length);
    if (args->copy != NULL)
    {
        const int fits = (value != NULL) && (length < args->capacity) &&
                         (memor.chr(&(MemoriaCfg){.src = value, .val = '\0', .bytes = length}) == NULL);
        memor.cpy(&(MemoriaCfg){.dst = args->copy, .src = fits ? value : "", .bytes = fits ? length : 0ULL});
        args->copy[fits ? length : 0ULL] = '\0';
    }
    return 1;
}

int cli_kernel_message_write(const CliJupyterOutCtx *args)
{
    const unsigned long long mark = sizeof(CLI_KERNEL_MARK) - 1ULL;
    unsigned int at = 0u;
    while ((at < args->count) &&
           !((args->frames[at].size == mark) &&
             (memor.cmp(&(MemoriaCfg){.src = args->frames[at].bytes, .other = CLI_KERNEL_MARK, .bytes = mark}) == 0)))
    {
        at += 1u;
    }
    if ((at >= args->count) || (args->count - at - 1u < CLI_KERNEL_PARTS))
    {
        return 0;
    }
    const CliZmtpFrame *const parts = args->frames + at + 1u;
    const char *const key = args->reader->kernel->key;
    if (key[0] != '\0')
    {
        char signature[72];
        EMBED_CALL(jupyter_sign, JupyterWireCtx, .key = key, .parts = parts + 1, .count = 4u, .hex = signature);
        if ((parts[0].size != 64ULL) ||
            (memor.cmp(&(MemoriaCfg){.src = parts[0].bytes, .other = signature, .bytes = 64u}) != 0))
        {
            return 0;
        }
    }
    const unsigned long long from = args->out->at;
    mmgr_line_put(args->out, "{\"channel\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = args->reader->channel,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->reader->channel, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(args->out, ",\"msg_type\":");
    const int header = EMBED_CALL(jupyter_header_member, JupyterWireCtx, .reader = args->reader, .header = &parts[1],
                                  .name = "msg_type", .out = args->out, .copy = NULL, .capacity = 0ULL);
    mmgr_line_put(args->out, ",\"msg_id\":");
    const int id =
        header && EMBED_CALL(jupyter_header_member, JupyterWireCtx, .reader = args->reader, .header = &parts[1],
                             .name = "msg_id", .out = args->out, .copy = NULL, .capacity = 0ULL);
    mmgr_line_put(args->out, ",\"parent\":");
    const int parented =
        id && EMBED_CALL(jupyter_header_member, JupyterWireCtx, .reader = args->reader, .header = &parts[2],
                         .name = "msg_id", .out = args->out, .copy = args->parent_out, .capacity = CLI_KERNEL_ID);
    mmgr_line_put(args->out, ",\"content\":");
    const int whole = parented &&
                      EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg,
                                 .bytes = (const char *)parts[4].bytes, .size = parts[4].size) &&
                      EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg,
                                 .text = (const char *)parts[4].bytes, .size = parts[4].size, .out = args->out);
    mmgr_line_ch(args->out, '}');
    if (!whole || !mmgr_line_ok(args->out))
    {
        args->out->at = from;
        return 0;
    }
    return 1;
}
