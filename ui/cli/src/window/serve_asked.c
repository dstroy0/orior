// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_asked.c: a call answered later. Where its arguments carry `ask`, a number the page chose, the call runs on a
// thread of its own and answers `{"asked":true}` at once, its room given back; what it answers, or why it failed,
// goes to the page as the event `call-answered` with that number. Without `ask` the call answers in place. At most
// CLI_SERVE_ASKS_MOST run at once; one more is refused at once with why said
#include "serve_internal.h"

#define CLI_SERVE_ASKS_MOST 32u
// the bytes an asked call's answer and its event are begun to be written in, doubling as they fill
#define CLI_SERVE_ASKED_FIRST 4096ull

// A call running on its own: the thread, what answers it, the ask's number, and the arguments as text
typedef struct
{
    CliThread thread;
    CliServeAnswer answer;
    unsigned long long ask;
    unsigned long long size;
    char args[];
} CliServeAsked;

static CliLock s_lock = CLI_LOCK_INIT;
static unsigned int s_running;

// the event of an ask's answer told: `{"ask", "answer"}`, or `{"ask", "error"}` with why
static void asked_told(unsigned long long ask, int ok, const CliLine *out, const CliLine *said)
{
    CliLine told;
    if (!cli_line_growing(&told, CLI_SERVE_ASKED_FIRST, "an asked call's event"))
    {
        return;
    }
    cli_line_text(&told, "{\"ask\":");
    cli_line_decimal(&told, ask, 1u);
    if (ok)
    {
        cli_line_text(&told, ",\"answer\":");
        if (out->at > 0ull)
        {
            cli_line_counted(&told, out->out, out->at);
        }
        else
        {
            cli_line_text(&told, "null");
        }
    }
    else
    {
        cli_line_text(&told, ",\"error\":");
        cli_json_quoted(&told, said->out, said->at);
    }
    cli_line_character(&told, '}');
    if (cli_line_fits(&told))
    {
        cli_serve_emitted("call-answered", told.out, told.at);
    }
    cli_cell_given(told.out);
}

// the call run where it was asked, its answer told, and the ask's cell given back
static void asked_run(void *context)
{
    CliServeAsked *const asked = context;
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out;
    const int grown = cli_line_growing(&out, CLI_SERVE_ASKED_FIRST, "an asked call's answer");
    if (!grown)
    {
        cli_line_text(&said, "the answer could not be given a cell");
    }
    int ok = grown && cli_serve_answered(asked->answer, asked->args, asked->size, &out, &said);
    if (ok && !cli_line_fits(&out))
    {
        said.at = 0ull;
        cli_line_text(&said, "the answer does not fit the room it is made in");
        ok = 0;
    }
    asked_told(asked->ask, ok, &out, &said);
    if (grown)
    {
        cli_cell_given(out.out);
    }
    cli_lock_take(&s_lock);
    s_running -= 1u;
    cli_lock_give(&s_lock);
    cli_cell_given(asked);
}

int cli_serve_asked_or(const CliServeArgs *args, CliServeAnswer answer, CliLine *out, CliLine *said)
{
    if (cli_serve_arg(args, "ask") == CLI_JSON_NONE)
    {
        return answer(args, out, said);
    }
    unsigned long long ask = 0ull;
    if (!cli_serve_whole(args, "ask", &ask, said))
    {
        return 0;
    }
    // the arguments written again as text, so the thread reads them once the room they came in is given back
    CliLine text;
    if (!cli_line_growing(&text, CLI_SERVE_ASKED_FIRST, "an asked call's arguments"))
    {
        cli_line_text(said, "the asked call's arguments could not be given a cell");
        return 0;
    }
    cli_json_write(args->json, args->object, &text, 0u, 0);
    CliServeAsked *const asked =
        cli_line_fits(&text) ? cli_cell_taken(sizeof(*asked) + text.at + 1ull, "an asked call") : NULL;
    if (asked == NULL)
    {
        cli_cell_given(text.out);
        cli_line_text(said, "the asked call could not be given a cell");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = asked->args, .src = text.out, .bytes = text.at});
    asked->args[text.at] = '\0';
    asked->size = text.at;
    asked->answer = answer;
    asked->ask = ask;
    asked->thread = (CliThread){asked_run, asked, CLI_PRIORITY_WORK};
    cli_cell_given(text.out);
    cli_lock_take(&s_lock);
    const int room = s_running < CLI_SERVE_ASKS_MOST;
    s_running += room ? 1u : 0u;
    cli_lock_give(&s_lock);
    if (!room)
    {
        cli_cell_given(asked);
        cli_line_text(said, "too many calls are asked at once");
        return 0;
    }
    if (!cli_thread_started(&asked->thread))
    {
        cli_lock_take(&s_lock);
        s_running -= 1u;
        cli_lock_give(&s_lock);
        cli_cell_given(asked);
        cli_line_text(said, "the asked call's thread did not start");
        return 0;
    }
    cli_line_text(out, "{\"asked\":true}");
    return 1;
}
