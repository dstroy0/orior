// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_check_files.c: a batch of the tree's files handed to the tree's check: each file of a language with a server
// queued for it, its server started where it is not running, at most CLI_SERVERS_LARGEST bytes of a file; a server
// that checks its projects as a whole instructed once to check them, unless the tree's own code is disabled, as a check
// of the projects as a whole builds them and runs the tree's own code
#include "../command_line/cells.h"
#include "../machine/trust.h"
#include "../plugins/plugins.h"
#include "servers_internal.h"

// the most files of one batch queued, and the most languages it is of
#define CLI_SERVERS_BATCH (1u << 16u)
#define CLI_SERVERS_BATCH_LANGUAGES 64u

// A language of the batch: its id, whether a server serves it, the server, referenced, and its spec, and whether the
// server was instructed to check its projects
typedef struct
{
    char id[CLI_NAME_MAX_LEN];
    int served;
    CliLsp *server;
    CliServerSpec spec;
    int reported;
} CliServersBatchLanguage;

// The batch, a cell taken with its lock and given back with it: its languages and the languages known. Its files'
// paths, packed one after another, the place of each, and what each goes to, are cells of their own, sized to the
// batch's files
typedef struct
{
    CliServersBatchLanguage language[CLI_SERVERS_BATCH_LANGUAGES];
    CliLanguages known;
} CliServersBatch;

// the batch and how many languages it is of, under its lock, and the cells a batch is allocated in
static CliServersBatch *s_batch;
static unsigned int s_languages;

typedef struct
{
    const char *root;
    const char *id;
    const CliServersSink *sink;
} ServersCheckFilesCtx;
// the language `id` of the batch, its server started at `root` where it is first met: its place, or
// CLI_SERVERS_BATCH_LANGUAGES where the batch is of too many
EMBED_INLINE unsigned int check_files_language(const ServersCheckFilesCtx *args)
{
    const char *root = args->root;
    const CliServersSink *sink = args->sink;
    for (unsigned int at = 0u; at < s_languages; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_batch->language[at].id, .other = args->id, .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    if (s_languages == CLI_SERVERS_BATCH_LANGUAGES)
    {
        return CLI_SERVERS_BATCH_LANGUAGES;
    }
    CliServersBatchLanguage *const one = &s_batch->language[s_languages];
    memor.cpy(&(MemoriaCfg){.dst = one->id,
                            .src = args->id,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}) + 1u});
    one->reported = 0;
    char quiet[1024];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ULL};
    one->server = EMBED_CALL(cli_servers_language_internal.spec_for, CliServersLanguageCtx, .language = args->id,
                             .spec = &one->spec)
                      ? EMBED_CALL(cli_servers_language_internal.server, CliServersLanguageCtx, .root = root,
                                   .language = args->id, .sink = sink, .spec = &one->spec, .err_msg = &err_msg)
                      : NULL;
    one->served = (one->server != NULL);
    s_languages += 1u;
    return s_languages - 1u;
}

unsigned int cli_servers_check_files(const CliServersFilesCtx *args)
{
    // a file's full path is the tree's, a slash, and its own: the paths' text is no longer than those
    const unsigned int most = (args->count < CLI_SERVERS_BATCH) ? args->count : CLI_SERVERS_BATCH;
    const unsigned long long root_length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL});
    unsigned long long text_bytes = 1ULL;
    for (unsigned int file = 0u; file < args->count; file += 1u)
    {
        text_bytes +=
            root_length + cellul.len(&(CatenaFinitaCfg){.src = args->files[file], .cap = CLI_CELLS_POOL}) + 2ULL;
    }
    s_batch = g_cli_cells->persistent_buf_alloc(sizeof(*s_batch), &g_cli_servers_check_cells);
    char *const text = g_cli_cells->temporary_buf_alloc(text_bytes, &g_cli_servers_check_cells);
    const char **const paths = g_cli_cells->temporary_buf_alloc(((unsigned long long)most + 1ULL) * sizeof(*paths),
                                                                &g_cli_servers_check_cells);
    CliServersWaiting *const waiting_on = g_cli_cells->temporary_buf_alloc(
        ((unsigned long long)most + 1ULL) * sizeof(*waiting_on), &g_cli_servers_check_cells);
    if ((s_batch == NULL) || (text == NULL) || (paths == NULL) || (waiting_on == NULL))
    {
        g_cli_cells->persistent_buf_release(waiting_on, &g_cli_servers_check_cells);
        g_cli_cells->persistent_buf_release(paths, &g_cli_servers_check_cells);
        g_cli_cells->persistent_buf_release(text, &g_cli_servers_check_cells);
        g_cli_cells->persistent_buf_release(s_batch, &g_cli_servers_check_cells);
        s_batch = NULL;
        return 0u;
    }
    EMBED_CALL(cli_plugins_table.languages, CliPluginsCfg, .languages = &s_batch->known);
    s_languages = 0u;
    const int blocked = EMBED_CALL(cli_trust_tree_words.block, CliTrustTreeWordsCfg, .tree = args->root);
    unsigned int waiting = 0u;
    unsigned long long used = 0ULL;
    for (unsigned int file = 0u; (file < args->count) && (waiting < most); file += 1u)
    {
        char id[CLI_NAME_MAX_LEN];
        if (!EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                        .languages_read = &s_batch->known, .path = args->files[file], .id_out = id))
        {
            continue;
        }
        const unsigned int at =
            EMBED_CALL(check_files_language, ServersCheckFilesCtx, .root = args->root, .id = id, .sink = args->sink);
        if ((at == CLI_SERVERS_BATCH_LANGUAGES) || !s_batch->language[at].served)
        {
            continue;
        }
        CliServersBatchLanguage *const language = &s_batch->language[at];
        if (language->spec.checks[0] != '\0')
        {
            // a check of the projects as a whole builds them, and runs the tree's own code
            if (!blocked && !language->reported)
            {
                static const char s_params[] = "{\"textDocument\":null}";
                char quiet[256];
                VerbaLinea ignored = {quiet, sizeof(quiet), 0ULL};
                (void)EMBED_CALL(cli_lsp_err_msg_length.notify, CliLspErrMsgLengthCfg, .lsp = language->server,
                                 .method = language->spec.checks, .params = s_params, .length = sizeof(s_params) - 1u,
                                 .err_msg = &ignored);
                language->reported = 1;
            }
            continue;
        }
        char full[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = args->files[file],
                   .joined = full);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}) + 1ULL;
        unsigned long long size = 0ULL;
        if (!EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = full, .size_pointer = &size) ||
            (size > CLI_SERVERS_LARGEST) || ((used + length) > text_bytes))
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = text + used, .src = full, .bytes = (size_t)length});
        paths[waiting] = text + used;
        used += length;
        EMBED_CALL(cli_servers_path_language_internal.id_get, CliServersPathLanguageCtx, .spec_read = &language->spec,
                   .path = paths[waiting], .language = id, .id_out = waiting_on[waiting].id);
        waiting_on[waiting].server = language->server;
        EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = language->server);
        waiting += 1u;
    }
    for (unsigned int at = 0u; at < s_languages; at += 1u)
    {
        if (s_batch->language[at].served)
        {
            EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = s_batch->language[at].server);
        }
    }
    EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = &s_batch->known);
    const unsigned int queued = EMBED_CALL(cli_servers_count_internal.check_queue, CliServersCountCtx, .paths = paths,
                                           .waiting = waiting_on, .count = waiting, .sink = args->sink);
    g_cli_cells->persistent_buf_release(waiting_on, &g_cli_servers_check_cells);
    g_cli_cells->persistent_buf_release(paths, &g_cli_servers_check_cells);
    g_cli_cells->persistent_buf_release(text, &g_cli_servers_check_cells);
    g_cli_cells->persistent_buf_release(s_batch, &g_cli_servers_check_cells);
    s_batch = NULL;
    return queued;
}
