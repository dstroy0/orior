// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_standing.c: what a place has against what was last sent there and what goes now: the files changed there
// since they were sent, each shown with its differences where both copies are text, and the files there the tree no
// longer has; and the record of the hashes last sent to a place written after a send
#include "../command_line/cells.h"
#include "../text/pieces.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"
#include "memoria_operor/memoria_operor.h"

// the most bytes of a copy at a place compared, and the most lines of a copy here compared
#define CLI_DEPLOY_COMPARED 65536u
#define CLI_DEPLOY_LINES (1u << 18u)

// The lines of a text: the text, and where each line starts and its length
typedef struct
{
    const char *text;
    unsigned long long *start;
    unsigned long long *size;
} CliDeployLines;

// the lines of `size` bytes of `lines->text`, as Rust's str::lines splits them, into its starts and sizes where they
// are not NULL, `most` at the most: their count, or more than `most` where there are more
static unsigned int standing_lines(const CliDeployLines *lines, unsigned long long size, unsigned int most)
{
    const char *const text = lines->text;
    unsigned int count = 0u;
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = text + at, .bytes = (size_t)(size - at), .val = (uint8_t)'\n'});
        unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        if (count == most)
        {
            return most + 1u;
        }
        const unsigned long long start = at;
        at = (newline != NULL) ? (end + 1u) : size;
        end -= ((newline != NULL) && (end > start) && (text[end - 1u] == '\r')) ? 1u : 0u;
        if (lines->start != NULL)
        {
            lines->start[count] = start;
            lines->size[count] = end - start;
        }
        count += 1u;
    }
    return count;
}

// whether line `one` of `old` and line `other` of `new_lines` are the same
static int standing_same(const CliDeployLines *old, unsigned int one, const CliDeployLines *new_lines,
                         unsigned int other)
{
    return (old->size[one] == new_lines->size[other]) &&
           (memor.cmp(&(MemoriaCfg){.src = old->text + old->start[one],
                                    .other = new_lines->text + new_lines->start[other],
                                    .bytes = (size_t)old->size[one]}) == 0);
}

// a line printed, two spaces, a mark and a space before `size` bytes of `text`
static void standing_line_printed(const char *mark, const char *text, unsigned long long size)
{
    char line[CLI_DEPLOY_COMPARED + 16u];
    VerbaLinea print_line = {line, sizeof(line), 0ull};
    mmgr_line_put(&print_line, "  ");
    mmgr_line_put(&print_line, mark);
    mmgr_line_put_n(&print_line, text, size);
    cli_deploy_print(&print_line);
}

void cli_deploy_difference(const char *old, unsigned long long old_size, const char *new_text,
                           unsigned long long new_size)
{
    CliDeployLines olds_lines = {old, NULL, NULL};
    CliDeployLines news_lines = {new_text, NULL, NULL};
    const unsigned int olds = standing_lines(&olds_lines, old_size, CLI_DEPLOY_COMPARED + 1u);
    const unsigned int news = standing_lines(&news_lines, new_size, CLI_DEPLOY_LINES);
    if ((olds > CLI_DEPLOY_COMPARED + 1u) || (news > CLI_DEPLOY_LINES))
    {
        return;
    }
    // each line's start and length, the old text's then the new's, in one cell of their count
    unsigned long long *const counted =
        g_cli_cells->persistent_buf_alloc(2ull * ((unsigned long long)olds + news + 1u) * sizeof(counted[0]), NULL);
    if (counted == NULL)
    {
        return;
    }
    olds_lines.start = counted;
    olds_lines.size = counted + olds;
    news_lines.start = counted + (2u * olds);
    news_lines.size = counted + (2u * olds) + news;
    (void)standing_lines(&olds_lines, old_size, olds);
    (void)standing_lines(&news_lines, new_size, news);
    unsigned int head = 0u;
    while ((head < olds) && (head < news) && standing_same(&olds_lines, head, &news_lines, head))
    {
        head += 1u;
    }
    unsigned int tail = 0u;
    while ((head + tail < olds) && (head + tail < news) &&
           standing_same(&olds_lines, olds - tail - 1u, &news_lines, news - tail - 1u))
    {
        tail += 1u;
    }
    char mark[32];
    VerbaLinea mark_line = {mark, sizeof(mark), 0ull};
    mmgr_line_put(&mark_line, "@@ line ");
    mmgr_line_decimal(&mark_line, head + 1u, 1u);
    standing_line_printed("", mark, mark_line.at);
    for (unsigned int at = head; at < olds - tail; at += 1u)
    {
        standing_line_printed("- ", old + olds_lines.start[at], olds_lines.size[at]);
    }
    for (unsigned int at = head; at < news - tail; at += 1u)
    {
        standing_line_printed("+ ", new_text + news_lines.start[at], news_lines.size[at]);
    }
    cli_cell_release(counted, NULL);
}

// the copy of `path` the place at `index` has read as text into a cell of its size, that size into `size`, where it
// is text of 64 KB at most: the cell, or NULL where it is not
static char *standing_theirs(unsigned int index, const char *path, unsigned long long *size)
{
    const CliDeployPlace *const place = &g_cli_deploy.place[index];
    char piece[CLI_DEPLOY_PIECE];
    CliPieces pieces;
    VerbaLinea text;
    cli_pieces_begun(&pieces, &text, piece, sizeof(piece));
    int read = 0;
    if (place->here)
    {
        unsigned long long mapped = 0ull;
        const unsigned char *const bytes = cli_file_read(path, &mapped);
        read = (bytes != NULL) && (mapped <= CLI_DEPLOY_COMPARED);
        if (read)
        {
            cli_lossy(&text, bytes, mapped);
        }
        if (bytes != NULL)
        {
            cli_file_let_go(bytes, mapped);
        }
    }
    else
    {
        char line[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea command = {line, sizeof(line), 0ull};
        mmgr_line_put(&command, "head -c 65537 ");
        cli_link_quote(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), &command);
        mmgr_line_finish(&command);
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea none = {why, sizeof(why), 0ull};
        read = cli_deploy_remote_run(place, line, NULL, 0ull, &text, &none);
    }
    char *const theirs = cli_pieces_joined(&pieces, &text, "a copy at a place", size);
    if (!read || (theirs == NULL) || (*size > CLI_DEPLOY_COMPARED) ||
        (memor.chr(&(MemoriaCfg){.src = theirs, .bytes = (size_t)*size, .val = 0u}) != NULL))
    {
        cli_cell_release(theirs, NULL);
        return NULL;
    }
    return theirs;
}

// the file changed at the place at `index` printed, and its differences where both copies are text
static void standing_changed(unsigned int index, const char *folder, const char *path, int replace)
{
    char place[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea place_line = {place, sizeof(place), 0ull};
    cli_deploy_place_name(&g_cli_deploy.place[index], &place_line);
    char line[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea print_line = {line, sizeof(line), 0ull};
    mmgr_line_put(&print_line, path);
    mmgr_line_put(&print_line, " changed at ");
    mmgr_line_put_n(&print_line, place, place_line.at);
    mmgr_line_put(&print_line, replace ? " since it was last sent there, and is replaced:"
                                       : " since it was last sent there, and is retained:");
    cli_deploy_print(&print_line);
    char there[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea there_line = {there, sizeof(there), 0ull};
    mmgr_line_put(&there_line, folder);
    mmgr_line_ch(&there_line, '/');
    mmgr_line_put(&there_line, path);
    mmgr_line_finish(&there_line);
    unsigned long long theirs_size = 0ull;
    char *const theirs = standing_theirs(index, there, &theirs_size);
    const CliDeployRun *const run = &g_cli_deploy;
    for (unsigned int sent = 0u; (theirs != NULL) && (sent < run->sents); sent += 1u)
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = run->sent[sent].path, .other = path, .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        char full[(2u * CLI_PATH_MAX_LEN) + 2u];
        VerbaLinea full_line = {full, sizeof(full), 0ull};
        cli_deploy_full(sent, &full_line);
        mmgr_line_finish(&full_line);
        unsigned long long size = 0ull;
        const unsigned char *const mine = cli_file_read(full, &size);
        if ((mine != NULL) && cli_utf8_whole((const char *)mine, size))
        {
            cli_deploy_difference(theirs, theirs_size, (const char *)mine, size);
        }
        if (mine != NULL)
        {
            cli_file_let_go(mine, size);
        }
        break;
    }
    cli_cell_release(theirs, NULL);
}

// a file at the place at `index` that the tree no longer has printed, and what becomes of it
static void standing_gone(unsigned int index, const char *path, int releases, int remove)
{
    char place[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea place_line = {place, sizeof(place), 0ull};
    cli_deploy_place_name(&g_cli_deploy.place[index], &place_line);
    char line[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea print_line = {line, sizeof(line), 0ull};
    mmgr_line_put(&print_line, path);
    mmgr_line_put(&print_line, " is at ");
    mmgr_line_put_n(&print_line, place, place_line.at);
    mmgr_line_put(&print_line, " and no longer in the tree, and is ");
    mmgr_line_put(&print_line, !remove ? "retained" : releases ? "left out of the new release" : "deleted");
    cli_deploy_print(&print_line);
}

// the files the place has that changed there since they were last sent, with their hashes there, or where `gone`,
// those the tree no longer has, into `list` where it is not NULL, their texts stored in the run's pool: their count
static unsigned int standing_listed(int gone, CliDeployPair *list)
{
    CliDeployRun *const run = &g_cli_deploy;
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < g_cli_deploy_there_count; at += 1u)
    {
        const CliDeployPair *const there = &g_cli_deploy_there[at];
        const char *const ours = cli_deploy_ours(there->path);
        const char *const last = gone ? NULL : cli_deploy_find(g_cli_deploy_last, g_cli_deploy_last_count, there->path);
        const int listed =
            gone ? ((ours == NULL) && (cli_deploy_left_out_by(run->pattern, run->patterns, there->path) == NULL))
                 : ((last != NULL) &&
                    !cellul.eq(&(CatenaFinitaCfg){.src = last, .other = there->text, .cap = CLI_CELLS_POOL}) &&
                    ((ours == NULL) ||
                     !cellul.eq(&(CatenaFinitaCfg){.src = ours, .other = there->text, .cap = CLI_CELLS_POOL})));
        if (listed && (list != NULL))
        {
            list[count].path = cli_deploy_text_stored(
                &run->pool, there->path, cellul.len(&(CatenaFinitaCfg){.src = there->path, .cap = CLI_CELLS_POOL}));
            list[count].text =
                gone
                    ? NULL
                    : cli_deploy_text_stored(&run->pool, there->text,
                                             cellul.len(&(CatenaFinitaCfg){.src = there->text, .cap = CLI_CELLS_POOL}));
        }
        count += listed ? 1u : 0u;
    }
    return count;
}

int cli_deploy_standing(unsigned int index, const char *folder, int replace, int remove, VerbaLinea *err_msg)
{
    CliDeployRun *const run = &g_cli_deploy;
    if (!cli_deploy_hashes(index, folder, err_msg))
    {
        return 0;
    }
    cli_deploy_last_read(index);
    const unsigned int changed = standing_listed(0, NULL);
    const unsigned int gone = standing_listed(1, NULL);
    run->changed_list[index] =
        (changed > 0u) ? cli_deploy_placed(&run->pool, (unsigned long long)changed * sizeof(CliDeployPair)) : NULL;
    run->gone_list[index] =
        (gone > 0u) ? cli_deploy_placed(&run->pool, (unsigned long long)gone * sizeof(CliDeployPair)) : NULL;
    if (!run->pool.refused)
    {
        run->changed[index] = standing_listed(0, run->changed_list[index]);
        run->gone[index] = standing_listed(1, run->gone_list[index]);
    }
    if (run->pool.refused)
    {
        run->changed[index] = 0u;
        run->gone[index] = 0u;
        mmgr_line_put(err_msg, "it has more files than orior_cli supports");
        return 0;
    }
    const int releases = !run->deployment.in_place && !run->place[index].here;
    for (unsigned int at = 0u; at < run->changed[index]; at += 1u)
    {
        standing_changed(index, folder, run->changed_list[index][at].path, replace);
    }
    for (unsigned int at = 0u; at < run->gone[index]; at += 1u)
    {
        standing_gone(index, run->gone_list[index][at].path, releases, remove);
    }
    return 1;
}

// a path and its hash written as a member of the record, pretty as serde_json writes it
static void standing_member(const char *path, const char *hash, int first, VerbaLinea *record)
{
    mmgr_line_put(record, first ? "\n  " : ",\n  ");
    cli_json_quoted(record, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(record, ": ");
    cli_json_quoted(record, hash, cellul.len(&(CatenaFinitaCfg){.src = hash, .cap = CLI_CELLS_POOL}));
}

void cli_deploy_recorded(unsigned int index, int replace)
{
    const CliDeployRun *const run = &g_cli_deploy;
    char path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    if (!cli_deploy_record_path(index, path) || !cli_path_parent(path, folder))
    {
        return;
    }
    (void)query_folder_made(folder);
    char piece[CLI_DEPLOY_PIECE];
    CliPieces pieces;
    VerbaLinea record;
    cli_pieces_begun(&pieces, &record, piece, sizeof(piece));
    mmgr_line_ch(&record, '{');
    const CliDeployPair *const preserved = replace ? NULL : run->changed_list[index];
    const unsigned int preserved_count = replace ? 0u : run->changed[index];
    unsigned int sent = 0u;
    unsigned int other = 0u;
    int first = 1;
    while ((sent < run->sents) || (other < preserved_count))
    {
        const int order = (sent == run->sents)         ? 1
                          : (other == preserved_count) ? -1
                                                       : cli_deploy_order(run->sent[sent].path, preserved[other].path);
        const char *const name = (order <= 0) ? run->sent[sent].path : preserved[other].path;
        const char *const hash = (order < 0) ? run->sent[sent].hash : preserved[other].text;
        standing_member(name, hash, first, &record);
        first = 0;
        sent += (order <= 0) ? 1u : 0u;
        other += (order >= 0) ? 1u : 0u;
    }
    mmgr_line_put(&record, first ? "}" : "\n}");
    unsigned long long size = 0ull;
    char *const joined = cli_pieces_joined(&pieces, &record, "a deployment's record", &size);
    if (joined != NULL)
    {
        (void)cli_file_appended(path, joined, size, 1);
    }
    cli_cell_release(joined, NULL);
}
