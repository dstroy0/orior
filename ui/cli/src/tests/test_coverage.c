// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_coverage.c: a run's coverage: the shim pytest runs under, which marks each line of the tree's files that runs
// and writes them with the lines each file could run, and what the run's processes wrote, put together by file and
// reported
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "test_runs_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// The counts a coverage file's bytes give before it is read: its quotes, two to each key, its runs of digits, one to
// each line marked, and its bytes
typedef struct
{
    unsigned long long quotes;
    unsigned long long numbers;
    unsigned long long bytes;
} CliCoverCounts;
// A reading of one coverage file: its text, its size and where it stands
typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
} CliCoverRead;
// the bytes the coverage reported is written in

const char g_cli_test_shim[] =
    "\n"
    "import json, os, sys, threading\n"
    "out, root, words = sys.argv[1], os.path.normcase(os.path.abspath(sys.argv[2])), sys.argv[3:]\n"
    "ran = {}\n"
    "known = {}\n"
    "def ours(name):\n"
    "    if name not in known:\n"
    "        full = os.path.normcase(os.path.abspath(name))\n"
    "        known[name] = not name.startswith(\"<\") and os.path.isfile(name) and full.startswith(root + os.sep) and "
    "not any(part in full.split(os.sep) for part in (\"site-packages\", \".venv\", \"venv\", \"__pycache__\"))\n"
    "    return known[name]\n"
    "if hasattr(sys, \"monitoring\"):\n"
    "    watch = sys.monitoring\n"
    "    tool = watch.COVERAGE_ID\n"
    "    watch.use_tool_id(tool, \"orior\")\n"
    "    def seen(code, line):\n"
    "        if ours(code.co_filename):\n"
    "            ran.setdefault(code.co_filename, set()).add(line)\n"
    "        return watch.DISABLE\n"
    "    watch.register_callback(tool, watch.events.LINE, seen)\n"
    "    watch.set_events(tool, watch.events.LINE)\n"
    "else:\n"
    "    def trace(frame, event, arg):\n"
    "        if not ours(frame.f_code.co_filename):\n"
    "            return None\n"
    "        def lines(frame, event, arg):\n"
    "            if event == \"line\":\n"
    "                ran.setdefault(frame.f_code.co_filename, set()).add(frame.f_lineno)\n"
    "            return lines\n"
    "        return lines(frame, event, arg)\n"
    "    sys.settrace(trace)\n"
    "    threading.settrace(trace)\n"
    "import pytest\n"
    "code = pytest.main(words)\n"
    "if hasattr(sys, \"monitoring\"):\n"
    "    sys.monitoring.set_events(sys.monitoring.COVERAGE_ID, 0)\n"
    "else:\n"
    "    sys.settrace(None)\n"
    "found = {}\n"
    "for name, lines in ran.items():\n"
    "    could = set()\n"
    "    try:\n"
    "        with open(name, encoding=\"utf-8\") as handle:\n"
    "            codes = [compile(handle.read(), name, \"exec\")]\n"
    "        while codes:\n"
    "            one = codes.pop()\n"
    "            could.update(line for _, _, line in one.co_lines() if line)\n"
    "            codes.extend(inner for inner in one.co_consts if hasattr(inner, \"co_lines\"))\n"
    "    except Exception:\n"
    "        could = set(lines)\n"
    "    found[os.path.relpath(name, root)] = {\"ran\": sorted(lines), \"could\": sorted(could)}\n"
    "with open(out, \"w\", encoding=\"utf-8\") as handle:\n"
    "    json.dump(found, handle)\n"
    "sys.exit(code)\n";

// A line marked: its file, whether it ran or could run, and the line, counted from 0
typedef struct
{
    unsigned int file;
    unsigned int could;
    unsigned int line;
} CliCoverLine;

// The coverage put together: the files by name and the lines marked, parts of one cell sized by a count over the
// run's coverage files before they are read, the files by their quotes, the lines by their numbers and the names by
// their bytes
typedef struct
{
    void *capacity;
    unsigned long long *names;
    unsigned int files;
    unsigned int files_max;
    char *text;
    unsigned long long used;
    unsigned long long text_max;
    CliCoverLine *lines;
    unsigned int line_count;
    unsigned int lines_max;
    unsigned int *order;
} CliCover;

static CliCover s_capacity;
static CliCover *s_cover;

typedef struct
{
    const char *text;
    unsigned long long size;
    CliCoverCounts *counts;
    unsigned char **at;
    unsigned long long bytes;
    const CliCoverCounts *counts_read;
    CliCoverRead *read;
    VerbaLinea *out;
    const char *name;
    unsigned long long length;
    unsigned int file;
    unsigned int could;
    unsigned int from;
    unsigned int to;
    unsigned int line;
} TestCoverageCtx;
// the counts of `size` bytes of `text` added to `counts`
EMBED_INLINE void cover_count(const TestCoverageCtx *args)
{
    int in_digits = 0;
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        const char letter = args->text[at];
        const int digit = (letter >= '0') && (letter <= '9');
        args->counts->quotes += (letter == '"') ? 1ULL : 0ULL;
        args->counts->numbers += (digit && !in_digits) ? 1ULL : 0ULL;
        in_digits = digit;
    }
    args->counts->bytes += args->size;
}

// the next part of a cell, `bytes` long, taken from `*at`, its start aligned to a word's boundary: its start
EMBED_INLINE void *cover_part(const TestCoverageCtx *args)
{
    unsigned char **at = args->at;
    void *const part = *at;
    *at += (args->bytes + 7ULL) & ~7ULL;
    return part;
}

// the coverage's capacity taken for `counts` and its parts set into it: 1, or 0 with no capacity
EMBED_INLINE int cover_work_state_alloc(const TestCoverageCtx *args)
{
    const unsigned long long files = (args->counts_read->quotes / 2ULL) + 1ULL;
    const unsigned long long lines = args->counts_read->numbers + 1ULL;
    const unsigned long long bytes = (files * (sizeof(unsigned long long) + sizeof(unsigned int))) +
                                     (lines * sizeof(CliCoverLine)) + args->counts_read->bytes + 1ULL + (4ULL * 8ULL);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_tests_cells);
    if (cell == NULL)
    {
        return 0;
    }
    unsigned char *at = cell;
    s_cover->capacity = cell;
    s_cover->names = EMBED_CALL(cover_part, TestCoverageCtx, .at = &at, .bytes = files * sizeof(unsigned long long));
    s_cover->order = EMBED_CALL(cover_part, TestCoverageCtx, .at = &at, .bytes = files * sizeof(unsigned int));
    // the counts are of files read whole into memory, which an unsigned int's worth of files and lines represents
    s_cover->files_max = (unsigned int)files;
    s_cover->lines = EMBED_CALL(cover_part, TestCoverageCtx, .at = &at, .bytes = lines * sizeof(CliCoverLine));
    s_cover->lines_max = (unsigned int)lines;
    s_cover->text = EMBED_CALL(cover_part, TestCoverageCtx, .at = &at, .bytes = args->counts_read->bytes + 1ULL);
    s_cover->text_max = args->counts_read->bytes + 1ULL;
    return 1;
}

EMBED_INLINE void cover_white(const TestCoverageCtx *args)
{
    while ((args->read->at < args->read->size) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = args->read->text[args->read->at]}) != NULL))
    {
        args->read->at += 1ULL;
    }
}

// a JSON string at the reading, read into `out` as its letters: 1, or 0 where it is none
static int cover_string(const TestCoverageCtx *args)
{
    cover_white(args);
    if ((args->read->at >= args->read->size) || (args->read->text[args->read->at] != '"'))
    {
        return 0;
    }
    char quoted[CLI_PATH_MAX_LEN + 2u];
    const unsigned long long start = args->read->at;
    args->read->at += 1ULL;
    args->read->at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->read->text,
                                .at = args->read->at, .size = args->read->size, .set = "\"\\");
    while ((args->read->at < args->read->size) && (args->read->text[args->read->at] == '\\'))
    {
        args->read->at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->read->text,
                                    .at = args->read->at + 2ULL, .size = args->read->size, .set = "\"\\");
    }
    args->read->at += 1ULL;
    // the string read back from its JSON, through the tree's own reading of one string
    const unsigned long long length = args->read->at - start;
    if ((args->read->at > args->read->size) || (length > sizeof(quoted)))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = quoted, .src = args->read->text + start, .bytes = length});
    CliJsonNode node[1];
    CfgJsonToken token[1];
    char letters[CLI_PATH_MAX_LEN + 2u];
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = node, .nodes_max = 1u, .text_out = letters, .text_max = sizeof(letters));
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = &json,
                   .text = quoted, .size = length, .tokens = token, .tokens_max = 1u, .err_msg = &unread);
    unsigned long long size = 0ULL;
    const char *const value = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                         .json_read = &json, .node = root, .length_pointer = &size);
    if (value == NULL)
    {
        return 0;
    }
    mmgr_line_put_n(args->out, value, size);
    return 1;
}

// the place of a file's name among those covered, put where it is new: the most files where none is left
EMBED_INLINE unsigned int cover_file(const TestCoverageCtx *args)
{
    for (unsigned int at = 0u; at < s_cover->files; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_cover->text + s_cover->names[at], .cap = CLI_CELLS_POOL}) ==
             args->length) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = s_cover->text + s_cover->names[at], .other = args->name, .bytes = args->length}) == 0))
        {
            return at;
        }
    }
    if ((s_cover->files == s_cover->files_max) || (s_cover->used + args->length + 1ULL > s_cover->text_max))
    {
        return s_cover->files_max;
    }
    s_cover->names[s_cover->files] = s_cover->used;
    memor.cpy(&(MemoriaCfg){.dst = s_cover->text + s_cover->used, .src = args->name, .bytes = args->length});
    s_cover->text[s_cover->used + args->length] = '\0';
    s_cover->used += args->length + 1ULL;
    s_cover->files += 1u;
    return s_cover->files - 1u;
}

// a JSON value at the reading passed over
static void cover_skip(const TestCoverageCtx *args)
{
    cover_white(args);
    if (args->read->at >= args->read->size)
    {
        return;
    }
    const char first = args->read->text[args->read->at];
    if ((first == '[') || (first == '{'))
    {
        const char close = (first == '[') ? ']' : '}';
        args->read->at += 1ULL;
        cover_white(args);
        while ((args->read->at < args->read->size) && (args->read->text[args->read->at] != close))
        {
            if (first == '{')
            {
                char key[CLI_PATH_MAX_LEN];
                VerbaLinea line = {key, sizeof(key), 0ULL};
                (void)EMBED_CALL(cover_string, TestCoverageCtx, .read = args->read, .out = &line);
                cover_white(args);
                args->read->at += 1ULL;
            }
            cover_skip(args);
            cover_white(args);
            args->read->at +=
                ((args->read->at < args->read->size) && (args->read->text[args->read->at] == ',')) ? 1ULL : 0ULL;
            cover_white(args);
        }
        args->read->at += 1ULL;
        return;
    }
    if (first == '"')
    {
        char ignored[CLI_PATH_MAX_LEN];
        VerbaLinea line = {ignored, sizeof(ignored), 0ULL};
        (void)EMBED_CALL(cover_string, TestCoverageCtx, .read = args->read, .out = &line);
        return;
    }
    while ((args->read->at < args->read->size) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = ",]} \t\r\n", .cap = CLI_CELLS_POOL, .byte = args->read->text[args->read->at]}) == NULL))
    {
        args->read->at += 1ULL;
    }
}

// a file's lines that ran, or could run, at the reading: each whole number in it marked, a line counted
// from 1 marked from 0 and cut to 32 bits as Rust's `as u32` cuts it, and any other value passed over
EMBED_INLINE void cover_lines(const TestCoverageCtx *args)
{
    cover_white(args);
    if ((args->read->at >= args->read->size) || (args->read->text[args->read->at] != '['))
    {
        cover_skip(args);
        return;
    }
    args->read->at += 1ULL;
    cover_white(args);
    while ((args->read->at < args->read->size) && (args->read->text[args->read->at] != ']'))
    {
        const unsigned long long start = args->read->at;
        cover_skip(args);
        unsigned long long value = 0ULL;
        const unsigned long long end =
            start + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                               .text = args->read->text + start, .at = args->read->at - start, .set = " \t\r\n");
        const int whole =
            (end > start) && ((end - start) <= 19ULL) &&
            (EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->read->text,
                        .at = start, .size = end, .classes = CLI_TEXT_DIGITS, .set = "") == end);
        value = whole ? EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = args->read->text, .at = start,
                                   .size = end, .end = NULL)
                      : 0ULL;
        if (whole && (args->file < s_cover->files_max) && (s_cover->line_count < s_cover->lines_max))
        {
            s_cover->lines[s_cover->line_count] =
                (CliCoverLine){args->file, args->could, (unsigned int)((value > 0ULL) ? value - 1ULL : 0ULL)};
            s_cover->line_count += 1u;
        }
        cover_white(args);
        args->read->at +=
            ((args->read->at < args->read->size) && (args->read->text[args->read->at] == ',')) ? 1ULL : 0ULL;
        cover_white(args);
    }
    args->read->at += 1ULL;
}

// one coverage file a process wrote, JSON whole and an object, each file it names recorded with the lines it marks
EMBED_INLINE void cover_read(const TestCoverageCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    char tiny[1];
    VerbaLinea checked = {tiny, sizeof(tiny), 0ULL};
    CliCoverRead read = {text, size, 0ULL};
    EMBED_CALL(cover_white, TestCoverageCtx, .read = &read);
    if (!EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = text, .size = size, .out = &checked) ||
        (read.at >= size) || (text[read.at] != '{'))
    {
        return;
    }
    read.at += 1ULL;
    EMBED_CALL(cover_white, TestCoverageCtx, .read = &read);
    while ((read.at < size) && (text[read.at] != '}'))
    {
        char name[CLI_PATH_MAX_LEN];
        VerbaLinea line = {name, sizeof(name), 0ULL};
        (void)EMBED_CALL(cover_string, TestCoverageCtx, .read = &read, .out = &line);
        for (unsigned long long at = 0ULL; at < line.at; at += 1ULL)
        {
            name[at] = (name[at] == '\\') ? '/' : name[at];
        }
        const unsigned int file = EMBED_CALL(cover_file, TestCoverageCtx, .name = name, .length = line.at);
        EMBED_CALL(cover_white, TestCoverageCtx, .read = &read);
        read.at += 1ULL;
        EMBED_CALL(cover_white, TestCoverageCtx, .read = &read);
        if ((read.at < size) && (text[read.at] == '{'))
        {
            read.at += 1ULL;
            EMBED_CALL(cover_white, TestCoverageCtx, .read = &read);
            while ((read.at < size) && (text[read.at] != '}'))
            {
                char key[16];
                VerbaLinea key_line = {key, sizeof(key), 0ULL};
                (void)EMBED_CALL(cover_string, TestCoverageCtx, .read = &read, .out = &key_line);
                mmgr_line_finish(&key_line);
                EMBED_CALL(cover_white, TestCoverageCtx, .read = &read);
                read.at += 1ULL;
                const int run_result =
                    (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "ran", .cap = CLI_CELLS_POOL}));
                const int could = (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "could", .cap = CLI_CELLS_POOL}));
                if (run_result || could)
                {
                    EMBED_CALL(cover_lines, TestCoverageCtx, .read = &read, .file = file, .could = could ? 1u : 0u);
                }
                else
                {
                    EMBED_CALL(cover_skip, TestCoverageCtx, .read = &read);
                }
                EMBED_CALL(cover_white, TestCoverageCtx, .read = &read);
                read.at += ((read.at < size) && (text[read.at] == ',')) ? 1ULL : 0ULL;
                EMBED_CALL(cover_white, TestCoverageCtx, .read = &read);
            }
            read.at += 1ULL;
        }
        else
        {
            EMBED_CALL(cover_skip, TestCoverageCtx, .read = &read);
        }
        EMBED_CALL(cover_white, TestCoverageCtx, .read = &read);
        read.at += ((read.at < size) && (text[read.at] == ',')) ? 1ULL : 0ULL;
        EMBED_CALL(cover_white, TestCoverageCtx, .read = &read);
    }
}

static int cover_line_order(const void *left, const void *right)
{
    const CliCoverLine *const one = (const CliCoverLine *)left;
    const CliCoverLine *const other = (const CliCoverLine *)right;
    if (one->file != other->file)
    {
        return (one->file < other->file) ? -1 : 1;
    }
    if (one->could != other->could)
    {
        return (one->could < other->could) ? -1 : 1;
    }
    if (one->line < other->line)
    {
        return -1;
    }
    if (one->line > other->line)
    {
        return 1;
    }
    return 0;
}

static int cover_file_order(const void *left, const void *right)
{
    return EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                      .one = s_cover->text + s_cover->names[*(const unsigned int *)left],
                      .other_pointer = s_cover->text + s_cover->names[*(const unsigned int *)right]);
}

// whether a file's lines, from `from` to `to` in the lines marked, contain the line `line` as one that could run
EMBED_INLINE int cover_could(const TestCoverageCtx *args)
{
    for (unsigned int at = args->from; at < args->to; at += 1u)
    {
        if ((s_cover->lines[at].could == 1u) && (s_cover->lines[at].line == args->line))
        {
            return 1;
        }
    }
    return 0;
}

// a file's lines written: those that ran, where it could run them, and those it could run that did not, in order
EMBED_INLINE void cover_file_write(const TestCoverageCtx *args)
{
    unsigned int could_from = args->from;
    while ((could_from < args->to) && (s_cover->lines[could_from].could == 0u))
    {
        could_from += 1u;
    }
    mmgr_line_put(args->out, ",\"ran\":[");
    int first = 1;
    for (unsigned int at = args->from; at < could_from; at += 1u)
    {
        const unsigned int line = s_cover->lines[at].line;
        const int again = (at > args->from) && (s_cover->lines[at - 1u].line == line);
        if (!again && ((could_from == args->to) ||
                       EMBED_CALL(cover_could, TestCoverageCtx, .from = could_from, .to = args->to, .line = line)))
        {
            mmgr_line_put(args->out, first ? "" : ",");
            mmgr_line_decimal(args->out, line, 1u);
            first = 0;
        }
    }
    mmgr_line_put(args->out, "],\"missed\":[");
    first = 1;
    for (unsigned int at = could_from; at < args->to; at += 1u)
    {
        const unsigned int line = s_cover->lines[at].line;
        int run_result = (at > could_from) && (s_cover->lines[at - 1u].line == line);
        unsigned int one = args->from;
        while (!run_result && (one < could_from))
        {
            run_result = (s_cover->lines[one].line == line);
            one += 1u;
        }
        if (!run_result)
        {
            mmgr_line_put(args->out, first ? "" : ",");
            mmgr_line_decimal(args->out, line, 1u);
            first = 0;
        }
    }
    mmgr_line_put(args->out, "]}");
}

void cli_test_coverage_emit(const CliTestRunsRunTextCtx *args)
{
    s_cover = &s_capacity;
    // the files counted on one pass and read on the next, into a capacity the counts size
    CliCoverCounts counts = {0ULL, 0ULL, 0ULL};
    for (unsigned int at = 0u; at < args->run->worker_count; at += 1u)
    {
        const CliTestWorker *const worker = &args->run->workers[at];
        unsigned long long size = 0ULL;
        const char *const text = worker->cargo
                                     ? NULL
                                     : EMBED_CALL(cli_test_runs_path_internal.test_file_read, CliTestRunsPathCtx,
                                                  .path = worker->covered, .size_pointer = &size);
        if (text != NULL)
        {
            EMBED_CALL(cover_count, TestCoverageCtx, .text = text, .size = size, .counts = &counts);
        }
        EMBED_CALL(cli_test_runs_text_worker_internal.test_file_let_go, CliTestRunsTextWorkerCtx, .text = text,
                   .size = size);
    }
    if (!EMBED_CALL(cover_work_state_alloc, TestCoverageCtx, .counts_read = &counts))
    {
        s_cover = NULL;
        return;
    }
    s_cover->files = 0u;
    s_cover->used = 0ULL;
    s_cover->line_count = 0u;
    for (unsigned int at = 0u; at < args->run->worker_count; at += 1u)
    {
        const CliTestWorker *const worker = &args->run->workers[at];
        unsigned long long size = 0ULL;
        const char *const text = worker->cargo
                                     ? NULL
                                     : EMBED_CALL(cli_test_runs_path_internal.test_file_read, CliTestRunsPathCtx,
                                                  .path = worker->covered, .size_pointer = &size);
        if (text != NULL)
        {
            EMBED_CALL(cover_read, TestCoverageCtx, .text = text, .size = size);
        }
        EMBED_CALL(cli_test_runs_text_worker_internal.test_file_let_go, CliTestRunsTextWorkerCtx, .text = text,
                   .size = size);
    }
    // a name's byte is escaped to at most 6, a line's number takes at most 11 and its comma, and a file's words are few
    const unsigned long long out_capacity = (s_cover->used * 6ULL) + ((unsigned long long)s_cover->files * 64ULL) +
                                            ((unsigned long long)s_cover->line_count * 12ULL) + 64ULL;
    char *const out_cell =
        (s_cover->files > 0u) ? g_cli_cells->temporary_buf_alloc(out_capacity, &g_cli_tests_cells) : NULL;
    VerbaLinea out = mmgr_line_over(out_cell, out_capacity);
    if (out_cell != NULL)
    {
        qsort(s_cover->lines, s_cover->line_count, sizeof(s_cover->lines[0]), cover_line_order);
        for (unsigned int file = 0u; file < s_cover->files; file += 1u)
        {
            s_cover->order[file] = file;
        }
        qsort(s_cover->order, s_cover->files, sizeof(s_cover->order[0]), cover_file_order);
        mmgr_line_put(&out, "{\"kind\":\"coverage\",\"run\":");
        mmgr_line_decimal(&out, args->run->number, 1u);
        mmgr_line_put(&out, ",\"files\":[");
        for (unsigned int at = 0u; at < s_cover->files; at += 1u)
        {
            const unsigned int file = s_cover->order[at];
            unsigned int from = 0u;
            while ((from < s_cover->line_count) && (s_cover->lines[from].file < file))
            {
                from += 1u;
            }
            unsigned int to = from;
            while ((to < s_cover->line_count) && (s_cover->lines[to].file == file))
            {
                to += 1u;
            }
            mmgr_line_put(&out, (at > 0u) ? ",{\"file\":" : "{\"file\":");
            const char *const name = s_cover->text + s_cover->names[file];
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
                       .text_bytes = (const unsigned char *)name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
            EMBED_CALL(cover_file_write, TestCoverageCtx, .out = &out, .from = from, .to = to);
        }
        mmgr_line_put(&out, "]}");
        if (mmgr_line_ok(&out))
        {
            EMBED_CALL(args->run->sink.emit, CliEachJsonSizeCfg, .context = args->run->sink.context, .json = out.out,
                       .size = out.at);
        }
    }
    g_cli_cells->persistent_buf_release(out.out, &g_cli_tests_cells);
    g_cli_cells->persistent_buf_release(s_cover->capacity, &g_cli_tests_cells);
    s_cover->capacity = NULL;
    s_cover = NULL;
}
