// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// binary_ld.c: the folders the Linux loader looks in after a program's own run paths: those /etc/ld.so.conf names and
// the files it includes, then the system's own library folders
#include "../text/text.h"
#include "binary.h"

// the most bytes of a file of the loader's settings
#define CLI_LD_CONF_BYTES (1u << 16u)

// The files of a folder whose names start with `head` and end with `tail`, in order, read in turn
typedef struct
{
    const char *head;
    const char *tail;
    char names[64][CLI_NAME_LONGEST];
    unsigned int count;
} CliIncluded;

static void cli_included_each(void *context, const char *name, int is_folder)
{
    CliIncluded *const included = (CliIncluded *)context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    const unsigned long long head = cellul.len(&(CatenaFinitaCfg){.src = included->head, .cap = CLI_CELLS_POOL});
    const unsigned long long tail = cellul.len(&(CatenaFinitaCfg){.src = included->tail, .cap = CLI_CELLS_POOL});
    if (is_folder || (included->count >= 64u) || (length < (head + tail)) ||
        (!cli_text_same(name, included->head, head)) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - tail, .other = included->tail, .cap = CLI_CELLS_POOL})))
    {
        return;
    }
    unsigned int place = included->count;
    while ((place > 0u) && (cli_text_order(included->names[place - 1u], name) > 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = included->names[place],
            .src = included->names[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = included->names[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = included->names[place],
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    included->count += 1u;
}

// The folders a file of the loader's settings names, those of the files it includes among them, four includes deep at
// most, put into `folders` after `count`: the count after
static unsigned int cli_ld_conf(const char *path, char (*folders)[CLI_PATH_LONGEST], unsigned int count,
                                unsigned int depth)
{
    static char s_text[5][CLI_LD_CONF_BYTES];
    static CliIncluded s_included[5];
    char *const text = s_text[depth];
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)text, CLI_LD_CONF_BYTES - 1u);
    if (size == QRY_MEMBER_UNREAD)
    {
        return count;
    }
    text[size] = '\0';
    char *line = text;
    while (line[0] != '\0')
    {
        char *end = line;
        while ((end[0] != '\0') && (end[0] != '\n'))
        {
            end += 1;
        }
        char *const next = (end[0] != '\0') ? (end + 1) : end;
        end[0] = '\0';
        char *const hash = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '#'});
        if (hash != NULL)
        {
            hash[0] = '\0';
        }
        char *from = line;
        char *to = line + cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL});
        while ((from < to) && ((from[0] == ' ') || (from[0] == '\t') || (from[0] == '\r')))
        {
            from += 1;
        }
        while ((to > from) && ((to[-1] == ' ') || (to[-1] == '\t') || (to[-1] == '\r')))
        {
            to -= 1;
        }
        to[0] = '\0';
        line = next;
        if (from[0] == '\0')
        {
            continue;
        }
        if (!cellul.starts(&(CatenaFinitaCfg){.src = from, .other = "include", .cap = CLI_CELLS_POOL}))
        {
            if (count < CLI_BINARY_LINUX_FOLDERS)
            {
                memor.cpy(
                    &(MemoriaCfg){.dst = folders[count],
                                  .src = from,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
                count += 1u;
            }
            continue;
        }
        from += 7;
        while ((from[0] == ' ') || (from[0] == '\t'))
        {
            from += 1;
        }
        char folder[CLI_PATH_LONGEST];
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = from,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
        char *const slash =
            cli_text_last(folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), '/');
        if ((slash == NULL) || (slash[1] == '\0'))
        {
            continue;
        }
        slash[0] = '\0';
        char name[CLI_NAME_LONGEST];
        memor.cpy(&(MemoriaCfg){.dst = name,
                                .src = slash + 1,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = slash + 1, .cap = CLI_CELLS_POOL}) + 1u});
        char *const star = cellul.chr(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL, .byte = '*'});
        CliIncluded *const included = &s_included[depth];
        included->tail = (star != NULL) ? (star + 1) : "";
        if (star != NULL)
        {
            star[0] = '\0';
        }
        included->head = name;
        included->count = 0u;
        query_folder_each((folder[0] != '\0') ? folder : "/", cli_included_each, included);
        for (unsigned int at = 0u; (depth < 4u) && (at < included->count); at += 1u)
        {
            char file[CLI_PATH_LONGEST];
            CliLine joined = {file, sizeof(file), 0ull};
            cli_line_text(&joined, folder);
            cli_line_character(&joined, '/');
            cli_line_text(&joined, included->names[at]);
            if (cli_line_closed(&joined) != 0ull)
            {
                count = cli_ld_conf(file, folders, count, depth + 1u);
            }
        }
    }
    return count;
}

unsigned int cli_binary_linux_folders(char (*folders)[CLI_PATH_LONGEST])
{
    static const char *const s_system[8] = {"/lib64",
                                            "/usr/lib64",
                                            "/lib",
                                            "/usr/lib",
                                            "/lib/x86_64-linux-gnu",
                                            "/usr/lib/x86_64-linux-gnu",
                                            "/lib/aarch64-linux-gnu",
                                            "/usr/lib/aarch64-linux-gnu"};
    unsigned int count = cli_ld_conf("/etc/ld.so.conf", folders, 0u, 0u);
    for (unsigned int at = 0u; (at < 8u) && (count < CLI_BINARY_LINUX_FOLDERS); at += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = folders[count],
                          .src = s_system[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_system[at], .cap = CLI_CELLS_POOL}) + 1u});
        count += 1u;
    }
    return count;
}
