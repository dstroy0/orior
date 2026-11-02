// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_reports.c: what a Python process reports of its tests read: pytest's report in JUnit's form, each test with how
// long it took, a failure's and a skip's message and the line its traceback last names; and the reading of a report's
// text both forms share
#include "test_runs_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// the most attributes a tag of the report contains, and the most bytes their values and a failure's text take
#define CLI_REPORT_ATTRIBUTES 32u
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
#define CLI_REPORT_VALUES (1u << 18u)
#define CLI_REPORT_TEXT (1u << 22u)

static CliReportTag s_case;
static CliReportTag s_tag;
static CliLock s_report_lock = CLI_LOCK_INIT;

typedef struct
{
    const char *text;
    unsigned long long length;
    VerbaLinea *out;
    CliReportTag *tag;
    const CliReportTag *tag_read;
    const char *name;
    unsigned long long size;
    const char *wanted;
    long long *line;
    CliTestWorker *worker;
    const char *open;
    unsigned long long open_size;
    const char *body;
    unsigned long long body_size;
} TestReportsCtx;
// `length` bytes of an XML text with their entities read written into `out`
EMBED_INLINE void report_unescaped(const TestReportsCtx *args)
{
    static const char *const s_entities[7] = {"&#10;", "&#13;", "&quot;", "&apos;", "&lt;", "&gt;", "&amp;"};
    static const char s_letters[7] = {'\n', '\r', '"', '\'', '<', '>', '&'};
    unsigned long long at = 0ull;
    while (at < args->length)
    {
        int read = 0;
        for (unsigned int entity = 0u; !read && (args->text[at] == '&') && (entity < 7u); entity += 1u)
        {
            const unsigned long long size =
                cellul.len(&(CatenaFinitaCfg){.src = s_entities[entity], .cap = CLI_CELLS_POOL});
            if ((at + size <= args->length) &&
                (memor.cmp(&(MemoriaCfg){.src = args->text + at, .other = s_entities[entity], .bytes = size}) == 0))
            {
                mmgr_line_ch(args->out, s_letters[entity]);
                at += size;
                read = 1;
            }
        }
        if (!read)
        {
            mmgr_line_ch(args->out, args->text[at]);
            at += 1ull;
        }
    }
}

unsigned long long cli_report_find(const CliTestRunsTextLengthNeedleCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->needle, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at + size <= args->length; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = args->text + at, .other = args->needle, .bytes = size}) == 0)
        {
            return at;
        }
    }
    return args->length;
}

// the attributes of an XML tag's text, `<name a="1" b="2">`, read into `tag`
static void report_attributes(const TestReportsCtx *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    args->tag->count = 0u;
    args->tag->used = 0ull;
    // each value takes its letters, an entity read to one, and a terminator for the `="` and `"` about it, so the
    // values fit in the bytes of the tag's text and one more
    const unsigned long long room = (length < CLI_REPORT_VALUES) ? (length + 1ull) : CLI_REPORT_VALUES;
    if (args->tag->room < room)
    {
        g_cli_cells->persistent_buf_release(args->tag->values, &g_cli_test_report_cells);
        args->tag->values = g_cli_cells->persistent_buf_alloc(room, &g_cli_test_report_cells);
        args->tag->room = (args->tag->values != NULL) ? room : 0ull;
    }
    if (args->tag->values == NULL)
    {
        return;
    }
    unsigned long long at = 0ull;
    for (;;)
    {
        const unsigned long long equal =
            at + EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                            .text = text + at, .length = length - at, .needle = "=\"");
        if (equal >= length)
        {
            return;
        }
        const unsigned long long from = equal + 2ull;
        const unsigned long long size =
            EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                       .text = text + from, .length = length - from, .needle = "\"");
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
        VerbaLinea value = {args->tag->values + args->tag->used, args->tag->room - args->tag->used, 0ull};
        EMBED_CALL(report_unescaped, TestReportsCtx, .text = text + from, .length = size, .out = &value);
        // a name given again takes the later value, as a map's insert does
        unsigned int index = 0u;
        while ((index < args->tag->count) &&
               !((args->tag->name_size[index] == equal - name) &&
                 (memor.cmp(
                      &(MemoriaCfg){.src = args->tag->name[index], .other = text + name, .bytes = equal - name}) == 0)))
        {
            index += 1u;
        }
        if ((mmgr_line_finish(&value) != 0ull) && (index < CLI_REPORT_ATTRIBUTES))
        {
            args->tag->name[index] = text + name;
            args->tag->name_size[index] = equal - name;
            args->tag->value[index] = args->tag->used;
            args->tag->used += value.at + 1ull;
            args->tag->count += (index == args->tag->count) ? 1u : 0u;
        }
        at = from + size + 1ull;
    }
}

// the value of the attribute `name` of a tag, NULL where it has none
EMBED_INLINE const char *report_attribute(const TestReportsCtx *args)
{
    for (unsigned int at = 0u; at < args->tag_read->count; at += 1u)
    {
        if ((args->tag_read->name_size[at] ==
             cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL})) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = args->tag_read->name[at], .other = args->name, .bytes = args->tag_read->name_size[at]}) == 0))
        {
            return args->tag_read->values + args->tag_read->value[at];
        }
    }
    return NULL;
}

int cli_report_count(const CliTestRunsTextLengthCtx *args)
{
    unsigned long long length = args->length;
    long long *value = args->value;
    unsigned long long start = 0ull;
    while ((start < length) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = args->text[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((length > start) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = args->text[length - 1ull]}) != NULL))
    {
        length -= 1ull;
    }
    start += ((start < length) && (args->text[start] == '+')) ? 1ull : 0ull;
    unsigned long long sum = 0ull;
    for (unsigned long long at = start; at < length; at += 1ull)
    {
        if ((args->text[at] < '0') || (args->text[at] > '9') || (sum > 429496729ull))
        {
            return 0;
        }
        sum = (sum * 10ull) + (unsigned long long)(args->text[at] - '0');
    }
    *value = (long long)sum;
    return (start < length) && (sum <= 4294967295ull);
}

int cli_report_ends_with(const CliTestRunsLengthPathCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->wanted, .cap = CLI_CELLS_POOL});
    if (args->length < size)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        const char letter = args->path[args->length - size + at];
        if (((letter == '\\') ? '/' : letter) != args->wanted[at])
        {
            return 0;
        }
    }
    return 1;
}

int cli_report_line(const CliTestRunsTextSizeCtx *args)
{
    unsigned long long *at = args->at;
    unsigned long long *start = args->start;
    unsigned long long *end = args->end;
    if (*at >= args->size)
    {
        return 0;
    }
    *start = *at;
    const CliTextLine bounds =
        EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = args->text, .at = *at, .size = args->size);
    *end = bounds.stop;
    *at = bounds.next;
    *end -= bounds.stop - bounds.cut;
    return 1;
}

// the line of `wanted` a traceback in pytest's form last names, `path:line: ...`, from 0, into `line`: 1, or 0 where it
// names none
EMBED_INLINE int report_traceback_line(const TestReportsCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    const char *wanted = args->wanted;
    long long *line = args->line;
    int found = 0;
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long end = 0ull;
    while (EMBED_CALL(cli_test_runs_text_size_internal.report_line, CliTestRunsTextSizeCtx, .text = text, .size = size,
                      .at = &at, .start = &start, .end = &end))
    {
        const unsigned long long place =
            start + EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                               .text = text + start, .length = end - start, .needle = ": ");
        if (place >= end)
        {
            continue;
        }
        unsigned long long colon = place;
        colon = (start + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = text + start,
                                    .at = colon - start, .set = ":"));
        long long number = 0ll;
        if ((colon > start) &&
            EMBED_CALL(cli_test_runs_length_path_internal.report_ends_with, CliTestRunsLengthPathCtx,
                       .path = text + start, .length = colon - 1ull - start, .wanted = wanted) &&
            EMBED_CALL(cli_test_runs_text_length_internal.report_count, CliTestRunsTextLengthCtx, .text = text + colon,
                       .length = place - colon, .value = &number))
        {
            *line = (number > 0ll) ? number - 1ll : 0ll;
            found = 1;
        }
    }
    return found;
}

int cli_report_last_line(const CliTestRunsTextCtx *args)
{
    unsigned long long *last = args->last;
    unsigned long long *last_size = args->last_size;
    int found = 0;
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long end = 0ull;
    while (EMBED_CALL(cli_test_runs_text_size_internal.report_line, CliTestRunsTextSizeCtx, .text = args->text,
                      .size = args->size, .at = &at, .start = &start, .end = &end))
    {
        const int rule = args->skip_rules && (end - start >= 3ull) &&
                         (memor.cmp(&(MemoriaCfg){.src = args->text + start, .other = "---", .bytes = 3u}) == 0);
        while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = args->text[start]}) != NULL))
        {
            start += 1ull;
        }
        while ((end > start) &&
               (cellul.chr(&(CatenaFinitaCfg){
                    .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = args->text[end - 1ull]}) != NULL))
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

// one test case of the report reported: its outcome, the message and line of a failure, an error or a skip, and how
// long it took; a test that passed is reported for how long it took
EMBED_INLINE void report_case_emit(const TestReportsCtx *args)
{
    CliTestWorker *worker = args->worker;
    EMBED_CALL(report_attributes, TestReportsCtx, .text = args->open, .length = args->open_size, .tag = &s_case);
    const char *const file = EMBED_CALL(report_attribute, TestReportsCtx, .tag_read = &s_case, .name = "file");
    if (file == NULL)
    {
        return;
    }
    const char *const name = EMBED_CALL(report_attribute, TestReportsCtx, .tag_read = &s_case, .name = "name");
    const char *const classname =
        EMBED_CALL(report_attribute, TestReportsCtx, .tag_read = &s_case, .name = "classname");
    char wanted[CLI_PATH_MAX_LEN];
    char module[CLI_PATH_MAX_LEN];
    char id[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea wanted_line = {wanted, sizeof(wanted), 0ull};
    VerbaLinea module_line = {module, sizeof(module), 0ull};
    size_t file_size = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL});
    for (size_t at = 0u; at < file_size; at += 1u)
    {
        mmgr_line_ch(&wanted_line, (file[at] == '\\') ? '/' : file[at]);
    }
    mmgr_line_finish(&wanted_line);
    while ((file_size >= 3u) &&
           (memor.cmp(&(MemoriaCfg){.src = file + file_size - 3u, .other = ".py", .bytes = 3u}) == 0))
    {
        file_size -= 3u;
    }
    for (size_t at = 0u; at < file_size; at += 1u)
    {
        mmgr_line_ch(&module_line, ((file[at] == '/') || (file[at] == '\\')) ? '.' : file[at]);
    }
    mmgr_line_finish(&module_line);
    const char *class_name =
        ((classname != NULL) &&
         EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = classname, .other_pointer = module,
                    .size = cellul.len(&(CatenaFinitaCfg){.src = module, .cap = CLI_CELLS_POOL})))
            ? classname + cellul.len(&(CatenaFinitaCfg){.src = module, .cap = CLI_CELLS_POOL})
            : NULL;
    if (class_name != NULL)
    {
        class_name +=
            EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = class_name, .at = 0ull,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = class_name, .cap = CLI_CELLS_POOL}), .set = ".");
    }
    VerbaLinea id_line = {id, sizeof(id), 0ull};
    mmgr_line_put(&id_line, wanted);
    mmgr_line_put(&id_line, "::");
    const char *const class_part = (class_name != NULL) ? class_name : "";
    EMBED_CALL(cli_text_set_table.put_replace, CliTextSetCfg, .out = &id_line, .text = class_part, .set = ".",
               .with = "::");
    mmgr_line_put(&id_line, (class_part[0] != '\0') ? "::" : "");
    mmgr_line_put(&id_line, (name != NULL) ? name : "");
    mmgr_line_finish(&id_line);
    const char *const time = EMBED_CALL(report_attribute, TestReportsCtx, .tag_read = &s_case, .name = "time");
    char *ended = NULL;
    const double seconds = (time != NULL) ? strtod(time, &ended) : 0.0;
    long long own_line = -1ll;
    const char *const line_text = EMBED_CALL(report_attribute, TestReportsCtx, .tag_read = &s_case, .name = "line");
    if ((line_text == NULL) ||
        !EMBED_CALL(cli_test_runs_text_length_internal.report_count, CliTestRunsTextLengthCtx, .text = line_text,
                    .length = cellul.len(&(CatenaFinitaCfg){.src = line_text, .cap = CLI_CELLS_POOL}),
                    .value = &own_line))
    {
        own_line = -1ll;
    }
    const double taken = ((ended != NULL) && (*ended == '\0') && (ended != time)) ? seconds : 0.0;
    static const char *const s_tags[3] = {"<failure", "<error", "<skipped"};
    static const char *const s_outcomes[3] = {"failed", "failed", "skipped"};
    for (unsigned int tag = 0u; tag < 3u; tag += 1u)
    {
        const unsigned long long at =
            EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                       .text = args->body, .length = args->body_size, .needle = s_tags[tag]);
        if (at >= args->body_size)
        {
            continue;
        }
        const unsigned long long tag_end =
            at + EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                            .text = args->body + at, .length = args->body_size - at, .needle = ">");
        EMBED_CALL(report_attributes, TestReportsCtx, .text = args->body + at, .length = tag_end - at, .tag = &s_tag);
        const unsigned long long text_end =
            (tag_end < args->body_size)
                ? tag_end + EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find,
                                       CliTestRunsTextLengthNeedleCtx, .text = args->body + tag_end,
                                       .length = args->body_size - tag_end, .needle = "</")
                : args->body_size;
        const unsigned long long text_start = (tag_end + 1ull < text_end) ? tag_end + 1ull : text_end;
        // the text read takes no more bytes than it is written in, an entity read to one letter
        const unsigned long long text_room =
            ((text_end - text_start) < CLI_REPORT_TEXT) ? ((text_end - text_start) + 1ull) : CLI_REPORT_TEXT;
        char *const text_cell = g_cli_cells->temporary_buf_alloc(text_room, &g_cli_test_report_cells);
        char refused[1] = {'\0'};
        VerbaLinea text = (text_cell != NULL) ? (VerbaLinea){text_cell, text_room, 0ull}
                                              : (VerbaLinea){refused, sizeof(refused), 0ull};
        EMBED_CALL(report_unescaped, TestReportsCtx, .text = args->body + text_start, .length = text_end - text_start,
                   .out = &text);
        long long line = own_line;
        if (!EMBED_CALL(report_traceback_line, TestReportsCtx, .text = text.out, .size = text.at, .wanted = wanted,
                        .line = &line))
        {
            line = own_line;
        }
        const char *const message = EMBED_CALL(report_attribute, TestReportsCtx, .tag_read = &s_tag, .name = "message");
        unsigned long long last = 0ull;
        unsigned long long last_size = 0ull;
        const int given = (message != NULL) && (message[0] != '\0');
        const int found =
            !given && EMBED_CALL(cli_test_runs_text_internal.report_last_line, CliTestRunsTextCtx, .text = text.out,
                                 .size = text.at, .skip_rules = 0, .last = &last, .last_size = &last_size);
        EMBED_CALL(cli_test_runs_outcome_internal.test_result_emit, CliTestRunsOutcomeCtx, .worker = worker, .id = id,
                   .outcome = s_outcomes[tag], .message = given ? message : (found ? text.out + last : ""),
                   .message_size = given ? cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL})
                                         : (found ? last_size : 0ull),
                   .file = wanted, .line_i64 = line, .seconds = taken);
        g_cli_cells->persistent_buf_release(text_cell, &g_cli_test_report_cells);
        return;
    }
    EMBED_CALL(cli_test_runs_outcome_internal.test_result_emit, CliTestRunsOutcomeCtx, .worker = worker, .id = id,
               .outcome = "passed", .message = "", .message_size = 0ull, .file = wanted, .line_i64 = own_line,
               .seconds = taken);
}

void cli_test_report_emit(const CliTestRunsWorkerCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_report_lock);
    unsigned long long rest = 0ull;
    for (;;)
    {
        const unsigned long long start =
            rest + EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                              .text = args->xml + rest, .length = args->size - rest, .needle = "<testcase ");
        if (start >= args->size)
        {
            break;
        }
        const char *const at = args->xml + start;
        const unsigned long long length = args->size - start;
        const unsigned long long open_end =
            EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                       .text = at, .length = length, .needle = ">");
        const int lone = (open_end > 0ull) && (open_end <= length) && (at[open_end - 1ull] == '/');
        // a lone tag closes at its own end; any other where its closing tag ends, or at the text's end without one
        const unsigned long long closing =
            lone ? 0ull
                 : EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                              .text = at, .length = length, .needle = "</testcase>");
        const unsigned long long close = lone ? (open_end + 1ull) : ((closing < length) ? closing + 11ull : length);
        const unsigned long long body = (open_end < close) ? open_end : close;
        EMBED_CALL(report_case_emit, TestReportsCtx, .worker = args->worker, .open = at, .open_size = open_end,
                   .body = at + body, .body_size = close - body);
        rest = start + ((close < length) ? close : length);
    }
    g_cli_cells->persistent_buf_release(s_case.values, &g_cli_test_report_cells);
    g_cli_cells->persistent_buf_release(s_tag.values, &g_cli_test_report_cells);
    s_case = (CliReportTag){0};
    s_tag = (CliReportTag){0};
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_report_lock);
}
