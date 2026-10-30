// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// checkers_read->c: the findings a checker wrote read as the protocol's diagnostics, by file: a finding a line, or the
// JSON of ruff, ESLint and ShellCheck
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "checkers_internal.h"

// the most bytes of a finding's fixes
#define CLI_CHECK_FIXES (1u << 16u)

// A check being read, under the reading's lock: the checker's name, the tree, the file `-` stands for, the findings,
// and the tree a checker's JSON is read into, in a cell sized to that JSON and given back as the reading ends
typedef struct
{
    const char *name;
    const char *root;
    const char *path;
    CliFindings *findings;
    CliJson json;
    char fixes[CLI_CHECK_FIXES];
} CliCheckRead;

static CliCheckRead s_room;
static CliCheckRead *s_read;

// a path's key: with forward slashes, in lower case
static void cli_key(const char *path, char *key)
{
    memor.cpy(&(MemoriaCfg){
        .dst = key, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = key; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : (((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0]);
    }
}

unsigned int cli_checkers_file(CliFindings *findings, const char *path, int first)
{
    char key[CLI_PATH_MAX_LEN];
    cli_key(path, key);
    for (unsigned int at = 0u; at < findings->files; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = findings->key[at], .other = key, .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    if (findings->files >= CLI_CHECK_FILES)
    {
        return CLI_CHECK_FILES;
    }
    if (!first)
    {
        memor.cpy(&(MemoriaCfg){.dst = findings->path[findings->files],
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = findings->key[findings->files],
                                .src = key,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
        findings->files += 1u;
        return findings->files - 1u;
    }
    for (unsigned int at = findings->files; at > 0u; at -= 1u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = findings->path[at],
            .src = findings->path[at - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = findings->path[at - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){
            .dst = findings->key[at],
            .src = findings->key[at - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = findings->key[at - 1u], .cap = CLI_CELLS_POOL}) + 1u});
    }
    for (unsigned int at = 0u; at < findings->findings; at += 1u)
    {
        findings->file_of[at] += 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = findings->path[0],
                            .src = path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = findings->key[0],
                            .src = key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    findings->files += 1u;
    return 0u;
}

unsigned int cli_checkers_file_of(const char *name, unsigned long long length)
{
    char given[CLI_PATH_MAX_LEN];
    const unsigned long long copied_len = (length < CLI_PATH_MAX_LEN) ? length : (CLI_PATH_MAX_LEN - 1ull);
    memor.cpy(&(MemoriaCfg){.dst = given, .src = name, .bytes = copied_len});
    given[copied_len] = '\0';
    const int input = (cellul.eq(&(CatenaFinitaCfg){.src = given, .other = "-", .cap = CLI_CELLS_POOL})) ||
                      (cellul.eq(&(CatenaFinitaCfg){.src = given, .other = "<stdin>", .cap = CLI_CELLS_POOL})) ||
                      (cellul.eq(&(CatenaFinitaCfg){.src = given, .other = "stdin", .cap = CLI_CELLS_POOL}));
    if (input && (s_read->path != NULL))
    {
        return cli_checkers_file(s_read->findings, s_read->path, 0);
    }
    const int absolute = (given[0] == '/') || (given[0] == '\\') || ((given[0] != '\0') && (given[1] == ':'));
    char path[CLI_PATH_MAX_LEN];
    if (absolute)
    {
        memor.cpy(&(MemoriaCfg){.dst = path,
                                .src = given,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = given, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        cli_full(s_read->root, given, path);
    }
    return cli_checkers_file(s_read->findings, path, 0);
}

void cli_checkers_finding_put(unsigned int file, const CliFinding *finding)
{
    CliFindings *const findings = s_read->findings;
    if ((file >= findings->files) || (findings->findings >= CLI_CHECK_FINDINGS))
    {
        return;
    }
    VerbaLinea out = {findings->text + findings->used, CLI_CHECK_TEXT - findings->used, 0ull};
    const unsigned long long line = (finding->line > 0ull) ? (finding->line - 1ull) : 0ull;
    const unsigned long long col = (finding->col > 0ull) ? (finding->col - 1ull) : 0ull;
    mmgr_line_put(&out, "{\"range\":{\"start\":{\"line\":");
    mmgr_line_decimal(&out, line, 0u);
    mmgr_line_put(&out, ",\"character\":");
    mmgr_line_decimal(&out, col, 0u);
    mmgr_line_put(&out, "},\"end\":{\"line\":");
    if (finding->has_end)
    {
        mmgr_line_decimal(&out, (finding->end_line > 0ull) ? (finding->end_line - 1ull) : 0ull, 0u);
        mmgr_line_put(&out, ",\"character\":");
        mmgr_line_decimal(&out, (finding->end_col > 0ull) ? (finding->end_col - 1ull) : 0ull, 0u);
    }
    else
    {
        mmgr_line_decimal(&out, line, 0u);
        mmgr_line_put(&out, ",\"character\":");
        mmgr_line_decimal(&out, finding->col, 0u);
    }
    mmgr_line_put(&out, "}},\"severity\":");
    mmgr_line_decimal(&out, finding->severity, 0u);
    mmgr_line_put(&out, ",\"message\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)finding->message, finding->message_length);
    mmgr_line_put(&out, ",\"source\":");
    cli_json_quoted(&out, s_read->name, cellul.len(&(CatenaFinitaCfg){.src = s_read->name, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&out, ",\"code\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)finding->code, finding->code_length);
    mmgr_line_put(&out, ",\"data\":{\"orior\":");
    mmgr_line_put(&out, finding->fixes);
    mmgr_line_put(&out, "}}");
    if (!mmgr_line_ok(&out))
    {
        return;
    }
    findings->file_of[findings->findings] = file;
    findings->start[findings->findings] = findings->used;
    // a diagnostic is below CLI_CHECK_TEXT bytes, which an unsigned int represents
    findings->length[findings->findings] = (unsigned int)out.at;
    findings->used += (unsigned int)out.at;
    findings->findings += 1u;
}

// a whole number member of a node, or `otherwise` where it is none
static unsigned long long cli_whole(unsigned int node, const char *key, unsigned long long otherwise)
{
    const unsigned int member = cli_json_member(&s_read->json, node, key);
    if (!cli_json_is(&s_read->json, member, CFG_JSON_NUMBER))
    {
        return otherwise;
    }
    const CliJsonNode *const one = &s_read->json.node[member];
    const char *const digits = s_read->json.text + one->text;
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; at < one->length; at += 1ull)
    {
        if ((digits[at] < '0') || (digits[at] > '9'))
        {
            return otherwise;
        }
        value = (value * 10ull) + (unsigned long long)(digits[at] - '0');
    }
    return (one->length > 0ull) ? value : otherwise;
}

// a string member of a node with its length, `otherwise` where it is none
static const char *cli_text(unsigned int node, const char *key, const char *otherwise, unsigned long long *length)
{
    const char *const text = cli_json_string(&s_read->json, cli_json_member(&s_read->json, node, key), length);
    if (text == NULL)
    {
        *length = cellul.len(&(CatenaFinitaCfg){.src = otherwise, .cap = CLI_CELLS_POOL});
    }
    return (text != NULL) ? text : otherwise;
}

// the checker's JSON read into a tree sized to it: its root, or CLI_JSON_NONE
static unsigned int cli_json_of(const char *out, unsigned long long size)
{
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    return cli_json_taken(&s_read->json, out, size, 0u, 0ull, &unread, &g_cli_toolchains_cells);
}

// a place in ruff's findings, its row and column counted from one, written counted from zero as `{"line", "col"}`
static void cli_place_written(VerbaLinea *out, unsigned int place)
{
    const unsigned long long row = cli_whole(place, "row", 1ull);
    const unsigned long long column = cli_whole(place, "column", 1ull);
    mmgr_line_put(out, "{\"line\":");
    mmgr_line_decimal(out, (row > 0ull) ? (row - 1ull) : 0ull, 0u);
    mmgr_line_put(out, ",\"col\":");
    mmgr_line_decimal(out, (column > 0ull) ? (column - 1ull) : 0ull, 0u);
    mmgr_line_ch(out, '}');
}

// ruff's findings, each with the fix it gives
static void cli_ruff_read(const char *out, unsigned long long size)
{
    CliJson *const json = &s_read->json;
    const unsigned int root = cli_json_of(out, size);
    for (unsigned int at = 0u; cli_json_item(json, root, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(json, root, at);
        const unsigned int location = cli_json_member(json, one, "location");
        const unsigned int end = cli_json_member(json, one, "end_location");
        const unsigned int fix = cli_json_member(json, one, "fix");
        const unsigned int edits = cli_json_member(json, fix, "edits");
        VerbaLinea fixes = {s_read->fixes, sizeof(s_read->fixes), 0ull};
        if (cli_json_is(json, edits, CFG_JSON_ARRAY))
        {
            unsigned long long length = 0ull;
            const char *const title = cli_text(fix, "message", "Fix it as ruff does", &length);
            mmgr_line_put(&fixes, "[{\"title\":");
            cli_json_quoted(&fixes, title, length);
            mmgr_line_put(&fixes, ",\"edits\":[");
            for (unsigned int edit = 0u; cli_json_item(json, edits, edit) != CLI_JSON_NONE; edit += 1u)
            {
                const unsigned int one_edit = cli_json_item(json, edits, edit);
                mmgr_line_put(&fixes, (edit != 0u) ? ",{\"from\":" : "{\"from\":");
                cli_place_written(&fixes, cli_json_member(json, one_edit, "location"));
                mmgr_line_put(&fixes, ",\"to\":");
                cli_place_written(&fixes, cli_json_member(json, one_edit, "end_location"));
                const char *const content = cli_text(one_edit, "content", "", &length);
                mmgr_line_put(&fixes, ",\"text\":");
                cli_json_quoted(&fixes, content, length);
                mmgr_line_ch(&fixes, '}');
            }
            mmgr_line_put(&fixes, "]}]");
        }
        else
        {
            mmgr_line_put(&fixes, "[]");
        }
        mmgr_line_finish(&fixes);
        unsigned long long name_length = 0ull;
        const char *const name = cli_text(one, "filename", "-", &name_length);
        CliFinding finding = {cli_whole(location, "row", 1ull),
                              cli_whole(location, "column", 1ull),
                              1,
                              cli_whole(end, "row", 1ull),
                              cli_whole(end, "column", 1ull),
                              2u,
                              NULL,
                              0ull,
                              NULL,
                              0ull,
                              mmgr_line_ok(&fixes) ? s_read->fixes : "[]"};
        finding.message = cli_text(one, "message", "", &finding.message_length);
        finding.code = cli_text(one, "code", "", &finding.code_length);
        cli_checkers_finding_put(cli_checkers_file_of(name, name_length), &finding);
    }
}

// ESLint's findings, by file
static void cli_eslint_read(const char *out, unsigned long long size)
{
    CliJson *const json = &s_read->json;
    const unsigned int root = cli_json_of(out, size);
    for (unsigned int at = 0u; cli_json_item(json, root, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int file = cli_json_item(json, root, at);
        unsigned long long name_length = 0ull;
        const char *const name = cli_text(file, "filePath", "-", &name_length);
        const unsigned int messages = cli_json_member(json, file, "messages");
        for (unsigned int index = 0u; cli_json_item(json, messages, index) != CLI_JSON_NONE; index += 1u)
        {
            const unsigned int one = cli_json_item(json, messages, index);
            const int has_end = cli_json_is(json, cli_json_member(json, one, "endLine"), CFG_JSON_NUMBER);
            CliFinding finding = {cli_whole(one, "line", 1ull),
                                  cli_whole(one, "column", 1ull),
                                  has_end,
                                  cli_whole(one, "endLine", 1ull),
                                  cli_whole(one, "endColumn", 1ull),
                                  (cli_whole(one, "severity", 0ull) == 2ull) ? 1u : 2u,
                                  NULL,
                                  0ull,
                                  NULL,
                                  0ull,
                                  "[]"};
            finding.message = cli_text(one, "message", "", &finding.message_length);
            finding.code = cli_text(one, "ruleId", "", &finding.code_length);
            cli_checkers_finding_put(cli_checkers_file_of(name, name_length), &finding);
        }
    }
}

// ShellCheck's findings, each with its level as a severity and its code as SC and the number
static void cli_shellcheck_read(const char *out, unsigned long long size)
{
    CliJson *const json = &s_read->json;
    const unsigned int comments = cli_json_member(json, cli_json_of(out, size), "comments");
    for (unsigned int at = 0u; cli_json_item(json, comments, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(json, comments, at);
        unsigned long long length = 0ull;
        const char *const level = cli_text(one, "level", "", &length);
        const unsigned int severity =
            (cellul.eq(&(CatenaFinitaCfg){.src = level, .other = "error", .cap = CLI_CELLS_POOL}))     ? 1u
            : (cellul.eq(&(CatenaFinitaCfg){.src = level, .other = "warning", .cap = CLI_CELLS_POOL})) ? 2u
            : (cellul.eq(&(CatenaFinitaCfg){.src = level, .other = "info", .cap = CLI_CELLS_POOL}))    ? 3u
                                                                                                       : 4u;
        char code[32] = "";
        if (cli_json_is(json, cli_json_member(json, one, "code"), CFG_JSON_NUMBER))
        {
            VerbaLinea written = {code, sizeof(code), 0ull};
            mmgr_line_put(&written, "SC");
            mmgr_line_decimal(&written, cli_whole(one, "code", 0ull), 0u);
            mmgr_line_finish(&written);
        }
        const int has_end = cli_json_is(json, cli_json_member(json, one, "endLine"), CFG_JSON_NUMBER);
        CliFinding finding = {cli_whole(one, "line", 1ull),
                              cli_whole(one, "column", 1ull),
                              has_end,
                              cli_whole(one, "endLine", 1ull),
                              cli_whole(one, "endColumn", 1ull),
                              severity,
                              NULL,
                              0ull,
                              code,
                              cellul.len(&(CatenaFinitaCfg){.src = code, .cap = CLI_CELLS_POOL}),
                              "[]"};
        finding.message = cli_text(one, "message", "", &finding.message_length);
        unsigned long long name_length = 0ull;
        const char *const name = cli_text(one, "file", "-", &name_length);
        cli_checkers_finding_put(cli_checkers_file_of(name, name_length), &finding);
    }
}

void cli_checkers_read(const char *name, const char *reads, const char *out, unsigned long long out_size,
                       const char *err, unsigned long long err_size, const char *root, const char *path,
                       CliFindings *findings)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    s_read = &s_room;
    s_read->name = name;
    s_read->root = root;
    s_read->path = path;
    s_read->findings = findings;
    findings->files = 0u;
    findings->findings = 0u;
    findings->used = 0u;
    if (cellul.eq(&(CatenaFinitaCfg){.src = reads, .other = "ruff", .cap = CLI_CELLS_POOL}))
    {
        cli_ruff_read(out, out_size);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = reads, .other = "eslint", .cap = CLI_CELLS_POOL}))
    {
        cli_eslint_read(out, out_size);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = reads, .other = "shellcheck", .cap = CLI_CELLS_POOL}))
    {
        cli_shellcheck_read(out, out_size);
    }
    else
    {
        // what it wrote, then, for a checker that writes its findings a line each, what it printed
        cli_checkers_lines_read(out, out_size);
        if (cellul.eq(&(CatenaFinitaCfg){.src = reads, .other = "lines", .cap = CLI_CELLS_POOL}))
        {
            cli_checkers_lines_read(err, err_size);
        }
    }
    cli_json_given(&s_read->json);
    s_read = NULL;
    cli_lock_give(&s_lock);
}
