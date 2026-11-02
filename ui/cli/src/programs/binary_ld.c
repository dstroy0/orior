// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// binary_ld.c: the folders the Linux loader looks in after a program's own run paths: those /etc/ld.so.conf names and
// the files it includes, then the system's own library folders
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "binary.h"

// the most bytes of a file of the loader's settings
#define CLI_LD_CONF_BYTES (1u << 16u)

// The files of a folder whose names start with `head` and end with `tail`, in order, read in turn
typedef struct
{
    const char *head;
    const char *tail;
    char names[64][CLI_NAME_MAX_LEN];
    unsigned int count;
} CliIncluded;

static void cli_included_each(const CliEachNameIsFolderCfg *args)
{
    CliIncluded *const included = (CliIncluded *)args->context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    const unsigned long long head = cellul.len(&(CatenaFinitaCfg){.src = included->head, .cap = CLI_CELLS_POOL});
    const unsigned long long tail = cellul.len(&(CatenaFinitaCfg){.src = included->tail, .cap = CLI_CELLS_POOL});
    if (args->is_folder || (included->count >= 64u) || (length < (head + tail)) ||
        (!EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->name, .other_pointer = included->head,
                     .size = head)) ||
        (!cellul.eq(
            &(CatenaFinitaCfg){.src = args->name + length - tail, .other = included->tail, .cap = CLI_CELLS_POOL})))
    {
        return;
    }
    unsigned int place = included->count;
    while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = included->names[place - 1u],
                                       .other_pointer = args->name) > 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = included->names[place],
            .src = included->names[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = included->names[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = included->names[place],
                            .src = args->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1u});
    included->count += 1u;
}

typedef struct
{
    const char *path;
    char (*folders)[CLI_PATH_MAX_LEN];
    unsigned int count;
    unsigned int depth;
} BinaryLdCtx;
// The folders a file of the loader's settings names, those of the files it includes among them, four includes deep at
// most, put into `folders` after `count`: the count after
static unsigned int cli_ld_conf(const BinaryLdCtx *args)
{
    unsigned int count = args->count;
    static char s_text[5][CLI_LD_CONF_BYTES];
    static CliIncluded s_included[5];
    char *const text = s_text[args->depth];
    const unsigned long long size = qry_member_file_read(args->path, (unsigned char *)text, CLI_LD_CONF_BYTES - 1u);
    if (size == QRY_MEMBER_UNREAD)
    {
        return count;
    }
    text[size] = '\0';
    char *line = text;
    while (line[0] != '\0')
    {
        char *end = line;
        end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ull,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}), .set = "\n");
        char *const next = (end[0] != '\0') ? (end + 1) : end;
        end[0] = '\0';
        char *const hash = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '#'});
        if (hash != NULL)
        {
            hash[0] = '\0';
        }
        char *from = line;
        char *to = line + cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL});
        from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ull,
                           .size = (unsigned long long)(to - from), .set = " \t\r");
        to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                               .at = (unsigned long long)(to - from), .set = " \t\r");
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
                    &(MemoriaCfg){.dst = args->folders[count],
                                  .src = from,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
                count += 1u;
            }
            continue;
        }
        from += 7;
        from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ull,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}), .set = " \t");
        char folder[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = from,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
        char *const slash = EMBED_CALL(
            cli_text_byte.last, CliTextByteCfg, .text = folder,
            .length_value = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), .byte = '/');
        if ((slash == NULL) || (slash[1] == '\0'))
        {
            continue;
        }
        slash[0] = '\0';
        char name[CLI_NAME_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = name,
                                .src = slash + 1,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = slash + 1, .cap = CLI_CELLS_POOL}) + 1u});
        char *const star = cellul.chr(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL, .byte = '*'});
        CliIncluded *const included = &s_included[args->depth];
        included->tail = (star != NULL) ? (star + 1) : "";
        if (star != NULL)
        {
            star[0] = '\0';
        }
        included->head = name;
        included->count = 0u;
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx,
                   .folder = (folder[0] != '\0') ? folder : "/", .each = cli_included_each, .context_bytess = included);
        for (unsigned int at = 0u; (args->depth < 4u) && (at < included->count); at += 1u)
        {
            char file[CLI_PATH_MAX_LEN];
            VerbaLinea joined = {file, sizeof(file), 0ull};
            mmgr_line_put(&joined, folder);
            mmgr_line_ch(&joined, '/');
            mmgr_line_put(&joined, included->names[at]);
            if (mmgr_line_finish(&joined) != 0ull)
            {
                count = EMBED_CALL(cli_ld_conf, BinaryLdCtx, .path = file, .folders = args->folders, .count = count,
                                   .depth = args->depth + 1u);
            }
        }
    }
    return count;
}

unsigned int cli_binary_linux_folders(const CliBinaryCfg *args)
{
    char (*folders)[CLI_PATH_MAX_LEN] = args->folders;
    static const char *const s_system[8] = {"/lib64",
                                            "/usr/lib64",
                                            "/lib",
                                            "/usr/lib",
                                            "/lib/x86_64-linux-gnu",
                                            "/usr/lib/x86_64-linux-gnu",
                                            "/lib/aarch64-linux-gnu",
                                            "/usr/lib/aarch64-linux-gnu"};
    unsigned int count =
        EMBED_CALL(cli_ld_conf, BinaryLdCtx, .path = "/etc/ld.so.conf", .folders = folders, .count = 0u, .depth = 0u);
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
