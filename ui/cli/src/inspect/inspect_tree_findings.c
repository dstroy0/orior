// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_findings.c: every finding in every file, by its path: what each file shows alone, then what the tree
// shows, its imports that lead back round and the attributes no class of a family sets, each written as the
// protocol's diagnostic. The tree's findings are counted by one walk of what makes them and stored by a second, in one
// cell sized to their count and their messages; a diagnostic is measured before it is written, in a cell of its size
#include "../command_line/cells.h"
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
static void findings_collected(void *context, unsigned int file, CliInspectPlace from, CliInspectPlace to,
                               const char *code, const VerbaLinea *message)
{
    (void)context;
    if (s_findings.counting)
    {
        s_findings.count += 1u;
        s_findings.used += message->at + 1ull;
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
    memor.cpy(
        &(MemoriaCfg){.dst = s_findings.msg_text + s_findings.used, .src = message->out, .bytes = (size_t)message->at});
    s_findings.used += message->at;
    s_findings.msg_text[s_findings.used] = '\0';
    s_findings.used += 1ull;
    s_findings.finding_file[s_findings.count] = file;
    s_findings.count += 1u;
    s_findings.file_count[file] += 1u;
}

// the tree's findings put in the order of their files, each file's in the order they were made
static void findings_ordered(void)
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
static void *findings_taken(void)
{
    s_findings = (CliTreeFindings){1, 0u, 0ull, NULL, NULL, NULL, NULL, NULL, NULL};
    cli_tree_cycles(findings_collected, NULL);
    cli_tree_attributes(findings_collected, NULL);
    const unsigned long long count = s_findings.count;
    const unsigned long long files = g_cli_tree_files;
    const unsigned long long finding_bytes = count * sizeof(CliInspectFinding);
    const unsigned long long number_bytes = ((2ull * count) + (2ull * files)) * sizeof(unsigned int);
    unsigned char *const cell =
        g_cli_cells->persistent_buf_alloc(finding_bytes + number_bytes + s_findings.used + 16ull, &g_cli_inspect_cells);
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
    s_findings.used = 0ull;
    cli_tree_cycles(findings_collected, NULL);
    cli_tree_attributes(findings_collected, NULL);
    findings_ordered();
    return cell;
}

// finding `finding` of `facts` measured, written into a cell of its size and handed to `each` for `path`
static void findings_handed(const CliFacts *facts, unsigned int finding, const char *path, CliInspectFound each,
                            void *context)
{
    char piece[256];
    VerbaLinea measure;
    cli_measure_begun(&measure, piece, sizeof(piece));
    cli_inspect_finding_written(facts, finding, &measure);
    const unsigned long long size = cli_measured(&measure) + 1ull;
    char *const written = cli_lane_alloc(size, &g_cli_inspect_cells);
    if (written == NULL)
    {
        return;
    }
    VerbaLinea line = {written, size, 0ull};
    cli_inspect_finding_written(facts, finding, &line);
    if (mmgr_line_ok(&line))
    {
        each(context, path, written, line.at);
    }
    cli_cell_release(written, &g_cli_inspect_cells);
}

void cli_inspect_tree_findings(CliInspectFound each, void *context)
{
    cli_lock_take(&g_cli_tree_lock);
    void *const cell = findings_taken();
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
        each(context, tree_file->path, NULL, 0ull);
        for (unsigned int finding = 0u; finding < tree_file->facts.findings; finding += 1u)
        {
            findings_handed(&tree_file->facts, finding, tree_file->path, each, context);
        }
        for (unsigned int at = 0u; (cell != NULL) && (at < s_findings.file_count[file]); at += 1u)
        {
            findings_handed(&tree, s_findings.order[s_findings.file_first[file] + at], tree_file->path, each, context);
        }
    }
    cli_cell_release(cell, &g_cli_inspect_cells);
    s_findings = (CliTreeFindings){0, 0u, 0ull, NULL, NULL, NULL, NULL, NULL, NULL};
    cli_lock_give(&g_cli_tree_lock);
}
