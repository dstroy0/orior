// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_test.c: the server's check: base64 round-trips every length, a name of more than one word is found as the page
// writes it, and calls over lines are answered by id
#include "../../../../../ui/cli/src/text/base64.h"
#include "../../../../../ui/cli/src/window/serve_internal.h"
#include "cli_check.h"

static const char s_input[] = "{\"id\":1,\"name\":\"file_read\",\"args\":{\"path\":\"a.txt\"}}\n"
                              "{\"id\":2,\"name\":\"nothing_at_all\",\"args\":{}}\n"
                              "{\"id\":3,\"name\":\"file_write\",\"args\":{\"path\":\"b.txt\",\"text\":\"two\\n\"}}\n";
static unsigned long long s_read;
static char s_heard[1u << 20u];
static unsigned long long s_heard_at;
static CliLock s_lock = CLI_LOCK_INIT;

// base64 round-trips every length
static void serve_base64(void)
{
    int round = 1;
    for (unsigned int length = 0u; length < 40u; length += 1u)
    {
        unsigned char bytes[40];
        for (unsigned int at = 0u; at < length; at += 1u)
        {
            // a byte of the pattern, its low eight bits kept
            bytes[at] = (unsigned char)((at * 37u + 11u) & 0xffu);
        }
        char text[64];
        CliLine line = {text, sizeof(text), 0ull};
        cli_base64(&line, bytes, length);
        unsigned char back[40];
        unsigned long long size = 0ull;
        round &= cli_unbase64(text, line.at, back, sizeof(back), &size) && (size == length) &&
                 (memor.cmp(&(MemoriaCfg){.src = back, .other = bytes, .bytes = length}) == 0);
    }
    cli_check(round, "base64 round-trips every length");
    char text[16];
    CliLine line = {text, sizeof(text), 0ull};
    cli_base64(&line, (const unsigned char *)"orior", 5ull);
    cli_check_text(text, line.at, "b3Jpb3I=", "and writes the standard's letters");
}

// a name of more than one word is found as the page writes it
static void serve_names(void)
{
    static const char s_args[] = "{\"withErrors\": true, \"path\": \"a\"}";
    unsigned int object = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_args, sizeof(s_args) - 1u, &object);
    const CliServeArgs args = {json, object};
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    int with_errors = 0;
    cli_check(cli_serve_bool(&args, "with_errors", &with_errors, &said) && with_errors, "with_errors is withErrors");
    const char *const path = cli_serve_string(&args, "path", &said);
    cli_check((path != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = "a", .cap = CLI_CELLS_POOL})),
              "a name of one word is itself");
    cli_check(cli_serve_arg(&args, "repo") == CLI_JSON_NONE, "a name not given is none");
}

// a line of the input read, QRY_MEMBER_UNREAD once it ends
static unsigned long long serve_line_read(void *context, char *line, unsigned long long room)
{
    (void)context;
    const unsigned long long size = sizeof(s_input) - 1u;
    if (s_read >= size)
    {
        return QRY_MEMBER_UNREAD;
    }
    const char *const end = memor.chr(&(MemoriaCfg){.src = s_input + s_read, .val = '\n', .bytes = size - s_read});
    // a line ends inside the input, at or past where the read stands
    const unsigned long long length = (end != NULL) ? (unsigned long long)(end - (s_input + s_read)) : size - s_read;
    const unsigned long long kept = (length < room) ? length : room;
    memor.cpy(&(MemoriaCfg){.dst = line, .src = s_input + s_read, .bytes = kept});
    s_read += length + 1ull;
    return kept;
}

// what the server writes kept
static void serve_written(void *context, const char *bytes, unsigned long long size)
{
    (void)context;
    cli_lock_take(&s_lock);
    if (s_heard_at + size < sizeof(s_heard))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_heard + s_heard_at, .src = bytes, .bytes = size});
        s_heard_at += size;
    }
    cli_lock_give(&s_lock);
}

// the line the server wrote that answers `id`, or, where `id` is 0, the first line: its node in the shared tree,
// CLI_JSON_NONE where none does
static unsigned int serve_line(unsigned int id, const CliJson **json)
{
    for (unsigned long long at = 0ull; at < s_heard_at;)
    {
        const char *const end = memor.chr(&(MemoriaCfg){.src = s_heard + at, .val = '\n', .bytes = s_heard_at - at});
        // a line ends inside what was heard, at or past its start
        const unsigned long long length = (end != NULL) ? (unsigned long long)(end - (s_heard + at)) : s_heard_at - at;
        unsigned int line = CLI_JSON_NONE;
        *json = cli_check_parsed(s_heard + at, length, &line);
        at += length + 1ull;
        const unsigned int number = cli_json_member(*json, line, "id");
        const int answers = cli_json_is(*json, number, CFG_JSON_NUMBER) && ((*json)->node[number].length == 1u) &&
                            ((unsigned int)((*json)->text[(*json)->node[number].text] - '0') == id);
        if ((id == 0u) || answers)
        {
            return line;
        }
    }
    return CLI_JSON_NONE;
}

// calls over lines are answered by id
static void serve_lines(void)
{
    char dir[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("serve", dir);
    cli_full(dir, "src/cu/engine", path);
    (void)query_folder_made(path);
    cli_full(dir, "src/cu/engine/engine_config.h", path);
    (void)query_file_written(path, "// marks the tree\n", 18ull);
    cli_full(dir, "a.txt", path);
    (void)query_file_written(path, "one\n", 4ull);
    const CliServeLink link = {serve_line_read, serve_written, NULL};
    cli_serve_over(dir, &link);
    const CliJson *json = NULL;
    unsigned int line = serve_line(0u, &json);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(line, "event"), .other = "serving", .cap = CLI_CELLS_POOL}),
              "the server says it serves first");
    line = serve_line(1u, &json);
    cli_check(cellul.starts(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_member(json, line, "ok"), "text"),
                                               .other = "one",
                                               .cap = CLI_CELLS_POOL}),
              "a call is answered with what it read");
    line = serve_line(2u, &json);
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = cli_check_member_text(line, "error"),
                                             .cap = CLI_CELLS_POOL,
                                             .other = "nothing_at_all",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "a call of no name says so");
    line = serve_line(3u, &json);
    cli_check(cli_json_is(json, cli_json_member(json, line, "ok"), CFG_JSON_NULL),
              "a call that answers nothing is null");
    char text[16];
    cli_full(dir, "b.txt", path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)text, sizeof(text));
    cli_check_text(text, (size == QRY_MEMBER_UNREAD) ? 0ull : size, "two\n", "and what it writes is written");
}

int main(void)
{
    serve_base64();
    serve_names();
    serve_lines();
    return cli_checked();
}
