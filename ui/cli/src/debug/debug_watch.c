// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_watch.c: a stop where the bytes of a name change, as the adapter takes data breakpoints: each set again whole
// with each one added; and through lldb's own watchpoints where lldb-dap on Windows ends at reporting the stop of one
#include "debug_internal.h"

#if defined(_WIN32)
// the spaces a text is trimmed of
static const char s_spaces[] = " \t\n\r\f\v";

// the watchpoint's number lldb names in what it answers, "Watchpoint created: Watchpoint 1: addr = ...", into
// `number`: 1, or 0 where it names none
static int watch_number(const char *text, unsigned long long size, unsigned int *number)
{
    static const char s_mark[] = "Watchpoint ";
    const unsigned long long mark = sizeof(s_mark) - 1u;
    // each piece of the text between the marks, the first before any, read to its first colon
    for (unsigned long long start = 0ull; start <= size;)
    {
        unsigned long long end = start;
        while ((end + mark <= size) &&
               (memor.cmp(&(MemoriaCfg){.src = text + end, .other = s_mark, .bytes = mark}) != 0))
        {
            end += 1ull;
        }
        end = (end + mark <= size) ? end : size;
        unsigned long long colon = start;
        while ((colon < end) && (text[colon] != ':'))
        {
            colon += 1ull;
        }
        unsigned long long first = start;
        unsigned long long last = colon;
        while ((first < last) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = s_spaces, .cap = CLI_CELLS_POOL, .byte = text[first]}) != NULL))
        {
            first += 1ull;
        }
        while ((last > first) && (cellul.chr(&(CatenaFinitaCfg){
                                      .src = s_spaces, .cap = CLI_CELLS_POOL, .byte = text[last - 1ull]}) != NULL))
        {
            last -= 1ull;
        }
        unsigned long long value = 0ull;
        int digits = (last > first) && (last - first <= 10ull);
        for (unsigned long long at = first; digits && (at < last); at += 1ull)
        {
            digits = (text[at] >= '0') && (text[at] <= '9');
            value = (value * 10ull) + (unsigned long long)(text[at] - '0');
        }
        if (digits && (value <= 0xffffffffull))
        {
            *number = (unsigned int)value;
            return 1;
        }
        if (end == size)
        {
            break;
        }
        start = end + mark;
    }
    return 0;
}

// the bytes lldb names as "address/size" watched by a watchpoint of lldb's own, whose stop orior's script turns into a
// stop at the next instruction the program runs: 1, or 0 with why said
static int watch_by_lldb(CliDebugSession *session, const char *data, CliLine *said)
{
    const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = data, .cap = CLI_CELLS_POOL, .byte = '/'});
    if (slash == NULL)
    {
        cli_line_text(said, "lldb named the bytes ");
        cli_line_text(said, data);
        return 0;
    }
    const char *address = data;
    while ((address < slash) && (address[0] == '0') && ((address[1] == 'x') || (address[1] == 'X')))
    {
        address += 2;
    }
    char command[CLI_NAME_LONGEST * 2u];
    CliLine line = {command, sizeof(command), 0ull};
    cli_line_text(&line, "watchpoint set expression -w write -s ");
    cli_line_text(&line, slash + 1);
    cli_line_text(&line, " -- 0x");
    cli_line_counted(&line, address, (unsigned long long)(slash - address));
    cli_line_closed(&line);
    char heard[CLI_SAID_LONGEST];
    CliLine answer = {heard, sizeof(heard), 0ull};
    unsigned int number = 0u;
    if (!cli_debug_lldb_script(session, said) || !cli_debug_lldb_command(session, command, &answer, said))
    {
        return 0;
    }
    if (!watch_number(answer.out, answer.at, &number))
    {
        unsigned long long start = 0ull;
        unsigned long long end = answer.at;
        while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = s_spaces, .cap = CLI_CELLS_POOL, .byte = answer.out[start]}) != NULL))
        {
            start += 1ull;
        }
        while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = s_spaces, .cap = CLI_CELLS_POOL, .byte = answer.out[end - 1ull]}) != NULL))
        {
            end -= 1ull;
        }
        cli_line_counted(said, answer.out + start, end - start);
        return 0;
    }
    line.at = 0ull;
    cli_line_text(&line, "watchpoint command add -F orior_lldb.changed ");
    cli_line_decimal(&line, number, 1u);
    cli_line_closed(&line);
    answer.at = 0ull;
    if (!cli_debug_lldb_command(session, command, &answer, said))
    {
        return 0;
    }
    cli_lock_take(&g_cli_debug.lock);
    session->watching = 1;
    cli_lock_give(&g_cli_debug.lock);
    return 1;
}
#endif

// the data breakpoint `data` added to the session's and all of them set again
static int watch_set(CliDebugSession *session, const char *data, CliLine *said)
{
    CliDebugAnswer *const answer = cli_debug_answer_taken();
    cli_lock_take(&g_cli_debug.lock);
    session->data = cli_debug_grown(session->data, session->data_count, &session->data_room, sizeof(*session->data),
                                    CLI_DEBUG_DATA);
    const int room = (session->data_count < session->data_room);
    if (room)
    {
        memor.cpy(&(MemoriaCfg){.dst = session->data[session->data_count],
                                .src = data,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = data, .cap = CLI_CELLS_POOL}) + 1u});
        session->data_count += 1u;
    }
    cli_line_text(&answer->asked, "{\"breakpoints\":[");
    for (unsigned int at = 0u; at < session->data_count; at += 1u)
    {
        cli_line_text(&answer->asked, (at > 0u) ? ",{\"dataId\":" : "{\"dataId\":");
        cli_json_quoted_lossy(&answer->asked, (const unsigned char *)session->data[at],
                              cellul.len(&(CatenaFinitaCfg){.src = session->data[at], .cap = CLI_CELLS_POOL}));
        cli_line_text(&answer->asked, ",\"accessType\":\"write\"}");
    }
    cli_lock_give(&g_cli_debug.lock);
    cli_line_text(&answer->asked, "]}");
    if (!room)
    {
        cli_debug_answer_given(answer);
        cli_line_text(said, "the session watches as many bytes as it can already");
        return 0;
    }
    const int set = cli_debug_asked(session, "setDataBreakpoints", answer, CLI_DEBUG_ASKING, said);
    cli_debug_answer_given(answer);
    return set;
}

int cli_debug_watch_data(unsigned long long id, const char *name, int has_reference, long long reference, int has_bytes,
                         unsigned long long bytes, CliLine *out, CliLine *said)
{
    CliDebugSession *const session = cli_debug_taken(id, said);
    if (session == NULL)
    {
        return 0;
    }
    CliDebugAnswer *const answer = cli_debug_answer_taken();
    cli_line_text(&answer->asked, "{\"name\":");
    cli_json_quoted_lossy(&answer->asked, (const unsigned char *)name,
                          cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    if (has_reference)
    {
        cli_line_text(&answer->asked, ",\"variablesReference\":");
        cli_line_signed(&answer->asked, reference);
    }
    if (has_bytes)
    {
        cli_line_text(&answer->asked, ",\"asAddress\":true,\"bytes\":");
        cli_line_decimal(&answer->asked, bytes, 1u);
    }
    cli_line_character(&answer->asked, '}');
    if (!cli_debug_asked(session, "dataBreakpointInfo", answer, CLI_DEBUG_ASKING, said))
    {
        cli_debug_answer_given(answer);
        cli_debug_let_go(session);
        return 0;
    }
    const CliJson *const info = &answer->json;
    char data[CLI_NAME_LONGEST];
    char description[CLI_SAID_LONGEST];
    const char *const named = cli_json_string(info, cli_json_member(info, answer->root, "dataId"), NULL);
    const char *const described = cli_json_string(info, cli_json_member(info, answer->root, "description"), NULL);
    memor.cpy(&(MemoriaCfg){
        .dst = data,
        .src = (named != NULL) ? named : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (named != NULL) ? named : "", .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(
        &(MemoriaCfg){.dst = description, .bytes = sizeof(description), .src = (described != NULL) ? described : name});
    cli_debug_answer_given(answer);
    if (named == NULL)
    {
        cli_line_text(said, (described != NULL) ? description : "the adapter cannot watch that");
        cli_debug_let_go(session);
        return 0;
    }
    int watched = 0;
#if defined(_WIN32)
    // lldb-dap on Windows ends where it reports a stop at a watchpoint; the watchpoint is lldb's own, and its script
    // stops the program at the next instruction it runs
    if (cellul.eq(&(CatenaFinitaCfg){.src = session->program, .other = "lldb-dap", .cap = CLI_CELLS_POOL}))
    {
        watched = watch_by_lldb(session, data, said) ? 1 : -1;
    }
#endif
    if (watched == 0)
    {
        watched = watch_set(session, data, said) ? 1 : -1;
    }
    cli_debug_let_go(session);
    if (watched < 0)
    {
        return 0;
    }
    cli_json_quoted_lossy(out, (const unsigned char *)description,
                          cellul.len(&(CatenaFinitaCfg){.src = description, .cap = CLI_CELLS_POOL}));
    return 1;
}
