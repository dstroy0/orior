// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_files.c: `orior_cli run run-file`, a file run with its language's toolchain, in bash at the tree's top
// folder, attached to this terminal, exiting with its code; `orior_cli run validate`, each file validated with the tool
// plugin for its language, what it found and its verdict printed, or the whole report as JSON; and `orior_cli edit
// format`, each file formatted in place with its language's formatter, or with --check each that would change named
#include "../code/git.h"
#include "../plugins/validate.h"
#include "../runs/run_file.h"
#include "../runs/runner.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "../toolchains/format.h"
#include "../toolchains/toolchains.h"
#include "cells.h"
#include "command_line_internal.h"

#include <stdlib.h>

// A command's room, a cell taken as the command starts and given back as it ends: the tree a text is read into, a
// line printed, a file read and what is made of it
typedef struct
{
    CliJsonNode node[1u << 16u];
    char tree_text[1u << 24u];
    CfgJsonToken token[1u << 16u];
    CliJson json;
    char line[1u << 16u];
    char read[1u << 24u];
    char made[1u << 24u];
} CliFilesRoom;

static CliFilesRoom *s_room;

// the command's room taken: 1, or 0 reported on the errors
static int files_room_made(void)
{
    s_room = g_cli_cells->persistent_buf_alloc(sizeof(*s_room), &g_cli_command_line_cells);
    if (s_room == NULL)
    {
        cli_err("a command on files could not be taken");
    }
    return s_room != NULL;
}

// the command's room given back
static void files_released(void)
{
    cli_cell_release(s_room, &g_cli_command_line_cells);
    s_room = NULL;
}

// `text` read as JSON into the tree: its node, or CLI_JSON_NONE
static unsigned int files_tree(const char *text, unsigned long long size)
{
    cli_json_begun(&s_room->json, s_room->node, sizeof(s_room->node) / sizeof(s_room->node[0]), s_room->tree_text,
                   sizeof(s_room->tree_text));
    char why[256];
    VerbaLinea ignored = {why, sizeof(why), 0ull};
    return cli_json_read(&s_room->json, text, size, s_room->token, sizeof(s_room->token) / sizeof(s_room->token[0]),
                         &ignored);
}

// a line of the errors made of up to three texts
static void files_err(const char *one, const char *two, const char *three)
{
    VerbaLinea line = {s_room->line, sizeof(s_room->line), 0ull};
    mmgr_line_put(&line, one);
    mmgr_line_put(&line, two);
    mmgr_line_put(&line, three);
    cli_err_line(s_room->line, mmgr_line_ok(&line) ? line.at : 0ull);
}

// a word naming a file made a full path into `full`, the word as it is where it cannot be
static void files_full(const char *word, char *full)
{
    if (!query_path_full(word, full, CLI_PATH_MAX_LEN))
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL});
        const unsigned long long copied_len = (size < CLI_PATH_MAX_LEN) ? size : CLI_PATH_MAX_LEN - 1ull;
        memor.cpy(&(MemoriaCfg){.dst = full, .src = word, .bytes = copied_len});
        full[copied_len] = '\0';
    }
}

static int files_run_words(const char *named, const char *const *words, unsigned int count)
{
    if (count != 1u)
    {
        cli_err("run-file takes the one file to run");
        return CLI_WRONG;
    }
    static char s_root[CLI_PATH_MAX_LEN];
    static char s_err_buf[4096];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    if (!cli_command_line_tree(named, s_root, &err_msg))
    {
        cli_err_line(s_err_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        return CLI_NO_CODE;
    }
    char path[CLI_PATH_MAX_LEN];
    files_full(words[0], path);
    char language[CLI_NAME_MAX_LEN];
    if (!cli_format_language_of(path, language))
    {
        files_err(words[0], ": no plugin opens it, and so orior_cli knows no way to run it", "");
        return CLI_NO_CODE;
    }
    VerbaLinea run = {s_room->read, sizeof(s_room->read), 0ull};
    if (!cli_run_file_line(s_root, path, language, &run, &err_msg))
    {
        cli_err_line(s_err_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        return CLI_NO_CODE;
    }
    const unsigned int tree = files_tree(s_room->read, mmgr_line_ok(&run) ? run.at : 0ull);
    const char *const line = cli_json_string(&s_room->json, cli_json_member(&s_room->json, tree, "line"), NULL);
    char bash[CLI_PATH_MAX_LEN];
    if ((line == NULL) || !cli_runner_bash(bash, &err_msg))
    {
        cli_err_line(s_err_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        return CLI_NO_CODE;
    }
    files_err("$ ", line, "");
    // the run's lines are this terminal's; the window is notified that it started and how it ended
    cli_window_channel_begun();
    VerbaLinea state = {s_room->made, sizeof(s_room->made), 0ull};
    mmgr_line_put(&state, "{\"state\":\"started\",\"file\":");
    cli_json_quoted_lossy(&state, (const unsigned char *)words[0],
                          cellul.len(&(CatenaFinitaCfg){.src = words[0], .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&state, ",\"line\":");
    cli_json_quoted(&state, line, cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&state, ",\"root\":");
    cli_json_quoted_lossy(&state, (const unsigned char *)s_root,
                          cellul.len(&(CatenaFinitaCfg){.src = s_root, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&state, '}');
    cli_window_channel_sent(s_room->made, mmgr_line_ok(&state) ? state.at : 0ull);
    const unsigned long long began = cli_clock_microseconds();
    static char s_path[CLI_PATH_TEXT];
    cli_toolchains_run_path(s_path, sizeof(s_path));
    const CliSetting setting = {"PATH", s_path};
    const char *const command[4] = {bash, "-c", line, NULL};
    long long ran = 0ll;
    err_msg.at = 0ull;
    int code = CLI_NO_CODE;
    if (!cli_spawn_shared(command, &setting, 1u, &ran, &err_msg))
    {
        cli_err_line(s_err_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
    }
    else if (ran >= 0ll)
    {
        // an exit code is the system's int
        code = (int)ran;
    }
    state.at = 0ull;
    mmgr_line_put(&state, "{\"state\":\"ended\",\"code\":");
    mmgr_line_decimal_signed(&state, code);
    mmgr_line_put(&state, ",\"ms\":");
    cli_json_double(&state, (double)(cli_clock_microseconds() - began) / 1000.0);
    mmgr_line_ch(&state, '}');
    cli_window_channel_sent(s_room->made, mmgr_line_ok(&state) ? state.at : 0ull);
    return code;
}

// the strings of an array member joined by `between`
static void files_joined(VerbaLinea *out, unsigned int node, const char *name, const char *between)
{
    const unsigned int array = cli_json_member(&s_room->json, node, name);
    const unsigned int first =
        cli_json_is(&s_room->json, array, CFG_JSON_ARRAY) ? s_room->json.node[array].first : CLI_JSON_NONE;
    for (unsigned int item = first; item != CLI_JSON_NONE; item = s_room->json.node[item].next)
    {
        const char *const text = cli_json_string(&s_room->json, item, NULL);
        mmgr_line_put(out, (item != first) ? between : "");
        mmgr_line_put(out, (text != NULL) ? text : "");
    }
}

// a number member, as written, read; 0 where it has none
static double files_number(unsigned int node, const char *name)
{
    const unsigned int number = cli_json_member(&s_room->json, node, name);
    if (!cli_json_is(&s_room->json, number, CFG_JSON_NUMBER))
    {
        return 0.0;
    }
    char text[64];
    const unsigned long long length = s_room->json.node[number].length;
    const unsigned long long copied_len = (length < sizeof(text)) ? length : sizeof(text) - 1u;
    memor.cpy(
        &(MemoriaCfg){.dst = text, .src = s_room->json.text + s_room->json.node[number].text, .bytes = copied_len});
    text[copied_len] = '\0';
    return strtod(text, NULL);
}

// each check of a report printed, then each finding, then the file's verdict
static void files_report_printed(const char *file, unsigned int report)
{
    const unsigned int checks = cli_json_member(&s_room->json, report, "checks");
    const unsigned int first =
        cli_json_is(&s_room->json, checks, CFG_JSON_ARRAY) ? s_room->json.node[checks].first : CLI_JSON_NONE;
    for (unsigned int check = first; check != CLI_JSON_NONE; check = s_room->json.node[check].next)
    {
        VerbaLinea line = {s_room->line, sizeof(s_room->line), 0ull};
        mmgr_line_put(&line, "  checked ");
        const unsigned int settings = cli_json_member(&s_room->json, check, "settings");
        if (!cli_json_is(&s_room->json, settings, CFG_JSON_ARRAY) || (s_room->json.node[settings].count == 0u))
        {
            mmgr_line_put(&line, "as it is");
        }
        files_joined(&line, check, "settings", " ");
        mmgr_line_put(&line, ": ");
        const double seconds = files_number(check, "seconds");
        // the seconds to a tenth, rounded; a check's seconds are not negative
        const unsigned long long tenths = (unsigned long long)(seconds * 10.0 + 0.5);
        mmgr_line_decimal(&line, tenths / 10ull, 1u);
        mmgr_line_ch(&line, '.');
        mmgr_line_decimal(&line, tenths % 10ull, 1u);
        mmgr_line_put(&line, " s, ");
        const double errors = files_number(check, "errors");
        // a count of errors is a whole number
        mmgr_line_decimal(&line, (unsigned long long)errors, 1u);
        mmgr_line_put(&line, (errors == 1.0) ? " error" : " errors");
        const unsigned int barriers = cli_json_member(&s_room->json, check, "barriers");
        if (cli_json_is(&s_room->json, barriers, CFG_JSON_ARRAY) && (s_room->json.node[barriers].count > 0u))
        {
            mmgr_line_put(&line, ", stopped by ");
            files_joined(&line, check, "barriers", ", ");
        }
        cli_out_line(s_room->line, mmgr_line_ok(&line) ? line.at : 0ull);
    }
    const unsigned int findings = cli_json_member(&s_room->json, report, "findings");
    const unsigned int found =
        cli_json_is(&s_room->json, findings, CFG_JSON_ARRAY) ? s_room->json.node[findings].first : CLI_JSON_NONE;
    for (unsigned int finding = found; finding != CLI_JSON_NONE; finding = s_room->json.node[finding].next)
    {
        const char *const kind = cli_json_string(&s_room->json, cli_json_member(&s_room->json, finding, "kind"), NULL);
        const char *const message =
            cli_json_string(&s_room->json, cli_json_member(&s_room->json, finding, "message"), NULL);
        const char *const lifted =
            cli_json_string(&s_room->json, cli_json_member(&s_room->json, finding, "lifted"), NULL);
        VerbaLinea line = {s_room->line, sizeof(s_room->line), 0ull};
        mmgr_line_put(&line, file);
        mmgr_line_ch(&line, ':');
        // a line and a column counted from 0 are whole numbers
        mmgr_line_decimal(&line, (unsigned long long)files_number(finding, "line") + 1ull, 1u);
        mmgr_line_ch(&line, ':');
        mmgr_line_decimal(&line, (unsigned long long)files_number(finding, "col") + 1ull, 1u);
        mmgr_line_put(&line, ": ");
        mmgr_line_put(&line, (kind != NULL) ? kind : "");
        mmgr_line_put(&line, ": ");
        const char *rest = (message != NULL) ? message : "";
        int head = 1;
        for (;;)
        {
            const char *const end = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .byte = '\n'});
            const unsigned long long size = (end != NULL)
                                                ? (unsigned long long)(end - rest)
                                                : cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL});
            unsigned long long from = 0ull;
            unsigned long long to = 0ull;
            cli_text_trimmed(rest, size, &from, &to);
            if (to > from)
            {
                if (!head)
                {
                    line.at = 0ull;
                    mmgr_line_put(&line, "    ");
                }
                const unsigned long long len = ((size > 0ull) && (rest[size - 1ull] == '\r')) ? size - 1ull : size;
                mmgr_line_put_n(&line, rest, len);
                cli_out_line(s_room->line, mmgr_line_ok(&line) ? line.at : 0ull);
                head = 0;
            }
            if (end == NULL)
            {
                break;
            }
            rest = end + 1;
        }
        if (head)
        {
            cli_out_line(s_room->line, mmgr_line_ok(&line) ? line.at : 0ull);
        }
        if (lifted != NULL)
        {
            line.at = 0ull;
            mmgr_line_put(&line, "    ");
            mmgr_line_put(&line, lifted);
            cli_out_line(s_room->line, mmgr_line_ok(&line) ? line.at : 0ull);
        }
    }
    const int passes = cli_json_is(&s_room->json, cli_json_member(&s_room->json, report, "passes"), CFG_JSON_TRUE);
    const char *const verdict = cli_json_string(&s_room->json, cli_json_member(&s_room->json, report, "verdict"), NULL);
    VerbaLinea line = {s_room->line, sizeof(s_room->line), 0ull};
    mmgr_line_put(&line, file);
    mmgr_line_put(&line, passes ? ": passes: " : ": fails: ");
    mmgr_line_put(&line, (verdict != NULL) ? verdict : "");
    cli_out_line(s_room->line, mmgr_line_ok(&line) ? line.at : 0ull);
}

// whether `flag` is among the words, and whether every other word names a file, there being one
static int files_words(const char *const *words, unsigned int count, const char *flag, int *flagged)
{
    unsigned int files = 0u;
    *flagged = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = flag, .cap = CLI_CELLS_POOL}))
        {
            *flagged = 1;
            continue;
        }
        if (cellul.starts(&(CatenaFinitaCfg){.src = words[at], .other = "--", .cap = CLI_CELLS_POOL}))
        {
            return 0;
        }
        files += 1u;
    }
    return files > 0u;
}

static int files_validated(const char *const *words, unsigned int count)
{
    int json = 0;
    if (!files_words(words, count, "--json", &json))
    {
        cli_err("validate takes [--json] and the files to validate");
        return CLI_WRONG;
    }
    int code = 0;
    static char s_err_buf[4096];
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const file = words[at];
        if (cellul.eq(&(CatenaFinitaCfg){.src = file, .other = "--json", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        files_full(file, path);
        char language[CLI_NAME_MAX_LEN];
        char tool[CLI_NAME_MAX_LEN];
        if (!cli_format_language_of(path, language) || !cli_validate_tool_for(language, tool))
        {
            files_err(file, ": no tool plugin validates it", "");
            code = CLI_NO_CODE;
            continue;
        }
        VerbaLinea report = {s_room->read, sizeof(s_room->read), 0ull};
        VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
        if (!cli_validated(tool, path, &report, &err_msg))
        {
            (void)mmgr_line_finish(&err_msg);
            files_err(file, ": ", s_err_buf);
            code = CLI_NO_CODE;
            continue;
        }
        const unsigned int tree = files_tree(s_room->read, mmgr_line_ok(&report) ? report.at : 0ull);
        if (!cli_json_is(&s_room->json, cli_json_member(&s_room->json, tree, "passes"), CFG_JSON_TRUE))
        {
            code = CLI_NO_CODE;
        }
        if (json)
        {
            VerbaLinea pretty = {s_room->made, sizeof(s_room->made), 0ull};
            if (tree != CLI_JSON_NONE)
            {
                cli_json_write(&s_room->json, tree, &pretty, 2u, 0);
            }
            cli_out_line(s_room->made, mmgr_line_ok(&pretty) ? pretty.at : 0ull);
            continue;
        }
        files_report_printed(file, tree);
    }
    return code;
}

static int files_formatted(const char *const *words, unsigned int count)
{
    int check = 0;
    if (!files_words(words, count, "--check", &check))
    {
        cli_err("format takes [--check] and the files to format");
        return CLI_WRONG;
    }
    int code = 0;
    static char s_err_buf[4096];
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const file = words[at];
        if (cellul.eq(&(CatenaFinitaCfg){.src = file, .other = "--check", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        char full[CLI_PATH_MAX_LEN];
        files_full(file, full);
        char language[CLI_NAME_MAX_LEN];
        if (!cli_format_language_of(file, language))
        {
            files_err(file, ": no plugin opens it, and so no formatter knows it", "");
            code = CLI_NO_CODE;
            continue;
        }
        const unsigned long long size = qry_member_file_read(file, (unsigned char *)s_room->read, sizeof(s_room->read));
        if ((size == QRY_MEMBER_UNREAD) || !cli_utf8_whole(s_room->read, size))
        {
            files_err(file, ": it could not be read", "");
            code = CLI_NO_CODE;
            continue;
        }
        VerbaLinea formatted = {s_room->made, sizeof(s_room->made), 0ull};
        VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
        if (!cli_format(full, language, s_room->read, size, &formatted, &err_msg))
        {
            cli_err_line(s_err_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
            code = CLI_NO_CODE;
            continue;
        }
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(
            &s_room->json, files_tree(s_room->made, mmgr_line_ok(&formatted) ? formatted.at : 0ull), &length);
        if ((text != NULL) && (length == size) &&
            (memor.cmp(&(MemoriaCfg){.src = text, .other = s_room->read, .bytes = size}) == 0))
        {
            continue;
        }
        if (check)
        {
            VerbaLinea line = {s_room->line, sizeof(s_room->line), 0ull};
            mmgr_line_put(&line, file);
            mmgr_line_put(&line, " would change");
            cli_out_line(s_room->line, mmgr_line_ok(&line) ? line.at : 0ull);
            code = CLI_NO_CODE;
            continue;
        }
        if ((text == NULL) || !query_file_written(file, text, length))
        {
            files_err(file, ": it could not be written", "");
            code = CLI_NO_CODE;
            continue;
        }
        VerbaLinea line = {s_room->line, sizeof(s_room->line), 0ull};
        mmgr_line_put(&line, file);
        mmgr_line_put(&line, " formatted");
        cli_out_line(s_room->line, mmgr_line_ok(&line) ? line.at : 0ull);
    }
    return code;
}

int cli_run_file_words(const char *named, const char *const *words, unsigned int count)
{
    if (!files_room_made())
    {
        return CLI_NO_CODE;
    }
    const int code = files_run_words(named, words, count);
    files_released();
    return code;
}

int cli_validate_files(const char *const *words, unsigned int count)
{
    if (!files_room_made())
    {
        return CLI_NO_CODE;
    }
    const int code = files_validated(words, count);
    files_released();
    return code;
}

int cli_format_files(const char *const *words, unsigned int count)
{
    if (!files_room_made())
    {
        return CLI_NO_CODE;
    }
    const int code = files_formatted(words, count);
    files_released();
    return code;
}
