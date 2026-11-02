// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_terminal.c: the window's terminals: a shell each in a pseudo-terminal of its own, its output carried to
// the page as it comes and the keys typed carried back. The output reaches the page as `term-out` events, text cut only
// between whole letters, and the end of the shell as one `term-exit` with its exit code; a terminal is gone once its
// shell ends, and closing one hangs up on its shell. A shell on this machine starts at the folder the page names, else
// the tree's, else the reader's home, with orior's PATH and orior's own folder last on it, its part of the comms layer,
// and bash saying the folder it stands in before each prompt; where the tree is on another machine, the reader's shell
// there opens over the link
#include "../comms/link.h"
#include "../runs/runner.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "window_internal.h"

#define CLI_TERMINAL_PIECE 8192u
#define CLI_TERMINAL_BODY ((CLI_TERMINAL_PIECE * 6u) + 64u)
// One terminal: its number, 0 where the place is free; its shell; its key to the comms layer, where it has one; whether
// its output is still read and its shell still waited on, the place freed once neither is; the threads that do each;
// and the output read and the event it is sent in
typedef struct
{
    unsigned long long id;
    CliSpawned spawned;
    char key[CLI_COMMS_KEY];
    int keyed;
    int reading;
    int waiting;
    CliThread reader;
    CliThread waiter;
    char piece[CLI_TERMINAL_PIECE + 4u];
    char body[CLI_TERMINAL_BODY];
} CliTerminal;
// the most terminals open at once; the bytes of output read at once, and the event they are sent in, each read byte
// escaped as six at the most
#define CLI_TERMINALS 64u

// the most words a shell on this machine starts with, and the most settings of its environment
#define CLI_TERMINAL_WORDS 8u
#define CLI_TERMINAL_SETTINGS 8u

// whether the window runs on Windows, where a path starts at a drive's letter and a terminal's shell is Git's bash; the
// separator of the folders of a PATH; and the variable naming the reader's home
#if defined(_WIN32)
#define CLI_TERMINAL_WINDOWS 1
#define CLI_TERMINAL_PATH_SEPARATOR ";"
#define CLI_TERMINAL_HOME "USERPROFILE"
#else
#define CLI_TERMINAL_WINDOWS 0
#define CLI_TERMINAL_PATH_SEPARATOR ":"
#define CLI_TERMINAL_HOME "HOME"
#endif

// whether an exit code below 0 is a signal's end, which the page reads as no code, as on POSIX; on Windows every exit
// code is a 32-bit word the page reads whole
#if defined(_WIN32)
#define CLI_TERMINAL_CODE_SIGNALED 0
#else
#define CLI_TERMINAL_CODE_SIGNALED 1
#endif

static CliTerminal s_terminal[CLI_TERMINALS];
static unsigned long long s_next_id;
static CliLock s_lock = CLI_LOCK_INIT;

typedef struct
{
    unsigned long long id;
    CliTerminal *terminal;
    const char *bytes;
    unsigned long long size;
    char digit;
    const char *url;
    char *folder;
    const char *at;
    unsigned int cols;
    unsigned int rows;
    VerbaLinea *err_msg;
    const CliLinkAddress *address;
} WindowCallTerminalCtx;
// the terminal numbered `id`, or NULL where none is open; under the lock
EMBED_INLINE CliTerminal *terminal_get(const WindowCallTerminalCtx *args)
{
    for (unsigned int at = 0u; (args->id != 0ull) && (at < CLI_TERMINALS); at += 1u)
    {
        if (s_terminal[at].id == args->id)
        {
            return &s_terminal[at];
        }
    }
    return NULL;
}

// a terminal's place freed once its output is read to its end and its shell waited on; under the lock
EMBED_INLINE void terminal_release(const WindowCallTerminalCtx *args)
{
    if (!args->terminal->reading && !args->terminal->waiting)
    {
        EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &args->terminal->spawned);
        args->terminal->id = 0ull;
    }
}

// how many bytes at the start of `size` bytes of `bytes` are whole letters; a letter the end of a read cuts waits for
// the rest of it in the next
EMBED_INLINE unsigned long long terminal_whole(const WindowCallTerminalCtx *args)
{
    for (unsigned long long back = 1ull; (back <= 3ull) && (back <= args->size); back += 1ull)
    {
        const unsigned char letter = (unsigned char)args->bytes[args->size - back];
        if ((letter & 0xc0u) == 0x80u)
        {
            continue;
        }
        const unsigned long long needs =
            (letter >= 0xf0u) ? 4ull : ((letter >= 0xe0u) ? 3ull : ((letter >= 0xc0u) ? 2ull : 1ull));
        return (needs > back) ? (args->size - back) : args->size;
    }
    return args->size;
}

// the output a terminal's shell writes sent to the page as it comes, and its place freed once its shell is also waited
// on
static void terminal_read(void *context)
{
    CliTerminal *const terminal = context;
    unsigned long long pending = 0ull;
    for (;;)
    {
        const unsigned long long got = EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx,
                                                  .pipe_handle = terminal->spawned.out,
                                                  .piece = terminal->piece + pending, .room = CLI_TERMINAL_PIECE);
        if (got == 0ull)
        {
            break;
        }
        const unsigned long long size = pending + got;
        const unsigned long long whole =
            EMBED_CALL(terminal_whole, WindowCallTerminalCtx, .bytes = terminal->piece, .size = size);
        if (whole > 0ull)
        {
            VerbaLinea body = {terminal->body, sizeof(terminal->body), 0ull};
            mmgr_line_put(&body, "{\"id\":");
            mmgr_line_decimal(&body, terminal->id, 1u);
            mmgr_line_put(&body, ",\"text\":\"");
            EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = &body,
                       .text_bytes = (const unsigned char *)terminal->piece, .length = whole);
            mmgr_line_put(&body, "\"}");
            if (mmgr_line_ok(&body))
            {
                EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "term-out", .body = body.out,
                           .size = body.at);
            }
        }
        // the bytes of a letter the read cut, three at the most, moved to the piece's start for the rest of it
        for (unsigned long long at = whole; at < size; at += 1ull)
        {
            terminal->piece[at - whole] = terminal->piece[at];
        }
        pending = size - whole;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    terminal->reading = 0;
    EMBED_CALL(terminal_release, WindowCallTerminalCtx, .terminal = terminal);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// a terminal's shell waited on to its end: its part of the comms layer let go, the pseudo-terminal let go so that its
// reader meets the end of the output, the end sent to the page with the shell's exit code, and the place freed once its
// output is also read to its end
static void terminal_wait(void *context)
{
    CliTerminal *const terminal = context;
    const long long code = EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &terminal->spawned);
    if (terminal->keyed)
    {
        EMBED_CALL(cli_window_key_internal.terminal_leave, CliWindowKeyCtx, .key = terminal->key);
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    EMBED_CALL(cli_spawned_internal.spawn_terminal_close, CliSpawnedCtx, .spawned = &terminal->spawned);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    char text[96];
    VerbaLinea body = {text, sizeof(text), 0ull};
    mmgr_line_put(&body, "{\"id\":");
    mmgr_line_decimal(&body, terminal->id, 1u);
    mmgr_line_put(&body, ",\"code\":");
    if ((code < 0ll) && CLI_TERMINAL_CODE_SIGNALED)
    {
        mmgr_line_put(&body, "null");
    }
    else
    {
        // an exit code is a 32-bit word, read whole
        mmgr_line_decimal(&body, (unsigned long long)code & 0xffffffffull, 1u);
    }
    mmgr_line_ch(&body, '}');
    EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "term-exit", .body = text,
               .size = mmgr_line_ok(&body) ? body.at : 0ull);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    terminal->waiting = 0;
    EMBED_CALL(terminal_release, WindowCallTerminalCtx, .terminal = terminal);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// the value of the hexadecimal digit `digit`, or 16 where it is none
EMBED_INLINE unsigned int terminal_hex_get(const WindowCallTerminalCtx *args)
{
    // a digit's distance from the first of its run is below 16 where the digit is one
    return ((args->digit >= '0') && (args->digit <= '9'))   ? (unsigned int)(args->digit - '0')
           : ((args->digit >= 'a') && (args->digit <= 'f')) ? (unsigned int)(args->digit - 'a') + 10u
           : ((args->digit >= 'A') && (args->digit <= 'F')) ? (unsigned int)(args->digit - 'A') + 10u
                                                            : 16u;
}

// the folder a file URL names, as OSC 7 gives it, into `folder`, sized for CLI_PATH_MAX_LEN: its host taken off, its
// escapes read, and on Windows a path of Git's bash, `/d/x`, read as `D:/x`: 1, or 0 where it does not fit
EMBED_INLINE int terminal_folder_get(const WindowCallTerminalCtx *args)
{
    const char *path = args->url;
    if (cellul.starts(&(CatenaFinitaCfg){.src = args->url, .other = "file://", .cap = CLI_CELLS_POOL}))
    {
        const char *const slash =
            cellul.chr(&(CatenaFinitaCfg){.src = args->url + 7, .cap = CLI_CELLS_POOL, .byte = '/'});
        path = (slash != NULL) ? slash : (args->url + 7);
    }
    char read[CLI_PATH_MAX_LEN];
    VerbaLinea decoded = {read, sizeof(read), 0ull};
    EMBED_CALL(cli_text_size_set.put_unescaped, CliTextSizeSetCfg, .out = &decoded, .text = path,
               .size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    if (mmgr_line_finish(&decoded) == 0ull)
    {
        return 0;
    }
    VerbaLinea whole = {args->folder, CLI_PATH_MAX_LEN, 0ull};
    const int letter = ((read[1] >= 'a') && (read[1] <= 'z')) || ((read[1] >= 'A') && (read[1] <= 'Z'));
    const int drive = CLI_TERMINAL_WINDOWS && (read[0] == '/') && letter && ((read[2] == '/') || (read[2] == '\0'));
    if (drive)
    {
        // a drive's letter, written as Windows writes it
        mmgr_line_ch(&whole, (read[1] >= 'a') ? (char)(read[1] - ('a' - 'A')) : read[1]);
        mmgr_line_put(&whole, ":/");
        mmgr_line_put(&whole, (read[2] == '/') ? (read + 3) : "");
    }
    else
    {
        mmgr_line_put(&whole, read);
    }
    return mmgr_line_finish(&whole) != 0ull;
}

// the reader's shell on this machine started in `terminal`, at the folder `at` names where it is one; under the lock:
// 1, or 0 with why given
EMBED_INLINE int terminal_local_start(const WindowCallTerminalCtx *args)
{
    const char *at = args->at;
    unsigned int cols = args->cols;
    unsigned int rows = args->rows;
    VerbaLinea *err_msg = args->err_msg;
    static char s_folder[CLI_PATH_MAX_LEN];
    static char s_shell[CLI_PATH_MAX_LEN];
    static char s_path[CLI_PATH_TEXT];
    static char s_program[CLI_PATH_MAX_LEN];
    static char s_address[CLI_PATH_MAX_LEN];
    static char s_prompt[16];
    int found = (at != NULL) && EMBED_CALL(terminal_folder_get, WindowCallTerminalCtx, .url = at, .folder = s_folder) &&
                EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = s_folder);
    found = found || EMBED_CALL(cli_serve_err_msg_root.root_now, CliServeErrMsgRootCfg, .root_out = s_folder);
    found = found || (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx,
                                 .name = CLI_TERMINAL_HOME, .value_out = s_folder, .room = sizeof(s_folder)) &&
                      (s_folder[0] != '\0'));
    found = found || EMBED_CALL(cli_path_room_internal.folder_current, CliPathRoomCtx, .path_out = s_folder,
                                .room_size = sizeof(s_folder));
    if (!found)
    {
        mmgr_line_put(err_msg, "no folder to open a terminal in");
        return 0;
    }
    const char *words[CLI_TERMINAL_WORDS];
    unsigned int word = 0u;
    CliSetting settings[CLI_TERMINAL_SETTINGS];
    unsigned int count = 0u;
    // ORIOR_SHELL where it is set; on Windows Git's bash, the one the jobs run in, as a login shell told to stay in the
    // folder it starts in; elsewhere the reader's own SHELL, else bash, as a login shell
    const int named = EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_SHELL",
                                 .value_out = s_shell, .room = sizeof(s_shell));
    const int own_shell = !named && !CLI_TERMINAL_WINDOWS &&
                          EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "SHELL",
                                     .value_out = s_shell, .room = sizeof(s_shell));
    if (!named && CLI_TERMINAL_WINDOWS &&
        !EMBED_CALL(cli_runner_name.bash, CliRunnerNameCfg, .path = s_shell, .err_msg = err_msg))
    {
        return 0;
    }
    words[word] = (named || own_shell || CLI_TERMINAL_WINDOWS) ? s_shell : "bash";
    word += 1u;
    if (!named)
    {
        words[word] = CLI_TERMINAL_WINDOWS ? "--login" : "-l";
        word += 1u;
    }
    if (!named && CLI_TERMINAL_WINDOWS)
    {
        words[word] = "-i";
        word += 1u;
        settings[count] = (CliSetting){"CHERE_INVOKING", "1"};
        count += 1u;
    }
    words[word] = NULL;
    // orior's own folder comes last: `orior` runs in the terminal where no other is installed, and shadows no program
    // of the reader's
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_path, .size = sizeof(s_path));
    if (EMBED_CALL(cli_path_room_internal.program_own, CliPathRoomCtx, .path_out = s_program,
                   .room_size = sizeof(s_program)))
    {
        size_t end = cellul.len(&(CatenaFinitaCfg){.src = s_program, .cap = CLI_CELLS_POOL});
        end = (size_t)EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = s_program,
                                 .at = end, .set = "/\\");
        s_program[(end > 0u) ? end - 1u : 0u] = '\0';
        VerbaLinea path = {s_path, sizeof(s_path),
                           cellul.len(&(CatenaFinitaCfg){.src = s_path, .cap = sizeof(s_path)})};
        mmgr_line_put(&path, (path.at > 0ull) ? CLI_TERMINAL_PATH_SEPARATOR : "");
        mmgr_line_put(&path, s_program);
        (void)mmgr_line_finish(&path);
    }
    settings[count] = (CliSetting){"PATH", s_path};
    settings[count + 1u] = (CliSetting){"TERM", "xterm-256color"};
    settings[count + 2u] = (CliSetting){"COLORTERM", "truecolor"};
    count += 3u;
    // bash says the folder it stands in before each prompt, as OSC 7, where nothing of the reader's already sets what
    // it does before a prompt: the window saves the folder to open in again
    const size_t shell_length = cellul.len(&(CatenaFinitaCfg){.src = s_shell, .cap = CLI_CELLS_POOL});
    const int bash =
        !named &&
        (CLI_TERMINAL_WINDOWS || !own_shell ||
         ((shell_length >= 4u) &&
          cellul.eq(&(CatenaFinitaCfg){.src = s_shell + shell_length - 4u, .other = "bash", .cap = CLI_CELLS_POOL})));
    if (bash && !EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PROMPT_COMMAND",
                            .value_out = s_prompt, .room = sizeof(s_prompt)))
    {
        settings[count] =
            (CliSetting){"PROMPT_COMMAND", "printf '\\033]7;file://%s%s\\007' \"${HOSTNAME:-}\" \"$PWD\""};
        count += 1u;
    }
    // the shell, and every program run in it, joins the door with the key it is started with
    args->terminal->keyed = EMBED_CALL(cli_window_key_address_internal.terminal_key, CliWindowKeyAddressCtx,
                                       .key_out = args->terminal->key, .address = s_address);
    if (args->terminal->keyed)
    {
        settings[count] = (CliSetting){"ORIOR_COMMS", s_address};
        settings[count + 1u] = (CliSetting){"ORIOR_KEY", args->terminal->key};
        count += 2u;
    }
    const int started =
        EMBED_CALL(cli_words_err_msg_spawned_internal.spawn_terminal, CliWordsErrMsgSpawnedCtx,
                   .spawned = &args->terminal->spawned, .words = words, .folder = s_folder, .settings = settings,
                   .count = count, .cols = cols, .rows = rows, .err_msg = err_msg);
    if (started && args->terminal->keyed)
    {
        // a process id fits the 32 bits the system gives it
        EMBED_CALL(cli_window_key_internal.terminal_bind, CliWindowKeyCtx, .key = args->terminal->key,
                   .process = (unsigned int)args->terminal->spawned.id);
    }
    else if (args->terminal->keyed)
    {
        EMBED_CALL(cli_window_key_internal.terminal_leave, CliWindowKeyCtx, .key = args->terminal->key);
    }
    return started;
}

// the reader's shell on the tree's machine started in `terminal` over the link, at the folder `at` names, or the tree's
// folder where it names none; under the lock: 1, or 0 with why given
EMBED_INLINE int terminal_linked_start(const WindowCallTerminalCtx *args)
{
    CliTerminal *terminal = args->terminal;
    const CliLinkAddress *address = args->address;
    unsigned int cols = args->cols;
    unsigned int rows = args->rows;
    VerbaLinea *err_msg = args->err_msg;
    static char s_folder[CLI_PATH_MAX_LEN];
    static char s_line[CLI_PATH_MAX_LEN * 4u];
    static char s_found[CLI_PATH_MAX_LEN];
    static CliWords s_words;
    const char *folder = NULL;
    if ((args->at != NULL) &&
        EMBED_CALL(terminal_folder_get, WindowCallTerminalCtx, .url = args->at, .folder = s_folder))
    {
        EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = s_folder,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = s_folder, .cap = CLI_CELLS_POOL}), .from = '\\',
                   .to = '/');
        folder = s_folder;
    }
    VerbaLinea shell = {s_line, sizeof(s_line), 0ull};
    EMBED_CALL(cli_link_out.shell_line, CliLinkOutCfg, .address = address, .folder = folder, .out = &shell);
    (void)mmgr_line_finish(&shell);
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    EMBED_CALL(cli_link_address_table.runs, CliLinkAddressCfg, .address = address, .line = s_line, .terminal = 1,
               .words = &s_words);
    if (s_words.count == 0u)
    {
        mmgr_line_put(err_msg, "no shell opens on the tree's machine");
        return 0;
    }
    // the program as a file on the PATH: on Windows a pseudo-console runs the first file of the name it finds, and
    // docker's folder holds a script named docker beside docker.exe
    const char *const program = s_words.word[0];
    if (EMBED_CALL(cli_toolchains_names.on_path, CliToolchainsNamesCfg, .names = &program, .count = 1u,
                   .program = s_found))
    {
        s_words.word[0] = s_found;
    }
    s_words.word[s_words.count] = NULL;
    const CliSetting settings[1] = {{"TERM", "xterm-256color"}};
    terminal->keyed = 0;
    return EMBED_CALL(cli_words_err_msg_spawned_internal.spawn_terminal, CliWordsErrMsgSpawnedCtx,
                      .spawned = &terminal->spawned, .words = s_words.word, .folder = NULL, .settings = settings,
                      .count = 1u, .cols = cols, .rows = rows, .err_msg = err_msg);
}

// a terminal opened `cols` wide and `rows` tall at the folder the file URL `at` names: its number
static int terminal_open(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned long long cols = EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx,
                                               .request = arguments, .key = "cols", .otherwise_u64 = 80ull);
    const unsigned long long rows = EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx,
                                               .request = arguments, .key = "rows", .otherwise_u64 = 24ull);
    const char *const at = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                      .key = "at", .otherwise = NULL);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    CliTerminal *terminal = NULL;
    for (unsigned int place = 0u; (terminal == NULL) && (place < CLI_TERMINALS); place += 1u)
    {
        terminal = (s_terminal[place].id == 0ull) ? &s_terminal[place] : NULL;
    }
    CliLinkAddress address;
    // a terminal's size is far below what an unsigned int counts
    const unsigned int wide = (cols < 0xffffull) ? (unsigned int)cols : 0xffffu;
    const unsigned int tall = (rows < 0xffffull) ? (unsigned int)rows : 0xffffu;
    int started = 0;
    if (terminal == NULL)
    {
        mmgr_line_put(err_msg, "every terminal is taken");
    }
    else
    {
        started = EMBED_CALL(cli_link_text.address, CliLinkTextCfg, .address_out = &address)
                      ? EMBED_CALL(terminal_linked_start, WindowCallTerminalCtx, .terminal = terminal,
                                   .address = &address, .at = at, .cols = wide, .rows = tall, .err_msg = err_msg)
                      : EMBED_CALL(terminal_local_start, WindowCallTerminalCtx, .terminal = terminal, .at = at,
                                   .cols = wide, .rows = tall, .err_msg = err_msg);
    }
    if (started)
    {
        s_next_id += 1ull;
        terminal->id = s_next_id;
        terminal->reader = (CliThread){terminal_read, terminal, CLI_PRIORITY_WORK};
        terminal->waiter = (CliThread){terminal_wait, terminal, CLI_PRIORITY_WORK};
        terminal->reading = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &terminal->reader);
        terminal->waiting = terminal->reading &&
                            EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &terminal->waiter);
        if (!terminal->waiting)
        {
            // a shell no thread waits on is hung up on, and its place freed once its reader meets the output's end
            EMBED_CALL(cli_cols_internal.spawn_terminal_hung_up, CliColsCtx, .spawned_read = &terminal->spawned);
            EMBED_CALL(cli_spawned_internal.spawn_terminal_close, CliSpawnedCtx, .spawned = &terminal->spawned);
            EMBED_CALL(terminal_release, WindowCallTerminalCtx, .terminal = terminal);
            mmgr_line_put(err_msg, "a terminal's thread did not start");
            started = 0;
        }
        else
        {
            mmgr_line_decimal(args->out, terminal->id, 1u);
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return started;
}

// what was typed, `text`, written to the shell of the terminal `id`
static int terminal_write(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long id = 0ull;
    unsigned long long length = 0ull;
    const char *const text =
        EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "id", .value = &id,
                   .err_msg = err_msg)
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
                         .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments,
                                            .key = "text"),
                         .length_pointer = &length)
            : NULL;
    if (text == NULL)
    {
        mmgr_line_put(err_msg, (err_msg->at == 0ull) ? "text: a terminal is written text" : "");
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const CliTerminal *const terminal = EMBED_CALL(terminal_get, WindowCallTerminalCtx, .id = id);
    const int written =
        (terminal != NULL) && EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx,
                                         .pipe_handle = terminal->spawned.in, .bytes = text, .size = length);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    if (!written)
    {
        mmgr_line_put(err_msg, "no terminal ");
        mmgr_line_decimal(err_msg, id, 1u);
    }
    return written;
}

// the terminal `id` given a new size, `cols` wide and `rows` tall, which its shell and the programs in it are told of
static int terminal_resize(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long id = 0ull;
    unsigned long long cols = 0ull;
    unsigned long long rows = 0ull;
    if (!EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "id",
                    .value = &id, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "cols",
                    .value = &cols, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "rows",
                    .value = &rows, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const CliTerminal *const terminal = EMBED_CALL(terminal_get, WindowCallTerminalCtx, .id = id);
    // a terminal's size is far below what an unsigned int counts
    const int sized = (terminal != NULL) && EMBED_CALL(cli_cols_internal.spawn_terminal_resize, CliColsCtx,
                                                       .spawned_read = &terminal->spawned,
                                                       .cols = (cols < 0xffffull) ? (unsigned int)cols : 0xffffu,
                                                       .rows = (rows < 0xffffull) ? (unsigned int)rows : 0xffffu);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    if (terminal == NULL)
    {
        mmgr_line_put(err_msg, "no terminal ");
        mmgr_line_decimal(err_msg, id, 1u);
    }
    else if (!sized)
    {
        mmgr_line_put(err_msg, "the terminal could not be given its size");
    }
    return sized;
}

// the shell of the terminal `id` hung up on, which closes the terminal; a terminal already gone is closed
static int terminal_close(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long id = 0ull;
    if (!EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "id",
                    .value = &id, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const CliTerminal *const terminal = EMBED_CALL(terminal_get, WindowCallTerminalCtx, .id = id);
    if ((terminal != NULL) && terminal->waiting)
    {
        EMBED_CALL(cli_cols_internal.spawn_terminal_hung_up, CliColsCtx, .spawned_read = &terminal->spawned);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return 1;
}

const CliServeCall g_cli_window_terminal_calls[] = {
    {"term_open", terminal_open},
    {"term_write", terminal_write},
    {"term_resize", terminal_resize},
    {"term_close", terminal_close},
};
const unsigned int g_cli_window_terminal_count =
    sizeof(g_cli_window_terminal_calls) / sizeof(g_cli_window_terminal_calls[0]);
