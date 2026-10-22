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

// the bytes a call's arguments and an answer of the tree's side are begun to be written in, doubling as they fill
#define CLI_OVER_ARGUMENTS_FIRST 4096u
#define CLI_OVER_ANSWER_FIRST 1024u

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
    const unsigned char *const bytes = cli_file_held(source, &size);
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
        cli_file_let_go(bytes, size);
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
    if (!cli_paste_begun(root, into, folder, out, said))
    {
        return 0;
    }
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const char *const source = from[index];
        char name[CLI_PATH_LONGEST];
        if (!cli_paste_named(source, name, said))
        {
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

// What a bring over a link copies: the call of the tree's side, who is told how far it is, and a call's arguments and
// its answer, each a growing line given back once the bring ends
typedef struct
{
    CliCall call;
    void *call_context;
    CliTold told;
    void *context;
    CliLine arguments;
    CliLine answer;
} CliOver;

// a call of the tree's side with the arguments written into the bring's arguments, its answer into the bring's
// answer: 1, or 0 with what it said
static int cli_over_called(CliOver *over, const char *name, CliLine *said)
{
    over->answer.at = 0ull;
    if (!cli_line_fits(&over->arguments))
    {
        cli_line_text(said, "the cells cannot meet the call's arguments");
        return 0;
    }
    return over->call(over->call_context, name, over->arguments.out, &over->answer, said);
}

// a folder made there, or a file read here and written there a part at a time
static int cli_over_each(void *context, const char *source, const char *target, int folder, CliLine *said)
{
    CliOver *const over = context;
    CliLine *const arguments = &over->arguments;
    arguments->at = 0ull;
    if (folder)
    {
        cli_line_text(arguments, "{\"path\":");
        cli_json_quoted(arguments, target, cellul.len(&(CatenaFinitaCfg){.src = target, .cap = CLI_CELLS_POOL}));
        cli_line_character(arguments, '}');
        cli_line_closed(arguments);
        return cli_over_called(over, "folder_create", said);
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(source, &size);
    if (bytes == NULL)
    {
        cli_line_text(said, "it could not be read");
        return 0;
    }
    over->told(over->context, target, 0ull, size);
    unsigned long long at = 0ull;
    int sent = 1;
    int last = 0;
    while (sent && !last)
    {
        const unsigned long long read = ((size - at) < CLI_PART_BYTES) ? (size - at) : CLI_PART_BYTES;
        last = ((at + read) >= size) || (read < CLI_PART_BYTES);
        arguments->at = 0ull;
        cli_line_text(arguments, "{\"path\":");
        cli_json_quoted(arguments, target, cellul.len(&(CatenaFinitaCfg){.src = target, .cap = CLI_CELLS_POOL}));
        cli_line_text(arguments, ",\"at\":");
        cli_line_decimal(arguments, at, 0u);
        cli_line_text(arguments, ",\"data\":\"");
        cli_base64(arguments, bytes + at, read);
        cli_line_text(arguments, last ? "\",\"last\":true}" : "\",\"last\":false}");
        cli_line_closed(arguments);
        sent = cli_over_called(over, "file_part", said);
        at += read;
        if (sent)
        {
            over->told(over->context, target, at, (size > at) ? size : at);
        }
    }
    cli_file_let_go(bytes, size);
    return sent;
}

// the tree's side asked for the path of `name` in `into`, numbered where it holds it already, into `to`: 1, or 0 with
// why said
static int cli_over_named(CliOver *over, const char *into, const char *name, char *to, CliLine *said)
{
    CliLine *const arguments = &over->arguments;
    arguments->at = 0ull;
    cli_line_text(arguments, "{\"into\":");
    cli_json_quoted(arguments, into, cellul.len(&(CatenaFinitaCfg){.src = into, .cap = CLI_CELLS_POOL}));
    cli_line_text(arguments, ",\"name\":");
    cli_json_quoted(arguments, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    cli_line_character(arguments, '}');
    cli_line_closed(arguments);
    if (!cli_over_called(over, "bring_name", said))
    {
        return 0;
    }
    CliJson json = {0};
    char why[CLI_NAME_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_line_fits(&over->answer)
                                  ? cli_json_taken(&json, over->answer.out, over->answer.at, 0u, 0ull, &unread)
                                  : CLI_JSON_NONE;
    unsigned long long length = 0ull;
    const char *const named = (root != CLI_JSON_NONE) ? cli_json_string(&json, root, &length) : NULL;
    const int fits = (named != NULL) && (length < CLI_PATH_LONGEST);
    if (fits)
    {
        memor.cpy(&(MemoriaCfg){.dst = to, .src = named, .bytes = length});
        to[length] = '\0';
    }
    cli_json_given(&json);
    if (!fits)
    {
        cli_line_text(said, "the tree's side gave no name");
    }
    return fits;
}

// each path of `from` brought over into `into`, the names they took written into `out`: 1, or 0 with why said
static int cli_over_brought(CliOver *over, const char *into, const char *const *from, unsigned int count, CliLine *out,
                            CliLine *said)
{
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
            return 0;
        }
        if (!cli_over_named(over, into, name, to, said))
        {
            return 0;
        }
        char why[CLI_NAME_LONGEST * 4u];
        CliLine failed = {why, sizeof(why), 0ull};
        if (!cli_paste_walked(source, to, cli_over_each, over, &failed))
        {
            cli_line_text(said, source);
            cli_line_text(said, ": ");
            cli_line_counted(said, why, failed.at);
            return 0;
        }
        cli_pasted_written(out, index == 0u, NULL, to);
    }
    cli_line_character(out, ']');
    return 1;
}

int cli_files_bring_over(CliCall call, void *call_context, const char *into, const char *const *from,
                         unsigned int count, CliTold told, void *context, CliLine *out, CliLine *said)
{
    CliOver over = {
        call, call_context, told, context, {NULL, 0ull, 0ull, NULL, NULL, 0ull}, {NULL, 0ull, 0ull, NULL, NULL, 0ull}};
    (void)cli_line_growing(&over.arguments, CLI_OVER_ARGUMENTS_FIRST, "a call's arguments of a bring over a link");
    (void)cli_line_growing(&over.answer, CLI_OVER_ANSWER_FIRST, "an answer of the tree's side");
    const int brought = cli_over_brought(&over, into, from, count, out, said);
    cli_cell_given(over.arguments.out);
    cli_cell_given(over.answer.out);
    return brought;
}
