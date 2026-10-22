// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_internal.h: what the requests sources share: a file of requests read, the variables a request is filled
// from, a request ready to go, and its answer
#ifndef REQUESTS_INTERNAL_H
#define REQUESTS_INTERNAL_H

#include "../command_line/cells.h"
#include "requests.h"

// the cells the requests are read, filled and answered in (requests_values.c)
extern CliCellsTally g_cli_requests_cells;

// the most requests a file holds that are read, headers a request holds, variables a file sets, values an environment
// gives, and the bytes their texts take; the most of an answer's body read, and of it kept with the answers; how many
// answers each request keeps; and the most redirects one request follows
#define CLI_REQUESTS_MOST 1024u
#define CLI_REQUESTS_HEADERS 64u
#define CLI_REQUESTS_VARIABLES 256u
#define CLI_REQUESTS_VALUES 512u
#define CLI_REQUESTS_TEXT (1u << 22u)
#define CLI_REQUESTS_BODY (32u << 20u)
#define CLI_REQUESTS_KEPT_BODY (1u << 20u)
#define CLI_REQUESTS_KEEP 10u
#define CLI_REQUESTS_REDIRECTS 10u

// the bytes of an address, of a header's name and value, and of an answer's reason and version
#define CLI_REQUESTS_ADDRESS 8192u
#define CLI_REQUESTS_NAME 256u
#define CLI_REQUESTS_VALUE 4096u

// A text kept in a pool: where it starts and its length, a 0 after it
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
// file's parts and the pool a cell sized to the file, each given back with cli_requests_parsed_given
typedef struct
{
    CliRequest *requests;
    unsigned int count;
    unsigned int requests_room;
    CliRequestsText variable_name[CLI_REQUESTS_VARIABLES];
    CliRequestsText variable_value[CLI_REQUESTS_VARIABLES];
    unsigned int variables;
    char *text;
    unsigned long long used;
    unsigned long long text_room;
    int full;
} CliRequestsParsed;

// The values a request's variables are filled from, a name to one value, and the pool of their texts, a cell whose
// room doubles as they are put, to CLI_REQUESTS_TEXT at the most, given back with cli_requests_values_given
typedef struct
{
    CliRequestsText name[CLI_REQUESTS_VALUES];
    CliRequestsText value[CLI_REQUESTS_VALUES];
    unsigned int count;
    char *text;
    unsigned long long used;
    unsigned long long room;
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

// the bytes of an answer's head kept and the most headers it holds; and the bytes a connection's reading holds, the
// longest line of a head and one read past it
#define CLI_REQUESTS_HEAD (1u << 20u)
#define CLI_REQUESTS_ANSWER_HEADERS 4096u
#define CLI_REQUESTS_READING ((64u << 10u) + (16u << 10u) + 1u)

// An answer: when it came, its address, its status, its reason, version and headers kept in the pool of its head, its
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
    char failed[CLI_SAID_LONGEST];
} CliRequestsAnswer;

// An address taken apart: whether it is https, its host, its port, and the path with its query
typedef struct
{
    int secure;
    char host[CLI_REQUESTS_ADDRESS];
    unsigned int port;
    char target[CLI_REQUESTS_ADDRESS + 1u];
} CliRequestsAddress;

// the bytes a request's body and an answer's body as read are begun in, each a growing line that doubles as it fills,
// the answer's to CLI_REQUESTS_BODY at the most
#define CLI_REQUESTS_BODY_FIRST (1u << 12u)

// The room a request is sent in: the request as it was made ready and its body, a growing line, the request as it
// goes on each hop and its address taken apart, its answer, the answer's body as read, a growing line, and as
// undone, a cell of `unpacked_room` taken as it is undone, and the bytes read from the connection not yet taken,
// from `start` to `end`
typedef struct
{
    CliRequestsSent sent;
    CliLine body;
    CliRequestsSent hop;
    CliRequestsAddress address;
    CliRequestsAnswer answer;
    CliLine raw;
    unsigned char *unpacked;
    unsigned long long unpacked_room;
    unsigned char reading[CLI_REQUESTS_READING];
    unsigned long long start;
    unsigned long long end;
} CliRequestsRoom;

// the lock a file of requests is read under, and the file read under it (requests_parse.c)
extern CliLock g_cli_requests_lock;
extern CliRequestsParsed g_cli_requests_parsed;

// a file of requests read into `parsed`, what it held before given back, the lock above held: 1, or 0 where it is
// larger than `parsed` holds or its cells are refused; its cells given back, `parsed` left empty; the request whose
// part holds line `line`, or the last before it, the first where none does: NULL where the file holds none; a text of
// the pool (requests_white.c); the part of a text from `start` to `end` with the white letters at its two ends passed
// over, as Rust's str::trim trims it; and the bytes of the white letter at `at` of a text ending at `end`, 0 where the
// letter there is not white (requests_parse.c, requests_white.c)
int cli_requests_parse(const char *text, unsigned long long size, CliRequestsParsed *parsed);
void cli_requests_parsed_given(CliRequestsParsed *parsed);
const CliRequest *cli_requests_at(const CliRequestsParsed *parsed, unsigned long long line);
const char *cli_requests_text(const char *pool, CliRequestsText text);
void cli_requests_trimmed(const char *text, unsigned long long *start, unsigned long long *end);
unsigned long long cli_requests_white_at(const char *text, unsigned long long at, unsigned long long end);
// the key a request's answers are kept under, its name, else its method and address as written, into `out`; and the
// reason a file of requests was not read whole (requests_written.c)
void cli_requests_full_said(CliLine *said);
void cli_requests_key(const CliRequestsParsed *parsed, const CliRequest *request, CliLine *out);

// the nearest file `name` beside the file at `path` or in a folder above it within the tree, into `found`: 1, or 0
// where there is none; the values of environment `env`, or of none, for the file at `path`: the shared file's then the
// private file's, `$shared` under the environment's own: 1, or 0 with why said; the values' pool given back and the
// values left empty; a value put, in place of one of its name: 1, or 0 where it does not fit or its room is refused; a
// name's value, NULL where it has none; and whether the peer's certificate is checked for environment `env`
// (requests_values.c)
int cli_requests_nearest(const char *root, const char *path, const char *name, char *found);
int cli_requests_values(const char *root, const char *path, const char *env, CliRequestsValues *values, CliLine *said);
void cli_requests_values_given(CliRequestsValues *values);
int cli_requests_value_put(CliRequestsValues *values, const char *name, unsigned long long name_size, const char *value,
                           unsigned long long value_size);
const CliRequestsText *cli_requests_value(const CliRequestsValues *values, const char *name, unsigned long long size);
int cli_requests_verifies(const char *root, const char *path, const char *env);
// `size` bytes of `text` with each `{{name}}` given its value, a value that names others given theirs in turn, eight
// deep, and one no variable sets given as `$uuid`, `$random.uuid`, `$timestamp`, `$isoTimestamp` and `$randomInt`
// name it, into `out`: 1, or 0 with why said (requests_fill.c)
int cli_requests_filled(const char *text, unsigned long long size, const CliRequestsValues *values, CliLine *out,
                        CliLine *said);
// the date and time `seconds` after 1970 began, as ISO 8601 in UTC, written into `out` (requests_fill.c)
void cli_requests_iso(unsigned long long seconds, CliLine *out);
// the curl line that sends `sent`, written inside a JSON string into `out` (requests_curl_line.c)
void cli_requests_curl_written(const CliRequestsSent *sent, CliLine *out);

// the request at line `line` of the file at `path`, whose text is `size` bytes of `text`, made ready with the variables
// of the file and of environment `env` into `sent`, its body read into `body`, a growing line the sent request's body
// then points into, and its key into `key` where it is not NULL: 1, or 0 with why said (requests_ready.c)
int cli_requests_ready(const char *root, const char *path, const char *text, unsigned long long size,
                       unsigned long long line, const char *env, CliRequestsSent *sent, CliLine *body, CliLine *key,
                       CliLine *said);

// `size` bytes of `text` read as Rust reads an unsigned integer of base `base`, 10 or 16, a `+` let before its digits,
// into `value`: 1, or 0 where it is no such integer or is above `most`; whether `size` bytes of `text` hold `needle`,
// written in lower case, ASCII case aside; an address taken apart into `address`: 1, or 0 with why said; the address
// `location` names, read from the address `base` it was given at, into `out`; and a header of an answer by its name,
// ASCII case aside, NULL where it has none (requests_url.c)
int cli_requests_number(const char *text, unsigned long long size, unsigned int base, unsigned long long most,
                        unsigned long long *value);
int cli_requests_holds(const char *text, unsigned long long size, const char *needle);
int cli_requests_address(const char *url, CliRequestsAddress *address, CliLine *said);
void cli_requests_joined(const char *base, const char *location, CliLine *out);
const CliRequestsText *cli_requests_header(const CliRequestsAnswer *answer, const char *name);

// `size` bytes of `text` kept in the pool of the answer's head into `kept`, and a line of the answer's head kept as a
// header, its name and value trimmed, a line with no colon passed over: each 1, or 0 where the head is full; the head
// of the hop to `address` written into `out`, a Host, User-Agent, Accept, Accept-Encoding, Content-Length and
// Connection given where the request gives none; the answer's body of `size` bytes in `raw` undone where its
// Content-Encoding is gzip or deflate, the body and its size set: 1, or 0 with why said; and whether the answer's body
// is text by its Content-Type, else by its bytes (requests_head.c)
int cli_requests_head_kept(CliRequestsAnswer *answer, const char *text, unsigned long long size, CliRequestsText *kept);
int cli_requests_head_line(CliRequestsAnswer *answer, const char *line, unsigned long long size);
void cli_requests_head_written(const CliRequestsSent *hop, const CliRequestsAddress *address, CliLine *out);
int cli_requests_decoded(CliRequestsRoom *room, unsigned long long size, CliLine *said);
int cli_requests_is_text(const CliRequestsAnswer *answer);

// the room's hop sent once, to its own address, the peer's certificate checked where `verify` is 1, and the whole
// answer read into the room's answer, its body read into `raw`, undone into `unpacked`, a cell taken for it, where it
// was packed: 1, or 0 with why said (requests_http.c)
int cli_requests_once(CliRequestsRoom *room, int verify, CliLine *said);

// a room of the two a request is sent in taken, waiting while both are in use, and given back; and `sent` sent and
// its answer read into the room's, following redirects where it does, the peer's certificate checked where `verify`
// is 1: an answer that says what failed where nothing answered (requests_send.c)
CliRequestsRoom *cli_requests_room_taken(void);
void cli_requests_room_given(CliRequestsRoom *room);
void cli_requests_send(const CliRequestsSent *sent, int verify, CliRequestsRoom *room);

// an answer written as serde writes it, with what was sent, its body cut to `most` bytes of its text at a letter's
// start, `cut` then true; and the answer kept first among the answers of the request named `key` of the file at
// `path`, its body cut to CLI_REQUESTS_KEPT_BODY, and the last CLI_REQUESTS_KEEP of them only: 1, or 0 with why said
// (requests_kept.c)
void cli_requests_answer_written(const CliRequestsAnswer *answer, const CliRequestsSent *sent, unsigned long long most,
                                 CliLine *out);
int cli_requests_keep(const char *root, const char *path, const char *key, unsigned long long key_size,
                      const CliRequestsAnswer *answer, const CliRequestsSent *sent, CliLine *said);

// the bytes of a curl line read and the most words it holds
#define CLI_REQUESTS_CURL_TEXT (1u << 22u)
#define CLI_REQUESTS_CURL_WORDS 65536u

// The words of a shell line, each kept in the pool with a 0 after it
typedef struct
{
    CliRequestsText word[CLI_REQUESTS_CURL_WORDS];
    unsigned int count;
    char text[CLI_REQUESTS_CURL_TEXT];
    unsigned long long used;
} CliRequestsWords;

// the words of `size` bytes of a shell line as bash reads them into `words`: 1, or 0 with why said; and `size` bytes
// of a line copied for cmd as bash reads it, each caret taking the letter after it as it is and a caret at the end of a
// line joining it to the next, into `out` (requests_words.c)
int cli_requests_words(const char *line, unsigned long long size, CliRequestsWords *words, CliLine *said);
void cli_requests_uncareted(const char *line, unsigned long long size, CliLine *out);

#endif
