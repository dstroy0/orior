// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_test.c: the window's check: the window enters orior through cli_window_command_line and runs no main of
// orior's, so this check makes no pool of its own, and a call the window makes after the command line is answered in
// cells of the pool the command line made; the window's place for a tree kept and put back, fitted to the monitors
// there are now; a terminal screen's scrollback kept on disk and read back; a page of the tree served to a window of
// its own; and a terminal's shell given a line and its output sent to the page, then its end
#include "../../../../../ui/cli/src/window/serve.h"
#include "../../../../../ui/cli/src/window/serve_internal.h"
#include "../../../../../ui/cli/src/window/window.h"
#include "cli_check.h"

// whether the argument tree of the call that last ran `window_args_read` stood in the temporary tier
static int s_args_temporary;

// a call's answer that reads where its argument tree stands: a cell from the tree's tier lies between two temporary
// cells taken around it only where that tier is the temporary one
static int window_args_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)err_msg;
    char *const below = g_cli_cells->temporary_buf_alloc(16u, &g_checks_cells);
    s_args_temporary = (const char *)args->json->node > below;
    g_cli_cells->persistent_buf_release(below, &g_checks_cells);
    mmgr_line_put(out, "null");
    return 1;
}

// a call's argument tree lasts the call and stands in the temporary tier
static void window_args_checked(void)
{
    static const char s_args[] = "{\"key\":\"a call's own\"}";
    char answer[16];
    VerbaLinea out = {answer, sizeof(answer), 0ull};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check(cli_serve_answered(window_args_read, s_args, sizeof(s_args) - 1u, &out, &err_msg) && s_args_temporary,
              "a call's argument tree stands in the temporary tier");
}

// whether `place` stands at `x`, `y`, `width` by `height`, filling its monitor where `maximized`
static int window_place_is(const CliWindowPlace *place, int x, int y, unsigned int width, unsigned int height,
                           int maximized)
{
    return (place->x == x) && (place->y == y) && (place->width == width) && (place->height == height) &&
           (place->maximized == maximized);
}

// a place kept for a tree, and put back on the monitors there are: the main one alone, or it and one beside it
static void window_place_checked(void)
{
    char tree[CLI_PATH_MAX_LEN];
    cli_check_folder("window_place", tree);
    cli_serve_begun(tree, NULL);
    static const CliWindowArea s_main[1] = {{0, 0, 1920u, 1080u}};
    static const CliWindowArea s_both[2] = {{0, 0, 1920u, 1080u}, {1920, 0, 2560u, 1440u}};
    CliWindowPlace place = {0, 0, 0u, 0u, 0};
    cli_check(!cli_window_place_kept(s_both, 2u, &place), "a tree with no place kept has none");
    cli_window_place_keep(&(CliWindowPlace){2000, 100, 1440u, 920u, 1});
    cli_check(cli_window_place_kept(s_both, 2u, &place) && window_place_is(&place, 2000, 100, 1440u, 920u, 1),
              "a place on a monitor there is holds");
    cli_check(cli_window_place_kept(s_main, 1u, &place) && window_place_is(&place, 240, 80, 1440u, 920u, 1),
              "a place on a monitor gone goes to the middle of the main one");
    cli_check(!cli_window_place_kept(s_main, 0u, &place), "with no monitor there is no place");
    cli_window_place_keep(&(CliWindowPlace){-40, 30, 2400u, 1300u, 0});
    cli_check(cli_window_place_kept(s_both, 2u, &place) && window_place_is(&place, 0, 0, 1920u, 1080u, 0),
              "a place larger than its monitor is cut to it and moved in");
    cli_window_place_keep(&(CliWindowPlace){1200, 600, 1440u, 920u, 0});
    cli_check(cli_window_place_kept(s_main, 1u, &place) && window_place_is(&place, 480, 160, 1440u, 920u, 0),
              "a place hanging off its monitor is moved wholly onto it");
    cli_window_place_keep(&(CliWindowPlace){0, 0, 305u, 1392u, 0});
    cli_check(!cli_window_place_kept(s_main, 1u, &place), "a place narrower than the window's least width is none, the window opening at its first size");
    cli_window_place_keep(&(CliWindowPlace){0, 0, 1440u, 300u, 0});
    cli_check(!cli_window_place_kept(s_main, 1u, &place), "a place lower than its least height is none");
    cli_window_place_keep(&(CliWindowPlace){0, 0, CLI_PLACE_LEAST_WIDTH, CLI_PLACE_LEAST_HEIGHT, 0});
    cli_check(cli_window_place_kept(s_main, 1u, &place) && window_place_is(&place, 0, 0, CLI_PLACE_LEAST_WIDTH, CLI_PLACE_LEAST_HEIGHT, 0),
              "a place of the least size holds");
}

// the window's own call `name` with the JSON `args` answered, its answer, or why it failed, into `answer`, sized for
// `room`: 1 where it answered
static int window_called(const char *name, const char *args, char *answer, unsigned long long room)
{
    const unsigned int called =
        cli_window_called(1, name, args, cellul.len(&(CatenaFinitaCfg){.src = args, .cap = CLI_CELLS_POOL}));
    const char *text = NULL;
    unsigned long long size = 0ull;
    const int answered = cli_window_answer(called, &text, &size);
    VerbaLinea copied = {answer, room, 0ull};
    mmgr_line_put_n(&copied, text, size);
    (void)mmgr_line_finish(&copied);
    cli_window_given(called);
    return answered;
}

// the lines `first` up to `last` of the check's scrollback written as JSON strings, after a comma where `after`
static void window_lines_written(VerbaLinea *out, unsigned int first, unsigned int last, int after)
{
    for (unsigned int line = first; line < last; line += 1u)
    {
        mmgr_line_put(out, (after || (line > first)) ? ",\"<span>line " : "\"<span>line ");
        mmgr_line_decimal(out, line, 1u);
        mmgr_line_put(out, "</span>\"");
    }
}

// whether the scrollback `args` reads gives the lines `first` up to `last`
static int window_lines_read(const char *args, unsigned int first, unsigned int last)
{
    static char s_answer[1u << 16u];
    static char s_expected[1u << 16u];
    VerbaLinea expected = {s_expected, sizeof(s_expected), 0ull};
    mmgr_line_ch(&expected, '[');
    window_lines_written(&expected, first, last, 0);
    mmgr_line_ch(&expected, ']');
    (void)mmgr_line_finish(&expected);
    return window_called("scrollback_read", args, s_answer, sizeof(s_answer)) &&
           cellul.eq(&(CatenaFinitaCfg){.src = s_answer, .other = s_expected, .cap = sizeof(s_answer)});
}

// a scrollback's lines kept on disk and read back from anywhere, a newline in a line read back as a space, and the
// scrollback let go
static void window_scrollback_checked(void)
{
    static char s_args[1u << 16u];
    char answer[256];
    cli_check(window_called("scrollback_reset", "{}", answer, sizeof(answer)) &&
                  window_called("scrollback_open", "{}", answer, sizeof(answer)),
              "a scrollback is made");
    char id[32];
    VerbaLinea named = {id, sizeof(id), 0ull};
    mmgr_line_put(&named, answer);
    (void)mmgr_line_finish(&named);
    const unsigned int bounds[3] = {0u, 600u, 1000u};
    const char *const counts[2] = {"600", "1000"};
    for (unsigned int part = 0u; part < 2u; part += 1u)
    {
        VerbaLinea args = {s_args, sizeof(s_args), 0ull};
        mmgr_line_put(&args, "{\"id\":");
        mmgr_line_put(&args, id);
        mmgr_line_put(&args, ",\"lines\":[");
        window_lines_written(&args, bounds[part], bounds[part + 1u], 0);
        mmgr_line_put(&args, "]}");
        (void)mmgr_line_finish(&args);
        cli_check(window_called("scrollback_keep", s_args, answer, sizeof(answer)) &&
                      cellul.eq(&(CatenaFinitaCfg){.src = answer, .other = counts[part], .cap = sizeof(answer)}),
                  "a keep answers how many lines the scrollback holds");
    }
    const char *const reads[3] = {",\"from\":595,\"count\":10}", ",\"from\":990,\"count\":50}",
                                  ",\"from\":1000,\"count\":5}"};
    const unsigned int firsts[3] = {595u, 990u, 1000u};
    const unsigned int lasts[3] = {605u, 1000u, 1000u};
    for (unsigned int read = 0u; read < 3u; read += 1u)
    {
        VerbaLinea args = {s_args, sizeof(s_args), 0ull};
        mmgr_line_put(&args, "{\"id\":");
        mmgr_line_put(&args, id);
        mmgr_line_put(&args, reads[read]);
        (void)mmgr_line_finish(&args);
        cli_check(window_lines_read(s_args, firsts[read], lasts[read]), "lines kept are read back from anywhere");
    }
    VerbaLinea args = {s_args, sizeof(s_args), 0ull};
    mmgr_line_put(&args, "{\"id\":");
    mmgr_line_put(&args, id);
    mmgr_line_put(&args, ",\"lines\":[\"a\\nb\",\"\\u00e9\"]}");
    (void)mmgr_line_finish(&args);
    (void)window_called("scrollback_keep", s_args, answer, sizeof(answer));
    args.at = 0ull;
    mmgr_line_put(&args, "{\"id\":");
    mmgr_line_put(&args, id);
    mmgr_line_put(&args, ",\"from\":1000,\"count\":2}");
    (void)mmgr_line_finish(&args);
    cli_check(
        window_called("scrollback_read", s_args, answer, sizeof(answer)) &&
            cellul.eq(&(CatenaFinitaCfg){.src = answer, .other = "[\"a b\",\"\xc3\xa9\"]", .cap = sizeof(answer)}),
        "a newline in a line is read back as a space, and a letter of two bytes whole");
    args.at = 0ull;
    mmgr_line_put(&args, "{\"id\":");
    mmgr_line_put(&args, id);
    mmgr_line_ch(&args, '}');
    (void)mmgr_line_finish(&args);
    (void)window_called("scrollback_close", s_args, answer, sizeof(answer));
    args.at = 0ull;
    mmgr_line_put(&args, "{\"id\":");
    mmgr_line_put(&args, id);
    mmgr_line_put(&args, ",\"from\":0,\"count\":1}");
    (void)mmgr_line_finish(&args);
    cli_check(!window_called("scrollback_read", s_args, answer, sizeof(answer)), "a scrollback let go reads nothing");
}

// What a page window was served: its status, its media type and its bytes
typedef struct
{
    unsigned int status;
    char media_type[64];
    char bytes[64];
    unsigned long long size;
} CliViewServed;

// a file a page window asks for kept as it was served
static void window_view_kept(void *context, unsigned int status, const char *media_type, const unsigned char *bytes,
                             unsigned long long size)
{
    CliViewServed *const served = context;
    served->status = status;
    VerbaLinea type = {served->media_type, sizeof(served->media_type), 0ull};
    mmgr_line_put(&type, media_type);
    (void)mmgr_line_finish(&type);
    VerbaLinea body = {served->bytes, sizeof(served->bytes), 0ull};
    mmgr_line_put_n(&body, (const char *)bytes, size);
    (void)mmgr_line_finish(&body);
    served->size = size;
}

// a page of the tree given an address in the `view` scheme, its name's spaces and marks escaped, and served back from
// it; a file outside the tree refused
static void window_view_checked(void)
{
    char tree[CLI_PATH_MAX_LEN];
    cli_check_folder("window_place", tree);
    char folder[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    VerbaLinea folder_line = {folder, sizeof(folder), 0ull};
    mmgr_line_put(&folder_line, tree);
    mmgr_line_put(&folder_line, "/views/3");
    (void)mmgr_line_finish(&folder_line);
    (void)query_folder_made(folder);
    VerbaLinea file_line = {file, sizeof(file), 0ull};
    mmgr_line_put(&file_line, folder);
    mmgr_line_put(&file_line, "/a view#1.html");
    (void)mmgr_line_finish(&file_line);
    (void)query_file_written(file, "<p>page</p>", 11ull);
    char answer[512];
    cli_check(
        window_called("view_window", "{\"path\":\"views/3/a view#1.html\"}", answer, sizeof(answer)) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = answer, .cap = sizeof(answer), .other = "views/3/a%20view%231.html\"", .other_cap = 64u}) !=
             NULL),
        "a page of the tree gets an address, its space and its mark escaped");
    CliViewServed served;
    static const char s_path[] = "/views/3/a%20view%231.html";
    cli_window_view_served(s_path, sizeof(s_path) - 1u, window_view_kept, &served);
    cli_check((served.status == 200u) && (served.size == 11ull) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = served.bytes, .other = "<p>page</p>", .cap = 64u}) &&
                  cellul.starts(&(CatenaFinitaCfg){.src = served.media_type, .other = "text/html", .cap = 64u}),
              "the page is served back from its address, as HTML");
    static const char s_outside[] = "/../outside.html";
    cli_window_view_served(s_outside, sizeof(s_outside) - 1u, window_view_kept, &served);
    cli_check(served.status == 403u, "a file outside the tree is refused");
    cli_check(!window_called("view_window", "{\"path\":\"views/3/none.html\"}", answer, sizeof(answer)),
              "a page that is no file gets no window");
}

// What the check's terminals sent the page: the text of every `term-out`, joined, and how many `term-exit`s came, each
// kept under the lock as it comes and the check woken
typedef struct
{
    CliLock lock;
    CliWake wake;
    char said[1u << 16u];
    unsigned long long size;
    unsigned int ended;
} CliTerminalHeard;

static CliTerminalHeard s_heard = {CLI_LOCK_INIT, CLI_WAKE_INIT, {0}, 0ull, 0u};

// an event the window sends the page kept where it is a terminal's: of a `term-out`, the text between its `"text":"`
// and the `"}` that ends it
static void window_event_heard(void *context, const char *event, const char *body, unsigned long long size)
{
    (void)context;
    static const char s_text[] = "\"text\":\"";
    cli_lock_take(&s_heard.lock);
    if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "term-out", .cap = 64u}))
    {
        const char *const text =
            cellul.find(&(CatenaFinitaCfg){.src = body, .cap = size, .other = s_text, .other_cap = sizeof(s_text)});
        const unsigned long long from = (text != NULL) ? (unsigned long long)(text - body) + sizeof(s_text) - 1u : size;
        const unsigned long long to = (size >= 2ull) ? size - 2ull : 0ull;
        VerbaLinea said = {s_heard.said, sizeof(s_heard.said) - 1u, s_heard.size};
        mmgr_line_put_n(&said, body + from, (to > from) ? to - from : 0ull);
        s_heard.size = said.at;
        s_heard.said[said.at] = '\0';
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "term-exit", .cap = 64u}))
    {
        s_heard.ended += 1u;
    }
    cli_wake_all(&s_heard.wake);
    cli_lock_give(&s_heard.lock);
}

// whether what the terminals said holds `wanted` within `milliseconds`, woken as each event comes
static int window_terminal_said(const char *wanted, unsigned long long milliseconds)
{
    cli_lock_take(&s_heard.lock);
    unsigned long long waited = 0ull;
    int found = 0;
    while (!found && (waited < milliseconds))
    {
        found = cellul.find(&(CatenaFinitaCfg){
                    .src = s_heard.said, .cap = sizeof(s_heard.said), .other = wanted, .other_cap = 64u}) != NULL;
        if (!found)
        {
            (void)cli_wake_wait_for(&s_heard.wake, &s_heard.lock, 250ull);
            waited += 250ull;
        }
    }
    cli_lock_give(&s_heard.lock);
    return found;
}

// whether `count` terminals have ended within `milliseconds`, woken as each event comes
static int window_terminals_ended(unsigned int count, unsigned long long milliseconds)
{
    cli_lock_take(&s_heard.lock);
    unsigned long long waited = 0ull;
    while ((s_heard.ended < count) && (waited < milliseconds))
    {
        (void)cli_wake_wait_for(&s_heard.wake, &s_heard.lock, 250ull);
        waited += 250ull;
    }
    const int ended = s_heard.ended >= count;
    cli_lock_give(&s_heard.lock);
    return ended;
}

// a terminal opened on the reader's shell, given a line whose output the line itself does not contain, its output sent
// to the page, and its shell's end sent once it is told to exit
static void window_terminal_checked(void)
{
    cli_serve_emit_set(window_event_heard, NULL);
    char id[32];
    cli_check(window_called("term_open", "{\"cols\":80,\"rows\":24}", id, sizeof(id)) && (id[0] >= '1') &&
                  (id[0] <= '9'),
              "a terminal opens and is numbered");
    char args[256];
    char answer[256];
    VerbaLinea line = {args, sizeof(args), 0ull};
    mmgr_line_put(&line, "{\"id\":");
    mmgr_line_put(&line, id);
    mmgr_line_put(&line, ",\"text\":\"echo orior_$((6*7))_seen\\r\"}");
    (void)mmgr_line_finish(&line);
    cli_check(window_called("term_write", args, answer, sizeof(answer)), "a line is written to the terminal's shell");
    cli_check(window_terminal_said("orior_42_seen", 30000ull), "the shell's output is sent to the page");
    line.at = 0ull;
    mmgr_line_put(&line, "{\"id\":");
    mmgr_line_put(&line, id);
    mmgr_line_put(&line, ",\"text\":\"exit\\r\"}");
    (void)mmgr_line_finish(&line);
    (void)window_called("term_write", args, answer, sizeof(answer));
    cli_check(window_terminals_ended(1u, 15000ull), "the shell's end is sent to the page");
}

int main(void)
{
    char home[CLI_PATH_MAX_LEN];
    cli_check_folder("window_home", home);
    (void)query_environment_set("ORIOR_HOME", home);
    int window = 0;
    const int code = cli_window_command_line(&window);
    cli_check((code == 0) && (window == 1), "no words open the window");
    const unsigned int room = cli_window_called(1, "plugins_read", "{}", 2ull);
    const char *text = NULL;
    unsigned long long size = 0ull;
    const int answered = cli_window_answer(room, &text, &size);
    cli_check(answered && (size != 0ull), "the window's first call is answered in cells of the pool");
    cli_window_given(room);
    window_args_checked();
    window_place_checked();
    window_scrollback_checked();
    window_view_checked();
    window_terminal_checked();
    return cli_checked();
}
