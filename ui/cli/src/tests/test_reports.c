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
// a cell of `capacity` bytes taken larger as a longer tag is read and given back as the report is
typedef struct
{
    const char *name[CLI_REPORT_ATTRIBUTES];
    unsigned long long name_size[CLI_REPORT_ATTRIBUTES];
    unsigned long long value[CLI_REPORT_ATTRIBUTES];
    unsigned int count;
    char *values;
    unsigned long long capacity;
    unsigned long long used;
} CliReportTag;
#define CLI_REPORT_VALUES (1u << 18u)
#define CLI_REPORT_TEXT (1u << 22u)

static CliReportTag s_case;
static CliReportTag s_tag;

typedef struct
{
    const char *text;
    unsigned long long length;
    VerbaLinea *out;
    CliReportTag *tag;
    const CliReportTag *tag_read;
    const char *name;
    unsigned long long size;
    const char *required;
    long long *line;
    CliTestWorker *worker;
    const char *open;
    unsigned long long open_size;
    const char *body;
    unsigned long long body_size;
    const char *classname;
    char *required_out;
    char *id_out;
    const char *id;
    unsigned long long at;
    unsigned int kind;
    long long own_line;
    double seconds;
} TestReportsCtx;

// the tags a case's outcome other than a pass stands in, and the outcome each reports
static const char *const s_tags[3] = {"<failure", "<error", "<skipped"};
static const char *const s_outcomes[3] = {"failed", "failed", "skipped"};
// `length` bytes of an XML text with their entities read written into `out`
EMBED_INLINE void report_unescaped(const TestReportsCtx *args)
{
    static const char *const s_entities[7] = {"&#10;", "&#13;", "&quot;", "&apos;", "&lt;", "&gt;", "&amp;"};
    static const char s_letters[7] = {'\n', '\r', '"', '\'', '<', '>', '&'};
    unsigned long long at = 0ULL;
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
            at += 1ULL;
        }
    }
}

unsigned long long cli_report_find(const CliTestRunsTextLengthNeedleCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->needle, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ULL; at + size <= args->length; at += 1ULL)
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
    args->tag->used = 0ULL;
    // each value takes its letters, an entity read to one, and a terminator for the `="` and `"` about it, so the
    // values fit in the bytes of the tag's text and one more
    const unsigned long long capacity = (length < CLI_REPORT_VALUES) ? (length + 1ULL) : CLI_REPORT_VALUES;
    if (args->tag->capacity < capacity)
    {
        g_cli_cells->persistent_buf_release(args->tag->values, &g_cli_test_report_cells);
        args->tag->values = g_cli_cells->persistent_buf_alloc(capacity, &g_cli_test_report_cells);
        args->tag->capacity = (args->tag->values != NULL) ? capacity : 0ULL;
    }
    if (args->tag->values == NULL)
    {
        return;
    }
    unsigned long long at = 0ULL;
    for (;;)
    {
        const unsigned long long equal =
            at + EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                            .text = text + at, .length = length - at, .needle = "=\"");
        if (equal >= length)
        {
            return;
        }
        const unsigned long long from = equal + 2ULL;
        const unsigned long long size =
            EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                       .text = text + from, .length = length - from, .needle = "\"");
        if (from + size >= length)
        {
            return;
        }
        unsigned long long name = equal;
        while ((name > at) && (cellul.chr(&(CatenaFinitaCfg){
                                   .src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = text[name - 1ULL]}) == NULL))
        {
            name -= 1ULL;
        }
        VerbaLinea value = {args->tag->values + args->tag->used, args->tag->capacity - args->tag->used, 0ULL};
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
        if ((mmgr_line_finish(&value) != 0ULL) && (index < CLI_REPORT_ATTRIBUTES))
        {
            args->tag->name[index] = text + name;
            args->tag->name_size[index] = equal - name;
            args->tag->value[index] = args->tag->used;
            args->tag->used += value.at + 1ULL;
            args->tag->count += (index == args->tag->count) ? 1u : 0u;
        }
        at = from + size + 1ULL;
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
    unsigned long long start = 0ULL;
    while ((start < length) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = args->text[start]}) != NULL))
    {
        start += 1ULL;
    }
    while ((length > start) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = args->text[length - 1ULL]}) != NULL))
    {
        length -= 1ULL;
    }
    start += ((start < length) && (args->text[start] == '+')) ? 1ULL : 0ULL;
    unsigned long long sum = 0ULL;
    for (unsigned long long at = start; at < length; at += 1ULL)
    {
        if ((args->text[at] < '0') || (args->text[at] > '9') || (sum > 429496729ULL))
        {
            return 0;
        }
        sum = (sum * 10ULL) + (unsigned long long)(args->text[at] - '0');
    }
    *value = (long long)sum;
    return (start < length) && (sum <= 4294967295ULL);
}

int cli_report_ends_with(const CliTestRunsLengthPathCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->required, .cap = CLI_CELLS_POOL});
    if (args->length < size)
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < size; at += 1ULL)
    {
        const char letter = args->path[args->length - size + at];
        if (((letter == '\\') ? '/' : letter) != args->required[at])
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

// the line of `required` a traceback in pytest's form last names, `path:line: ...`, from 0, into `line`: 1, or 0 where
// it names none
EMBED_INLINE int report_traceback_line(const TestReportsCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    const char *required = args->required;
    long long *line = args->line;
    int found = 0;
    unsigned long long at = 0ULL;
    unsigned long long start = 0ULL;
    unsigned long long end = 0ULL;
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
        long long number = 0LL;
        if ((colon > start) &&
            EMBED_CALL(cli_test_runs_length_path_internal.report_ends_with, CliTestRunsLengthPathCtx,
                       .path = text + start, .length = colon - 1ULL - start, .required = required) &&
            EMBED_CALL(cli_test_runs_text_length_internal.report_count, CliTestRunsTextLengthCtx, .text = text + colon,
                       .length = place - colon, .value = &number))
        {
            *line = (number > 0LL) ? number - 1LL : 0LL;
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
    unsigned long long at = 0ULL;
    unsigned long long start = 0ULL;
    unsigned long long end = 0ULL;
    while (EMBED_CALL(cli_test_runs_text_size_internal.report_line, CliTestRunsTextSizeCtx, .text = args->text,
                      .size = args->size, .at = &at, .start = &start, .end = &end))
    {
        const int rule = args->skip_rules && (end - start >= 3ULL) &&
                         (memor.cmp(&(MemoriaCfg){.src = args->text + start, .other = "---", .bytes = 3u}) == 0);
        while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = args->text[start]}) != NULL))
        {
            start += 1ULL;
        }
        while ((end > start) &&
               (cellul.chr(&(CatenaFinitaCfg){
                    .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = args->text[end - 1ULL]}) != NULL))
        {
            end -= 1ULL;
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

// the test's file, `text`, with its separators as slashes into `required_out`, sized for CLI_PATH_MAX_LEN, and its id
// into `id_out`, sized for twice that: the file, its class past the module the file makes, each dot as `::`, where
// `classname` names one, and its `name`, `::` between them
EMBED_INLINE void report_case_named(const TestReportsCtx *args)
{
    const char *const file = args->text;
    const char *const name = args->name;
    const char *const classname = args->classname;
    char *const required = args->required_out;
    char module[CLI_PATH_MAX_LEN];
    VerbaLinea requested_line = {required, CLI_PATH_MAX_LEN, 0ULL};
    VerbaLinea module_line = {module, sizeof(module), 0ULL};
    size_t file_size = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL});
    for (size_t at = 0u; at < file_size; at += 1u)
    {
        mmgr_line_ch(&requested_line, (file[at] == '\\') ? '/' : file[at]);
    }
    mmgr_line_finish(&requested_line);
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
            EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = class_name, .at = 0ULL,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = class_name, .cap = CLI_CELLS_POOL}), .set = ".");
    }
    VerbaLinea id_line = {args->id_out, CLI_PATH_MAX_LEN * 2u, 0ULL};
    mmgr_line_put(&id_line, required);
    mmgr_line_put(&id_line, "::");
    const char *const class_part = (class_name != NULL) ? class_name : "";
    EMBED_CALL(cli_text_set_table.append_replace, CliTextSetCfg, .out = &id_line, .text = class_part, .set = ".",
               .with = "::");
    mmgr_line_put(&id_line, (class_part[0] != '\0') ? "::" : "");
    mmgr_line_put(&id_line, (name != NULL) ? name : "");
    mmgr_line_finish(&id_line);
}

// the outcome `kind` of s_outcomes reported by `worker` for the test `id`, its tag at `at` of the `body_size` bytes of
// `body`: the message the tag gives, else the last line of its text; the line the traceback gives in the test's file
// `required`, else `own_line`; and its `seconds`
EMBED_INLINE void report_outcome_emit(const TestReportsCtx *args)
{
    const unsigned long long at = args->at;
    const unsigned long long tag_end =
        at + EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                        .text = args->body + at, .length = args->body_size - at, .needle = ">");
    EMBED_CALL(report_attributes, TestReportsCtx, .text = args->body + at, .length = tag_end - at, .tag = &s_tag);
    const unsigned long long text_end =
        (tag_end < args->body_size) ? tag_end + EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find,
                                                           CliTestRunsTextLengthNeedleCtx, .text = args->body + tag_end,
                                                           .length = args->body_size - tag_end, .needle = "</")
                                    : args->body_size;
    const unsigned long long text_start = (tag_end + 1ULL < text_end) ? tag_end + 1ULL : text_end;
    // the text read takes no more bytes than it is written in, an entity read to one letter
    const unsigned long long text_capacity =
        ((text_end - text_start) < CLI_REPORT_TEXT) ? ((text_end - text_start) + 1ULL) : CLI_REPORT_TEXT;
    char *const text_cell = g_cli_cells->temporary_buf_alloc(text_capacity, &g_cli_test_report_cells);
    char refused[1] = {'\0'};
    VerbaLinea text = (text_cell != NULL) ? (VerbaLinea){text_cell, text_capacity, 0ULL}
                                          : (VerbaLinea){refused, sizeof(refused), 0ULL};
    EMBED_CALL(report_unescaped, TestReportsCtx, .text = args->body + text_start, .length = text_end - text_start,
               .out = &text);
    long long line = args->own_line;
    if (!EMBED_CALL(report_traceback_line, TestReportsCtx, .text = text.out, .size = text.at,
                    .required = args->required, .line = &line))
    {
        line = args->own_line;
    }
    const char *const message = EMBED_CALL(report_attribute, TestReportsCtx, .tag_read = &s_tag, .name = "message");
    unsigned long long last = 0ULL;
    unsigned long long last_size = 0ULL;
    const int supplied = (message != NULL) && (message[0] != '\0');
    const int found =
        !supplied && EMBED_CALL(cli_test_runs_text_internal.report_last_line, CliTestRunsTextCtx, .text = text.out,
                                .size = text.at, .skip_rules = 0, .last = &last, .last_size = &last_size);
    const char *const found_text = found ? text.out + last : "";
    const unsigned long long found_size = found ? last_size : 0ULL;
    EMBED_CALL(cli_test_runs_outcome_internal.test_result_emit, CliTestRunsOutcomeCtx, .worker = args->worker,
               .id = args->id, .outcome = s_outcomes[args->kind], .message = supplied ? message : found_text,
               .message_size =
                   supplied ? cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL}) : found_size,
               .file = args->required, .line_i64 = line, .seconds = args->seconds);
    g_cli_cells->persistent_buf_release(text_cell, &g_cli_test_report_cells);
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
    char required[CLI_PATH_MAX_LEN];
    char id[CLI_PATH_MAX_LEN * 2u];
    EMBED_CALL(report_case_named, TestReportsCtx, .text = file,
               .name = EMBED_CALL(report_attribute, TestReportsCtx, .tag_read = &s_case, .name = "name"),
               .classname = EMBED_CALL(report_attribute, TestReportsCtx, .tag_read = &s_case, .name = "classname"),
               .required_out = required, .id_out = id);
    const char *const time = EMBED_CALL(report_attribute, TestReportsCtx, .tag_read = &s_case, .name = "time");
    char *ended = NULL;
    const double seconds = (time != NULL) ? strtod(time, &ended) : 0.0;
    long long own_line = -1LL;
    const char *const line_text = EMBED_CALL(report_attribute, TestReportsCtx, .tag_read = &s_case, .name = "line");
    if ((line_text == NULL) ||
        !EMBED_CALL(cli_test_runs_text_length_internal.report_count, CliTestRunsTextLengthCtx, .text = line_text,
                    .length = cellul.len(&(CatenaFinitaCfg){.src = line_text, .cap = CLI_CELLS_POOL}),
                    .value = &own_line))
    {
        own_line = -1LL;
    }
    const double reserved = ((ended != NULL) && (*ended == '\0') && (ended != time)) ? seconds : 0.0;
    for (unsigned int tag = 0u; tag < 3u; tag += 1u)
    {
        const unsigned long long at =
            EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                       .text = args->body, .length = args->body_size, .needle = s_tags[tag]);
        if (at < args->body_size)
        {
            EMBED_CALL(report_outcome_emit, TestReportsCtx, .worker = worker, .id = id, .required = required,
                       .body = args->body, .body_size = args->body_size, .at = at, .kind = tag, .own_line = own_line,
                       .seconds = reserved);
            return;
        }
    }
    EMBED_CALL(cli_test_runs_outcome_internal.test_result_emit, CliTestRunsOutcomeCtx, .worker = worker, .id = id,
               .outcome = "passed", .message = "", .message_size = 0ULL, .file = required, .line_i64 = own_line,
               .seconds = reserved);
}

void cli_test_report_emit(const CliTestRunsWorkerCtx *args)
{
    unsigned long long rest = 0ULL;
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
        const int lone = (open_end > 0ULL) && (open_end <= length) && (at[open_end - 1ULL] == '/');
        // a lone tag closes at its own end; any other where its closing tag ends, or at the text's end without one
        const unsigned long long closing =
            lone ? 0ULL
                 : EMBED_CALL(cli_test_runs_text_length_needle_internal.report_find, CliTestRunsTextLengthNeedleCtx,
                              .text = at, .length = length, .needle = "</testcase>");
        unsigned long long close;
        if (lone)
        {
            close = open_end + 1ULL;
        }
        else if (closing < length)
        {
            close = closing + 11ULL;
        }
        else
        {
            close = length;
        }
        const unsigned long long body = (open_end < close) ? open_end : close;
        EMBED_CALL(report_case_emit, TestReportsCtx, .worker = args->worker, .open = at, .open_size = open_end,
                   .body = at + body, .body_size = close - body);
        rest = start + ((close < length) ? close : length);
    }
    g_cli_cells->persistent_buf_release(s_case.values, &g_cli_test_report_cells);
    g_cli_cells->persistent_buf_release(s_tag.values, &g_cli_test_report_cells);
    s_case = (CliReportTag){0};
    s_tag = (CliReportTag){0};
}
