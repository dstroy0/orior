// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_internal.h: what the requests sources share: a file of requests read, the variables a request is filled
// from, a request ready to go, and its answer
#ifndef REQUESTS_INTERNAL_H
#define REQUESTS_INTERNAL_H
typedef struct CliRequestsTextCtx CliRequestsTextCtx;
typedef struct CliRequestsErrMsgCtx CliRequestsErrMsgCtx;
typedef struct CliRequestsTextSizeCtx CliRequestsTextSizeCtx;
typedef struct CliRequestsTextSizeBaseCtx CliRequestsTextSizeBaseCtx;
typedef struct CliRequestsNameCtx CliRequestsNameCtx;
typedef struct CliRequestsErrMsgRootCtx CliRequestsErrMsgRootCtx;
typedef struct CliRequestsOutCtx CliRequestsOutCtx;
typedef struct CliRequestsTextSizeAnswerCtx CliRequestsTextSizeAnswerCtx;
typedef struct CliRequestsRootCtx CliRequestsRootCtx;
typedef struct CliRequestsSizeCtx CliRequestsSizeCtx;
typedef struct CliRequestsErrMsgUrlCtx CliRequestsErrMsgUrlCtx;
typedef struct CliRequestsTextSizeNeedleCtx CliRequestsTextSizeNeedleCtx;
typedef struct CliRequestsSizeErrMsgCtx CliRequestsSizeErrMsgCtx;
typedef struct CliRequestsSizeAnswerCtx CliRequestsSizeAnswerCtx;
typedef struct CliRequestsOutHopCtx CliRequestsOutHopCtx;
typedef struct CliRequestsOutBaseCtx CliRequestsOutBaseCtx;
typedef struct CliRequestsParsedCtx CliRequestsParsedCtx;
typedef struct CliRequestsErrMsgWorkStateCtx CliRequestsErrMsgWorkStateCtx;
typedef struct CliRequestsTextSizeParsedCtx CliRequestsTextSizeParsedCtx;
typedef struct CliRequestsSentCtx CliRequestsSentCtx;
typedef struct CliRequestsTextStartCtx CliRequestsTextStartCtx;
typedef struct CliRequestsSizeOutCtx CliRequestsSizeOutCtx;
typedef struct CliRequestsSizeNameCtx CliRequestsSizeNameCtx;
typedef struct CliRequestsTextAtCtx CliRequestsTextAtCtx;
typedef struct CliRequestsCtx CliRequestsCtx;
typedef struct CliRequestsNameAnswerCtx CliRequestsNameAnswerCtx;
typedef struct CliRequestsOutSecondsCtx CliRequestsOutSecondsCtx;
typedef struct CliRequestsPoolCtx CliRequestsPoolCtx;

#include "../command_line/cells.h"
#include "embed_dispatch_layout.h"
#include "requests.h"

// the cells the requests are read, filled and answered in (requests_values.c)
extern CellOwner g_cli_requests_cells;

// the most requests a file contains that are read, headers a request contains, variables a file sets, values an
// environment gives, and the bytes their texts take; the most of an answer's body read, and of it stored with the
// answers; how many answers each request retains; and the most redirects one request follows
#define CLI_REQUESTS_MAX 1024u
#define CLI_REQUESTS_HEADERS 64u
#define CLI_REQUESTS_VARIABLES 256u
#define CLI_REQUESTS_VALUES 512u
#define CLI_REQUESTS_TEXT (1u << 22u)
#define CLI_REQUESTS_BODY (32u << 20u)
#define CLI_REQUESTS_HISTORY_BODY_MAX (1u << 20u)
#define CLI_REQUESTS_HISTORY_MAX 10u
#define CLI_REQUESTS_REDIRECTS 10u

// the bytes of an address, of a header's name and value, and of an answer's reason and version
#define CLI_REQUESTS_ADDRESS 8192u
#define CLI_REQUESTS_NAME 256u
#define CLI_REQUESTS_VALUE 4096u

// A text stored in a pool: where it starts and its length, a 0 after it
typedef struct
{
    unsigned long long at;
    unsigned long long size;
} CliRequestsText;

// A request of a file as it is written, its variables not yet filled in: its name; the first and the last line of the
// request's part of the file, and its request line, from 0; its method and address; its headers; its body or the file
// it is read from, where it has one; and whether it follows a redirect
typedef struct
{
    CliRequestsText name;
    unsigned long long start;
    unsigned long long end;
    unsigned long long line;
    CliRequestsText method;
    CliRequestsText url;
    CliRequestsText header_name[CLI_REQUESTS_HEADERS];
    CliRequestsText header_value[CLI_REQUESTS_HEADERS];
    unsigned int headers;
    CliRequestsText body;
    int has_body;
    CliRequestsText body_file;
    int has_body_file;
    int redirect;
} CliRequest;

// A file of requests read: its requests in order, the variables it sets, the pool of their texts, and 1 in `full`
// where a line, a request, a header, a variable or a text did not fit. The requests are a cell of the count of the
// file's parts and the pool a cell sized to the file, each given back with cli_requests_parsed_free
typedef struct
{
    CliRequest *requests;
    unsigned int count;
    unsigned int requests_capacity;
    CliRequestsText variable_name[CLI_REQUESTS_VARIABLES];
    CliRequestsText variable_value[CLI_REQUESTS_VARIABLES];
    unsigned int variables;
    char *text;
    unsigned long long used;
    unsigned long long text_capacity;
    int full;
} CliRequestsParsed;

// The values a request's variables are filled from, a name to one value, and the pool of their texts, a cell at their
// most, to CLI_REQUESTS_TEXT at the most, given back with cli_requests_values_free
typedef struct
{
    CliRequestsText name[CLI_REQUESTS_VALUES];
    CliRequestsText value[CLI_REQUESTS_VALUES];
    unsigned int count;
    char *text;
    unsigned long long used;
    unsigned long long capacity;
} CliRequestsValues;

// A request ready to go: its method, address and headers with its variables filled in, its body read, and whether it
// follows a redirect
typedef struct
{
    char method[CLI_REQUESTS_NAME];
    char url[CLI_REQUESTS_ADDRESS];
    char header_name[CLI_REQUESTS_HEADERS][CLI_REQUESTS_NAME];
    char header_value[CLI_REQUESTS_HEADERS][CLI_REQUESTS_VALUE];
    unsigned int headers;
    const char *body;
    unsigned long long body_size;
    int redirect;
} CliRequestsSent;

// How long each part of a request took, in milliseconds: finding the host and opening the connection, TLS, waiting for
// the first byte of the answer, reading the rest, and all of it
typedef struct
{
    double resolve;
    double connect;
    double tls;
    double wait;
    double read;
    double total;
} CliRequestsTimes;

// the bytes of an answer's head stored and the most headers it contains; and the bytes a connection's reading buffers,
// the longest line of a head and one read past it
#define CLI_REQUESTS_HEAD (1u << 20u)
#define CLI_REQUESTS_ANSWER_HEADERS 4096u
#define CLI_REQUESTS_READ_STATE ((64u << 10u) + (16u << 10u) + 1u)

// An answer: when it came, its address, its status, its reason, version and headers stored in the pool of its head, its
// body, its size, whether it was cut, whether it is not text, how long each part took, the redirects followed on the
// way, and what failed where nothing answered
typedef struct
{
    unsigned long long at;
    char url[CLI_REQUESTS_ADDRESS];
    unsigned int status;
    CliRequestsText reason;
    CliRequestsText version;
    CliRequestsText header_name[CLI_REQUESTS_ANSWER_HEADERS];
    CliRequestsText header_value[CLI_REQUESTS_ANSWER_HEADERS];
    unsigned int headers;
    char head[CLI_REQUESTS_HEAD];
    unsigned long long head_used;
    const unsigned char *body;
    unsigned long long size;
    int cut;
    int binary;
    CliRequestsTimes times;
    unsigned int redirect_status[CLI_REQUESTS_REDIRECTS];
    char redirect_url[CLI_REQUESTS_REDIRECTS][CLI_REQUESTS_ADDRESS];
    unsigned int redirects;
    int has_failed;
    char failed[CLI_ERR_MSG_MAX_LEN];
} CliRequestsAnswer;

// An address taken apart: whether it is https, its host, its port, and the path with its query
typedef struct
{
    int secure;
    char host[CLI_REQUESTS_ADDRESS];
    unsigned int port;
    char target[CLI_REQUESTS_ADDRESS + 1u];
} CliRequestsAddress;

// the bytes a request's body and an answer's body as read are written in, each a line at their most, the answer's to
// CLI_REQUESTS_BODY at the most
#define CLI_REQUESTS_HEAD_CAPACITY (1ull << 20u)

// The work state a request is sent in: the request as it was made ready and its body, a line, the request as it goes on
// each hop and its address taken apart, its answer, the answer's body as read, a line, and as undone, a cell of
// `unpacked_capacity` taken as it is undone, and the bytes read from the connection not yet taken, from `start` to
// `end`
typedef struct
{
    CliRequestsSent sent;
    VerbaLinea body;
    CliRequestsSent hop;
    CliRequestsAddress address;
    CliRequestsAnswer answer;
    VerbaLinea raw;
    unsigned char *unpacked;
    unsigned long long unpacked_capacity;
    unsigned char *read_state;
    unsigned long long start;
    unsigned long long end;
} CliRequestsWorkState;

// the lock a file of requests is read under, and the file read under it (requests_parse.c)
extern CliRequestsParsed g_cli_requests_parsed;

// a file of requests read into `parsed`, what it contained before given back, the lock above acquired: 1, or 0 where it
// is larger than `parsed` fits or its cells are refused; its cells given back, `parsed` left empty; the request whose
// part contains line `line`, or the last before it, the first where none does: NULL where the file contains none; a
// text of the pool (requests_white.c); the part of a text from `start` to `end` with the white letters at its two ends
// passed over, as Rust's str::trim trims it; and the bytes of the white letter at `at` of a text ending at `end`, 0
// where the letter there is not white (requests_parse.c, requests_white.c)
int cli_requests_parse(const CliRequestsTextSizeParsedCtx *args);
void cli_requests_parsed_free(const CliRequestsTextSizeParsedCtx *args);
const CliRequest *cli_requests_at(const CliRequestsCtx *args);
const char *cli_requests_text(const CliRequestsPoolCtx *args);
void cli_requests_trim(const CliRequestsTextStartCtx *args);
unsigned long long cli_requests_white_at(const CliRequestsTextAtCtx *args);
// the key a request's answers are stored under, its name, else its method and address as written, into `out`; and the
// reason a file of requests was not read whole (requests_written.c)
void cli_requests_full_err(const CliRequestsTextCtx *args);
void cli_requests_key(const CliRequestsParsedCtx *args);

// the nearest file `name` beside the file at `path` or in a folder above it within the tree, into `found`: 1, or 0
// where there is none; the values of environment `env`, or of none, for the file at `path`: the shared file's then the
// private file's, `$shared` under the environment's own: 1, or 0 with why given; the values' pool given back and the
// values left empty; a value put, in place of one of its name: 1, or 0 where it does not fit or its work state is
// refused; a name's value, NULL where it has none; and whether the peer's certificate is checked for environment `env`
// (requests_values.c)
int cli_requests_nearest(const CliRequestsRootCtx *args);
int cli_requests_values(const CliRequestsErrMsgRootCtx *args);
void cli_requests_values_free(const CliRequestsErrMsgRootCtx *args);
int cli_requests_value_append(const CliRequestsNameCtx *args);
const CliRequestsText *cli_requests_value(const CliRequestsSizeNameCtx *args);
int cli_requests_verifies(const CliRequestsErrMsgRootCtx *args);
// `size` bytes of `text` with each `{{name}}` given its value, a value that names others given theirs in turn, eight
// deep, and one no variable sets given as `$uuid`, `$random.uuid`, `$timestamp`, `$isoTimestamp` and `$randomInt`
// name it, into `out`: 1, or 0 with why given (requests_fill.c)
int cli_requests_fill(const CliRequestsTextSizeCtx *args);
// the date and time `seconds` after 1970 began, as ISO 8601 in UTC, written into `out` (requests_fill.c)
void cli_requests_iso(const CliRequestsOutSecondsCtx *args);
// the curl line that sends `sent`, written inside a JSON string into `out` (requests_curl_line.c)
void cli_requests_curl_write(const CliRequestsOutCtx *args);

// the request at line `line` of the file at `path`, whose text is `size` bytes of `text`, made ready with the variables
// of the file and of environment `env` into `sent`, its body read into `body`, a line the sent request's body then
// points into, and its key into `key` where it is not NULL: 1, or 0 with why given (requests_ready.c)
int cli_requests_ready(const CliRequestsTextCtx *args);

// `size` bytes of `text` read as Rust reads an unsigned integer of base `base`, 10 or 16, a `+` let before its digits,
// into `value`: 1, or 0 where it is no such integer or is above `most`; whether `size` bytes of `text` contain
// `needle`, written in lower case, ASCII case aside; an address taken apart into `address`: 1, or 0 with why given; the
// address `location` names, read from the address `base` it was given at, into `out`; and a header of an answer by its
// name, ASCII case aside, NULL where it has none (requests_url.c)
int cli_requests_number(const CliRequestsTextSizeBaseCtx *args);
int cli_requests_contains(const CliRequestsTextSizeNeedleCtx *args);
int cli_requests_address(const CliRequestsErrMsgUrlCtx *args);
void cli_requests_join(const CliRequestsOutBaseCtx *args);
const CliRequestsText *cli_requests_header(const CliRequestsNameAnswerCtx *args);

// `size` bytes of `text` stored in the pool of the answer's head into `text_span`, and a line of the answer's head
// stored as a header, its name and value trimmed, a line with no colon passed over: each 1, or 0 where the head is
// full; the head of the hop to `address` written into `out`, a Host, User-Agent, Accept, Accept-Encoding,
// Content-Length and Connection given where the request gives none; the answer's body of `size` bytes in `raw` undone
// where its Content-Encoding is gzip or deflate, the body and its size set: 1, or 0 with why given; and whether the
// answer's body is text by its Content-Type, else by its bytes (requests_head.c)
int cli_requests_head_store(const CliRequestsTextSizeAnswerCtx *args);
int cli_requests_head_line(const CliRequestsSizeAnswerCtx *args);
void cli_requests_head_write(const CliRequestsOutHopCtx *args);
int cli_requests_decode(const CliRequestsSizeErrMsgCtx *args);
int cli_requests_is_text(const CliRequestsNameAnswerCtx *args);

// a work state of the two a request is sent in taken, waiting while both are in use, and given back; and `sent` sent
// and its answer read into the work state's, following redirects where it does, the peer's certificate checked where
// `verify` is 1: an answer that reports what failed where nothing answered (requests_send.c)
CliRequestsWorkState *cli_requests_work_state_alloc(void);
void cli_requests_work_state_free(const CliRequestsSizeErrMsgCtx *args);

// an answer written as serde writes it, with what was sent, its body cut to `most` bytes of its text at a letter's
// start, `cut` then true; and the answer stored first among the answers of the request named `key` of the file at
// `path`, its body cut to CLI_REQUESTS_HISTORY_BODY_MAX, and the last CLI_REQUESTS_HISTORY_MAX of them only: 1, or 0
// with why given (requests_history.c)
void cli_requests_answer_write(const CliRequestsOutCtx *args);
int cli_requests_history_save(const CliRequestsErrMsgCtx *args);

// the bytes of a curl line read and the most words it contains
#define CLI_REQUESTS_CURL_TEXT (1u << 22u)
#define CLI_REQUESTS_CURL_WORDS 65536u

// The words of a shell line, each stored in the pool with a 0 after it
typedef struct
{
    CliRequestsText word[CLI_REQUESTS_CURL_WORDS];
    unsigned int count;
    char text[CLI_REQUESTS_CURL_TEXT];
    unsigned long long used;
} CliRequestsWords;

// the words of `size` bytes of a shell line as bash reads them into `words`: 1, or 0 with why given; and `size` bytes
// of a line copied for cmd as bash reads it, each caret taking the letter after it as it is and a caret at the end of a
// line joining it to the next, into `out` (requests_words.c)
int cli_requests_words(const CliRequestsSizeCtx *args);
void cli_requests_uncareted(const CliRequestsSizeOutCtx *args);

struct CliRequestsTextCtx
{
    const char *text;
    unsigned long long size;
    unsigned long long line_u64;
    VerbaLinea *err_msg;
    const char *root;
    const char *path;
    const char *env;
    CliRequestsSent *sent_out;
    VerbaLinea *body;
    VerbaLinea *key;
};

struct CliRequestsErrMsgCtx
{
    VerbaLinea *err_msg;
    const char *root;
    const char *path;
    const CliRequestsSent *sent;
    const CliRequestsAnswer *answer;
    const char *key_text;
    unsigned long long key_size;
};

struct CliRequestsTextSizeCtx
{
    const char *text;
    unsigned long long size;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    const CliRequestsValues *values_read;
};

struct CliRequestsTextSizeBaseCtx
{
    const char *text;
    unsigned long long size;
    unsigned int base;
    unsigned long long most;
    unsigned long long *value_u64s;
};

struct CliRequestsNameCtx
{
    const char *name;
    CliRequestsValues *values;
    unsigned long long name_size;
    const char *value;
    unsigned long long value_size;
};

struct CliRequestsErrMsgRootCtx
{
    VerbaLinea *err_msg;
    const char *root;
    const char *path;
    const char *env;
    CliRequestsValues *values;
};

struct CliRequestsOutCtx
{
    VerbaLinea *out;
    const CliRequestsSent *sent;
    unsigned long long most;
    const CliRequestsAnswer *answer;
};

struct CliRequestsTextSizeAnswerCtx
{
    const char *text;
    unsigned long long size;
    CliRequestsAnswer *answer_out;
    CliRequestsText *text_span;
};

struct CliRequestsRootCtx
{
    const char *root;
    const char *path;
    const char *name;
    char *found;
};

struct CliRequestsSizeCtx
{
    unsigned long long size;
    VerbaLinea *err_msg;
    const char *line;
    CliRequestsWords *words;
};

struct CliRequestsErrMsgUrlCtx
{
    VerbaLinea *err_msg;
    const char *url;
    CliRequestsAddress *address;
};

struct CliRequestsTextSizeNeedleCtx
{
    const char *text;
    unsigned long long size;
    const char *needle;
};

struct CliRequestsSizeErrMsgCtx
{
    unsigned long long size;
    VerbaLinea *err_msg;
    CliRequestsWorkState *work_state;
};

struct CliRequestsSizeAnswerCtx
{
    unsigned long long size;
    CliRequestsAnswer *answer_out;
    const char *line;
};

struct CliRequestsOutHopCtx
{
    VerbaLinea *out;
    const CliRequestsSent *hop;
    const CliRequestsAddress *address_read;
};

struct CliRequestsOutBaseCtx
{
    VerbaLinea *out;
    const char *base_text;
    const char *location;
};

struct CliRequestsParsedCtx
{
    const CliRequestsParsed *parsed_read;
    const CliRequest *request;
    VerbaLinea *out;
};

struct CliRequestsErrMsgWorkStateCtx
{
    VerbaLinea *err_msg;
    CliRequestsWorkState *work_state;
    int verify;
};

struct CliRequestsTextSizeParsedCtx
{
    const char *text;
    unsigned long long size;
    CliRequestsParsed *parsed;
};

struct CliRequestsSentCtx
{
    const CliRequestsSent *sent;
    CliRequestsWorkState *work_state;
    int verify;
};

struct CliRequestsTextStartCtx
{
    const char *text;
    unsigned long long *start;
    unsigned long long *end;
};

struct CliRequestsSizeOutCtx
{
    unsigned long long size;
    VerbaLinea *out;
    const char *line;
};

struct CliRequestsSizeNameCtx
{
    unsigned long long size;
    const char *name;
    const CliRequestsValues *values_read;
};

struct CliRequestsTextAtCtx
{
    const char *text;
    unsigned long long at;
    unsigned long long end_value;
};

struct CliRequestsCtx
{
    const CliRequestsParsed *parsed_read;
    unsigned long long line_u64;
};

struct CliRequestsNameAnswerCtx
{
    const char *name;
    const CliRequestsAnswer *answer;
};

struct CliRequestsOutSecondsCtx
{
    VerbaLinea *out;
    unsigned long long seconds;
};

struct CliRequestsPoolCtx
{
    const char *pool;
    CliRequestsText text_requests_text;
};

typedef struct
{
    int (*parse)(const CliRequestsTextSizeParsedCtx *args);
    void (*parsed_free)(const CliRequestsTextSizeParsedCtx *args);
} CliRequestsTextSizeParsedNs;
EMBED_TABLE_LAYOUT(CliRequestsTextSizeParsedNs, parse, parsed_free);
EMBED_TABLE_STORAGE CliRequestsTextSizeParsedNs cli_requests_text_size_parsed_internal EMBED_UNUSED = {
    .parse = cli_requests_parse,
    .parsed_free = cli_requests_parsed_free,
};

typedef struct
{
    const CliRequest *(*at)(const CliRequestsCtx *args);
} CliRequestsNsTable;
EMBED_TABLE_LAYOUT(CliRequestsNsTable, at);
EMBED_TABLE_STORAGE CliRequestsNsTable cli_requests_internal EMBED_UNUSED = {
    .at = cli_requests_at,
};

typedef struct
{
    const char *(*text)(const CliRequestsPoolCtx *args);
} CliRequestsPoolNs;
EMBED_TABLE_LAYOUT(CliRequestsPoolNs, text);
EMBED_TABLE_STORAGE CliRequestsPoolNs cli_requests_pool_internal EMBED_UNUSED = {
    .text = cli_requests_text,
};

typedef struct
{
    void (*trim)(const CliRequestsTextStartCtx *args);
} CliRequestsTextStartNs;
EMBED_TABLE_LAYOUT(CliRequestsTextStartNs, trim);
EMBED_TABLE_STORAGE CliRequestsTextStartNs cli_requests_text_start_internal EMBED_UNUSED = {
    .trim = cli_requests_trim,
};

typedef struct
{
    unsigned long long (*white_at)(const CliRequestsTextAtCtx *args);
} CliRequestsTextAtNs;
EMBED_TABLE_LAYOUT(CliRequestsTextAtNs, white_at);
EMBED_TABLE_STORAGE CliRequestsTextAtNs cli_requests_text_at_internal EMBED_UNUSED = {
    .white_at = cli_requests_white_at,
};

typedef struct
{
    void (*full_err)(const CliRequestsTextCtx *args);
    int (*ready)(const CliRequestsTextCtx *args);
} CliRequestsTextNsTable;
EMBED_TABLE_LAYOUT(CliRequestsTextNsTable, full_err, ready);
EMBED_TABLE_STORAGE CliRequestsTextNsTable cli_requests_text_internal EMBED_UNUSED = {
    .full_err = cli_requests_full_err,
    .ready = cli_requests_ready,
};

typedef struct
{
    void (*key)(const CliRequestsParsedCtx *args);
} CliRequestsParsedNs;
EMBED_TABLE_LAYOUT(CliRequestsParsedNs, key);
EMBED_TABLE_STORAGE CliRequestsParsedNs cli_requests_parsed_internal EMBED_UNUSED = {
    .key = cli_requests_key,
};

typedef struct
{
    int (*nearest)(const CliRequestsRootCtx *args);
} CliRequestsRootNs;
EMBED_TABLE_LAYOUT(CliRequestsRootNs, nearest);
EMBED_TABLE_STORAGE CliRequestsRootNs cli_requests_root_internal EMBED_UNUSED = {
    .nearest = cli_requests_nearest,
};

typedef struct
{
    int (*values)(const CliRequestsErrMsgRootCtx *args);
    void (*values_free)(const CliRequestsErrMsgRootCtx *args);
    int (*verifies)(const CliRequestsErrMsgRootCtx *args);
} CliRequestsErrMsgRootNs;
EMBED_TABLE_LAYOUT(CliRequestsErrMsgRootNs, values, values_free, verifies);
EMBED_TABLE_STORAGE CliRequestsErrMsgRootNs cli_requests_err_msg_root_internal EMBED_UNUSED = {
    .values = cli_requests_values,
    .values_free = cli_requests_values_free,
    .verifies = cli_requests_verifies,
};

typedef struct
{
    int (*value_append)(const CliRequestsNameCtx *args);
} CliRequestsNameNs;
EMBED_TABLE_LAYOUT(CliRequestsNameNs, value_append);
EMBED_TABLE_STORAGE CliRequestsNameNs cli_requests_name_internal EMBED_UNUSED = {
    .value_append = cli_requests_value_append,
};

typedef struct
{
    const CliRequestsText *(*value)(const CliRequestsSizeNameCtx *args);
} CliRequestsSizeNameNs;
EMBED_TABLE_LAYOUT(CliRequestsSizeNameNs, value);
EMBED_TABLE_STORAGE CliRequestsSizeNameNs cli_requests_size_name_internal EMBED_UNUSED = {
    .value = cli_requests_value,
};

typedef struct
{
    int (*fill)(const CliRequestsTextSizeCtx *args);
} CliRequestsTextSizeNs;
EMBED_TABLE_LAYOUT(CliRequestsTextSizeNs, fill);
EMBED_TABLE_STORAGE CliRequestsTextSizeNs cli_requests_text_size_internal EMBED_UNUSED = {
    .fill = cli_requests_fill,
};

typedef struct
{
    void (*iso)(const CliRequestsOutSecondsCtx *args);
} CliRequestsOutSecondsNs;
EMBED_TABLE_LAYOUT(CliRequestsOutSecondsNs, iso);
EMBED_TABLE_STORAGE CliRequestsOutSecondsNs cli_requests_out_seconds_internal EMBED_UNUSED = {
    .iso = cli_requests_iso,
};

typedef struct
{
    void (*curl_write)(const CliRequestsOutCtx *args);
    void (*answer_write)(const CliRequestsOutCtx *args);
} CliRequestsOutNs;
EMBED_TABLE_LAYOUT(CliRequestsOutNs, curl_write, answer_write);
EMBED_TABLE_STORAGE CliRequestsOutNs cli_requests_out_internal EMBED_UNUSED = {
    .curl_write = cli_requests_curl_write,
    .answer_write = cli_requests_answer_write,
};

typedef struct
{
    int (*number)(const CliRequestsTextSizeBaseCtx *args);
} CliRequestsTextSizeBaseNs;
EMBED_TABLE_LAYOUT(CliRequestsTextSizeBaseNs, number);
EMBED_TABLE_STORAGE CliRequestsTextSizeBaseNs cli_requests_text_size_base_internal EMBED_UNUSED = {
    .number = cli_requests_number,
};

typedef struct
{
    int (*contains)(const CliRequestsTextSizeNeedleCtx *args);
} CliRequestsTextSizeNeedleNs;
EMBED_TABLE_LAYOUT(CliRequestsTextSizeNeedleNs, contains);
EMBED_TABLE_STORAGE CliRequestsTextSizeNeedleNs cli_requests_text_size_needle_internal EMBED_UNUSED = {
    .contains = cli_requests_contains,
};

typedef struct
{
    int (*address)(const CliRequestsErrMsgUrlCtx *args);
} CliRequestsErrMsgUrlNs;
EMBED_TABLE_LAYOUT(CliRequestsErrMsgUrlNs, address);
EMBED_TABLE_STORAGE CliRequestsErrMsgUrlNs cli_requests_err_msg_url_internal EMBED_UNUSED = {
    .address = cli_requests_address,
};

typedef struct
{
    void (*join)(const CliRequestsOutBaseCtx *args);
} CliRequestsOutBaseNs;
EMBED_TABLE_LAYOUT(CliRequestsOutBaseNs, join);
EMBED_TABLE_STORAGE CliRequestsOutBaseNs cli_requests_out_base_internal EMBED_UNUSED = {
    .join = cli_requests_join,
};

typedef struct
{
    const CliRequestsText *(*header)(const CliRequestsNameAnswerCtx *args);
    int (*is_text)(const CliRequestsNameAnswerCtx *args);
} CliRequestsNameAnswerNs;
EMBED_TABLE_LAYOUT(CliRequestsNameAnswerNs, header, is_text);
EMBED_TABLE_STORAGE CliRequestsNameAnswerNs cli_requests_name_answer_internal EMBED_UNUSED = {
    .header = cli_requests_header,
    .is_text = cli_requests_is_text,
};

typedef struct
{
    int (*head_store)(const CliRequestsTextSizeAnswerCtx *args);
} CliRequestsTextSizeAnswerNs;
EMBED_TABLE_LAYOUT(CliRequestsTextSizeAnswerNs, head_store);
EMBED_TABLE_STORAGE CliRequestsTextSizeAnswerNs cli_requests_text_size_answer_internal EMBED_UNUSED = {
    .head_store = cli_requests_head_store,
};

typedef struct
{
    int (*head_line)(const CliRequestsSizeAnswerCtx *args);
} CliRequestsSizeAnswerNs;
EMBED_TABLE_LAYOUT(CliRequestsSizeAnswerNs, head_line);
EMBED_TABLE_STORAGE CliRequestsSizeAnswerNs cli_requests_size_answer_internal EMBED_UNUSED = {
    .head_line = cli_requests_head_line,
};

typedef struct
{
    void (*head_write)(const CliRequestsOutHopCtx *args);
} CliRequestsOutHopNs;
EMBED_TABLE_LAYOUT(CliRequestsOutHopNs, head_write);
EMBED_TABLE_STORAGE CliRequestsOutHopNs cli_requests_out_hop_internal EMBED_UNUSED = {
    .head_write = cli_requests_head_write,
};

typedef struct
{
    int (*decode)(const CliRequestsSizeErrMsgCtx *args);
    void (*capacity_free)(const CliRequestsSizeErrMsgCtx *args);
} CliRequestsSizeErrMsgNs;
EMBED_TABLE_LAYOUT(CliRequestsSizeErrMsgNs, decode, capacity_free);
EMBED_TABLE_STORAGE CliRequestsSizeErrMsgNs cli_requests_size_err_msg_internal EMBED_UNUSED = {
    .decode = cli_requests_decode,
    .capacity_free = cli_requests_work_state_free,
};

typedef struct
{
    int (*history_save)(const CliRequestsErrMsgCtx *args);
} CliRequestsErrMsgNs;
EMBED_TABLE_LAYOUT(CliRequestsErrMsgNs, history_save);
EMBED_TABLE_STORAGE CliRequestsErrMsgNs cli_requests_err_msg_internal EMBED_UNUSED = {
    .history_save = cli_requests_history_save,
};

typedef struct
{
    int (*words)(const CliRequestsSizeCtx *args);
} CliRequestsSizeNsTable;
EMBED_TABLE_LAYOUT(CliRequestsSizeNsTable, words);
EMBED_TABLE_STORAGE CliRequestsSizeNsTable cli_requests_size_internal EMBED_UNUSED = {
    .words = cli_requests_words,
};

typedef struct
{
    void (*uncareted)(const CliRequestsSizeOutCtx *args);
} CliRequestsSizeOutNs;
EMBED_TABLE_LAYOUT(CliRequestsSizeOutNs, uncareted);
EMBED_TABLE_STORAGE CliRequestsSizeOutNs cli_requests_size_out_internal EMBED_UNUSED = {
    .uncareted = cli_requests_uncareted,
};

#endif
