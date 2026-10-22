// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_send.c: a deployment's files sent to a place: on a machine a tar stream into a new release beside the one it
// runs, the files it keeps and those the tree no longer holds that it keeps copied into the release from the one it
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
static char *s_releases_said;

// a machine's releases and its listing given back
static void send_releases_given(void)
{
    cli_cell_given((void *)s_releases);
    s_releases = NULL;
    s_release_count = 0u;
    cli_cell_given(s_releases_said);
    s_releases_said = NULL;
}

// the shell line written on `line` asked of `place`, joined into a cell of its size, `longer` said where that cell
// could not be taken: 1, or 0 with why said
static int send_line_asked(const CliDeployPlace *place, CliPieces *pieces, CliLine *line, const char *longer,
                           CliLine *said)
{
    unsigned long long size = 0ull;
    char *const joined = cli_pieces_joined(pieces, line, "a deployment's shell line", &size);
    if (joined == NULL)
    {
        cli_line_text(said, longer);
        return 0;
    }
    CliLine nothing = {NULL, 0ull, 0ull};
    const int asked = cli_deploy_ask(place, joined, NULL, 0ull, &nothing, said);
    cli_cell_given(joined);
    return asked;
}

// `size` bytes of `text`, then `path`, as one word of a shell line
static void send_quoted(CliLine *out, const char *text, const char *path)
{
    char joined[(2u * CLI_PATH_LONGEST) + 64u];
    CliLine line = {joined, sizeof(joined), 0ull};
    cli_line_text(&line, text);
    cli_line_text(&line, path);
    cli_link_quote(joined, line.at, out);
}

void cli_deploy_switch_line(const char *folder, const char *release, CliLine *out)
{
    cli_line_text(out, "cd ");
    cli_link_quote(folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), out);
    cli_line_text(out, " && ln -sfn ");
    send_quoted(out, "releases/", release);
    cli_line_text(out, " current.orior && { mv -fT current.orior current 2>/dev/null || { rm -f current && mv "
                       "current.orior current; }; }");
}

// the files kept at the place at `index`, those changed there where `replace` is 0, sorted
static const CliDeployPair *send_kept(unsigned int index, int replace, unsigned int *count)
{
    const CliDeployRun *const run = &g_cli_deploy;
    *count = replace ? 0u : run->changed[index];
    return run->changed_list[index];
}

// the files the run sends but those kept copied into the folder `folder` of this machine: 1, or 0 with why said
static int send_here(const char *folder, const CliDeployPair *kept, unsigned int kept_count, CliLine *said)
{
    const CliDeployRun *const run = &g_cli_deploy;
    for (unsigned int index = 0u; index < run->sents; index += 1u)
    {
        int skipped = 0;
        for (unsigned int at = 0u; !skipped && (at < kept_count); at += 1u)
        {
            skipped = cellul.eq(
                &(CatenaFinitaCfg){.src = kept[at].path, .other = run->sent[index].path, .cap = CLI_CELLS_POOL});
        }
        if (skipped)
        {
            continue;
        }
        char to[(2u * CLI_PATH_LONGEST) + 2u];
        char parent[CLI_PATH_LONGEST];
        char full[(2u * CLI_PATH_LONGEST) + 2u];
        CliLine to_line = {to, sizeof(to), 0ull};
        CliLine full_line = {full, sizeof(full), 0ull};
        cli_line_text(&to_line, folder);
        cli_line_character(&to_line, '/');
        cli_line_text(&to_line, run->sent[index].path);
        cli_deploy_full(index, &full_line);
        if ((cli_line_closed(&to_line) == 0ull) || (cli_line_closed(&full_line) == 0ull) ||
            (cli_path_parent(to, parent) && !query_folder_made(parent)))
        {
            cli_line_text(said, to);
            cli_line_text(said, ": its folder could not be made");
            return 0;
        }
        if (!query_file_copied(full, to))
        {
            cli_line_text(said, to);
            cli_line_text(said, ": it could not be written");
            return 0;
        }
    }
    return 1;
}

// the files kept and the files gone that are kept copied into the release `into` from the one the place at `index`
// runs: 1, or 0 with why said
static int send_carried(unsigned int index, const char *into, const CliDeployPair *kept, unsigned int kept_count,
                        int remove, CliLine *said)
{
    const CliDeployRun *const run = &g_cli_deploy;
    const CliDeployPlace *const place = &run->place[index];
    const unsigned int gone = remove ? 0u : run->gone[index];
    if (kept_count + gone == 0u)
    {
        return 1;
    }
    char piece[CLI_DEPLOY_PIECE];
    CliPieces pieces;
    CliLine line;
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
    char current[CLI_PATH_LONGEST + 16u];
    CliLine current_line = {current, sizeof(current), 0ull};
    cli_line_text(&current_line, cli_deploy_place_folder(place));
    cli_line_text(&current_line, "/current/");
    cli_line_closed(&current_line);
    for (unsigned int at = 0u; at < kept_count + gone; at += 1u)
    {
        const char *const path = (at < kept_count) ? kept[at].path : run->gone_list[index][at - kept_count].path;
        char to[(2u * CLI_PATH_LONGEST) + 2u];
        CliLine to_line = {to, sizeof(to), 0ull};
        cli_line_text(&to_line, into);
        cli_line_character(&to_line, '/');
        cli_line_text(&to_line, path);
        cli_line_closed(&to_line);
        unsigned long long slash = to_line.at;
        while ((slash > 0u) && (to[slash] != '/'))
        {
            slash -= 1u;
        }
        cli_line_text(&line, (at > 0u) ? " && mkdir -p " : "mkdir -p ");
        cli_link_quote(to, slash, &line);
        cli_line_text(&line, " && cp -p ");
        send_quoted(&line, current, path);
        cli_line_character(&line, ' ');
        cli_link_quote(to, to_line.at, &line);
    }
    return send_line_asked(place, &pieces, &line, "the line that copies the files kept is longer than orior_cli runs",
                           said);
}

int cli_deploy_send_to(unsigned int index, int releases, const char *release, unsigned long long time, int replace,
                       int remove, CliLine *said)
{
    const CliDeployPlace *const place = &g_cli_deploy.place[index];
    unsigned int kept_count = 0u;
    const CliDeployPair *const kept = send_kept(index, replace, &kept_count);
    if (place->here)
    {
        return send_here(place->path, kept, kept_count, said);
    }
    char into[CLI_PATH_LONGEST + 64u];
    CliLine into_line = {into, sizeof(into), 0ull};
    cli_line_text(&into_line, place->address.folder);
    if (releases)
    {
        cli_line_text(&into_line, "/releases/");
        cli_line_text(&into_line, release);
    }
    cli_line_closed(&into_line);
    char tar[CLI_PATH_LONGEST];
    CliLine tar_line = {tar, sizeof(tar), 0ull};
    cli_temp_folder(tar);
    tar_line.at = cellul.len(&(CatenaFinitaCfg){.src = tar, .cap = sizeof(tar)});
    cli_line_text(&tar_line, "/orior-deploy-");
    cli_line_decimal(&tar_line, cli_process_own(), 1u);
    cli_line_character(&tar_line, '-');
    cli_line_decimal(&tar_line, index, 1u);
    cli_line_text(&tar_line, ".tar");
    cli_line_closed(&tar_line);
    if (!cli_deploy_tar(tar, kept, kept_count, time, said))
    {
        (void)query_removed(tar);
        return 0;
    }
    char line[(4u * CLI_PATH_LONGEST) + 64u];
    CliLine command = {line, sizeof(line), 0ull};
    cli_line_text(&command, "mkdir -p ");
    cli_link_quote(into, into_line.at, &command);
    cli_line_text(&command, " && tar -x -f - -C ");
    cli_link_quote(into, into_line.at, &command);
    cli_line_closed(&command);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(tar, &size);
    CliLine none = {NULL, 0ull, 0ull};
    const int sent = (bytes != NULL) && cli_deploy_ask(place, line, (const char *)bytes, size, &none, said);
    if (bytes == NULL)
    {
        cli_line_text(said, tar);
        cli_line_text(said, ": it could not be read");
    }
    else
    {
        cli_file_let_go(bytes, size);
    }
    (void)query_removed(tar);
    return sent && (!releases || send_carried(index, into, kept, kept_count, remove, said));
}

// two releases in the order of their names
static int send_order(const void *one, const void *other)
{
    return cli_deploy_order(*(const char *const *)one, *(const char *const *)other);
}

// the releases of the machine at `index`, oldest first, into s_releases, and the one `current` names, NULL for none,
// those before given back: 1, or 0 with why said
static int send_releases(unsigned int index, const char **current, CliLine *said)
{
    const CliDeployPlace *const place = &g_cli_deploy.place[index];
    send_releases_given();
    *current = NULL;
    char line[CLI_PATH_LONGEST + 160u];
    CliLine command = {line, sizeof(line), 0ull};
    cli_line_text(&command, "cd ");
    cli_link_quote(place->address.folder,
                   cellul.len(&(CatenaFinitaCfg){.src = place->address.folder, .cap = CLI_CELLS_POOL}), &command);
    cli_line_text(&command, " 2>/dev/null || exit 0; ls -1 releases 2>/dev/null; echo '<current>'; readlink current "
                            "2>/dev/null; true");
    cli_line_closed(&command);
    char piece[CLI_DEPLOY_PIECE];
    CliPieces pieces;
    CliLine listing;
    cli_pieces_begun(&pieces, &listing, piece, sizeof(piece));
    const int asked = cli_deploy_ask(place, line, NULL, 0ull, &listing, said);
    unsigned long long size = 0ull;
    s_releases_said = cli_pieces_joined(&pieces, &listing, "a machine's releases", &size);
    if (!asked)
    {
        send_releases_given();
        return 0;
    }
    if (s_releases_said == NULL)
    {
        cli_line_text(said, "it holds more files than orior_cli holds");
        return 0;
    }
    static const char s_mark[] = "<current>";
    char *const mark = (char *)cellul.find(
        &(CatenaFinitaCfg){.src = s_releases_said, .cap = size + 1u, .other = s_mark, .other_cap = sizeof(s_mark)});
    char *after = (mark != NULL) ? (mark + 9) : NULL;
    if (mark != NULL)
    {
        *mark = '\0';
    }
    unsigned int lines = 1u;
    for (const char *at = s_releases_said; *at != '\0'; at += 1)
    {
        lines += (*at == '\n') ? 1u : 0u;
    }
    s_releases = cli_cell_taken((unsigned long long)lines * sizeof(s_releases[0]), "a machine's releases");
    if (s_releases == NULL)
    {
        send_releases_given();
        cli_line_text(said, "it holds more files than orior_cli holds");
        return 0;
    }
    for (char *at = s_releases_said; (*at != '\0') && (s_release_count < lines);)
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

void cli_deploy_prune(unsigned int index, unsigned long long keep)
{
    const CliDeployPlace *const place = &g_cli_deploy.place[index];
    const char *current = NULL;
    char why[CLI_SAID_LONGEST];
    CliLine none = {why, sizeof(why), 0ull};
    if (!send_releases(index, &current, &none))
    {
        return;
    }
    const unsigned long long newest = (keep > 1u) ? keep : 1u;
    char piece[CLI_DEPLOY_PIECE];
    CliPieces pieces;
    CliLine line;
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
    cli_line_text(&line, "cd ");
    send_quoted(&line, place->address.folder, "/releases");
    cli_line_text(&line, " && rm -rf --");
    unsigned int old = 0u;
    for (unsigned int at = s_release_count; at > 0u; at -= 1u)
    {
        const char *const name = s_releases[at - 1u];
        if (((unsigned long long)(s_release_count - at) >= newest) &&
            ((current == NULL) || !cellul.eq(&(CatenaFinitaCfg){.src = current, .other = name, .cap = CLI_CELLS_POOL})))
        {
            cli_line_character(&line, ' ');
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
    if (!send_line_asked(place, &pieces, &line, "", &none))
    {
        return;
    }
    char said[(2u * CLI_PATH_LONGEST) + 64u];
    CliLine said_line = {said, sizeof(said), 0ull};
    cli_deploy_place_name(place, &said_line);
    cli_line_text(&said_line, " let go of ");
    cli_line_decimal(&said_line, old, 1u);
    cli_line_text(&said_line, " old releases");
    cli_deploy_say(&said_line);
}

int cli_deploy_roll_back(CliLine *said)
{
    CliDeployRun *const run = &g_cli_deploy;
    // the release each place turns back to, kept in the run's pool
    const char **const back = cli_deploy_placed(&run->pool, (unsigned long long)run->places * sizeof(back[0]));
    if (back == NULL)
    {
        cli_line_text(said, "no place was rolled back");
        return 0;
    }
    for (unsigned int index = 0u; index < run->places; index += 1u)
    {
        const CliDeployPlace *const place = &run->place[index];
        if (run->deployment.in_place || place->here)
        {
            cli_deploy_place_name(place, said);
            cli_line_text(said, " takes the files in place and keeps no release before them to go back to: no place "
                                "was rolled back");
            return 0;
        }
        const char *current = NULL;
        char why[CLI_SAID_LONGEST];
        CliLine why_line = {why, sizeof(why), 0ull};
        if (!send_releases(index, &current, &why_line))
        {
            cli_deploy_place_name(place, said);
            cli_line_text(said, ": ");
            cli_line_counted(said, why, why_line.at);
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
            cli_deploy_place_name(place, said);
            cli_line_text(said, " holds no release before the one it runs: no place was rolled back");
            return 0;
        }
        back[index] =
            cli_deploy_kept(&run->pool, s_releases[at - 1u],
                            cellul.len(&(CatenaFinitaCfg){.src = s_releases[at - 1u], .cap = CLI_CELLS_POOL}));
        send_releases_given();
    }
    for (unsigned int index = 0u; index < run->places; index += 1u)
    {
        const CliDeployPlace *const place = &run->place[index];
        char line[(2u * CLI_PATH_LONGEST) + 256u];
        CliLine command = {line, sizeof(line), 0ull};
        cli_deploy_switch_line(place->address.folder, back[index], &command);
        cli_line_closed(&command);
        char why[CLI_SAID_LONGEST];
        CliLine why_line = {why, sizeof(why), 0ull};
        CliLine none = {NULL, 0ull, 0ull};
        if (!cli_deploy_ask(place, line, NULL, 0ull, &none, &why_line))
        {
            cli_deploy_place_name(place, said);
            cli_line_text(said, ": ");
            cli_line_counted(said, why, why_line.at);
            return 0;
        }
        char told[(2u * CLI_PATH_LONGEST) + 64u];
        CliLine told_line = {told, sizeof(told), 0ull};
        cli_deploy_place_name(place, &told_line);
        cli_line_text(&told_line, " now runs release ");
        cli_line_text(&told_line, back[index]);
        cli_deploy_say(&told_line);
    }
    return 1;
}

int cli_deploy_removed(unsigned int index, CliLine *said)
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
            char path[(2u * CLI_PATH_LONGEST) + 2u];
            CliLine path_line = {path, sizeof(path), 0ull};
            cli_line_text(&path_line, place->path);
            cli_line_character(&path_line, '/');
            cli_line_text(&path_line, gone[at].path);
            if ((cli_line_closed(&path_line) == 0ull) || !query_removed(path))
            {
                cli_line_text(said, gone[at].path);
                cli_line_text(said, ": it could not be removed");
                return 0;
            }
        }
    }
    else
    {
        char piece[CLI_DEPLOY_PIECE];
        CliPieces pieces;
        CliLine line;
        cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
        cli_line_text(&line, "cd ");
        cli_link_quote(place->address.folder,
                       cellul.len(&(CatenaFinitaCfg){.src = place->address.folder, .cap = CLI_CELLS_POOL}), &line);
        cli_line_text(&line, " && rm -f --");
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            cli_line_character(&line, ' ');
            cli_link_quote(gone[at].path, cellul.len(&(CatenaFinitaCfg){.src = gone[at].path, .cap = CLI_CELLS_POOL}),
                           &line);
        }
        if (!send_line_asked(place, &pieces, &line, "the line that deletes the files gone is longer than orior_cli runs",
                             said))
        {
            return 0;
        }
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char told[(3u * CLI_PATH_LONGEST) + 32u];
        CliLine told_line = {told, sizeof(told), 0ull};
        cli_line_text(&told_line, "deleted ");
        cli_line_text(&told_line, gone[at].path);
        cli_line_text(&told_line, " at ");
        cli_deploy_place_name(place, &told_line);
        cli_deploy_say(&told_line);
    }
    return 1;
}
