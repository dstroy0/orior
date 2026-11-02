// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_written.c: what git reports of the tree written as the page reads it: each list a JSON array of objects, each
// field by its name
#include "../text/json_tree.h"
#include "git.h"

typedef struct
{
    const CliGitText *text;
    unsigned int at;
    const char *name;
    int first;
    VerbaLinea *out;
    int has;
    long long when;
    char state;
    unsigned long long value;
    const char *repo;
} GitWrittenCtx;

// a text of a list written as a JSON string after `name`, a `,` before it where `first` is 0
EMBED_INLINE void write_text(const GitWrittenCtx *args)
{
    const CliGitText *text = args->text;
    unsigned int at = args->at;
    VerbaLinea *out = args->out;
    mmgr_line_put(out, args->first ? "\"" : ",\"");
    mmgr_line_put(out, args->name);
    mmgr_line_put(out, "\":");
    const char *const value = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = text, .at = at);
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)value,
               .length = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}));
}

// a text of a list, or `null` where there is none, written after `name`
EMBED_INLINE void write_optional(const GitWrittenCtx *args)
{
    if (args->has)
    {
        EMBED_CALL(write_text, GitWrittenCtx, .text = args->text, .at = args->at, .name = args->name, .first = 0,
                   .out = args->out);
        return;
    }
    mmgr_line_put(args->out, ",\"");
    mmgr_line_put(args->out, args->name);
    mmgr_line_put(args->out, "\":null");
}

// a time in seconds since 1970 written after `name`
EMBED_INLINE void write_when(const GitWrittenCtx *args)
{
    mmgr_line_put(args->out, ",\"");
    mmgr_line_put(args->out, args->name);
    mmgr_line_put(args->out, "\":");
    mmgr_line_decimal_signed(args->out, args->when);
}

// a state letter written as a JSON string, a `,` before it
EMBED_INLINE void write_state(const GitWrittenCtx *args)
{
    mmgr_line_put(args->out, ",\"state\":\"");
    mmgr_line_ch(args->out, args->state);
    mmgr_line_ch(args->out, '"');
}

// a whole number written after `name`, a `,` before it
EMBED_INLINE void write_number(const GitWrittenCtx *args)
{
    mmgr_line_put(args->out, ",\"");
    mmgr_line_put(args->out, args->name);
    mmgr_line_put(args->out, "\":");
    mmgr_line_decimal(args->out, args->value, 1u);
}

// the opening of an item of a list, a `,` before it but the first
EMBED_INLINE void write_item(const GitWrittenCtx *args)
{
    mmgr_line_put(args->out, (args->at > 0u) ? ",{" : "{");
}

void cli_git_repositories_write(const CliGitOutFoundRepositoriesCfg *args)
{
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < args->found_repositories_pointer_other->count; at += 1u)
    {
        const CliRepository *const one = &args->found_repositories_pointer_other->item[at];
        EMBED_CALL(write_item, GitWrittenCtx, .at = at, .out = args->out);
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_repositories_pointer_other->text, .at = one->path,
                   .name = "path", .first = 1, .out = args->out);
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_repositories_pointer_other->text, .at = one->name,
                   .name = "name", .first = 0, .out = args->out);
        EMBED_CALL(write_optional, GitWrittenCtx, .text = &args->found_repositories_pointer_other->text,
                   .at = one->branch, .has = one->has_branch, .name = "branch", .out = args->out);
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, ']');
}

void cli_git_changed_write(const CliGitOutFoundChangedListCfg *args)
{
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < args->found_changed_list_pointer_other->count; at += 1u)
    {
        EMBED_CALL(write_item, GitWrittenCtx, .at = at, .out = args->out);
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_changed_list_pointer_other->text,
                   .at = args->found_changed_list_pointer_other->item[at].path, .name = "path", .first = 1,
                   .out = args->out);
        EMBED_CALL(write_state, GitWrittenCtx, .state = args->found_changed_list_pointer_other->item[at].state,
                   .out = args->out);
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, ']');
}

void cli_git_commits_write(const CliGitOutFoundCfg *args)
{
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < args->found_read->count; at += 1u)
    {
        const CliCommit *const one = &args->found_read->item[at];
        EMBED_CALL(write_item, GitWrittenCtx, .at = at, .out = args->out);
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_read->text, .at = one->id, .name = "id", .first = 1,
                   .out = args->out);
        EMBED_CALL(write_when, GitWrittenCtx, .when = one->when, .name = "when", .out = args->out);
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_read->text, .at = one->subject, .name = "subject",
                   .first = 0, .out = args->out);
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, ']');
}

void cli_git_branches_write(const CliGitOutFoundBranchesCfg *args)
{
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < args->found_branches_pointer_other->count; at += 1u)
    {
        const CliBranch *const one = &args->found_branches_pointer_other->item[at];
        EMBED_CALL(write_item, GitWrittenCtx, .at = at, .out = args->out);
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_branches_pointer_other->text, .at = one->name,
                   .name = "name", .first = 1, .out = args->out);
        mmgr_line_put(args->out, one->remote ? ",\"remote\":true" : ",\"remote\":false");
        mmgr_line_put(args->out, one->current ? ",\"current\":true" : ",\"current\":false");
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_branches_pointer_other->text, .at = one->upstream,
                   .name = "upstream", .first = 0, .out = args->out);
        EMBED_CALL(write_when, GitWrittenCtx, .when = one->when, .name = "when", .out = args->out);
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_branches_pointer_other->text, .at = one->subject,
                   .name = "subject", .first = 0, .out = args->out);
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, ']');
}

void cli_git_stashes_write(const CliGitOutFoundStashesCfg *args)
{
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < args->found_stashes_pointer_other->count; at += 1u)
    {
        const CliStash *const one = &args->found_stashes_pointer_other->item[at];
        EMBED_CALL(write_item, GitWrittenCtx, .at = at, .out = args->out);
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_stashes_pointer_other->text, .at = one->name,
                   .name = "name", .first = 1, .out = args->out);
        EMBED_CALL(write_when, GitWrittenCtx, .when = one->when, .name = "when", .out = args->out);
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_stashes_pointer_other->text, .at = one->subject,
                   .name = "subject", .first = 0, .out = args->out);
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, ']');
}

void cli_git_graph_write(const CliGitOutFoundGraphCfg *args)
{
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < args->found_graph_pointer_other->count; at += 1u)
    {
        const CliDrawn *const one = &args->found_graph_pointer_other->item[at];
        EMBED_CALL(write_item, GitWrittenCtx, .at = at, .out = args->out);
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_graph_pointer_other->text, .at = one->id,
                   .name = "id", .first = 1, .out = args->out);
        mmgr_line_put(args->out, ",\"parents\":[");
        for (unsigned int parent = 0u; parent < one->parents; parent += 1u)
        {
            const char *const id =
                EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                           .text_read = &args->found_graph_pointer_other->text, .at = one->parent[parent]);
            mmgr_line_put(args->out, (parent > 0u) ? "," : "");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                       .text_bytes = (const unsigned char *)id,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
        }
        mmgr_line_ch(args->out, ']');
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_graph_pointer_other->text, .at = one->author,
                   .name = "author", .first = 0, .out = args->out);
        EMBED_CALL(write_when, GitWrittenCtx, .when = one->when, .name = "when", .out = args->out);
        mmgr_line_put(args->out, ",\"refs\":[");
        for (unsigned int ref = 0u; ref < one->refs; ref += 1u)
        {
            const char *const name =
                EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                           .text_read = &args->found_graph_pointer_other->text, .at = one->ref[ref]);
            mmgr_line_put(args->out, (ref > 0u) ? "," : "");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                       .text_bytes = (const unsigned char *)name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        }
        mmgr_line_ch(args->out, ']');
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_graph_pointer_other->text, .at = one->subject,
                   .name = "subject", .first = 0, .out = args->out);
        EMBED_CALL(write_number, GitWrittenCtx, .value = one->lane, .name = "lane", .out = args->out);
        EMBED_CALL(write_number, GitWrittenCtx, .value = one->color, .name = "color", .out = args->out);
        mmgr_line_put(args->out, ",\"lines\":[");
        for (unsigned int stroke = 0u; stroke < one->stroke_count; stroke += 1u)
        {
            const unsigned int *const line = args->found_graph_pointer_other->stroke[one->first_stroke + stroke];
            mmgr_line_put(args->out, (stroke > 0u) ? ",[" : "[");
            for (unsigned int part = 0u; part < 4u; part += 1u)
            {
                mmgr_line_put(args->out, (part > 0u) ? "," : "");
                mmgr_line_decimal(args->out, line[part], 1u);
            }
            mmgr_line_ch(args->out, ']');
        }
        mmgr_line_put(args->out, "]}");
    }
    mmgr_line_ch(args->out, ']');
}

// a touched file's path written after `name`, after `repo` and a `/` where `repo` is not empty
EMBED_INLINE void write_in_repo(const GitWrittenCtx *args)
{
    const CliGitText *text = args->text;
    unsigned int at = args->at;
    VerbaLinea *out = args->out;
    mmgr_line_put(out, args->first ? "\"" : ",\"");
    mmgr_line_put(out, args->name);
    mmgr_line_put(out, "\":\"");
    if (args->repo[0] != '\0')
    {
        EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)args->repo,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->repo, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(out, '/');
    }
    const char *const value = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = text, .at = at);
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)value,
               .length = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(out, '"');
}

void cli_git_touched_write(const CliGitOutFoundTouchedListCfg *args)
{
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < args->found_touched_list_pointer_other->count; at += 1u)
    {
        const CliTouched *const one = &args->found_touched_list_pointer_other->item[at];
        EMBED_CALL(write_item, GitWrittenCtx, .at = at, .out = args->out);
        EMBED_CALL(write_in_repo, GitWrittenCtx, .text = &args->found_touched_list_pointer_other->text, .at = one->path,
                   .repo = args->repo, .name = "path", .first = 1, .out = args->out);
        EMBED_CALL(write_state, GitWrittenCtx, .state = one->state, .out = args->out);
        if (one->has_was)
        {
            EMBED_CALL(write_in_repo, GitWrittenCtx, .text = &args->found_touched_list_pointer_other->text,
                       .at = one->was, .repo = args->repo, .name = "was", .first = 0, .out = args->out);
        }
        else
        {
            mmgr_line_put(args->out, ",\"was\":null");
        }
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, ']');
}

void cli_git_line_history_write(const CliGitOutFoundLineHistoryCfg *args)
{
    mmgr_line_put(args->out, "{\"commits\":[");
    for (unsigned int at = 0u; at < args->found_line_history_pointer_other->commits; at += 1u)
    {
        const CliLineCommit *const one = &args->found_line_history_pointer_other->commit[at];
        EMBED_CALL(write_item, GitWrittenCtx, .at = at, .out = args->out);
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_line_history_pointer_other->text, .at = one->id,
                   .name = "id", .first = 1, .out = args->out);
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_line_history_pointer_other->text, .at = one->author,
                   .name = "author", .first = 0, .out = args->out);
        EMBED_CALL(write_when, GitWrittenCtx, .when = one->when, .name = "when", .out = args->out);
        EMBED_CALL(write_text, GitWrittenCtx, .text = &args->found_line_history_pointer_other->text, .at = one->subject,
                   .name = "subject", .first = 0, .out = args->out);
        EMBED_CALL(write_optional, GitWrittenCtx, .text = &args->found_line_history_pointer_other->text,
                   .at = one->path, .has = one->has_path, .name = "path", .out = args->out);
        EMBED_CALL(write_optional, GitWrittenCtx, .text = &args->found_line_history_pointer_other->text,
                   .at = one->parent, .has = one->has_parent, .name = "parent", .out = args->out);
        EMBED_CALL(write_optional, GitWrittenCtx, .text = &args->found_line_history_pointer_other->text, .at = one->was,
                   .has = one->has_was, .name = "was", .out = args->out);
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_put(args->out, "],\"lines\":[");
    for (unsigned int at = 0u; at < args->found_line_history_pointer_other->lines; at += 1u)
    {
        mmgr_line_put(args->out, (at > 0u) ? "," : "");
        mmgr_line_decimal_signed(args->out, args->found_line_history_pointer_other->line[at]);
    }
    mmgr_line_put(args->out, "]}");
}
