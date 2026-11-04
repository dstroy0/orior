// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// place.c: the window's place for each tree: where it stood, how large it was and whether it filled its monitor, saved
// in the file `places` in orior's home as the window closes, one record for each tree, and put back as the tree opens,
// fitted to the monitors there are now. A place on no monitor there is goes onto the first, in its middle, and a place
// larger than its monitor is cut to it
#include "view.h"

#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"
#include "../text/text.h"

// how much of a window's top edge must stand on a monitor for its place there to stand, in pixels: enough to take it
// by its title and move it
#define CLI_PLACE_STRIP 64LL

// One record of the file: the place and the bytes of the tree's folder after it, its `\` each a `/`
typedef struct
{
    CliWindowPosition place;
    unsigned int tree_length;
} CliPlaceRecord;

typedef struct
{
    const CliWindowPosition *place;
    const CliWindowArea *area;
    int at;
    unsigned int size;
    int start;
    unsigned int span;
    CliWindowPosition *position_out;
    const CliWindowArea *areas;
    unsigned int count;
    const char *root;
    char *key;
    char *path;
    unsigned long long *length;
} CliPlaceCtx;

// the part of `place`'s top strip, CLI_PLACE_STRIP high, that stands on `area`, in pixels
EMBED_INLINE long long place_existing(const CliPlaceCtx *args)
{
    const long long left = (args->place->x > args->area->x) ? args->place->x : args->area->x;
    const long long position_right = (long long)args->place->x + args->place->width;
    const long long area_right = (long long)args->area->x + args->area->width;
    const long long right = (position_right < area_right) ? position_right : area_right;
    const long long top = (args->place->y > args->area->y) ? args->place->y : args->area->y;
    const long long position_bottom = (long long)args->place->y + CLI_PLACE_STRIP;
    const long long area_bottom = (long long)args->area->y + args->area->height;
    const long long bottom = (position_bottom < area_bottom) ? position_bottom : area_bottom;
    const long long pixel_width = (right > left) ? right - left : 0LL;
    const long long high = (bottom > top) ? bottom - top : 0LL;
    return pixel_width * high;
}

// `at` moved in to stand between `start` and `start + span - size`
EMBED_INLINE int place_inside(const CliPlaceCtx *args)
{
    // a monitor's span and a size cut to it are below what an int counts, and the size is at most the span
    const int last = args->start + (int)(args->span - args->size);
    if (args->at < args->start)
    {
        return args->start;
    }
    if (args->at > last)
    {
        return last;
    }
    return args->at;
}

// `place` fitted to the monitors `areas`, `count` of them, the first the system's main one: put on the monitor its top
// edge stands on most, the last of those standing on as much, cut to that monitor's size and moved in to stand wholly
// on it; or, standing on none enough, put in the middle of the first: 1, or 0 where there is no monitor
EMBED_INLINE int place_fit(const CliPlaceCtx *args)
{
    if (args->count == 0u)
    {
        return 0;
    }
    unsigned int most = 0u;
    long long exists = EMBED_CALL(place_existing, CliPlaceCtx, .place = args->position_out, .area = &args->areas[0]);
    for (unsigned int at = 1u; at < args->count; at += 1u)
    {
        const long long existing =
            EMBED_CALL(place_existing, CliPlaceCtx, .place = args->position_out, .area = &args->areas[at]);
        if (existing >= exists)
        {
            most = at;
            exists = existing;
        }
    }
    const long long width_strip =
        (args->position_out->width < CLI_PLACE_STRIP) ? (long long)args->position_out->width : CLI_PLACE_STRIP;
    const int stands = exists >= CLI_PLACE_STRIP * width_strip;
    const CliWindowArea *const area = stands ? &args->areas[most] : &args->areas[0];
    const unsigned int width = (args->position_out->width < area->width) ? args->position_out->width : area->width;
    const unsigned int height = (args->position_out->height < area->height) ? args->position_out->height : area->height;
    if (stands)
    {
        args->position_out->x = EMBED_CALL(place_inside, CliPlaceCtx, .at = args->position_out->x, .size = width,
                                           .start = area->x, .span = area->width);
        args->position_out->y = EMBED_CALL(place_inside, CliPlaceCtx, .at = args->position_out->y, .size = height,
                                           .start = area->y, .span = area->height);
    }
    else
    {
        // half a monitor's span less a size cut to it is below what an int counts
        args->position_out->x = area->x + (int)((area->width - width) / 2u);
        args->position_out->y = area->y + (int)((area->height - height) / 2u);
    }
    args->position_out->width = width;
    args->position_out->height = height;
    return 1;
}

// the tree `root`, its folder as written with every `\` a `/`, into `key` with its bytes in `length`, and the file in
// orior's home into `path`, each sized for CLI_PATH_MAX_LEN: 1, or 0 where no tree is open or orior has no home
EMBED_INLINE int place_paths(const CliPlaceCtx *args)
{
    char home[CLI_PATH_MAX_LEN];
    if ((args->root == NULL) || !EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        return 0;
    }
    *args->length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_PATH_MAX_LEN});
    memor.cpy(&(MemoriaCfg){.dst = args->key, .src = args->root, .bytes = *args->length + 1ULL});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = args->key, .size = *args->length, .from = '\\',
               .to = '/');
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "places", .joined = args->path);
    return 1;
}

int cli_view_place_saved(const char *root, const CliWindowArea *areas, unsigned int count, CliWindowPosition *place)
{
    char key[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    unsigned long long key_length = 0ULL;
    if (!EMBED_CALL(place_paths, CliPlaceCtx, .root = root, .key = key, .path = path, .length = &key_length))
    {
        return 0;
    }
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = &g_cli_view_cells);
    int found = 0;
    unsigned long long at = 0ULL;
    while ((bytes != NULL) && !found && ((at + sizeof(CliPlaceRecord)) <= size))
    {
        CliPlaceRecord record;
        memor.cpy(&(MemoriaCfg){.dst = &record, .src = bytes + at, .bytes = sizeof(record)});
        at += sizeof(record);
        if ((at + record.tree_length) > size)
        {
            break;
        }
        found = (record.tree_length == key_length) &&
                (memor.cmp(&(MemoriaCfg){.src = bytes + at, .other = key, .bytes = key_length}) == 0);
        at += record.tree_length;
        if (found)
        {
            *place = record.place;
        }
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_view_cells);
    return found && (place->width >= CLI_POSITION_MIN_WIDTH) && (place->height >= CLI_POSITION_MIN_HEIGHT) &&
           EMBED_CALL(place_fit, CliPlaceCtx, .position_out = place, .areas = areas, .count = count);
}

void cli_view_place_store(const char *root, const CliWindowPosition *place)
{
    char key[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    unsigned long long key_length = 0ULL;
    if (!EMBED_CALL(place_paths, CliPlaceCtx, .root = root, .key = key, .path = path, .length = &key_length))
    {
        return;
    }
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = &g_cli_view_cells);
    const unsigned long long most = ((bytes != NULL) ? size : 0ULL) + sizeof(CliPlaceRecord) + key_length;
    unsigned char *const written = g_cli_cells->persistent_buf_alloc(most, &g_cli_view_cells);
    if (written != NULL)
    {
        // a key's bytes are below what a record's length counts
        const CliPlaceRecord own = {*place, (unsigned int)key_length};
        memor.cpy(&(MemoriaCfg){.dst = written, .src = &own, .bytes = sizeof(own)});
        memor.cpy(&(MemoriaCfg){.dst = written + sizeof(own), .src = key, .bytes = key_length});
        unsigned long long used = sizeof(own) + key_length;
        unsigned long long at = 0ULL;
        while ((bytes != NULL) && ((at + sizeof(CliPlaceRecord)) <= size))
        {
            CliPlaceRecord record;
            memor.cpy(&(MemoriaCfg){.dst = &record, .src = bytes + at, .bytes = sizeof(record)});
            const unsigned long long whole = sizeof(record) + record.tree_length;
            if ((at + whole) > size)
            {
                break;
            }
            const int same =
                (record.tree_length == key_length) &&
                (memor.cmp(&(MemoriaCfg){.src = bytes + at + sizeof(record), .other = key, .bytes = key_length}) == 0);
            if (!same)
            {
                memor.cpy(&(MemoriaCfg){.dst = written + used, .src = bytes + at, .bytes = whole});
                used += whole;
            }
            at += whole;
        }
        (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path,
                         .bytes = written, .size = used);
        g_cli_cells->persistent_buf_release(written, &g_cli_view_cells);
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_view_cells);
}
