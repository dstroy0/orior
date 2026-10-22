// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// report.c: whether errors file on their own, kept in settings.json in orior's own folder with the window's own
// settings kept as they are; what identifies the reporter struck from a report; an error's fingerprint; and the run's
// recent errors
#include "report.h"
#include "../text/json_tree.h"

// the most recent errors kept for a report that asks for them, and the most bytes of each; the nodes and the bytes of
// texts a tree of the settings holds over for the answer, beyond its copy of the settings; and the bytes the settings
// written and a text struck are begun with, each growing past them
#define CLI_REPORT_KEPT 20u
#define CLI_REPORT_ERROR (1u << 16u)
#define CLI_REPORT_ANSWER_NODES 4u
#define CLI_REPORT_ANSWER_TEXT 64ull
#define CLI_REPORT_WRITTEN_FIRST 4096ull
#define CLI_REPORT_STRUCK_MORE 64ull

// the run's recent errors, a ring of cells each of its error's size, the oldest at `first`, under the lock
static char *s_errors[CLI_REPORT_KEPT];
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

// settings.json's path into `path`, which holds CLI_PATH_LONGEST: 1, or 0 where orior_cli has no folder of its own
static int cli_settings_path(char *path)
{
    char home[CLI_PATH_LONGEST];
    if (!cli_home_folder(home))
    {
        return 0;
    }
    CliLine line = {path, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, home);
    cli_line_text(&line, "/settings.json");
    return cli_line_closed(&line) != 0ull;
}

// settings.json read, the lock held, into a tree of its own sized to it, with room over for a copy of it and the
// answer where `copied` is 1: its object, or CLI_JSON_NONE where it is not there or does not read, the tree then
// holding room for the answer alone where `copied` is 1; the tree given back with cli_json_given
static unsigned int cli_settings(CliJson *json, int copied)
{
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
    char path[CLI_PATH_LONGEST];
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_settings_path(path) ? cli_file_held(path, &size) : NULL;
    // the file's bytes are read as its text
    const char *const text = (const char *)bytes;
    const unsigned int tokens = (copied && (bytes != NULL)) ? cli_json_tokens_counted(text, size) : 0u;
    char why[CLI_NAME_LONGEST];
    CliLine reason = {why, sizeof(why), 0ull};
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
        (void)cli_json_held(json, CLI_REPORT_ANSWER_NODES, CLI_REPORT_ANSWER_TEXT, "the settings");
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
    char none[CLI_NAME_LONGEST];
    int on = 0;
    return !query_environment_read("ORIOR_NO_REPORTS", none, sizeof(none)) && cli_answer(&on) && on;
}

int cli_report_asked(void)
{
    int on = 0;
    return cli_answer(&on);
}

void cli_report_ask_once(void)
{
    char none[CLI_NAME_LONGEST];
    if (cli_report_asked() || query_environment_read("ORIOR_NO_REPORTS", none, sizeof(none)) || !query_terminal_is() ||
        !cli_errors_terminal())
    {
        return;
    }
    static const char s_asked[] = CLI_REPORT_QUESTION "\nFile them? [Y/n] ";
    query_terminal_written(1, s_asked, sizeof(s_asked) - 1u);
    char answer[CLI_NAME_LONGEST];
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
    char why[CLI_PATH_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    if (!cli_report_set_auto(on, &said))
    {
        cli_line_character(&said, '\n');
        query_terminal_written(1, why, said.at);
    }
}

int cli_report_set_auto(int on, CliLine *said)
{
    char path[CLI_PATH_LONGEST];
    if (!cli_settings_path(path))
    {
        cli_line_text(said, "orior_cli has no folder of its own to keep settings in");
        return 0;
    }
    char folder[CLI_PATH_LONGEST];
    if (cli_path_parent(path, folder) && !query_folder_made(folder))
    {
        cli_line_text(said, folder);
        cli_line_text(said, ": it could not be made");
        return 0;
    }
    cli_lock_take(&s_settings_lock);
    CliJson json;
    const unsigned int kept = cli_settings(&json, 1);
    // the answer first, then the window's own settings as they are
    const unsigned int made = cli_json_made(&json, CFG_JSON_OBJECT);
    cli_json_set(&json, made, "auto_report", cli_json_made(&json, on ? CFG_JSON_TRUE : CFG_JSON_FALSE));
    for (unsigned int member = (kept != CLI_JSON_NONE) ? json.node[kept].first : CLI_JSON_NONE; member != CLI_JSON_NONE;
         member = json.node[member].next)
    {
        const char *const key = json.text + json.node[member].key;
        if (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "auto_report", .cap = CLI_CELLS_POOL}))
        {
            cli_json_set(&json, made, key, cli_json_copy(&json, member));
        }
    }
    CliLine out;
    const int begun = cli_line_growing(&out, CLI_REPORT_WRITTEN_FIRST, "the settings written");
    cli_json_write(&json, made, &out, 2u, 0);
    const int written = begun && !json.full && cli_line_fits(&out) && query_file_written(path, out.out, out.at);
    cli_cell_given(out.out);
    cli_json_given(&json);
    cli_lock_give(&s_settings_lock);
    if (!written)
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be written");
    }
    return written;
}

// a letter in lower case, ASCII's
static char cli_lower(char letter)
{
    return ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter + ('a' - 'A')) : letter;
}

// `text` with every `found` in it, ASCII's case aside, put as `put`, into `out`
static void cli_replaced(const char *text, const char *found, const char *put, CliLine *out)
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
            cli_line_text(out, put);
            at += length;
        }
        else
        {
            cli_line_character(out, at[0]);
            at += 1;
        }
    }
    cli_line_closed(out);
}

// `found`, three letters or more, struck from the text `*struck` holds as `put`, as it is, with its slashes leaning
// forward, and with them leaning back, each pass into a cell of its own grown to the text and the one before given
// back; where a cell is refused the text is let go whole, so nothing it held goes out unstruck
static void cli_strike(char **struck, const char *found, const char *put)
{
    if (cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL}) < 3u)
    {
        return;
    }
    char forms[3][CLI_PATH_LONGEST];
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
        CliLine out;
        const int begun = cli_line_growing(
            &out, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + CLI_REPORT_STRUCK_MORE,
            "a report's text struck");
        if (begun)
        {
            cli_replaced(text, forms[form], put, &out);
        }
        cli_cell_given(*struck);
        *struck = (begun && cli_line_fits(&out)) ? out.out : NULL;
        if (begun && (*struck == NULL))
        {
            cli_cell_given(out.out);
        }
    }
}

void cli_report_scrub(const char *text, const char *root, CliLine *out)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    char *struck = cli_cell_taken(length + 1ull, "a report's text struck");
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
        char home[CLI_PATH_LONGEST];
        if (query_environment_read(s_homes[at], home, sizeof(home)))
        {
            cli_strike(&struck, home, "~");
        }
    }
    static const char *const s_users[2] = {"USERNAME", "USER"};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        char name[CLI_NAME_LONGEST];
        if (!query_environment_read(s_users[at], name, sizeof(name)))
        {
            continue;
        }
        static const char *const s_slashes[2] = {"\\", "/"};
        for (unsigned int slash = 0u; slash < 2u; slash += 1u)
        {
            char found[CLI_NAME_LONGEST + 4u];
            char put[16];
            CliLine line = {found, sizeof(found), 0ull};
            cli_line_text(&line, s_slashes[slash]);
            cli_line_text(&line, name);
            cli_line_text(&line, s_slashes[slash]);
            cli_line_closed(&line);
            CliLine user = {put, sizeof(put), 0ull};
            cli_line_text(&user, s_slashes[slash]);
            cli_line_text(&user, "<user>");
            cli_line_text(&user, s_slashes[slash]);
            cli_line_closed(&user);
            cli_strike(&struck, found, put);
        }
    }
    cli_line_text(out, (struck != NULL) ? struck : "");
    cli_cell_given(struck);
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
    CliLine line = {out, 17u, 0ull};
    cli_line_hex(&line, hash, 16u);
    cli_line_closed(&line);
}

void cli_report_keep(const char *text, const char *root)
{
    CliLine line;
    if (!cli_line_growing(&line, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1ull,
                          "an error kept"))
    {
        return;
    }
    cli_report_scrub(text, root, &line);
    // an error kept is its first CLI_REPORT_ERROR bytes at the most
    line.at = (line.at < CLI_REPORT_ERROR) ? line.at : (CLI_REPORT_ERROR - 1u);
    line.out[(line.at < line.capacity) ? line.at : (line.capacity - 1u)] = '\0';
    cli_lock_take(&s_errors_lock);
    const unsigned int place = (s_first + s_count) % CLI_REPORT_KEPT;
    cli_cell_given(s_errors[place]);
    s_errors[place] = line.out;
    if (s_count < CLI_REPORT_KEPT)
    {
        s_count += 1u;
    }
    else
    {
        s_first = (s_first + 1u) % CLI_REPORT_KEPT;
    }
    cli_lock_give(&s_errors_lock);
}

void cli_report_recent(CliLine *out)
{
    cli_lock_take(&s_errors_lock);
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? "\n\n" : "");
        cli_line_text(out, s_errors[(s_first + at) % CLI_REPORT_KEPT]);
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
        char held[CLI_PATH_LONGEST];
        CliLine kept = {held, sizeof(held), 0ull};
        cli_line_counted(&kept, line, (unsigned long long)(stop - line));
        cli_line_closed(&kept);
        line = (stop[0] != '\0') ? (stop + 1) : stop;
        if (cellul.find(&(CatenaFinitaCfg){
                .src = held, .cap = CLI_CELLS_POOL, .other = "orior_gui_lib::", .other_cap = CLI_CELLS_POOL}) != NULL)
        {
            return "ui";
        }
        if ((cellul.find(&(CatenaFinitaCfg){
                 .src = held, .cap = CLI_CELLS_POOL, .other = "orior_cli::cli::", .other_cap = CLI_CELLS_POOL}) !=
             NULL) ||
            (cellul.find(&(CatenaFinitaCfg){
                 .src = held, .cap = CLI_CELLS_POOL, .other = "orior_cli::main", .other_cap = CLI_CELLS_POOL}) != NULL))
        {
            return "cli";
        }
        if ((cellul.find(&(CatenaFinitaCfg){
                 .src = held, .cap = CLI_CELLS_POOL, .other = "orior_cli::", .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = held, .cap = CLI_CELLS_POOL, .other = "orior_cli::report::", .other_cap = CLI_CELLS_POOL}) ==
             NULL))
        {
            return "library";
        }
    }
    return "unknown";
}
