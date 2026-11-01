// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp_start.c: a language server started at a tree, its readers started, and requested to initialize with what
// orior_cli can do
#include "../command_line/cells.h"
#include "../text/text.h"
#include "lsp_internal.h"

// how long the server has to answer that it started, in milliseconds
#define CLI_LSP_STARTING 30000ull

// what orior_cli can do, as it informs a server it starts
static const char s_capabilities[] =
    "{\"general\":{\"positionEncodings\":[\"utf-16\"]},"
    "\"textDocument\":{"
    "\"synchronization\":{\"didSave\":false},"
    "\"hover\":{\"contentFormat\":[\"markdown\",\"plaintext\"]},"
    "\"completion\":{\"completionItem\":{\"snippetSupport\":false,\"documentationFormat\":[\"plaintext\"]}},"
    "\"definition\":{\"linkSupport\":false},"
    "\"references\":{},"
    "\"rename\":{\"prepareSupport\":true},"
    "\"callHierarchy\":{},"
    "\"inlayHint\":{},"
    "\"foldingRange\":{\"lineFoldingOnly\":true},"
    "\"semanticTokens\":{\"requests\":{\"full\":true,\"range\":false},"
    "\"tokenTypes\":[\"namespace\",\"type\",\"class\",\"enum\",\"interface\",\"struct\",\"typeParameter\","
    "\"parameter\","
    "\"variable\",\"property\",\"enumMember\",\"event\",\"function\",\"method\",\"macro\",\"keyword\",\"modifier\","
    "\"comment\",\"string\",\"number\",\"regexp\",\"operator\",\"decorator\"],"
    "\"tokenModifiers\":[],\"formats\":[\"relative\"],\"overlappingTokenSupport\":false,"
    "\"multilineTokenSupport\":false},"
    "\"signatureHelp\":{\"signatureInformation\":{\"documentationFormat\":[\"markdown\",\"plaintext\"],"
    "\"parameterInformation\":{\"labelOffsetSupport\":true},\"activeParameterSupport\":true}},"
    "\"codeAction\":{\"codeActionLiteralSupport\":{\"codeActionKind\":{\"valueSet\":[\"\",\"quickfix\",\"refactor\","
    "\"refactor.extract\",\"refactor.inline\",\"refactor.rewrite\",\"source\",\"source.organizeImports\"]}},"
    "\"isPreferredSupport\":true,\"disabledSupport\":true,\"dataSupport\":true,"
    "\"resolveSupport\":{\"properties\":[\"edit\"]}},"
    "\"publishDiagnostics\":{\"relatedInformation\":false,\"codeDescriptionSupport\":true,"
    "\"markupMessageSupport\":true}"
    "},"
    "\"workspace\":{\"applyEdit\":true,\"workspaceEdit\":{\"documentChanges\":true},\"configuration\":true,"
    "\"inlayHint\":{\"refreshSupport\":true},\"semanticTokens\":{\"refreshSupport\":true}},"
    "\"window\":{\"workDoneProgress\":false}}";

// `size` bytes written whole to the server's input
static int lsp_input_write(void *sink, const void *bytes, unsigned long long size)
{
    return cli_spawn_written(((CliLsp *)sink)->spawned.in, bytes, size);
}

// a server's cell taken, referenced once, numbered apart from every other started, and made empty: the server, or NULL
// where the cells cannot meet it
static CliLsp *lsp_place_taken(void)
{
    static unsigned int s_started;
    CliLsp *const lsp = g_cli_cells->persistent_buf_alloc(sizeof(*lsp), &g_cli_servers_cells);
    if (lsp != NULL)
    {
        cli_lock_take(&g_cli_lsps_lock);
        lsp->taken = 1;
        lsp->refs = 1u;
        lsp->number = s_started;
        s_started += 1u;
        cli_lock_give(&g_cli_lsps_lock);
    }
    if (lsp != NULL)
    {
        lsp->ended = 0;
        memor.set(&(MemoriaCfg){.dst = &lsp->spawned, .val = 0, .bytes = sizeof(lsp->spawned)});
        lsp->input_lock = (CliLock)CLI_LOCK_INIT;
        lsp->lock = (CliLock)CLI_LOCK_INIT;
        lsp->wake = (CliWake)CLI_WAKE_INIT;
        lsp->next = 1ull;
        memor.set(&(MemoriaCfg){.dst = lsp->waiters, .val = 0, .bytes = sizeof(lsp->waiters)});
        lsp->complaint_size = 0ull;
        lsp->reading = 0u;
        lsp->capabilities = NULL;
        lsp->capabilities_size = 0ull;
        lsp->input = (CliLspWriter){lsp_input_write, lsp};
    }
    return lsp;
}

// the params of `initialize` written into `out`: this process, the tree by its URI and its folder's name, what orior
// can do, the server's own options, and orior's name
static void lsp_initialize_params(const char *root, const char *options, unsigned long long length, VerbaLinea *out)
{
    char uri[CLI_PATH_MAX_LEN * 3u];
    cli_lsp_uri_of(root, uri, sizeof(uri));
    size_t end = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    end = (size_t)cli_text_back_past(root, end, "/\\");
    size_t start = end;
    start = (size_t)cli_text_back_any(root, start, "/\\");
    mmgr_line_put(out, "{\"processId\":");
    mmgr_line_decimal(out, cli_process_own(), 1u);
    mmgr_line_put(out, ",\"rootUri\":");
    cli_json_quoted(out, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"workspaceFolders\":[{\"uri\":");
    cli_json_quoted(out, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"name\":");
    cli_json_quoted_lossy(out, (const unsigned char *)root + start, end - start);
    mmgr_line_put(out, "}],\"capabilities\":");
    mmgr_line_put(out, s_capabilities);
    mmgr_line_put(out, ",\"initializationOptions\":");
    mmgr_line_put_n(out, (length > 0ull) ? options : "null", (length > 0ull) ? length : 4ull);
    mmgr_line_put(out, ",\"clientInfo\":{\"name\":\"orior\"}}");
}

CliLsp *cli_lsp_started(const char *program, const char *const *args, unsigned int count, const char *root,
                        const char *options, unsigned long long length, const CliSetting *settings,
                        unsigned int setting_count, CliLspRecvFn recv, void *context, VerbaLinea *err_msg)
{
    CliLsp *const lsp = lsp_place_taken();
    if (lsp == NULL)
    {
        mmgr_line_put(err_msg, "no more language servers are run at once");
        return NULL;
    }
    lsp->recv = recv;
    lsp->context = context;
    lsp->words.count = 0u;
    lsp->words.used = 0u;
    lsp->words.full = 0;
    cli_words_put(&lsp->words, program);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_words_put(&lsp->words, args[at]);
    }
    const unsigned int settings_used = (setting_count < 64u) ? setting_count : 64u;
    memor.cpy(&(MemoriaCfg){.dst = lsp->settings, .src = settings, .bytes = settings_used * sizeof(settings[0])});
    if (!cli_spawn_fed(&lsp->spawned, lsp->words.word, root, lsp->settings, settings_used, 1, err_msg))
    {
        g_cli_cells->persistent_buf_release(lsp, &g_cli_servers_cells);
        return NULL;
    }
    cli_lsp_readers_started(lsp);
    // the params in a cell of the most they hold: the tree's URI twice, its folder's name escaped, what orior can do,
    // the server's options and the members around them
    const unsigned long long params_room = (CLI_PATH_MAX_LEN * 12ull) + sizeof(s_capabilities) + length + 256ull;
    char *const params_text = g_cli_cells->temporary_buf_alloc(params_room, &g_cli_servers_cells);
    VerbaLinea params = mmgr_line_over(params_text, params_room);
    lsp_initialize_params(root, options, length, &params);
    CliLspAnswer answer = {NULL, NULL, 0ull};
    const unsigned long long err_from = err_msg->at;
    if (!mmgr_line_ok(&params))
    {
        mmgr_line_put(err_msg, "the server's initialize params could not be written: they do not fit");
    }
    const int initialized = mmgr_line_ok(&params) && cli_lsp_requested(lsp, "initialize", params.out, params.at,
                                                                       CLI_LSP_STARTING, &answer, err_msg);
    g_cli_cells->persistent_buf_release(params_text, &g_cli_servers_cells);
    if (!initialized)
    {
        query_paused(200u);
        char complaint[CLI_LSP_COMPLAINT];
        VerbaLinea complaint_line = {complaint, sizeof(complaint), 0ull};
        cli_lsp_complaint(lsp, &complaint_line);
        if ((complaint_line.at > 0ull) && (err_msg->at > err_from))
        {
            mmgr_line_put(err_msg, ": ");
            mmgr_line_put_n(err_msg, complaint, complaint_line.at);
        }
        cli_lsp_unref(lsp);
        return NULL;
    }
    // what the server reported it can do, in a cell of its size
    unsigned long long at = 0ull;
    unsigned long long size = 0ull;
    const int has = cli_lsp_member(answer.text, answer.size, "capabilities", &at, &size);
    lsp->capabilities_size = has ? size : 4ull;
    lsp->capabilities = g_cli_cells->persistent_buf_alloc(lsp->capabilities_size + 1ull, &g_cli_servers_cells);
    if (lsp->capabilities != NULL)
    {
        // a member of the reply, which a size_t counts
        memor.cpy(&(MemoriaCfg){
            .dst = lsp->capabilities, .src = has ? answer.text + at : "null", .bytes = (size_t)lsp->capabilities_size});
        lsp->capabilities[lsp->capabilities_size] = '\0';
    }
    g_cli_cells->persistent_buf_release(answer.message, &g_cli_servers_cells);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unsent = {why, sizeof(why), 0ull};
    (void)cli_lsp_notified(lsp, "initialized", "{}", 2ull, &unsent);
    return lsp;
}
