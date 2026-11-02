// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_place.c: the window's place for each tree: where it stood, how large it was and whether it filled its monitor,
// saved in places.json in orior's home as the window closes, each tree under its folder as written, and put back as the
// tree opens, fitted to the monitors there are now. A place on no monitor there is goes onto the first, in its middle,
// and a place larger than its monitor is cut to it
#include "../text/text.h"
#include "window_internal.h"

// how much of a window's top edge must stand on a monitor for its place there to hold, in pixels: enough to take it by
// its title and move it
#define CLI_PLACE_STRIP 64ll

// the nodes a place put takes over those places.json holds: the tree's member and its five numbers; and the bytes of
// their texts, the tree's folder and the numbers
#define CLI_PLACE_NODES_MORE 8u
#define CLI_PLACE_TEXT_MORE (CLI_PATH_MAX_LEN + 128ull)

// the bytes places.json is written in, begun at and

static CliLock s_lock = CLI_LOCK_INIT;

typedef struct
{
    const CliWindowPlace *place;
    const CliWindowArea *area;
    int at;
    unsigned int size;
    int start;
    unsigned int span;
    CliWindowPlace *place_out;
    const CliWindowArea *areas;
    unsigned int count;
    char *key;
    char *path;
    VerbaLinea *err_msg;
    CliJson *json;
    unsigned int object;
    const char *name;
    long long value;
    const char *key_read;
    const char *path_read;
} WindowPlaceCtx;
// the part of `place`'s top strip, CLI_PLACE_STRIP high, that stands on `area`, in pixels
EMBED_INLINE long long place_standing(const WindowPlaceCtx *args)
{
    const long long left = (args->place->x > args->area->x) ? args->place->x : args->area->x;
    const long long place_right = (long long)args->place->x + args->place->width;
    const long long area_right = (long long)args->area->x + args->area->width;
    const long long right = (place_right < area_right) ? place_right : area_right;
    const long long top = (args->place->y > args->area->y) ? args->place->y : args->area->y;
    const long long place_bottom = (long long)args->place->y + CLI_PLACE_STRIP;
    const long long area_bottom = (long long)args->area->y + args->area->height;
    const long long bottom = (place_bottom < area_bottom) ? place_bottom : area_bottom;
    const long long wide = (right > left) ? right - left : 0ll;
    const long long high = (bottom > top) ? bottom - top : 0ll;
    return wide * high;
}

// `at` moved in to stand between `start` and `start + span - size`
EMBED_INLINE int place_inside(const WindowPlaceCtx *args)
{
    // a monitor's span and a size cut to it are below what an int counts, and the size is at most the span
    const int last = args->start + (int)(args->span - args->size);
    return (args->at < args->start) ? args->start : ((args->at > last) ? last : args->at);
}

// `place` fitted to the monitors `areas`, `count` of them, the first the system's main one: saved on the monitor its
// top edge stands on most, the last of those standing on as much, cut to that monitor's size and moved in to stand
// wholly on it; or, standing on none enough to hold, put in the middle of the first: 1, or 0 where there is no monitor
EMBED_INLINE int place_fit(const WindowPlaceCtx *args)
{
    if (args->count == 0u)
    {
        return 0;
    }
    unsigned int most = 0u;
    long long stands = EMBED_CALL(place_standing, WindowPlaceCtx, .place = args->place_out, .area = &args->areas[0]);
    for (unsigned int at = 1u; at < args->count; at += 1u)
    {
        const long long standing =
            EMBED_CALL(place_standing, WindowPlaceCtx, .place = args->place_out, .area = &args->areas[at]);
        if (standing >= stands)
        {
            most = at;
            stands = standing;
        }
    }
    const long long width_strip =
        (args->place_out->width < CLI_PLACE_STRIP) ? (long long)args->place_out->width : CLI_PLACE_STRIP;
    const int holds = stands >= CLI_PLACE_STRIP * width_strip;
    const CliWindowArea *const area = holds ? &args->areas[most] : &args->areas[0];
    const unsigned int width = (args->place_out->width < area->width) ? args->place_out->width : area->width;
    const unsigned int height = (args->place_out->height < area->height) ? args->place_out->height : area->height;
    if (holds)
    {
        args->place_out->x = EMBED_CALL(place_inside, WindowPlaceCtx, .at = args->place_out->x, .size = width,
                                        .start = area->x, .span = area->width);
        args->place_out->y = EMBED_CALL(place_inside, WindowPlaceCtx, .at = args->place_out->y, .size = height,
                                        .start = area->y, .span = area->height);
    }
    else
    {
        // half a monitor's span less a size cut to it is below what an int counts
        args->place_out->x = area->x + (int)((area->width - width) / 2u);
        args->place_out->y = area->y + (int)((area->height - height) / 2u);
    }
    args->place_out->width = width;
    args->place_out->height = height;
    return 1;
}

// the tree open, its folder as written with every `\` a `/`, into `key`, and places.json in orior's home into `path`,
// each sized for CLI_PATH_MAX_LEN: 1, or 0 with why given
EMBED_INLINE int place_paths(const WindowPlaceCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_root.root, CliServeErrMsgRootCfg, .root_out = args->key, .err_msg = err_msg) ||
        !EMBED_CALL(cli_window_internal.home, CliWindowCtx, .folder = home, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = args->key,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL}), .from = '\\',
               .to = '/');
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "places.json", .joined = args->path);
    return 1;
}

int cli_window_place_kept(const CliWindowArea *areas, unsigned int count, CliWindowPlace *place)
{
    char key[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(place_paths, WindowPlaceCtx, .key = key, .path = path, .err_msg = &err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    CliJson json;
    const unsigned int places =
        EMBED_CALL(cli_json_tree_err_msg.json_file_take, CliJsonTreeErrMsgCfg, .json = &json, .path = path,
                   .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_cli_window_cells);
    const CliServeArgs saved = {&json, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &json, .object = places, .name = key)};
    long long x = 0ll;
    long long y = 0ll;
    unsigned long long width = 0ull;
    unsigned long long height = 0ull;
    int maximized = 0;
    const int read = (places != CLI_JSON_NONE) &&
                     EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                .node = saved.object, .kind = CFG_JSON_OBJECT) &&
                     EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = &saved, .key = "x",
                                .value_i64s = &x, .err_msg = &err_msg) &&
                     EMBED_CALL(cli_serve_err_msg_internal.serve_sign, CliServeErrMsgCtx, .request = &saved, .key = "y",
                                .value_i64s = &y, .err_msg = &err_msg) &&
                     EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = &saved, .key = "width",
                                .value = &width, .err_msg = &err_msg) &&
                     EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = &saved, .key = "height",
                                .value = &height, .err_msg = &err_msg) &&
                     EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = &saved,
                                .key = "maximized", .value_i32s = &maximized, .err_msg = &err_msg);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    // a place saved is one the window stood in, each of its numbers within the 32 bits the system gives it
    *place = (CliWindowPlace){(int)x, (int)y, (unsigned int)width, (unsigned int)height, maximized};
    return read && (place->width >= CLI_PLACE_LEAST_WIDTH) && (place->height >= CLI_PLACE_LEAST_HEIGHT) &&
           EMBED_CALL(place_fit, WindowPlaceCtx, .place_out = place, .areas = areas, .count = count);
}

// a number written as the member `name` of `object`
EMBED_INLINE void place_number_set(const WindowPlaceCtx *args)
{
    CliJson *json = args->json;
    unsigned int object = args->object;
    const char *name = args->name;
    long long value = args->value;
    char digits[32];
    VerbaLinea written = {digits, sizeof(digits), 0ull};
    mmgr_line_put(&written, (value < 0ll) ? "-" : "");
    // the magnitude of a place's number is within the 32 bits the system gives it
    mmgr_line_decimal(&written, (unsigned long long)((value < 0ll) ? -value : value), 1u);
    (void)mmgr_line_finish(&written);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = object,
               .name = name,
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_number_init, CliJsonTreeTextLengthKindCfg,
                                   .json = json, .text = digits));
}

// `place` put under `key` among the places at `path`, those of the other trees saved, and the file written whole, the
// trees in the order of their keys: 1, or 0 with why given
EMBED_INLINE int place_write(const WindowPlaceCtx *args)
{
    const char *path = args->path_read;
    VerbaLinea *err_msg = args->err_msg;
    CliJson json;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    unsigned int places = EMBED_CALL(cli_json_tree_err_msg.json_file_take, CliJsonTreeErrMsgCfg, .json = &json,
                                     .path = path, .nodes_more = CLI_PLACE_NODES_MORE, .text_more = CLI_PLACE_TEXT_MORE,
                                     .err_msg = &unread, .owner = &g_cli_window_cells);
    if ((places == CLI_JSON_NONE) || !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                 .json_read = &json, .node = places, .kind = CFG_JSON_OBJECT))
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        if (!EMBED_CALL(cli_json_tree_nodes_most.json_reserve, CliJsonTreeNodesMostCfg, .json = &json,
                        .nodes_most = CLI_PLACE_NODES_MORE, .text_most = CLI_PLACE_TEXT_MORE,
                        .what = "a window's place", .owner = &g_cli_window_cells))
        {
            mmgr_line_put(err_msg, "the cells a window's place is written in could not be taken");
            return 0;
        }
        places = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &json, .kind = CFG_JSON_OBJECT);
    }
    const unsigned int saved =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(place_number_set, WindowPlaceCtx, .json = &json, .object = saved, .name = "x", .value = args->place->x);
    EMBED_CALL(place_number_set, WindowPlaceCtx, .json = &json, .object = saved, .name = "y", .value = args->place->y);
    EMBED_CALL(place_number_set, WindowPlaceCtx, .json = &json, .object = saved, .name = "width",
               .value = args->place->width);
    EMBED_CALL(place_number_set, WindowPlaceCtx, .json = &json, .object = saved, .name = "height",
               .value = args->place->height);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = saved,
               .name = "maximized",
               .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &json,
                                   .kind = args->place->maximized ? CFG_JSON_TRUE : CFG_JSON_FALSE));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = places,
               .name = args->key_read, .value = saved);
    const unsigned long long out_room =
        EMBED_CALL(cli_json_tree_node_out.json_pretty_room, CliJsonTreeNodeOutCfg, .json_read = &json, .indent = 2u);
    char *const out_cell = json.full ? NULL : g_cli_cells->temporary_buf_alloc(out_room, &g_cli_window_cells);
    VerbaLinea out = mmgr_line_over(out_cell, out_room);
    int written = out_cell != NULL;
    if (written)
    {
        EMBED_CALL(cli_json_tree_object_name.json_sort, CliJsonTreeObjectNameCfg, .json = &json, .object = places);
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = places,
                   .out = &out, .indent = 2u, .sorted = 0);
        written = mmgr_line_ok(&out) && EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                                   .path = path, .bytes = out.out, .size = out.at);
        g_cli_cells->persistent_buf_release(out.out, &g_cli_window_cells);
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    if (!written)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be written");
    }
    return written;
}

void cli_window_place_keep(const CliWindowPlace *place)
{
    char key[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    static char s_line[CLI_ERR_MSG_MAX_LEN + 64u];
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, "orior: the window's place: ");
    VerbaLinea err_msg = {s_line + line.at, sizeof(s_line) - line.at - 1u, 0ull};
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    if (EMBED_CALL(place_paths, WindowPlaceCtx, .key = key, .path = path, .err_msg = &err_msg) &&
        !EMBED_CALL(place_write, WindowPlaceCtx, .key_read = key, .path_read = path, .place = place,
                    .err_msg = &err_msg))
    {
        line.at += mmgr_line_ok(&err_msg) ? err_msg.at : 0ull;
        mmgr_line_ch(&line, '\n');
        EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = s_line,
                   .length = mmgr_line_ok(&line) ? line.at : 0ull);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}
