// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_place.c: the window's place for each tree: where it stood, how large it was and whether it filled its
// monitor, saved in the file `places` in orior's home as the window closes, one record for each tree, and put back as
// the tree opens, fitted to the monitors there are now. A place on no monitor there is goes onto the first, in its
// middle, and a place larger than its monitor is cut to it
#include "controller.h"
#include "controller_internal.h"

#include "../command_line/cells.h"
#include "../text/text.h"

// how much of a window's top edge must stand on a monitor for its place there to hold, in pixels: enough to take it by
// its title and move it
#define CONTROLLER_POSITION_STRIP 64ll

// One record of the file: the place and the bytes of the tree's folder after it, its `\` each a `/`
typedef struct
{
    CliWindowPosition place;
    unsigned int tree_length;
} ControllerPlaceRecord;

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
    char *key;
    char *path;
    unsigned long long *length;
} ControllerPlaceCtx;

// the part of `place`'s top strip, CONTROLLER_POSITION_STRIP high, that stands on `area`, in pixels
EMBED_INLINE long long position_existing(const ControllerPlaceCtx *args)
{
    const long long left = (args->place->x > args->area->x) ? args->place->x : args->area->x;
    const long long position_right = (long long)args->place->x + args->place->width;
    const long long area_right = (long long)args->area->x + args->area->width;
    const long long right = (position_right < area_right) ? position_right : area_right;
    const long long top = (args->place->y > args->area->y) ? args->place->y : args->area->y;
    const long long position_bottom = (long long)args->place->y + CONTROLLER_POSITION_STRIP;
    const long long area_bottom = (long long)args->area->y + args->area->height;
    const long long bottom = (position_bottom < area_bottom) ? position_bottom : area_bottom;
    const long long pixel_width = (right > left) ? right - left : 0ll;
    const long long high = (bottom > top) ? bottom - top : 0ll;
    return pixel_width * high;
}

// `at` moved in to stand between `start` and `start + span - size`
EMBED_INLINE int position_inside(const ControllerPlaceCtx *args)
{
    // a monitor's span and a size cut to it are below what an int counts, and the size is at most the span
    const int last = args->start + (int)(args->span - args->size);
    return (args->at < args->start) ? args->start : ((args->at > last) ? last : args->at);
}

// `place` fitted to the monitors `areas`, `count` of them, the first the system's main one: saved on the monitor its
// top edge stands on most, the last of those standing on as much, cut to that monitor's size and moved in to stand
// wholly on it; or, standing on none enough to hold, put in the middle of the first: 1, or 0 where there is no monitor
EMBED_INLINE int position_fit(const ControllerPlaceCtx *args)
{
    if (args->count == 0u)
    {
        return 0;
    }
    unsigned int most = 0u;
    long long exists =
        EMBED_CALL(position_existing, ControllerPlaceCtx, .place = args->position_out, .area = &args->areas[0]);
    for (unsigned int at = 1u; at < args->count; at += 1u)
    {
        const long long existing =
            EMBED_CALL(position_existing, ControllerPlaceCtx, .place = args->position_out, .area = &args->areas[at]);
        if (existing >= exists)
        {
            most = at;
            exists = existing;
        }
    }
    const long long width_strip = (args->position_out->width < CONTROLLER_POSITION_STRIP)
                                      ? (long long)args->position_out->width
                                      : CONTROLLER_POSITION_STRIP;
    const int holds = exists >= CONTROLLER_POSITION_STRIP * width_strip;
    const CliWindowArea *const area = holds ? &args->areas[most] : &args->areas[0];
    const unsigned int width = (args->position_out->width < area->width) ? args->position_out->width : area->width;
    const unsigned int height = (args->position_out->height < area->height) ? args->position_out->height : area->height;
    if (holds)
    {
        args->position_out->x = EMBED_CALL(position_inside, ControllerPlaceCtx, .at = args->position_out->x,
                                           .size = width, .start = area->x, .span = area->width);
        args->position_out->y = EMBED_CALL(position_inside, ControllerPlaceCtx, .at = args->position_out->y,
                                           .size = height, .start = area->y, .span = area->height);
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

// the tree open, its folder as written with every `\` a `/`, into `key` with its bytes in `length`, and the file in
// orior's home into `path`, each sized for CLI_PATH_MAX_LEN: 1, or 0 where no tree is open or orior has no home
EMBED_INLINE int position_paths(const ControllerPlaceCtx *args)
{
    char home[CLI_PATH_MAX_LEN];
    const char *const root = cli_controller_root();
    if ((root == NULL) || !EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        return 0;
    }
    *args->length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_PATH_MAX_LEN});
    memor.cpy(&(MemoriaCfg){.dst = args->key, .src = root, .bytes = *args->length + 1ull});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = args->key, .size = *args->length, .from = '\\',
               .to = '/');
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "places", .joined = args->path);
    return 1;
}

int cli_controller_place_saved(const CliWindowArea *areas, unsigned int count, CliWindowPosition *place)
{
    char key[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    unsigned long long key_length = 0ull;
    if (!EMBED_CALL(position_paths, ControllerPlaceCtx, .key = key, .path = path, .length = &key_length))
    {
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = &g_cli_controller_cells);
    int found = 0;
    unsigned long long at = 0ull;
    while ((bytes != NULL) && !found && ((at + sizeof(ControllerPlaceRecord)) <= size))
    {
        ControllerPlaceRecord record;
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
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_controller_cells);
    return found && (place->width >= CLI_POSITION_MIN_WIDTH) && (place->height >= CLI_POSITION_MIN_HEIGHT) &&
           EMBED_CALL(position_fit, ControllerPlaceCtx, .position_out = place, .areas = areas, .count = count);
}

void cli_controller_place_store(const CliWindowPosition *place)
{
    char key[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    unsigned long long key_length = 0ull;
    if (!EMBED_CALL(position_paths, ControllerPlaceCtx, .key = key, .path = path, .length = &key_length))
    {
        return;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = &g_cli_controller_cells);
    const unsigned long long most = ((bytes != NULL) ? size : 0ull) + sizeof(ControllerPlaceRecord) + key_length;
    unsigned char *const written = g_cli_cells->persistent_buf_alloc(most, &g_cli_controller_cells);
    if (written != NULL)
    {
        // a key's bytes are below what a record's length counts
        const ControllerPlaceRecord own = {*place, (unsigned int)key_length};
        memor.cpy(&(MemoriaCfg){.dst = written, .src = &own, .bytes = sizeof(own)});
        memor.cpy(&(MemoriaCfg){.dst = written + sizeof(own), .src = key, .bytes = key_length});
        unsigned long long used = sizeof(own) + key_length;
        unsigned long long at = 0ull;
        while ((bytes != NULL) && ((at + sizeof(ControllerPlaceRecord)) <= size))
        {
            ControllerPlaceRecord record;
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
        g_cli_cells->persistent_buf_release(written, &g_cli_controller_cells);
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_controller_cells);
}
