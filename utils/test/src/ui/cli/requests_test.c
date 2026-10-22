// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_test.c: Requests' check: a file's requests are read with their names, lines, headers and bodies; variables
// come from the file and the environment chosen, the private values over the shared; the private file is made beside
// the file and named in the .gitignore; a curl line becomes a request and a request a curl line; and each request keeps
// its last answers, the last first, orior's own folder a folder of the run's
#include "../../../../../ui/cli/src/requests/requests_internal.h"
#include "cli_check.h"

static const char s_file[] =
    "@base = http://{{host}}/v1\n\n### List them\nGET {{base}}/items\n    ?page=2\nAccept: application/json\n# a "
    "comment\n\n###\n# @name make\n# @no-redirect\nPOST {{base}}/items\nContent-Type: application/json\n\n{\"name\": "
    "\"{{who}}\"}\n\n> {% client.test(); %}\n\n### From a file\nhttp://example.com/upload\n\n< ./body.json\n";

static CliRequestsParsed s_parsed;
static CliRequestsValues s_values;
static CliRequestsSent s_sent;
static CliRequestsAnswer s_answer;
static char s_body[1u << 16u];
static char s_out[16u << 20u];
static char s_big[CLI_REQUESTS_KEPT_BODY + 10u];

// a text of the parsed file's pool
static const char *requests_text(CliRequestsText text)
{
    return cli_requests_text(s_parsed.text, text);
}

// the key of a request of the parsed file into `key`, which holds `room`
static const char *requests_key(const CliRequest *request, char *key, unsigned long long room)
{
    CliLine line = {key, room, 0ull};
    cli_requests_key(&s_parsed, request, &line);
    (void)cli_line_closed(&line);
    return key;
}

// a file's requests are read with their names, lines, headers and bodies
static void requests_parsed(void)
{
    char key[256];
    cli_check(cli_requests_parse(s_file, sizeof(s_file) - 1u, &s_parsed) && (s_parsed.count == 3u) &&
                  (s_parsed.variables == 1u),
              "a file's three requests are read");
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = requests_text(s_parsed.variable_name[0]), .other = "base", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = requests_text(s_parsed.variable_value[0]),
                                                .other = "http://{{host}}/v1",
                                                .cap = CLI_CELLS_POOL})),
              "and the variable it sets");
    const CliRequest *const list = &s_parsed.requests[0];
    const CliRequest *const make = &s_parsed.requests[1];
    const CliRequest *const upload = &s_parsed.requests[2];
    cli_check((cellul.eq(
                  &(CatenaFinitaCfg){.src = requests_text(list->name), .other = "List them", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(
                      &(CatenaFinitaCfg){.src = requests_text(list->method), .other = "GET", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = requests_text(list->url), .other = "{{base}}/items?page=2", .cap = CLI_CELLS_POOL})) &&
                  (list->line == 3ull),
              "a request's name, method, address run on and line");
    cli_check(
        (list->headers == 1u) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = requests_text(list->header_name[0]), .other = "Accept", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = requests_text(list->header_value[0]), .other = "application/json", .cap = CLI_CELLS_POOL})) &&
            !list->has_body,
        "its headers, and no body");
    cli_check(
        (cellul.eq(&(CatenaFinitaCfg){.src = requests_text(make->name), .other = "make", .cap = CLI_CELLS_POOL})) &&
            !make->redirect && make->has_body &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = requests_text(make->body), .other = "{\"name\": \"{{who}}\"}", .cap = CLI_CELLS_POOL})),
        "a request named, kept from redirects, with its body");
    cli_check(
        (cellul.eq(&(CatenaFinitaCfg){.src = requests_text(upload->method), .other = "GET", .cap = CLI_CELLS_POOL})) &&
            upload->has_body_file &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = requests_text(upload->body_file), .other = "./body.json", .cap = CLI_CELLS_POOL})),
        "a body read from a file");
    cli_check(cli_requests_at(&s_parsed, 14ull) == make, "the request a line is in");
    cli_check(cli_requests_at(&s_parsed, 0ull) == list, "above the first, the first");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = requests_key(make, key, sizeof(key)), .other = "make", .cap = CLI_CELLS_POOL}),
              "a request's key is its name");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = requests_key(upload, key, sizeof(key)), .other = "From a file", .cap = CLI_CELLS_POOL}),
              "from its ### line too");
    static const char s_bare[] = "http://example.com/upload";
    (void)cli_requests_parse(s_bare, sizeof(s_bare) - 1u, &s_parsed);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = requests_key(&s_parsed.requests[0], key, sizeof(key)),
                                           .other = "GET http://example.com/upload",
                                           .cap = CLI_CELLS_POOL}),
              "one with no name by its method and address");
    cli_check((s_parsed.requests_room == 1u) && (s_parsed.text_room <= (2u * sizeof(s_bare)) + 64u),
              "a file read holds as many requests as its parts and texts as twice its bytes");
    cli_requests_parsed_given(&s_parsed);
}

// the folder `name` of the run's, its api/v1 made, into `root`
static void requests_root(const char *name, char *root)
{
    char folder[CLI_PATH_LONGEST];
    cli_check_folder(name, root);
    cli_full(root, "api/v1", folder);
    (void)query_folder_made(folder);
}

// a file of the tree `root` written
static void requests_written(const char *root, const char *name, const char *text)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, name, path);
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the value `name` of s_values, "" where it has none
static const char *requests_value(const char *name)
{
    const CliRequestsText *const value =
        cli_requests_value(&s_values, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    return (value != NULL) ? cli_requests_text(s_values.text, *value) : "";
}

// variables come from the file and the environment chosen, the private values over the shared
static void requests_values(void)
{
    char root[CLI_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    requests_root("requests_values", root);
    requests_written(root, "api/" CLI_REQUESTS_PUBLIC,
                     "{\"$shared\": {\"who\": \"all\"}, \"dev\": {\"host\": \"localhost:1\", \"token\": \"none\"}, "
                     "\"prod\": {\"host\": \"example.com\"}}");
    requests_written(root, CLI_REQUESTS_PRIVATE, "{\"dev\": {\"token\": \"secret\"}}");
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_requests_environments(root, "api/v1/calls.http", &out, &said), "the environments are read");
    cli_check_text(s_out, out.at,
                   "{\"names\":[\"dev\",\"prod\"],\"shared\":\"api/" CLI_REQUESTS_PUBLIC
                   "\",\"private\":\"" CLI_REQUESTS_PRIVATE "\"}",
                   "their names, and the files they come from");
    cli_check(
        cli_requests_values(root, "api/v1/calls.http", "dev", &s_values, &said) &&
            (cellul.eq(
                &(CatenaFinitaCfg){.src = requests_value("host"), .other = "localhost:1", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = requests_value("token"), .other = "secret", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = requests_value("who"), .other = "all", .cap = CLI_CELLS_POOL})),
        "the private values over the shared, $shared to every environment");
    cli_requests_values_given(&s_values);
    cli_check((g_cli_requests_cells.holds == 0ull) && (g_cli_requests_cells.hw != 0ull),
              "the environments and their values are read in cells each given back");
    cli_check(cli_requests_verifies(root, "api/v1/calls.http", "dev"), "a certificate is checked");
    requests_written(root, CLI_REQUESTS_PRIVATE,
                     "{\"dev\": {\"token\": \"secret\", \"SSLConfiguration\": {\"verifyHostCertificate\": false}}}");
    cli_check(!cli_requests_verifies(root, "api/v1/calls.http", "dev") &&
                  cli_requests_verifies(root, "api/v1/calls.http", "prod"),
              "unless the environment says not to");
    char key[256];
    CliLine key_line = {key, sizeof(key), 0ull};
    CliLine body = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    cli_line_growing(&body, 4096ull, "a test request's body");
    said.at = 0ull;
    cli_check(cli_requests_ready(root, "api/v1/calls.http", s_file, sizeof(s_file) - 1u, 14ull, "dev", &s_sent, &body,
                                 &key_line, &said),
              "a request is made ready");
    cli_check_text(key, key_line.at, "make", "the request the line is in");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = s_sent.url, .other = "http://localhost:1/v1/items", .cap = CLI_CELLS_POOL}),
        "its address filled in");
    cli_check_text(s_sent.body, s_sent.body_size, "{\"name\": \"all\"}", "and its body");
    said.at = 0ull;
    cli_check(!cli_requests_ready(root, "api/v1/calls.http", s_file, sizeof(s_file) - 1u, 14ull, NULL, &s_sent, &body,
                                  NULL, &said) &&
                  (cli_line_closed(&said) != 0ull) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = why, .cap = CLI_CELLS_POOL, .other = "{{host}}", .other_cap = CLI_CELLS_POOL}) != NULL),
              "no environment, no host");
    cli_cell_given(body.out);
    static CliRequestsValues s_none;
    static const char s_dynamic[] = "{{$uuid}} {{$timestamp}}";
    out.at = 0ull;
    said.at = 0ull;
    cli_check(cli_requests_filled(s_dynamic, sizeof(s_dynamic) - 1u, &s_none, &out, &said) && (out.at > 40ull) &&
                  (s_out[14] == '4'),
              "a uuid of version 4 and a timestamp");
    out.at = 0ull;
    cli_requests_iso(0ull, &out);
    cli_check_text(s_out, out.at, "1970-01-01T00:00:00Z", "the time began");
    out.at = 0ull;
    cli_requests_iso(1760000000ull, &out);
    cli_check_text(s_out, out.at, "2025-10-09T08:53:20Z", "and a time since");
}

// the private file is made beside the file and named in the .gitignore
static void requests_private(void)
{
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    requests_root("requests_private", root);
    requests_written(root, "api/" CLI_REQUESTS_PUBLIC, "{\"local\": {}, \"stage\": {}}");
    requests_written(root, ".gitignore", "target");
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_requests_private_made(root, "api/calls.http", &out, &said), "the private file is made");
    cli_check_text(s_out, out.at, "api/" CLI_REQUESTS_PRIVATE, "beside the file");
    cli_full(root, "api/" CLI_REQUESTS_PRIVATE, path);
    unsigned long long size = qry_member_file_read(path, (unsigned char *)s_body, sizeof(s_body));
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_body, (size != QRY_MEMBER_UNREAD) ? size : 0ull, &read);
    out.at = 0ull;
    cli_json_write(json, read, &out, 0u, 0);
    cli_check_text(s_out, out.at, "{\"local\":{},\"stage\":{}}", "with the shared file's environments");
    cli_full(root, ".gitignore", path);
    size = qry_member_file_read(path, (unsigned char *)s_body, sizeof(s_body));
    cli_check_text(s_body, (size != QRY_MEMBER_UNREAD) ? size : 0ull, "target\n" CLI_REQUESTS_PRIVATE "\n",
                   "and named in the .gitignore");
    out.at = 0ull;
    cli_check(cli_requests_private_made(root, "api/calls.http", &out, &said), "made again");
    cli_check_text(s_out, out.at, "api/" CLI_REQUESTS_PRIVATE, "the one there is kept");
    size = qry_member_file_read(path, (unsigned char *)s_body, sizeof(s_body));
    cli_check_text(s_body, (size != QRY_MEMBER_UNREAD) ? size : 0ull, "target\n" CLI_REQUESTS_PRIVATE "\n",
                   "and named once");
}

// the request `line` sends, written as a request of a file, into s_out: 1, or 0 where it is no curl line
static int requests_from_curl(const char *line)
{
    char written[4096];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {written, sizeof(written), 0ull};
    if (!cli_requests_from_curl(line, cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}), &out, &said))
    {
        return 0;
    }
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(written, out.at, &read);
    const char *const text = cli_json_string(json, read, NULL);
    memor.cpy(&(MemoriaCfg){
        .dst = s_out,
        .src = (text != NULL) ? text : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (text != NULL) ? text : "", .cap = CLI_CELLS_POOL}) + 1u});
    return text != NULL;
}

// a curl line becomes a request and a request a curl line
static void requests_curl(void)
{
    cli_check(requests_from_curl("curl 'https://api.example.com/v1/items?x=1' \\\n  -H 'Accept: application/json' "
                                 "\\\n  -H \"Authorization: Bearer abc\" \\\n  --data-raw $'{\"a\":\"it\\'s\"}' \\\n "
                                 " --compressed") &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_out,
                      .other = "POST https://api.example.com/v1/items?x=1\nAccept: application/json\nAuthorization: "
                               "Bearer abc\nContent-Type: application/x-www-form-urlencoded\n\n{\"a\":\"it's\"}\n",
                      .cap = CLI_CELLS_POOL})),
              "a curl line becomes a request");
    cli_check(
        requests_from_curl("curl -G https://h/s -d q=a -d n=2") &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "GET https://h/s?q=a&n=2\n", .cap = CLI_CELLS_POOL})),
        "-G puts the data in the address");
    cli_check(
        requests_from_curl("curl -u me:pw -X DELETE http://h/x") &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = s_out, .other = "DELETE http://h/x\nAuthorization: Basic bWU6cHc=\n", .cap = CLI_CELLS_POOL})),
        "-u is basic authorization");
    cli_check(requests_from_curl("curl -F name=me -F file=@a.png http://h/up") &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_out,
                       .cap = CLI_CELLS_POOL,
                       .other = "Content-Disposition: form-data; name=\"file\"; filename=\"a.png\"\n\n< a.png\n",
                       .other_cap = CLI_CELLS_POOL}) != NULL),
              "-F is a form, a file read from beside it");
    cli_check(
        requests_from_curl("curl ^\n  \"http://h/x\" ^\n  -H ^\"A: b^\"") &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "GET http://h/x\nA: b\n", .cap = CLI_CELLS_POOL})),
        "a line copied for cmd");
    cli_check(!requests_from_curl("wget http://h"), "and no other program's");
    memor.set(&(MemoriaCfg){.dst = &s_sent, .val = 0, .bytes = sizeof(s_sent)});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.method,
                            .src = "PUT",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "PUT", .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.url,
                            .src = "http://h/x",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "http://h/x", .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.header_name[0],
                            .src = "A",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "A", .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.header_value[0],
                            .src = "it's",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "it's", .cap = CLI_CELLS_POOL}) + 1u});
    s_sent.headers = 1u;
    s_sent.body = "{}";
    s_sent.body_size = 2ull;
    char written[4096];
    CliLine out = {written, sizeof(written), 0ull};
    cli_requests_curl_written(&s_sent, &out);
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(written, out.at, &read);
    char back[4096];
    const char *const curl = cli_json_string(json, read, NULL);
    memor.cpy(&(MemoriaCfg){
        .dst = back,
        .src = (curl != NULL) ? curl : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (curl != NULL) ? curl : "", .cap = CLI_CELLS_POOL}) + 1u});
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = back,
                  .other = "curl \\\n  -X PUT \\\n  'http://h/x' \\\n  -H 'A: it'\\''s' \\\n  --data-raw '{}'",
                  .cap = CLI_CELLS_POOL}),
              "a request becomes a curl line");
    cli_check(requests_from_curl(back) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_out,
                      .other = "PUT http://h/x\nA: it's\nContent-Type: application/x-www-form-urlencoded\n\n{}\n",
                      .cap = CLI_CELLS_POOL})),
              "and back");
}

// the answers kept for the request at `line` of s_kept, read into the shared tree: the array of them
static unsigned int requests_answers(const char *root, unsigned long long line, const CliJson **json)
{
    static const char s_kept[] = "### make\nGET http://h/a\n\n### other\nGET http://h/b\n\n### none\nGET "
                                 "http://h/c\n\n### big\nGET http://h/d\n";
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    (void)cli_requests_answers(root, "calls.http", s_kept, sizeof(s_kept) - 1u, line, &out, &said);
    unsigned int read = CLI_JSON_NONE;
    *json = cli_check_parsed(s_out, out.at, &read);
    return cli_json_member(*json, read, "answers");
}

// an answer at `at` with `size` bytes of `body` kept under `key`: 1, or 0
static int requests_keep(const char *root, const char *key, unsigned long long at, const char *body,
                         unsigned long long size)
{
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    s_answer.at = at;
    s_answer.status = 200u;
    s_answer.body = (const unsigned char *)body;
    s_answer.size = size;
    return cli_requests_keep(root, "calls.http", key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}),
                             &s_answer, &s_sent, &said);
}

// each request keeps its last answers, the last first
static void requests_kept(void)
{
    char root[CLI_PATH_LONGEST];
    cli_check_folder("requests_kept", root);
    memor.set(&(MemoriaCfg){.dst = s_big, .val = 'x', .bytes = sizeof(s_big)});
    int kept = 1;
    for (unsigned int at = 0u; at < (CLI_REQUESTS_KEEP + 2u); at += 1u)
    {
        kept = kept && requests_keep(root, "make", at, s_big, (at == 0u) ? sizeof(s_big) : 0ull);
    }
    cli_check(kept && requests_keep(root, "other", 99ull, "", 0ull), "answers are kept");
    const CliJson *json = NULL;
    const unsigned int make = requests_answers(root, 1ull, &json);
    int order = (cli_json_is(json, make, CFG_JSON_ARRAY) && (json->node[make].count == CLI_REQUESTS_KEEP));
    for (unsigned int at = 0u; order && (at < CLI_REQUESTS_KEEP); at += 1u)
    {
        const unsigned int when = cli_json_member(json, cli_json_item(json, make, at), "at");
        char wanted[16];
        CliLine line = {wanted, sizeof(wanted), 0ull};
        cli_line_decimal(&line, CLI_REQUESTS_KEEP + 1u - at, 1u);
        order = (when != CLI_JSON_NONE) && (json->node[when].length == line.at) &&
                (memor.cmp(
                     &(MemoriaCfg){.src = json->text + json->node[when].text, .other = wanted, .bytes = line.at}) == 0);
    }
    cli_check(order, "the last of them, the last first");
    const unsigned int other = requests_answers(root, 4ull, &json);
    cli_check(json->node[other].count == 1u, "each request its own");
    const unsigned int none = requests_answers(root, 7ull, &json);
    cli_check(json->node[none].count == 0u, "and none where none were kept");
    memor.set(&(MemoriaCfg){.dst = s_big, .val = 'y', .bytes = sizeof(s_big)});
    cli_check(requests_keep(root, "big", 0ull, s_big, sizeof(s_big)), "a large answer is kept");
    const unsigned int answers = requests_answers(root, 10ull, &json);
    const unsigned int big = cli_json_item(json, answers, 0u);
    const unsigned int body = cli_json_member(json, big, "body");
    cli_check(cli_json_is(json, cli_json_member(json, big, "cut"), CFG_JSON_TRUE) &&
                  (json->node[body].length == CLI_REQUESTS_KEPT_BODY),
              "its body cut");
}

int main(void)
{
    char home[CLI_PATH_LONGEST];
    cli_check_folder("requests_home", home);
    (void)query_environment_set("ORIOR_HOME", home);
    requests_parsed();
    requests_values();
    requests_private();
    requests_curl();
    memor.set(&(MemoriaCfg){.dst = &s_sent, .val = 0, .bytes = sizeof(s_sent)});
    memor.cpy(&(MemoriaCfg){.dst = s_sent.method,
                            .src = "GET",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "GET", .cap = CLI_CELLS_POOL}) + 1u});
    s_sent.body = "";
    requests_kept();
    return cli_checked();
}
