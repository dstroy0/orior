// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_send.c: a deployment's files sent to a place: on a machine a tar stream into a new release beside the one it
// runs, the files it retains and those the tree no longer has that it retains copied into the release from the one it
// runs, or into its folder itself; to a folder here each file copied into it. And a machine's `current` turned to a
// release, its old releases let go of, and each place turned back to the release before the one it runs
#include "../command_line/cells.h"
#include "../text/pieces.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"

#include <stdlib.h>

// a machine's releases, oldest first, in a cell of the count of the lines of its listing, which is in a cell of its
// size, each given back as the next machine's are read and as their reader ends
static const char **s_releases;
static unsigned int s_release_count;
static char *s_releases_listing;

// a machine's releases and its listing given back
static void send_releases_given(void)
{
    g_cli_cells->persistent_buf_release((void *)s_releases, NULL);
    s_releases = NULL;
    s_release_count = 0u;
    g_cli_cells->persistent_buf_release(s_releases_listing, NULL);
    s_releases_listing = NULL;
}

// the shell line written on `line` run on `place`, joined into a cell of its size, `longer` given where that cell
// could not be taken: 1, or 0 with why given
static int send_line_run(const CliDeployPlace *place, CliPieces *pieces, VerbaLinea *line, const char *longer,
                         VerbaLinea *err_msg)
{
    unsigned long long size = 0ull;
    char *const joined = cli_pieces_joined(pieces, line, "a deployment's shell line", &size);
    if (joined == NULL)
    {
        mmgr_line_put(err_msg, longer);
        return 0;
    }
    VerbaLinea nothing = {NULL, 0ull, 0ull};
    const int ran = cli_deploy_remote_run(place, joined, NULL, 0ull, &nothing, err_msg);
    g_cli_cells->persistent_buf_release(joined, NULL);
    return ran;
}

// `size` bytes of `text`, then `path`, as one word of a shell line
static void send_quoted(VerbaLinea *out, const char *text, const char *path)
{
    char joined[(2u * CLI_PATH_MAX_LEN) + 64u];
    VerbaLinea line = {joined, sizeof(joined), 0ull};
    mmgr_line_put(&line, text);
    mmgr_line_put(&line, path);
    cli_link_quote(joined, line.at, out);
}

void cli_deploy_switch_line(const char *folder, const char *release, VerbaLinea *out)
{
    mmgr_line_put(out, "cd ");
    cli_link_quote(folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), out);
    mmgr_line_put(out, " && ln -sfn ");
    send_quoted(out, "releases/", release);
    mmgr_line_put(out, " current.orior && { mv -fT current.orior current 2>/dev/null || { rm -f current && mv "
                       "current.orior current; }; }");
}

// the files retained at the place at `index`, those changed there where `replace` is 0, sorted
static const CliDeployPair *send_preserved(unsigned int index, int replace, unsigned int *count)
{
    const CliDeployRun *const run = &g_cli_deploy;
    *count = replace ? 0u : run->changed[index];
    return run->changed_list[index];
}

// the files the run sends but those retained copied into the folder `folder` of this machine: 1, or 0 with why given
static int send_here(const char *folder, const CliDeployPair *preserved, unsigned int preserved_count,
                     VerbaLinea *err_msg)
{
    const CliDeployRun *const run = &g_cli_deploy;
    for (unsigned int index = 0u; index < run->sents; index += 1u)
    {
        int skipped = 0;
        for (unsigned int at = 0u; !skipped && (at < preserved_count); at += 1u)
        {
            skipped = cellul.eq(
                &(CatenaFinitaCfg){.src = preserved[at].path, .other = run->sent[index].path, .cap = CLI_CELLS_POOL});
        }
        if (skipped)
        {
            continue;
        }
        char to[(2u * CLI_PATH_MAX_LEN) + 2u];
        char parent[CLI_PATH_MAX_LEN];
        char full[(2u * CLI_PATH_MAX_LEN) + 2u];
        VerbaLinea to_line = {to, sizeof(to), 0ull};
        VerbaLinea full_line = {full, sizeof(full), 0ull};
        mmgr_line_put(&to_line, folder);
        mmgr_line_ch(&to_line, '/');
        mmgr_line_put(&to_line, run->sent[index].path);
        cli_deploy_full(index, &full_line);
        if ((mmgr_line_finish(&to_line) == 0ull) || (mmgr_line_finish(&full_line) == 0ull) ||
            (cli_path_parent(to, parent) && !query_folder_made(parent)))
        {
            mmgr_line_put(err_msg, to);
            mmgr_line_put(err_msg, ": its folder could not be made");
            return 0;
        }
        if (!query_file_copied(full, to))
        {
            mmgr_line_put(err_msg, to);
            mmgr_line_put(err_msg, ": it could not be written");
            return 0;
        }
    }
    return 1;
}

// the files retained and the files gone that are retained copied into the release `into` from the one the place at
// `index` runs: 1, or 0 with why given
static int send_carried(unsigned int index, const char *into, const CliDeployPair *preserved,
                        unsigned int preserved_count, int remove, VerbaLinea *err_msg)
{
    const CliDeployRun *const run = &g_cli_deploy;
    const CliDeployPlace *const place = &run->place[index];
    const unsigned int gone = remove ? 0u : run->gone[index];
    if (preserved_count + gone == 0u)
    {
        return 1;
    }
    char piece[CLI_DEPLOY_PIECE];
    CliPieces pieces;
    VerbaLinea line;
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
    char current[CLI_PATH_MAX_LEN + 16u];
    VerbaLinea current_line = {current, sizeof(current), 0ull};
    mmgr_line_put(&current_line, cli_deploy_place_folder(place));
    mmgr_line_put(&current_line, "/current/");
    mmgr_line_finish(&current_line);
    for (unsigned int at = 0u; at < preserved_count + gone; at += 1u)
    {
        const char *const path =
            (at < preserved_count) ? preserved[at].path : run->gone_list[index][at - preserved_count].path;
        char to[(2u * CLI_PATH_MAX_LEN) + 2u];
        VerbaLinea to_line = {to, sizeof(to), 0ull};
        mmgr_line_put(&to_line, into);
        mmgr_line_ch(&to_line, '/');
        mmgr_line_put(&to_line, path);
        mmgr_line_finish(&to_line);
        unsigned long long slash = to_line.at;
        while ((slash > 0u) && (to[slash] != '/'))
        {
            slash -= 1u;
        }
        mmgr_line_put(&line, (at > 0u) ? " && mkdir -p " : "mkdir -p ");
        cli_link_quote(to, slash, &line);
        mmgr_line_put(&line, " && cp -p ");
        send_quoted(&line, current, path);
        mmgr_line_ch(&line, ' ');
        cli_link_quote(to, to_line.at, &line);
    }
    return send_line_run(place, &pieces, &line, "the line that copies the files retained is longer than orior_cli runs",
                         err_msg);
}

int cli_deploy_send_to(unsigned int index, int releases, const char *release, unsigned long long time, int replace,
                       int remove, VerbaLinea *err_msg)
{
    const CliDeployPlace *const place = &g_cli_deploy.place[index];
    unsigned int preserved_count = 0u;
    const CliDeployPair *const preserved = send_preserved(index, replace, &preserved_count);
    if (place->here)
    {
        return send_here(place->path, preserved, preserved_count, err_msg);
    }
    char into[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea into_line = {into, sizeof(into), 0ull};
    mmgr_line_put(&into_line, place->address.folder);
    if (releases)
    {
        mmgr_line_put(&into_line, "/releases/");
        mmgr_line_put(&into_line, release);
    }
    mmgr_line_finish(&into_line);
    char tar[CLI_PATH_MAX_LEN];
    VerbaLinea tar_line = {tar, sizeof(tar), 0ull};
    cli_temp_folder(tar);
    tar_line.at = cellul.len(&(CatenaFinitaCfg){.src = tar, .cap = sizeof(tar)});
    mmgr_line_put(&tar_line, "/orior-deploy-");
    mmgr_line_decimal(&tar_line, cli_process_own(), 1u);
    mmgr_line_ch(&tar_line, '-');
    mmgr_line_decimal(&tar_line, index, 1u);
    mmgr_line_put(&tar_line, ".tar");
    mmgr_line_finish(&tar_line);
    if (!cli_deploy_tar(tar, preserved, preserved_count, time, err_msg))
    {
        (void)query_removed(tar);
        return 0;
    }
    char line[(4u * CLI_PATH_MAX_LEN) + 64u];
    VerbaLinea command = {line, sizeof(line), 0ull};
    mmgr_line_put(&command, "mkdir -p ");
    cli_link_quote(into, into_line.at, &command);
    mmgr_line_put(&command, " && tar -x -f - -C ");
    cli_link_quote(into, into_line.at, &command);
    mmgr_line_finish(&command);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_read(tar, &size);
    VerbaLinea none = {NULL, 0ull, 0ull};
    const int sent = (bytes != NULL) && cli_deploy_remote_run(place, line, (const char *)bytes, size, &none, err_msg);
    if (bytes == NULL)
    {
        mmgr_line_put(err_msg, tar);
        mmgr_line_put(err_msg, ": it could not be read");
    }
    else
    {
        cli_file_let_go(bytes, size);
    }
    (void)query_removed(tar);
    return sent && (!releases || send_carried(index, into, preserved, preserved_count, remove, err_msg));
}

// two releases in the order of their names
static int send_order(const void *one, const void *other)
{
    return cli_deploy_order(*(const char *const *)one, *(const char *const *)other);
}

// the releases of the machine at `index`, oldest first, into s_releases, and the one `current` names, NULL for none,
// those before given back: 1, or 0 with why given
static int send_releases(unsigned int index, const char **current, VerbaLinea *err_msg)
{
    const CliDeployPlace *const place = &g_cli_deploy.place[index];
    send_releases_given();
    *current = NULL;
    char line[CLI_PATH_MAX_LEN + 160u];
    VerbaLinea command = {line, sizeof(line), 0ull};
    mmgr_line_put(&command, "cd ");
    cli_link_quote(place->address.folder,
                   cellul.len(&(CatenaFinitaCfg){.src = place->address.folder, .cap = CLI_CELLS_POOL}), &command);
    mmgr_line_put(&command, " 2>/dev/null || exit 0; ls -1 releases 2>/dev/null; echo '<current>'; readlink current "
                            "2>/dev/null; true");
    mmgr_line_finish(&command);
    char piece[CLI_DEPLOY_PIECE];
    CliPieces pieces;
    VerbaLinea listing;
    cli_pieces_begun(&pieces, &listing, piece, sizeof(piece));
    const int ran = cli_deploy_remote_run(place, line, NULL, 0ull, &listing, err_msg);
    unsigned long long size = 0ull;
    s_releases_listing = cli_pieces_joined(&pieces, &listing, "a machine's releases", &size);
    if (!ran)
    {
        send_releases_given();
        return 0;
    }
    if (s_releases_listing == NULL)
    {
        mmgr_line_put(err_msg, "it has more files than orior_cli supports");
        return 0;
    }
    static const char s_mark[] = "<current>";
    char *const mark = (char *)cellul.find(
        &(CatenaFinitaCfg){.src = s_releases_listing, .cap = size + 1u, .other = s_mark, .other_cap = sizeof(s_mark)});
    char *after = (mark != NULL) ? (mark + 9) : NULL;
    if (mark != NULL)
    {
        *mark = '\0';
    }
    unsigned int lines = 1u;
    for (const char *at = s_releases_listing; *at != '\0'; at += 1)
    {
        lines += (*at == '\n') ? 1u : 0u;
    }
    s_releases = g_cli_cells->persistent_buf_alloc((unsigned long long)lines * sizeof(s_releases[0]), NULL);
    if (s_releases == NULL)
    {
        send_releases_given();
        mmgr_line_put(err_msg, "it has more files than orior_cli supports");
        return 0;
    }
    for (char *at = s_releases_listing; (*at != '\0') && (s_release_count < lines);)
    {
        char *end = at;
        while ((*end != '\0') && (*end != '\n'))
        {
            end += 1;
        }
        const int more = (*end == '\n');
        *end = '\0';
        char *start = at;
        while (cellul.ws(&(CatenaFinitaCfg){.src = start}))
        {
            start += 1;
        }
        char *last = end;
        while ((last > start) && cellul.ws(&(CatenaFinitaCfg){.src = last - 1}))
        {
            last -= 1;
        }
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
        char *last = name + cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
        while ((last > name) && ((last[-1] == '/') || cellul.ws(&(CatenaFinitaCfg){.src = last - 1})))
        {
            last -= 1;
        }
        *last = '\0';
        *current = name;
    }
    return 1;
}

void cli_deploy_prune(unsigned int index, unsigned long long retain)
{
    const CliDeployPlace *const place = &g_cli_deploy.place[index];
    const char *current = NULL;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea none = {why, sizeof(why), 0ull};
    if (!send_releases(index, &current, &none))
    {
        return;
    }
    const unsigned long long newest = (retain > 1u) ? retain : 1u;
    char piece[CLI_DEPLOY_PIECE];
    CliPieces pieces;
    VerbaLinea line;
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
    mmgr_line_put(&line, "cd ");
    send_quoted(&line, place->address.folder, "/releases");
    mmgr_line_put(&line, " && rm -rf --");
    unsigned int old = 0u;
    for (unsigned int at = s_release_count; at > 0u; at -= 1u)
    {
        const char *const name = s_releases[at - 1u];
        if (((unsigned long long)(s_release_count - at) >= newest) &&
            ((current == NULL) || !cellul.eq(&(CatenaFinitaCfg){.src = current, .other = name, .cap = CLI_CELLS_POOL})))
        {
            mmgr_line_ch(&line, ' ');
            cli_link_quote(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), &line);
            old += 1u;
        }
    }
    send_releases_given();
    if (old == 0u)
    {
        cli_pieces_given(&pieces);
        return;
    }
    if (!send_line_run(place, &pieces, &line, "", &none))
    {
        return;
    }
    char print_buf[(2u * CLI_PATH_MAX_LEN) + 64u];
    VerbaLinea print_line = {print_buf, sizeof(print_buf), 0ull};
    cli_deploy_place_name(place, &print_line);
    mmgr_line_put(&print_line, " let go of ");
    mmgr_line_decimal(&print_line, old, 1u);
    mmgr_line_put(&print_line, " old releases");
    cli_deploy_print(&print_line);
}

int cli_deploy_roll_back(VerbaLinea *err_msg)
{
    CliDeployRun *const run = &g_cli_deploy;
    // the release each place turns back to, stored in the run's pool
    const char **const back = cli_deploy_placed(&run->pool, (unsigned long long)run->places * sizeof(back[0]));
    if (back == NULL)
    {
        mmgr_line_put(err_msg, "no place was rolled back");
        return 0;
    }
    for (unsigned int index = 0u; index < run->places; index += 1u)
    {
        const CliDeployPlace *const place = &run->place[index];
        if (run->deployment.in_place || place->here)
        {
            cli_deploy_place_name(place, err_msg);
            mmgr_line_put(err_msg,
                          " takes the files in place and retains no release before them to go back to: no place "
                          "was rolled back");
            return 0;
        }
        const char *current = NULL;
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea why_line = {why, sizeof(why), 0ull};
        if (!send_releases(index, &current, &why_line))
        {
            cli_deploy_place_name(place, err_msg);
            mmgr_line_put(err_msg, ": ");
            mmgr_line_put_n(err_msg, why, why_line.at);
            return 0;
        }
        unsigned int at = 0u;
        while ((current != NULL) && (at < s_release_count) &&
               !cellul.eq(&(CatenaFinitaCfg){.src = s_releases[at], .other = current, .cap = CLI_CELLS_POOL}))
        {
            at += 1u;
        }
        if ((current == NULL) || (at == s_release_count) || (at == 0u))
        {
            send_releases_given();
            cli_deploy_place_name(place, err_msg);
            mmgr_line_put(err_msg, " has no release before the one it runs: no place was rolled back");
            return 0;
        }
        back[index] =
            cli_deploy_text_stored(&run->pool, s_releases[at - 1u],
                                   cellul.len(&(CatenaFinitaCfg){.src = s_releases[at - 1u], .cap = CLI_CELLS_POOL}));
        send_releases_given();
    }
    for (unsigned int index = 0u; index < run->places; index += 1u)
    {
        const CliDeployPlace *const place = &run->place[index];
        char line[(2u * CLI_PATH_MAX_LEN) + 256u];
        VerbaLinea command = {line, sizeof(line), 0ull};
        cli_deploy_switch_line(place->address.folder, back[index], &command);
        mmgr_line_finish(&command);
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea why_line = {why, sizeof(why), 0ull};
        VerbaLinea none = {NULL, 0ull, 0ull};
        if (!cli_deploy_remote_run(place, line, NULL, 0ull, &none, &why_line))
        {
            cli_deploy_place_name(place, err_msg);
            mmgr_line_put(err_msg, ": ");
            mmgr_line_put_n(err_msg, why, why_line.at);
            return 0;
        }
        char print_buf[(2u * CLI_PATH_MAX_LEN) + 64u];
        VerbaLinea print_line = {print_buf, sizeof(print_buf), 0ull};
        cli_deploy_place_name(place, &print_line);
        mmgr_line_put(&print_line, " now runs release ");
        mmgr_line_put(&print_line, back[index]);
        cli_deploy_print(&print_line);
    }
    return 1;
}

int cli_deploy_removed(unsigned int index, VerbaLinea *err_msg)
{
    const CliDeployRun *const run = &g_cli_deploy;
    const CliDeployPlace *const place = &run->place[index];
    const unsigned int count = run->gone[index];
    if (count == 0u)
    {
        return 1;
    }
    const CliDeployPair *const gone = run->gone_list[index];
    if (place->here)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            char path[(2u * CLI_PATH_MAX_LEN) + 2u];
            VerbaLinea path_line = {path, sizeof(path), 0ull};
            mmgr_line_put(&path_line, place->path);
            mmgr_line_ch(&path_line, '/');
            mmgr_line_put(&path_line, gone[at].path);
            if ((mmgr_line_finish(&path_line) == 0ull) || !query_removed(path))
            {
                mmgr_line_put(err_msg, gone[at].path);
                mmgr_line_put(err_msg, ": it could not be removed");
                return 0;
            }
        }
    }
    else
    {
        char piece[CLI_DEPLOY_PIECE];
        CliPieces pieces;
        VerbaLinea line;
        cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
        mmgr_line_put(&line, "cd ");
        cli_link_quote(place->address.folder,
                       cellul.len(&(CatenaFinitaCfg){.src = place->address.folder, .cap = CLI_CELLS_POOL}), &line);
        mmgr_line_put(&line, " && rm -f --");
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            mmgr_line_ch(&line, ' ');
            cli_link_quote(gone[at].path, cellul.len(&(CatenaFinitaCfg){.src = gone[at].path, .cap = CLI_CELLS_POOL}),
                           &line);
        }
        if (!send_line_run(place, &pieces, &line, "the line that deletes the files gone is longer than orior_cli runs",
                           err_msg))
        {
            return 0;
        }
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char print_buf[(3u * CLI_PATH_MAX_LEN) + 32u];
        VerbaLinea print_line = {print_buf, sizeof(print_buf), 0ull};
        mmgr_line_put(&print_line, "deleted ");
        mmgr_line_put(&print_line, gone[at].path);
        mmgr_line_put(&print_line, " at ");
        cli_deploy_place_name(place, &print_line);
        cli_deploy_print(&print_line);
    }
    return 1;
}
