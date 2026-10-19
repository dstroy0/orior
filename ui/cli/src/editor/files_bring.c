// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_bring.c: files and folders copied into the tree as a paste copies them, each file told as it is copied, and
// copied into a tree on another machine through calls of the tree's side there, a part at a time
#include "../text/base64.h"
#include "../text/json_tree.h"
#include "files_internal.h"

// how often a file brought into the tree tells how far along its copy is, in milliseconds; and how much of a file
// goes into one write here, or over the link in one part
#define CLI_BRING_TELLS 100ull
#define CLI_PART_BYTES (1ull << 20u)

// the bytes of a part's call written, its data in base64, and of an answer of the tree's side
#define CLI_PART_CALL ((CLI_PART_BYTES / 3ull * 4ull) + 8192ull)
#define CLI_ANSWER_BYTES (1u << 16u)

// What a bring copies: the tree, the path it was asked for, which a failure names, and who is told how far it is
typedef struct
{
    const char *root;
    const char *source;
    CliTold told;
    void *context;
} CliBringing;

// a folder made, or a file copied a piece at a time, told when it starts, every CLI_BRING_TELLS and once it is whole
static int cli_brought(void *context, const char *source, const char *target, int folder, CliLine *said)
{
    const CliBringing *const bringing = (const CliBringing *)context;
    if (folder)
    {
        if (query_folder_made(target))
        {
            return 1;
        }
        cli_line_text(said, bringing->source);
        cli_line_text(said, ": it could not be copied");
        return 0;
    }
    char path[CLI_PATH_LONGEST];
    cli_relative(bringing->root, target, path);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(source, &size);
    int written = bytes != NULL;
    if (written)
    {
        bringing->told(bringing->context, path, 0ull, size);
    }
    unsigned long long done = 0ull;
    unsigned long long last = cli_clock_milliseconds();
    int fresh = 1;
    while (written && (fresh || (done < size)))
    {
        const unsigned long long piece = ((size - done) < CLI_PART_BYTES) ? (size - done) : CLI_PART_BYTES;
        written = cli_file_appended(target, bytes + done, piece, fresh);
        fresh = 0;
        done += piece;
        if (written && ((cli_clock_milliseconds() - last) >= CLI_BRING_TELLS))
        {
            bringing->told(bringing->context, path, done, size);
            last = cli_clock_milliseconds();
        }
    }
    if (bytes != NULL)
    {
        qry_member_file_unmapped(bytes, size);
    }
    if (!written)
    {
        cli_line_text(said, bringing->source);
        cli_line_text(said, ": it could not be copied");
        return 0;
    }
    bringing->told(bringing->context, path, size, size);
    return 1;
}

int cli_files_bring(const char *root, const char *into, const char *const *from, unsigned int count, CliTold told,
                    void *context, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    if (!cli_inside(root, into, folder, said))
    {
        return 0;
    }
    if (!query_is_folder(folder))
    {
        cli_line_text(said, into);
        cli_line_text(said, " is no folder");
        return 0;
    }
    cli_line_character(out, '[');
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const char *const source = from[index];
        char name[CLI_PATH_LONGEST];
        if (!cli_paste_name(source, name))
        {
            cli_line_text(said, source);
            cli_line_text(said, " has no name");
            return 0;
        }
        if (query_is_folder(source) && cli_paste_under(source, folder))
        {
            cli_line_text(said, source);
            cli_line_text(said, " is not copied into itself");
            return 0;
        }
        char target[CLI_PATH_LONGEST];
        cli_paste_free(folder, name, target);
        CliBringing bringing = {root, source, told, context};
        if (!cli_paste_walked(source, target, cli_brought, &bringing, said))
        {
            return 0;
        }
        char was[CLI_PATH_LONGEST];
        const int inside = cli_paste_under(root, source);
        if (inside)
        {
            cli_relative(root, source, was);
        }
        char to[CLI_PATH_LONGEST];
        cli_relative(root, target, to);
        cli_pasted_written(out, index == 0u, inside ? was : NULL, to);
    }
    cli_line_character(out, ']');
    cli_files_forget_seen();
    return 1;
}

// What a bring over a link copies: the call of the tree's side, the path it was asked for, who is told how far it
// is, and room for a call's arguments and its answer
typedef struct
{
    CliCall call;
    void *call_context;
    CliTold told;
    void *context;
    char arguments[CLI_PART_CALL];
    char answer[CLI_ANSWER_BYTES];
} CliOver;

static CliOver s_over;

// a call of the tree's side with the arguments written into `arguments`: 1, or 0 with what it said
static int cli_over_called(const char *name, const CliLine *arguments, CliLine *said)
{
    CliLine answer = {s_over.answer, sizeof(s_over.answer), 0ull};
    return (cli_line_fits(arguments) != 0) && s_over.call(s_over.call_context, name, s_over.arguments, &answer, said);
}

// a folder made there, or a file read here and written there a part at a time
static int cli_over_each(void *context, const char *source, const char *target, int folder, CliLine *said)
{
    (void)context;
    CliLine arguments = {s_over.arguments, sizeof(s_over.arguments), 0ull};
    if (folder)
    {
        cli_line_text(&arguments, "{\"path\":");
        cli_json_quoted(&arguments, target, cellul.len(&(CatenaFinitaCfg){.src = target, .cap = CLI_CELLS_POOL}));
        cli_line_character(&arguments, '}');
        cli_line_closed(&arguments);
        return cli_over_called("folder_create", &arguments, said);
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(source, &size);
    if (bytes == NULL)
    {
        cli_line_text(said, "it could not be read");
        return 0;
    }
    s_over.told(s_over.context, target, 0ull, size);
    unsigned long long at = 0ull;
    int sent = 1;
    int last = 0;
    while (sent && !last)
    {
        const unsigned long long read = ((size - at) < CLI_PART_BYTES) ? (size - at) : CLI_PART_BYTES;
        last = ((at + read) >= size) || (read < CLI_PART_BYTES);
        arguments.at = 0ull;
        cli_line_text(&arguments, "{\"path\":");
        cli_json_quoted(&arguments, target, cellul.len(&(CatenaFinitaCfg){.src = target, .cap = CLI_CELLS_POOL}));
        cli_line_text(&arguments, ",\"at\":");
        cli_line_decimal(&arguments, at, 0u);
        cli_line_text(&arguments, ",\"data\":\"");
        cli_base64(&arguments, bytes + at, read);
        cli_line_text(&arguments, last ? "\",\"last\":true}" : "\",\"last\":false}");
        cli_line_closed(&arguments);
        sent = cli_over_called("file_part", &arguments, said);
        at += read;
        if (sent)
        {
            s_over.told(s_over.context, target, at, (size > at) ? size : at);
        }
    }
    qry_member_file_unmapped(bytes, size);
    return sent;
}

// the tree's side asked for the path of `name` in `into`, numbered where it holds it already, into `to`: 1, or 0 with
// why said
static int cli_over_named(const char *into, const char *name, char *to, CliLine *said)
{
    static CliJsonNode s_nodes[16];
    static char s_text[CLI_ANSWER_BYTES];
    static CfgJsonToken s_tokens[16];
    CliLine arguments = {s_over.arguments, sizeof(s_over.arguments), 0ull};
    cli_line_text(&arguments, "{\"into\":");
    cli_json_quoted(&arguments, into, cellul.len(&(CatenaFinitaCfg){.src = into, .cap = CLI_CELLS_POOL}));
    cli_line_text(&arguments, ",\"name\":");
    cli_json_quoted(&arguments, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    cli_line_character(&arguments, '}');
    cli_line_closed(&arguments);
    CliLine answer = {s_over.answer, sizeof(s_over.answer), 0ull};
    if (!s_over.call(s_over.call_context, "bring_name", s_over.arguments, &answer, said))
    {
        return 0;
    }
    CliJson json;
    cli_json_begun(&json, s_nodes, 16u, s_text, sizeof(s_text));
    char why[CLI_NAME_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_read(&json, s_over.answer, answer.at, s_tokens, 16u, &unread);
    unsigned long long length = 0ull;
    const char *const named = (root != CLI_JSON_NONE) ? cli_json_string(&json, root, &length) : NULL;
    if ((named == NULL) || (length >= CLI_PATH_LONGEST))
    {
        cli_line_text(said, "the tree's side gave no name");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = to, .src = named, .bytes = length});
    to[length] = '\0';
    return 1;
}

int cli_files_bring_over(CliCall call, void *call_context, const char *into, const char *const *from,
                         unsigned int count, CliTold told, void *context, CliLine *out, CliLine *said)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    s_over.call = call;
    s_over.call_context = call_context;
    s_over.told = told;
    s_over.context = context;
    cli_line_character(out, '[');
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const char *const source = from[index];
        char name[CLI_PATH_LONGEST];
        char to[CLI_PATH_LONGEST];
        if (!cli_paste_name(source, name))
        {
            cli_line_text(said, source);
            cli_line_text(said, " has no name");
            cli_lock_give(&s_lock);
            return 0;
        }
        if (!cli_over_named(into, name, to, said))
        {
            cli_lock_give(&s_lock);
            return 0;
        }
        char why[CLI_NAME_LONGEST * 4u];
        CliLine failed = {why, sizeof(why), 0ull};
        if (!cli_paste_walked(source, to, cli_over_each, NULL, &failed))
        {
            cli_line_text(said, source);
            cli_line_text(said, ": ");
            cli_line_counted(said, why, failed.at);
            cli_lock_give(&s_lock);
            return 0;
        }
        cli_pasted_written(out, index == 0u, NULL, to);
    }
    cli_line_character(out, ']');
    cli_lock_give(&s_lock);
    return 1;
}
