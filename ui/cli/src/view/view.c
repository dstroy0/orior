// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// view.c: the view's thread and what joins it to the window's. The system's thread writes the events' two rings and
// reads the orders' ring; the view's thread reads the first and writes the second. A raster ordered shown stands
// untouched until its event comes back, one at a time on each surface, which paces the frames by the system's own
// showing. A thread that finds nothing to do tests its rings again after reading the wake's generation and sleeps on
// that generation, so a write after the test ends the sleep.
#include "view.h"

#include "../command_line/cli_internal.h"
#include "../command_line/command_line.h"
#include "window.h"

// the events and the orders the rings hold, each a power of two, the bytes of the events' names and text on their way,
// the bytes one event's name or text may take, the buffers a surface's map is handed over in and the rectangles a map
// holds at the most
#define CLI_VIEW_EVENTS 256u
#define CLI_VIEW_EVENT_SEGMENT 128u
#define CLI_VIEW_EVENT_BYTES 4096u
#define CLI_VIEW_EVENT_TEXT 512u
#define CLI_VIEW_ORDERS 16u
#define CLI_VIEW_ORDER_SEGMENT 256u
#define CLI_VIEW_HIT_BUFFERS 3u
// the bit that says a map waits to be taken, over the index of its buffer
#define CLI_VIEW_HITS_FRESH 4ULL
EMBED_STATIC_ASSERT(sizeof(CliViewEvent) <= CLI_VIEW_EVENT_SEGMENT,
                    "a CliViewEvent fits one segment of the event ring");
EMBED_STATIC_ASSERT(sizeof(CliViewOrder) <= CLI_VIEW_ORDER_SEGMENT,
                    "a CliViewOrder fits one segment of the order ring");

// A surface's map on its way over: the buffers, which the two threads swap through `shared`, the writer's and the
// reader's
typedef struct
{
    _Alignas(64) embed_atomic_u64 shared;
    CliViewHits *buffer;
    unsigned int back;
    unsigned int front;
} CliViewHitsTransfer;

// The view: its window; the rings and the maps, in one cell; what wakes each thread; whether the thread is to end and
// whether it runs; a raster's showing awaited on each surface, the framebuffer a whole frame ordered stands in, when
// the last was shown and the monitor's refresh; when the tick the window wants is due; whether Preferences' window was
// ordered open; whether the main window's frame has been laid, and the icons
typedef struct
{
    GuiWindow window;
    mmgr_ring event_ring;
    mmgr_ring event_bytes_ring;
    mmgr_ring order_ring;
    CliViewHitsTransfer hits[CLI_VIEW_SURFACES];
    uint8_t *cell;
    CliWake wake;
    CliThread thread;
    CliViewWoken woken;
    void *context;
    _Alignas(64) embed_atomic_u32 ending;
    _Alignas(64) embed_atomic_u32 running;
    CliWake ended;
    int presenting_main;
    int presenting_preferences;
    int frame_ordered;
    unsigned long long presented_ms;
    unsigned int frame_ms;
    unsigned long long tick_at_ms;
    int preferences_opened;
    int close_ordered;
    int icons_ordered;
    GuiRaster icon_small;
    GuiRaster icon_large;
} CliView;

static CliView s_view;

unsigned int cli_view_hit_of(const CliViewHits *hits, int x, int y)
{
    if (hits->width == 0u)
    {
        return GUI_HIT_CLIENT;
    }
    // a client's sides are far inside what an int represents
    const int left = x < hits->border;
    const int right = x >= (int)hits->width - hits->border;
    const int top = y < hits->border;
    const int bottom = y >= (int)hits->height - hits->border;
    if (top)
    {
        if (left)
        {
            return GUI_HIT_TOP_LEFT;
        }
        if (right)
        {
            return GUI_HIT_TOP_RIGHT;
        }
        return GUI_HIT_TOP;
    }
    if (bottom)
    {
        if (left)
        {
            return GUI_HIT_BOTTOM_LEFT;
        }
        if (right)
        {
            return GUI_HIT_BOTTOM_RIGHT;
        }
        return GUI_HIT_BOTTOM;
    }
    if (left || right)
    {
        return left ? GUI_HIT_LEFT : GUI_HIT_RIGHT;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = hits->minimize, .x = x, .y = y))
    {
        return GUI_HIT_MINIMIZE;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = hits->maximize, .x = x, .y = y))
    {
        return GUI_HIT_MAXIMIZE;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = hits->close, .x = x, .y = y))
    {
        return GUI_HIT_CLOSE;
    }
    for (unsigned int at = 0u; at < hits->clients; at += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = hits->client[at], .x = x, .y = y))
        {
            return GUI_HIT_CLIENT;
        }
    }
    return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = hits->caption, .x = x, .y = y)
               ? GUI_HIT_CAPTION
               : GUI_HIT_CLIENT;
}

unsigned int cli_view_hit(unsigned int surface, int x, int y)
{
    CliViewHitsTransfer *const transfer = &s_view.hits[surface];
    if ((EMBED_ATOMIC_LOAD(&transfer->shared) & CLI_VIEW_HITS_FRESH) != 0ULL)
    {
        // an index of three buffers is far inside what an unsigned int counts
        transfer->front = (unsigned int)(EMBED_ATOMIC_EXCHANGE_64(&transfer->shared, (embed_u64)transfer->front) &
                                         ~CLI_VIEW_HITS_FRESH);
    }
    return cli_view_hit_of(&transfer->buffer[transfer->front], x, y);
}

// the writer's buffer of `surface`'s map handed over, the buffer last on its way the writer's next
static void view_hits_publish(unsigned int surface)
{
    CliViewHitsTransfer *const transfer = &s_view.hits[surface];
    // an index of three buffers is far inside what an unsigned int counts
    transfer->back =
        (unsigned int)(EMBED_ATOMIC_EXCHANGE_64(&transfer->shared, (embed_u64)transfer->back | CLI_VIEW_HITS_FRESH) &
                       ~CLI_VIEW_HITS_FRESH);
}

void cli_view_event(const CliViewEvent *event)
{
    const int keyed = (event->kind == CLI_VIEW_EVENT_INPUT) && (event->input.type == GUI_INPUT_KEY);
    const int typed = (event->kind == CLI_VIEW_EVENT_INPUT) && (event->input.type == GUI_INPUT_TEXT);
    const char *bytes;
    if (keyed)
    {
        bytes = event->input.name;
    }
    else if (typed)
    {
        bytes = event->input.text;
    }
    else
    {
        bytes = NULL;
    }
    unsigned long long length;
    if (keyed)
    {
        length = cellul.len(&(CatenaFinitaCfg){.src = bytes, .cap = CLI_VIEW_EVENT_TEXT - 1ULL}) + 1ULL;
    }
    else if (typed)
    {
        length = event->input.size;
    }
    else
    {
        length = 0ULL;
    }
    size_t at = 0u;
    // the bytes a name or a typing takes are far inside what a size_t counts
    if ((length < CLI_VIEW_EVENT_TEXT) &&
        (anularis.vacant(&(AnularisCfg){.ring = &s_view.event_bytes_ring}) >= (size_t)length) &&
        anularis.seg_next(&(AnularisCfg){.ring = &s_view.event_ring, .out_index = &at}))
    {
        (void)anularis.put(&(AnularisCfg){
            .ring = &s_view.event_bytes_ring, .src = (const uint8_t *)(const void *)bytes, .bytes = (size_t)length});
        CliViewEvent *const written =
            (CliViewEvent *)(void *)anularis.seg_at(&(AnularisCfg){.ring = &s_view.event_ring, .index = at});
        *written = *event;
        written->input.size = length;
        anularis.seg_publish(&(AnularisCfg){.ring = &s_view.event_ring});
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_view.wake);
    }
}

int cli_view_order(CliViewOrder *order)
{
    size_t at = 0u;
    if (!anularis.seg_front(&(AnularisCfg){.ring = &s_view.order_ring, .out_index = &at}))
    {
        return 0;
    }
    *order =
        *(const CliViewOrder *)(const void *)anularis.seg_at(&(AnularisCfg){.ring = &s_view.order_ring, .index = at});
    anularis.seg_release(&(AnularisCfg){.ring = &s_view.order_ring});
    return 1;
}

// an order written and the system's thread woken: 1, or 0 where the ring is full
static int view_order_write(const CliViewOrder *order)
{
    size_t at = 0u;
    if (!anularis.seg_next(&(AnularisCfg){.ring = &s_view.order_ring, .out_index = &at}))
    {
        return 0;
    }
    *(CliViewOrder *)(void *)anularis.seg_at(&(AnularisCfg){.ring = &s_view.order_ring, .index = at}) = *order;
    anularis.seg_publish(&(AnularisCfg){.ring = &s_view.order_ring});
    s_view.woken(s_view.context);
    return 1;
}

// the eye made at the two sizes and given to the system, the rasters standing until the view ends
static void view_icons_order(void)
{
    if (s_view.icons_ordered)
    {
        return;
    }
    s_view.icons_ordered = 1;
    if (EMBED_CALL(cli_window_icon.icon, GuiWindowIconCfg, .window = &s_view.window, .icon = &s_view.icon_small,
                   .size = GUI_ICON_SMALL) &&
        EMBED_CALL(cli_window_icon.icon, GuiWindowIconCfg, .window = &s_view.window, .icon = &s_view.icon_large,
                   .size = GUI_ICON_LARGE))
    {
        const CliViewOrder order = {
            .kind = CLI_VIEW_ORDER_ICON, .raster = s_view.icon_small, .raster_large = s_view.icon_large};
        (void)view_order_write(&order);
    }
}

// an input of the main window: a window control pressed acts as the button is let go over it
static void view_input(const GuiInput *input)
{
    GuiWindow *const window = &s_view.window;
    const unsigned int pressed = window->pressed;
    const int released = input->type == GUI_INPUT_UP;
    if (released)
    {
        window->pressed = GUI_HIT_CLIENT;
    }
    EMBED_CALL(gui_window_input_kind.input, GuiWindowInputKindCfg, .window = window, .input = input);
    if (released && (pressed != GUI_HIT_CLIENT) &&
        (EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = window, .input = input) == pressed))
    {
        const unsigned int other_act = (pressed == GUI_HIT_MAXIMIZE) ? GUI_ACT_MAXIMIZE : GUI_ACT_CLOSE;
        const CliViewOrder order = {.kind = CLI_VIEW_ORDER_ACT,
                                    .act = (pressed == GUI_HIT_MINIMIZE) ? GUI_ACT_MINIMIZE : other_act};
        (void)view_order_write(&order);
    }
}

// a raster's showing come back: the framebuffer a whole frame stood in shown, the two flipped
static void view_presented(unsigned int surface)
{
    if (surface != CLI_VIEW_MAIN)
    {
        s_view.presenting_preferences = 0;
        return;
    }
    s_view.presenting_main = 0;
    s_view.presented_ms = cli_clock_milliseconds();
    if (s_view.frame_ordered)
    {
        s_view.frame_ordered = 0;
        EMBED_CALL(gui_window_table.flip, GuiWindowCfg, .window = &s_view.window);
    }
}

// one event answered
static void view_event(const CliViewEvent *event)
{
    GuiWindow *const window = &s_view.window;
    switch (event->kind)
    {
    case CLI_VIEW_EVENT_SIZED:
        if (event->surface == CLI_VIEW_PREFERENCES)
        {
            EMBED_CALL(gui_window_width.preferences_resize, GuiWindowWidthCfg, .window = window, .width = event->width,
                       .height = event->height, .system_scale = event->scale);
        }
        else
        {
            s_view.frame_ms = event->frame_ms;
            (void)EMBED_CALL(gui_window_width.resize, GuiWindowWidthCfg, .window = window, .width = event->width,
                             .height = event->height, .system_scale = event->scale);
            view_icons_order();
        }
        break;
    case CLI_VIEW_EVENT_INPUT:
        if (event->surface == CLI_VIEW_PREFERENCES)
        {
            EMBED_CALL(gui_window_input_kind.preferences_input, GuiWindowInputKindCfg, .window = window,
                       .input = &event->input);
        }
        else
        {
            view_input(&event->input);
        }
        break;
    case CLI_VIEW_EVENT_CLOSING:
        if (window->preferences.displayed)
        {
            EMBED_CALL(gui_window_table.preferences_close, GuiWindowCfg, .window = window);
        }
        break;
    case CLI_VIEW_EVENT_DISPLAYED:
        EMBED_CALL(cli_window_shown.show, GuiWindowDisplayedCfg, .window = window, .displayed = event->flag);
        break;
    case CLI_VIEW_EVENT_MOTION:
        EMBED_CALL(gui_window_reduced.motion_wish, GuiWindowReducedCfg, .window = window, .reduced = event->flag);
        break;
    case CLI_VIEW_EVENT_PRESENTED:
        view_presented(event->surface);
        break;
    case CLI_VIEW_EVENT_REFUSED:
        window->preferences.displayed = 0;
        s_view.preferences_opened = 0;
        break;
    default:
        break;
    }
}

// every event waiting answered, a name or a text read out of its ring as its event is
static void view_events_take(void)
{
    char bytes[CLI_VIEW_EVENT_TEXT];
    size_t at = 0u;
    while (anularis.seg_front(&(AnularisCfg){.ring = &s_view.event_ring, .out_index = &at}))
    {
        CliViewEvent event = *(const CliViewEvent *)(const void *)anularis.seg_at(
            &(AnularisCfg){.ring = &s_view.event_ring, .index = at});
        anularis.seg_release(&(AnularisCfg){.ring = &s_view.event_ring});
        // the event's writer holds what it wrote under the text's bound
        const size_t length = anularis.read(&(AnularisCfg){
            .ring = &s_view.event_bytes_ring, .dst = (uint8_t *)(void *)bytes, .bytes = (size_t)event.input.size});
        bytes[(length < (CLI_VIEW_EVENT_TEXT - 1u)) ? length : (CLI_VIEW_EVENT_TEXT - 1u)] = '\0';
        event.input.name = bytes;
        event.input.text = bytes;
        view_event(&event);
    }
}

// Preferences' window opened or closed as the command last left it, and its frame drawn and ordered shown where one is
// owed and none is on its way
static void view_preferences_follow(void)
{
    GuiWindow *const window = &s_view.window;
    if (window->preferences.displayed && !s_view.preferences_opened)
    {
        CliViewOrder order = {.kind = CLI_VIEW_ORDER_PREFERENCES_OPEN, .surface = CLI_VIEW_PREFERENCES};
        const int zoomed =
            (window->system_scale * (int)EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg,
                                                    .step = window->preference_values.zoom_step)) /
            GUI_SCALE_ONE;
        EMBED_CALL(gui_preferences_scale.want, GuiPreferencesScaleCfg, .palette = window->palette, .scale = zoomed,
                   .width_pointer = &order.width, .height_pointer = &order.height);
        if (view_order_write(&order))
        {
            s_view.preferences_opened = 1;
        }
    }
    else if (!window->preferences.displayed && s_view.preferences_opened)
    {
        const CliViewOrder order = {.kind = CLI_VIEW_ORDER_PREFERENCES_CLOSE, .surface = CLI_VIEW_PREFERENCES};
        if (view_order_write(&order))
        {
            s_view.preferences_opened = 0;
            s_view.presenting_preferences = 0;
        }
    }
    if (s_view.preferences_opened && window->preferences_pending && !s_view.presenting_preferences)
    {
        EMBED_CALL(gui_window_table.preferences_draw, GuiWindowCfg, .window = window);
        if (window->preferences_raster.pixels != NULL)
        {
            EMBED_CALL(gui_window_table.preferences_hits, GuiWindowHitsCfg, .window = window,
                       .hits = &s_view.hits[CLI_VIEW_PREFERENCES].buffer[s_view.hits[CLI_VIEW_PREFERENCES].back]);
            view_hits_publish(CLI_VIEW_PREFERENCES);
            const CliViewOrder order = {
                .kind = CLI_VIEW_ORDER_PRESENT, .surface = CLI_VIEW_PREFERENCES, .raster = window->preferences_raster};
            if (view_order_write(&order))
            {
                s_view.presenting_preferences = 1;
            }
            else
            {
                window->preferences_pending = 1;
            }
        }
    }
}

// a part of the raster shown, drawn over it, ordered shown again
static void view_part_order(GuiRect part)
{
    const GuiWindow *const window = &s_view.window;
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = part))
    {
        return;
    }
    const CliViewOrder order = {.kind = CLI_VIEW_ORDER_PRESENT_PART,
                                .surface = CLI_VIEW_MAIN,
                                .part = part,
                                .raster = *((window->displayed != NULL) ? window->displayed : window->raster)};
    s_view.presenting_main = view_order_write(&order);
}

// what is owed of the main window drawn and ordered shown, one at a time: a frame whole where one is owed, else the
// caret's ink or the circle's stage alone over the frame shown
static void view_frame(void)
{
    GuiWindow *const window = &s_view.window;
    if (s_view.presenting_main)
    {
        return;
    }
    if (window->pending)
    {
        EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = window);
        if (window->raster->pixels == NULL)
        {
            return;
        }
        EMBED_CALL(gui_window_table.hits, GuiWindowHitsCfg, .window = window,
                   .hits = &s_view.hits[CLI_VIEW_MAIN].buffer[s_view.hits[CLI_VIEW_MAIN].back]);
        view_hits_publish(CLI_VIEW_MAIN);
        const CliViewOrder order = {
            .kind = CLI_VIEW_ORDER_PRESENT, .surface = CLI_VIEW_MAIN, .raster = *window->raster};
        if (view_order_write(&order))
        {
            s_view.presenting_main = 1;
            s_view.frame_ordered = 1;
        }
        else
        {
            window->pending = 1;
        }
        return;
    }
    if (window->caret_alone)
    {
        view_part_order(EMBED_CALL(gui_window_table.caret_draw, GuiWindowCfg, .window = window));
    }
    if (window->busy_alone && !s_view.presenting_main)
    {
        view_part_order(EMBED_CALL(gui_window_table.busy_draw, GuiWindowCfg, .window = window));
    }
}

// what the view's thread runs: every event in turn, the window's modules stepped, the tick where its time came and what
// is owed drawn, until the view ends; the window's own moving things come to it by the clock, each no sooner than the
// monitor's next refresh after the last frame shown
static void view_run(void *context)
{
    (void)context;
    GuiWindow *const window = &s_view.window;
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &s_view.wake);
        view_events_take();
        EMBED_CALL(gui_window_table.step, GuiWindowCfg, .window = window);
        const unsigned long long now_ms = cli_clock_milliseconds();
        if ((s_view.tick_at_ms != 0ULL) && (now_ms >= s_view.tick_at_ms))
        {
            s_view.tick_at_ms = 0ULL;
            EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = window);
        }
        view_preferences_follow();
        // a command asked the window to close
        if (window->closing && !s_view.close_ordered)
        {
            const CliViewOrder order = {.kind = CLI_VIEW_ORDER_ACT, .act = GUI_ACT_CLOSE};
            s_view.close_ordered = view_order_write(&order);
        }
        view_frame();
        if (EMBED_ATOMIC_LOAD(&s_view.ending) != 0u)
        {
            break;
        }
        const unsigned long long after =
            EMBED_CALL(cli_window_input.timeout_ms, GuiWindowInputCfg, .window_read = window);
        if (after == 0ULL)
        {
            s_view.tick_at_ms = 0ULL;
            EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &s_view.wake, .seen = seen);
            continue;
        }
        const unsigned long long since_ms = cli_clock_milliseconds() - s_view.presented_ms;
        const unsigned long long refresh_ms = (since_ms < s_view.frame_ms) ? s_view.frame_ms - since_ms : 0ULL;
        const unsigned long long wait_ms = (after > refresh_ms) ? after : refresh_ms;
        s_view.tick_at_ms = cli_clock_milliseconds() + wait_ms;
        (void)EMBED_CALL(cli_wake_internal.wake_wait_seen_for, CliWakeCtx, .wake = &s_view.wake, .seen = seen,
                         .milliseconds_u64 = wait_ms);
    }
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &s_view.icon_small);
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &s_view.icon_large);
    EMBED_CALL(gui_window_table.end, GuiWindowCfg, .window = window);
    (void)EMBED_ATOMIC_ADD_32(&s_view.running, (embed_u32)-1);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_view.ended);
}

// what a module's thread brings, a command through the door, a server's report or a line the walk colored, wakes the
// view's thread
static void view_woken(void)
{
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_view.wake);
}

// the rings and the maps laid in one cell, whichever bytes each takes: 1, or 0 where the cell cannot be taken
static int view_cell_init(void)
{
    const size_t event_bytes = (size_t)CLI_VIEW_EVENTS * CLI_VIEW_EVENT_SEGMENT;
    const size_t order_bytes = (size_t)CLI_VIEW_ORDERS * CLI_VIEW_ORDER_SEGMENT;
    const size_t map_bytes = sizeof(CliViewHits) + (GUI_WINDOW_HIT_CLIENTS * sizeof(GuiRect));
    const size_t maps_bytes = (size_t)CLI_VIEW_SURFACES * CLI_VIEW_HIT_BUFFERS * map_bytes;
    s_view.cell = g_cli_cells->persistent_buf_alloc(event_bytes + CLI_VIEW_EVENT_BYTES + order_bytes + maps_bytes,
                                                    &g_cli_view_cells);
    if (s_view.cell == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){
        .dst = s_view.cell + event_bytes + CLI_VIEW_EVENT_BYTES + order_bytes, .val = 0, .bytes = maps_bytes});
    (void)anularis.init(&(AnularisCfg){
        .ring = &s_view.event_ring, .buf = s_view.cell, .capacity = event_bytes, .segment_count = CLI_VIEW_EVENTS});
    (void)anularis.init(&(AnularisCfg){.ring = &s_view.event_bytes_ring,
                                       .buf = s_view.cell + event_bytes,
                                       .capacity = CLI_VIEW_EVENT_BYTES,
                                       .segment_count = 1u});
    (void)anularis.init(&(AnularisCfg){.ring = &s_view.order_ring,
                                       .buf = s_view.cell + event_bytes + CLI_VIEW_EVENT_BYTES,
                                       .capacity = order_bytes,
                                       .segment_count = CLI_VIEW_ORDERS});
    uint8_t *map = s_view.cell + event_bytes + CLI_VIEW_EVENT_BYTES + order_bytes;
    // the maps lie on a boundary of the rings' segments and their rectangles after them
    CliViewHits *const headers = (CliViewHits *)(void *)map;
    GuiRect *const rects = (GuiRect *)(void *)(headers + ((size_t)CLI_VIEW_SURFACES * CLI_VIEW_HIT_BUFFERS));
    for (unsigned int surface = 0u; surface < CLI_VIEW_SURFACES; surface += 1u)
    {
        s_view.hits[surface].buffer = headers + (surface * CLI_VIEW_HIT_BUFFERS);
        s_view.hits[surface].back = 0u;
        s_view.hits[surface].front = 2u;
        EMBED_ATOMIC_STORE(&s_view.hits[surface].shared, (embed_u64)1u);
        for (unsigned int at = 0u; at < CLI_VIEW_HIT_BUFFERS; at += 1u)
        {
            s_view.hits[surface].buffer[at].client =
                rects + ((((size_t)surface * CLI_VIEW_HIT_BUFFERS) + at) * GUI_WINDOW_HIT_CLIENTS);
        }
    }
    return 1;
}

int cli_view_begin(const CliViewBeginCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = &s_view, .val = 0, .bytes = sizeof(s_view)});
    s_view.woken = args->woken;
    s_view.context = args->context;
    s_view.frame_ms = GUI_VIEW_FRAME_MS;
    if (!view_cell_init())
    {
        mmgr_line_put(args->err_msg, "the view's rings could not be taken");
        return 0;
    }
    if (!EMBED_CALL(gui_window_woken.begin, GuiWindowWokenCfg, .window = &s_view.window, .err_msg = args->err_msg,
                    .display_width = args->display_width, .display_height = args->display_height))
    {
        g_cli_cells->persistent_buf_release(s_view.cell, &g_cli_view_cells);
        return 0;
    }
    cli_command_line_tree_begin(view_woken);
    EMBED_CALL(gui_window_table.start, GuiWindowCfg, .window = &s_view.window, .woken = view_woken);
    s_view.thread.run = view_run;
    s_view.thread.context = NULL;
    s_view.thread.priority = CLI_PRIORITY_WORK;
    (void)EMBED_ATOMIC_ADD_32(&s_view.running, 1u);
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_view.thread))
    {
        (void)EMBED_ATOMIC_ADD_32(&s_view.running, (embed_u32)-1);
        mmgr_line_put(args->err_msg, "the view's thread could not be started");
        EMBED_CALL(gui_window_table.end, GuiWindowCfg, .window = &s_view.window);
        cli_command_line_tree_end();
        g_cli_cells->persistent_buf_release(s_view.cell, &g_cli_view_cells);
        return 0;
    }
    return 1;
}

void cli_view_end(void)
{
    EMBED_ATOMIC_STORE(&s_view.ending, 1u);
    EMBED_ATOMIC_ORDERED();
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_view.wake);
    while (EMBED_ATOMIC_LOAD(&s_view.running) != 0u)
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_view.ended, .milliseconds_u64 = 1ULL);
    }
    cli_command_line_tree_end();
    g_cli_cells->persistent_buf_release(s_view.cell, &g_cli_view_cells);
    s_view.cell = NULL;
}
