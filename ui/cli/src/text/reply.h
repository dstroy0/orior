// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reply.h: a JSON reply, a build tool's or a kernel's, read off its bytes in one pass with no tree made: a member found
// by its key in an object, an array's items taken in turn, a string decoded and a number read, every value not
// requested for passed over by its brackets. CMake's file API and Meson's introspection write a known shape, so a
// reader walks it by key and the reply's bytes are the only capacity it takes (reply.c)
#ifndef REPLY_H
#define REPLY_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliReplyAtOutCfg CliReplyAtOutCfg;
typedef struct CliReplyArrayCfg CliReplyArrayCfg;
typedef struct CliReplyObjectCfg CliReplyObjectCfg;
typedef struct CliReplyAtCfg CliReplyAtCfg;
typedef struct CliReplyCfg CliReplyCfg;
typedef struct CliReplyPathCfg CliReplyPathCfg;

// A reply: its bytes and their count, a file mapped or a program's output as it came; and no value: a member an
// object does not contain, an item past an array's last, a reply with nothing in it
typedef struct
{
    const char *text;
    unsigned long long size;
} CliReply;
#define CLI_REPLY_NONE 0xffffffffffffffffull

// the reply file at `path` mapped into `reply`: 1, or 0 with the reply empty where it cannot be read; and a reply
// mapped so let go. The reply's value at its start; the value of member `key` of the object at `object`; the first item
// of the array at `array`, the item after `item`, and the item at `index`; the place past the value at `at`; the
// bytes a string at `at` is written in, a bound on its letters decoded; the string at `at` decoded into `out`, which
// is sized for `capacity`: 1, or 0 where it is no string or does not fit; the whole number at `at`, all ones where none
// is; whether the value at `at` is `true`; and whether the string at `at` is `word`
int cli_reply_read(const CliReplyPathCfg *args);
void cli_reply_let_go(const CliReplyPathCfg *args);
unsigned long long cli_reply_root(const CliReplyCfg *args);
unsigned long long cli_reply_member(const CliReplyObjectCfg *args);
unsigned long long cli_reply_first(const CliReplyArrayCfg *args);
unsigned long long cli_reply_next(const CliReplyCfg *args);
unsigned long long cli_reply_item(const CliReplyArrayCfg *args);
unsigned long long cli_reply_value_end(const CliReplyAtCfg *args);
unsigned long long cli_reply_string_span(const CliReplyAtCfg *args);
int cli_reply_string(const CliReplyAtOutCfg *args);
unsigned long long cli_reply_integer(const CliReplyAtCfg *args);
int cli_reply_true(const CliReplyAtCfg *args);
int cli_reply_string_is(const CliReplyAtCfg *args);

struct CliReplyAtOutCfg
{
    const CliReply *reply;
    unsigned long long at;
    char *out;
    unsigned long long capacity;
};

struct CliReplyArrayCfg
{
    const CliReply *reply;
    unsigned long long array;
    unsigned long long index;
};

struct CliReplyObjectCfg
{
    const CliReply *reply;
    unsigned long long object;
    const char *key;
};

struct CliReplyAtCfg
{
    const CliReply *reply;
    unsigned long long at;
    const char *word_text;
};

struct CliReplyCfg
{
    const CliReply *reply;
    unsigned long long item;
};

struct CliReplyPathCfg
{
    const char *path;
    CliReply *reply_out;
};

typedef struct
{
    int (*read)(const CliReplyPathCfg *args);
    void (*let_go)(const CliReplyPathCfg *args);
} CliReplyPathNs;
EMBED_TABLE_LAYOUT(CliReplyPathNs, read, let_go);
EMBED_TABLE_STORAGE CliReplyPathNs cli_reply_path EMBED_UNUSED = {
    .read = cli_reply_read,
    .let_go = cli_reply_let_go,
};

typedef struct
{
    unsigned long long (*root)(const CliReplyCfg *args);
    unsigned long long (*next)(const CliReplyCfg *args);
} CliReplyNs;
EMBED_TABLE_LAYOUT(CliReplyNs, root, next);
EMBED_TABLE_STORAGE CliReplyNs cli_reply EMBED_UNUSED = {
    .root = cli_reply_root,
    .next = cli_reply_next,
};

typedef struct
{
    unsigned long long (*member)(const CliReplyObjectCfg *args);
} CliReplyObjectNs;
EMBED_TABLE_LAYOUT(CliReplyObjectNs, member);
EMBED_TABLE_STORAGE CliReplyObjectNs cli_reply_object EMBED_UNUSED = {
    .member = cli_reply_member,
};

typedef struct
{
    unsigned long long (*first)(const CliReplyArrayCfg *args);
    unsigned long long (*item)(const CliReplyArrayCfg *args);
} CliReplyArrayNs;
EMBED_TABLE_LAYOUT(CliReplyArrayNs, first, item);
EMBED_TABLE_STORAGE CliReplyArrayNs cli_reply_array EMBED_UNUSED = {
    .first = cli_reply_first,
    .item = cli_reply_item,
};

typedef struct
{
    unsigned long long (*value_end)(const CliReplyAtCfg *args);
    unsigned long long (*string_span)(const CliReplyAtCfg *args);
    unsigned long long (*whole)(const CliReplyAtCfg *args);
    int (*reply_true)(const CliReplyAtCfg *args);
    int (*string_is)(const CliReplyAtCfg *args);
} CliReplyAtNs;
EMBED_TABLE_LAYOUT(CliReplyAtNs, value_end, string_span, whole, reply_true, string_is);
EMBED_TABLE_STORAGE CliReplyAtNs cli_reply_at EMBED_UNUSED = {
    .value_end = cli_reply_value_end,
    .string_span = cli_reply_string_span,
    .whole = cli_reply_integer,
    .reply_true = cli_reply_true,
    .string_is = cli_reply_string_is,
};

typedef struct
{
    int (*string)(const CliReplyAtOutCfg *args);
} CliReplyAtOutNs;
EMBED_TABLE_LAYOUT(CliReplyAtOutNs, string);
EMBED_TABLE_STORAGE CliReplyAtOutNs cli_reply_at_out EMBED_UNUSED = {
    .string = cli_reply_string,
};

#endif
