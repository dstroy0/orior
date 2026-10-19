// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_reports.c: what a Python process reports of its tests read: pytest's report in JUnit's form, each test with how
// long it took, a failure's and a skip's message and the line its traceback last names; and unittest's report of its
// failures, each with its last line and the line of its file its traceback last names
#include "test_runs_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// the most attributes a tag of the report holds, and the bytes their values and a failure's text take
#define CLI_REPORT_ATTRIBUTES 32u
#define CLI_REPORT_VALUES (1u << 18u)
#define CLI_REPORT_TEXT (1u << 22u)

// The attributes of a tag: each name's start and size in the report, and its value, its entities read, in the values
typedef struct
{
    const char *name[CLI_REPORT_ATTRIBUTES];
    unsigned long long name_size[CLI_REPORT_ATTRIBUTES];
    unsigned long long value[CLI_REPORT_ATTRIBUTES];
    unsigned int count;
    char values[CLI_REPORT_VALUES];
    unsigned long long used;
} CliReportTag;

static CliReportTag s_case;
static CliReportTag s_said;
static char s_text[CLI_REPORT_TEXT];
static CliLock s_report_lock = CLI_LOCK_INIT;

// `length` bytes of an XML text with their entities read written into `out`
static void report_unescaped(const char *text, unsigned long long length, CliLine *out)
{
    static const char *const s_entities[7] = {"&#10;", "&#13;", "&quot;", "&apos;", "&lt;", "&gt;", "&amp;"};
    static const char s_letters[7] = {'\n', '\r', '"', '\'', '<', '>', '&'};
    unsigned long long at = 0ull;
    while (at < length)
    {
        int read = 0;
        for (unsigned int entity = 0u; !read && (text[at] == '&') && (entity < 7u); entity += 1u)
        {
            const unsigned long long size =
                cellul.len(&(CatenaFinitaCfg){.src = s_entities[entity], .cap = CLI_CELLS_POOL});
            if ((at + size <= length) &&
                (memor.cmp(&(MemoriaCfg){.src = text + at, .other = s_entities[entity], .bytes = size}) == 0))
            {
                cli_line_character(out, s_letters[entity]);
                at += size;
                read = 1;
            }
        }
        if (!read)
        {
            cli_line_character(out, text[at]);
            at += 1ull;
        }
    }
}

// where `needle` is first found in `length` bytes of `text`, `length` where it is not
static unsigned long long report_find(const char *text, unsigned long long length, const char *needle)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = needle, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at + size <= length; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = text + at, .other = needle, .bytes = size}) == 0)
        {
            return at;
        }
    }
    return length;
}

// the attributes of an XML tag's text, `<name a="1" b="2">`, read into `tag`
static void report_attributes(const char *text, unsigned long long length, CliReportTag *tag)
{
    tag->count = 0u;
    tag->used = 0ull;
    unsigned long long at = 0ull;
    for (;;)
    {
        const unsigned long long equal = at + report_find(text + at, length - at, "=\"");
        if (equal >= length)
        {
            return;
        }
        const unsigned long long from = equal + 2ull;
        const unsigned long long size = report_find(text + from, length - from, "\"");
        if (from + size >= length)
        {
            return;
        }
        unsigned long long name = equal;
        while ((name > at) && (cellul.chr(&(CatenaFinitaCfg){
                                   .src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = text[name - 1ull]}) == NULL))
        {
            name -= 1ull;
        }
        CliLine value = {tag->values + tag->used, CLI_REPORT_VALUES - tag->used, 0ull};
        report_unescaped(text + from, size, &value);
        // a name given again takes the later value, as a map's insert does
        unsigned int slot = 0u;
        while ((slot < tag->count) &&
               !((tag->name_size[slot] == equal - name) &&
                 (memor.cmp(&(MemoriaCfg){.src = tag->name[slot], .other = text + name, .bytes = equal - name}) == 0)))
        {
            slot += 1u;
        }
        if ((cli_line_closed(&value) != 0ull) && (slot < CLI_REPORT_ATTRIBUTES))
        {
            tag->name[slot] = text + name;
            tag->name_size[slot] = equal - name;
            tag->value[slot] = tag->used;
            tag->used += value.at + 1ull;
            tag->count += (slot == tag->count) ? 1u : 0u;
        }
        at = from + size + 1ull;
    }
}

// the value of the attribute `name` of a tag, NULL where it has none
static const char *report_attribute(const CliReportTag *tag, const char *name)
{
    for (unsigned int at = 0u; at < tag->count; at += 1u)
    {
        if ((tag->name_size[at] == cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL})) &&
            (memor.cmp(&(MemoriaCfg){.src = tag->name[at], .other = name, .bytes = tag->name_size[at]}) == 0))
        {
            return tag->values + tag->value[at];
        }
    }
    return NULL;
}

// a whole text read as a 32-bit count, a + before it as Rust takes one: 1, or 0 where it is none
static int report_count(const char *text, unsigned long long length, long long *value)
{
    unsigned long long start = 0ull;
    while ((start < length) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = text[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((length > start) && (cellul.chr(&(CatenaFinitaCfg){
                                    .src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = text[length - 1ull]}) != NULL))
    {
        length -= 1ull;
    }
    start += ((start < length) && (text[start] == '+')) ? 1ull : 0ull;
    unsigned long long sum = 0ull;
    for (unsigned long long at = start; at < length; at += 1ull)
    {
        if ((text[at] < '0') || (text[at] > '9') || (sum > 429496729ull))
        {
            return 0;
        }
        sum = (sum * 10ull) + (unsigned long long)(text[at] - '0');
    }
    *value = (long long)sum;
    return (start < length) && (sum <= 4294967295ull);
}

// whether `length` bytes of a path, its backslashes read as slashes, end with `wanted`
static int report_ends_with(const char *path, unsigned long long length, const char *wanted)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_CELLS_POOL});
    if (length < size)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        const char letter = path[length - size + at];
        if (((letter == '\\') ? '/' : letter) != wanted[at])
        {
            return 0;
        }
    }
    return 1;
}

// the next line of `size` bytes from `at`, a "\r" before its "\n" taken off: 1 with its start and end, or 0 where none
// is left
static int report_line(const char *text, unsigned long long size, unsigned long long *at, unsigned long long *start,
                       unsigned long long *end)
{
    if (*at >= size)
    {
        return 0;
    }
    *start = *at;
    const char *const newline = memor.chr(&(MemoriaCfg){.src = text + *at, .val = '\n', .bytes = size - *at});
    *end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
    *at = (newline != NULL) ? *end + 1ull : size;
    *end -= ((newline != NULL) && (*end > *start) && (text[*end - 1ull] == '\r')) ? 1ull : 0ull;
    return 1;
}

// the line of `wanted` a traceback in pytest's form last names, `path:line: ...`, from 0, into `line`: 1, or 0 where it
// names none
static int report_traceback_line(const char *text, unsigned long long size, const char *wanted, long long *line)
{
    int found = 0;
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long end = 0ull;
    while (report_line(text, size, &at, &start, &end))
    {
        const unsigned long long place = start + report_find(text + start, end - start, ": ");
        if (place >= end)
        {
            continue;
        }
        unsigned long long colon = place;
        while ((colon > start) && (text[colon - 1ull] != ':'))
        {
            colon -= 1ull;
        }
        long long number = 0ll;
        if ((colon > start) && report_ends_with(text + start, colon - 1ull - start, wanted) &&
            report_count(text + colon, place - colon, &number))
        {
            *line = (number > 0ll) ? number - 1ll : 0ll;
            found = 1;
        }
    }
    return found;
}

// the last line of a text that is not blank, trimmed, as its start and size: 1, or 0 where every line is blank
static int report_last_line(const char *text, unsigned long long size, int skip_rules, unsigned long long *last,
                            unsigned long long *last_size)
{
    int found = 0;
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long end = 0ull;
    while (report_line(text, size, &at, &start, &end))
    {
        const int rule = skip_rules && (end - start >= 3ull) &&
                         (memor.cmp(&(MemoriaCfg){.src = text + start, .other = "---", .bytes = 3u}) == 0);
        while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = text[start]}) != NULL))
        {
            start += 1ull;
        }
        while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = text[end - 1ull]}) != NULL))
        {
            end -= 1ull;
        }
        if ((end > start) && !rule)
        {
            *last = start;
            *last_size = end - start;
            found = 1;
        }
    }
    return found;
}

// one test case of the report told: its outcome, the message and line of a failure, an error or a skip, and how long it
// took; a test that passed is told for how long it took
static void report_case_told(CliTestWorker *worker, const char *open, unsigned long long open_size, const char *body,
                             unsigned long long body_size)
{
    report_attributes(open, open_size, &s_case);
    const char *const file = report_attribute(&s_case, "file");
    if (file == NULL)
    {
        return;
    }
    const char *const name = report_attribute(&s_case, "name");
    const char *const classname = report_attribute(&s_case, "classname");
    char wanted[CLI_PATH_LONGEST];
    char module[CLI_PATH_LONGEST];
    char id[CLI_PATH_LONGEST * 2u];
    CliLine wanted_line = {wanted, sizeof(wanted), 0ull};
    CliLine module_line = {module, sizeof(module), 0ull};
    size_t file_size = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL});
    for (size_t at = 0u; at < file_size; at += 1u)
    {
        cli_line_character(&wanted_line, (file[at] == '\\') ? '/' : file[at]);
    }
    cli_line_closed(&wanted_line);
    while ((file_size >= 3u) &&
           (memor.cmp(&(MemoriaCfg){.src = file + file_size - 3u, .other = ".py", .bytes = 3u}) == 0))
    {
        file_size -= 3u;
    }
    for (size_t at = 0u; at < file_size; at += 1u)
    {
        cli_line_character(&module_line, ((file[at] == '/') || (file[at] == '\\')) ? '.' : file[at]);
    }
    cli_line_closed(&module_line);
    const char *class_name =
        ((classname != NULL) &&
         cli_text_same(classname, module, cellul.len(&(CatenaFinitaCfg){.src = module, .cap = CLI_CELLS_POOL})))
            ? classname + cellul.len(&(CatenaFinitaCfg){.src = module, .cap = CLI_CELLS_POOL})
            : NULL;
    while ((class_name != NULL) && (*class_name == '.'))
    {
        class_name += 1;
    }
    CliLine id_line = {id, sizeof(id), 0ull};
    cli_line_text(&id_line, wanted);
    cli_line_text(&id_line, "::");
    for (const char *letter = ((class_name != NULL) && (*class_name != '\0')) ? class_name : ""; *letter != '\0';
         letter += 1)
    {
        cli_line_text(&id_line, (*letter == '.') ? "::" : "");
        cli_line_counted(&id_line, letter, (*letter == '.') ? 0ull : 1ull);
    }
    cli_line_text(&id_line, ((class_name != NULL) && (*class_name != '\0')) ? "::" : "");
    cli_line_text(&id_line, (name != NULL) ? name : "");
    cli_line_closed(&id_line);
    const char *const time = report_attribute(&s_case, "time");
    char *ended = NULL;
    const double seconds = (time != NULL) ? strtod(time, &ended) : 0.0;
    long long own_line = -1ll;
    const char *const line_text = report_attribute(&s_case, "line");
    if ((line_text == NULL) ||
        !report_count(line_text, cellul.len(&(CatenaFinitaCfg){.src = line_text, .cap = CLI_CELLS_POOL}), &own_line))
    {
        own_line = -1ll;
    }
    const double taken = ((ended != NULL) && (*ended == '\0') && (ended != time)) ? seconds : 0.0;
    static const char *const s_tags[3] = {"<failure", "<error", "<skipped"};
    static const char *const s_outcomes[3] = {"failed", "failed", "skipped"};
    for (unsigned int tag = 0u; tag < 3u; tag += 1u)
    {
        const unsigned long long at = report_find(body, body_size, s_tags[tag]);
        if (at >= body_size)
        {
            continue;
        }
        const unsigned long long tag_end = at + report_find(body + at, body_size - at, ">");
        report_attributes(body + at, tag_end - at, &s_said);
        const unsigned long long text_end =
            (tag_end < body_size) ? tag_end + report_find(body + tag_end, body_size - tag_end, "</") : body_size;
        const unsigned long long text_start = (tag_end + 1ull < text_end) ? tag_end + 1ull : text_end;
        CliLine text = {s_text, sizeof(s_text), 0ull};
        report_unescaped(body + text_start, text_end - text_start, &text);
        long long line = own_line;
        if (!report_traceback_line(s_text, text.at, wanted, &line))
        {
            line = own_line;
        }
        const char *const message = report_attribute(&s_said, "message");
        unsigned long long last = 0ull;
        unsigned long long last_size = 0ull;
        const int given = (message != NULL) && (message[0] != '\0');
        const int found = !given && report_last_line(s_text, text.at, 0, &last, &last_size);
        cli_test_result_told(worker, id, s_outcomes[tag], given ? message : (found ? s_text + last : ""),
                             given ? cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL})
                                   : (found ? last_size : 0ull),
                             wanted, line, taken);
        return;
    }
    cli_test_result_told(worker, id, "passed", "", 0ull, wanted, own_line, taken);
}

void cli_test_report_told(CliTestWorker *worker, const char *xml, unsigned long long size)
{
    cli_lock_take(&s_report_lock);
    unsigned long long rest = 0ull;
    for (;;)
    {
        const unsigned long long start = rest + report_find(xml + rest, size - rest, "<testcase ");
        if (start >= size)
        {
            break;
        }
        const char *const at = xml + start;
        const unsigned long long length = size - start;
        const unsigned long long open_end = report_find(at, length, ">");
        const int lone = (open_end > 0ull) && (open_end <= length) && (at[open_end - 1ull] == '/');
        unsigned long long close = length;
        if (lone)
        {
            close = open_end + 1ull;
        }
        else
        {
            const unsigned long long closing = report_find(at, length, "</testcase>");
            close = (closing < length) ? closing + 11ull : length;
        }
        const unsigned long long body = (open_end < close) ? open_end : close;
        report_case_told(worker, at, open_end, at + body, close - body);
        rest = start + ((close < length) ? close : length);
    }
    cli_lock_give(&s_report_lock);
}

// one failure of unittest's report told: its name as pytest gives it, the last line of its traceback, and the line of
// its file the traceback last names, the rule under its head and the rule after it bounding the traceback
static void report_failure_told(CliTestWorker *worker, const char *block, unsigned long long size)
{
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long end = 0ull;
    (void)report_line(block, size, &at, &start, &end);
    const unsigned long long open = report_find(block, end, " (");
    if (open >= end)
    {
        return;
    }
    unsigned long long dotted_end = end;
    while ((dotted_end > open + 2ull) && (block[dotted_end - 1ull] == ')'))
    {
        dotted_end -= 1ull;
    }
    char id[CLI_PATH_LONGEST * 2u];
    if (!cli_test_unittest_id(block + open + 2ull, dotted_end - open - 2ull, id, sizeof(id)))
    {
        return;
    }
    char wanted[CLI_PATH_LONGEST];
    const char *const path =
        cellul.find(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL, .other = "::", .other_cap = CLI_CELLS_POOL});
    memor.cpy(&(MemoriaCfg){
        .dst = wanted, .src = id, .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
    wanted[(path != NULL) ? (size_t)(path - id) : cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL})] =
        '\0';
    unsigned long long body = size;
    unsigned long long body_end = size;
    while (report_line(block, size, &at, &start, &end))
    {
        if ((end - start >= 3ull) && (memor.cmp(&(MemoriaCfg){.src = block + start, .other = "---", .bytes = 3u}) == 0))
        {
            body = at;
            break;
        }
    }
    at = body;
    while (report_line(block, size, &at, &start, &end))
    {
        if ((end - start >= 3ull) && (memor.cmp(&(MemoriaCfg){.src = block + start, .other = "---", .bytes = 3u}) == 0))
        {
            body_end = start;
            break;
        }
    }
    body = (body < body_end) ? body : body_end;
    long long line = -1ll;
    at = body;
    while (report_line(block, body_end, &at, &start, &end))
    {
        while ((start < end) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_CELLS_POOL, .byte = block[start]}) != NULL))
        {
            start += 1ull;
        }
        if ((end - start < 6ull) ||
            (memor.cmp(&(MemoriaCfg){.src = block + start, .other = "File \"", .bytes = 6u}) != 0))
        {
            continue;
        }
        const unsigned long long quote = start + 6ull + report_find(block + start + 6ull, end - start - 6ull, "\"");
        if (quote >= end)
        {
            continue;
        }
        unsigned long long number = quote + 1ull;
        while ((number + 7ull <= end) &&
               (memor.cmp(&(MemoriaCfg){.src = block + number, .other = ", line ", .bytes = 7u}) == 0))
        {
            number += 7ull;
        }
        const unsigned long long number_end = number + report_find(block + number, end - number, ",");
        long long value = 0ll;
        if (report_ends_with(block + start + 6ull, quote - start - 6ull, wanted) &&
            report_count(block + number, number_end - number, &value))
        {
            line = (value > 0ll) ? value - 1ll : 0ll;
        }
    }
    unsigned long long last = 0ull;
    unsigned long long last_size = 0ull;
    const int found = report_last_line(block + body, body_end - body, 1, &last, &last_size);
    cli_test_result_told(worker, id, "failed", found ? block + body + last : "", found ? last_size : 0ull, wanted, line,
                         0.0);
}

void cli_test_unittest_failures_told(CliTestWorker *worker, const char *text, unsigned long long size)
{
    static const char s_rule[] = "\n======================================================================\n";
    const unsigned long long rule = sizeof(s_rule) - 1ull;
    unsigned long long at = report_find(text, size, s_rule);
    while (at < size)
    {
        const unsigned long long from = at + rule;
        const unsigned long long next = from + report_find(text + from, size - from, s_rule);
        report_failure_told(worker, text + from, ((next < size) ? next : size) - from);
        at = next;
    }
}
