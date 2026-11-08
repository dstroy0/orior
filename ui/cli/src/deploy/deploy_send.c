// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_send.c: a deployment's files sent to a place: on a machine a tar stream into a new release beside the one it
// runs, the files it retains and those the tree no longer has that it retains copied into the release from the one it
// runs, or into its folder itself; to a folder here each file copied into it. And a machine's `current` turned to a
// release, its old releases let go of, and each place turned back to the release before the one it runs
#include "../command_line/pool.h"
#include "../text/text.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"

#include <stdlib.h>

// a machine's releases, oldest first, in a cell of the count of the lines of its listing, which is in a cell of its
// size, each given back as the next machine's are read and as their reader ends
static const char **s_releases;
static unsigned int s_release_count;
static char *s_releases_listing;

typedef struct
{
    const CliDeployPosition *place;
    const VerbaLinea *line;
    const char *longer;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    const char *text;
    const char *path;
    unsigned int index;
    int replace;
    unsigned int *count;
    const char *folder;
    const CliDeployPair *preserved;
    unsigned int preserved_count;
    const char *into;
    int remove;
    const char **current;
} DeploySendCtx;
// the shell line written on `line` run on `place`, `longer` given where it did not fit: 1, or 0 with why given
EMBED_INLINE int send_line_run(const DeploySendCtx *args)
{
    const CliDeployPosition *place = args->place;
    const VerbaLinea *line = args->line;
    VerbaLinea *err_msg = args->err_msg;
    if (!mmgr_line_ok(line))
    {
        mmgr_line_put(err_msg, args->longer);
        return 0;
    }
    VerbaLinea nothing = {NULL, 0ULL, 0ULL};
    return EMBED_CALL(cli_deploy_size_internal.remote_run, CliDeploySizeCtx, .place = place, .line_text = line->out,
                      .input = NULL, .size = 0ULL, .out = &nothing, .err_msg = err_msg);
}

// `size` bytes of `text`, then `path`, as one word of a shell line
EMBED_INLINE void send_quote(const DeploySendCtx *args)
{
    VerbaLinea *out = args->out;
    const char *text = args->text;
    char joined[(2u * CLI_PATH_MAX_LEN) + 64u];
    VerbaLinea line = {joined, sizeof(joined), 0ULL};
    mmgr_line_put(&line, text);
    mmgr_line_put(&line, args->path);
    EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = joined, .size = line.at, .out = out);
}

void cli_deploy_switch_line(const CliDeployOutCtx *args)
{
    mmgr_line_put(args->out, "cd ");
    EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = args->folder,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_POOL_BYTES}), .out = args->out);
    mmgr_line_put(args->out, " && ln -sfn ");
    EMBED_CALL(send_quote, DeploySendCtx, .out = args->out, .text = "releases/", .path = args->release);
    mmgr_line_put(args->out, " current.orior && { mv -fT current.orior current 2>/dev/null || { rm -f current && mv "
                             "current.orior current; }; }");
}

// the files retained at the place at `index`, those changed there where `replace` is 0, sorted
EMBED_INLINE const CliDeployPair *send_preserve(const DeploySendCtx *args)
{
    unsigned int *count = args->count;
    const CliDeployRun *const run = &g_cli_deploy;
    *count = args->replace ? 0u : run->changed[args->index];
    return run->changed_list[args->index];
}

// the files the run sends but those retained copied into the folder `folder` of this machine: 1, or 0 with why given
EMBED_INLINE int send_here(const DeploySendCtx *args)
{
    const char *folder = args->folder;
    const CliDeployRun *const run = &g_cli_deploy;
    for (unsigned int index = 0u; index < run->sents; index += 1u)
    {
        int skipped = 0;
        for (unsigned int at = 0u; !skipped && (at < args->preserved_count); at += 1u)
        {
            skipped = cellul.eq(&(CatenaFinitaCfg){
                .src = args->preserved[at].path, .other = run->sent[index].path, .cap = CLI_POOL_BYTES});
        }
        if (skipped)
        {
            continue;
        }
        char to[(2u * CLI_PATH_MAX_LEN) + 2u];
        char parent[CLI_PATH_MAX_LEN];
        char full[(2u * CLI_PATH_MAX_LEN) + 2u];
        VerbaLinea to_line = {to, sizeof(to), 0ULL};
        VerbaLinea full_line = {full, sizeof(full), 0ULL};
        mmgr_line_put(&to_line, folder);
        mmgr_line_ch(&to_line, '/');
        mmgr_line_put(&to_line, run->sent[index].path);
        EMBED_CALL(cli_deploy_out_index_internal.full, CliDeployOutIndexCtx, .index = index, .out = &full_line);
        if ((mmgr_line_finish(&to_line) == 0ULL) || (mmgr_line_finish(&full_line) == 0ULL) ||
            (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = to, .out = parent) &&
             !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = parent)))
        {
            mmgr_line_put(args->err_msg, to);
            mmgr_line_put(args->err_msg, ": its folder could not be made");
            return 0;
        }
        if (!EMBED_CALL(query_from_internal.file_copy, QueryFromCtx, .from = full, .to = to))
        {
            mmgr_line_put(args->err_msg, to);
            mmgr_line_put(args->err_msg, ": it could not be written");
            return 0;
        }
    }
    return 1;
}

// the files retained and the files gone that are retained copied into the release `into` from the one the place at
// `index` runs: 1, or 0 with why given
EMBED_INLINE int send_retained_files(const DeploySendCtx *args)
{
    const CliDeployRun *const run = &g_cli_deploy;
    const CliDeployPosition *const place = &run->place[args->index];
    const unsigned int deleted = args->remove ? 0u : run->deleted[args->index];
    if (args->preserved_count + deleted == 0u)
    {
        return 1;
    }
    char *const line_text = g_cli_guard->temporary_buf_alloc(CLI_DEPLOY_LINE, g_cli_deploy_owner);
    VerbaLinea line = mmgr_line_over(line_text, CLI_DEPLOY_LINE);
    char current[CLI_PATH_MAX_LEN + 16u];
    VerbaLinea current_line = {current, sizeof(current), 0ULL};
    mmgr_line_put(&current_line,
                  EMBED_CALL(cli_deploy_size_internal.position_folder, CliDeploySizeCtx, .place = place));
    mmgr_line_put(&current_line, "/current/");
    mmgr_line_finish(&current_line);
    for (unsigned int at = 0u; at < args->preserved_count + deleted; at += 1u)
    {
        const char *const path = (at < args->preserved_count)
                                     ? args->preserved[at].path
                                     : run->removed_list[args->index][at - args->preserved_count].path;
        char to[(2u * CLI_PATH_MAX_LEN) + 2u];
        VerbaLinea to_line = {to, sizeof(to), 0ULL};
        mmgr_line_put(&to_line, args->into);
        mmgr_line_ch(&to_line, '/');
        mmgr_line_put(&to_line, path);
        mmgr_line_finish(&to_line);
        unsigned long long slash = to_line.at;
        const unsigned long long end =
            EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = to, .at = slash, .set = "/");
        slash = (end != 0ULL) ? (end - 1ULL) : 0ULL;
        mmgr_line_put(&line, (at > 0u) ? " && mkdir -p " : "mkdir -p ");
        EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = to, .size = slash, .out = &line);
        mmgr_line_put(&line, " && cp -p ");
        EMBED_CALL(send_quote, DeploySendCtx, .out = &line, .text = current, .path = path);
        mmgr_line_ch(&line, ' ');
        EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = to, .size = to_line.at, .out = &line);
    }
    const int run_result = EMBED_CALL(send_line_run, DeploySendCtx, .place = place, .line = &line,
                                      .longer = "the line that copies the files retained is longer than orior_cli runs",
                                      .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(line_text, g_cli_deploy_owner);
    return run_result;
}

int cli_deploy_send_to(const CliDeployErrMsgCtx *args)
{
    const CliDeployPosition *const place = &g_cli_deploy.place[args->index];
    unsigned int preserved_count = 0u;
    const CliDeployPair *const preserved = EMBED_CALL(send_preserve, DeploySendCtx, .index = args->index,
                                                      .replace = args->replace, .count = &preserved_count);
    if (place->here)
    {
        return EMBED_CALL(send_here, DeploySendCtx, .folder = place->path, .preserved = preserved,
                          .preserved_count = preserved_count, .err_msg = args->err_msg);
    }
    char into[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea into_line = {into, sizeof(into), 0ULL};
    mmgr_line_put(&into_line, place->address.folder);
    if (args->releases)
    {
        mmgr_line_put(&into_line, "/releases/");
        mmgr_line_put(&into_line, args->release);
    }
    mmgr_line_finish(&into_line);
    char tar[CLI_PATH_MAX_LEN];
    VerbaLinea tar_line = {tar, sizeof(tar), 0ULL};
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = tar);
    tar_line.at = cellul.len(&(CatenaFinitaCfg){.src = tar, .cap = sizeof(tar)});
    mmgr_line_put(&tar_line, "/orior-deploy-");
    mmgr_line_decimal(&tar_line, cli_process_own(), 1u);
    mmgr_line_ch(&tar_line, '-');
    mmgr_line_decimal(&tar_line, args->index, 1u);
    mmgr_line_put(&tar_line, ".tar");
    mmgr_line_finish(&tar_line);
    if (!EMBED_CALL(cli_deploy_err_msg_path_internal.tar, CliDeployErrMsgPathCtx, .path = tar, .preserved = preserved,
                    .preserved_count = preserved_count, .time = args->time, .err_msg = args->err_msg))
    {
        (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = tar);
        return 0;
    }
    char line[(4u * CLI_PATH_MAX_LEN) + 64u];
    VerbaLinea command = {line, sizeof(line), 0ULL};
    mmgr_line_put(&command, "mkdir -p ");
    EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = into, .size = into_line.at, .out = &command);
    mmgr_line_put(&command, " && tar -x -f - -C ");
    EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = into, .size = into_line.at, .out = &command);
    mmgr_line_finish(&command);
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = tar,
                                                  .size_pointer = &size, .owner = g_cli_deploy_owner);
    VerbaLinea none = {NULL, 0ULL, 0ULL};
    const int sent = (bytes != NULL) && EMBED_CALL(cli_deploy_size_internal.remote_run, CliDeploySizeCtx,
                                                   .place = place, .line_text = line, .input = (const char *)bytes,
                                                   .size = size, .out = &none, .err_msg = args->err_msg);
    if (bytes == NULL)
    {
        mmgr_line_put(args->err_msg, tar);
        mmgr_line_put(args->err_msg, ": it could not be read");
    }
    else
    {
        g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_deploy_owner));
    }
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = tar);
    return sent &&
           (!args->releases ||
            EMBED_CALL(send_retained_files, DeploySendCtx, .index = args->index, .into = into, .preserved = preserved,
                       .preserved_count = preserved_count, .remove = args->remove, .err_msg = args->err_msg));
}

// two releases in the order of their names
static int send_order(const void *one, const void *other)
{
    return EMBED_CALL(cli_deploy_left_internal.order, CliDeployLeftCtx, .left = *(const char *const *)one,
                      .right = *(const char *const *)other);
}

// the releases of the machine at `index`, oldest first, into s_releases, and the one `current` names, NULL for none,
// those before given back: 1, or 0 with why given
static int send_releases(const DeploySendCtx *args)
{
    const char **current = args->current;
    VerbaLinea *err_msg = args->err_msg;
    const CliDeployPosition *const place = &g_cli_deploy.place[args->index];
    g_cli_guard->persistent_buf_release((void *)s_releases, g_cli_deploy_owner);
    s_releases = NULL;
    s_release_count = 0u;
    g_cli_guard->persistent_buf_release(s_releases_listing, g_cli_deploy_owner);
    s_releases_listing = NULL;
    *current = NULL;
    char line[CLI_PATH_MAX_LEN + 160u];
    VerbaLinea command = {line, sizeof(line), 0ULL};
    mmgr_line_put(&command, "cd ");
    EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = place->address.folder,
               .size = cellul.len(&(CatenaFinitaCfg){.src = place->address.folder, .cap = CLI_POOL_BYTES}),
               .out = &command);
    mmgr_line_put(&command, " 2>/dev/null || exit 0; ls -1 releases 2>/dev/null; echo '<current>'; readlink current "
                            "2>/dev/null; true");
    mmgr_line_finish(&command);
    s_releases_listing = g_cli_guard->temporary_buf_alloc(CLI_DEPLOY_LINE, g_cli_deploy_owner);
    VerbaLinea listing = mmgr_line_over(s_releases_listing, CLI_DEPLOY_LINE);
    const int run_result =
        EMBED_CALL(cli_deploy_size_internal.remote_run, CliDeploySizeCtx, .place = place, .line_text = line,
                   .input = NULL, .size = 0ULL, .out = &listing, .err_msg = err_msg);
    const unsigned long long size = listing.at;
    if (!run_result)
    {
        g_cli_guard->persistent_buf_release((void *)s_releases, g_cli_deploy_owner);
        s_releases = NULL;
        s_release_count = 0u;
        g_cli_guard->persistent_buf_release(s_releases_listing, g_cli_deploy_owner);
        s_releases_listing = NULL;
        return 0;
    }
    if (!mmgr_line_ok(&listing))
    {
        g_cli_guard->persistent_buf_release((void *)s_releases, g_cli_deploy_owner);
        s_releases = NULL;
        s_release_count = 0u;
        g_cli_guard->persistent_buf_release(s_releases_listing, g_cli_deploy_owner);
        s_releases_listing = NULL;
        mmgr_line_put(err_msg, "it has more files than orior_cli supports");
        return 0;
    }
    static const char s_mark[] = "<current>";
    const char *const mark_read = cellul.find(
        &(CatenaFinitaCfg){.src = s_releases_listing, .cap = size + 1u, .other = s_mark, .other_cap = sizeof(s_mark)});
    char *const mark = (mark_read != NULL) ? (s_releases_listing + (mark_read - s_releases_listing)) : NULL;
    char *after = (mark != NULL) ? (mark + 9) : NULL;
    if (mark != NULL)
    {
        *mark = '\0';
    }
    unsigned int lines = 1u;
    lines += (unsigned int)EMBED_CALL(
        cli_text_size_byte.count, CliTextSizeByteCfg, .text = s_releases_listing,
        .size = cellul.len(&(CatenaFinitaCfg){.src = s_releases_listing, .cap = CLI_POOL_BYTES}), .byte = '\n');
    s_releases =
        g_cli_guard->temporary_buf_alloc((unsigned long long)lines * sizeof(s_releases[0]), g_cli_deploy_owner);
    if (s_releases == NULL)
    {
        g_cli_guard->persistent_buf_release((void *)s_releases, g_cli_deploy_owner);
        s_releases = NULL;
        s_release_count = 0u;
        g_cli_guard->persistent_buf_release(s_releases_listing, g_cli_deploy_owner);
        s_releases_listing = NULL;
        mmgr_line_put(err_msg, "it has more files than orior_cli supports");
        return 0;
    }
    for (char *at = s_releases_listing; (*at != '\0') && (s_release_count < lines);)
    {
        char *end = at;
        end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ULL,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_POOL_BYTES}), .set = "\n");
        const int more = (*end == '\n');
        *end = '\0';
        char *const start = at + EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = at,
                                            .at = 0ULL, .size = (unsigned long long)(end - at), .set = CLI_TEXT_WHITE);
        char *const last = start + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = start,
                                              .at = (unsigned long long)(end - start), .set = CLI_TEXT_WHITE);
        *last = '\0';
        if (*start != '\0')
        {
            s_releases[s_release_count] = start;
            s_release_count += 1u;
        }
        at = more ? (end + 1) : end;
    }
    qsort((void *)s_releases, s_release_count, sizeof(s_releases[0]), send_order);
    while ((after != NULL) && cellul.ws(&(CatenaFinitaCfg){.src = after}))
    {
        after += 1;
    }
    static const char s_releases_folder[] = "releases/";
    if ((after != NULL) &&
        cellul.starts(&(CatenaFinitaCfg){.src = after, .other = s_releases_folder, .cap = sizeof(s_releases_folder)}))
    {
        char *name = after + 9;
        char *last = name + cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES});
        last = name + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = name,
                                 .at = (unsigned long long)(last - name), .set = CLI_TEXT_WHITE "/");
        *last = '\0';
        *current = name;
    }
    return 1;
}

void cli_deploy_prune(const CliDeployIndexCtx *args)
{
    const CliDeployPosition *const place = &g_cli_deploy.place[args->index];
    const char *current = NULL;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea none = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(send_releases, DeploySendCtx, .index = args->index, .current = &current, .err_msg = &none))
    {
        return;
    }
    const unsigned long long newest = (args->retain > 1u) ? args->retain : 1u;
    char *const line_text = g_cli_guard->temporary_buf_alloc(CLI_DEPLOY_LINE, g_cli_deploy_owner);
    VerbaLinea line = mmgr_line_over(line_text, CLI_DEPLOY_LINE);
    mmgr_line_put(&line, "cd ");
    EMBED_CALL(send_quote, DeploySendCtx, .out = &line, .text = place->address.folder, .path = "/releases");
    mmgr_line_put(&line, " && rm -rf --");
    unsigned int old = 0u;
    for (unsigned int at = s_release_count; at > 0u; at -= 1u)
    {
        const char *const name = s_releases[at - 1u];
        if (((unsigned long long)(s_release_count - at) >= newest) &&
            ((current == NULL) || !cellul.eq(&(CatenaFinitaCfg){.src = current, .other = name, .cap = CLI_POOL_BYTES})))
        {
            mmgr_line_ch(&line, ' ');
            EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = name,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}), .out = &line);
            old += 1u;
        }
    }
    g_cli_guard->persistent_buf_release((void *)s_releases, g_cli_deploy_owner);
    s_releases = NULL;
    s_release_count = 0u;
    g_cli_guard->persistent_buf_release(s_releases_listing, g_cli_deploy_owner);
    s_releases_listing = NULL;
    const int run_result = (old != 0u) && EMBED_CALL(send_line_run, DeploySendCtx, .place = place, .line = &line,
                                                     .longer = "", .err_msg = &none);
    g_cli_guard->persistent_buf_release(line_text, g_cli_deploy_owner);
    if (!run_result)
    {
        return;
    }
    char print_buf[(2u * CLI_PATH_MAX_LEN) + 64u];
    VerbaLinea print_line = {print_buf, sizeof(print_buf), 0ULL};
    EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = place, .out = &print_line);
    mmgr_line_put(&print_line, " let go of ");
    mmgr_line_decimal(&print_line, old, 1u);
    mmgr_line_put(&print_line, " old releases");
    EMBED_CALL(cli_deploy_internal.print, CliDeployCtx, .line = &print_line);
}

int cli_deploy_roll_back(const CliDeployTextCtx *args)
{
    CliDeployRun *const run = &g_cli_deploy;
    // the release each place turns back to, stored in the run's pool
    const char **const back = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &run->pool,
                                         .size = (unsigned long long)run->positions * sizeof(back[0]));
    if (back == NULL)
    {
        mmgr_line_put(args->err_msg, "no place was rolled back");
        return 0;
    }
    for (unsigned int index = 0u; index < run->positions; index += 1u)
    {
        const CliDeployPosition *const place = &run->place[index];
        if (run->deployment.in_position || place->here)
        {
            EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = place, .out = args->err_msg);
            mmgr_line_put(args->err_msg,
                          " takes the files in place and retains no release before them to go back to: no place "
                          "was rolled back");
            return 0;
        }
        const char *current = NULL;
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea error_line = {why, sizeof(why), 0ULL};
        if (!EMBED_CALL(send_releases, DeploySendCtx, .index = index, .current = &current, .err_msg = &error_line))
        {
            EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = place, .out = args->err_msg);
            mmgr_line_put(args->err_msg, ": ");
            mmgr_line_put_n(args->err_msg, why, error_line.at);
            return 0;
        }
        unsigned int at = 0u;
        while ((current != NULL) && (at < s_release_count) &&
               !cellul.eq(&(CatenaFinitaCfg){.src = s_releases[at], .other = current, .cap = CLI_POOL_BYTES}))
        {
            at += 1u;
        }
        if ((current == NULL) || (at == s_release_count) || (at == 0u))
        {
            g_cli_guard->persistent_buf_release((void *)s_releases, g_cli_deploy_owner);
            s_releases = NULL;
            s_release_count = 0u;
            g_cli_guard->persistent_buf_release(s_releases_listing, g_cli_deploy_owner);
            s_releases_listing = NULL;
            EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = place, .out = args->err_msg);
            mmgr_line_put(args->err_msg, " has no release before the one it runs: no place was rolled back");
            return 0;
        }
        back[index] = EMBED_CALL(
            cli_deploy_pool_internal.text_store, CliDeployPoolCtx, .pool = &run->pool, .text = s_releases[at - 1u],
            .size = cellul.len(&(CatenaFinitaCfg){.src = s_releases[at - 1u], .cap = CLI_POOL_BYTES}));
        g_cli_guard->persistent_buf_release((void *)s_releases, g_cli_deploy_owner);
        s_releases = NULL;
        s_release_count = 0u;
        g_cli_guard->persistent_buf_release(s_releases_listing, g_cli_deploy_owner);
        s_releases_listing = NULL;
    }
    for (unsigned int index = 0u; index < run->positions; index += 1u)
    {
        const CliDeployPosition *const place = &run->place[index];
        char line[(2u * CLI_PATH_MAX_LEN) + 256u];
        VerbaLinea command = {line, sizeof(line), 0ULL};
        EMBED_CALL(cli_deploy_out_internal.switch_line, CliDeployOutCtx, .folder = place->address.folder,
                   .release = back[index], .out = &command);
        mmgr_line_finish(&command);
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea error_line = {why, sizeof(why), 0ULL};
        VerbaLinea none = {NULL, 0ULL, 0ULL};
        if (!EMBED_CALL(cli_deploy_size_internal.remote_run, CliDeploySizeCtx, .place = place, .line_text = line,
                        .input = NULL, .size = 0ULL, .out = &none, .err_msg = &error_line))
        {
            EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = place, .out = args->err_msg);
            mmgr_line_put(args->err_msg, ": ");
            mmgr_line_put_n(args->err_msg, why, error_line.at);
            return 0;
        }
        char print_buf[(2u * CLI_PATH_MAX_LEN) + 64u];
        VerbaLinea print_line = {print_buf, sizeof(print_buf), 0ULL};
        EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = place, .out = &print_line);
        mmgr_line_put(&print_line, " now runs release ");
        mmgr_line_put(&print_line, back[index]);
        EMBED_CALL(cli_deploy_internal.print, CliDeployCtx, .line = &print_line);
    }
    return 1;
}

int cli_deploy_remove(const CliDeployErrMsgIndexCtx *args)
{
    const CliDeployRun *const run = &g_cli_deploy;
    const CliDeployPosition *const place = &run->place[args->index];
    const unsigned int count = run->deleted[args->index];
    if (count == 0u)
    {
        return 1;
    }
    const CliDeployPair *const deleted = run->removed_list[args->index];
    if (place->here)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            char path[(2u * CLI_PATH_MAX_LEN) + 2u];
            VerbaLinea path_line = {path, sizeof(path), 0ULL};
            mmgr_line_put(&path_line, place->path);
            mmgr_line_ch(&path_line, '/');
            mmgr_line_put(&path_line, deleted[at].path);
            if ((mmgr_line_finish(&path_line) == 0ULL) ||
                !EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path))
            {
                mmgr_line_put(args->err_msg, deleted[at].path);
                mmgr_line_put(args->err_msg, ": it could not be removed");
                return 0;
            }
        }
    }
    else
    {
        char *const line_text = g_cli_guard->temporary_buf_alloc(CLI_DEPLOY_LINE, g_cli_deploy_owner);
        VerbaLinea line = mmgr_line_over(line_text, CLI_DEPLOY_LINE);
        mmgr_line_put(&line, "cd ");
        EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = place->address.folder,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = place->address.folder, .cap = CLI_POOL_BYTES}),
                   .out = &line);
        mmgr_line_put(&line, " && rm -f --");
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            mmgr_line_ch(&line, ' ');
            EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = deleted[at].path,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = deleted[at].path, .cap = CLI_POOL_BYTES}),
                       .out = &line);
        }
        const int run_result = EMBED_CALL(
            send_line_run, DeploySendCtx, .place = place, .line = &line,
            .longer = "the line that deletes the files gone is longer than orior_cli runs", .err_msg = args->err_msg);
        g_cli_guard->persistent_buf_release(line_text, g_cli_deploy_owner);
        if (!run_result)
        {
            return 0;
        }
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char print_buf[(3u * CLI_PATH_MAX_LEN) + 32u];
        VerbaLinea print_line = {print_buf, sizeof(print_buf), 0ULL};
        mmgr_line_put(&print_line, "deleted ");
        mmgr_line_put(&print_line, deleted[at].path);
        mmgr_line_put(&print_line, " at ");
        EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = place, .out = &print_line);
        EMBED_CALL(cli_deploy_internal.print, CliDeployCtx, .line = &print_line);
    }
    return 1;
}
