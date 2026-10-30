// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_path.c: where a path of the tree runs into an archive, and on into one inside it; what a folder within an
// archive contains; and a file within one read
#include "../command_line/cells.h"
#include "../text/text.h"
#include "archive_internal.h"

CliLevels *g_cli_archive_levels;
CliLock g_cli_archive_lock = CLI_LOCK_INIT;

// the rooms of the levels, in one list
#define CLI_ARCHIVE_ROOMS ((3u * CLI_ARCHIVE_DEPTH) - 1u + 1u)

static CliRoom *cli_archive_rooms(unsigned int at)
{
    CliLevels *const levels = g_cli_archive_levels;
    return (at < CLI_ARCHIVE_DEPTH)                 ? &levels->unpacked[at]
           : (at < ((2u * CLI_ARCHIVE_DEPTH) - 1u)) ? &levels->inner[at - CLI_ARCHIVE_DEPTH]
           : (at < ((3u * CLI_ARCHIVE_DEPTH) - 1u)) ? &levels->written[at - ((2u * CLI_ARCHIVE_DEPTH) - 1u)]
                                                    : &levels->tar;
}

int cli_archive_locked(VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_archive_lock);
    g_cli_archive_levels = g_cli_cells->persistent_buf_alloc(sizeof(*g_cli_archive_levels), &g_cli_editor_cells);
    if (g_cli_archive_levels == NULL)
    {
        cli_lock_give(&g_cli_archive_lock);
        mmgr_line_put(err_msg, "the archives a path runs through could not be taken");
        return 0;
    }
    for (unsigned int at = 0u; at < CLI_ARCHIVE_ROOMS; at += 1u)
    {
        *cli_archive_rooms(at) = (CliRoom){NULL, 0ull};
    }
    return 1;
}

void cli_archive_released(void)
{
    for (unsigned int at = 0u; at < CLI_ARCHIVE_ROOMS; at += 1u)
    {
        g_cli_cells->persistent_buf_release(cli_archive_rooms(at)->bytes, &g_cli_editor_cells);
    }
    g_cli_cells->persistent_buf_release(g_cli_archive_levels, &g_cli_editor_cells);
    g_cli_archive_levels = NULL;
    cli_lock_give(&g_cli_archive_lock);
}

int cli_archive_room(CliRoom *room, unsigned long long need, const char *what, VerbaLinea *err_msg)
{
    if (need <= room->room)
    {
        return 1;
    }
    if (need > CLI_ARCHIVE_MOST)
    {
        mmgr_line_put(err_msg, what);
        mmgr_line_put(err_msg, " is larger than orior_cli supports");
        return 0;
    }
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(need, &g_cli_editor_cells);
    if (cell == NULL)
    {
        mmgr_line_put(err_msg, what);
        mmgr_line_put(err_msg, " could not be taken");
        return 0;
    }
    g_cli_cells->persistent_buf_release(room->bytes, &g_cli_editor_cells);
    room->bytes = cell;
    room->room = need;
    return 1;
}

unsigned long long cli_archive_grown(unsigned long long size)
{
    return size + (size / 8ull) + 4096ull;
}

void cli_archive_parts_of(const char *path, CliParts *parts)
{
    parts->count = 0u;
    for (const char *at = path; at[0] != '\0';)
    {
        const char *stop = at;
        while ((stop[0] != '\0') && (stop[0] != '/') && (stop[0] != '\\'))
        {
            stop += 1;
        }
        if ((stop > at) && (parts->count < CLI_ARCHIVE_PARTS))
        {
            VerbaLinea line = {parts->part[parts->count], CLI_NAME_MAX_LEN, 0ull};
            mmgr_line_put_n(&line, at, (unsigned long long)(stop - at));
            mmgr_line_finish(&line);
            parts->count += 1u;
        }
        at = (stop[0] != '\0') ? (stop + 1) : stop;
    }
}

void cli_archive_joined(const CliParts *parts, unsigned int from, unsigned int to, char *out)
{
    VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ull};
    for (unsigned int at = from; at < to; at += 1u)
    {
        mmgr_line_put(&line, (at != from) ? "/" : "");
        mmgr_line_put(&line, parts->part[at]);
    }
    mmgr_line_finish(&line);
}

const CliMember *cli_archive_file_named(const CliMembers *members, const char *name)
{
    for (unsigned int at = 0u; at < members->count; at += 1u)
    {
        if (!members->item[at].dir &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = members->text + members->item[at].name, .other = name, .cap = CLI_CELLS_POOL})))
        {
            return &members->item[at];
        }
    }
    return NULL;
}

int cli_archive_split(const char *root, const char *path, char *file, char *inner)
{
    char full[CLI_PATH_MAX_LEN];
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!cli_inside(root, path, full, &err_msg))
    {
        return 0;
    }
    // the nearest folder up the path that is there
    memor.cpy(&(MemoriaCfg){
        .dst = file, .src = full, .bytes = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}) + 1u});
    while (!query_is_file(file) && !query_is_folder(file))
    {
        char above[CLI_PATH_MAX_LEN];
        if (!cli_path_parent(file, above))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = file,
                                .src = above,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = above, .cap = CLI_CELLS_POOL}) + 1u});
    }
    const char *const base =
        (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/') != NULL)
            ? (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/') + 1)
        : (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '\\') != NULL)
            ? (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '\\') + 1)
            : file;
    if (!query_is_file(file) || (cli_archive_kind_of(base) == CLI_ARCHIVE_NONE))
    {
        return 0;
    }
    const char *rest = full + cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL});
    rest += ((rest[0] == '/') || (rest[0] == '\\')) ? 1 : 0;
    memor.cpy(&(MemoriaCfg){
        .dst = inner, .src = rest, .bytes = cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = inner; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    return 1;
}

int cli_archive_descend(CliDescent *descent, const CliParts *parts, VerbaLinea *err_msg)
{
    unsigned int buffer = 0u;
    for (;;)
    {
        char inner[CLI_NAME_MAX_LEN];
        unsigned long long unpacked = 0ull;
        cli_gz_inner(descent->name, inner);
        if (!cli_members_of(descent->bytes, descent->size, descent->kind, inner, &g_cli_archive_levels->members[0],
                            &g_cli_archive_levels->unpacked[0], &unpacked, err_msg))
        {
            return 0;
        }
        int deeper = 0;
        for (unsigned int cut = parts->count; !deeper && (cut > descent->left); cut -= 1u)
        {
            char path[CLI_PATH_MAX_LEN];
            cli_archive_joined(parts, descent->left, cut, path);
            const CliMember *const member = cli_archive_file_named(&g_cli_archive_levels->members[0], path);
            const unsigned int nested = cli_archive_kind_of(path);
            if ((member == NULL) || (nested == CLI_ARCHIVE_NONE))
            {
                continue;
            }
            unsigned long long size = 0ull;
            if (!cli_data_of(descent->bytes, descent->size, descent->kind, g_cli_archive_levels->unpacked[0].bytes,
                             unpacked, &g_cli_archive_levels->members[0], member, &g_cli_archive_levels->inner[buffer],
                             &size, err_msg))
            {
                return 0;
            }
            descent->bytes = g_cli_archive_levels->inner[buffer].bytes;
            descent->size = size;
            descent->kind = nested;
            memor.cpy(&(MemoriaCfg){.dst = descent->name,
                                    .src = path,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
            descent->left = cut;
            buffer = 1u - buffer;
            deeper = 1;
        }
        if (!deeper)
        {
            return 1;
        }
    }
}

// the hex of `length` bytes, two lower case digits each, put in the children's texts: its place
static unsigned int cli_hex_stored(CliChildren *found, const char *bytes, unsigned long long length)
{
    static const char s_hex[17] = "0123456789abcdef";
    if ((found->used + (length * 2ull) + 1ull) > CLI_ARCHIVE_NAMES)
    {
        return 0u;
    }
    // a place is below CLI_ARCHIVE_NAMES, which an unsigned int stores
    const unsigned int at = (unsigned int)found->used;
    for (unsigned long long byte = 0ull; byte < length; byte += 1ull)
    {
        found->text[at + (byte * 2ull)] = s_hex[(unsigned char)bytes[byte] >> 4u];
        found->text[at + (byte * 2ull) + 1ull] = s_hex[(unsigned char)bytes[byte] & 0x0fu];
    }
    found->text[at + (length * 2ull)] = '\0';
    found->used += (length * 2ull) + 1ull;
    return at;
}

// a text put in the children's texts: its place
static unsigned int cli_text_stored(CliChildren *found, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    if ((found->used + length + 1ull) > CLI_ARCHIVE_NAMES)
    {
        return 0u;
    }
    // a place is below CLI_ARCHIVE_NAMES, which an unsigned int stores
    const unsigned int at = (unsigned int)found->used;
    memor.cpy(&(MemoriaCfg){.dst = found->text + at, .src = text, .bytes = length + 1ull});
    found->used += length + 1ull;
    return at;
}

// whether a text sorts before another, ASCII's case aside
static int cli_before(const char *one, const char *other)
{
    for (;; one += 1, other += 1)
    {
        const char left = ((one[0] >= 'A') && (one[0] <= 'Z')) ? (char)(one[0] + 32) : one[0];
        const char right = ((other[0] >= 'A') && (other[0] <= 'Z')) ? (char)(other[0] + 32) : other[0];
        if ((left != right) || (left == '\0'))
        {
            return (unsigned char)left < (unsigned char)right;
        }
    }
}

// the children of the folder the path's parts from `left` lead to within the archive at `descent`, its members read,
// each path prefixed with `prefix`, folders first, then by name, case aside
static void cli_children(const CliDescent *descent, const CliParts *parts, const char *prefix, CliChildren *found)
{
    const CliMembers *const members = &g_cli_archive_levels->members[0];
    const unsigned int depth = parts->count - descent->left;
    found->count = 0u;
    found->used = 1ull;
    found->text[0] = '\0';
    for (unsigned int index = 0u; index < members->count; index += 1u)
    {
        const CliMember *const member = &members->item[index];
        CliParts own;
        cli_archive_parts_of(members->text + member->name, &own);
        int under = own.count > depth;
        for (unsigned int at = 0u; under && (at < depth); at += 1u)
        {
            under = cellul.eq(&(CatenaFinitaCfg){
                .src = own.part[at], .other = parts->part[descent->left + at], .cap = CLI_CELLS_POOL});
        }
        if (!under)
        {
            continue;
        }
        const char *const child = own.part[depth];
        const int deeper = own.count > (depth + 1u);
        unsigned int seen = found->count;
        for (unsigned int at = 0u; (seen == found->count) && (at < found->count); at += 1u)
        {
            seen = (cellul.eq(&(CatenaFinitaCfg){
                       .src = found->text + found->item[at].name, .other = child, .cap = CLI_CELLS_POOL}))
                       ? at
                       : seen;
        }
        if (seen < found->count)
        {
            found->item[seen].dir = found->item[seen].dir || deeper || member->dir;
            continue;
        }
        if (found->count >= CLI_ARCHIVE_CHILDREN)
        {
            continue;
        }
        const int archive = !deeper && !member->dir && (cli_archive_kind_of(child) != CLI_ARCHIVE_NONE);
        CliChild *const one = &found->item[found->count];
        found->count += 1u;
        one->name = cli_text_stored(found, child);
        char path[CLI_PATH_MAX_LEN];
        VerbaLinea line = {path, sizeof(path), 0ull};
        mmgr_line_put(&line, prefix);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, child);
        mmgr_line_finish(&line);
        one->path = cli_text_stored(found, path);
        one->dir = deeper || member->dir || archive;
        one->archive = archive;
        one->has_raw = member->has_raw && !deeper;
        one->raw = 0u;
        if (one->has_raw)
        {
            // the name's own bytes past its last slash, or all of them where it ends with one
            const char *const raw = members->text + member->raw;
            const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = raw, .cap = CLI_CELLS_POOL});
            unsigned long long end = length;
            while ((end > 0ull) && (raw[end - 1ull] == '/'))
            {
                end -= 1ull;
            }
            unsigned long long start = end;
            while ((start > 0ull) && (raw[start - 1ull] != '/'))
            {
                start -= 1ull;
            }
            one->raw =
                (end > start) ? cli_hex_stored(found, raw + start, end - start) : cli_hex_stored(found, raw, length);
        }
    }
    // folders first, then by name, case aside
    for (unsigned int at = 1u; at < found->count; at += 1u)
    {
        const CliChild swap = found->item[at];
        unsigned int place = at;
        while (place > 0u)
        {
            const CliChild *const before = &found->item[place - 1u];
            const int first =
                (swap.dir && !before->dir) ||
                ((swap.dir == before->dir) && cli_before(found->text + swap.name, found->text + before->name));
            if (!first)
            {
                break;
            }
            found->item[place] = *before;
            place -= 1u;
        }
        found->item[place] = swap;
    }
}

int cli_archive_opened(const char *root, const char *path, const char *missing, CliDescent *descent, CliParts *parts,
                       char *file, VerbaLinea *err_msg)
{
    char inner[CLI_PATH_MAX_LEN];
    if (!cli_archive_split(root, path, file, inner))
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, missing);
        return 0;
    }
    const char *const base =
        (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/') != NULL)
            ? (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/') + 1)
        : (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '\\') != NULL)
            ? (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '\\') + 1)
            : file;
    descent->kind = cli_archive_kind_of(base);
    descent->bytes = cli_file_read(file, &descent->size);
    if (descent->bytes == NULL)
    {
        mmgr_line_put(err_msg, file);
        mmgr_line_put(err_msg, ": it could not be read");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = descent->name,
                            .src = base,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}) + 1u});
    descent->left = 0u;
    cli_archive_parts_of(inner, parts);
    return 1;
}

int cli_archive_list(const char *root, const char *path, CliChildren *found, VerbaLinea *err_msg)
{
    static CliParts s_parts;
    CliDescent descent;
    char file[CLI_PATH_MAX_LEN];
    if (!cli_archive_locked(err_msg))
    {
        return 0;
    }
    if (!cli_archive_opened(root, path, " is no archive", &descent, &s_parts, file, err_msg))
    {
        cli_archive_released();
        return 0;
    }
    const unsigned char *const mapped = descent.bytes;
    const unsigned long long mapped_size = descent.size;
    const int done = cli_archive_descend(&descent, &s_parts, err_msg);
    if (done)
    {
        char prefix[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = prefix,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = CLI_CELLS_POOL});
        while ((length > 0ull) && (prefix[length - 1ull] == '/'))
        {
            length -= 1ull;
        }
        prefix[length] = '\0';
        cli_children(&descent, &s_parts, prefix, found);
    }
    cli_file_let_go(mapped, mapped_size);
    cli_archive_released();
    return done;
}

int cli_archive_read(const char *root, const char *path, VerbaLinea *out, VerbaLinea *err_msg)
{
    static CliParts s_parts;
    CliDescent descent;
    char file[CLI_PATH_MAX_LEN];
    if (!cli_archive_locked(err_msg))
    {
        return 0;
    }
    if (!cli_archive_opened(root, path, " is in no archive", &descent, &s_parts, file, err_msg))
    {
        cli_archive_released();
        return 0;
    }
    const unsigned char *const mapped = descent.bytes;
    const unsigned long long mapped_size = descent.size;
    int done = cli_archive_descend(&descent, &s_parts, err_msg);
    char wanted[CLI_PATH_MAX_LEN];
    cli_archive_joined(&s_parts, descent.left, s_parts.count, wanted);
    char inner[CLI_NAME_MAX_LEN];
    cli_gz_inner(descent.name, inner);
    unsigned long long unpacked = 0ull;
    done = done && cli_members_of(descent.bytes, descent.size, descent.kind, inner, &g_cli_archive_levels->members[0],
                                  &g_cli_archive_levels->unpacked[0], &unpacked, err_msg);
    const CliMember *const member = done ? cli_archive_file_named(&g_cli_archive_levels->members[0], wanted) : NULL;
    if (done && (member == NULL))
    {
        mmgr_line_put(err_msg, descent.name);
        mmgr_line_put(err_msg, " contains no ");
        mmgr_line_put(err_msg, wanted);
        done = 0;
    }
    unsigned long long size = 0ull;
    done = done &&
           cli_data_of(descent.bytes, descent.size, descent.kind, g_cli_archive_levels->unpacked[0].bytes, unpacked,
                       &g_cli_archive_levels->members[0], member, &g_cli_archive_levels->written[0], &size, err_msg);
    if (done)
    {
        mmgr_line_put_n(out, (const char *)g_cli_archive_levels->written[0].bytes, size);
    }
    cli_file_let_go(mapped, mapped_size);
    cli_archive_released();
    return done;
}
