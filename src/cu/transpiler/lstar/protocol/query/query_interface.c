// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_interface.c: a walk of asks put from inside the interface, every ending kept as the answer of the address that
// caused it
#include "query_interface.h"
#include "../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../engine/runtime/scriptura/scriptura.h"

#include <stdlib.h>
#include <string.h>

#define QUERY_INTERFACE_CHECK(condition_, evacaddr_, error_, kind_)                                                    \
    engine_error_check((condition_), (kind_), ENGINE_MODULE_INTERFACE, (unsigned int)__LINE__,                         \
                       (const void *)(evacaddr_), (error_))

// one line is an address and two numbers, at most 16 + 1 + 10 + 1 + 10 + 1 characters; this holds every line of
// the most addresses a probe is given
#define QUERY_INTERFACE_LINE 48ull
static char s_query_interface_lines[(QUERY_INTERFACE_MOST * QUERY_INTERFACE_LINE) + 1ull];

// `value` in hex written into `text`, which holds 24
static void query_interface_hex(char *text, unsigned long long value)
{
    ScripturaLine line = {text, 24ull, 0ull};
    scriptura_hex(&line, value, 0u);
    scriptura_closed(&line);
}

// The lines the walk's probe handed the run (QUERY_WALK_RECORD) after the place `began` of the run's schedule, once
// every ring is taken, joined into s_query_interface_lines in the order the writer took them and ended by a zero byte
static void query_interface_lines_gathered(unsigned long long began)
{
    qry_member_rings_settled();
    ScripturaLine lines = {s_query_interface_lines, sizeof(s_query_interface_lines), 0ull};
    unsigned long long at = began;
    const QryRecord *record = NULL;
    while (qry_member_read(&at, &record))
    {
        if ((record->name_length == (sizeof(QUERY_WALK_RECORD) - 1u)) &&
            (memcmp(QRY_RECORD_NAME(record), QUERY_WALK_RECORD, sizeof(QUERY_WALK_RECORD) - 1u) == 0))
        {
            scriptura_counted(&lines, (const char *)QRY_RECORD_BLOB(record), record->blob_length);
        }
    }
    scriptura_closed(&lines);
}

int query_interface_run(const QueryWalk *walk, const QueryInterfaceDriver *driver, unsigned long long *probes,
                        EngineError *error)
{
    *probes = 0ull;
    unsigned long long next = 0ull;
    while (next < walk->count)
    {
        const unsigned long long left = walk->count - next;
        const unsigned long long given = (left < QUERY_INTERFACE_MOST) ? left : QUERY_INTERFACE_MOST;
        char qualifier[24];
        char from[24];
        char count[24];
        char stride[24];
        char turns[24];
        char word[24];
        query_interface_hex(qualifier, walk->qualifier);
        query_interface_hex(from, walk->from + (next * walk->stride));
        query_interface_hex(count, given);
        query_interface_hex(stride, walk->stride);
        query_interface_hex(turns, walk->turns);
        query_interface_hex(word, walk->word);
        // the interface's command is a list of words it does not write to; the cast only meets its declared type
        char *const asked[] = {(char *)walk->program, qualifier, from, count, stride, turns, word, NULL};
        char *const spanned[] = {(char *)walk->program, from, count, stride, NULL};
        const InterfaceProbe probe = {(driver->asks != 0) ? asked : spanned, NULL, walk->limit_microseconds};
        InterfaceAnswer answer = {0};
        // the place the run's schedule holds before the probe starts: every line it hands stands past it
        const unsigned long long began = qry_member_settled();
        if (interface_probe_run(&probe, &answer, error) != 0L)
        {
            return 0;
        }
        query_interface_lines_gathered(began);
        *probes += 1ull;
        if (!QUERY_INTERFACE_CHECK(answer.ending != INTERFACE_ENDING_NOT_STARTED, walk, error, ENGINE_ERROR_RESOURCE))
        {
            return 0;
        }
        const unsigned long long read = driver->read(walk, s_query_interface_lines, next, given, driver->answers);
        next += read;
        if ((answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 0ull))
        {
            // a probe that exited clean answered every address it was given, or it wrote something else
            if (!QUERY_INTERFACE_CHECK(read == given, walk, error, ENGINE_ERROR_LOGIC))
            {
                return 0;
            }
            continue;
        }
        // a probe that wrote a line for every address it was given and then ended answered them all, and its
        // ending is no address's answer
        if (read == given)
        {
            continue;
        }
        // the probe ended at the address after its last line: that address is answered by the ending
        driver->ended(walk, next, answer.fault, driver->answers);
        next += 1ull;
    }
    return 1;
}

// The lines query_walk wrote, read into the asks `answers` holds from `next` on. How many it answered in order; a
// line out of order, past the addresses it was given, or not three numbers ends the read where it stands
static unsigned long long query_interface_read(const QueryWalk *walk, const char *lines, unsigned long long next,
                                               unsigned long long given, void *answers)
{
    QueryAsk *const asks = (QueryAsk *)answers;
    unsigned long long read = 0ull;
    const char *at = lines;
    while (read < given)
    {
        char *end = NULL;
        const unsigned long long address = strtoull(at, &end, 16);
        if (end == at)
        {
            break;
        }
        at = end;
        // a bit and a kind are each one small number, kept at the width of the fields that hold them
        const unsigned int bit = (unsigned int)strtoul(at, &end, 10);
        if (end == at)
        {
            break;
        }
        at = end;
        // as above
        const unsigned int kind = (unsigned int)strtoul(at, &end, 10);
        if ((end == at) || (address != (walk->from + ((next + read) * walk->stride))))
        {
            break;
        }
        at = end;
        QueryAsk *const answer = &asks[next + read];
        answer->address = address;
        answer->qualifier = walk->qualifier;
        answer->word = walk->word;
        answer->bit = bit;
        answer->kind = kind;
        answer->fault = 0u;
        read += 1ull;
    }
    return read;
}

// the address `next` answered QUERY_ENDED, with the fault the interface named
static void query_interface_ended(const QueryWalk *walk, unsigned long long next, InterfaceFault fault, void *answers)
{
    QueryAsk *const ended = &((QueryAsk *)answers)[next];
    ended->address = walk->from + (next * walk->stride);
    ended->qualifier = walk->qualifier;
    ended->word = walk->word;
    ended->bit = 0u;
    ended->kind = QUERY_ENDED;
    // an InterfaceFault is a small count, which the field holds
    ended->fault = (unsigned int)fault;
}

unsigned long long query_interface_walk(const QueryWalk *walk, QueryAsk *answers, EngineError *error)
{
    const QueryInterfaceDriver driver = {1, query_interface_read, query_interface_ended, answers};
    unsigned long long probes = 0ull;
    return (query_interface_run(walk, &driver, &probes, error) != 0) ? probes : 0ull;
}
