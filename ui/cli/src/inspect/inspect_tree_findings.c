// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_findings.c: every finding in every file, by its path: what each file says alone, then what the tree
// says, its imports that lead back round and the attributes no class of a family sets, each written as the
// protocol's diagnostic. The tree's findings are counted by one walk of what makes them and kept by a second, in one
// cell sized to their count and their messages; a diagnostic is measured before it is written, in a cell of its size
#include "../command_line/cells.h"
#include "../text/measured.h"
#include "inspect_tree_internal.h"

// The tree's findings: each one's file, span, name and message, and the order they are handed on in by their files,
// counted where `counting` is 1 and kept where it is 0
typedef struct
{
    int counting;
    unsigned int count;
    unsigned long long used;
    unsigned int *said_file;
    CliInspectFinding *said;
    char *said_text;
    unsigned int *file_first;
    unsigned int *file_said;
    unsigned int *order;
} CliTreeSaidKept;

static CliTreeSaidKept s_kept;

// a finding of the tree's counted, or kept
static void findings_kept(void *context, unsigned int file, CliInspectPlace from, CliInspectPlace to, const char *code,
                          const CliLine *message)
{
    (void)context;
    if (s_kept.counting)
    {
        s_kept.count += 1u;
        s_kept.used += message->at + 1ull;
        return;
    }
    CliInspectFinding *const kept = &s_kept.said[s_kept.count];
    memor.set(&(MemoriaCfg){.dst = kept, .val = 0, .bytes = sizeof(*kept)});
    kept->from = from;
    kept->to = to;
    kept->severity = CLI_INSPECT_WARNING;
    kept->code = code;
    // the messages are what one walk counted, which an unsigned int's places reach
    kept->message.at = (unsigned int)s_kept.used;
    kept->message.size = (unsigned int)message->at;
    memor.cpy(&(MemoriaCfg){.dst = s_kept.said_text + s_kept.used, .src = message->out, .bytes = (size_t)message->at});
    s_kept.used += message->at;
    s_kept.said_text[s_kept.used] = '\0';
    s_kept.used += 1ull;
    s_kept.said_file[s_kept.count] = file;
    s_kept.count += 1u;
    s_kept.file_said[file] += 1u;
}

// the tree's findings put in the order of their files, each file's in the order they were made
static void findings_ordered(void)
{
    unsigned int first = 0u;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        s_kept.file_first[file] = first;
        first += s_kept.file_said[file];
        s_kept.file_said[file] = 0u;
    }
    for (unsigned int said = 0u; said < s_kept.count; said += 1u)
    {
        const unsigned int file = s_kept.said_file[said];
        s_kept.order[s_kept.file_first[file] + s_kept.file_said[file]] = said;
        s_kept.file_said[file] += 1u;
    }
}

// the tree's findings counted, their cell taken at that count, and kept: the cell, NULL where it cannot be taken
static void *findings_taken(void)
{
    s_kept = (CliTreeSaidKept){1, 0u, 0ull, NULL, NULL, NULL, NULL, NULL, NULL};
    cli_tree_cycles(findings_kept, NULL);
    cli_tree_attributes(findings_kept, NULL);
    const unsigned long long count = s_kept.count;
    const unsigned long long files = g_cli_tree_files;
    const unsigned long long finding_bytes = count * sizeof(CliInspectFinding);
    const unsigned long long number_bytes = ((2ull * count) + (2ull * files)) * sizeof(unsigned int);
    unsigned char *const cell =
        cli_cell_taken(finding_bytes + number_bytes + s_kept.used + 16ull, "the tree's findings");
    if (cell == NULL)
    {
        return NULL;
    }
    s_kept.said = (CliInspectFinding *)(void *)cell;
    s_kept.said_file = (unsigned int *)(void *)(cell + finding_bytes);
    s_kept.order = s_kept.said_file + count;
    s_kept.file_first = s_kept.order + count;
    s_kept.file_said = s_kept.file_first + files;
    s_kept.said_text = (char *)(s_kept.file_said + files);
    memor.set(&(MemoriaCfg){.dst = s_kept.file_said, .val = 0, .bytes = (size_t)(files * sizeof(unsigned int))});
    s_kept.counting = 0;
    s_kept.count = 0u;
    s_kept.used = 0ull;
    cli_tree_cycles(findings_kept, NULL);
    cli_tree_attributes(findings_kept, NULL);
    findings_ordered();
    return cell;
}

// finding `finding` of `facts` measured, written into a cell of its size and handed to `each` for `path`
static void findings_handed(const CliFacts *facts, unsigned int finding, const char *path, CliInspectFound each,
                            void *context)
{
    char piece[256];
    CliLine measure;
    cli_measure_begun(&measure, piece, sizeof(piece));
    cli_inspect_finding_written(facts, finding, &measure);
    const unsigned long long size = cli_measured(&measure) + 1ull;
    char *const written = cli_cell_taken(size, "a diagnostic written");
    if (written == NULL)
    {
        return;
    }
    CliLine line = {written, size, 0ull};
    cli_inspect_finding_written(facts, finding, &line);
    if (cli_line_fits(&line))
    {
        each(context, path, written, line.at);
    }
    cli_cell_given(written);
}

void cli_inspect_tree_findings(CliInspectFound each, void *context)
{
    cli_lock_take(&g_cli_tree_lock);
    void *const cell = findings_taken();
    CliFacts tree = {0};
    tree.finding = s_kept.said;
    tree.findings = (cell != NULL) ? s_kept.count : 0u;
    tree.text = s_kept.said_text;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        const CliTreeFile *const held = &CLI_TREE_FILE(file);
        if (!held->used)
        {
            continue;
        }
        each(context, held->path, NULL, 0ull);
        for (unsigned int finding = 0u; finding < held->facts.findings; finding += 1u)
        {
            findings_handed(&held->facts, finding, held->path, each, context);
        }
        for (unsigned int at = 0u; (cell != NULL) && (at < s_kept.file_said[file]); at += 1u)
        {
            findings_handed(&tree, s_kept.order[s_kept.file_first[file] + at], held->path, each, context);
        }
    }
    cli_cell_given(cell);
    s_kept = (CliTreeSaidKept){0, 0u, 0ull, NULL, NULL, NULL, NULL, NULL, NULL};
    cli_lock_give(&g_cli_tree_lock);
}
