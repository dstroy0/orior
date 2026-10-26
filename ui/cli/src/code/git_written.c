// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_written.c: what git reports of the tree written as the page reads it: each list a JSON array of objects, each
// field by its name
#include "../text/json_tree.h"
#include "git.h"

// a text of a list written as a JSON string after `name`, a `,` before it where `first` is 0
static void written_text(const CliGitText *text, unsigned int at, const char *name, int first, VerbaLinea *out)
{
    mmgr_line_put(out, first ? "\"" : ",\"");
    mmgr_line_put(out, name);
    mmgr_line_put(out, "\":");
    const char *const value = cli_git_text_of(text, at);
    cli_json_quoted_lossy(out, (const unsigned char *)value,
                          cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}));
}

// a text of a list, or `null` where there is none, written after `name`
static void written_optional(const CliGitText *text, unsigned int at, int has, const char *name, VerbaLinea *out)
{
    if (has)
    {
        written_text(text, at, name, 0, out);
        return;
    }
    mmgr_line_put(out, ",\"");
    mmgr_line_put(out, name);
    mmgr_line_put(out, "\":null");
}

// a time in seconds since 1970 written after `name`
static void written_when(long long when, const char *name, VerbaLinea *out)
{
    mmgr_line_put(out, ",\"");
    mmgr_line_put(out, name);
    mmgr_line_put(out, "\":");
    mmgr_line_decimal_signed(out, when);
}

// a state letter written as a JSON string, a `,` before it
static void written_state(char state, VerbaLinea *out)
{
    mmgr_line_put(out, ",\"state\":\"");
    mmgr_line_ch(out, state);
    mmgr_line_ch(out, '"');
}

// a whole number written after `name`, a `,` before it
static void written_number(unsigned long long value, const char *name, VerbaLinea *out)
{
    mmgr_line_put(out, ",\"");
    mmgr_line_put(out, name);
    mmgr_line_put(out, "\":");
    mmgr_line_decimal(out, value, 1u);
}

// the opening of an item of a list, a `,` before it but the first
static void written_item(unsigned int at, VerbaLinea *out)
{
    mmgr_line_put(out, (at > 0u) ? ",{" : "{");
}

void cli_git_repositories_written(const CliRepositories *found, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliRepository *const one = &found->item[at];
        written_item(at, out);
        written_text(&found->text, one->path, "path", 1, out);
        written_text(&found->text, one->name, "name", 0, out);
        written_optional(&found->text, one->branch, one->has_branch, "branch", out);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
}

void cli_git_changed_written(const CliChangedList *found, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        written_item(at, out);
        written_text(&found->text, found->item[at].path, "path", 1, out);
        written_state(found->item[at].state, out);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
}

void cli_git_commits_written(const CliCommits *found, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliCommit *const one = &found->item[at];
        written_item(at, out);
        written_text(&found->text, one->id, "id", 1, out);
        written_when(one->when, "when", out);
        written_text(&found->text, one->subject, "subject", 0, out);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
}

void cli_git_branches_written(const CliBranches *found, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliBranch *const one = &found->item[at];
        written_item(at, out);
        written_text(&found->text, one->name, "name", 1, out);
        mmgr_line_put(out, one->remote ? ",\"remote\":true" : ",\"remote\":false");
        mmgr_line_put(out, one->current ? ",\"current\":true" : ",\"current\":false");
        written_text(&found->text, one->upstream, "upstream", 0, out);
        written_when(one->when, "when", out);
        written_text(&found->text, one->subject, "subject", 0, out);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
}

void cli_git_stashes_written(const CliStashes *found, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliStash *const one = &found->item[at];
        written_item(at, out);
        written_text(&found->text, one->name, "name", 1, out);
        written_when(one->when, "when", out);
        written_text(&found->text, one->subject, "subject", 0, out);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
}

void cli_git_graph_written(const CliGraph *found, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliDrawn *const one = &found->item[at];
        written_item(at, out);
        written_text(&found->text, one->id, "id", 1, out);
        mmgr_line_put(out, ",\"parents\":[");
        for (unsigned int parent = 0u; parent < one->parents; parent += 1u)
        {
            const char *const id = cli_git_text_of(&found->text, one->parent[parent]);
            mmgr_line_put(out, (parent > 0u) ? "," : "");
            cli_json_quoted_lossy(out, (const unsigned char *)id,
                                  cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
        }
        mmgr_line_ch(out, ']');
        written_text(&found->text, one->author, "author", 0, out);
        written_when(one->when, "when", out);
        mmgr_line_put(out, ",\"refs\":[");
        for (unsigned int ref = 0u; ref < one->refs; ref += 1u)
        {
            const char *const name = cli_git_text_of(&found->text, one->ref[ref]);
            mmgr_line_put(out, (ref > 0u) ? "," : "");
            cli_json_quoted_lossy(out, (const unsigned char *)name,
                                  cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        }
        mmgr_line_ch(out, ']');
        written_text(&found->text, one->subject, "subject", 0, out);
        written_number(one->lane, "lane", out);
        written_number(one->color, "color", out);
        mmgr_line_put(out, ",\"lines\":[");
        for (unsigned int stroke = 0u; stroke < one->stroke_count; stroke += 1u)
        {
            const unsigned int *const line = found->stroke[one->first_stroke + stroke];
            mmgr_line_put(out, (stroke > 0u) ? ",[" : "[");
            for (unsigned int part = 0u; part < 4u; part += 1u)
            {
                mmgr_line_put(out, (part > 0u) ? "," : "");
                mmgr_line_decimal(out, line[part], 1u);
            }
            mmgr_line_ch(out, ']');
        }
        mmgr_line_put(out, "]}");
    }
    mmgr_line_ch(out, ']');
}

// a touched file's path written after `name`, after `repo` and a `/` where `repo` is not empty
static void written_in_repo(const CliGitText *text, unsigned int at, const char *repo, const char *name, int first,
                            VerbaLinea *out)
{
    mmgr_line_put(out, first ? "\"" : ",\"");
    mmgr_line_put(out, name);
    mmgr_line_put(out, "\":\"");
    if (repo[0] != '\0')
    {
        cli_json_escaped_lossy(out, (const unsigned char *)repo,
                               cellul.len(&(CatenaFinitaCfg){.src = repo, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(out, '/');
    }
    const char *const value = cli_git_text_of(text, at);
    cli_json_escaped_lossy(out, (const unsigned char *)value,
                           cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(out, '"');
}

void cli_git_touched_written(const CliTouchedList *found, const char *repo, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliTouched *const one = &found->item[at];
        written_item(at, out);
        written_in_repo(&found->text, one->path, repo, "path", 1, out);
        written_state(one->state, out);
        if (one->has_was)
        {
            written_in_repo(&found->text, one->was, repo, "was", 0, out);
        }
        else
        {
            mmgr_line_put(out, ",\"was\":null");
        }
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
}

void cli_git_line_history_written(const CliLineHistory *found, VerbaLinea *out)
{
    mmgr_line_put(out, "{\"commits\":[");
    for (unsigned int at = 0u; at < found->commits; at += 1u)
    {
        const CliLineCommit *const one = &found->commit[at];
        written_item(at, out);
        written_text(&found->text, one->id, "id", 1, out);
        written_text(&found->text, one->author, "author", 0, out);
        written_when(one->when, "when", out);
        written_text(&found->text, one->subject, "subject", 0, out);
        written_optional(&found->text, one->path, one->has_path, "path", out);
        written_optional(&found->text, one->parent, one->has_parent, "parent", out);
        written_optional(&found->text, one->was, one->has_was, "was", out);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_put(out, "],\"lines\":[");
    for (unsigned int at = 0u; at < found->lines; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        mmgr_line_decimal_signed(out, found->line[at]);
    }
    mmgr_line_put(out, "]}");
}
