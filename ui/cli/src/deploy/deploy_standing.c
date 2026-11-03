// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_existing.c: what a place has against what was last sent there and what goes now: the files changed there
// since they were sent, each shown with its differences where both copies are text, and the files there the tree no
// longer has; and the record of the hashes last sent to a place written after a send
#include "../command_line/cells.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"
#include "memoria_operor/memoria_operor.h"

// The lines of a text: the text, and where each line starts and its length
typedef struct
{
    const char *text;
    unsigned long long *start;
    unsigned long long *size;
} CliDeployLines;
// the most bytes of a copy at a place compared, and the most lines of a copy here compared
#define CLI_DEPLOY_COMPARED 65536u
#define CLI_DEPLOY_LINES (1u << 18u)

typedef struct
{
    const CliDeployLines *lines;
    unsigned long long size;
    unsigned int most;
    const CliDeployLines *old;
    unsigned int one;
    const CliDeployLines *new_lines;
    unsigned int other;
    const char *mark;
    const char *text;
    unsigned int index;
    const char *path;
    unsigned long long *size_pointer;
    const char *folder;
    int replace;
    int releases;
    int remove;
    int deleted;
    CliDeployPair *list;
    const char *hash;
    int first;
    VerbaLinea *record;
} DeployStandingCtx;
// the lines of `size` bytes of `lines->text`, as Rust's str::lines splits them, into its starts and sizes where they
// are not NULL, `most` at the most: their count, or more than `most` where there are more
EMBED_INLINE unsigned int existing_lines(const DeployStandingCtx *args)
{
    const char *const text = args->lines->text;
    unsigned int count = 0u;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = text + at, .bytes = (size_t)(args->size - at), .val = (uint8_t)'\n'});
        unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : args->size;
        if (count == args->most)
        {
            return args->most + 1u;
        }
        const unsigned long long start = at;
        at = (newline != NULL) ? (end + 1u) : args->size;
        end -= ((newline != NULL) && (end > start) && (text[end - 1u] == '\r')) ? 1u : 0u;
        if (args->lines->start != NULL)
        {
            args->lines->start[count] = start;
            args->lines->size[count] = end - start;
        }
        count += 1u;
    }
    return count;
}

// whether line `one` of `old` and line `other` of `new_lines` are the same
EMBED_INLINE int existing_same(const DeployStandingCtx *args)
{
    unsigned int other = args->other;
    return (args->old->size[args->one] == args->new_lines->size[other]) &&
           (memor.cmp(&(MemoriaCfg){.src = args->old->text + args->old->start[args->one],
                                    .other = args->new_lines->text + args->new_lines->start[other],
                                    .bytes = (size_t)args->old->size[args->one]}) == 0);
}

// a line printed, two spaces, a mark and a space before `size` bytes of `text`
EMBED_INLINE void existing_line_print(const DeployStandingCtx *args)
{
    char line[CLI_DEPLOY_COMPARED + 16u];
    VerbaLinea print_line = {line, sizeof(line), 0ULL};
    mmgr_line_put(&print_line, "  ");
    mmgr_line_put(&print_line, args->mark);
    mmgr_line_put_n(&print_line, args->text, args->size);
    EMBED_CALL(cli_deploy_internal.print, CliDeployCtx, .line = &print_line);
}

void cli_deploy_difference(const CliDeployOldCtx *args)
{
    CliDeployLines olds_lines = {args->old, NULL, NULL};
    CliDeployLines news_lines = {args->new_text, NULL, NULL};
    const unsigned int olds = EMBED_CALL(existing_lines, DeployStandingCtx, .lines = &olds_lines,
                                         .size = args->old_size, .most = CLI_DEPLOY_COMPARED + 1u);
    const unsigned int news = EMBED_CALL(existing_lines, DeployStandingCtx, .lines = &news_lines,
                                         .size = args->new_size, .most = CLI_DEPLOY_LINES);
    if ((olds > CLI_DEPLOY_COMPARED + 1u) || (news > CLI_DEPLOY_LINES))
    {
        return;
    }
    // each line's start and length, the old text's then the new's, in one cell of their count
    unsigned long long *const counted = g_cli_cells->persistent_buf_alloc(
        2ULL * ((unsigned long long)olds + news + 1u) * sizeof(counted[0]), &g_cli_deploy_cells);
    if (counted == NULL)
    {
        return;
    }
    olds_lines.start = counted;
    olds_lines.size = counted + olds;
    news_lines.start = counted + (2u * olds);
    news_lines.size = counted + (2u * olds) + news;
    (void)EMBED_CALL(existing_lines, DeployStandingCtx, .lines = &olds_lines, .size = args->old_size, .most = olds);
    (void)EMBED_CALL(existing_lines, DeployStandingCtx, .lines = &news_lines, .size = args->new_size, .most = news);
    unsigned int head = 0u;
    while ((head < olds) && (head < news) &&
           EMBED_CALL(existing_same, DeployStandingCtx, .old = &olds_lines, .one = head, .new_lines = &news_lines,
                      .other = head))
    {
        head += 1u;
    }
    unsigned int tail = 0u;
    while ((head + tail < olds) && (head + tail < news) &&
           EMBED_CALL(existing_same, DeployStandingCtx, .old = &olds_lines, .one = olds - tail - 1u,
                      .new_lines = &news_lines, .other = news - tail - 1u))
    {
        tail += 1u;
    }
    char mark[32];
    VerbaLinea mark_line = {mark, sizeof(mark), 0ULL};
    mmgr_line_put(&mark_line, "@@ line ");
    mmgr_line_decimal(&mark_line, head + 1u, 1u);
    EMBED_CALL(existing_line_print, DeployStandingCtx, .mark = "", .text = mark, .size = mark_line.at);
    for (unsigned int at = head; at < olds - tail; at += 1u)
    {
        EMBED_CALL(existing_line_print, DeployStandingCtx, .mark = "- ", .text = args->old + olds_lines.start[at],
                   .size = olds_lines.size[at]);
    }
    for (unsigned int at = head; at < news - tail; at += 1u)
    {
        EMBED_CALL(existing_line_print, DeployStandingCtx, .mark = "+ ", .text = args->new_text + news_lines.start[at],
                   .size = news_lines.size[at]);
    }
    g_cli_cells->persistent_buf_release(counted, &g_cli_deploy_cells);
}

// the copy of `path` the place at `index` has read as text into a cell of the most that is compared and one byte past
// it, its size into `size`, where it is text of 64 KB at most: the cell, or NULL where it is not
EMBED_INLINE char *existing_theirs(const DeployStandingCtx *args)
{
    const char *path = args->path;
    unsigned long long *size = args->size_pointer;
    const CliDeployPosition *const place = &g_cli_deploy.place[args->index];
    const unsigned long long capacity = CLI_DEPLOY_COMPARED + 2ULL;
    char *const theirs = g_cli_cells->temporary_buf_alloc(capacity, &g_cli_deploy_cells);
    VerbaLinea text = mmgr_line_over(theirs, capacity);
    int read = 0;
    if (place->here)
    {
        unsigned long long mapped = 0ULL;
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                      .size_pointer = &mapped, .owner = &g_cli_deploy_cells);
        read = (bytes != NULL) && (mapped <= CLI_DEPLOY_COMPARED);
        if (read)
        {
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &text, .text_bytes = bytes,
                       .length = mapped);
        }
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_deploy_cells);
    }
    else
    {
        char line[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea command = {line, sizeof(line), 0ULL};
        mmgr_line_put(&command, "head -c 65537 ");
        EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), .out = &command);
        mmgr_line_finish(&command);
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea none = {why, sizeof(why), 0ULL};
        read = EMBED_CALL(cli_deploy_size_internal.remote_run, CliDeploySizeCtx, .place = place, .line_text = line,
                          .input = NULL, .size = 0ULL, .out = &text, .err_msg = &none);
    }
    *size = text.at;
    if (!read || !mmgr_line_ok(&text) || (*size > CLI_DEPLOY_COMPARED) ||
        (memor.chr(&(MemoriaCfg){.src = theirs, .bytes = (size_t)*size, .val = 0u}) != NULL))
    {
        g_cli_cells->persistent_buf_release(theirs, &g_cli_deploy_cells);
        return NULL;
    }
    return theirs;
}

// the file changed at the place at `index` printed, and its differences where both copies are text
EMBED_INLINE void existing_change(const DeployStandingCtx *args)
{
    unsigned int index = args->index;
    const char *path = args->path;
    char place[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea position_line = {place, sizeof(place), 0ULL};
    EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = &g_cli_deploy.place[index],
               .out = &position_line);
    char line[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea print_line = {line, sizeof(line), 0ULL};
    mmgr_line_put(&print_line, path);
    mmgr_line_put(&print_line, " changed at ");
    mmgr_line_put_n(&print_line, place, position_line.at);
    mmgr_line_put(&print_line, args->replace ? " since it was last sent there, and is replaced:"
                                             : " since it was last sent there, and is retained:");
    EMBED_CALL(cli_deploy_internal.print, CliDeployCtx, .line = &print_line);
    char there[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea there_line = {there, sizeof(there), 0ULL};
    mmgr_line_put(&there_line, args->folder);
    mmgr_line_ch(&there_line, '/');
    mmgr_line_put(&there_line, path);
    mmgr_line_finish(&there_line);
    unsigned long long theirs_size = 0ULL;
    char *const theirs =
        EMBED_CALL(existing_theirs, DeployStandingCtx, .index = index, .path = there, .size_pointer = &theirs_size);
    const CliDeployRun *const run = &g_cli_deploy;
    for (unsigned int sent = 0u; (theirs != NULL) && (sent < run->sents); sent += 1u)
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = run->sent[sent].path, .other = path, .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        char full[(2u * CLI_PATH_MAX_LEN) + 2u];
        VerbaLinea full_line = {full, sizeof(full), 0ULL};
        EMBED_CALL(cli_deploy_out_index_internal.full, CliDeployOutIndexCtx, .index = sent, .out = &full_line);
        mmgr_line_finish(&full_line);
        unsigned long long size = 0ULL;
        const unsigned char *const ours_index = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = full,
                                                           .size_pointer = &size, .owner = &g_cli_deploy_cells);
        if ((ours_index != NULL) && EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg,
                                               .bytes = (const char *)ours_index, .size = size))
        {
            EMBED_CALL(cli_deploy_old_internal.difference, CliDeployOldCtx, .old = theirs, .old_size = theirs_size,
                       .new_text = (const char *)ours_index, .new_size = size);
        }
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = ours_index,
                   .owner = &g_cli_deploy_cells);
        break;
    }
    g_cli_cells->persistent_buf_release(theirs, &g_cli_deploy_cells);
}

// a file at the place at `index` that the tree no longer has printed, and what becomes of it
EMBED_INLINE void existing_remove(const DeployStandingCtx *args)
{
    char place[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea position_line = {place, sizeof(place), 0ULL};
    EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = &g_cli_deploy.place[args->index],
               .out = &position_line);
    char line[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea print_line = {line, sizeof(line), 0ULL};
    mmgr_line_put(&print_line, args->path);
    mmgr_line_put(&print_line, " is at ");
    mmgr_line_put_n(&print_line, place, position_line.at);
    mmgr_line_put(&print_line, " and no longer in the tree, and is ");
    mmgr_line_put(&print_line, !args->remove ? "retained" : args->releases ? "left out of the new release" : "deleted");
    EMBED_CALL(cli_deploy_internal.print, CliDeployCtx, .line = &print_line);
}

// the files the place has that changed there since they were last sent, with their hashes there, or where `deleted`,
// those the tree no longer has, into `list` where it is not NULL, their texts stored in the run's pool: their count
EMBED_INLINE unsigned int existing_list(const DeployStandingCtx *args)
{
    CliDeployRun *const run = &g_cli_deploy;
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < g_cli_deploy_there_count; at += 1u)
    {
        const CliDeployPair *const there = &g_cli_deploy_there[at];
        const char *const ours = EMBED_CALL(cli_deploy_count_internal.ours, CliDeployCountCtx, .path = there->path);
        const char *const last =
            args->deleted ? NULL
                          : EMBED_CALL(cli_deploy_count_internal.find, CliDeployCountCtx, .pairs = g_cli_deploy_last,
                                       .count = g_cli_deploy_last_count, .path = there->path);
        const int list_entries =
            args->deleted
                ? ((ours == NULL) &&
                   (EMBED_CALL(cli_deploy_patterns_internal.left_out_by, CliDeployPatternsCtx, .patterns = run->pattern,
                               .count = run->patterns, .path = there->path) == NULL))
                : ((last != NULL) &&
                   !cellul.eq(&(CatenaFinitaCfg){.src = last, .other = there->text, .cap = CLI_CELLS_POOL}) &&
                   ((ours == NULL) ||
                    !cellul.eq(&(CatenaFinitaCfg){.src = ours, .other = there->text, .cap = CLI_CELLS_POOL})));
        if (list_entries && (args->list != NULL))
        {
            args->list[count].path = EMBED_CALL(
                cli_deploy_pool_internal.text_store, CliDeployPoolCtx, .pool = &run->pool, .text = there->path,
                .size = cellul.len(&(CatenaFinitaCfg){.src = there->path, .cap = CLI_CELLS_POOL}));
            args->list[count].text =
                args->deleted
                    ? NULL
                    : EMBED_CALL(cli_deploy_pool_internal.text_store, CliDeployPoolCtx, .pool = &run->pool,
                                 .text = there->text,
                                 .size = cellul.len(&(CatenaFinitaCfg){.src = there->text, .cap = CLI_CELLS_POOL}));
        }
        count += list_entries ? 1u : 0u;
    }
    return count;
}

int cli_deploy_existing(const CliDeployErrMsgIndexCtx *args)
{
    CliDeployRun *const run = &g_cli_deploy;
    if (!cli_deploy_err_msg_index_internal.hashes(args))
    {
        return 0;
    }
    cli_deploy_err_msg_index_internal.last_read(args);
    const unsigned int changed = EMBED_CALL(existing_list, DeployStandingCtx, .deleted = 0, .list = NULL);
    const unsigned int deleted = EMBED_CALL(existing_list, DeployStandingCtx, .deleted = 1, .list = NULL);
    run->changed_list[args->index] =
        (changed > 0u) ? EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &run->pool,
                                    .size = (unsigned long long)changed * sizeof(CliDeployPair))
                       : NULL;
    run->removed_list[args->index] =
        (deleted > 0u) ? EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &run->pool,
                                    .size = (unsigned long long)deleted * sizeof(CliDeployPair))
                       : NULL;
    if (!run->pool.refused)
    {
        run->changed[args->index] =
            EMBED_CALL(existing_list, DeployStandingCtx, .deleted = 0, .list = run->changed_list[args->index]);
        run->deleted[args->index] =
            EMBED_CALL(existing_list, DeployStandingCtx, .deleted = 1, .list = run->removed_list[args->index]);
    }
    if (run->pool.refused)
    {
        run->changed[args->index] = 0u;
        run->deleted[args->index] = 0u;
        mmgr_line_put(args->err_msg, "it has more files than orior_cli supports");
        return 0;
    }
    const int releases = !run->deployment.in_position && !run->place[args->index].here;
    for (unsigned int at = 0u; at < run->changed[args->index]; at += 1u)
    {
        EMBED_CALL(existing_change, DeployStandingCtx, .index = args->index, .folder = args->folder,
                   .path = run->changed_list[args->index][at].path, .replace = args->replace);
    }
    for (unsigned int at = 0u; at < run->deleted[args->index]; at += 1u)
    {
        EMBED_CALL(existing_remove, DeployStandingCtx, .index = args->index,
                   .path = run->removed_list[args->index][at].path, .releases = releases, .remove = args->remove);
    }
    return 1;
}

// a path and its hash written as a member of the record, pretty as serde_json writes it
EMBED_INLINE void existing_member(const DeployStandingCtx *args)
{
    mmgr_line_put(args->record, args->first ? "\n  " : ",\n  ");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->record, .text = args->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->record, ": ");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->record, .text = args->hash,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->hash, .cap = CLI_CELLS_POOL}));
}

void cli_deploy_record(const CliDeployErrMsgIndexCtx *args)
{
    const CliDeployRun *const run = &g_cli_deploy;
    char path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_deploy_index_path_internal.record_path, CliDeployIndexPathCtx, .index = args->index,
                    .path_out = path) ||
        !EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = folder))
    {
        return;
    }
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    const CliDeployPair *const preserved = args->replace ? NULL : run->changed_list[args->index];
    const unsigned int preserved_count = args->replace ? 0u : run->changed[args->index];
    // the record holds a path and a hash for each file, as a place's hashes do
    char *const record_text = g_cli_cells->temporary_buf_alloc(CLI_DEPLOY_HASHES, &g_cli_deploy_cells);
    VerbaLinea record = mmgr_line_over(record_text, CLI_DEPLOY_HASHES);
    mmgr_line_ch(&record, '{');
    unsigned int sent = 0u;
    unsigned int other = 0u;
    int first = 1;
    while ((sent < run->sents) || (other < preserved_count))
    {
        const int order = (sent == run->sents) ? 1
                          : (other == preserved_count)
                              ? -1
                              : EMBED_CALL(cli_deploy_left_internal.order, CliDeployLeftCtx,
                                           .left = run->sent[sent].path, .right = preserved[other].path);
        const char *const name = (order <= 0) ? run->sent[sent].path : preserved[other].path;
        const char *const hash = (order < 0) ? run->sent[sent].hash : preserved[other].text;
        EMBED_CALL(existing_member, DeployStandingCtx, .path = name, .hash = hash, .first = first, .record = &record);
        first = 0;
        sent += (order <= 0) ? 1u : 0u;
        other += (order >= 0) ? 1u : 0u;
    }
    mmgr_line_put(&record, first ? "}" : "\n}");
    if (mmgr_line_ok(&record))
    {
        (void)EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = path, .bytes = record.out,
                         .size = record.at, .fresh = 1);
    }
    g_cli_cells->persistent_buf_release(record_text, &g_cli_deploy_cells);
}
