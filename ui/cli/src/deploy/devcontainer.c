// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// devcontainer.c: a description's JSON made plain, the description found in the tree, and the container's name
#include "devcontainer.h"
#include "../command_line/callback_cfg.h"
#include "../text/text.h"

typedef struct
{
    char letter;
} DevcontainerCtx;

// whether a byte is one of ASCII's spaces
EMBED_INLINE int cli_is_space(const DevcontainerCtx *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r'));
}

void cli_plain_json(const CliDevcontainerTextCfg *args)
{
    unsigned long long at = 0ull;
    int in_string = 0;
    while (at < args->size)
    {
        const char letter = args->text[at];
        if (in_string)
        {
            mmgr_line_ch(args->plain, letter);
            if ((letter == '\\') && ((at + 1ull) < args->size))
            {
                mmgr_line_ch(args->plain, args->text[at + 1u]);
                at += 2ull;
                continue;
            }
            in_string = letter != '"';
            at += 1ull;
        }
        else if (letter == '"')
        {
            in_string = 1;
            mmgr_line_ch(args->plain, letter);
            at += 1ull;
        }
        else if ((letter == '/') && ((at + 1ull) < args->size) && (args->text[at + 1u] == '/'))
        {
            at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->text, .at = at,
                            .size = args->size, .set = "\n");
        }
        else if ((letter == '/') && ((at + 1ull) < args->size) && (args->text[at + 1u] == '*'))
        {
            at += 2ull;
            const unsigned long long closed =
                EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = args->text, .at = at,
                           .size = args->size, .first = '*', .second = '/');
            at = (closed < args->size) ? closed : ((at > (args->size - 1ull)) ? at : (args->size - 1ull));
            at += 2ull;
        }
        else if (letter == ',')
        {
            unsigned long long next = at + 1ull;
            while ((next < args->size) && EMBED_CALL(cli_is_space, DevcontainerCtx, .letter = args->text[next]))
            {
                next += 1ull;
            }
            if ((next >= args->size) || ((args->text[next] != ']') && (args->text[next] != '}')))
            {
                mmgr_line_ch(args->plain, letter);
            }
            at += 1ull;
        }
        else
        {
            mmgr_line_ch(args->plain, letter);
            at += 1ull;
        }
    }
}

// The folders under `.devcontainer` looked through for a description: the folder, and the least path of one found
typedef struct
{
    const char *folder;
    char min[CLI_PATH_MAX_LEN];
    int found;
} CliDescriptionFound;

static void cli_description_each(const CliEachNameIsFolderCfg *args)
{
    CliDescriptionFound *const found = (CliDescriptionFound *)args->context;
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, found->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    mmgr_line_put(&line, "/devcontainer.json");
    if (!args->is_folder || (mmgr_line_finish(&line) == 0ull) ||
        !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
    {
        return;
    }
    if (!found->found ||
        (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = path, .other_pointer = found->min) < 0))
    {
        memor.cpy(&(MemoriaCfg){.dst = found->min,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        found->found = 1;
    }
}

int cli_devcontainer_find(const CliDevcontainerCfg *args)
{
    static const char *const s_named[2] = {"/.devcontainer/devcontainer.json", "/.devcontainer.json"};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        VerbaLinea line = {args->file, CLI_PATH_MAX_LEN, 0ull};
        mmgr_line_put(&line, args->root);
        mmgr_line_put(&line, s_named[at]);
        if ((mmgr_line_finish(&line) != 0ull) &&
            EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->file))
        {
            return 1;
        }
    }
    static CliDescriptionFound s_found;
    VerbaLinea line = {args->file, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, args->root);
    mmgr_line_put(&line, "/.devcontainer");
    if (mmgr_line_finish(&line) == 0ull)
    {
        return 0;
    }
    s_found.folder = args->file;
    s_found.found = 0;
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->file,
               .each = cli_description_each, .context_bytess = &s_found);
    if (s_found.found)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->file,
                          .src = s_found.min,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_found.min, .cap = CLI_CELLS_POOL}) + 1u});
    }
    return s_found.found;
}

void cli_devcontainer_name(const CliDevcontainerRootNameCfg *args)
{
    // the tree's folder: its path's last part, the separators after it left off
    const char *end = args->root + cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL});
    end = args->root + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->root,
                                  .at = (unsigned long long)(end - args->root), .set = "/\\");
    const char *base = end;
    base = args->root + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->root,
                                   .at = (unsigned long long)(base - args->root), .set = "/\\");
    base += ((base < end) && (base[0] != '\0') && (end - base == 2) && (base[1] == ':')) ? 2 : 0;
    // each letter in lower case, retained where docker takes it in a name and a '-' where not, one for each letter past
    // ASCII; the '-' at each end left off
    char docker_name[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ull;
    for (const char *at = base; (at < end) && (length + 1ull < sizeof(docker_name)); at += 1)
    {
        const unsigned char byte = (unsigned char)at[0];
        if ((byte >= 0x80u) && (byte < 0xc0u))
        {
            continue;
        }
        const char lower = ((byte >= 'A') && (byte <= 'Z')) ? (char)(byte + 32u) : (char)byte;
        const int reserved = ((lower >= 'a') && (lower <= 'z')) || ((lower >= '0') && (lower <= '9')) ||
                             (lower == '-') || (lower == '_');
        docker_name[length] = reserved ? lower : '-';
        length += 1ull;
    }
    unsigned long long from = 0ull;
    from = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = docker_name, .at = from,
                      .size = length, .set = "-");
    length = (from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = docker_name + from,
                                .at = length - from, .set = "-"));
    // FNV-1a over the path's bytes: a name the same tree is given again from one run to the next
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (const char *at = args->root; at[0] != '\0'; at += 1)
    {
        hash = (hash ^ (unsigned char)at[0]) * 0x100000001b3ull;
    }
    VerbaLinea line = {args->name, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, "orior-");
    mmgr_line_put_n(&line, docker_name + from, length - from);
    mmgr_line_ch(&line, '-');
    mmgr_line_hex(&line, hash & 0xffffffffull, 8u);
    mmgr_line_finish(&line);
}

const char *cli_plan_text(const CliDevcontainerPlanAtCfg *args)
{
    return args->plan->text + args->at;
}
