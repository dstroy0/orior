// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_check_files.c: a batch of the tree's files handed to the tree's check: each file of a language with a server
// queued for it, its server started where it is not running, at most CLI_SERVERS_LARGEST bytes of a file; a server
// that checks its projects as a whole told once to check them, unless the tree's own code is held back, as a check of
// the projects as a whole builds them and runs the tree's own code
#include "../command_line/cells.h"
#include "../machine/trust.h"
#include "../plugins/plugins.h"
#include "servers_internal.h"

// the most files of one batch queued, and the most languages it is of
#define CLI_SERVERS_BATCH (1u << 16u)
#define CLI_SERVERS_BATCH_LANGUAGES 64u

// A language of the batch: its id, whether a server serves it, the server, held, and its spec, and whether the
// server was told to check its projects
typedef struct
{
    char id[CLI_NAME_LONGEST];
    int served;
    CliLsp *server;
    CliServerSpec spec;
    int told;
} CliServersBatchLanguage;

// The batch, a cell taken with its lock and given back with it: its languages, the languages known, its files' paths
// and what each goes to
typedef struct
{
    CliServersBatchLanguage language[CLI_SERVERS_BATCH_LANGUAGES];
    CliLanguages known;
    char path[CLI_SERVERS_BATCH][CLI_PATH_LONGEST];
    const char *paths[CLI_SERVERS_BATCH];
    CliServersWaiting waiting[CLI_SERVERS_BATCH];
} CliServersBatch;

// the batch and how many languages it is of, under its lock
static CliServersBatch *s_batch;
static unsigned int s_languages;
static CliLock s_lock = CLI_LOCK_INIT;

// the language `id` of the batch, its server started at `root` where it is first met: its place, or
// CLI_SERVERS_BATCH_LANGUAGES where the batch is of too many
static unsigned int check_files_language(const char *root, const char *id, const CliServersEmit *emit)
{
    for (unsigned int at = 0u; at < s_languages; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_batch->language[at].id, .other = id, .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    if (s_languages == CLI_SERVERS_BATCH_LANGUAGES)
    {
        return CLI_SERVERS_BATCH_LANGUAGES;
    }
    CliServersBatchLanguage *const one = &s_batch->language[s_languages];
    memor.cpy(&(MemoriaCfg){
        .dst = one->id, .src = id, .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
    one->told = 0;
    char quiet[1024];
    CliLine said = {quiet, sizeof(quiet), 0ull};
    one->server = cli_servers_spec_for(id, &one->spec) ? cli_servers_server(root, id, emit, &one->spec, &said) : NULL;
    one->served = (one->server != NULL);
    s_languages += 1u;
    return s_languages - 1u;
}

unsigned int cli_servers_check_files(const char *root, const char *const *files, unsigned int count,
                                     const CliServersEmit *emit)
{
    cli_lock_take(&s_lock);
    s_batch = cli_cell_taken(sizeof(*s_batch), "a batch of files checked");
    if (s_batch == NULL)
    {
        cli_lock_give(&s_lock);
        return 0u;
    }
    cli_plugins_languages(&s_batch->known);
    s_languages = 0u;
    const int held = cli_trust_held_back(root);
    unsigned int waiting = 0u;
    for (unsigned int file = 0u; (file < count) && (waiting < CLI_SERVERS_BATCH); file += 1u)
    {
        char id[CLI_NAME_LONGEST];
        if (!cli_plugins_language_for(&s_batch->known, files[file], id))
        {
            continue;
        }
        const unsigned int at = check_files_language(root, id, emit);
        if ((at == CLI_SERVERS_BATCH_LANGUAGES) || !s_batch->language[at].served)
        {
            continue;
        }
        CliServersBatchLanguage *const language = &s_batch->language[at];
        if (language->spec.checks[0] != '\0')
        {
            // a check of the projects as a whole builds them, and runs the tree's own code
            if (!held && !language->told)
            {
                static const char s_params[] = "{\"textDocument\":null}";
                char quiet[256];
                CliLine ignored = {quiet, sizeof(quiet), 0ull};
                (void)cli_lsp_notified(language->server, language->spec.checks, s_params, sizeof(s_params) - 1u,
                                       &ignored);
                language->told = 1;
            }
            continue;
        }
        cli_full(root, files[file], s_batch->path[waiting]);
        unsigned long long size = 0ull;
        if (!cli_file_size(s_batch->path[waiting], &size) || (size > CLI_SERVERS_LARGEST))
        {
            continue;
        }
        cli_servers_id_of(&language->spec, s_batch->path[waiting], id, s_batch->waiting[waiting].id);
        s_batch->waiting[waiting].server = language->server;
        cli_lsp_held(language->server);
        s_batch->paths[waiting] = s_batch->path[waiting];
        waiting += 1u;
    }
    for (unsigned int at = 0u; at < s_languages; at += 1u)
    {
        if (s_batch->language[at].served)
        {
            cli_lsp_given(s_batch->language[at].server);
        }
    }
    const unsigned int queued = cli_servers_check_queued(s_batch->paths, s_batch->waiting, waiting, emit);
    cli_cell_given(s_batch);
    s_batch = NULL;
    cli_lock_give(&s_lock);
    return queued;
}
