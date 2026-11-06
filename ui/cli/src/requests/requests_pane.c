// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_pane.c: the Requests view of the Comms pane as the window reads it. The answers stored for the request the
// shown view's cursor stands in stand as one tree in a cell sized to them, read again only where the cursor moves into
// another request; the file's environments as another. The answer shown has its body laid out in a cell of its own
// where it is JSON, and two answers compared have their texts and their lines in one more. The rows stand in one cell
// with every text they compose: they are counted with their texts in a line that only counts, then laid once into a
// cell of that size, each text a pointer into one of those cells and a length, laid again only as the answers, the
// groups opened, the answer shown, the boxes checked or the comparison change. A send runs on a worker of its own,
// which hands the answers it read to the view's thread through one word and a wake, the step taking them in
#include "../command_line/command_line.h"
#include "../editor/document.h"
#include "../editor/view.h"
#include "../text/json_tree.h"
#include "embed_atomic.h"
#include "requests_internal.h"

// the bytes a counting line takes its texts in at once; the bytes of a request's key; the most of a body laid out;
// the most bytes of a text an answer is compared by; and the ms of a day, for an answer's time
#define REQUESTS_COUNTING 64u
#define REQUESTS_KEY (CLI_REQUESTS_ADDRESS + CLI_REQUESTS_NAME)
#define REQUESTS_BODY_MOST (1ULL << 20u)
#define REQUESTS_DAY_SECONDS 86400ULL

// A row laid: its kind, tone, group and answer, its text as a pointer where the text stands elsewhere, else as its
// offset into the rows' text, and its bytes; its sign, and whether it is open, checked, inert and the answer shown
typedef struct
{
    unsigned int kind;
    unsigned int tone;
    unsigned int group;
    unsigned int answer;
    const char *text;
    unsigned long long offset;
    unsigned long long length;
    char sign;
    int open;
    int checked;
    int inert;
    int shown;
} RequestsLaid;

// A line of a comparison: its sign, and its text as an offset into the comparison's texts and its bytes
typedef struct
{
    char sign;
    unsigned long long offset;
    unsigned long long length;
} RequestsCompared;

// The view: the file of requests followed with its path, the view's key and the line followed, and the key of the
// request shown with its bytes; the answers' tree and its array, how many answers; the environments' tree and its
// names, how many, the one chosen, 0 for none; the answer shown, a bit an answer checked, a byte a group open; the
// body shown laid out, its bytes and whether it was cut; the comparison's cell, its lines and texts and how many
// lines; the rows' cell, the rows and their text, how many rows; and the last send's refusal with its bytes
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    char view_key[CLI_PATH_MAX_LEN];
    unsigned long long view_line;
    char request[REQUESTS_KEY];
    unsigned long long request_length;
    int followed;
    CliJson answers;
    unsigned int answers_node;
    unsigned int answer_count;
    CliJson envs;
    unsigned int names_node;
    unsigned int env_count;
    unsigned int env_chosen;
    unsigned int shown;
    unsigned int checked;
    unsigned char open[CLI_REQUESTS_GROUPS];
    char *body;
    const char *body_text;
    unsigned long long body_length;
    int body_cut;
    unsigned char *compared_cell;
    RequestsCompared *compared;
    const char *compared_text;
    unsigned int compared_count;
    unsigned char *rows_cell;
    RequestsLaid *row;
    const char *text;
    unsigned int rows;
    char *refusal;
    unsigned long long refusal_length;
} RequestsPane;

static RequestsPane s_requests = {.open = {0u, 0u, 1u}};

// A send: its thread, the tree, the file and the request's line, the file's text taken on the view's thread and let
// go on the worker, and what it read: the answers' JSON with its bytes, or why it failed
typedef struct
{
    CliThread thread;
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char *text;
    unsigned long long size;
    unsigned long long line;
    char *answer;
    unsigned long long answer_length;
    char why[CLI_ERR_MSG_MAX_LEN];
    unsigned long long why_length;
} RequestsSend;

static RequestsSend s_send;
static embed_atomic_u32 s_running;
static embed_atomic_u32 s_arrived;
static void (*s_woken)(void);

typedef struct
{
    VerbaLinea *line;
    RequestsLaid *row;
    unsigned int *count;
    RequestsLaid laid;
    const char *text;
    unsigned long long length;
    unsigned int node;
    const char *name;
    unsigned long long value;
    const char *root;
    const char *path;
    const char *key;
    unsigned long long line_at;
    const CliJson *json;
    CliJson *tree;
    unsigned long long *length_out;
    unsigned long long *starts;
    VerbaLinea *err_msg;
    unsigned int at;
    CliRequestsPaneCfg *asked;
} RequestsPaneCtx;

// the changes a diff hands, put after one another into `hunk`, `count` of them so far
typedef struct
{
    CliEachHunkCfg *hunk;
    unsigned long long count;
} RequestsHunks;

// a counting line's bytes taken and let go, its flushed count the bytes of every text written to it
static int requests_counted(void *context, const char *bytes, size_t size)
{
    (void)context;
    (void)bytes;
    (void)size;
    return 1;
}

// the place the next text of `line` starts at: the bytes it handed on and those it has
EMBED_INLINE unsigned long long requests_place(const RequestsPaneCtx *args)
{
    return args->line->flushed + args->line->at;
}

// the row `laid` put after the rows so far where the rows are laid, counted either way
EMBED_INLINE void requests_laid_put(const RequestsPaneCtx *args)
{
    if (args->row != NULL)
    {
        args->row[*args->count] = args->laid;
    }
    *args->count += 1u;
}

// the member `name` of the object `node` of the tree `json`
EMBED_INLINE unsigned int requests_member(const RequestsPaneCtx *args)
{
    return EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                      .object = args->node, .name = args->name);
}

// the letters of the member `name` of `node`, NULL where it is none or no string, their bytes into `length_out`
EMBED_INLINE const char *requests_string(const RequestsPaneCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(requests_member, RequestsPaneCtx, .json = args->json, .node = args->node,
                                      .name = args->name),
                   .length_pointer = &length);
    *args->length_out = length;
    return text;
}

// the number the member `name` of `node` writes, in tenths, its fraction past the first digit rounded: 0 where it is
// none or no number
EMBED_INLINE unsigned long long requests_tenths(const RequestsPaneCtx *args)
{
    const unsigned int number =
        EMBED_CALL(requests_member, RequestsPaneCtx, .json = args->json, .node = args->node, .name = args->name);
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json, .node = number,
                    .kind = CFG_JSON_NUMBER))
    {
        return 0ULL;
    }
    const CliJsonNode *const one = &args->json->node[number];
    const char *const text = args->json->text + one->text;
    unsigned long long whole = 0ULL;
    unsigned long long at = 0ULL;
    while ((at < one->length) && (text[at] >= '0') && (text[at] <= '9'))
    {
        whole = (whole * 10ULL) + (unsigned long long)(text[at] - '0');
        at += 1ULL;
    }
    unsigned long long tenth = 0ULL;
    unsigned long long hundredth = 0ULL;
    if ((at < one->length) && (text[at] == '.'))
    {
        tenth = ((at + 1ULL < one->length) && (text[at + 1ULL] >= '0') && (text[at + 1ULL] <= '9'))
                    ? (unsigned long long)(text[at + 1ULL] - '0')
                    : 0ULL;
        hundredth = ((at + 2ULL < one->length) && (text[at + 2ULL] >= '0') && (text[at + 2ULL] <= '9'))
                        ? (unsigned long long)(text[at + 2ULL] - '0')
                        : 0ULL;
    }
    return (whole * 10ULL) + tenth + ((hundredth >= 5ULL) ? 1ULL : 0ULL);
}

// a time of `value` tenths of a millisecond written as the page writes one: a tenth shown below ten ms, whole above
EMBED_INLINE void requests_ms_put(const RequestsPaneCtx *args)
{
    if (args->value < 100ULL)
    {
        mmgr_line_decimal(args->line, args->value / 10ULL, 1u);
        mmgr_line_ch(args->line, '.');
        mmgr_line_decimal(args->line, args->value % 10ULL, 1u);
    }
    else
    {
        mmgr_line_decimal(args->line, (args->value + 5ULL) / 10ULL, 1u);
    }
    mmgr_line_put(args->line, " ms");
}

// `value` bytes written as the page writes a size: bytes below a KB, else KB or MB with a tenth
EMBED_INLINE void requests_bytes_put(const RequestsPaneCtx *args)
{
    if (args->value < 1024ULL)
    {
        mmgr_line_decimal(args->line, args->value, 1u);
        mmgr_line_put(args->line, " B");
        return;
    }
    const int mega = args->value >= (1ULL << 20u);
    const unsigned long long unit = mega ? (1ULL << 20u) : 1024ULL;
    const unsigned long long tenths = ((args->value * 10ULL) + (unit / 2ULL)) / unit;
    mmgr_line_decimal(args->line, tenths / 10ULL, 1u);
    mmgr_line_ch(args->line, '.');
    mmgr_line_decimal(args->line, tenths % 10ULL, 1u);
    mmgr_line_put(args->line, mega ? " MB" : " KB");
}

// the answer `at` of the answers, CLI_JSON_NONE where there is none
EMBED_INLINE unsigned int requests_answer(const RequestsPaneCtx *args)
{
    return (args->at < s_requests.answer_count)
               ? EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                            .json_read = &s_requests.answers, .array = s_requests.answers_node, .index = args->at)
               : CLI_JSON_NONE;
}

// the value of the header `name` among the pairs `node` of the answers' tree, matched without case: NULL where none
// is, its bytes into `out`'s place
EMBED_INLINE const char *requests_header(const RequestsPaneCtx *args)
{
    const CliJson *const json = &s_requests.answers;
    const unsigned long long wanted = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_NAME_MAX_LEN});
    for (unsigned int at = 0u;; at += 1u)
    {
        const unsigned int pair = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = args->node, .index = at);
        if (pair == CLI_JSON_NONE)
        {
            return NULL;
        }
        unsigned long long length = 0ULL;
        const char *const name =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = pair, .index = 0u),
                       .length_pointer = &length);
        int same = (name != NULL) && (length == wanted);
        for (unsigned long long letter = 0ULL; same && (letter < length); letter += 1ULL)
        {
            const char one = name[letter];
            const char two = args->name[letter];
            const char lower_one = ((one >= 'A') && (one <= 'Z')) ? (char)(one + ('a' - 'A')) : one;
            const char lower_two = ((two >= 'A') && (two <= 'Z')) ? (char)(two + ('a' - 'A')) : two;
            same = lower_one == lower_two;
        }
        if (same)
        {
            unsigned long long value_length = 0ULL;
            const char *const value =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = pair, .index = 1u),
                           .length_pointer = &value_length);
            *args->length_out = value_length;
            return value;
        }
    }
}

// the pairs of `node`, a header a pair, each laid as a row of `kind` written `name: value`
EMBED_INLINE void requests_pairs_laid(const RequestsPaneCtx *args)
{
    const CliJson *const json = &s_requests.answers;
    for (unsigned int at = 0u;; at += 1u)
    {
        const unsigned int pair = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = args->node, .index = at);
        if (pair == CLI_JSON_NONE)
        {
            return;
        }
        RequestsLaid laid = {.kind = args->laid.kind, .tone = CLI_REQUESTS_TONE_PLAIN};
        laid.offset = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line);
        for (unsigned int side = 0u; side < 2u; side += 1u)
        {
            unsigned long long length = 0ULL;
            const char *const text =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = pair, .index = side),
                           .length_pointer = &length);
            mmgr_line_put(args->line, (side != 0u) ? ": " : "");
            if (text != NULL)
            {
                (void)mmgr_line_put_n(args->line, text, length);
            }
        }
        laid.length = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line) - laid.offset;
        EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count, .laid = laid);
    }
}

// a row each line of `length` bytes of `text`, of the row `laid`'s kind, each a pointer into the text, its carriage
// return before the newline left out
EMBED_INLINE void requests_lines_laid(const RequestsPaneCtx *args)
{
    unsigned long long start = 0ULL;
    for (unsigned long long at = 0ULL; at <= args->length; at += 1ULL)
    {
        if ((at < args->length) && (args->text[at] != '\n'))
        {
            continue;
        }
        RequestsLaid laid = args->laid;
        laid.text = args->text + start;
        laid.length = at - start;
        laid.length -= ((laid.length != 0ULL) && (args->text[at - 1ULL] == '\r')) ? 1ULL : 0ULL;
        EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count, .laid = laid);
        start = at + 1ULL;
    }
}

// a group row of `laid`'s group written with the words `name`, and the count `value` in brackets where it is not 0
EMBED_INLINE void requests_group_laid(const RequestsPaneCtx *args)
{
    RequestsLaid laid = args->laid;
    laid.kind = CLI_REQUESTS_ROW_GROUP;
    laid.open = s_requests.open[laid.group];
    laid.offset = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line);
    mmgr_line_put(args->line, args->name);
    if (args->value != 0ULL)
    {
        mmgr_line_put(args->line, " (");
        mmgr_line_decimal(args->line, args->value, 1u);
        mmgr_line_ch(args->line, ')');
    }
    laid.length = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line) - laid.offset;
    EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count, .laid = laid);
}

// the answer shown laid: its status or why it failed, its times, its headers and what was sent, each a group, and its
// body or the row that reads why it is not drawn
EMBED_INLINE void requests_answer_laid(const RequestsPaneCtx *args)
{
    const CliJson *const json = &s_requests.answers;
    const unsigned int answer = args->node;
    unsigned long long length_of = 0ULL;
    const char *const failed = EMBED_CALL(requests_string, RequestsPaneCtx, .json = json, .node = answer,
                                          .name = "failed", .length_out = &length_of);
    const unsigned long long failed_length = length_of;
    RequestsLaid status = {.kind = CLI_REQUESTS_ROW_STATUS, .tone = CLI_REQUESTS_TONE_FAILED};
    if (failed != NULL)
    {
        status.text = failed;
        status.length = failed_length;
        EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count, .laid = status);
    }
    else
    {
        const unsigned long long code =
            EMBED_CALL(requests_tenths, RequestsPaneCtx, .json = json, .node = answer, .name = "status") / 10ULL;
        status.tone = (code >= 400ULL)   ? CLI_REQUESTS_TONE_FAILED
                      : (code >= 300ULL) ? CLI_REQUESTS_TONE_MOVED
                                         : CLI_REQUESTS_TONE_GOOD;
        status.offset = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line);
        mmgr_line_decimal(args->line, code, 1u);
        const char *const reason = EMBED_CALL(requests_string, RequestsPaneCtx, .json = json, .node = answer,
                                              .name = "reason", .length_out = &length_of);
        mmgr_line_ch(args->line, ' ');
        if (reason != NULL)
        {
            (void)mmgr_line_put_n(args->line, reason, length_of);
        }
        mmgr_line_put(args->line, "  ");
        const unsigned int times =
            EMBED_CALL(requests_member, RequestsPaneCtx, .json = json, .node = answer, .name = "times");
        EMBED_CALL(requests_ms_put, RequestsPaneCtx, .line = args->line,
                   .value = EMBED_CALL(requests_tenths, RequestsPaneCtx, .json = json, .node = times, .name = "total"));
        mmgr_line_put(args->line, ", ");
        EMBED_CALL(requests_bytes_put, RequestsPaneCtx, .line = args->line,
                   .value = EMBED_CALL(requests_tenths, RequestsPaneCtx, .json = json, .node = answer, .name = "size") /
                            10ULL);
        const unsigned int redirects =
            EMBED_CALL(requests_member, RequestsPaneCtx, .json = json, .node = answer, .name = "redirects");
        const unsigned int moves = (redirects != CLI_JSON_NONE) ? json->node[redirects].count : 0u;
        if (moves != 0u)
        {
            mmgr_line_put(args->line, ", after ");
            mmgr_line_decimal(args->line, moves, 1u);
            mmgr_line_put(args->line, (moves == 1u) ? " redirect" : " redirects");
        }
        status.length = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line) - status.offset;
        EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count, .laid = status);
        // the times of the parts that took any
        static const char *const s_parts[5] = {"resolve", "connect", "tls", "wait", "read"};
        static const char *const s_words[5] = {"resolve ", "connect ", "TLS ", "wait ", "read "};
        RequestsLaid timed = {.kind = CLI_REQUESTS_ROW_TIMES, .tone = CLI_REQUESTS_TONE_QUIET};
        timed.offset = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line);
        unsigned int written = 0u;
        for (unsigned int part = 0u; part < 5u; part += 1u)
        {
            const unsigned long long tenths =
                EMBED_CALL(requests_tenths, RequestsPaneCtx, .json = json, .node = times, .name = s_parts[part]);
            if (tenths == 0ULL)
            {
                continue;
            }
            mmgr_line_put(args->line, (written != 0u) ? ", " : "");
            mmgr_line_put(args->line, s_words[part]);
            EMBED_CALL(requests_ms_put, RequestsPaneCtx, .line = args->line, .value = tenths);
            written += 1u;
        }
        timed.length = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line) - timed.offset;
        EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count, .laid = timed);
        const unsigned int headers =
            EMBED_CALL(requests_member, RequestsPaneCtx, .json = json, .node = answer, .name = "headers");
        EMBED_CALL(requests_group_laid, RequestsPaneCtx, .line = args->line, .row = args->row, .count = args->count,
                   .laid = (RequestsLaid){.group = CLI_REQUESTS_GROUP_HEADERS}, .name = "Headers",
                   .value = (headers != CLI_JSON_NONE) ? json->node[headers].count : 0u);
        if (s_requests.open[CLI_REQUESTS_GROUP_HEADERS] && (headers != CLI_JSON_NONE))
        {
            EMBED_CALL(requests_pairs_laid, RequestsPaneCtx, .line = args->line, .row = args->row, .count = args->count,
                       .node = headers, .laid = (RequestsLaid){.kind = CLI_REQUESTS_ROW_HEADER});
        }
    }
    // what was sent: its request line, its headers, and its body after a blank row
    const unsigned int sent =
        EMBED_CALL(requests_member, RequestsPaneCtx, .json = json, .node = answer, .name = "sent");
    EMBED_CALL(requests_group_laid, RequestsPaneCtx, .line = args->line, .row = args->row, .count = args->count,
               .laid = (RequestsLaid){.group = CLI_REQUESTS_GROUP_SENT}, .name = "Sent", .value = 0ULL);
    if (s_requests.open[CLI_REQUESTS_GROUP_SENT] && (sent != CLI_JSON_NONE))
    {
        RequestsLaid request_line = {.kind = CLI_REQUESTS_ROW_SENT};
        request_line.offset = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line);
        const char *const method = EMBED_CALL(requests_string, RequestsPaneCtx, .json = json, .node = sent,
                                              .name = "method", .length_out = &length_of);
        if (method != NULL)
        {
            (void)mmgr_line_put_n(args->line, method, length_of);
        }
        mmgr_line_ch(args->line, ' ');
        const char *const url = EMBED_CALL(requests_string, RequestsPaneCtx, .json = json, .node = sent, .name = "url",
                                           .length_out = &length_of);
        if (url != NULL)
        {
            (void)mmgr_line_put_n(args->line, url, length_of);
        }
        request_line.length = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line) - request_line.offset;
        EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count, .laid = request_line);
        EMBED_CALL(requests_pairs_laid, RequestsPaneCtx, .line = args->line, .row = args->row, .count = args->count,
                   .node = EMBED_CALL(requests_member, RequestsPaneCtx, .json = json, .node = sent, .name = "headers"),
                   .laid = (RequestsLaid){.kind = CLI_REQUESTS_ROW_SENT});
        const char *const body = EMBED_CALL(requests_string, RequestsPaneCtx, .json = json, .node = sent,
                                            .name = "body", .length_out = &length_of);
        if ((body != NULL) && (length_of != 0u))
        {
            EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count,
                       .laid = (RequestsLaid){.kind = CLI_REQUESTS_ROW_SENT, .text = ""});
            EMBED_CALL(requests_lines_laid, RequestsPaneCtx, .row = args->row, .count = args->count, .text = body,
                       .length = length_of, .laid = (RequestsLaid){.kind = CLI_REQUESTS_ROW_SENT});
        }
    }
    if (failed != NULL)
    {
        return;
    }
    // the body: a row a line of it as it is laid out, or the row that reads it is not text, and the row that reads it
    // was cut
    const int binary =
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                   .node = EMBED_CALL(requests_member, RequestsPaneCtx, .json = json, .node = answer, .name = "binary"),
                   .kind = CFG_JSON_TRUE);
    if (binary)
    {
        RequestsLaid note = {.kind = CLI_REQUESTS_ROW_NOTE, .tone = CLI_REQUESTS_TONE_QUIET};
        note.offset = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line);
        EMBED_CALL(requests_bytes_put, RequestsPaneCtx, .line = args->line,
                   .value = EMBED_CALL(requests_tenths, RequestsPaneCtx, .json = json, .node = answer, .name = "size") /
                            10ULL);
        mmgr_line_put(args->line, " of ");
        const char *const type = EMBED_CALL(
            requests_header, RequestsPaneCtx,
            .node = EMBED_CALL(requests_member, RequestsPaneCtx, .json = json, .node = answer, .name = "headers"),
            .name = "content-type", .length_out = &length_of);
        if ((type != NULL) && (length_of != 0u))
        {
            (void)mmgr_line_put_n(args->line, type, length_of);
        }
        else
        {
            mmgr_line_put(args->line, "bytes");
        }
        mmgr_line_put(args->line, ", not text.");
        note.length = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line) - note.offset;
        EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count, .laid = note);
        return;
    }
    if (s_requests.body_text != NULL)
    {
        EMBED_CALL(requests_lines_laid, RequestsPaneCtx, .row = args->row, .count = args->count,
                   .text = s_requests.body_text, .length = s_requests.body_length,
                   .laid = (RequestsLaid){.kind = CLI_REQUESTS_ROW_BODY});
    }
    if (s_requests.body_cut)
    {
        RequestsLaid note = {.kind = CLI_REQUESTS_ROW_NOTE, .tone = CLI_REQUESTS_TONE_QUIET};
        note.offset = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line);
        mmgr_line_put(args->line, "Only the first ");
        EMBED_CALL(requests_bytes_put, RequestsPaneCtx, .line = args->line, .value = s_requests.body_length);
        mmgr_line_put(args->line, " of the body is shown.");
        note.length = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line) - note.offset;
        EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count, .laid = note);
    }
}

// how many answers are checked
EMBED_INLINE unsigned int requests_checked_count(const RequestsPaneCtx *args)
{
    (void)args;
    unsigned int count = 0u;
    for (unsigned int bits = s_requests.checked; bits != 0u; bits &= bits - 1u)
    {
        count += 1u;
    }
    return count;
}

// every row of the view laid, into `row` where it is not NULL, its composed texts written into `line`
EMBED_INLINE void requests_rows_written(const RequestsPaneCtx *args)
{
    if (s_requests.refusal != NULL)
    {
        EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count,
                   .laid = (RequestsLaid){.kind = CLI_REQUESTS_ROW_REFUSAL,
                                          .tone = CLI_REQUESTS_TONE_FAILED,
                                          .text = s_requests.refusal,
                                          .length = s_requests.refusal_length});
    }
    const unsigned int answer = EMBED_CALL(requests_answer, RequestsPaneCtx, .at = s_requests.shown);
    if (answer == CLI_JSON_NONE)
    {
        static const char s_none[] = "No answer yet. Ctrl+Enter sends the request the cursor stands in.";
        EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count,
                   .laid = (RequestsLaid){.kind = CLI_REQUESTS_ROW_NOTE,
                                          .tone = CLI_REQUESTS_TONE_QUIET,
                                          .text = s_none,
                                          .length = sizeof(s_none) - 1ULL});
        return;
    }
    EMBED_CALL(requests_answer_laid, RequestsPaneCtx, .line = args->line, .row = args->row, .count = args->count,
               .node = answer);
    if (s_requests.answer_count < 2u)
    {
        return;
    }
    EMBED_CALL(requests_group_laid, RequestsPaneCtx, .line = args->line, .row = args->row, .count = args->count,
               .laid = (RequestsLaid){.group = CLI_REQUESTS_GROUP_ANSWERS}, .name = "Last answers", .value = 0ULL);
    if (!s_requests.open[CLI_REQUESTS_GROUP_ANSWERS])
    {
        return;
    }
    const CliJson *const json = &s_requests.answers;
    for (unsigned int at = 0u; (at < s_requests.answer_count) && (at < CLI_REQUESTS_ANSWERS_MAX); at += 1u)
    {
        const unsigned int one = EMBED_CALL(requests_answer, RequestsPaneCtx, .at = at);
        RequestsLaid laid = {.kind = CLI_REQUESTS_ROW_ANSWER,
                             .answer = at,
                             .checked = (s_requests.checked & (1u << at)) != 0u,
                             .shown = at == s_requests.shown};
        laid.offset = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line);
        // the time of the day the answer came at, in the universal time
        const unsigned long long seconds =
            (EMBED_CALL(requests_tenths, RequestsPaneCtx, .json = json, .node = one, .name = "at") / 10000ULL) %
            REQUESTS_DAY_SECONDS;
        mmgr_line_decimal(args->line, seconds / 3600ULL, 2u);
        mmgr_line_ch(args->line, ':');
        mmgr_line_decimal(args->line, (seconds / 60ULL) % 60ULL, 2u);
        mmgr_line_ch(args->line, ':');
        mmgr_line_decimal(args->line, seconds % 60ULL, 2u);
        mmgr_line_put(args->line, "  ");
        unsigned long long length_of = 0ULL;
        if (EMBED_CALL(requests_string, RequestsPaneCtx, .json = json, .node = one, .name = "failed",
                       .length_out = &length_of) != NULL)
        {
            mmgr_line_put(args->line, "failed");
        }
        else
        {
            mmgr_line_decimal(
                args->line,
                EMBED_CALL(requests_tenths, RequestsPaneCtx, .json = json, .node = one, .name = "status") / 10ULL, 1u);
        }
        mmgr_line_put(args->line, "  ");
        EMBED_CALL(requests_ms_put, RequestsPaneCtx, .line = args->line,
                   .value = EMBED_CALL(
                       requests_tenths, RequestsPaneCtx, .json = json,
                       .node = EMBED_CALL(requests_member, RequestsPaneCtx, .json = json, .node = one, .name = "times"),
                       .name = "total"));
        mmgr_line_put(args->line, "  ");
        EMBED_CALL(requests_bytes_put, RequestsPaneCtx, .line = args->line,
                   .value =
                       EMBED_CALL(requests_tenths, RequestsPaneCtx, .json = json, .node = one, .name = "size") / 10ULL);
        laid.length = EMBED_CALL(requests_place, RequestsPaneCtx, .line = args->line) - laid.offset;
        EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count, .laid = laid);
    }
    static const char s_compare[] = "Compare";
    EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count,
               .laid = (RequestsLaid){.kind = CLI_REQUESTS_ROW_COMPARE,
                                      .text = s_compare,
                                      .length = sizeof(s_compare) - 1ULL,
                                      .inert = EMBED_CALL(requests_checked_count, RequestsPaneCtx, .at = 0u) != 2u});
    for (unsigned int at = 0u; at < s_requests.compared_count; at += 1u)
    {
        const RequestsCompared *const compared = &s_requests.compared[at];
        EMBED_CALL(requests_laid_put, RequestsPaneCtx, .row = args->row, .count = args->count,
                   .laid = (RequestsLaid){.kind = CLI_REQUESTS_ROW_SIGN,
                                          .tone = (compared->sign == '-')   ? CLI_REQUESTS_TONE_FAILED
                                                  : (compared->sign == '+') ? CLI_REQUESTS_TONE_GOOD
                                                                            : CLI_REQUESTS_TONE_PLAIN,
                                          .sign = compared->sign,
                                          .text = s_requests.compared_text + compared->offset,
                                          .length = compared->length});
    }
}

// the rows laid again into a cell sized to them and their texts, the cell they had released; the rows as they were
// where the cells cannot meet them
EMBED_INLINE void requests_rows_laid(const RequestsPaneCtx *args)
{
    (void)args;
    char counting_bytes[REQUESTS_COUNTING];
    VerbaLinea counting = {counting_bytes, sizeof(counting_bytes), 0ULL, requests_counted, NULL, 0ULL};
    unsigned int count = 0u;
    EMBED_CALL(requests_rows_written, RequestsPaneCtx, .line = &counting, .count = &count);
    const unsigned long long text_bytes = counting.flushed + counting.at;
    const unsigned long long row_bytes = (unsigned long long)count * sizeof(RequestsLaid);
    unsigned char *const cell = g_cli_guard->persistent_buf_alloc(row_bytes + text_bytes + 1ULL, g_cli_requests_owner);
    if (cell == NULL)
    {
        return;
    }
    RequestsLaid *const row = (RequestsLaid *)(void *)cell;
    char *const text = (char *)(cell + row_bytes);
    VerbaLinea text_line = {text, text_bytes + 1ULL, 0ULL};
    unsigned int laid = 0u;
    // the rows are laid again from what they were counted from, so no more of them come than were counted
    EMBED_CALL(requests_rows_written, RequestsPaneCtx, .line = &text_line, .row = row, .count = &laid);
    (void)mmgr_line_finish(&text_line);
    g_cli_guard->persistent_buf_release(s_requests.rows_cell, g_cli_requests_owner);
    s_requests.rows_cell = cell;
    s_requests.row = row;
    s_requests.text = text;
    s_requests.rows = laid;
}

// the body of the answer shown laid out: JSON as the page lays it, two spaces a level, in a cell of its own sized to
// it; any other text read in place in the answers' tree; the first REQUESTS_BODY_MOST bytes of it shown, cut past them
EMBED_INLINE void requests_body_laid(const RequestsPaneCtx *args)
{
    (void)args;
    g_cli_guard->persistent_buf_release(s_requests.body, g_cli_requests_owner);
    s_requests.body = NULL;
    s_requests.body_text = NULL;
    s_requests.body_length = 0ULL;
    s_requests.body_cut = 0;
    const unsigned int answer = EMBED_CALL(requests_answer, RequestsPaneCtx, .at = s_requests.shown);
    if (answer == CLI_JSON_NONE)
    {
        return;
    }
    CliJson *const json = &s_requests.answers;
    unsigned long long length_of = 0ULL;
    const char *const body = EMBED_CALL(requests_string, RequestsPaneCtx, .json = json, .node = answer, .name = "body",
                                        .length_out = &length_of);
    const unsigned long long length = length_of;
    if (body == NULL)
    {
        return;
    }
    const char *const type = EMBED_CALL(
        requests_header, RequestsPaneCtx,
        .node = EMBED_CALL(requests_member, RequestsPaneCtx, .json = json, .node = answer, .name = "headers"),
        .name = "content-type", .length_out = &length_of);
    const unsigned long long type_length = (type != NULL) ? length_of : 0ULL;
    int is_json =
        (type != NULL) &&
        (cellul.find(&(CatenaFinitaCfg){.src = type, .cap = type_length, .other = "json", .other_cap = 4ULL}) != NULL);
    unsigned long long first = 0ULL;
    while ((first < length) &&
           ((body[first] == ' ') || (body[first] == '\t') || (body[first] == '\r') || (body[first] == '\n')))
    {
        first += 1ULL;
    }
    is_json = is_json || ((first < length) && ((body[first] == '{') || (body[first] == '[')));
    s_requests.body_text = body;
    s_requests.body_length = length;
    s_requests.body_cut =
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                   .node = EMBED_CALL(requests_member, RequestsPaneCtx, .json = json, .node = answer, .name = "cut"),
                   .kind = CFG_JSON_TRUE);
    if (is_json)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        CliJson laid_out = {0};
        const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary,
                                             CliJsonTreeTextSizeErrMsgCfg, .json = &laid_out, .text = body,
                                             .size = length, .err_msg = &err_msg, .owner = g_cli_requests_owner);
        if (root != CLI_JSON_NONE)
        {
            const unsigned long long capacity =
                EMBED_CALL(cli_json_tree_node_out.json_pretty_capacity, CliJsonTreeNodeOutCfg, .json_read = &laid_out,
                           .node = root, .indent = 2u);
            char *const cell = g_cli_guard->persistent_buf_alloc(capacity + 1ULL, g_cli_requests_owner);
            if (cell != NULL)
            {
                VerbaLinea pretty = {cell, capacity + 1ULL, 0ULL};
                EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &laid_out,
                           .node = root, .out = &pretty, .indent = 2u);
                s_requests.body = cell;
                s_requests.body_text = cell;
                s_requests.body_length = mmgr_line_ok(&pretty) ? pretty.at : 0ULL;
            }
        }
        g_cli_guard->persistent_buf_release(laid_out.node, g_cli_requests_owner);
    }
    if (s_requests.body_length > REQUESTS_BODY_MOST)
    {
        s_requests.body_length = REQUESTS_BODY_MOST;
        s_requests.body_cut = 1;
    }
}

// the comparison let go, none standing
EMBED_INLINE void requests_compared_released(const RequestsPaneCtx *args)
{
    (void)args;
    g_cli_guard->persistent_buf_release(s_requests.compared_cell, g_cli_requests_owner);
    s_requests.compared_cell = NULL;
    s_requests.compared = NULL;
    s_requests.compared_text = NULL;
    s_requests.compared_count = 0u;
}

// `size` bytes of JSON read into the tree `json` in a cell of its own, the cell it had released: its root, or
// CLI_JSON_NONE with the tree as it was
EMBED_INLINE unsigned int requests_tree_read(const RequestsPaneCtx *args)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliJson read = {0};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &read,
                   .text = args->text, .size = args->length, .err_msg = &err_msg, .owner = g_cli_requests_owner);
    if (root == CLI_JSON_NONE)
    {
        g_cli_guard->persistent_buf_release(read.node, g_cli_requests_owner);
        return CLI_JSON_NONE;
    }
    g_cli_guard->persistent_buf_release(args->tree->node, g_cli_requests_owner);
    *args->tree = read;
    return root;
}

// the answers' JSON of `length` bytes at `text` taken in as the answers shown: their tree read, the first shown, none
// checked, no comparison, the body laid out and the rows laid
EMBED_INLINE void requests_answers_taken(const RequestsPaneCtx *args)
{
    if (args->length == 0ULL)
    {
        // no answers read: the tree let go, none shown
        g_cli_guard->persistent_buf_release(s_requests.answers.node, g_cli_requests_owner);
        s_requests.answers = (CliJson){0};
        s_requests.answers_node = CLI_JSON_NONE;
        s_requests.answer_count = 0u;
        s_requests.request_length = 0ULL;
    }
    const unsigned int root = (args->length != 0ULL)
                                  ? EMBED_CALL(requests_tree_read, RequestsPaneCtx, .tree = &s_requests.answers,
                                               .text = args->text, .length = args->length)
                                  : CLI_JSON_NONE;
    if (root != CLI_JSON_NONE)
    {
        const CliJson *const json = &s_requests.answers;
        unsigned long long length_of = 0ULL;
        const char *const key = EMBED_CALL(requests_string, RequestsPaneCtx, .json = json, .node = root, .name = "key",
                                           .length_out = &length_of);
        s_requests.request_length = ((key != NULL) && (length_of < REQUESTS_KEY)) ? length_of : 0ULL;
        if (s_requests.request_length != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = s_requests.request, .src = key, .bytes = s_requests.request_length});
        }
        s_requests.answers_node =
            EMBED_CALL(requests_member, RequestsPaneCtx, .json = json, .node = root, .name = "answers");
        s_requests.answer_count =
            (s_requests.answers_node != CLI_JSON_NONE) ? json->node[s_requests.answers_node].count : 0u;
    }
    s_requests.shown = 0u;
    s_requests.checked = 0u;
    EMBED_CALL(requests_compared_released, RequestsPaneCtx, .at = 0u);
    EMBED_CALL(requests_body_laid, RequestsPaneCtx, .at = 0u);
    EMBED_CALL(requests_rows_laid, RequestsPaneCtx, .at = 0u);
}

// what a call writes into a line read whole: the call run once into a line that only counts, a cell taken of that
// size, and the call run again into it: its bytes into `out`'s place, the cell, or NULL where the call failed or the
// cells cannot meet it. `at` names the call: the answers, 0, or the environments, 1
EMBED_INLINE char *requests_written(const RequestsPaneCtx *args)
{
    char counting_bytes[REQUESTS_COUNTING];
    VerbaLinea counting = {counting_bytes, sizeof(counting_bytes), 0ULL, requests_counted, NULL, 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const CliRequestsTextCfg asked = {.text = args->text,
                                      .size = args->length,
                                      .out = &counting,
                                      .err_msg = &err_msg,
                                      .line = args->line_at,
                                      .root = args->root,
                                      .path = args->path};
    const int counted =
        (args->at == 0u) ? cli_requests_text_table.answers(&asked) : cli_requests_text_table.environments(&asked);
    if (!counted)
    {
        if (args->err_msg != NULL)
        {
            (void)mmgr_line_put_n(args->err_msg, why, err_msg.at);
        }
        return NULL;
    }
    const unsigned long long bytes = counting.flushed + counting.at;
    char *const cell = g_cli_guard->persistent_buf_alloc(bytes + 1ULL, g_cli_requests_owner);
    if (cell == NULL)
    {
        return NULL;
    }
    VerbaLinea written = {cell, bytes + 1ULL, 0ULL};
    CliRequestsTextCfg again = asked;
    again.out = &written;
    err_msg.at = 0ULL;
    const int wrote =
        (args->at == 0u) ? cli_requests_text_table.answers(&again) : cli_requests_text_table.environments(&again);
    if (!wrote || !mmgr_line_ok(&written))
    {
        g_cli_guard->persistent_buf_release(cell, g_cli_requests_owner);
        return NULL;
    }
    *args->length_out = written.at;
    return cell;
}

// the environments of the file at `path` read into their tree, the one chosen standing where the file is the same
EMBED_INLINE void requests_envs_read(const RequestsPaneCtx *args)
{
    unsigned long long length_of = 0ULL;
    char *const written = EMBED_CALL(requests_written, RequestsPaneCtx, .root = args->root, .path = args->path,
                                     .at = 1u, .length_out = &length_of);
    const unsigned int root = (written != NULL)
                                  ? EMBED_CALL(requests_tree_read, RequestsPaneCtx, .tree = &s_requests.envs,
                                               .text = written, .length = length_of)
                                  : CLI_JSON_NONE;
    g_cli_guard->persistent_buf_release(written, g_cli_requests_owner);
    s_requests.names_node =
        (root != CLI_JSON_NONE)
            ? EMBED_CALL(requests_member, RequestsPaneCtx, .json = &s_requests.envs, .node = root, .name = "names")
            : CLI_JSON_NONE;
    s_requests.env_count =
        (s_requests.names_node != CLI_JSON_NONE) ? s_requests.envs.node[s_requests.names_node].count : 0u;
    s_requests.env_chosen = (s_requests.env_chosen <= s_requests.env_count) ? s_requests.env_chosen : 0u;
}

// the name of the environment chosen, NULL for none
EMBED_INLINE const char *requests_env_name(const RequestsPaneCtx *args)
{
    (void)args;
    return (s_requests.env_chosen != 0u)
               ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                            .json_read = &s_requests.envs,
                            .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = &s_requests.envs, .array = s_requests.names_node,
                                               .index = s_requests.env_chosen - 1u),
                            .length_pointer = NULL)
               : NULL;
}

// a send's worker: the answers stored for its request read into a cell of their own, or why not, then handed to the
// view's thread through the arrived word, and the view's thread woken; the file's text it was handed let go
static void requests_send_run(void *context)
{
    RequestsSend *const send = context;
    unsigned long long length_of = 0ULL;
    VerbaLinea why = {send->why, sizeof(send->why), 0ULL};
    send->answer =
        EMBED_CALL(requests_written, RequestsPaneCtx, .root = send->root, .path = send->path, .text = send->text,
                   .length = send->size, .line_at = send->line, .at = 0u, .length_out = &length_of, .err_msg = &why);
    send->answer_length = (send->answer != NULL) ? length_of : 0ULL;
    send->why_length = (send->answer == NULL) ? why.at : 0ULL;
    g_cli_guard->persistent_buf_release(send->text, g_cli_requests_owner);
    send->text = NULL;
    EMBED_ATOMIC_STORE(&s_arrived, 1u);
    if (s_woken != NULL)
    {
        s_woken();
    }
}

// the text stored under `key` read whole, its bytes into `out`'s place: the cell, NULL where none is stored
EMBED_INLINE char *requests_text_alloc(const RequestsPaneCtx *args)
{
    unsigned long long size = 0ULL;
    char *const text = (args->key != NULL)
                           ? EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = args->key,
                                        .size_pointer = &size, .owner = g_cli_requests_owner)
                           : NULL;
    *args->length_out = size;
    return text;
}

// where `asked` names no view, the shown view's key, its path and its cursor's line put in it, and the tree's root
// where it has none, as the head's Send and Copy as curl ask for the request the cursor stands in
EMBED_INLINE void requests_view_taken(const RequestsPaneCtx *args)
{
    CliRequestsPaneCfg *const asked = args->asked;
    asked->root = (asked->root != NULL) ? asked->root : cli_command_line_root();
    const int shown = cli_view_shown();
    if ((asked->key != NULL) || (shown < 0))
    {
        return;
    }
    // the view shown is among the views
    asked->key = cli_view_key((unsigned int)shown);
    asked->path = cli_view_path_at((unsigned int)shown);
    CliPosition cursor = {0ULL, 0ULL};
    (void)EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = asked->key, .cursor = &cursor);
    asked->line = cursor.line;
}

void cli_requests_pane_begin(const CliRequestsPaneCfg *args)
{
    s_woken = args->woken;
}

int cli_requests_pane_send(const CliRequestsPaneCfg *args)
{
    CliRequestsPaneCfg asked = *args;
    EMBED_CALL(requests_view_taken, RequestsPaneCtx, .asked = &asked);
    args = &asked;
    if ((args->root == NULL) || (args->path == NULL) || (EMBED_ATOMIC_LOAD(&s_running) != 0u))
    {
        return 0;
    }
    unsigned long long length_of = 0ULL;
    char *const text = EMBED_CALL(requests_text_alloc, RequestsPaneCtx, .key = args->key, .length_out = &length_of);
    if (text == NULL)
    {
        return 0;
    }
    const unsigned long long root_length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_PATH_MAX_LEN});
    const unsigned long long path_length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN});
    if ((root_length >= CLI_PATH_MAX_LEN) || (path_length >= CLI_PATH_MAX_LEN))
    {
        g_cli_guard->persistent_buf_release(text, g_cli_requests_owner);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_send.root, .src = args->root, .bytes = root_length + 1ULL});
    memor.cpy(&(MemoriaCfg){.dst = s_send.path, .src = args->path, .bytes = path_length + 1ULL});
    s_send.text = text;
    s_send.size = length_of;
    s_send.line = args->line;
    s_send.answer = NULL;
    s_send.answer_length = 0ULL;
    s_send.why_length = 0ULL;
    s_send.thread = (CliThread){requests_send_run, &s_send, 0u};
    EMBED_ATOMIC_STORE(&s_arrived, 0u);
    EMBED_ATOMIC_STORE(&s_running, 1u);
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_send.thread))
    {
        EMBED_ATOMIC_STORE(&s_running, 0u);
        g_cli_guard->persistent_buf_release(s_send.text, g_cli_requests_owner);
        s_send.text = NULL;
        return 0;
    }
    EMBED_CALL(requests_rows_laid, RequestsPaneCtx, .at = 0u);
    return 1;
}

int cli_requests_pane_sending(const CliRequestsPaneCfg *args)
{
    (void)args;
    return EMBED_ATOMIC_LOAD(&s_running) != 0u;
}

int cli_requests_pane_step(const CliRequestsPaneCfg *args)
{
    int changed = 0;
    if (EMBED_ATOMIC_EXCHANGE_32(&s_arrived, 0u) != 0u)
    {
        // the worker has let go of the send: its answers and its refusal are the view's now
        g_cli_guard->persistent_buf_release(s_requests.refusal, g_cli_requests_owner);
        s_requests.refusal = NULL;
        s_requests.refusal_length = 0ULL;
        if ((s_send.answer == NULL) && (s_send.why_length != 0ULL))
        {
            s_requests.refusal = g_cli_guard->persistent_buf_alloc(s_send.why_length + 1ULL, g_cli_requests_owner);
            if (s_requests.refusal != NULL)
            {
                memor.cpy(&(MemoriaCfg){.dst = s_requests.refusal, .src = s_send.why, .bytes = s_send.why_length});
                s_requests.refusal[s_send.why_length] = '\0';
                s_requests.refusal_length = s_send.why_length;
            }
        }
        if (s_send.answer != NULL)
        {
            EMBED_CALL(requests_answers_taken, RequestsPaneCtx, .text = s_send.answer, .length = s_send.answer_length);
        }
        g_cli_guard->persistent_buf_release(s_send.answer, g_cli_requests_owner);
        s_send.answer = NULL;
        EMBED_ATOMIC_STORE(&s_running, 0u);
        EMBED_CALL(requests_rows_laid, RequestsPaneCtx, .at = 0u);
        changed = 1;
    }
    if (!args->shown || (args->root == NULL))
    {
        return changed;
    }
    // the shown view's cursor followed where it moved, its request read again where it is another
    const int shown = cli_view_shown();
    if (shown < 0)
    {
        return changed;
    }
    // the view shown is among the views
    const char *const key = cli_view_key((unsigned int)shown);
    const char *const path = cli_view_path_at((unsigned int)shown);
    CliPosition cursor = {0ULL, 0ULL};
    if ((key == NULL) || (path == NULL) ||
        !EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = key, .cursor = &cursor))
    {
        return changed;
    }
    const int same = (cursor.line == s_requests.view_line) &&
                     cellul.eq(&(CatenaFinitaCfg){.src = key, .other = s_requests.view_key, .cap = CLI_PATH_MAX_LEN});
    if (same)
    {
        return changed;
    }
    const int followed = EMBED_CALL(cli_requests_pane.follow, CliRequestsPaneCfg, .root = args->root, .path = path,
                                    .key = key, .line = cursor.line);
    return changed || followed;
}

int cli_requests_pane_follow(const CliRequestsPaneCfg *args)
{
    unsigned long long length_of = 0ULL;
    const unsigned long long key_length =
        (args->key != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_PATH_MAX_LEN}) : 0ULL;
    const unsigned long long path_length =
        (args->path != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN}) : 0ULL;
    if ((args->root == NULL) || (key_length == 0ULL) || (key_length >= CLI_PATH_MAX_LEN) ||
        (path_length >= CLI_PATH_MAX_LEN))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_requests.view_key, .src = args->key, .bytes = key_length + 1ULL});
    s_requests.view_line = args->line;
    char *const text = EMBED_CALL(requests_text_alloc, RequestsPaneCtx, .key = args->key, .length_out = &length_of);
    if (text == NULL)
    {
        return 0;
    }
    const unsigned long long size = length_of;
    char request[REQUESTS_KEY];
    VerbaLinea request_line = {request, sizeof(request), 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int keyed = EMBED_CALL(cli_requests_text_table.key_at, CliRequestsTextCfg, .text = text, .size = size,
                                 .line = args->line, .out = &request_line, .err_msg = &err_msg) &&
                      mmgr_line_ok(&request_line);
    const int same_file =
        s_requests.followed &&
        cellul.eq(&(CatenaFinitaCfg){.src = args->path, .other = s_requests.path, .cap = CLI_PATH_MAX_LEN});
    const int same_request =
        same_file && keyed && (request_line.at == s_requests.request_length) &&
        (memor.cmp(&(MemoriaCfg){.src = request, .other = s_requests.request, .bytes = request_line.at}) == 0);
    if (same_request && !args->force)
    {
        g_cli_guard->persistent_buf_release(text, g_cli_requests_owner);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_requests.path, .src = args->path, .bytes = path_length + 1ULL});
    s_requests.followed = 1;
    if (!same_file || args->force)
    {
        EMBED_CALL(requests_envs_read, RequestsPaneCtx, .root = args->root, .path = args->path);
    }
    char *const written =
        EMBED_CALL(requests_written, RequestsPaneCtx, .root = args->root, .path = args->path, .text = text,
                   .length = size, .line_at = args->line, .at = 0u, .length_out = &length_of);
    g_cli_guard->persistent_buf_release(text, g_cli_requests_owner);
    if (written == NULL)
    {
        EMBED_CALL(requests_answers_taken, RequestsPaneCtx, .text = "", .length = 0ULL);
        return 1;
    }
    EMBED_CALL(requests_answers_taken, RequestsPaneCtx, .text = written, .length = length_of);
    g_cli_guard->persistent_buf_release(written, g_cli_requests_owner);
    return 1;
}

void cli_requests_pane_env(const CliRequestsPaneCfg *args)
{
    if ((args->at >= CLI_REQUESTS_CHOICE_ENV) && ((args->at - CLI_REQUESTS_CHOICE_ENV) <= s_requests.env_count))
    {
        s_requests.env_chosen = args->at - CLI_REQUESTS_CHOICE_ENV;
    }
}

unsigned int cli_requests_pane_rows(const CliRequestsPaneCfg *args)
{
    (void)args;
    return s_requests.rows;
}

int cli_requests_pane_row(const CliRequestsPaneCfg *args)
{
    if (args->at >= s_requests.rows)
    {
        return 0;
    }
    const RequestsLaid *const laid = &s_requests.row[args->at];
    *args->row = (CliRequestsRow){laid->kind,
                                  laid->tone,
                                  laid->group,
                                  laid->answer,
                                  (laid->text != NULL) ? laid->text : s_requests.text + laid->offset,
                                  laid->length,
                                  laid->sign,
                                  laid->open,
                                  laid->checked,
                                  laid->inert,
                                  laid->shown};
    return 1;
}

void cli_requests_pane_toggle(const CliRequestsPaneCfg *args)
{
    if ((args->at < s_requests.rows) && (s_requests.row[args->at].kind == CLI_REQUESTS_ROW_GROUP))
    {
        s_requests.open[s_requests.row[args->at].group] ^= 1u;
        EMBED_CALL(requests_rows_laid, RequestsPaneCtx, .at = 0u);
    }
}

void cli_requests_pane_show(const CliRequestsPaneCfg *args)
{
    if ((args->at < s_requests.rows) && (s_requests.row[args->at].kind == CLI_REQUESTS_ROW_ANSWER))
    {
        s_requests.shown = s_requests.row[args->at].answer;
        EMBED_CALL(requests_body_laid, RequestsPaneCtx, .at = 0u);
        EMBED_CALL(requests_rows_laid, RequestsPaneCtx, .at = 0u);
    }
}

void cli_requests_pane_check(const CliRequestsPaneCfg *args)
{
    if ((args->at < s_requests.rows) && (s_requests.row[args->at].kind == CLI_REQUESTS_ROW_ANSWER))
    {
        s_requests.checked ^= 1u << s_requests.row[args->at].answer;
        EMBED_CALL(requests_compared_released, RequestsPaneCtx, .at = 0u);
        EMBED_CALL(requests_rows_laid, RequestsPaneCtx, .at = 0u);
    }
}

// the text an answer is compared by written into `line`: why it failed, or its status line, its headers, a blank
// line and its body, or its size where it is not text, every carriage return left out
EMBED_INLINE void requests_answer_text(const RequestsPaneCtx *args)
{
    CliJson *const json = &s_requests.answers;
    VerbaLinea *const line = args->line;
    unsigned long long length_of = 0ULL;
    const char *const failed = EMBED_CALL(requests_string, RequestsPaneCtx, .json = json, .node = args->node,
                                          .name = "failed", .length_out = &length_of);
    if (failed != NULL)
    {
        (void)mmgr_line_put_n(line, failed, length_of);
        return;
    }
    static const char *const s_head[2] = {"version", "reason"};
    const char *const version = EMBED_CALL(requests_string, RequestsPaneCtx, .json = json, .node = args->node,
                                           .name = s_head[0], .length_out = &length_of);
    if (version != NULL)
    {
        (void)mmgr_line_put_n(line, version, length_of);
    }
    mmgr_line_ch(line, ' ');
    mmgr_line_decimal(
        line, EMBED_CALL(requests_tenths, RequestsPaneCtx, .json = json, .node = args->node, .name = "status") / 10ULL,
        1u);
    mmgr_line_ch(line, ' ');
    const char *const reason = EMBED_CALL(requests_string, RequestsPaneCtx, .json = json, .node = args->node,
                                          .name = s_head[1], .length_out = &length_of);
    if (reason != NULL)
    {
        (void)mmgr_line_put_n(line, reason, length_of);
    }
    const unsigned int headers =
        EMBED_CALL(requests_member, RequestsPaneCtx, .json = json, .node = args->node, .name = "headers");
    for (unsigned int at = 0u; headers != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int pair = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = headers, .index = at);
        if (pair == CLI_JSON_NONE)
        {
            break;
        }
        mmgr_line_ch(line, '\n');
        for (unsigned int side = 0u; side < 2u; side += 1u)
        {
            unsigned long long length = 0ULL;
            const char *const text =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = pair, .index = side),
                           .length_pointer = &length);
            mmgr_line_put(line, (side != 0u) ? ": " : "");
            if (text != NULL)
            {
                (void)mmgr_line_put_n(line, text, length);
            }
        }
    }
    mmgr_line_put(line, "\n\n");
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                   .node =
                       EMBED_CALL(requests_member, RequestsPaneCtx, .json = json, .node = args->node, .name = "binary"),
                   .kind = CFG_JSON_TRUE))
    {
        mmgr_line_decimal(
            line,
            EMBED_CALL(requests_tenths, RequestsPaneCtx, .json = json, .node = args->node, .name = "size") / 10ULL, 1u);
        mmgr_line_put(line, " bytes, not text");
        return;
    }
    const char *const body = EMBED_CALL(requests_string, RequestsPaneCtx, .json = json, .node = args->node,
                                        .name = "body", .length_out = &length_of);
    for (unsigned long long at = 0ULL; (body != NULL) && (at < length_of); at += 1ULL)
    {
        if (body[at] != '\r')
        {
            mmgr_line_ch(line, body[at]);
        }
    }
}

// the starts of the lines of `length` bytes at `text` into `out`'s cell of starts, each line ended by a newline or the
// text's end: how many lines
EMBED_INLINE unsigned int requests_line_starts(const RequestsPaneCtx *args)
{
    unsigned long long *const starts = args->starts;
    unsigned int lines = 0u;
    starts[lines] = 0ULL;
    lines += 1u;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        if (args->text[at] == '\n')
        {
            starts[lines] = at + 1ULL;
            lines += 1u;
        }
    }
    starts[lines] = args->length + 1ULL;
    return lines;
}

// a change the diff hands put after the changes found so far
static void requests_hunk_put(const CliEachHunkCfg *args)
{
    RequestsHunks *const found = args->context;
    found->hunk[found->count] = *args;
    found->count += 1ULL;
}

int cli_requests_pane_compare(const CliRequestsPaneCfg *args)
{
    (void)args;
    if (EMBED_CALL(requests_checked_count, RequestsPaneCtx, .at = 0u) != 2u)
    {
        return 0;
    }
    // the newer answer is the first checked, the answers standing the last first, and the older the second
    unsigned int newer = 0u;
    while ((s_requests.checked & (1u << newer)) == 0u)
    {
        newer += 1u;
    }
    unsigned int older = newer + 1u;
    while ((s_requests.checked & (1u << older)) == 0u)
    {
        older += 1u;
    }
    const unsigned int sides[2] = {EMBED_CALL(requests_answer, RequestsPaneCtx, .at = older),
                                   EMBED_CALL(requests_answer, RequestsPaneCtx, .at = newer)};
    // the two texts counted, then written into one cell for this comparison
    char counting_bytes[REQUESTS_COUNTING];
    unsigned long long bytes[2] = {0ULL, 0ULL};
    unsigned long long newlines_most = 0ULL;
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        VerbaLinea counting = {counting_bytes, sizeof(counting_bytes), 0ULL, requests_counted, NULL, 0ULL};
        EMBED_CALL(requests_answer_text, RequestsPaneCtx, .line = &counting, .node = sides[side]);
        bytes[side] = counting.flushed + counting.at;
        newlines_most += bytes[side] + 1ULL;
    }
    const unsigned long long text_bytes = bytes[0] + bytes[1] + 2ULL;
    char *const texts = g_cli_guard->temporary_buf_alloc(text_bytes, g_cli_requests_owner);
    if (texts == NULL)
    {
        return 0;
    }
    VerbaLinea older_line = {texts, bytes[0] + 1ULL, 0ULL};
    VerbaLinea newer_line = {texts + bytes[0] + 1ULL, bytes[1] + 1ULL, 0ULL};
    EMBED_CALL(requests_answer_text, RequestsPaneCtx, .line = &older_line, .node = sides[0]);
    EMBED_CALL(requests_answer_text, RequestsPaneCtx, .line = &newer_line, .node = sides[1]);
    const char *const side_text[2] = {texts, texts + bytes[0] + 1ULL};
    const unsigned long long side_bytes[2] = {older_line.at, newer_line.at};
    // the lines of each laid as the diff reads them, with the starts of the texts' own lines, and the changes found,
    // at most a change a line of either, in one cell for this comparison
    const unsigned long long units_bytes = (bytes[0] + bytes[1] + 2ULL) * sizeof(unsigned short);
    const unsigned long long lines_bytes = (newlines_most + 2ULL) * sizeof(unsigned long long);
    const unsigned long long hunks_bytes = (newlines_most + 2ULL) * sizeof(CliEachHunkCfg);
    unsigned char *const work =
        g_cli_guard->temporary_buf_alloc((3ULL * lines_bytes) + units_bytes + hunks_bytes, g_cli_requests_owner);
    if (work == NULL)
    {
        g_cli_guard->persistent_buf_release(texts, g_cli_requests_owner);
        return 0;
    }
    unsigned long long *const starts = (unsigned long long *)(void *)work;
    unsigned long long *const lengths = (unsigned long long *)(void *)(work + lines_bytes);
    unsigned long long *const own_starts = (unsigned long long *)(void *)(work + (2ULL * lines_bytes));
    unsigned short *const units = (unsigned short *)(void *)(work + (3ULL * lines_bytes));
    CliEachHunkCfg *const hunks = (CliEachHunkCfg *)(void *)(work + (3ULL * lines_bytes) + units_bytes);
    // the diff's lines are fewer than an unsigned int counts, as the texts' newlines are
    const unsigned int lines_most = (unsigned int)(newlines_most + 1ULL);
    CliLines laid[2];
    unsigned int line_count[2] = {0u, 0u};
    unsigned long long own_first[2] = {0ULL, 0ULL};
    unsigned long long used_units = 0ULL;
    unsigned long long used_lines = 0ULL;
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        laid[side] = (CliLines){units + used_units,
                                side_bytes[side] + 1ULL,
                                0ULL,
                                starts + used_lines,
                                lengths + used_lines,
                                lines_most - (unsigned int)used_lines,
                                0u,
                                0};
        EMBED_CALL(cli_document_text_size.lines_get, CliDocumentTextSizeCfg, .text = side_text[side],
                   .size = side_bytes[side], .lines_out = &laid[side]);
        used_units += side_bytes[side] + 1ULL;
        used_lines += laid[side].lines;
        // the newer's own starts stand after the older's and the older's end
        own_first[side] = (side == 0u) ? 0ULL : (unsigned long long)line_count[0] + 1ULL;
        line_count[side] = EMBED_CALL(requests_line_starts, RequestsPaneCtx, .text = side_text[side],
                                      .length = side_bytes[side], .starts = own_starts + own_first[side]);
    }
    RequestsHunks found = {hunks, 0ULL};
    const int diffed = EMBED_CALL(cli_document_out_then.line_changes, CliDocumentOutThenCfg, .then = &laid[0],
                                  .now = &laid[1], .each = requests_hunk_put, .context = &found);
    // the lines listed: those both have once, those only the older has with a minus, those only the newer has with a
    // plus, each change's taken out before it puts in
    unsigned int listed = line_count[0];
    for (unsigned long long hunk = 0ULL; diffed && (hunk < found.count); hunk += 1ULL)
    {
        listed += hunks[hunk].now_to - hunks[hunk].now_from;
    }
    const unsigned long long compared_bytes = (unsigned long long)listed * sizeof(RequestsCompared);
    unsigned char *const cell =
        diffed ? g_cli_guard->persistent_buf_alloc(compared_bytes + text_bytes, g_cli_requests_owner) : NULL;
    if (cell == NULL)
    {
        g_cli_guard->persistent_buf_release(work, g_cli_requests_owner);
        g_cli_guard->persistent_buf_release(texts, g_cli_requests_owner);
        return 0;
    }
    RequestsCompared *const compared = (RequestsCompared *)(void *)cell;
    char *const compared_text = (char *)(cell + compared_bytes);
    memor.cpy(&(MemoriaCfg){.dst = compared_text, .src = texts, .bytes = text_bytes});
    const unsigned long long newer_offset = bytes[0] + 1ULL;
    unsigned int put = 0u;
    unsigned int then_at = 0u;
    for (unsigned long long hunk = 0ULL; hunk <= found.count; hunk += 1ULL)
    {
        const int last = hunk == found.count;
        const unsigned int then_from = last ? line_count[0] : hunks[hunk].then_from;
        for (; (then_at < then_from) && (then_at < line_count[0]) && (put < listed); then_at += 1u)
        {
            const unsigned long long start = own_starts[then_at];
            compared[put] = (RequestsCompared){' ', start, own_starts[then_at + 1u] - start - 1ULL};
            put += 1u;
        }
        if (last)
        {
            break;
        }
        for (unsigned int line = hunks[hunk].then_from; (line < hunks[hunk].then_to) && (put < listed); line += 1u)
        {
            const unsigned long long start = own_starts[line];
            compared[put] = (RequestsCompared){'-', start, own_starts[line + 1u] - start - 1ULL};
            put += 1u;
        }
        for (unsigned int line = hunks[hunk].now_from; (line < hunks[hunk].now_to) && (put < listed); line += 1u)
        {
            const unsigned long long *const newer_starts = own_starts + own_first[1];
            compared[put] = (RequestsCompared){'+', newer_offset + newer_starts[line],
                                               newer_starts[line + 1u] - newer_starts[line] - 1ULL};
            put += 1u;
        }
        then_at = hunks[hunk].then_to;
    }
    g_cli_guard->persistent_buf_release(work, g_cli_requests_owner);
    g_cli_guard->persistent_buf_release(texts, g_cli_requests_owner);
    EMBED_CALL(requests_compared_released, RequestsPaneCtx, .at = 0u);
    s_requests.compared_cell = cell;
    s_requests.compared = compared;
    s_requests.compared_text = compared_text;
    s_requests.compared_count = put;
    EMBED_CALL(requests_rows_laid, RequestsPaneCtx, .at = 0u);
    return 1;
}

int cli_requests_pane_curl(const CliRequestsPaneCfg *args)
{
    CliRequestsPaneCfg asked = *args;
    EMBED_CALL(requests_view_taken, RequestsPaneCtx, .asked = &asked);
    args = &asked;
    unsigned long long length_of = 0ULL;
    char *const text = EMBED_CALL(requests_text_alloc, RequestsPaneCtx, .key = args->key, .length_out = &length_of);
    if ((text == NULL) || (args->root == NULL) || (args->path == NULL))
    {
        g_cli_guard->persistent_buf_release(text, g_cli_requests_owner);
        mmgr_line_put(args->out, "");
        return 0;
    }
    // the curl line as a JSON string, counted, written into a cell of its size and read as its letters
    char counting_bytes[REQUESTS_COUNTING];
    VerbaLinea counting = {counting_bytes, sizeof(counting_bytes), 0ULL, requests_counted, NULL, 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliRequestsTextCfg text_asked = {.text = text,
                                     .size = length_of,
                                     .out = &counting,
                                     .err_msg = &err_msg,
                                     .line = args->line,
                                     .root = args->root,
                                     .path = args->path,
                                     .env = EMBED_CALL(requests_env_name, RequestsPaneCtx, .at = 0u)};
    int curled = cli_requests_text_table.curl(&text_asked);
    const unsigned long long bytes = counting.flushed + counting.at;
    char *const quoted = curled ? g_cli_guard->temporary_buf_alloc(bytes + 1ULL, g_cli_requests_owner) : NULL;
    VerbaLinea quoted_line = {quoted, (quoted != NULL) ? bytes + 1ULL : 0ULL, 0ULL};
    text_asked.out = &quoted_line;
    curled = (quoted != NULL) && cli_requests_text_table.curl(&text_asked) && mmgr_line_ok(&quoted_line);
    g_cli_guard->persistent_buf_release(text, g_cli_requests_owner);
    CliJson read = {0};
    char json_why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea json_err = {json_why, sizeof(json_why), 0ULL};
    const unsigned int root =
        curled ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg,
                            .json = &read, .text = quoted, .size = quoted_line.at, .err_msg = &json_err,
                            .owner = g_cli_requests_owner)
               : CLI_JSON_NONE;
    unsigned long long length = 0ULL;
    const char *const letters = (root != CLI_JSON_NONE)
                                    ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                 .json_read = &read, .node = root, .length_pointer = &length)
                                    : NULL;
    if (letters != NULL)
    {
        (void)mmgr_line_put_n(args->out, letters, length);
    }
    else
    {
        (void)mmgr_line_put_n(args->out, why, err_msg.at);
    }
    g_cli_guard->persistent_buf_release(read.node, g_cli_requests_owner);
    g_cli_guard->persistent_buf_release(quoted, g_cli_requests_owner);
    return letters != NULL;
}

int cli_requests_pane_copy(const CliRequestsPaneCfg *args)
{
    CliRequestsPaneCfg asked = *args;
    EMBED_CALL(requests_view_taken, RequestsPaneCtx, .asked = &asked);
    args = &asked;
    char counting_bytes[REQUESTS_COUNTING];
    VerbaLinea counting = {counting_bytes, sizeof(counting_bytes), 0ULL, requests_counted, NULL, 0ULL};
    if (!EMBED_CALL(cli_requests_pane.curl, CliRequestsPaneCfg, .root = args->root, .path = args->path,
                    .key = args->key, .line = args->line, .out = &counting))
    {
        return 0;
    }
    const unsigned long long bytes = counting.flushed + counting.at;
    char *const cell = g_cli_guard->temporary_buf_alloc(bytes + 1ULL, g_cli_requests_owner);
    VerbaLinea line = {cell, (cell != NULL) ? bytes + 1ULL : 0ULL, 0ULL};
    const int curled =
        (cell != NULL) && EMBED_CALL(cli_requests_pane.curl, CliRequestsPaneCfg, .root = args->root, .path = args->path,
                                     .key = args->key, .line = args->line, .out = &line);
    const int copied =
        curled && (mmgr_line_finish(&line) != 0ULL) &&
        EMBED_CALL(cli_size_text_internal.clipboard_text_write, CliSizeTextCtx, .size = line.at, .text = cell);
    g_cli_guard->persistent_buf_release(cell, g_cli_requests_owner);
    return copied;
}

unsigned int cli_requests_pane_choices(const CliRequestsPaneCfg *args)
{
    (void)args;
    return CLI_REQUESTS_CHOICE_ENV + 1u + s_requests.env_count;
}

int cli_requests_pane_choice(const CliRequestsPaneCfg *args)
{
    if (args->at >= (CLI_REQUESTS_CHOICE_ENV + 1u + s_requests.env_count))
    {
        return 0;
    }
    CliRequestsRow *const row = args->row;
    *row = (CliRequestsRow){0};
    const int sending = EMBED_ATOMIC_LOAD(&s_running) != 0u;
    static const char s_send_text[] = "Send";
    static const char s_sending_text[] = "Sending";
    static const char s_curl_text[] = "Copy as curl";
    static const char s_none_text[] = "No environment";
    if (args->at == CLI_REQUESTS_CHOICE_SEND)
    {
        row->text = sending ? s_sending_text : s_send_text;
        row->text_length = sending ? sizeof(s_sending_text) - 1ULL : sizeof(s_send_text) - 1ULL;
        row->inert = sending;
        return 1;
    }
    if (args->at == CLI_REQUESTS_CHOICE_CURL)
    {
        row->text = s_curl_text;
        row->text_length = sizeof(s_curl_text) - 1ULL;
        return 1;
    }
    const unsigned int env = args->at - CLI_REQUESTS_CHOICE_ENV;
    row->open = env == s_requests.env_chosen;
    if (env == 0u)
    {
        row->text = s_none_text;
        row->text_length = sizeof(s_none_text) - 1ULL;
        return 1;
    }
    unsigned long long length = 0ULL;
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_requests.envs,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                      .json_read = &s_requests.envs, .array = s_requests.names_node, .index = env - 1u),
                   .length_pointer = &length);
    row->text = (name != NULL) ? name : "";
    row->text_length = (name != NULL) ? length : 0ULL;
    return 1;
}
