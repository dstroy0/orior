// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_run.c: git started at a tree with the words trust gives, its errors read on a thread of their own and its input
// written on another while its output is read, so none of the three waits on a pipe the others fill; what it gave kept
// in a room of the run's own, a cell taken for it
#include "../command_line/cells.h"
#include "../machine/trust.h"
#include "git_internal.h"

#include "../text/text.h"
#include <stddef.h>

// the most words git is started with
#define CLI_GIT_WORDS 8192u

// A room a run of git takes: what it gave and the bytes it points into, the process, the threads that read its errors
// and write its input and the count of them ended, the words it is started with, and what it wrote and said joined
typedef struct
{
    CliLock lock;
    CliWake wake;
    unsigned int ended;
    char out[CLI_GIT_OUT];
    char err[CLI_GIT_ERR];
    CliSpawned spawned;
    CliThread reader;
    CliThread writer;
    const char *input;
    unsigned long long length;
    const char *words[CLI_GIT_WORDS];
    char joined[CLI_GIT_OUT + CLI_GIT_ERR];
    CliGitGave gave;
} CliGitRoom;

// the empty gave handed out where the cells cannot meet a run, which every run of git refuses
static char s_nothing[1];
static CliGitGave s_refused = {s_nothing, 0ull, 1, s_nothing, 0ull, 0};

void cli_git_text_begun(CliGitText *text)
{
    text->bytes[0] = '\0';
    text->used = 1ull;
    text->full = 0;
}

unsigned int cli_git_text_put(CliGitText *text, const char *from, unsigned long long length)
{
    if ((text->used + length + 1ull) > CLI_GIT_TEXT)
    {
        text->full = 1;
        return 0u;
    }
    // a place is below CLI_GIT_TEXT, which an unsigned int holds
    const unsigned int at = (unsigned int)text->used;
    memor.cpy(&(MemoriaCfg){.dst = text->bytes + at, .src = from, .bytes = length});
    text->bytes[at + length] = '\0';
    text->used += length + 1ull;
    return at;
}

const char *cli_git_text_of(const CliGitText *text, unsigned int at)
{
    return text->bytes + at;
}

CliGitGave *cli_git_taken(void)
{
    CliGitRoom *const room = cli_cell_taken(sizeof(*room), "a run of git");
    if (room == NULL)
    {
        return &s_refused;
    }
    const CliLock lock = CLI_LOCK_INIT;
    const CliWake wake = CLI_WAKE_INIT;
    room->lock = lock;
    room->wake = wake;
    room->out[0] = '\0';
    room->err[0] = '\0';
    room->gave = (CliGitGave){room->out, 0ull, 1, room->err, 0ull, 0};
    return &room->gave;
}

// the room that holds `gave`, one cli_git_taken took from the cells
static CliGitRoom *cli_room_of(CliGitGave *gave)
{
    return (CliGitRoom *)(void *)((char *)gave - offsetof(CliGitRoom, gave));
}

void cli_git_given(CliGitGave *gave)
{
    if (gave != &s_refused)
    {
        cli_cell_given(cli_room_of(gave));
    }
}

// a thread of a run ended, told to the run
static void cli_ended(CliGitRoom *room)
{
    cli_lock_take(&room->lock);
    room->ended += 1u;
    cli_wake_all(&room->wake);
    cli_lock_give(&room->lock);
}

// what git says went wrong read until it ends, the bytes past the room read and let go
static void cli_errors_read(void *context)
{
    CliGitRoom *const room = (CliGitRoom *)context;
    CliGitGave *const gave = &room->gave;
    for (;;)
    {
        char spare[4096];
        const unsigned long long room_left = CLI_GIT_ERR - 1ull - gave->err_size;
        char *const into = (room_left != 0ull) ? (gave->err + gave->err_size) : spare;
        const unsigned long long got =
            cli_spawn_read(room->spawned.err, into, (room_left != 0ull) ? room_left : sizeof(spare));
        if (got == 0ull)
        {
            break;
        }
        gave->err_size += (room_left != 0ull) ? got : 0ull;
    }
    gave->err[gave->err_size] = '\0';
    cli_ended(room);
}

// the run's input written whole and its pipe closed, which git reads as its input's end
static void cli_input_written(void *context)
{
    CliGitRoom *const room = (CliGitRoom *)context;
    cli_spawn_written(room->spawned.in, room->input, room->length);
    cli_spawn_input_closed(&room->spawned);
    cli_ended(room);
}

int cli_git_gave(const char *root, const char *const *args, unsigned int count, unsigned int how, const char *index,
                 const char *input, unsigned long long length, CliGitGave *gave, CliLine *said)
{
    if (gave == &s_refused)
    {
        cli_line_text(said, "git: the cells cannot meet a run of git");
        return 0;
    }
    CliGitRoom *const room = cli_room_of(gave);
    gave->out_size = 0ull;
    gave->out_whole = 1;
    gave->err_size = 0ull;
    gave->out[0] = '\0';
    gave->err[0] = '\0';
    gave->ok = 0;
    if ((count + 4u) > CLI_GIT_WORDS)
    {
        cli_line_text(said, "git: too many words to start it with");
        return 0;
    }
    unsigned int words = 0u;
    room->words[words] = "git";
    words += 1u;
    words += cli_trust_git_words(root, room->words + words);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        room->words[words] = args[at];
        words += 1u;
    }
    room->words[words] = NULL;
    CliSetting settings[3];
    unsigned int set = 0u;
    if ((how & CLI_GIT_LOCKS_OFF) != 0u)
    {
        settings[set] = (CliSetting){"GIT_OPTIONAL_LOCKS", "0"};
        set += 1u;
    }
    if ((how & CLI_GIT_PROMPT_OFF) != 0u)
    {
        settings[set] = (CliSetting){"GIT_TERMINAL_PROMPT", "0"};
        set += 1u;
    }
    if (index != NULL)
    {
        settings[set] = (CliSetting){"GIT_INDEX_FILE", index};
        set += 1u;
    }
    const int started = (input != NULL) ? cli_spawn_fed(&room->spawned, room->words, root, settings, set, 1, said)
                                        : cli_spawn(&room->spawned, room->words, root, settings, set, 1, said);
    if (!started)
    {
        return 0;
    }
    room->ended = 0u;
    room->input = input;
    room->length = length;
    room->reader = (CliThread){cli_errors_read, room};
    room->writer = (CliThread){cli_input_written, room};
    // the reader and the writer each tell their end, on a thread of their own or, where none starts, here
    const unsigned int ending = (input != NULL) ? 2u : 1u;
    const int reading = cli_thread_started(&room->reader);
    if ((input != NULL) && !cli_thread_started(&room->writer))
    {
        cli_input_written(room);
    }
    for (;;)
    {
        char spare[4096];
        const unsigned long long room_left = CLI_GIT_OUT - 1ull - gave->out_size;
        char *const into = (room_left != 0ull) ? (gave->out + gave->out_size) : spare;
        const unsigned long long got =
            cli_spawn_read(room->spawned.out, into, (room_left != 0ull) ? room_left : sizeof(spare));
        if (got == 0ull)
        {
            break;
        }
        gave->out_size += (room_left != 0ull) ? got : 0ull;
        gave->out_whole = gave->out_whole && (room_left != 0ull);
    }
    gave->out[gave->out_size] = '\0';
    if (!reading)
    {
        cli_errors_read(room);
    }
    cli_lock_take(&room->lock);
    while (room->ended < ending)
    {
        cli_wake_wait(&room->wake, &room->lock);
    }
    cli_lock_give(&room->lock);
    gave->ok = cli_spawn_wait(&room->spawned) == 0ll;
    cli_spawn_closed(&room->spawned);
    return 1;
}

int cli_git_asked(const char *root, const char *const *args, unsigned int count, CliGitGave *gave)
{
    char why[CLI_NAME_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    return cli_git_gave(root, args, count, CLI_GIT_LOCKS_OFF, NULL, NULL, 0ull, gave, &said) && gave->ok &&
           gave->out_whole;
}

void cli_git_trimmed(const char **from, const char **to)
{
    while ((*from < *to) && ((**from == ' ') || ((**from >= '\t') && (**from <= '\r'))))
    {
        *from += 1;
    }
    while ((*to > *from) && (((*to)[-1] == ' ') || (((*to)[-1] >= '\t') && ((*to)[-1] <= '\r'))))
    {
        *to -= 1;
    }
}

int cli_git_ran(const char *root, const char *const *args, unsigned int count, const char *index, const char *input,
                unsigned long long length, CliLine *out, CliLine *said)
{
    CliGitGave *const gave = cli_git_taken();
    char why[CLI_PATH_LONGEST];
    CliLine started = {why, sizeof(why), 0ull};
    if (!cli_git_gave(root, args, count, CLI_GIT_PROMPT_OFF, index, input, length, gave, &started))
    {
        cli_line_text(said, "git: ");
        cli_line_counted(said, why, started.at);
        cli_git_given(gave);
        return 0;
    }
    CliGitRoom *const room = cli_room_of(gave);
    memor.cpy(&(MemoriaCfg){.dst = room->joined, .src = gave->out, .bytes = gave->out_size});
    memor.cpy(&(MemoriaCfg){.dst = room->joined + gave->out_size, .src = gave->err, .bytes = gave->err_size});
    const char *from = room->joined;
    const char *to = room->joined + gave->out_size + gave->err_size;
    cli_git_trimmed(&from, &to);
    const int ok = gave->ok;
    if (ok)
    {
        cli_line_counted(out, from, (unsigned long long)(to - from));
    }
    else if (from == to)
    {
        cli_line_text(said, "git ");
        cli_line_text(said, (count != 0u) ? args[0] : "");
        cli_line_text(said, " failed");
    }
    else
    {
        cli_line_counted(said, from, (unsigned long long)(to - from));
    }
    cli_git_given(gave);
    return ok;
}

int cli_git_run(const char *root, const char *const *args, unsigned int count, CliLine *out, CliLine *said)
{
    return cli_git_ran(root, args, count, NULL, NULL, 0ull, out, said);
}

int cli_git_is_id(const char *id)
{
    if (id[0] == '\0')
    {
        return 0;
    }
    for (const char *at = id; at[0] != '\0'; at += 1)
    {
        const char letter = at[0];
        if (!(((letter >= '0') && (letter <= '9')) || ((letter >= 'a') && (letter <= 'f')) ||
              ((letter >= 'A') && (letter <= 'F'))))
        {
            return 0;
        }
    }
    return 1;
}

int cli_git_beside(const char *root, const char *file, char *folder, char *shown, CliLine *said)
{
    char full[CLI_PATH_LONGEST];
    if (!cli_inside(root, file, full, said))
    {
        return 0;
    }
    if (!cli_path_parent(full, folder))
    {
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    }
    const unsigned long long root_length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    while (!query_is_folder(folder) && cli_text_same(folder, root, root_length) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = root, .cap = CLI_CELLS_POOL})))
    {
        char above[CLI_PATH_LONGEST];
        if (!cli_path_parent(folder, above))
        {
            memor.cpy(&(MemoriaCfg){.dst = above,
                                    .src = root,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
        }
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = above,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = above, .cap = CLI_CELLS_POOL}) + 1u});
    }
    char relative[CLI_PATH_LONGEST];
    cli_relative(folder, full, relative);
    CliLine line = {shown, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, "./");
    cli_line_text(&line, relative);
    if (cli_line_closed(&line) == 0ull)
    {
        cli_line_text(said, file);
        cli_line_text(said, ": its path is too long");
        return 0;
    }
    return 1;
}

const char *cli_git_field(const char **at, const char *end, char stop, unsigned long long *length)
{
    if (*at > end)
    {
        return NULL;
    }
    const char *const start = *at;
    const char *place = start;
    while ((place < end) && (place[0] != stop))
    {
        place += 1;
    }
    *length = (unsigned long long)(place - start);
    *at = place + 1;
    return start;
}

int cli_git_seconds(const char *text, unsigned long long length, long long *value)
{
    long long sign = 1ll;
    unsigned long long at = 0ull;
    if ((length > 0ull) && ((text[0] == '-') || (text[0] == '+')))
    {
        sign = (text[0] == '-') ? -1ll : 1ll;
        at = 1ull;
    }
    if (at == length)
    {
        return 0;
    }
    long long number = 0ll;
    for (; at < length; at += 1ull)
    {
        if ((text[at] < '0') || (text[at] > '9'))
        {
            return 0;
        }
        number = (number * 10ll) + (long long)(text[at] - '0');
    }
    *value = sign * number;
    return 1;
}
