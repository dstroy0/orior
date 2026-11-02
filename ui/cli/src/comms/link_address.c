// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link_address.c: an address of a tree on another machine read and written, the programs that run a shell line
// there, and a line run there and what it wrote read back
#include "../machine/net.h"
#include "../runs/runner.h"
#include "../text/json_tree.h"
#include "link.h"

#include "../text/text.h"

void cli_link_quote(const CliLinkTextSizeCfg *args)
{
    mmgr_line_ch(args->out, '\'');
    unsigned long long from = 0ull;
    for (unsigned long long at = 0ull; at < args->size; at += 1ull)
    {
        if (args->text[at] == '\'')
        {
            mmgr_line_put_n(args->out, args->text + from, at - from);
            mmgr_line_put(args->out, "'\\''");
            from = at + 1u;
        }
    }
    mmgr_line_put_n(args->out, args->text + from, args->size - from);
    mmgr_line_ch(args->out, '\'');
}

typedef struct
{
    unsigned int letter;
    const char *text;
    unsigned long long *start;
    unsigned long long *size;
    char *field;
    unsigned long long size_value;
} LinkAddressCtx;
// whether a letter is white as Rust's char::is_whitespace has it
EMBED_INLINE int address_white(const LinkAddressCtx *args)
{
    return ((args->letter >= 0x09u) && (args->letter <= 0x0du)) || (args->letter == 0x20u) || (args->letter == 0x85u) ||
           (args->letter == 0xa0u) || (args->letter == 0x1680u) ||
           ((args->letter >= 0x2000u) && (args->letter <= 0x200au)) || (args->letter == 0x2028u) ||
           (args->letter == 0x2029u) || (args->letter == 0x202fu) || (args->letter == 0x205fu) ||
           (args->letter == 0x3000u);
}

// `text` trimmed of white letters at its two ends, as Rust's str::trim trims it: its start and length
EMBED_INLINE void address_trim(const LinkAddressCtx *args)
{
    const char *text = args->text;
    unsigned long long *start = args->start;
    unsigned long long *size = args->size;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    unsigned long long begin = 0ull;
    unsigned int letter = 0u;
    while (begin < length)
    {
        const unsigned long long taken =
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                       .text_bytes = (const unsigned char *)text, .length = length, .at = begin, .letter = &letter);
        if (!EMBED_CALL(address_white, LinkAddressCtx, .letter = letter))
        {
            break;
        }
        begin += taken;
    }
    unsigned long long end = begin;
    unsigned long long at = begin;
    while (at < length)
    {
        const unsigned long long taken =
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                       .text_bytes = (const unsigned char *)text, .length = length, .at = at, .letter = &letter);
        at += taken;
        end = EMBED_CALL(address_white, LinkAddressCtx, .letter = letter) ? end : at;
    }
    *start = begin;
    *size = end - begin;
}

// `size` bytes of `text` stored as a field of an address: 1, or 0 where they are empty or do not fit
EMBED_INLINE int address_field_set(const LinkAddressCtx *args)
{
    if ((args->size_value == 0u) || (args->size_value >= CLI_PATH_MAX_LEN))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->field, .src = args->text, .bytes = (size_t)args->size_value});
    args->field[args->size_value] = '\0';
    return 1;
}

int cli_link_address_parse(const CliLinkTextCfg *args)
{
    unsigned long long start = 0ull;
    unsigned long long size = 0ull;
    EMBED_CALL(address_trim, LinkAddressCtx, .text = args->text, .start = &start, .size = &size);
    const char *const trimmed = args->text + start;
    const char *const first = memor.chr(&(MemoriaCfg){.src = trimmed, .val = ':', .bytes = (size_t)size});
    int read = first != NULL;
    if (read)
    {
        const unsigned long long first_size = (unsigned long long)(first - trimmed);
        const int docker =
            (first_size == 6u) && (memor.cmp(&(MemoriaCfg){.src = trimmed, .other = "docker", .bytes = 6u}) == 0);
        const int wsl =
            (first_size == 3u) && (memor.cmp(&(MemoriaCfg){.src = trimmed, .other = "wsl", .bytes = 3u}) == 0);
        const char *const rest = first + 1;
        const unsigned long long rest_size = size - first_size - 1u;
        if (docker || wsl)
        {
            const char *const second = memor.chr(&(MemoriaCfg){.src = rest, .val = ':', .bytes = (size_t)rest_size});
            read = second != NULL;
            if (read)
            {
                args->address_out->way = docker ? CLI_LINK_DOCKER : CLI_LINK_WSL;
                const unsigned long long name_size = (unsigned long long)(second - rest);
                read = EMBED_CALL(address_field_set, LinkAddressCtx, .field = args->address_out->name, .text = rest,
                                  .size_value = name_size) &&
                       EMBED_CALL(address_field_set, LinkAddressCtx, .field = args->address_out->folder,
                                  .text = second + 1, .size_value = rest_size - name_size - 1u);
            }
        }
        else
        {
            args->address_out->way = CLI_LINK_SSH;
            read = EMBED_CALL(address_field_set, LinkAddressCtx, .field = args->address_out->name, .text = trimmed,
                              .size_value = first_size) &&
                   EMBED_CALL(address_field_set, LinkAddressCtx, .field = args->address_out->folder, .text = rest,
                              .size_value = rest_size);
        }
    }
    if (!read)
    {
        mmgr_line_put_n(args->err_msg, trimmed, size);
        mmgr_line_put(args->err_msg, " names no machine and folder: user@host:folder, docker:container:folder or "
                                     "wsl:distribution:folder");
    }
    return read;
}

void cli_link_address_text(const CliLinkOutCfg *args)
{
    static const char *const ways[3] = {"", "docker:", "wsl:"};
    mmgr_line_put(args->out, ways[args->address->way]);
    mmgr_line_put(args->out, args->address->name);
    mmgr_line_ch(args->out, ':');
    mmgr_line_put(args->out, args->address->folder);
}

void cli_link_machine(const CliLinkOutCfg *args)
{
    const char *const at = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->address->name,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->address->name, .cap = CLI_CELLS_POOL}), .byte = '@');
    mmgr_line_put(args->out, ((args->address->way == CLI_LINK_SSH) && (at != NULL)) ? (at + 1) : args->address->name);
}

void cli_link_runs(const CliLinkAddressCfg *args)
{
    CliWords *words = args->words;
    memor.set(&(MemoriaCfg){.dst = words, .val = 0, .bytes = sizeof(*words)});
    if (args->address->way == CLI_LINK_SSH)
    {
        if (!EMBED_CALL(cli_runner_variable.program_words, CliRunnerVariableCfg, .variable = "ORIOR_SSH",
                        .words = words))
        {
            memor.set(&(MemoriaCfg){.dst = words, .val = 0, .bytes = sizeof(*words)});
            EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                       .word = "ssh");
        }
        if (args->terminal)
        {
            EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                       .word = "-tt");
        }
        else
        {
            EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                       .word = "-T");
            EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                       .word = "-o");
            EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                       .word = "BatchMode=yes");
        }
        static const char *const alive[4] = {"-o", "ServerAliveInterval=15", "-o", "ServerAliveCountMax=3"};
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                       .word = alive[at]);
        }
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                   .word = args->address->name);
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                   .word = args->line);
        return;
    }
    if (args->address->way == CLI_LINK_DOCKER)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                   .word = "docker");
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words, .word = "exec");
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                   .word = args->terminal ? "-it" : "-i");
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                   .word = args->address->name);
    }
    else
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words, .word = "wsl");
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words, .word = "-d");
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words,
                   .word = args->address->name);
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words, .word = "--");
    }
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words, .word = "sh");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words, .word = "-c");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words, .word = args->line);
}

void cli_link_shell_line(const CliLinkOutCfg *args)
{
    const char *const into = (args->folder != NULL) ? args->folder : args->address->folder;
    mmgr_line_put(args->out, "cd ");
    EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = into,
               .size = cellul.len(&(CatenaFinitaCfg){.src = into, .cap = CLI_CELLS_POOL}), .out = args->out);
    mmgr_line_put(args->out, " 2>/dev/null || cd; exec \"${SHELL:-sh}\" -l");
}

int cli_link_remote_run(const CliLinkSizeCfg *args)
{
    if ((args->address->way == CLI_LINK_SSH) &&
        !EMBED_CALL(cli_net_address.reach, CliNetAddressCfg, .address = args->address->name,
                    .purpose = "a command run over ssh", .err_msg = args->err_msg))
    {
        return 0;
    }
    CliWords words;
    EMBED_CALL(cli_link_address_table.runs, CliLinkAddressCfg, .address = args->address, .line = args->line,
               .terminal = 0, .words = &words);
    CliRan *const ran = cli_ran_take();
    const int started =
        !words.full &&
        EMBED_CALL(cli_words_internal.ran_feed, CliWordsCtx, .words = words.word, .folder = NULL, .settings = NULL,
                   .count = 0u, .input = (args->input != NULL) ? args->input : "",
                   .length = (args->input != NULL) ? args->size : 0ull, .ran = ran, .err_msg = args->err_msg);
    if (words.full)
    {
        mmgr_line_put(args->err_msg, "the line is longer than orior_cli runs");
    }
    const int done = started && (ran->code == 0);
    if (started && !done)
    {
        unsigned long long start = 0ull;
        unsigned long long length = 0ull;
        EMBED_CALL(address_trim, LinkAddressCtx, .text = ran->err, .start = &start, .size = &length);
        if (length > 0u)
        {
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                       .text_bytes = (const unsigned char *)ran->err + start, .length = length);
        }
        else
        {
            EMBED_CALL(cli_link_out.machine, CliLinkOutCfg, .address = args->address, .out = args->err_msg);
            mmgr_line_put(args->err_msg, " could not be reached");
        }
    }
    if (done)
    {
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)ran->out, .length = ran->out_size);
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return done;
}
