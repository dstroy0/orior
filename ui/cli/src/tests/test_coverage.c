// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_coverage.c: a run's coverage: the shim pytest runs under, which marks each line of the tree's files that runs
// and writes them with the lines each file could run, and what the run's processes wrote, put together by file and told
#include "../command_line/cells.h"
#include "test_runs_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// the most files covered, the bytes their names take, the lines marked, and the bytes the coverage told takes
#define CLI_COVER_FILES 16384u
#define CLI_COVER_NAMES (1u << 22u)
#define CLI_COVER_LINES (1u << 21u)
#define CLI_COVER_TOLD (1u << 25u)

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

// The coverage put together: the files by name and the lines marked
typedef struct
{
    unsigned long long names[CLI_COVER_FILES];
    unsigned int files;
    char text[CLI_COVER_NAMES];
    unsigned long long used;
    CliCoverLine lines[CLI_COVER_LINES];
    unsigned int line_count;
    unsigned int order[CLI_COVER_FILES];
    char told[CLI_COVER_TOLD];
} CliCover;

static CliCover *s_cover;

// A reading of one coverage file: its text, its size and where it stands
typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
} CliCoverRead;

static void cover_white(CliCoverRead *read)
{
    while (
        (read->at < read->size) &&
        (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = read->text[read->at]}) != NULL))
    {
        read->at += 1ull;
    }
}

// a JSON string at the reading, read into `out` as its letters: 1, or 0 where it is none
static int cover_string(CliCoverRead *read, CliLine *out)
{
    cover_white(read);
    if ((read->at >= read->size) || (read->text[read->at] != '"'))
    {
        return 0;
    }
    char quoted[CLI_PATH_LONGEST + 2u];
    const unsigned long long start = read->at;
    read->at += 1ull;
    while ((read->at < read->size) && (read->text[read->at] != '"'))
    {
        read->at += (read->text[read->at] == '\\') ? 2ull : 1ull;
    }
    read->at += 1ull;
    // the string read back from its JSON, through the tree's own reading of one string
    const unsigned long long length = read->at - start;
    if ((read->at > read->size) || (length > sizeof(quoted)))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = quoted, .src = read->text + start, .bytes = length});
    CliJsonNode node[1];
    CfgJsonToken token[1];
    char letters[CLI_PATH_LONGEST + 2u];
    CliJson json;
    cli_json_begun(&json, node, 1u, letters, sizeof(letters));
    char why[CLI_SAID_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_read(&json, quoted, length, token, 1u, &unread);
    unsigned long long size = 0ull;
    const char *const value = cli_json_string(&json, root, &size);
    if (value == NULL)
    {
        return 0;
    }
    cli_line_counted(out, value, size);
    return 1;
}

// the place of a file's name among those covered, put where it is new: CLI_COVER_FILES where none is left
static unsigned int cover_file(const char *name, unsigned long long length)
{
    for (unsigned int at = 0u; at < s_cover->files; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_cover->text + s_cover->names[at], .cap = CLI_CELLS_POOL}) ==
             length) &&
            (memor.cmp(&(MemoriaCfg){.src = s_cover->text + s_cover->names[at], .other = name, .bytes = length}) == 0))
        {
            return at;
        }
    }
    if ((s_cover->files == CLI_COVER_FILES) || (s_cover->used + length + 1ull > CLI_COVER_NAMES))
    {
        return CLI_COVER_FILES;
    }
    s_cover->names[s_cover->files] = s_cover->used;
    memor.cpy(&(MemoriaCfg){.dst = s_cover->text + s_cover->used, .src = name, .bytes = length});
    s_cover->text[s_cover->used + length] = '\0';
    s_cover->used += length + 1ull;
    s_cover->files += 1u;
    return s_cover->files - 1u;
}

// a JSON value at the reading passed over
static void cover_skipped(CliCoverRead *read)
{
    cover_white(read);
    if (read->at >= read->size)
    {
        return;
    }
    const char first = read->text[read->at];
    if ((first == '[') || (first == '{'))
    {
        const char close = (first == '[') ? ']' : '}';
        read->at += 1ull;
        cover_white(read);
        while ((read->at < read->size) && (read->text[read->at] != close))
        {
            if (first == '{')
            {
                char key[CLI_PATH_LONGEST];
                CliLine line = {key, sizeof(key), 0ull};
                (void)cover_string(read, &line);
                cover_white(read);
                read->at += 1ull;
            }
            cover_skipped(read);
            cover_white(read);
            read->at += ((read->at < read->size) && (read->text[read->at] == ',')) ? 1ull : 0ull;
            cover_white(read);
        }
        read->at += 1ull;
        return;
    }
    if (first == '"')
    {
        char ignored[CLI_PATH_LONGEST];
        CliLine line = {ignored, sizeof(ignored), 0ull};
        (void)cover_string(read, &line);
        return;
    }
    while ((read->at < read->size) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = ",]} \t\r\n", .cap = CLI_CELLS_POOL, .byte = read->text[read->at]}) ==
            NULL))
    {
        read->at += 1ull;
    }
}

// an array of a file's lines that ran, or could run, at the reading: each whole number in it marked, a line counted
// from 1 marked from 0 and cut to 32 bits as Rust's `as u32` cuts it, and any other value passed over
static void cover_lines(CliCoverRead *read, unsigned int file, unsigned int could)
{
    cover_white(read);
    if ((read->at >= read->size) || (read->text[read->at] != '['))
    {
        cover_skipped(read);
        return;
    }
    read->at += 1ull;
    cover_white(read);
    while ((read->at < read->size) && (read->text[read->at] != ']'))
    {
        const unsigned long long start = read->at;
        cover_skipped(read);
        unsigned long long value = 0ull;
        unsigned long long end = read->at;
        while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = read->text[end - 1ull]}) != NULL))
        {
            end -= 1ull;
        }
        int whole = (end > start);
        for (unsigned long long at = start; whole && (at < end); at += 1ull)
        {
            const unsigned long long digit = (unsigned long long)(read->text[at] - '0');
            whole = (read->text[at] >= '0') && (read->text[at] <= '9') && (value <= (~0ull - digit) / 10ull);
            value = whole ? (value * 10ull) + digit : value;
        }
        if (whole && (file < CLI_COVER_FILES) && (s_cover->line_count < CLI_COVER_LINES))
        {
            s_cover->lines[s_cover->line_count] =
                (CliCoverLine){file, could, (unsigned int)((value > 0ull) ? value - 1ull : 0ull)};
            s_cover->line_count += 1u;
        }
        cover_white(read);
        read->at += ((read->at < read->size) && (read->text[read->at] == ',')) ? 1ull : 0ull;
        cover_white(read);
    }
    read->at += 1ull;
}

// one coverage file a process wrote, JSON whole and an object, each file it names kept with the lines it marks
static void cover_read(const char *text, unsigned long long size)
{
    char tiny[1];
    CliLine checked = {tiny, sizeof(tiny), 0ull};
    CliCoverRead read = {text, size, 0ull};
    cover_white(&read);
    if (!cli_json_compacted(text, size, &checked) || (read.at >= size) || (text[read.at] != '{'))
    {
        return;
    }
    read.at += 1ull;
    cover_white(&read);
    while ((read.at < size) && (text[read.at] != '}'))
    {
        char name[CLI_PATH_LONGEST];
        CliLine line = {name, sizeof(name), 0ull};
        (void)cover_string(&read, &line);
        for (unsigned long long at = 0ull; at < line.at; at += 1ull)
        {
            name[at] = (name[at] == '\\') ? '/' : name[at];
        }
        const unsigned int file = cover_file(name, line.at);
        cover_white(&read);
        read.at += 1ull;
        cover_white(&read);
        if ((read.at < size) && (text[read.at] == '{'))
        {
            read.at += 1ull;
            cover_white(&read);
            while ((read.at < size) && (text[read.at] != '}'))
            {
                char key[16];
                CliLine key_line = {key, sizeof(key), 0ull};
                (void)cover_string(&read, &key_line);
                cli_line_closed(&key_line);
                cover_white(&read);
                read.at += 1ull;
                const int ran = (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "ran", .cap = CLI_CELLS_POOL}));
                const int could = (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "could", .cap = CLI_CELLS_POOL}));
                if (ran || could)
                {
                    cover_lines(&read, file, could ? 1u : 0u);
                }
                else
                {
                    cover_skipped(&read);
                }
                cover_white(&read);
                read.at += ((read.at < size) && (text[read.at] == ',')) ? 1ull : 0ull;
                cover_white(&read);
            }
            read.at += 1ull;
        }
        else
        {
            cover_skipped(&read);
        }
        cover_white(&read);
        read.at += ((read.at < size) && (text[read.at] == ',')) ? 1ull : 0ull;
        cover_white(&read);
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
    return (one->line < other->line) ? -1 : (one->line > other->line) ? 1 : 0;
}

static int cover_file_order(const void *left, const void *right)
{
    return cli_text_order(s_cover->text + s_cover->names[*(const unsigned int *)left],
                          s_cover->text + s_cover->names[*(const unsigned int *)right]);
}

// whether a file's lines, from `from` to `to` in the lines marked, hold the line `line` as one that could run
static int cover_could(unsigned int from, unsigned int to, unsigned int line)
{
    for (unsigned int at = from; at < to; at += 1u)
    {
        if ((s_cover->lines[at].could == 1u) && (s_cover->lines[at].line == line))
        {
            return 1;
        }
    }
    return 0;
}

// a file's lines written: those that ran, where it could run them, and those it could run that did not, in order
static void cover_file_written(CliLine *out, unsigned int from, unsigned int to)
{
    unsigned int could_from = from;
    while ((could_from < to) && (s_cover->lines[could_from].could == 0u))
    {
        could_from += 1u;
    }
    cli_line_text(out, ",\"ran\":[");
    int first = 1;
    for (unsigned int at = from; at < could_from; at += 1u)
    {
        const unsigned int line = s_cover->lines[at].line;
        const int again = (at > from) && (s_cover->lines[at - 1u].line == line);
        if (!again && ((could_from == to) || cover_could(could_from, to, line)))
        {
            cli_line_text(out, first ? "" : ",");
            cli_line_decimal(out, line, 1u);
            first = 0;
        }
    }
    cli_line_text(out, "],\"missed\":[");
    first = 1;
    for (unsigned int at = could_from; at < to; at += 1u)
    {
        const unsigned int line = s_cover->lines[at].line;
        int ran = (at > could_from) && (s_cover->lines[at - 1u].line == line);
        for (unsigned int one = from; !ran && (one < could_from); one += 1u)
        {
            ran = (s_cover->lines[one].line == line);
        }
        if (!ran)
        {
            cli_line_text(out, first ? "" : ",");
            cli_line_decimal(out, line, 1u);
            first = 0;
        }
    }
    cli_line_text(out, "]}");
}

void cli_test_coverage_told(CliTestRun *run)
{
    static CliLock s_cover_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_cover_lock);
    s_cover = cli_cell_taken(sizeof(*s_cover), "a run's coverage");
    if (s_cover == NULL)
    {
        cli_lock_give(&s_cover_lock);
        return;
    }
    s_cover->files = 0u;
    s_cover->used = 0ull;
    s_cover->line_count = 0u;
    for (unsigned int at = 0u; at < run->worker_count; at += 1u)
    {
        const CliTestWorker *const worker = &run->workers[at];
        unsigned long long size = 0ull;
        const char *const text = worker->cargo ? NULL : cli_test_file_read(worker->covered, &size);
        if (text != NULL)
        {
            cover_read(text, size);
        }
        cli_test_file_let_go(text, size);
    }
    if (s_cover->files > 0u)
    {
        qsort(s_cover->lines, s_cover->line_count, sizeof(s_cover->lines[0]), cover_line_order);
        for (unsigned int file = 0u; file < s_cover->files; file += 1u)
        {
            s_cover->order[file] = file;
        }
        qsort(s_cover->order, s_cover->files, sizeof(s_cover->order[0]), cover_file_order);
        CliLine out = {s_cover->told, sizeof(s_cover->told), 0ull};
        cli_line_text(&out, "{\"kind\":\"coverage\",\"run\":");
        cli_line_decimal(&out, run->number, 1u);
        cli_line_text(&out, ",\"files\":[");
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
            cli_line_text(&out, (at > 0u) ? ",{\"file\":" : "{\"file\":");
            const char *const name = s_cover->text + s_cover->names[file];
            cli_json_quoted_lossy(&out, (const unsigned char *)name,
                                  cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
            cover_file_written(&out, from, to);
        }
        cli_line_text(&out, "]}");
        if (cli_line_fits(&out))
        {
            run->tell.told(run->tell.context, out.out, out.at);
        }
    }
    cli_cell_given(s_cover);
    s_cover = NULL;
    cli_lock_give(&s_cover_lock);
}
