// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_findings.c: every finding in every file, by its path: what each file shows alone, then what the tree
// shows, its imports that lead back round and the attributes no class of a family sets, each written as the
// protocol's diagnostic. The tree's findings are counted by one walk of what makes them and stored by a second, in one
// cell sized to their count and their messages; a diagnostic is measured before it is written, in a cell of its size
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/measured.h"
#include "inspect_tree_internal.h"

// The tree's findings: each one's file, span, name and message, and the order they are handed on in by their files,
// counted where `counting` is 1 and stored where it is 0
typedef struct
{
    int counting;
    unsigned int count;
    unsigned long long used;
    unsigned int *finding_file;
    CliInspectFinding *findings;
    char *msg_text;
    unsigned int *file_first;
    unsigned int *file_count;
    unsigned int *order;
} CliTreeFindings;

static CliTreeFindings s_findings;

// a finding of the tree's counted, or stored
static void findings_collect(const CliTreeFindingCfg *args)
{
    CliInspectPosition from = args->from;
    CliInspectPosition to = args->to;
    const char *code = args->code;
    const VerbaLinea *message = args->message;
    if (s_findings.counting)
    {
        s_findings.count += 1u;
        s_findings.used += message->at + 1ULL;
        return;
    }
    CliInspectFinding *const finding = &s_findings.findings[s_findings.count];
    memor.set(&(MemoriaCfg){.dst = finding, .val = 0, .bytes = sizeof(*finding)});
    finding->from = from;
    finding->to = to;
    finding->severity = CLI_INSPECT_WARNING;
    finding->code = code;
    // the messages are what one walk counted, which an unsigned int's places reach
    finding->message.at = (unsigned int)s_findings.used;
    finding->message.size = (unsigned int)message->at;
    memor.cpy(&(MemoriaCfg){.dst = s_findings.msg_text + s_findings.used, .src = message->out, .bytes = message->at});
    s_findings.used += message->at;
    s_findings.msg_text[s_findings.used] = '\0';
    s_findings.used += 1ULL;
    s_findings.finding_file[s_findings.count] = args->file;
    s_findings.count += 1u;
    s_findings.file_count[args->file] += 1u;
}

// the tree's findings put in the order of their files, each file's in the order they were made
static void findings_order(void)
{
    unsigned int first = 0u;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        s_findings.file_first[file] = first;
        first += s_findings.file_count[file];
        s_findings.file_count[file] = 0u;
    }
    for (unsigned int finding = 0u; finding < s_findings.count; finding += 1u)
    {
        const unsigned int file = s_findings.finding_file[finding];
        s_findings.order[s_findings.file_first[file] + s_findings.file_count[file]] = finding;
        s_findings.file_count[file] += 1u;
    }
}

// the tree's findings counted, their cell taken at that count, and stored: the cell, NULL where it cannot be taken
static void *findings_alloc(void)
{
    s_findings = (CliTreeFindings){1, 0u, 0ULL, NULL, NULL, NULL, NULL, NULL, NULL};
    EMBED_CALL(cli_inspect_tree_internal.tree_cycles, CliInspectTreeCtx, .each = findings_collect, .context = NULL);
    EMBED_CALL(cli_inspect_tree_internal.tree_attributes, CliInspectTreeCtx, .each = findings_collect, .context = NULL);
    const unsigned long long count = s_findings.count;
    const unsigned long long files = g_cli_tree_files;
    const unsigned long long finding_bytes = count * sizeof(CliInspectFinding);
    const unsigned long long number_bytes = ((2ULL * count) + (2ULL * files)) * sizeof(unsigned int);
    unsigned char *const cell =
        g_cli_guard->persistent_buf_alloc(finding_bytes + number_bytes + s_findings.used + 16ULL, g_cli_inspect_owner);
    if (cell == NULL)
    {
        return NULL;
    }
    s_findings.findings = (CliInspectFinding *)(void *)cell;
    s_findings.finding_file = (unsigned int *)(void *)(cell + finding_bytes);
    s_findings.order = s_findings.finding_file + count;
    s_findings.file_first = s_findings.order + count;
    s_findings.file_count = s_findings.file_first + files;
    s_findings.msg_text = (char *)(s_findings.file_count + files);
    memor.set(&(MemoriaCfg){.dst = s_findings.file_count, .val = 0, .bytes = (size_t)(files * sizeof(unsigned int))});
    s_findings.counting = 0;
    s_findings.count = 0u;
    s_findings.used = 0ULL;
    EMBED_CALL(cli_inspect_tree_internal.tree_cycles, CliInspectTreeCtx, .each = findings_collect, .context = NULL);
    EMBED_CALL(cli_inspect_tree_internal.tree_attributes, CliInspectTreeCtx, .each = findings_collect, .context = NULL);
    findings_order();
    return cell;
}

typedef struct
{
    const CliFacts *facts;
    unsigned int finding;
    const char *path;
    CliInspectFound each;
    void *context;
} InspectTreeFindingsCtx;
// finding `finding` of `facts` measured, written into a cell of its size and handed to `each` for `path`
EMBED_INLINE void findings_dispatch(const InspectTreeFindingsCtx *args)
{
    const CliFacts *facts = args->facts;
    unsigned int finding = args->finding;
    char piece[256];
    VerbaLinea measure;
    EMBED_CALL(cli_measured_piece.measure_begin, CliMeasuredPieceCfg, .line = &measure, .piece = piece,
               .piece_size = sizeof(piece));
    EMBED_CALL(cli_inspect_facts_finding.finding_write, CliInspectFactsFindingCfg, .facts_read = facts,
               .finding = finding, .out = &measure);
    const unsigned long long size =
        EMBED_CALL(cli_measured_table.measure, CliMeasuredCfg, .line_read = &measure) + 1ULL;
    char *const written = g_cli_guard->temporary_buf_alloc(size, g_cli_inspect_owner);
    if (written == NULL)
    {
        return;
    }
    VerbaLinea line = {written, size, 0ULL};
    EMBED_CALL(cli_inspect_facts_finding.finding_write, CliInspectFactsFindingCfg, .facts_read = facts,
               .finding = finding, .out = &line);
    if (mmgr_line_ok(&line))
    {
        EMBED_CALL(args->each, CliInspectFoundCfg, .context = args->context, .path = args->path, .finding = written,
                   .size = line.at);
    }
    g_cli_guard->persistent_buf_release(written, g_cli_inspect_owner);
}

void cli_inspect_tree_findings(const CliInspectTreeCfg *args)
{
    void *const cell = findings_alloc();
    CliFacts tree = {0};
    tree.finding = s_findings.findings;
    tree.findings = (cell != NULL) ? s_findings.count : 0u;
    tree.text = s_findings.msg_text;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        const CliTreeFile *const tree_file = &CLI_TREE_FILE(file);
        if (!tree_file->used)
        {
            continue;
        }
        EMBED_CALL(args->each_inspect_found, CliInspectFoundCfg, .context = args->context, .path = tree_file->path,
                   .finding = NULL, .size = 0ULL);
        for (unsigned int finding = 0u; finding < tree_file->facts.findings; finding += 1u)
        {
            EMBED_CALL(findings_dispatch, InspectTreeFindingsCtx, .facts = &tree_file->facts, .finding = finding,
                       .path = tree_file->path, .each = args->each_inspect_found, .context = args->context);
        }
        for (unsigned int at = 0u; (cell != NULL) && (at < s_findings.file_count[file]); at += 1u)
        {
            EMBED_CALL(findings_dispatch, InspectTreeFindingsCtx, .facts = &tree,
                       .finding = s_findings.order[s_findings.file_first[file] + at], .path = tree_file->path,
                       .each = args->each_inspect_found, .context = args->context);
        }
    }
    g_cli_guard->persistent_buf_release(cell, g_cli_inspect_owner);
    s_findings = (CliTreeFindings){0, 0u, 0ULL, NULL, NULL, NULL, NULL, NULL, NULL};
}
