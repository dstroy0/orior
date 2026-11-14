// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests.h: Requests: a file of HTTP requests, `.http` or `.rest`, as JetBrains' HTTP Client writes them, each sent
// from this process and its answers stored.
//
// A file contains requests one after another, each starting at a line of `###`, the rest of which names it, or at the
// top of the file. A request is its request line, the method and the address, GET where no method is written, an
// address that runs on in indented lines; the lines of its headers under it; and after a blank line its body, or `<
// path` for a body read from a file beside it. A line of `#` or `//` before the body is a comment, `# @name x` names
// the request, and `# @no-redirect` prevents it from following a redirect. A line `@name = value` outside a request's
// body sets a variable of the file, and a line of `> ` after a body, a handler of JetBrains' that runs a script, is
// passed over: a request here runs nothing.
//
// `{{name}}` stands for the value of a variable: of the file, or of the environment chosen, read from
// http-client.env.json and the reader's own http-client.private.env.json, each the nearest beside the file or in a
// folder above it within the tree, the private values over the shared ones and `$shared` giving values to every
// environment; or it is one of `{{$uuid}}`, `{{$timestamp}}`, `{{$isoTimestamp}}` and `{{$randomInt}}`. The private
// file is the reader's alone: impero adds its name to the tree's .gitignore as it makes it.
//
// A request goes out over HTTP/1.1 from this process, through the system's TLS for https (tls.h), and its answer is
// read whole: chunked, gzip and deflate are undone, and a redirect is followed up to ten times. Each request's last
// answers are stored in orior's own folder, the last first, each with what was sent, for the reader to look back on and
// compare. A curl line becomes a request, and a request a curl line. What a call gives is written as JSON, as the
// window reads it (requests_parse.c, requests_written.c, requests_values.c, requests_fill.c, requests_ready.c,
// requests_url.c, requests_head.c, requests_http.c, requests_send.c, requests_answer.c, requests_history.c,
// requests_words.c, requests_curl.c, requests_curl_line.c)
#ifndef REQUESTS_H
#define REQUESTS_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliRequestsTextCfg CliRequestsTextCfg;
typedef struct CliRequestsAddressCfg CliRequestsAddressCfg;
typedef struct CliRequestsCfg CliRequestsCfg;

// the environment files a tree names
#define CLI_REQUESTS_PUBLIC "http-client.env.json"
#define CLI_REQUESTS_PRIVATE "http-client.private.env.json"

// the requests of `size` bytes of `text`, a file of requests, and the variables it sets, written into `out`; and the
// key the answers of the request whose part contains line `line`, or the last before it, are stored under, its name,
// else its method and address as written, into `out`. Each 1, or 0 with why given (requests_written.c)
int cli_requests_on_parse(const CliRequestsTextCfg *args);
int cli_requests_key_at(const CliRequestsTextCfg *args);

// the environment files of the file at `path`, the names of their environments and the tree's paths of the shared
// file and the private one, written into `out`; the reader's private environment file made beside the file at `path`
// where none is there, with its name added to the tree's .gitignore, its path in the tree into `out` as a JSON string.
// Each 1, or 0 with
// why given (requests_values.c)
int cli_requests_environments(const CliRequestsTextCfg *args);
int cli_requests_private_init(const CliRequestsTextCfg *args);

// the answer the request at line `line` of the file at `path`, whose text is `size` bytes of `text`, gets in
// environment `env`, none where it is NULL, stored with its last answers and written into `out`: 1, or 0 with why given
// (requests_send.c); the answers stored for the request named `key` of the file at `path`, the last first, written into
// `out`, with the key (requests_history.c)
int cli_requests_answers(const CliRequestsTextCfg *args);

// the curl line that sends the request at line `line`, for bash and the shells that read quotes as it does; and the
// request a curl line sends, written as a request of a file. Each written into `out` as a JSON string: 1, or 0 with
// why given (requests_curl_line.c, requests_curl.c)
int cli_requests_curl(const CliRequestsTextCfg *args);
int cli_requests_from_curl(const CliRequestsCfg *args);

struct CliRequestsTextCfg
{
    const char *text;
    unsigned long long size;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned long long line;
    const char *root;
    const char *path;
    const char *env;
};

struct CliRequestsAddressCfg
{
    VerbaLinea *err_msg;
    const char *address;
    unsigned int *status;
    VerbaLinea *reason;
    int *cut;
    VerbaLinea *body;
};

struct CliRequestsCfg
{
    unsigned long long size;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *line_text;
};

typedef struct
{
    int (*parse)(const CliRequestsTextCfg *args);
    int (*key_at)(const CliRequestsTextCfg *args);
    int (*environments)(const CliRequestsTextCfg *args);
    int (*private_init)(const CliRequestsTextCfg *args);
    int (*answers)(const CliRequestsTextCfg *args);
    int (*curl)(const CliRequestsTextCfg *args);
} CliRequestsTextNs;
EMBED_TABLE_LAYOUT(CliRequestsTextNs, parse, key_at, environments, private_init, answers, curl);
EMBED_TABLE_STORAGE CliRequestsTextNs cli_requests_text_table EMBED_UNUSED = {
    .parse = cli_requests_on_parse,
    .key_at = cli_requests_key_at,
    .environments = cli_requests_environments,
    .private_init = cli_requests_private_init,
    .answers = cli_requests_answers,
    .curl = cli_requests_curl,
};

// The Requests view of the Comms pane as the window reads it (requests_pane.c): for the file of requests shown, the
// answers stored for the request its cursor stands in, read again where the cursor moves into another request. Over
// the rows the head's choices: Send, which reads Sending and is inert while a send runs, Copy as curl, then No
// environment and each environment of the file, the one chosen lit. The rows: where the last send was refused, its
// refusal; the shown answer's status, colored good, moved or failed by its class, with its total time, its size and
// its redirects, or why it failed; its times; a group row Headers opening to a row each header; a group row Sent
// opening to what was sent; its body a row a line, laid out where it is JSON, with a row that reads so where it is cut
// or not text; and with two answers or more a group row Last answers, a row each answer with its box lit where it is
// checked and the one shown marked, and a Compare row inert unless two are checked, the two compared after it a row a
// line with a sign: a space for a line both have, a minus for one only the older has, a plus for one only the newer
// has. With no answer, one row reads so. Every text is copied into the row's own bytes with its length, cut to
// CLI_REQUESTS_ROW_TEXT
#define CLI_REQUESTS_ROW_REFUSAL 0u
#define CLI_REQUESTS_ROW_STATUS 1u
#define CLI_REQUESTS_ROW_TIMES 2u
#define CLI_REQUESTS_ROW_GROUP 3u
#define CLI_REQUESTS_ROW_HEADER 4u
#define CLI_REQUESTS_ROW_SENT 5u
#define CLI_REQUESTS_ROW_BODY 6u
#define CLI_REQUESTS_ROW_NOTE 7u
#define CLI_REQUESTS_ROW_ANSWER 8u
#define CLI_REQUESTS_ROW_COMPARE 9u
#define CLI_REQUESTS_ROW_SIGN 10u
// a group row's group: the headers, what was sent, the last answers
#define CLI_REQUESTS_GROUP_HEADERS 0u
#define CLI_REQUESTS_GROUP_SENT 1u
#define CLI_REQUESTS_GROUP_ANSWERS 2u
#define CLI_REQUESTS_GROUPS 3u
// a row's tone: the text's own, an answer of 2xx, of 3xx, one failed or of 4xx and above, and quieter
#define CLI_REQUESTS_TONE_PLAIN 0u
#define CLI_REQUESTS_TONE_GOOD 1u
#define CLI_REQUESTS_TONE_MOVED 2u
#define CLI_REQUESTS_TONE_FAILED 3u
#define CLI_REQUESTS_TONE_QUIET 4u
// the head's choices: Send, Copy as curl, then the environments, No environment first
#define CLI_REQUESTS_CHOICE_SEND 0u
#define CLI_REQUESTS_CHOICE_CURL 1u
#define CLI_REQUESTS_CHOICE_ENV 2u
// the most answers the view lists and checks, a bit of a word each; and the most bytes of a row's text with its end
#define CLI_REQUESTS_ANSWERS_MAX 32u
#define CLI_REQUESTS_ROW_TEXT 2048u
typedef struct
{
    unsigned int type;
    unsigned int tone;
    unsigned int group;
    unsigned int answer;
    const char *text;
    unsigned long long text_length;
    char sign;
    int open;
    int checked;
    int inert;
    int shown;
    char text_copied[CLI_REQUESTS_ROW_TEXT];
} CliRequestsRow;

typedef struct CliRequestsPaneCfg CliRequestsPaneCfg;
// the pane begun, `woken` waking the view's thread as an answer comes in; the request the cursor of the view `key` at
// `path` stands in at `line`, the shown view's where `key` is NULL, in the tree at `root`, the command line's where it
// is NULL, sent on a worker of its own and its stored answers read again there: 1 where it started,
// 0 where a send runs or nothing is to send; whether a send runs; and the answer of a send taken in where one came in,
// and while `shown` the answers followed to the request the shown view's cursor stands in: 1 where the rows changed
void cli_requests_pane_begin(const CliRequestsPaneCfg *args);
int cli_requests_pane_send(const CliRequestsPaneCfg *args);
int cli_requests_pane_sending(const CliRequestsPaneCfg *args);
int cli_requests_pane_step(const CliRequestsPaneCfg *args);
// the answers stored for the request the line `line` of the view `key` at `path` stands in read where its request is
// another than the one shown, or where `force`, with the file's environments: 1 where the rows changed
int cli_requests_pane_follow(const CliRequestsPaneCfg *args);
// the environment chosen, `at` among the head's choices; how many rows there are; the row `at` into `row`: 1, or 0 past
// the last; the group of the row `at` opened or closed; the answer of the row `at` shown; its box checked, or not
// where it is; and the two answers checked compared: 1 where they were
void cli_requests_pane_env(const CliRequestsPaneCfg *args);
unsigned int cli_requests_pane_rows(const CliRequestsPaneCfg *args);
int cli_requests_pane_row(const CliRequestsPaneCfg *args);
void cli_requests_pane_toggle(const CliRequestsPaneCfg *args);
void cli_requests_pane_show(const CliRequestsPaneCfg *args);
void cli_requests_pane_check(const CliRequestsPaneCfg *args);
int cli_requests_pane_compare(const CliRequestsPaneCfg *args);
// the curl line of the request at `line` of the view `key` at `path`, the shown view's where `key` is NULL as for a
// send, in the environment chosen written into `out`: 1,
// or 0 with why written; the same put on the system's clipboard: 1 where it went; how many choices the head has; and
// the choice `at` into `row`, its text, lit where it is the environment chosen, inert where it can do nothing: 1, or 0
// past the last
int cli_requests_pane_curl(const CliRequestsPaneCfg *args);
int cli_requests_pane_copy(const CliRequestsPaneCfg *args);
unsigned int cli_requests_pane_choices(const CliRequestsPaneCfg *args);
int cli_requests_pane_choice(const CliRequestsPaneCfg *args);
struct CliRequestsPaneCfg
{
    void (*woken)(void);
    const char *root;
    const char *path;
    const char *key;
    unsigned long long line;
    unsigned int at;
    CliRequestsRow *row;
    int shown;
    int force;
    unsigned long long now_ms;
    VerbaLinea *out;
};

typedef struct
{
    void (*begin)(const CliRequestsPaneCfg *args);
    int (*send)(const CliRequestsPaneCfg *args);
    int (*sending)(const CliRequestsPaneCfg *args);
    int (*step)(const CliRequestsPaneCfg *args);
    int (*follow)(const CliRequestsPaneCfg *args);
    void (*env)(const CliRequestsPaneCfg *args);
    unsigned int (*rows)(const CliRequestsPaneCfg *args);
    int (*row)(const CliRequestsPaneCfg *args);
    void (*toggle)(const CliRequestsPaneCfg *args);
    void (*show)(const CliRequestsPaneCfg *args);
    void (*check)(const CliRequestsPaneCfg *args);
    int (*compare)(const CliRequestsPaneCfg *args);
    int (*curl)(const CliRequestsPaneCfg *args);
    int (*copy)(const CliRequestsPaneCfg *args);
    unsigned int (*choices)(const CliRequestsPaneCfg *args);
    int (*choice)(const CliRequestsPaneCfg *args);
} CliRequestsPaneNs;
EMBED_TABLE_LAYOUT(CliRequestsPaneNs, begin, send, sending, step, follow, env, rows, row, toggle, show, check, compare,
                   curl, copy, choices, choice);
EMBED_TABLE_STORAGE CliRequestsPaneNs cli_requests_pane EMBED_UNUSED = {
    .begin = cli_requests_pane_begin,
    .send = cli_requests_pane_send,
    .sending = cli_requests_pane_sending,
    .step = cli_requests_pane_step,
    .follow = cli_requests_pane_follow,
    .env = cli_requests_pane_env,
    .rows = cli_requests_pane_rows,
    .row = cli_requests_pane_row,
    .toggle = cli_requests_pane_toggle,
    .show = cli_requests_pane_show,
    .check = cli_requests_pane_check,
    .compare = cli_requests_pane_compare,
    .curl = cli_requests_pane_curl,
    .copy = cli_requests_pane_copy,
    .choices = cli_requests_pane_choices,
    .choice = cli_requests_pane_choice,
};

typedef struct
{
    int (*from_curl)(const CliRequestsCfg *args);
} CliRequestsNs;
EMBED_TABLE_LAYOUT(CliRequestsNs, from_curl);
EMBED_TABLE_STORAGE CliRequestsNs cli_requests_table EMBED_UNUSED = {
    .from_curl = cli_requests_from_curl,
};

#endif
