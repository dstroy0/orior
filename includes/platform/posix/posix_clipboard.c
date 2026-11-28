// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_clipboard.c: the system's clipboard on the POSIX systems: the pages of the UTF-16 units that a copy takes and
// a paste opens, and the text as UTF-8 that the system's clipboard takes made after them. The clipboard itself is the
// system's (linux/linux_clipboard.c, macos/macos_clipboard.c)
#if !defined(_WIN32)
// sockets, poll, gethostname and the descriptor flags are POSIX, outside strict C11
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include "../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../ui/cli/src/command_line/cli_internal.h"
#include "../../../ui/cli/src/text/text.h"
#include "posix_systems.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

// The pages that units_take takes from the system, NULL where none are out. They hold the UTF-16 units read into
// them, how many they hold and how many a write put in. After the units comes the same text as UTF-8, the form the
// system's clipboard takes, NULL until units_put makes it, and its bytes. The pages that units_open takes for a text
// the clipboard holds: the text as UTF-8 and after it as UTF-16 units. One thread at a time uses each
static unsigned char *s_units_pages;
static unsigned long long s_units_pages_size;
static unsigned short *s_units;
static unsigned long long s_units_capacity;
static unsigned long long s_units_at;
char *g_cli_clipboard_units_text;
unsigned long long g_cli_clipboard_units_text_size;
static unsigned char *s_held_pages;
static unsigned long long s_held_pages_size;
// the max bytes of a text the clipboard holds that a paste reads; the system commits only the pages the text fills
#define CLI_CLIPBOARD_PASTE_BYTES (64ULL * 1024ULL * 1024ULL)

// the pages units_take took given back to the system; on X11 the clipboard lets go of the text it served from them
void cli_clipboard_units_pages_free(void)
{
    cli_clipboard_units_released();
    if (s_units_pages != NULL)
    {
        platform_pages_released(s_units_pages, s_units_pages_size);
    }
    s_units_pages = NULL;
    s_units_pages_size = 0ULL;
    s_units = NULL;
    s_units_capacity = 0ULL;
    s_units_at = 0ULL;
    g_cli_clipboard_units_text = NULL;
    g_cli_clipboard_units_text_size = 0ULL;
}

int cli_clipboard_units_take(const CliClipboardUnitsCtx *args)
{
    cli_clipboard_units_pages_free();
    s_units_capacity = args->count + args->breaks;
    // a UTF-16 unit makes at most 3 bytes of UTF-8, and the UTF-8 text follows the units in the same pages
    s_units_pages_size = (s_units_capacity * sizeof(*s_units)) + (3ULL * s_units_capacity) + 1ULL;
    s_units_pages = platform_pages_mapped(s_units_pages_size);
    // the system's pages start on a page's boundary, which holds a unit
    s_units = (unsigned short *)(void *)s_units_pages;
    if (s_units_pages == NULL)
    {
        s_units_pages_size = 0ULL;
        s_units_capacity = 0ULL;
    }
    return s_units_pages != NULL;
}

int cli_clipboard_units_append(const CarcerMovableCfg *args)
{
    const unsigned long long count = args->bytes / sizeof(*s_units);
    if ((s_units == NULL) || (count > (s_units_capacity - s_units_at)))
    {
        return 0;
    }
    const int read = g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = args->cell, .offset = args->offset, .dst = s_units + s_units_at, .bytes = count * sizeof(*s_units)});
    s_units_at += read ? count : 0ULL;
    return read;
}

int cli_clipboard_units_append_text(const CliSizeTextCtx *args)
{
    if (s_units == NULL)
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        unsigned long long length = 0ULL;
        const unsigned int character =
            cli_text_character(&(CliTextAtCfg){.text = args->text, .at = at, .size = args->size, .length = &length});
        at += (length != 0ULL) ? length : 1ULL;
        const unsigned long long units = (character >= 0x10000u) ? 2ULL : 1ULL;
        if (units > (s_units_capacity - s_units_at))
        {
            return 0;
        }
        if (character >= 0x10000u)
        {
            s_units[s_units_at] = (unsigned short)(0xd800u + ((character - 0x10000u) >> 10u));
            s_units[s_units_at + 1ULL] = (unsigned short)(0xdc00u + ((character - 0x10000u) & 0x3ffu));
        }
        else
        {
            // a character below the second plane is one unit
            s_units[s_units_at] = (unsigned short)character;
        }
        s_units_at += units;
    }
    return 1;
}

int cli_clipboard_units_break(const CliClipboardUnitsCtx *args)
{
    (void)args;
    if ((s_units == NULL) || (s_units_at >= s_units_capacity))
    {
        return 0;
    }
    s_units[s_units_at] = (unsigned short)'\n';
    s_units_at += 1ULL;
    return 1;
}

// the units written made UTF-8 after them in the same pages, a pair of units one character: 1, or 0 where a write put
// none in
int cli_clipboard_units_text_made(void)
{
    if (s_units == NULL)
    {
        return 0;
    }
    // the UTF-8 text starts past the units the pages hold
    g_cli_clipboard_units_text = (char *)(void *)(s_units + s_units_capacity);
    VerbaLinea text = {g_cli_clipboard_units_text, (3ULL * s_units_capacity) + 1ULL, 0ULL};
    unsigned long long at = 0ULL;
    while (at < s_units_at)
    {
        unsigned int character = s_units[at];
        at += 1ULL;
        const int paired = (character >= 0xd800u) && (character <= 0xdbffu) && (at < s_units_at) &&
                           (s_units[at] >= 0xdc00u) && (s_units[at] <= 0xdfffu);
        if (paired)
        {
            character = 0x10000u + ((character - 0xd800u) << 10u) + ((unsigned int)s_units[at] - 0xdc00u);
            at += 1ULL;
        }
        cli_utf8_append(&(CliLineLetterCtx){.line = &text, .character = character});
    }
    g_cli_clipboard_units_text_size = text.at;
    return mmgr_line_ok(&text);
}

const unsigned short *cli_clipboard_units_open(const CliClipboardUnitsCtx *args)
{
    *args->count_out = 0ULL;
    cli_clipboard_units_close(args);
    // the text as UTF-8 at the pages' start, and after it as UTF-16 units, no more units than it has bytes
    s_held_pages_size = (CLI_CLIPBOARD_PASTE_BYTES * (1ULL + sizeof(unsigned short))) + sizeof(unsigned short);
    s_held_pages = platform_pages_mapped(s_held_pages_size);
    if (s_held_pages == NULL)
    {
        s_held_pages_size = 0ULL;
        return NULL;
    }
    VerbaLinea text = {(char *)s_held_pages, CLI_CLIPBOARD_PASTE_BYTES, 0ULL};
    if (!cli_clipboard_text_read(&(CliCtx){.text_verba_linea_pointer = &text}) || !mmgr_line_ok(&text))
    {
        cli_clipboard_units_close(args);
        return NULL;
    }
    // the units start past the text's bytes, at a boundary of a unit
    unsigned short *const units =
        (unsigned short *)(void *)(s_held_pages + CLI_CLIPBOARD_PASTE_BYTES + sizeof(unsigned short));
    unsigned long long count = 0ULL;
    for (unsigned long long at = 0ULL; at < text.at;)
    {
        unsigned long long length = 0ULL;
        const unsigned int character = cli_text_character(
            &(CliTextAtCfg){.text = (const char *)s_held_pages, .at = at, .size = text.at, .length = &length});
        at += (length != 0ULL) ? length : 1ULL;
        if (character >= 0x10000u)
        {
            units[count] = (unsigned short)(0xd800u + ((character - 0x10000u) >> 10u));
            units[count + 1ULL] = (unsigned short)(0xdc00u + ((character - 0x10000u) & 0x3ffu));
            count += 2ULL;
        }
        else
        {
            units[count] = (unsigned short)character;
            count += 1ULL;
        }
    }
    *args->count_out = count;
    return units;
}

void cli_clipboard_units_close(const CliClipboardUnitsCtx *args)
{
    (void)args;
    if (s_held_pages != NULL)
    {
        platform_pages_released(s_held_pages, s_held_pages_size);
    }
    s_held_pages = NULL;
    s_held_pages_size = 0ULL;
}
#endif
