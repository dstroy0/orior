// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_reports.c: what a Python process reports of its tests read: pytest's report in JUnit's form, each test with how
// long it took, a failure's and a skip's message and the line its traceback last names; and the reading of a report's
// text both forms share
#include "test_runs_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// the most attributes a tag of the report holds, and the most bytes their values and a failure's text take
#define CLI_REPORT_ATTRIBUTES 32u
#define CLI_REPORT_VALUES (1u << 18u)
#define CLI_REPORT_TEXT (1u << 22u)

// The attributes of a tag: each name's start and size in the report, and its value, its entities read, in the values,
// a cell of `room` bytes taken larger as a longer tag is read and given back as the report is
typedef struct
{
    const char *name[CLI_REPORT_ATTRIBUTES];
    unsigned long long name_size[CLI_REPORT_ATTRIBUTES];
    unsigned long long value[CLI_REPORT_ATTRIBUTES];
    unsigned int count;
    char *values;
    unsigned long long room;
    unsigned long long used;
} CliReportTag;

CliCellsTally g_cli_test_report_cells = CLI_CELLS_TALLY("cli_test_report");

static CliReportTag s_case;
static CliReportTag s_said;
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

unsigned long long cli_report_find(const char *text, unsigned long long length, const char *needle)
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
    // each value takes its letters, an entity read to one, and a terminator for the `="` and `"` about it, so the
    // values fit in the bytes of the tag's text and one more
    const unsigned long long room = (length < CLI_REPORT_VALUES) ? (length + 1ull) : CLI_REPORT_VALUES;
    if (tag->room < room)
    {
        cli_cell_given_by(&g_cli_test_report_cells, tag->values);
        tag->values = cli_cell_taken_by(&g_cli_test_report_cells, room, "the values of a report's tag");
        tag->room = (tag->values != NULL) ? room : 0ull;
    }
    if (tag->values == NULL)
    {
        return;
    }
    unsigned long long at = 0ull;
    for (;;)
    {
        const unsigned long long equal = at + cli_report_find(text + at, length - at, "=\"");
        if (equal >= length)
        {
            return;
        }
        const unsigned long long from = equal + 2ull;
        const unsigned long long size = cli_report_find(text + from, length - from, "\"");
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
        CliLine value = {tag->values + tag->used, tag->room - tag->used, 0ull};
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

int cli_report_count(const char *text, unsigned long long length, long long *value)
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

int cli_report_ends_with(const char *path, unsigned long long length, const char *wanted)
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

int cli_report_line(const char *text, unsigned long long size, unsigned long long *at, unsigned long long *start,
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
    while (cli_report_line(text, size, &at, &start, &end))
    {
        const unsigned long long place = start + cli_report_find(text + start, end - start, ": ");
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
        if ((colon > start) && cli_report_ends_with(text + start, colon - 1ull - start, wanted) &&
            cli_report_count(text + colon, place - colon, &number))
        {
            *line = (number > 0ll) ? number - 1ll : 0ll;
            found = 1;
        }
    }
    return found;
}

int cli_report_last_line(const char *text, unsigned long long size, int skip_rules, unsigned long long *last,
                         unsigned long long *last_size)
{
    int found = 0;
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long end = 0ull;
    while (cli_report_line(text, size, &at, &start, &end))
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
        !cli_report_count(line_text, cellul.len(&(CatenaFinitaCfg){.src = line_text, .cap = CLI_CELLS_POOL}),
                          &own_line))
    {
        own_line = -1ll;
    }
    const double taken = ((ended != NULL) && (*ended == '\0') && (ended != time)) ? seconds : 0.0;
    static const char *const s_tags[3] = {"<failure", "<error", "<skipped"};
    static const char *const s_outcomes[3] = {"failed", "failed", "skipped"};
    for (unsigned int tag = 0u; tag < 3u; tag += 1u)
    {
        const unsigned long long at = cli_report_find(body, body_size, s_tags[tag]);
        if (at >= body_size)
        {
            continue;
        }
        const unsigned long long tag_end = at + cli_report_find(body + at, body_size - at, ">");
        report_attributes(body + at, tag_end - at, &s_said);
        const unsigned long long text_end =
            (tag_end < body_size) ? tag_end + cli_report_find(body + tag_end, body_size - tag_end, "</") : body_size;
        const unsigned long long text_start = (tag_end + 1ull < text_end) ? tag_end + 1ull : text_end;
        // the text read takes no more bytes than it is written in, an entity read to one letter
        const unsigned long long text_room =
            ((text_end - text_start) < CLI_REPORT_TEXT) ? ((text_end - text_start) + 1ull) : CLI_REPORT_TEXT;
        char *const held = cli_cell_taken_by(&g_cli_test_report_cells, text_room, "a failure's text in a report");
        char refused[1] = {'\0'};
        CliLine text = (held != NULL) ? (CliLine){held, text_room, 0ull} : (CliLine){refused, sizeof(refused), 0ull};
        report_unescaped(body + text_start, text_end - text_start, &text);
        long long line = own_line;
        if (!report_traceback_line(text.out, text.at, wanted, &line))
        {
            line = own_line;
        }
        const char *const message = report_attribute(&s_said, "message");
        unsigned long long last = 0ull;
        unsigned long long last_size = 0ull;
        const int given = (message != NULL) && (message[0] != '\0');
        const int found = !given && cli_report_last_line(text.out, text.at, 0, &last, &last_size);
        cli_test_result_told(worker, id, s_outcomes[tag], given ? message : (found ? text.out + last : ""),
                             given ? cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL})
                                   : (found ? last_size : 0ull),
                             wanted, line, taken);
        cli_cell_given_by(&g_cli_test_report_cells, held);
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
        const unsigned long long start = rest + cli_report_find(xml + rest, size - rest, "<testcase ");
        if (start >= size)
        {
            break;
        }
        const char *const at = xml + start;
        const unsigned long long length = size - start;
        const unsigned long long open_end = cli_report_find(at, length, ">");
        const int lone = (open_end > 0ull) && (open_end <= length) && (at[open_end - 1ull] == '/');
        // a lone tag closes at its own end; any other where its closing tag ends, or at the text's end without one
        const unsigned long long closing = lone ? 0ull : cli_report_find(at, length, "</testcase>");
        const unsigned long long close = lone ? (open_end + 1ull) : ((closing < length) ? closing + 11ull : length);
        const unsigned long long body = (open_end < close) ? open_end : close;
        report_case_told(worker, at, open_end, at + body, close - body);
        rest = start + ((close < length) ? close : length);
    }
    cli_cell_given_by(&g_cli_test_report_cells, s_case.values);
    cli_cell_given_by(&g_cli_test_report_cells, s_said.values);
    s_case = (CliReportTag){0};
    s_said = (CliReportTag){0};
    cli_lock_give(&s_report_lock);
}
