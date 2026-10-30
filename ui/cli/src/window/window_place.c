// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_place.c: the window's place for each tree: where it stood, how large it was and whether it filled its monitor,
// kept in places.json in orior's home as the window closes, each tree under its folder as written, and put back as the
// tree opens, fitted to the monitors there are now. A place on no monitor there is goes onto the first, in its middle,
// and a place larger than its monitor is cut to it
#include "window_internal.h"

// how much of a window's top edge must stand on a monitor for its place there to hold, in pixels: enough to take it by
// its title and move it
#define CLI_PLACE_HELD 64ll

// the nodes a place put takes over those places.json holds: the tree's member and its five numbers; and the bytes of
// their texts, the tree's folder and the numbers
#define CLI_PLACE_NODES_MORE 8u
#define CLI_PLACE_TEXT_MORE (CLI_PATH_MAX_LEN + 128ull)

// the bytes places.json is written in, begun at and doubling as it fills
#define CLI_PLACE_WRITTEN_FIRST 4096ull

static CliLock s_lock = CLI_LOCK_INIT;

// the part of `place`'s top strip, CLI_PLACE_HELD high, that stands on `area`, in pixels
static long long place_standing(const CliWindowPlace *place, const CliWindowArea *area)
{
    const long long left = (place->x > area->x) ? place->x : area->x;
    const long long place_right = (long long)place->x + place->width;
    const long long area_right = (long long)area->x + area->width;
    const long long right = (place_right < area_right) ? place_right : area_right;
    const long long top = (place->y > area->y) ? place->y : area->y;
    const long long place_bottom = (long long)place->y + CLI_PLACE_HELD;
    const long long area_bottom = (long long)area->y + area->height;
    const long long bottom = (place_bottom < area_bottom) ? place_bottom : area_bottom;
    const long long wide = (right > left) ? right - left : 0ll;
    const long long high = (bottom > top) ? bottom - top : 0ll;
    return wide * high;
}

// `at` moved in to stand between `start` and `start + span - size`
static int place_inside(int at, unsigned int size, int start, unsigned int span)
{
    // a monitor's span and a size cut to it are below what an int counts, and the size is at most the span
    const int last = start + (int)(span - size);
    return (at < start) ? start : ((at > last) ? last : at);
}

// `place` fitted to the monitors `areas`, `count` of them, the first the system's main one: kept on the monitor its top
// edge stands on most, the last of those standing on as much, cut to that monitor's size and moved in to stand wholly
// on it; or, standing on none enough to hold, put in the middle of the first: 1, or 0 where there is no monitor
static int place_fitted(CliWindowPlace *place, const CliWindowArea *areas, unsigned int count)
{
    if (count == 0u)
    {
        return 0;
    }
    unsigned int most = 0u;
    long long stands = place_standing(place, &areas[0]);
    for (unsigned int at = 1u; at < count; at += 1u)
    {
        const long long standing = place_standing(place, &areas[at]);
        if (standing >= stands)
        {
            most = at;
            stands = standing;
        }
    }
    const long long width_held = (place->width < CLI_PLACE_HELD) ? (long long)place->width : CLI_PLACE_HELD;
    const int holds = stands >= CLI_PLACE_HELD * width_held;
    const CliWindowArea *const area = holds ? &areas[most] : &areas[0];
    const unsigned int width = (place->width < area->width) ? place->width : area->width;
    const unsigned int height = (place->height < area->height) ? place->height : area->height;
    if (holds)
    {
        place->x = place_inside(place->x, width, area->x, area->width);
        place->y = place_inside(place->y, height, area->y, area->height);
    }
    else
    {
        // half a monitor's span less a size cut to it is below what an int counts
        place->x = area->x + (int)((area->width - width) / 2u);
        place->y = area->y + (int)((area->height - height) / 2u);
    }
    place->width = width;
    place->height = height;
    return 1;
}

// the tree open, its folder as written with every `\` a `/`, into `key`, and places.json in orior's home into `path`,
// each sized for CLI_PATH_MAX_LEN: 1, or 0 with why given
static int place_paths(char *key, char *path, VerbaLinea *err_msg)
{
    char home[CLI_PATH_MAX_LEN];
    if (!cli_serve_root(key, err_msg) || !cli_window_home(home, err_msg))
    {
        return 0;
    }
    for (char *at = key; *at != '\0'; at += 1)
    {
        *at = (*at == '\\') ? '/' : *at;
    }
    cli_full(home, "places.json", path);
    return 1;
}

int cli_window_place_kept(const CliWindowArea *areas, unsigned int count, CliWindowPlace *place)
{
    char key[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!place_paths(key, path, &err_msg))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    CliJson json;
    const unsigned int places = cli_json_file_taken(&json, path, 0u, 0ull, &err_msg, &g_cli_window_cells);
    const CliServeArgs kept = {&json, cli_json_member(&json, places, key)};
    long long x = 0ll;
    long long y = 0ll;
    unsigned long long width = 0ull;
    unsigned long long height = 0ull;
    int maximized = 0;
    const int read = (places != CLI_JSON_NONE) && cli_json_is(&json, kept.object, CFG_JSON_OBJECT) &&
                     cli_serve_signed(&kept, "x", &x, &err_msg) && cli_serve_signed(&kept, "y", &y, &err_msg) &&
                     cli_serve_whole(&kept, "width", &width, &err_msg) &&
                     cli_serve_whole(&kept, "height", &height, &err_msg) &&
                     cli_serve_bool(&kept, "maximized", &maximized, &err_msg);
    cli_json_given(&json);
    cli_lock_give(&s_lock);
    // a place kept is one the window stood in, each of its numbers within the 32 bits the system gives it
    *place = (CliWindowPlace){(int)x, (int)y, (unsigned int)width, (unsigned int)height, maximized};
    return read && place_fitted(place, areas, count);
}

// a number written as the member `name` of `object`
static void place_number_set(CliJson *json, unsigned int object, const char *name, long long value)
{
    char digits[32];
    VerbaLinea written = {digits, sizeof(digits), 0ull};
    mmgr_line_put(&written, (value < 0ll) ? "-" : "");
    // the magnitude of a place's number is within the 32 bits the system gives it
    mmgr_line_decimal(&written, (unsigned long long)((value < 0ll) ? -value : value), 1u);
    (void)mmgr_line_finish(&written);
    cli_json_set(json, object, name, cli_json_number_made(json, digits));
}

// `place` put under `key` among the places at `path`, those of the other trees kept, and the file written whole, the
// trees in the order of their keys: 1, or 0 with why given
static int place_written(const char *key, const char *path, const CliWindowPlace *place, VerbaLinea *err_msg)
{
    CliJson json;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    unsigned int places =
        cli_json_file_taken(&json, path, CLI_PLACE_NODES_MORE, CLI_PLACE_TEXT_MORE, &unread, &g_cli_window_cells);
    if ((places == CLI_JSON_NONE) || !cli_json_is(&json, places, CFG_JSON_OBJECT))
    {
        cli_json_given(&json);
        if (!cli_json_reserved(&json, CLI_PLACE_NODES_MORE, CLI_PLACE_TEXT_MORE, "a window's place",
                               &g_cli_window_cells))
        {
            mmgr_line_put(err_msg, "the cells a window's place is written in could not be taken");
            return 0;
        }
        places = cli_json_made(&json, CFG_JSON_OBJECT);
    }
    const unsigned int kept = cli_json_made(&json, CFG_JSON_OBJECT);
    place_number_set(&json, kept, "x", place->x);
    place_number_set(&json, kept, "y", place->y);
    place_number_set(&json, kept, "width", place->width);
    place_number_set(&json, kept, "height", place->height);
    cli_json_set(&json, kept, "maximized", cli_json_made(&json, place->maximized ? CFG_JSON_TRUE : CFG_JSON_FALSE));
    cli_json_set(&json, places, key, kept);
    VerbaLinea out;
    int written = !json.full && cli_growing_line_temporary(&out, CLI_PLACE_WRITTEN_FIRST, &g_cli_window_cells);
    if (written)
    {
        cli_json_sorted(&json, places);
        cli_json_write(&json, places, &out, 2u, 0);
        written = mmgr_line_ok(&out) && query_file_written(path, out.out, out.at);
        g_cli_cells->persistent_buf_release(out.out, &g_cli_window_cells);
    }
    cli_json_given(&json);
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
    cli_lock_take(&s_lock);
    if (place_paths(key, path, &err_msg) && !place_written(key, path, place, &err_msg))
    {
        line.at += mmgr_line_ok(&err_msg) ? err_msg.at : 0ull;
        mmgr_line_ch(&line, '\n');
        query_terminal_written(1, s_line, mmgr_line_ok(&line) ? line.at : 0ull);
    }
    cli_lock_give(&s_lock);
}
