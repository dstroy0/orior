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
// file is the reader's alone: orior_cli adds its name to the tree's .gitignore as it makes it.
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
typedef struct CliRequestsTextCfg CliRequestsTextCfg;
typedef struct CliRequestsAddressCfg CliRequestsAddressCfg;
typedef struct CliRequestsCfg CliRequestsCfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

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

typedef struct
{
    int (*from_curl)(const CliRequestsCfg *args);
} CliRequestsNs;
EMBED_TABLE_LAYOUT(CliRequestsNs, from_curl);
EMBED_TABLE_STORAGE CliRequestsNs cli_requests_table EMBED_UNUSED = {
    .from_curl = cli_requests_from_curl,
};

#endif
