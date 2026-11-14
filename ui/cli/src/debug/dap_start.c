// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dap_start.c: a debug adapter begun: a program started for the session, or one listening at an address connected to;
// one that runs in impero is begun by dap_within.c
#include "../command_line/callback_cfg.h"
#include "../machine/net.h"
#include "dap_internal.h"

// how long an adapter listening at an address has to take the connection, in milliseconds
#define CLI_DAP_CONNECTING 5000u

// `size` bytes written whole to the adapter program's input, or to the adapter's connection
static int start_pipe(const CliSinkBytesSizeCfg *args)
{
    const void *bytes = args->bytes;
    unsigned long long size = args->size;
    return EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx,
                      .pipe_handle = ((CliDap *)args->sink)->spawned.in, .bytes = bytes, .size = size);
}

static int start_send(const CliSinkBytesSizeCfg *args)
{
    const void *bytes = args->bytes;
    unsigned long long size = args->size;
    return EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &((CliDap *)args->sink)->socket,
                      .bytes = bytes, .size = size);
}

// a piece of what the adapter program writes, or of what comes over the adapter's connection, read
static unsigned long long start_piped_read(const CliSourcePieceCapacityCfg *args)
{
    char *piece = args->piece;
    unsigned long long capacity = args->capacity;
    return EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                      .pipe_handle = ((CliDap *)args->source)->spawned.out, .piece = piece, .capacity = capacity);
}

static unsigned long long start_receive(const CliSourcePieceCapacityCfg *args)
{
    unsigned long long capacity = args->capacity;
    const long long count =
        EMBED_CALL(cli_capacity_internal.socket_some, CliCapacityCtx, .sock = &((CliDap *)args->source)->socket,
                   .bytes_out = args->piece, .capacity = capacity);
    return (count > 0LL) ? (unsigned long long)count : 0ULL;
}

typedef struct
{
    CliDap *dap;
    unsigned long long (*read)(const CliSourcePieceCapacityCfg *args);
} DapStartCtx;
// an adapter's reader made to read from `read`
EMBED_INLINE void start_reader(const DapStartCtx *args)
{
    unsigned long long (*read)(const CliSourcePieceCapacityCfg *) = args->read;
    args->dap->reader.read = read;
    args->dap->reader.source = args->dap;
    args->dap->reader.start = 0ULL;
    args->dap->reader.end = 0ULL;
}

CliDap *cli_dap_start(const CliDapProgramCfg *args)
{
    unsigned int count = args->count;
    int ends_program = args->ends_program;
    CliDapEventFn emit = args->emit;
    void *context = args->context;
    CliDap *const dap = cli_dap_position_alloc();
    if (dap == NULL)
    {
        mmgr_line_put(args->err_msg, "no more debug adapters are open at once");
        return NULL;
    }
    dap->type = CLI_DAP_PROGRAM;
    dap->ends_program = ends_program;
    dap->emit = emit;
    dap->context = context;
    dap->words.count = 0u;
    dap->words.used = 0u;
    dap->words.full = 0;
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &dap->words,
               .word_text = args->program);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &dap->words,
                   .word_text = args->request[at]);
    }
    memor.cpy(&(MemoriaCfg){.dst = dap->path,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    const CliSetting settings[1] = {{"PATH", dap->path}};
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn_feed, CliWordsErrMsgCtx, .spawned = &dap->spawned,
                    .words = dap->words.word_text, .folder = args->folder, .settings = settings, .count = 1u,
                    .detached = 1, .err_msg = args->err_msg))
    {
        dap->reserved = 0;
        return NULL;
    }
    dap->input = (CliLspWriter){start_pipe, dap};
    EMBED_CALL(start_reader, DapStartCtx, .dap = dap, .read = start_piped_read);
    EMBED_CALL(cli_dap_internal.readers_start, CliDapCtx, .dap = dap, .complaints = 1);
    return dap;
}

CliDap *cli_dap_connect(const CliDapEndsProgramCfg *args)
{
    int ends_program = args->ends_program;
    CliDapEventFn emit = args->emit;
    void *context = args->context;
    char address[CLI_PATH_MAX_LEN];
    VerbaLinea address_line = {address, sizeof(address), 0ULL};
    mmgr_line_put(&address_line, args->host);
    mmgr_line_ch(&address_line, ':');
    mmgr_line_decimal(&address_line, args->port, 1u);
    mmgr_line_finish(&address_line);
    if (!EMBED_CALL(cli_net_address.reach, CliNetAddressCfg, .address = address, .purpose = "a debug adapter",
                    .err_msg = args->err_msg))
    {
        return NULL;
    }
    CliDap *const dap = cli_dap_position_alloc();
    if (dap == NULL)
    {
        mmgr_line_put(args->err_msg, "no more debug adapters are open at once");
        return NULL;
    }
    if (!EMBED_CALL(cli_err_msg_internal.socket_connected_within, CliErrMsgCtx, .sock_out = &dap->socket,
                    .host = args->host, .port = args->port, .milliseconds = CLI_DAP_CONNECTING,
                    .err_msg = args->err_msg))
    {
        dap->reserved = 0;
        return NULL;
    }
    dap->type = CLI_DAP_SOCKET;
    dap->ends_program = ends_program;
    dap->emit = emit;
    dap->context = context;
    dap->input = (CliLspWriter){start_send, dap};
    EMBED_CALL(start_reader, DapStartCtx, .dap = dap, .read = start_receive);
    EMBED_CALL(cli_dap_internal.readers_start, CliDapCtx, .dap = dap, .complaints = 0);
    return dap;
}
