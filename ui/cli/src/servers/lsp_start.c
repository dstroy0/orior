// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp_start.c: a language server started at a tree, its readers started, and asked to initialize with what orior_cli can do
#include "../command_line/cells.h"
#include "../text/pieces.h"
#include "lsp_internal.h"

// how long the server has to answer that it started, in milliseconds
#define CLI_LSP_STARTING 30000ull

// what orior_cli can do, as it tells a server it starts
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
    "\"selectionRange\":{},"
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

// a server's cell taken, held once, numbered apart from every other started, and made empty: the server, or NULL
// where the cells cannot meet it
static CliLsp *lsp_place_taken(void)
{
    static unsigned int s_started;
    CliLsp *const lsp = cli_cell_taken(sizeof(*lsp), "a language server");
    if (lsp != NULL)
    {
        cli_lock_take(&g_cli_lsps_lock);
        lsp->taken = 1;
        lsp->holds = 1u;
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
static void lsp_initialize_params(const char *root, const char *options, unsigned long long length, CliLine *out)
{
    char uri[CLI_PATH_LONGEST * 3u];
    cli_lsp_uri_of(root, uri, sizeof(uri));
    size_t end = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    while ((end > 0u) && ((root[end - 1u] == '/') || (root[end - 1u] == '\\')))
    {
        end -= 1u;
    }
    size_t start = end;
    while ((start > 0u) && (root[start - 1u] != '/') && (root[start - 1u] != '\\'))
    {
        start -= 1u;
    }
    cli_line_text(out, "{\"processId\":");
    cli_line_decimal(out, cli_process_own(), 1u);
    cli_line_text(out, ",\"rootUri\":");
    cli_json_quoted(out, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"workspaceFolders\":[{\"uri\":");
    cli_json_quoted(out, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"name\":");
    cli_json_quoted_lossy(out, (const unsigned char *)root + start, end - start);
    cli_line_text(out, "}],\"capabilities\":");
    cli_line_text(out, s_capabilities);
    cli_line_text(out, ",\"initializationOptions\":");
    cli_line_counted(out, (length > 0ull) ? options : "null", (length > 0ull) ? length : 4ull);
    cli_line_text(out, ",\"clientInfo\":{\"name\":\"orior\"}}");
}

CliLsp *cli_lsp_started(const char *program, const char *const *args, unsigned int count, const char *root,
                        const char *options, unsigned long long length, const CliSetting *settings,
                        unsigned int setting_count, CliLspHeard heard, void *context, CliLine *said)
{
    CliLsp *const lsp = lsp_place_taken();
    if (lsp == NULL)
    {
        cli_line_text(said, "no more language servers are run at once");
        return NULL;
    }
    lsp->heard = heard;
    lsp->context = context;
    lsp->words.count = 0u;
    lsp->words.used = 0u;
    lsp->words.full = 0;
    cli_words_put(&lsp->words, program);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_words_put(&lsp->words, args[at]);
    }
    const unsigned int kept = (setting_count < 64u) ? setting_count : 64u;
    memor.cpy(&(MemoriaCfg){.dst = lsp->settings, .src = settings, .bytes = kept * sizeof(settings[0])});
    if (!cli_spawn_fed(&lsp->spawned, lsp->words.word, root, lsp->settings, kept, 1, said))
    {
        cli_cell_given(lsp);
        return NULL;
    }
    cli_lsp_readers_started(lsp);
    // the params and the answer each written through pieces into a cell of its size
    char params_piece[4096];
    CliPieces params_pieces;
    CliLine params;
    cli_pieces_begun(&params_pieces, &params, params_piece, sizeof(params_piece));
    lsp_initialize_params(root, options, length, &params);
    unsigned long long params_size = 0ull;
    char *const asked = cli_pieces_joined(&params_pieces, &params, "a server's initialize params", &params_size);
    char answer_piece[4096];
    CliPieces answer_pieces;
    CliLine answer;
    cli_pieces_begun(&answer_pieces, &answer, answer_piece, sizeof(answer_piece));
    const unsigned long long said_from = said->at;
    if (asked == NULL)
    {
        cli_line_text(said, "the server's initialize params could not be written: the cells cannot meet them");
    }
    const int initialized =
        (asked != NULL) && cli_lsp_requested(lsp, "initialize", asked, params_size, CLI_LSP_STARTING, &answer, said);
    cli_cell_given(asked);
    unsigned long long reply_size = 0ull;
    char *const reply = cli_pieces_joined(&answer_pieces, &answer, "a server's initialize answer", &reply_size);
    if (!initialized || (reply == NULL))
    {
        cli_cell_given(reply);
        query_paused(200u);
        char complaint[CLI_LSP_COMPLAINT];
        CliLine complaint_line = {complaint, sizeof(complaint), 0ull};
        cli_lsp_complaint(lsp, &complaint_line);
        if ((complaint_line.at > 0ull) && (said->at > said_from))
        {
            cli_line_text(said, ": ");
            cli_line_counted(said, complaint, complaint_line.at);
        }
        cli_lsp_given(lsp);
        return NULL;
    }
    // what the server said it can do, in a cell of its size
    unsigned long long at = 0ull;
    unsigned long long size = 0ull;
    const int has = cli_lsp_member(reply, reply_size, "capabilities", &at, &size);
    lsp->capabilities_size = has ? size : 4ull;
    lsp->capabilities = cli_cell_taken(lsp->capabilities_size + 1ull, "what a server said it can do");
    if (lsp->capabilities != NULL)
    {
        // a member of the reply, which a size_t counts
        memor.cpy(&(MemoriaCfg){
            .dst = lsp->capabilities, .src = has ? reply + at : "null", .bytes = (size_t)lsp->capabilities_size});
        lsp->capabilities[lsp->capabilities_size] = '\0';
    }
    cli_cell_given(reply);
    char why[CLI_SAID_LONGEST];
    CliLine unsent = {why, sizeof(why), 0ull};
    (void)cli_lsp_notified(lsp, "initialized", "{}", 2ull, &unsent);
    return lsp;
}
