// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reply.h: a build tool's JSON reply read off its bytes in one pass with no tree made: a member found by its key in
// an object, an array's items taken in turn, a string decoded and a number read, every value not asked for passed
// over by its brackets. CMake's file API and Meson's introspection write a known shape, so a reader walks it by key
// and the reply's bytes are the only room it takes (compile_db_reply.c)
#ifndef REPLY_H
#define REPLY_H

#include "../command_line/cli_internal.h"

// A reply: its bytes and their count, a file held mapped or a program's output as it came; and no value: a member an
// object does not hold, an item past an array's last, a reply with nothing in it
typedef struct
{
    const char *text;
    unsigned long long size;
} CliReply;
#define CLI_REPLY_NONE 0xffffffffffffffffull

// the reply file at `path` held mapped into `reply`: 1, or 0 with the reply empty where it cannot be read; and a reply
// held so let go. The reply's value at its start; the value of member `key` of the object at `object`; the first item
// of the array at `array`, the item after `item`, and the item at `index`; the place past the value at `at`; the
// bytes a string at `at` is written in, a bound on its letters decoded; the string at `at` decoded into `out`, which
// holds `capacity`: 1, or 0 where it is no string or does not fit; the whole number at `at`, all ones where none is;
// whether the value at `at` is `true`; and whether the string at `at` is `word`
int cli_reply_held(const char *path, CliReply *reply);
void cli_reply_let_go(CliReply *reply);
unsigned long long cli_reply_root(const CliReply *reply);
unsigned long long cli_reply_member(const CliReply *reply, unsigned long long object, const char *key);
unsigned long long cli_reply_first(const CliReply *reply, unsigned long long array);
unsigned long long cli_reply_next(const CliReply *reply, unsigned long long item);
unsigned long long cli_reply_item(const CliReply *reply, unsigned long long array, unsigned long long index);
unsigned long long cli_reply_value_end(const CliReply *reply, unsigned long long at);
unsigned long long cli_reply_string_span(const CliReply *reply, unsigned long long at);
int cli_reply_string(const CliReply *reply, unsigned long long at, char *out, unsigned long long capacity);
unsigned long long cli_reply_whole(const CliReply *reply, unsigned long long at);
int cli_reply_true(const CliReply *reply, unsigned long long at);
int cli_reply_string_is(const CliReply *reply, unsigned long long at, const char *word);

#endif
