// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// report.c: whether errors file on their own, stored in settings.json in orior's own folder with the window's own
// settings retained as they are; what identifies the reporter struck from a report; an error's fingerprint; and the
// run's recent errors
#include "report.h"
#include "../text/json_tree.h"

// the most recent errors retained for a report that requests them, and the most bytes of each; the nodes and the bytes
// of texts a tree of the settings reserves over for the answer, beyond its copy of the settings; and the bytes the
// settings written and a text struck are begun with, each growing past them
#define CLI_REPORT_RETAIN_MAX 20u
#define CLI_REPORT_ERROR (1u << 16u)
#define CLI_REPORT_ANSWER_NODES 4u
#define CLI_REPORT_ANSWER_TEXT 64ull
#define CLI_REPORT_WRITTEN_FIRST 4096ull
#define CLI_REPORT_STRUCK_MORE 64ull

// the run's recent errors, a ring of cells each of its error's size, the oldest at `first`, under the lock
static char *s_errors[CLI_REPORT_RETAIN_MAX];
static unsigned int s_first;
static unsigned int s_count;
static CliLock s_errors_lock = CLI_LOCK_INIT;

// settings.json read and written whole under its lock
static CliLock s_settings_lock = CLI_LOCK_INIT;

int cli_report_category_known(const char *category)
{
    static const char *const s_categories[4] = {"library", "cli", "ui", "unknown"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = category, .other = s_categories[at], .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// settings.json's path into `path`, sized for CLI_PATH_MAX_LEN: 1, or 0 where orior_cli has no folder of its own
static int cli_settings_path(char *path)
{
    char home[CLI_PATH_MAX_LEN];
    if (!cli_home_folder(home))
    {
        return 0;
    }
    VerbaLinea line = {path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, home);
    mmgr_line_put(&line, "/settings.json");
    return mmgr_line_finish(&line) != 0ull;
}

// settings.json read, the lock acquired, into a tree of its own sized to it, with room over for a copy of it and the
// answer where `copied` is 1: its object, or CLI_JSON_NONE where it is not there or does not read, the tree then
// reserving room for the answer alone where `copied` is 1; the tree given back with cli_json_given
static unsigned int cli_settings(CliJson *json, int copied)
{
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
    char path[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_settings_path(path) ? cli_file_read(path, &size) : NULL;
    // the file's bytes are read as its text
    const char *const text = (const char *)bytes;
    const unsigned int tokens = (copied && (bytes != NULL)) ? cli_json_tokens_counted(text, size) : 0u;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea reason = {why, sizeof(why), 0ull};
    const unsigned int read = (bytes != NULL)
                                  ? cli_json_taken(json, text, size, copied ? (tokens + CLI_REPORT_ANSWER_NODES) : 0u,
                                                   copied ? (size + CLI_REPORT_ANSWER_TEXT) : 0ull, &reason)
                                  : CLI_JSON_NONE;
    if (bytes != NULL)
    {
        cli_file_let_go(bytes, size);
    }
    const unsigned int root = cli_json_is(json, read, CFG_JSON_OBJECT) ? read : CLI_JSON_NONE;
    if (copied && (root == CLI_JSON_NONE))
    {
        cli_json_given(json);
        (void)cli_json_reserved(json, CLI_REPORT_ANSWER_NODES, CLI_REPORT_ANSWER_TEXT, "the settings");
    }
    return root;
}

// the reporter's answer: 1 with it into `on`, or 0 where they have not answered
static int cli_answer(int *on)
{
    cli_lock_take(&s_settings_lock);
    CliJson json;
    const unsigned int root = cli_settings(&json, 0);
    const unsigned int answer = cli_json_member(&json, root, "auto_report");
    const int answered = cli_json_is(&json, answer, CFG_JSON_TRUE) || cli_json_is(&json, answer, CFG_JSON_FALSE);
    *on = cli_json_is(&json, answer, CFG_JSON_TRUE);
    cli_json_given(&json);
    cli_lock_give(&s_settings_lock);
    return answered;
}

int cli_report_auto(void)
{
    char none[CLI_NAME_MAX_LEN];
    int on = 0;
    return !query_environment_read("ORIOR_NO_REPORTS", none, sizeof(none)) && cli_answer(&on) && on;
}

int cli_report_prompted(void)
{
    int on = 0;
    return cli_answer(&on);
}

void cli_report_prompt_once(void)
{
    char none[CLI_NAME_MAX_LEN];
    if (cli_report_prompted() || query_environment_read("ORIOR_NO_REPORTS", none, sizeof(none)) ||
        !query_terminal_is() || !cli_errors_terminal())
    {
        return;
    }
    static const char s_prompt[] = CLI_REPORT_QUESTION "\nFile them? [Y/n] ";
    query_terminal_written(1, s_prompt, sizeof(s_prompt) - 1u);
    char answer[CLI_NAME_MAX_LEN];
    const unsigned long long length = query_terminal_line_read(answer, sizeof(answer));
    if (length == QRY_MEMBER_UNREAD)
    {
        return;
    }
    const char *from = answer;
    while ((from[0] == ' ') || ((from[0] >= '\t') && (from[0] <= '\r')))
    {
        from += 1;
    }
    const int on = !((from[0] == 'n') || (from[0] == 'N'));
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!cli_report_set_auto(on, &err_msg))
    {
        mmgr_line_ch(&err_msg, '\n');
        query_terminal_written(1, why, err_msg.at);
    }
}

int cli_report_set_auto(int on, VerbaLinea *err_msg)
{
    char path[CLI_PATH_MAX_LEN];
    if (!cli_settings_path(path))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own to store settings in");
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    if (cli_path_parent(path, folder) && !query_folder_made(folder))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    cli_lock_take(&s_settings_lock);
    CliJson json;
    const unsigned int settings_node = cli_settings(&json, 1);
    // the answer first, then the window's own settings as they are
    const unsigned int made = cli_json_made(&json, CFG_JSON_OBJECT);
    cli_json_set(&json, made, "auto_report", cli_json_made(&json, on ? CFG_JSON_TRUE : CFG_JSON_FALSE));
    for (unsigned int member = (settings_node != CLI_JSON_NONE) ? json.node[settings_node].first : CLI_JSON_NONE;
         member != CLI_JSON_NONE; member = json.node[member].next)
    {
        const char *const key = json.text + json.node[member].key;
        if (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "auto_report", .cap = CLI_CELLS_POOL}))
        {
            cli_json_set(&json, made, key, cli_json_copy(&json, member));
        }
    }
    VerbaLinea out;
    const int begun = cli_growing_line_made(&out, CLI_REPORT_WRITTEN_FIRST);
    cli_json_write(&json, made, &out, 2u, 0);
    const int written = begun && !json.full && mmgr_line_ok(&out) && query_file_written(path, out.out, out.at);
    g_cli_cells->persistent_buf_release(out.out, NULL);
    cli_json_given(&json);
    cli_lock_give(&s_settings_lock);
    if (!written)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be written");
    }
    return written;
}

// a letter in lower case, ASCII's
static char cli_lower(char letter)
{
    return ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter + ('a' - 'A')) : letter;
}

// `text` with every `found` in it, ASCII's case aside, put as `put`, into `out`
static void cli_replaced(const char *text, const char *found, const char *put, VerbaLinea *out)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL});
    for (const char *at = text; at[0] != '\0';)
    {
        unsigned long long matched = 0ull;
        while ((matched < length) && (at[matched] != '\0') && (cli_lower(at[matched]) == cli_lower(found[matched])))
        {
            matched += 1ull;
        }
        if ((length != 0ull) && (matched == length))
        {
            mmgr_line_put(out, put);
            at += length;
        }
        else
        {
            mmgr_line_ch(out, at[0]);
            at += 1;
        }
    }
    mmgr_line_finish(out);
}

// `found`, three letters or more, struck from the text `*struck` contains as `put`, as it is, with its slashes leaning
// forward, and with them leaning back, each pass into a cell of its own grown to the text and the one before given
// back; where a cell is refused the text is let go whole, so nothing it contained goes out unstruck
static void cli_strike(char **struck, const char *found, const char *put)
{
    if (cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL}) < 3u)
    {
        return;
    }
    char forms[3][CLI_PATH_MAX_LEN];
    for (unsigned int form = 0u; form < 3u; form += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = forms[form],
                                .src = found,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL}) + 1u});
        for (char *at = forms[form]; (form != 0u) && (at[0] != '\0'); at += 1)
        {
            at[0] = ((form == 1u) && (at[0] == '\\')) ? '/' : ((form == 2u) && (at[0] == '/')) ? '\\' : at[0];
        }
        const char *const text = (*struck != NULL) ? *struck : "";
        VerbaLinea out;
        const int begun = cli_growing_line_made(
            &out, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + CLI_REPORT_STRUCK_MORE);
        if (begun)
        {
            cli_replaced(text, forms[form], put, &out);
        }
        g_cli_cells->persistent_buf_release(*struck, NULL);
        *struck = (begun && mmgr_line_ok(&out)) ? out.out : NULL;
        if (begun && (*struck == NULL))
        {
            g_cli_cells->persistent_buf_release(out.out, NULL);
        }
    }
}

void cli_report_scrub(const char *text, const char *root, VerbaLinea *out)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    char *struck = g_cli_cells->persistent_buf_alloc(length + 1ull, NULL);
    if (struck != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = struck, .src = text, .bytes = length + 1u});
    }
    if (root != NULL)
    {
        cli_strike(&struck, root, "<tree>");
    }
    static const char *const s_homes[2] = {"USERPROFILE", "HOME"};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        char home[CLI_PATH_MAX_LEN];
        if (query_environment_read(s_homes[at], home, sizeof(home)))
        {
            cli_strike(&struck, home, "~");
        }
    }
    static const char *const s_users[2] = {"USERNAME", "USER"};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        char name[CLI_NAME_MAX_LEN];
        if (!query_environment_read(s_users[at], name, sizeof(name)))
        {
            continue;
        }
        static const char *const s_slashes[2] = {"\\", "/"};
        for (unsigned int slash = 0u; slash < 2u; slash += 1u)
        {
            char found[CLI_NAME_MAX_LEN + 4u];
            char put[16];
            VerbaLinea line = {found, sizeof(found), 0ull};
            mmgr_line_put(&line, s_slashes[slash]);
            mmgr_line_put(&line, name);
            mmgr_line_put(&line, s_slashes[slash]);
            mmgr_line_finish(&line);
            VerbaLinea user = {put, sizeof(put), 0ull};
            mmgr_line_put(&user, s_slashes[slash]);
            mmgr_line_put(&user, "<user>");
            mmgr_line_put(&user, s_slashes[slash]);
            mmgr_line_finish(&user);
            cli_strike(&struck, found, put);
        }
    }
    mmgr_line_put(out, (struck != NULL) ? struck : "");
    g_cli_cells->persistent_buf_release(struck, NULL);
}

void cli_report_fingerprint(const char *category, const char *text, char *out)
{
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (const char *at = category; at[0] != '\0'; at += 1)
    {
        hash = (hash ^ (unsigned char)at[0]) * 0x100000001b3ull;
    }
    for (const char *at = text; at[0] != '\0'; at += 1)
    {
        if ((at[0] < '0') || (at[0] > '9'))
        {
            hash = (hash ^ (unsigned char)at[0]) * 0x100000001b3ull;
        }
    }
    VerbaLinea line = {out, 17u, 0ull};
    mmgr_line_hex(&line, hash, 16u);
    mmgr_line_finish(&line);
}

void cli_report_saved(const char *text, const char *root)
{
    VerbaLinea line;
    if (!cli_growing_line_made(&line, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1ull))
    {
        return;
    }
    cli_report_scrub(text, root, &line);
    // an error stored is its first CLI_REPORT_ERROR bytes at the most
    line.at = (line.at < CLI_REPORT_ERROR) ? line.at : (CLI_REPORT_ERROR - 1u);
    line.out[(line.at < line.capacity) ? line.at : (line.capacity - 1u)] = '\0';
    cli_lock_take(&s_errors_lock);
    const unsigned int place = (s_first + s_count) % CLI_REPORT_RETAIN_MAX;
    g_cli_cells->persistent_buf_release(s_errors[place], NULL);
    s_errors[place] = line.out;
    if (s_count < CLI_REPORT_RETAIN_MAX)
    {
        s_count += 1u;
    }
    else
    {
        s_first = (s_first + 1u) % CLI_REPORT_RETAIN_MAX;
    }
    cli_lock_give(&s_errors_lock);
}

void cli_report_recent(VerbaLinea *out)
{
    cli_lock_take(&s_errors_lock);
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        mmgr_line_put(out, (at != 0u) ? "\n\n" : "");
        mmgr_line_put(out, s_errors[(s_first + at) % CLI_REPORT_RETAIN_MAX]);
    }
    cli_lock_give(&s_errors_lock);
}

const char *cli_report_panic_category(const char *trace)
{
    for (const char *line = trace; line[0] != '\0';)
    {
        const char *stop = line;
        while ((stop[0] != '\0') && (stop[0] != '\n'))
        {
            stop += 1;
        }
        char category_buf[CLI_PATH_MAX_LEN];
        VerbaLinea category_line = {category_buf, sizeof(category_buf), 0ull};
        mmgr_line_put_n(&category_line, line, (unsigned long long)(stop - line));
        mmgr_line_finish(&category_line);
        line = (stop[0] != '\0') ? (stop + 1) : stop;
        if (cellul.find(&(CatenaFinitaCfg){
                .src = category_buf, .cap = CLI_CELLS_POOL, .other = "orior_gui_lib::", .other_cap = CLI_CELLS_POOL}) !=
            NULL)
        {
            return "ui";
        }
        if ((cellul.find(&(CatenaFinitaCfg){.src = category_buf,
                                            .cap = CLI_CELLS_POOL,
                                            .other = "orior_cli::cli::",
                                            .other_cap = CLI_CELLS_POOL}) != NULL) ||
            (cellul.find(&(CatenaFinitaCfg){.src = category_buf,
                                            .cap = CLI_CELLS_POOL,
                                            .other = "orior_cli::main",
                                            .other_cap = CLI_CELLS_POOL}) != NULL))
        {
            return "cli";
        }
        if ((cellul.find(&(CatenaFinitaCfg){
                 .src = category_buf, .cap = CLI_CELLS_POOL, .other = "orior_cli::", .other_cap = CLI_CELLS_POOL}) !=
             NULL) &&
            (cellul.find(&(CatenaFinitaCfg){.src = category_buf,
                                            .cap = CLI_CELLS_POOL,
                                            .other = "orior_cli::report::",
                                            .other_cap = CLI_CELLS_POOL}) == NULL))
        {
            return "library";
        }
    }
    return "unknown";
}
