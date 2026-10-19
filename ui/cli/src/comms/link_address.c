// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link_address.c: an address of a tree on another machine read and written, the programs that run a shell line
// there, and a line run there and what it wrote read back
#include "../machine/net.h"
#include "../runs/runner.h"
#include "../text/json_tree.h"
#include "link.h"

#include "../text/text.h"

void cli_link_quote(const char *text, unsigned long long size, CliLine *out)
{
    cli_line_character(out, '\'');
    unsigned long long from = 0ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        if (text[at] == '\'')
        {
            cli_line_counted(out, text + from, at - from);
            cli_line_text(out, "'\\''");
            from = at + 1u;
        }
    }
    cli_line_counted(out, text + from, size - from);
    cli_line_character(out, '\'');
}

// whether a letter is white as Rust's char::is_whitespace has it
static int address_white(unsigned int letter)
{
    return ((letter >= 0x09u) && (letter <= 0x0du)) || (letter == 0x20u) || (letter == 0x85u) || (letter == 0xa0u) ||
           (letter == 0x1680u) || ((letter >= 0x2000u) && (letter <= 0x200au)) || (letter == 0x2028u) ||
           (letter == 0x2029u) || (letter == 0x202fu) || (letter == 0x205fu) || (letter == 0x3000u);
}

// `text` trimmed of white letters at its two ends, as Rust's str::trim trims it: its start and length
static void address_trimmed(const char *text, unsigned long long *start, unsigned long long *size)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    unsigned long long begin = 0ull;
    unsigned int letter = 0u;
    while (begin < length)
    {
        const unsigned long long taken = cli_lossy_letter((const unsigned char *)text, length, begin, &letter);
        if (!address_white(letter))
        {
            break;
        }
        begin += taken;
    }
    unsigned long long end = begin;
    unsigned long long at = begin;
    while (at < length)
    {
        const unsigned long long taken = cli_lossy_letter((const unsigned char *)text, length, at, &letter);
        at += taken;
        end = address_white(letter) ? end : at;
    }
    *start = begin;
    *size = end - begin;
}

// `size` bytes of `text` kept as a field of an address: 1, or 0 where they are empty or do not fit
static int address_kept(char *field, const char *text, unsigned long long size)
{
    if ((size == 0u) || (size >= CLI_PATH_LONGEST))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = field, .src = text, .bytes = (size_t)size});
    field[size] = '\0';
    return 1;
}

int cli_link_address_parse(const char *text, CliLinkAddress *address, CliLine *said)
{
    unsigned long long start = 0ull;
    unsigned long long size = 0ull;
    address_trimmed(text, &start, &size);
    const char *const trimmed = text + start;
    const char *const first = memor.chr(&(MemoriaCfg){.src = trimmed, .val = ':', .bytes = (size_t)size});
    const unsigned long long first_size = (first != NULL) ? (unsigned long long)(first - trimmed) : 0ull;
    const int docker = (first != NULL) && (first_size == 6u) &&
                       (memor.cmp(&(MemoriaCfg){.src = trimmed, .other = "docker", .bytes = 6u}) == 0);
    const int wsl = (first != NULL) && (first_size == 3u) &&
                    (memor.cmp(&(MemoriaCfg){.src = trimmed, .other = "wsl", .bytes = 3u}) == 0);
    const char *const rest = (first != NULL) ? (first + 1) : NULL;
    const unsigned long long rest_size = (first != NULL) ? (size - first_size - 1u) : 0ull;
    const char *const second =
        (docker || wsl) ? memor.chr(&(MemoriaCfg){.src = rest, .val = ':', .bytes = (size_t)rest_size}) : NULL;
    int read = (first != NULL) && (!(docker || wsl) || (second != NULL));
    if (read && (docker || wsl))
    {
        address->way = docker ? CLI_LINK_DOCKER : CLI_LINK_WSL;
        const unsigned long long name_size = (unsigned long long)(second - rest);
        read = address_kept(address->name, rest, name_size) &&
               address_kept(address->folder, second + 1, rest_size - name_size - 1u);
    }
    else if (read)
    {
        address->way = CLI_LINK_SSH;
        read = address_kept(address->name, trimmed, first_size) && address_kept(address->folder, rest, rest_size);
    }
    if (!read)
    {
        cli_line_counted(said, trimmed, size);
        cli_line_text(said, " names no machine and folder: user@host:folder, docker:container:folder or "
                            "wsl:distribution:folder");
    }
    return read;
}

void cli_link_address_text(const CliLinkAddress *address, CliLine *out)
{
    static const char *const ways[3] = {"", "docker:", "wsl:"};
    cli_line_text(out, ways[address->way]);
    cli_line_text(out, address->name);
    cli_line_character(out, ':');
    cli_line_text(out, address->folder);
}

void cli_link_machine(const CliLinkAddress *address, CliLine *out)
{
    const char *const at =
        cli_text_last(address->name, cellul.len(&(CatenaFinitaCfg){.src = address->name, .cap = CLI_CELLS_POOL}), '@');
    cli_line_text(out, ((address->way == CLI_LINK_SSH) && (at != NULL)) ? (at + 1) : address->name);
}

void cli_link_runs(const CliLinkAddress *address, const char *line, int terminal, CliWords *words)
{
    memor.set(&(MemoriaCfg){.dst = words, .val = 0, .bytes = sizeof(*words)});
    if (address->way == CLI_LINK_SSH)
    {
        if (!cli_runner_program_words("ORIOR_SSH", words))
        {
            memor.set(&(MemoriaCfg){.dst = words, .val = 0, .bytes = sizeof(*words)});
            cli_words_put(words, "ssh");
        }
        if (terminal)
        {
            cli_words_put(words, "-tt");
        }
        else
        {
            cli_words_put(words, "-T");
            cli_words_put(words, "-o");
            cli_words_put(words, "BatchMode=yes");
        }
        static const char *const alive[4] = {"-o", "ServerAliveInterval=15", "-o", "ServerAliveCountMax=3"};
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            cli_words_put(words, alive[at]);
        }
        cli_words_put(words, address->name);
        cli_words_put(words, line);
        return;
    }
    if (address->way == CLI_LINK_DOCKER)
    {
        cli_words_put(words, "docker");
        cli_words_put(words, "exec");
        cli_words_put(words, terminal ? "-it" : "-i");
        cli_words_put(words, address->name);
    }
    else
    {
        cli_words_put(words, "wsl");
        cli_words_put(words, "-d");
        cli_words_put(words, address->name);
        cli_words_put(words, "--");
    }
    cli_words_put(words, "sh");
    cli_words_put(words, "-c");
    cli_words_put(words, line);
}

void cli_link_shell_line(const CliLinkAddress *address, const char *folder, CliLine *out)
{
    const char *const into = (folder != NULL) ? folder : address->folder;
    cli_line_text(out, "cd ");
    cli_link_quote(into, cellul.len(&(CatenaFinitaCfg){.src = into, .cap = CLI_CELLS_POOL}), out);
    cli_line_text(out, " 2>/dev/null || cd; exec \"${SHELL:-sh}\" -l");
}

int cli_link_ask(const CliLinkAddress *address, const char *line, const char *input, unsigned long long size,
                 CliLine *out, CliLine *said)
{
    if ((address->way == CLI_LINK_SSH) && !cli_net_reach(address->name, "a command run over ssh", said))
    {
        return 0;
    }
    CliWords words;
    cli_link_runs(address, line, 0, &words);
    CliRan *const ran = cli_ran_taken();
    const int started = !words.full && cli_ran_fed(words.word, NULL, NULL, 0u, (input != NULL) ? input : "",
                                                   (input != NULL) ? size : 0ull, ran, said);
    if (words.full)
    {
        cli_line_text(said, "the line is longer than orior runs");
    }
    const int done = started && (ran->code == 0);
    if (started && !done)
    {
        unsigned long long start = 0ull;
        unsigned long long length = 0ull;
        address_trimmed(ran->err, &start, &length);
        if (length > 0u)
        {
            cli_lossy(said, (const unsigned char *)ran->err + start, length);
        }
        else
        {
            cli_link_machine(address, said);
            cli_line_text(said, " could not be reached");
        }
    }
    if (done)
    {
        cli_lossy(out, (const unsigned char *)ran->out, ran->out_size);
    }
    cli_ran_given(ran);
    return done;
}
