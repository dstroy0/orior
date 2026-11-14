// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_findings.c: every finding in every file, by its path: what each file shows alone, then what the tree
// shows, its imports that lead back round and the attributes no class of a family sets, each written as the
// protocol's diagnostic. The tree's findings are counted by one walk of what makes them and stored by a second, in one
// movable cell sized to their count and their messages, laid as a facts cell of findings and texts, with the order
// they are handed on in by their files in a cell of the call's own; a diagnostic is measured before it is written, in a
// cell of its size
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/measured.h"
#include "inspect_tree_internal.h"

// The tree's findings: each one's file, span, name and message, and the order they are handed on in by their files,
// counted where `counting` is 1 and stored where it is 0; the findings and their messages in the movable cell `facts`
// names, the numbers in `numbers`
typedef struct
{
    int counting;
    unsigned int count;
    unsigned long long used;
    CliFactsCell facts;
    unsigned int *numbers;
    unsigned int *finding_file;
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
    CliInspectFinding finding;
    memor.set(&(MemoriaCfg){.dst = &finding, .val = 0, .bytes = sizeof(finding)});
    finding.from = from;
    finding.to = to;
    finding.severity = CLI_INSPECT_WARNING;
    finding.code = code;
    // the messages are what one walk counted, which an unsigned int's places reach
    finding.message.at = (unsigned int)s_findings.used;
    finding.message.size = (unsigned int)message->at;
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_findings.facts.cell,
                            .offset = CLI_FACTS_FINDING_AT(&s_findings.facts, s_findings.count),
                            .src = &finding,
                            .bytes = sizeof(finding)});
    const char zero = '\0';
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_findings.facts.cell,
                            .offset = CLI_FACTS_TEXT_AT(&s_findings.facts, s_findings.used),
                            .src = message->out,
                            .bytes = message->at});
    s_findings.used += message->at;
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_findings.facts.cell,
                            .offset = CLI_FACTS_TEXT_AT(&s_findings.facts, s_findings.used),
                            .src = &zero,
                            .bytes = 1ULL});
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

// the cells of the findings given back
static void findings_free(void)
{
    if (s_findings.facts.cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_findings.facts.cell, g_cli_inspect_owner);
    }
    g_cli_guard->persistent_buf_release(s_findings.numbers, g_cli_inspect_owner);
    memor.set(&(MemoriaCfg){.dst = &s_findings, .val = 0, .bytes = sizeof(s_findings)});
}

// the tree's findings counted, their cells taken at that count, and stored: 1, or 0 where a cell cannot be taken
static int findings_alloc(void)
{
    memor.set(&(MemoriaCfg){.dst = &s_findings, .val = 0, .bytes = sizeof(s_findings)});
    s_findings.counting = 1;
    EMBED_CALL(cli_inspect_tree_internal.tree_cycles, CliInspectTreeCtx, .each = findings_collect, .context = NULL);
    EMBED_CALL(cli_inspect_tree_internal.tree_attributes, CliInspectTreeCtx, .each = findings_collect, .context = NULL);
    const unsigned long long count = s_findings.count;
    const unsigned long long files = g_cli_tree_files;
    const unsigned long long finding_bytes = count * sizeof(CliInspectFinding);
    const unsigned long long number_bytes = ((2ULL * count) + (2ULL * files)) * sizeof(unsigned int);
    s_findings.facts.cell =
        g_cli_guard->movable_buf_alloc(finding_bytes + s_findings.used + 16ULL, g_cli_inspect_owner);
    s_findings.numbers = g_cli_guard->temporary_buf_alloc(number_bytes + 16ULL, g_cli_inspect_owner);
    if ((s_findings.facts.cell == NULL) || (s_findings.numbers == NULL))
    {
        findings_free();
        return 0;
    }
    s_findings.facts.finding_at = 0ULL;
    s_findings.facts.text_at = finding_bytes;
    s_findings.facts.findings = (unsigned int)count;
    s_findings.facts.used = (unsigned int)s_findings.used;
    s_findings.finding_file = s_findings.numbers;
    s_findings.order = s_findings.finding_file + count;
    s_findings.file_first = s_findings.order + count;
    s_findings.file_count = s_findings.file_first + files;
    memor.set(
        &(MemoriaCfg){.dst = s_findings.file_count, .val = 0, .bytes = (embed_word)(files * sizeof(unsigned int))});
    s_findings.counting = 0;
    s_findings.count = 0u;
    s_findings.used = 0ULL;
    EMBED_CALL(cli_inspect_tree_internal.tree_cycles, CliInspectTreeCtx, .each = findings_collect, .context = NULL);
    EMBED_CALL(cli_inspect_tree_internal.tree_attributes, CliInspectTreeCtx, .each = findings_collect, .context = NULL);
    findings_order();
    return 1;
}

typedef struct
{
    const CliFactsCell *facts;
    unsigned int finding;
    const char *path;
    CliInspectFound each;
    void *context;
} InspectTreeFindingsCtx;
// finding `finding` of the facts cell `facts` measured, written into a cell of its size and handed to `each` for
// `path`
EMBED_INLINE void findings_dispatch(const InspectTreeFindingsCtx *args)
{
    const CliFactsCell *facts = args->facts;
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
    const int stored = findings_alloc();
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        CliTreeFile tree_file;
        CLI_TREE_FILE_READ(file, &tree_file);
        if (!tree_file.used)
        {
            continue;
        }
        char path[CLI_TREE_PATH_BYTES];
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = tree_file.facts.cell,
                                                                .offset = CLI_TREE_PATH_AT,
                                                                .dst = path,
                                                                .bytes = tree_file.path_size + 1ULL});
        EMBED_CALL(args->each_inspect_found, CliInspectFoundCfg, .context = args->context, .path = path,
                   .finding = NULL, .size = 0ULL);
        for (unsigned int finding = 0u; finding < tree_file.facts.findings; finding += 1u)
        {
            EMBED_CALL(findings_dispatch, InspectTreeFindingsCtx, .facts = &tree_file.facts, .finding = finding,
                       .path = path, .each = args->each_inspect_found, .context = args->context);
        }
        for (unsigned int at = 0u; stored && (at < s_findings.file_count[file]); at += 1u)
        {
            EMBED_CALL(findings_dispatch, InspectTreeFindingsCtx, .facts = &s_findings.facts,
                       .finding = s_findings.order[s_findings.file_first[file] + at], .path = path,
                       .each = args->each_inspect_found, .context = args->context);
        }
    }
    findings_free();
}
