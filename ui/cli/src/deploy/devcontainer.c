// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// devcontainer.c: a description's JSON made plain, the description found in the tree, and the container's name
#include "devcontainer.h"
#include "../text/text.h"

// whether a byte is one of ASCII's spaces
static int cli_is_space(char letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
}

void cli_plain_json(const char *text, unsigned long long size, VerbaLinea *plain)
{
    unsigned long long at = 0ull;
    int in_string = 0;
    while (at < size)
    {
        const char letter = text[at];
        if (in_string)
        {
            mmgr_line_ch(plain, letter);
            if ((letter == '\\') && ((at + 1ull) < size))
            {
                mmgr_line_ch(plain, text[at + 1u]);
                at += 2ull;
                continue;
            }
            in_string = letter != '"';
            at += 1ull;
        }
        else if (letter == '"')
        {
            in_string = 1;
            mmgr_line_ch(plain, letter);
            at += 1ull;
        }
        else if ((letter == '/') && ((at + 1ull) < size) && (text[at + 1u] == '/'))
        {
            at = cli_text_to_any(text, at, size, "\n");
        }
        else if ((letter == '/') && ((at + 1ull) < size) && (text[at + 1u] == '*'))
        {
            at += 2ull;
            const unsigned long long closed = cli_text_to_pair(text, at, size, '*', '/');
            at = (closed < size) ? closed : ((at > (size - 1ull)) ? at : (size - 1ull));
            at += 2ull;
        }
        else if (letter == ',')
        {
            unsigned long long next = at + 1ull;
            while ((next < size) && cli_is_space(text[next]))
            {
                next += 1ull;
            }
            if ((next >= size) || ((text[next] != ']') && (text[next] != '}')))
            {
                mmgr_line_ch(plain, letter);
            }
            at += 1ull;
        }
        else
        {
            mmgr_line_ch(plain, letter);
            at += 1ull;
        }
    }
}

// The folders under `.devcontainer` looked through for a description: the folder, and the least path of one found
typedef struct
{
    const char *folder;
    char least[CLI_PATH_MAX_LEN];
    int found;
} CliDescriptionFound;

static void cli_description_each(void *context, const char *name, int is_folder)
{
    CliDescriptionFound *const found = (CliDescriptionFound *)context;
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, found->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, "/devcontainer.json");
    if (!is_folder || (mmgr_line_finish(&line) == 0ull) || !query_is_file(path))
    {
        return;
    }
    if (!found->found || (cli_text_order(path, found->least) < 0))
    {
        memor.cpy(&(MemoriaCfg){.dst = found->least,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        found->found = 1;
    }
}

int cli_devcontainer_find(const char *root, char *file)
{
    static const char *const s_named[2] = {"/.devcontainer/devcontainer.json", "/.devcontainer.json"};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        VerbaLinea line = {file, CLI_PATH_MAX_LEN, 0ull};
        mmgr_line_put(&line, root);
        mmgr_line_put(&line, s_named[at]);
        if ((mmgr_line_finish(&line) != 0ull) && query_is_file(file))
        {
            return 1;
        }
    }
    static CliDescriptionFound s_found;
    VerbaLinea line = {file, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, root);
    mmgr_line_put(&line, "/.devcontainer");
    if (mmgr_line_finish(&line) == 0ull)
    {
        return 0;
    }
    s_found.folder = file;
    s_found.found = 0;
    query_folder_each(file, cli_description_each, &s_found);
    if (s_found.found)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = file,
                          .src = s_found.least,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_found.least, .cap = CLI_CELLS_POOL}) + 1u});
    }
    return s_found.found;
}

void cli_devcontainer_name(const char *root, char *name)
{
    // the tree's folder: its path's last part, the separators after it left off
    const char *end = root + cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    end = root + cli_text_back_past(root, (unsigned long long)(end - root), "/\\");
    const char *base = end;
    base = root + cli_text_back_any(root, (unsigned long long)(base - root), "/\\");
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
        const int taken = ((lower >= 'a') && (lower <= 'z')) || ((lower >= '0') && (lower <= '9')) || (lower == '-') ||
                          (lower == '_');
        docker_name[length] = taken ? lower : '-';
        length += 1ull;
    }
    unsigned long long from = 0ull;
    from = cli_text_past_any(docker_name, from, length, "-");
    length = (from + cli_text_back_past(docker_name + from, length - from, "-"));
    // FNV-1a over the path's bytes: a name the same tree is given again from one run to the next
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (const char *at = root; at[0] != '\0'; at += 1)
    {
        hash = (hash ^ (unsigned char)at[0]) * 0x100000001b3ull;
    }
    VerbaLinea line = {name, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, "orior-");
    mmgr_line_put_n(&line, docker_name + from, length - from);
    mmgr_line_ch(&line, '-');
    mmgr_line_hex(&line, hash & 0xffffffffull, 8u);
    mmgr_line_finish(&line);
}

const char *cli_plan_text(const CliPlan *plan, unsigned int at)
{
    return plan->text + at;
}
