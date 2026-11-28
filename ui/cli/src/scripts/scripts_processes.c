// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scripts_processes.c: the programs that a plugin asks orior to start (scripts_internal.h). The host gives the plugin a
// stand-in of each program at once, under a key of the host's own, and sends `process.start`. orior checks the start
// against the plugin's right `starts`, and the reader answers that right as each other right. orior then starts the
// program at the tree's top. Each byte to and from it goes through orior. Its output and its errors go to the host as
// the notification `orior/process` {key, stream, data} in base64, and its end as {key, exit}. `process.write` writes
// to its input, `process.close` closes its input, and `process.kill` ends its group. A program of a host that ends
// ends with it
#include "../command_line/pool.h"
#include "../servers/lsp.h"
#include "../text/base64.h"
#include "../text/json_tree.h"
#include "scripts_internal.h"

// The max programs that run for the plugins at once, and the max words of one program. Then the max bytes of one read
// of a program's output, which one notification carries
#define CLI_SCRIPTS_PROCESSES 16u
#define CLI_SCRIPTS_PROCESS_WORDS 64u
#define CLI_SCRIPTS_PROCESS_PIECE 32768u

typedef struct ScriptsProcess ScriptsProcess;

// one stream of a program that a reader reads: the program, and 1 for its errors or 0 for its output
typedef struct
{
    ScriptsProcess *process;
    unsigned int errors;
} ScriptsStream;

// One program that runs for a plugin. It holds whether it holds its place and whether it ended, under the lock of the
// record. Then it holds the plugin, the host that it reports to and the host's key of it, and the program itself.
// Last come its two readers, their streams, and how many readers still read
struct ScriptsProcess
{
    int used;
    int ended;
    CliGate gate;
    char id[CLI_NAME_MAX_LEN];
    CliLsp *host;
    unsigned long long key;
    PlatformProgram spawned;
    CliThread readers[2];
    ScriptsStream streams[2];
    unsigned int reading;
};

static ScriptsProcess s_processes[CLI_SCRIPTS_PROCESSES];
static CliGate s_processes_gate = CLI_GATE_INIT;

typedef struct
{
    const CliJson *json;
    unsigned int args;
    unsigned int index;
    CliLsp *host;
    unsigned long long key;
    ScriptsProcess *process;
    const char *text;
    VerbaLinea *answer;
} ScriptsProcessesCtx;

// an answer of the error `text`, as the host rejects a call's promise with it
EMBED_INLINE void processes_refused(const ScriptsProcessesCtx *args)
{
    mmgr_line_put(args->answer, "{\"oriorError\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->answer,
               .text_bytes = (const unsigned char *)args->text,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_ERR_MSG_MAX_LEN}));
    mmgr_line_put(args->answer, "}");
}

// the node at the place `index` of the call's args, CLI_JSON_NONE where the call has no such place
EMBED_INLINE unsigned int processes_arg(const ScriptsProcessesCtx *args)
{
    return EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = args->json,
                      .array = args->args, .index = args->index);
}

// the host's key at the place `index` of the call's args into `key`: 1, or 0 where it is no whole number. The tree
// keeps a number's text as the JSON writes it, ended by a 0
static int processes_key(const ScriptsProcessesCtx *args, unsigned long long *key)
{
    const unsigned int node =
        EMBED_CALL(processes_arg, ScriptsProcessesCtx, .json = args->json, .args = args->args, .index = args->index);
    const int number = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = args->json,
                                  .node = node, .type = CFG_JSON_NUMBER);
    const char *const text = number ? args->json->text + args->json->node[node].text : NULL;
    const char *end = text;
    *key = number ? cellul.to_ulong_in(&(TransfiguroCfg){.src = text, .end = &end, .base = 10u}) : 0ULL;
    return number && (end == text + args->json->node[node].length);
}

// The program of the host `host` under the key `key`, its lock taken, NULL where none runs. The caller opens the
// lock of a program that the call gives
EMBED_INLINE ScriptsProcess *processes_found(const ScriptsProcessesCtx *args)
{
    ScriptsProcess *found = NULL;
    cli_gate_shut(&s_processes_gate);
    for (unsigned int at = 0u; (found == NULL) && (at < CLI_SCRIPTS_PROCESSES); at += 1u)
    {
        found = (s_processes[at].used && (s_processes[at].host == args->host) && (s_processes[at].key == args->key))
                    ? &s_processes[at]
                    : NULL;
    }
    if (found != NULL)
    {
        cli_gate_shut(&found->gate);
    }
    cli_gate_opened(&s_processes_gate);
    if ((found != NULL) && found->ended)
    {
        cli_gate_opened(&found->gate);
        found = NULL;
    }
    return found;
}

// one notification `orior/process` to the program's host, `text` its params
EMBED_INLINE void processes_told(const ScriptsProcessesCtx *args)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unsent = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_lsp_err_msg_length.notify, CliLspErrMsgLengthCfg, .lsp = args->process->host,
                     .method = "orior/process", .params = args->text,
                     .length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}),
                     .err_msg = &unsent);
}

// The last reader of a program: its end waited on, the end sent to its host, the program let go and its place given
// back
static void processes_ended(ScriptsProcess *process)
{
    long long code = 0LL;
    code = EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &process->spawned, .code = &code);
    char text[96];
    VerbaLinea ended = {text, sizeof(text), 0ULL};
    mmgr_line_put(&ended, "{\"key\":");
    mmgr_line_decimal(&ended, process->key, 1u);
    mmgr_line_put(&ended, ",\"exit\":");
    if (code < 0LL)
    {
        mmgr_line_put(&ended, "null");
    }
    else
    {
        // an exit code that is not negative converts exactly
        mmgr_line_decimal(&ended, (unsigned long long)code, 1u);
    }
    mmgr_line_put(&ended, "}");
    if (mmgr_line_finish(&ended) != 0ULL)
    {
        EMBED_CALL(processes_told, ScriptsProcessesCtx, .process = process, .text = text);
    }
    cli_gate_shut(&process->gate);
    process->ended = 1;
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &process->spawned);
    cli_gate_opened(&process->gate);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = process->host);
    cli_gate_shut(&s_processes_gate);
    process->used = 0;
    cli_gate_opened(&s_processes_gate);
}

// One reader of a program: each piece of its stream sent to the host in base64 as it comes, until the stream ends.
// The last reader to end ends the program
static void processes_read(void *context)
{
    const ScriptsStream *const stream = (const ScriptsStream *)context;
    ScriptsProcess *const process = stream->process;
    const unsigned long long pipe = stream->errors ? process->spawned.err : process->spawned.out;
    // the cells are temporary: the reader takes them as it starts and gives them back as it ends
    char *const piece = g_platform_guard->temporary_buf_alloc(CLI_SCRIPTS_PROCESS_PIECE, g_cli_scripts_owner);
    const unsigned long long capacity = ((CLI_SCRIPTS_PROCESS_PIECE / 3ULL) + 1ULL) * 4ULL + 96ULL;
    char *const text = g_platform_guard->temporary_buf_alloc(capacity, g_cli_scripts_owner);
    unsigned long long got = ((piece != NULL) && (text != NULL))
                                 ? EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                                              .pipe_handle = pipe, .piece = piece,
                                              .capacity = CLI_SCRIPTS_PROCESS_PIECE)
                                 : 0ULL;
    while (got != 0ULL)
    {
        VerbaLinea told = mmgr_line_over(text, capacity);
        mmgr_line_put(&told, "{\"key\":");
        mmgr_line_decimal(&told, process->key, 1u);
        mmgr_line_put(&told, stream->errors ? ",\"stream\":\"stderr\",\"data\":\"" : ",\"stream\":\"stdout\",\"data\":\"");
        EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = &told, .bytes = (const unsigned char *)piece,
                   .size = got);
        mmgr_line_put(&told, "\"}");
        if (mmgr_line_finish(&told) != 0ULL)
        {
            EMBED_CALL(processes_told, ScriptsProcessesCtx, .process = process, .text = text);
        }
        got = EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx, .pipe_handle = pipe,
                         .piece = piece, .capacity = CLI_SCRIPTS_PROCESS_PIECE);
    }
    g_platform_guard->persistent_buf_release(text, g_cli_scripts_owner);
    g_platform_guard->persistent_buf_release(piece, g_cli_scripts_owner);
    cli_gate_shut(&process->gate);
    process->reading -= 1u;
    const int last = process->reading == 0u;
    cli_gate_opened(&process->gate);
    if (last)
    {
        processes_ended(process);
    }
}

void cli_scripts_process_start(const CliScriptsProcessCtx *args)
{
    unsigned long long key = 0ULL;
    const char *const program = EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
        .node = EMBED_CALL(processes_arg, ScriptsProcessesCtx, .json = args->json, .args = args->args, .index = 0u),
        .length_pointer = NULL);
    const unsigned int listed =
        EMBED_CALL(processes_arg, ScriptsProcessesCtx, .json = args->json, .args = args->args, .index = 1u);
    if ((program == NULL) || (args->root == NULL) ||
        !processes_key(&(ScriptsProcessesCtx){.json = args->json, .args = args->args, .index = 3u}, &key))
    {
        EMBED_CALL(processes_refused, ScriptsProcessesCtx, .answer = args->answer,
                   .text = (args->root == NULL) ? "no tree is open" : "the start names no program or no key");
        return;
    }
    // the program, then each word of the list, then the NULL that ends the words
    const char *words[CLI_SCRIPTS_PROCESS_WORDS + 2u];
    words[0] = program;
    unsigned int count = 1u;
    unsigned int node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                   .json_read = args->json, .array = listed, .index = 0u);
    while ((node != CLI_JSON_NONE) && (count <= CLI_SCRIPTS_PROCESS_WORDS))
    {
        words[count] = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                  .json_read = args->json, .node = node, .length_pointer = NULL);
        if (words[count] == NULL)
        {
            EMBED_CALL(processes_refused, ScriptsProcessesCtx, .answer = args->answer,
                       .text = "a word of the start is no string");
            return;
        }
        node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = args->json,
                          .array = listed, .index = count);
        count += 1u;
    }
    if (node != CLI_JSON_NONE)
    {
        EMBED_CALL(processes_refused, ScriptsProcessesCtx, .answer = args->answer,
                   .text = "the start has more words than orior takes");
        return;
    }
    words[count] = NULL;
    ScriptsProcess *process = NULL;
    cli_gate_shut(&s_processes_gate);
    for (unsigned int at = 0u; (process == NULL) && (at < CLI_SCRIPTS_PROCESSES); at += 1u)
    {
        process = s_processes[at].used ? NULL : &s_processes[at];
    }
    if (process != NULL)
    {
        process->used = 1;
        process->ended = 0;
        process->gate = (CliGate)CLI_GATE_INIT;
    }
    cli_gate_opened(&s_processes_gate);
    if (process == NULL)
    {
        EMBED_CALL(processes_refused, ScriptsProcessesCtx, .answer = args->answer,
                   .text = "no more programs run for the plugins at once");
        return;
    }
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = process->id, .src = args->id, .cap = CLI_NAME_MAX_LEN});
    process->host = args->lsp;
    process->key = key;
    memor.set(&(MemoriaCfg){.dst = &process->spawned, .val = 0, .bytes = sizeof(process->spawned)});
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn_feed, CliWordsErrMsgCtx, .spawned = &process->spawned,
                    .words = words, .folder = args->root, .detached = 1, .err_msg = &err_msg))
    {
        cli_gate_shut(&s_processes_gate);
        process->used = 0;
        cli_gate_opened(&s_processes_gate);
        (void)mmgr_line_finish(&err_msg);
        EMBED_CALL(processes_refused, ScriptsProcessesCtx, .answer = args->answer, .text = why);
        return;
    }
    EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = args->lsp);
    process->reading = 2u;
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        process->streams[at] = (ScriptsStream){process, at};
        process->readers[at] = (CliThread){processes_read, &process->streams[at], 0u};
        if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &process->readers[at]))
        {
            // a stream with no reader ends the program, and the reader that started ends it in turn
            EMBED_CALL(cli_cols_internal.spawn_group_kill, CliColsCtx, .spawned_read = &process->spawned);
            cli_gate_shut(&process->gate);
            process->reading -= 1u;
            const int last = process->reading == 0u;
            cli_gate_opened(&process->gate);
            if (last)
            {
                processes_ended(process);
            }
        }
    }
    mmgr_line_put(args->answer, "null");
}

void cli_scripts_process_write(const CliScriptsProcessCtx *args)
{
    unsigned long long key = 0ULL;
    unsigned long long length = 0ULL;
    const char *const data = EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
        .node = EMBED_CALL(processes_arg, ScriptsProcessesCtx, .json = args->json, .args = args->args, .index = 1u),
        .length_pointer = &length);
    if ((data == NULL) || !processes_key(&(ScriptsProcessesCtx){.json = args->json, .args = args->args}, &key))
    {
        EMBED_CALL(processes_refused, ScriptsProcessesCtx, .answer = args->answer,
                   .text = "the write names no key or no bytes");
        return;
    }
    const unsigned long long capacity = ((length / 4ULL) + 1ULL) * 3ULL;
    unsigned char *const bytes = g_platform_guard->temporary_buf_alloc(capacity, g_cli_scripts_owner);
    unsigned long long size = 0ULL;
    const int decoded = (bytes != NULL) && EMBED_CALL(cli_base64_text.unbase64, CliBase64TextCfg, .text = data,
                                                      .length = length, .out_bytes = bytes, .capacity = capacity,
                                                      .size_pointer = &size);
    ScriptsProcess *const process =
        decoded ? EMBED_CALL(processes_found, ScriptsProcessesCtx, .host = args->lsp, .key = key) : NULL;
    const int written =
        (process != NULL) && EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx,
                                        .pipe_handle = process->spawned.in, .bytes = bytes, .size = size);
    if (process != NULL)
    {
        cli_gate_opened(&process->gate);
    }
    g_platform_guard->persistent_buf_release(bytes, g_cli_scripts_owner);
    if (written)
    {
        mmgr_line_put(args->answer, "null");
        return;
    }
    EMBED_CALL(processes_refused, ScriptsProcessesCtx, .answer = args->answer,
               .text = !decoded ? "the bytes are no base64" : "the program does not take the bytes");
}

// the program of the call's key: its input closed where `kill` is 0, else its whole group ended. null, or the reason
static void processes_handled(const CliScriptsProcessCtx *args, int kill)
{
    unsigned long long key = 0ULL;
    ScriptsProcess *const process =
        processes_key(&(ScriptsProcessesCtx){.json = args->json, .args = args->args}, &key)
            ? EMBED_CALL(processes_found, ScriptsProcessesCtx, .host = args->lsp, .key = key)
            : NULL;
    if (process == NULL)
    {
        EMBED_CALL(processes_refused, ScriptsProcessesCtx, .answer = args->answer, .text = "no such program runs");
        return;
    }
    if (kill)
    {
        EMBED_CALL(cli_cols_internal.spawn_group_kill, CliColsCtx, .spawned_read = &process->spawned);
    }
    else
    {
        EMBED_CALL(cli_spawned_internal.spawn_input_close, CliSpawnedCtx, .spawned = &process->spawned);
    }
    cli_gate_opened(&process->gate);
    mmgr_line_put(args->answer, "null");
}

void cli_scripts_process_close(const CliScriptsProcessCtx *args)
{
    processes_handled(args, 0);
}

void cli_scripts_process_kill(const CliScriptsProcessCtx *args)
{
    processes_handled(args, 1);
}

void cli_scripts_processes_end(const CliLsp *host)
{
    for (unsigned int at = 0u; at < CLI_SCRIPTS_PROCESSES; at += 1u)
    {
        cli_gate_shut(&s_processes_gate);
        ScriptsProcess *const process =
            (s_processes[at].used && (s_processes[at].host == host)) ? &s_processes[at] : NULL;
        if (process != NULL)
        {
            cli_gate_shut(&process->gate);
        }
        cli_gate_opened(&s_processes_gate);
        if (process != NULL)
        {
            if (!process->ended)
            {
                EMBED_CALL(cli_cols_internal.spawn_group_kill, CliColsCtx, .spawned_read = &process->spawned);
            }
            cli_gate_opened(&process->gate);
        }
    }
}
