// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// thread.c: the view's thread and what joins it to the window's. The system's thread writes the events' two rings and
// reads the orders' ring; the view's thread reads the first and writes the second. A raster ordered shown stands
// untouched until its event comes back, one at a time on each surface, which paces the frames by the system's own
// showing. A thread that finds nothing to do tests its rings again after reading the wake's generation and sleeps on
// that generation, so a write after the test ends the sleep.
#include "thread.h"

#include "../../cli/src/command_line/cli_internal.h"
#include "../../cli/src/command_line/command_line.h"
#include "../../cli/src/command_line/rings.h"
#include "window.h"

EMBED_STATIC_ASSERT(sizeof(CliViewEvent) <= CLI_VIEW_EVENT_SEGMENT,
                    "a CliViewEvent fits one segment of the event ring");
EMBED_STATIC_ASSERT(sizeof(CliViewOrder) <= CLI_VIEW_ORDER_SEGMENT,
                    "a CliViewOrder fits one segment of the order ring");

// A surface's maps on their way over: the ticket ring the view's thread pushes a map on each frame and the system's
// thread pops them off in order, its slots a run of the view's part of the rings block, the stamp of the map granted
// and not yet pushed, the next ticket the view's thread takes and the next the system's thread pops, the buffers a
// ticket's map is laid in, one a slot, and the map the system's thread keeps, the last it popped
typedef struct
{
    EmbedTicketRing ring;
    embed_u64 granted;
    embed_u32 ticket;
    embed_u32 taken;
    CliViewHits *buffer;
    CliViewHits *kept;
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
    EmbedTurnstiles hits_turnstiles;
    CliViewHitsTransfer hits[CLI_VIEW_SURFACES];
    embed_u8 *cell;
    CliWake wake;
    CliThread thread;
    CliViewWoken woken;
    void *context;
    EMBED_ALIGN(64) embed_atomic_u32 ending;
    EMBED_ALIGN(64) embed_atomic_u32 running;
    CliWake ended;
    int presenting_main;
    int presenting_preferences;
    int presenting_dialog;
    int dialog_opened;
    int frame_ordered;
    // a frame of the changes alone ordered shown on the raster shown, which flips nothing as it is shown
    int damage_ordered;
    unsigned long long presented_ms;
    unsigned int frame_ms;
    unsigned long long tick_at_ms;
    int preferences_opened;
    int close_ordered;
    int icons_ordered;
    // the pointer's shape last ordered of the system, GUI_CURSOR_ARROW at the start
    unsigned int cursor_ordered;
    GuiRaster icon_small;
    GuiRaster icon_large;
} CliView;

static CliView s_view;

// The frames' telemetry since its last line, on the view's thread alone: the whole frames drawn, the microseconds each
// draw took and the most of them, the microseconds from a frame's order to its showing and the most of them, and
// the passes of the view's loop that a moving part paced. Then the microseconds the events took and the most of one
// pass's events, and the most microseconds between two frames shown, a gap of VIEW_GAP_IDLE_US or more left out as a
// rest. Then the timed waits each of the window's deadlines set
typedef struct
{
    unsigned long long woken_by[GUI_WINDOW_WAKES];
    unsigned long long frames;
    unsigned long long moving_passes;
    unsigned long long draw_us;
    unsigned long long draw_max_us;
    unsigned long long shown_us;
    unsigned long long shown_max_us;
    unsigned long long ordered_at_us;
    unsigned long long line_at_us;
    unsigned long long events_us;
    unsigned long long events_max_us;
    unsigned long long gap_max_us;
    unsigned long long presented_at_us;
} ViewFrameTelemetry;

// the gap between two frames shown that counts as a rest, not a late frame
#define VIEW_GAP_IDLE_US 250000ULL

static ViewFrameTelemetry s_frame_telemetry;

// the telemetry's line every GUI_MEMORY_MS to the error stream, as the cells' log is, its counts then cleared
static void view_frame_telemetry_log(unsigned long long now_us)
{
    ViewFrameTelemetry *const told = &s_frame_telemetry;
    if (now_us - told->line_at_us < GUI_MEMORY_MS * 1000ULL)
    {
        return;
    }
    char wakes[512];
    VerbaLinea line = {wakes, sizeof(wakes), 0u, NULL, NULL, 0u};
    if (told->frames != 0ULL)
    {
        const unsigned long long counts[9] = {told->frames,
                                              told->draw_us / told->frames,
                                              told->draw_max_us,
                                              told->shown_us / told->frames,
                                              told->shown_max_us,
                                              told->moving_passes,
                                              told->events_us / told->frames,
                                              told->events_max_us,
                                              told->gap_max_us};
        static const char *const s_names[9] = {"frames ",  " draw ",   " ", " shown ", " ",
                                               " moving ", " events ", " ", " gap "};
        for (unsigned int at = 0u; at < 9u; at += 1u)
        {
            mmgr_line_put(&line, s_names[at]);
            mmgr_line_decimal(&line, counts[at], 1u);
        }
        mmgr_line_ch(&line, '\n');
        EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = wakes,
                   .length = mmgr_line_finish(&line));
        line.at = 0ULL;
    }
    // the deadlines that set waits since the last line, each with its count
    for (unsigned int at = 0u; at < GUI_WINDOW_WAKES; at += 1u)
    {
        if (told->woken_by[at] != 0ULL)
        {
            mmgr_line_ch(&line, ' ');
            mmgr_line_put(&line, g_gui_window_wake_names[at]);
            mmgr_line_ch(&line, ' ');
            mmgr_line_decimal(&line, told->woken_by[at], 1u);
        }
        told->woken_by[at] = 0ULL;
    }
    if ((line.at != 0u) && mmgr_line_ok(&line))
    {
        char woken[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea named = {woken, sizeof(woken), 0u, NULL, NULL, 0u};
        mmgr_line_put(&named, "wakes");
        mmgr_line_put_n(&named, wakes, line.at);
        mmgr_line_ch(&named, '\n');
        EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = woken,
                   .length = mmgr_line_finish(&named));
    }
    told->events_us = 0ULL;
    told->events_max_us = 0ULL;
    told->gap_max_us = 0ULL;
    told->frames = 0ULL;
    told->moving_passes = 0ULL;
    told->draw_us = 0ULL;
    told->draw_max_us = 0ULL;
    told->shown_us = 0ULL;
    told->shown_max_us = 0ULL;
    told->line_at_us = now_us;
}

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

// On the system's thread: every map pushed popped in the order of its ticket, each copied whole into the kept map,
// its rectangles after it, before its slot goes back to the view's thread; then the point read against the kept map
unsigned int cli_view_hit(unsigned int surface, int x, int y)
{
    CliViewHitsTransfer *const transfer = &s_view.hits[surface];
    // Explicit cast carries the transfer's address as the pass's entry
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)transfer;
    embed_u64 stamp = 0u;
    while (cli_ticket_pushed(&transfer->ring, transfer->taken, &stamp))
    {
        void *job = NULL;
        embed_u64 seen = 0u;
        if (!embed_ticket_ring_popped(&s_view.hits_turnstiles, address, &transfer->ring, stamp, &job, &seen))
        {
            break;
        }
        const CliViewHits *const map = job;
        GuiRect *const rects = transfer->kept->client;
        memor.cpy(&(MemoriaCfg){.dst = rects, .src = map->client, .bytes = GUI_WINDOW_HIT_CLIENTS * sizeof(GuiRect)});
        *transfer->kept = *map;
        transfer->kept->client = rects;
        (void)embed_ticket_ring_released(&s_view.hits_turnstiles, &transfer->ring, stamp, &seen);
        transfer->taken += 1u;
    }
    return cli_view_hit_of(transfer->kept, x, y);
}

// On the view's thread: the buffer of `surface`'s next map, its ticket's slot granted: NULL where every slot is in
// flight, and the frame goes with no new map
static CliViewHits *view_hits_next(unsigned int surface)
{
    CliViewHitsTransfer *const transfer = &s_view.hits[surface];
    const embed_u64 stamp = embed_ticket_stamp(cli_ticket_epoch(), transfer->ticket);
    embed_u64 seen = 0u;
    // Explicit cast carries the transfer's address as the pass's entry
    if (!embed_ticket_ring_granted(&s_view.hits_turnstiles, (embed_atomic_uptr_value)(embed_word)transfer,
                                   &transfer->ring, stamp, &seen))
    {
        return NULL;
    }
    transfer->granted = stamp;
    return &transfer->buffer[transfer->ticket & (CLI_VIEW_HITS_SLOTS - 1u)];
}

// the map laid in the granted buffer pushed over to the system's thread
static void view_hits_publish(unsigned int surface)
{
    CliViewHitsTransfer *const transfer = &s_view.hits[surface];
    embed_u64 seen = 0u;
    // Explicit cast carries the transfer's address as the pass's entry
    (void)embed_ticket_ring_pushed(&s_view.hits_turnstiles, (embed_atomic_uptr_value)(embed_word)transfer,
                                   &transfer->ring, transfer->granted,
                                   &transfer->buffer[transfer->ticket & (CLI_VIEW_HITS_SLOTS - 1u)], &seen);
    transfer->ticket += 1u;
}

void cli_view_event(const CliViewEvent *event)
{
    const int keyed = (event->type == CLI_VIEW_EVENT_INPUT) && (event->input.type == GUI_INPUT_KEY);
    const int typed = (event->type == CLI_VIEW_EVENT_INPUT) && (event->input.type == GUI_INPUT_TEXT);
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
    embed_word at = 0u;
    // the bytes a name or a typing takes are far inside what a embed_word counts
    if ((length < CLI_VIEW_EVENT_TEXT) &&
        (anularis.vacant(&(AnularisCfg){.ring = &s_view.event_bytes_ring}) >= (embed_word)length) &&
        anularis.seg_next(&(AnularisCfg){.ring = &s_view.event_ring, .out_index = &at}))
    {
        (void)anularis.put(&(AnularisCfg){.ring = &s_view.event_bytes_ring,
                                          .src = (const embed_u8 *)(const void *)bytes,
                                          .bytes = (embed_word)length});
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
    embed_word at = 0u;
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
    embed_word at = 0u;
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
                   .size = GUI_ICON_SMALL, .segment = 0u) &&
        EMBED_CALL(cli_window_icon.icon, GuiWindowIconCfg, .window = &s_view.window, .icon = &s_view.icon_large,
                   .size = GUI_ICON_LARGE, .segment = 1u))
    {
        const CliViewOrder order = {
            .type = CLI_VIEW_ORDER_ICON, .raster = s_view.icon_small, .raster_large = s_view.icon_large};
        (void)view_order_write(&order);
    }
}

// the pointer's shape ordered of the system where the window's differs from the one last ordered: the four-way arrow
// while the autoscroll runs, the arrow otherwise
static void view_cursor_order(void)
{
    const unsigned int wanted = s_view.window.autoscroll ? GUI_CURSOR_SCROLL : GUI_CURSOR_ARROW;
    if (wanted == s_view.cursor_ordered)
    {
        return;
    }
    const CliViewOrder order = {.type = CLI_VIEW_ORDER_CURSOR, .act = wanted};
    if (view_order_write(&order))
    {
        s_view.cursor_ordered = wanted;
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
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = window, .input = input);
    view_cursor_order();
    if (released && (pressed != GUI_HIT_CLIENT) &&
        (EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = window, .input = input) == pressed))
    {
        const unsigned int other_act = (pressed == GUI_HIT_MAXIMIZE) ? GUI_ACT_MAXIMIZE : GUI_ACT_CLOSE;
        const CliViewOrder order = {.type = CLI_VIEW_ORDER_ACT,
                                    .act = (pressed == GUI_HIT_MINIMIZE) ? GUI_ACT_MINIMIZE : other_act};
        (void)view_order_write(&order);
    }
}

// a raster's showing come back: the framebuffer a whole frame stood in shown, the two flipped
static void view_presented(unsigned int surface)
{
    if (surface == CLI_VIEW_PREFERENCES)
    {
        s_view.presenting_preferences = 0;
        return;
    }
    if (surface == CLI_VIEW_DIALOG)
    {
        s_view.presenting_dialog = 0;
        return;
    }
    s_view.presenting_main = 0;
    s_view.presented_ms = cli_clock_milliseconds();
    // a frame of changes alone was drawn on the raster shown, so it counts as a frame shown and flips nothing
    if (s_view.damage_ordered)
    {
        const unsigned long long presented_us = cli_clock_microseconds();
        const unsigned long long gap_us = presented_us - s_frame_telemetry.presented_at_us;
        if ((gap_us < VIEW_GAP_IDLE_US) && (gap_us > s_frame_telemetry.gap_max_us))
        {
            s_frame_telemetry.gap_max_us = gap_us;
        }
        s_frame_telemetry.presented_at_us = presented_us;
        s_view.damage_ordered = 0;
    }
    if (s_view.frame_ordered)
    {
        const unsigned long long presented_us = cli_clock_microseconds();
        const unsigned long long gap_us = presented_us - s_frame_telemetry.presented_at_us;
        if ((gap_us < VIEW_GAP_IDLE_US) && (gap_us > s_frame_telemetry.gap_max_us))
        {
            s_frame_telemetry.gap_max_us = gap_us;
        }
        s_frame_telemetry.presented_at_us = presented_us;
        const unsigned long long shown_us = presented_us - s_frame_telemetry.ordered_at_us;
        s_frame_telemetry.shown_us += shown_us;
        s_frame_telemetry.shown_max_us =
            (shown_us > s_frame_telemetry.shown_max_us) ? shown_us : s_frame_telemetry.shown_max_us;
        s_view.frame_ordered = 0;
        EMBED_CALL(gui_window_table.flip, GuiWindowCfg, .window = &s_view.window);
    }
}

// one event answered
static void view_event(const CliViewEvent *event)
{
    GuiWindow *const window = &s_view.window;
    switch (event->type)
    {
    case CLI_VIEW_EVENT_SIZED:
        if (event->surface == CLI_VIEW_PREFERENCES)
        {
            EMBED_CALL(gui_window_width.preferences_resize, GuiWindowWidthCfg, .window = window, .width = event->width,
                       .height = event->height, .system_scale = event->scale);
        }
        else if (event->surface == CLI_VIEW_DIALOG)
        {
            EMBED_CALL(gui_window_dialog_width.resize, GuiWindowWidthCfg, .window = window, .width = event->width,
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
        // While the dialog shows, it is modal: the window that owns it takes no input but the focus coming and going
        if (window->dialog.displayed && (event->surface == window->dialog_owner) &&
            (event->input.type != GUI_INPUT_FOCUS) && (event->input.type != GUI_INPUT_BLUR))
        {
            break;
        }
        if (event->surface == CLI_VIEW_DIALOG)
        {
            EMBED_CALL(gui_window_dialog_input_type.input, GuiWindowInputTypeCfg, .window = window,
                       .input = &event->input);
        }
        else if (event->surface == CLI_VIEW_PREFERENCES)
        {
            EMBED_CALL(gui_window_input_type.preferences_input, GuiWindowInputTypeCfg, .window = window,
                       .input = &event->input);
        }
        else
        {
            view_input(&event->input);
        }
        break;
    case CLI_VIEW_EVENT_CLOSING:
        if (event->surface == CLI_VIEW_DIALOG)
        {
            EMBED_CALL(gui_window_dialog_table.close, GuiWindowCfg, .window = window);
        }
        else if (window->preferences.displayed)
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
        if (event->surface == CLI_VIEW_DIALOG)
        {
            EMBED_CALL(gui_window_dialog_table.close, GuiWindowCfg, .window = window);
            s_view.dialog_opened = 0;
            break;
        }
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
    embed_word at = 0u;
    while (anularis.seg_front(&(AnularisCfg){.ring = &s_view.event_ring, .out_index = &at}))
    {
        CliViewEvent event = *(const CliViewEvent *)(const void *)anularis.seg_at(
            &(AnularisCfg){.ring = &s_view.event_ring, .index = at});
        anularis.seg_release(&(AnularisCfg){.ring = &s_view.event_ring});
        // the event's writer holds what it wrote under the text's bound
        const embed_word length = anularis.read(&(AnularisCfg){
            .ring = &s_view.event_bytes_ring, .dst = (embed_u8 *)(void *)bytes, .bytes = (embed_word)event.input.size});
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
        CliViewOrder order = {.type = CLI_VIEW_ORDER_PREFERENCES_OPEN, .surface = CLI_VIEW_PREFERENCES};
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
        const CliViewOrder order = {.type = CLI_VIEW_ORDER_PREFERENCES_CLOSE, .surface = CLI_VIEW_PREFERENCES};
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
            CliViewHits *const map = view_hits_next(CLI_VIEW_PREFERENCES);
            if (map != NULL)
            {
                EMBED_CALL(gui_window_table.preferences_hits, GuiWindowHitsCfg, .window = window, .hits = map);
                view_hits_publish(CLI_VIEW_PREFERENCES);
            }
            const CliViewOrder order = {
                .type = CLI_VIEW_ORDER_PRESENT, .surface = CLI_VIEW_PREFERENCES, .raster = window->preferences_raster};
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

// The dialog's window opened or closed as the dialog last stood, modal over the window that owns it. Its frame is
// drawn and ordered shown where one is owed and none is on its way
static void view_dialog_follow(void)
{
    GuiWindow *const window = &s_view.window;
    if (window->dialog.displayed && !s_view.dialog_opened)
    {
        CliViewOrder order = {
            .type = CLI_VIEW_ORDER_DIALOG_OPEN, .surface = CLI_VIEW_DIALOG, .owner = window->dialog_owner};
        EMBED_CALL(gui_window_dialog_wanting.want, GuiWindowDialogWantCfg, .window = window,
                   .width_pointer = &order.width, .height_pointer = &order.height);
        if (view_order_write(&order))
        {
            s_view.dialog_opened = 1;
        }
    }
    else if (!window->dialog.displayed && s_view.dialog_opened)
    {
        const CliViewOrder order = {
            .type = CLI_VIEW_ORDER_DIALOG_CLOSE, .surface = CLI_VIEW_DIALOG, .owner = window->dialog_owner};
        if (view_order_write(&order))
        {
            s_view.dialog_opened = 0;
            s_view.presenting_dialog = 0;
        }
    }
    if (s_view.dialog_opened && window->dialog_pending && !s_view.presenting_dialog)
    {
        EMBED_CALL(gui_window_dialog_table.draw, GuiWindowCfg, .window = window);
        if (window->dialog_raster.pixels != NULL)
        {
            CliViewHits *const map = view_hits_next(CLI_VIEW_DIALOG);
            if (map != NULL)
            {
                EMBED_CALL(gui_window_dialog_hits_table.hits, GuiWindowHitsCfg, .window = window, .hits = map);
                view_hits_publish(CLI_VIEW_DIALOG);
            }
            const CliViewOrder order = {
                .type = CLI_VIEW_ORDER_PRESENT, .surface = CLI_VIEW_DIALOG, .raster = window->dialog_raster};
            if (view_order_write(&order))
            {
                s_view.presenting_dialog = 1;
            }
            else
            {
                window->dialog_pending = 1;
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
    const CliViewOrder order = {.type = CLI_VIEW_ORDER_PRESENT_PART,
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
    // an autoscroll the frame ended, its editor gone, gives the pointer its arrow back
    view_cursor_order();
    if (window->pending)
    {
        const unsigned long long draw_from_us = cli_clock_microseconds();
        EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = window);
        const unsigned long long drawn_us = cli_clock_microseconds();
        s_frame_telemetry.frames += 1ULL;
        s_frame_telemetry.draw_us += drawn_us - draw_from_us;
        s_frame_telemetry.draw_max_us = ((drawn_us - draw_from_us) > s_frame_telemetry.draw_max_us)
                                            ? (drawn_us - draw_from_us)
                                            : s_frame_telemetry.draw_max_us;
        s_frame_telemetry.ordered_at_us = drawn_us;
        view_frame_telemetry_log(drawn_us);
        if (window->raster->pixels == NULL)
        {
            return;
        }
        CliViewHits *const map = view_hits_next(CLI_VIEW_MAIN);
        if (map != NULL)
        {
            EMBED_CALL(gui_window_table.hits, GuiWindowHitsCfg, .window = window, .hits = map);
            view_hits_publish(CLI_VIEW_MAIN);
        }
        const CliViewOrder order = {
            .type = CLI_VIEW_ORDER_PRESENT, .surface = CLI_VIEW_MAIN, .raster = *window->raster};
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
    // the parts of the frame that changed drawn again alone, the caret and the circle with them where they stand there
    if ((window->damage.width > 0) && (window->damage.height > 0))
    {
        const unsigned long long draw_from_us = cli_clock_microseconds();
        const GuiRect part = EMBED_CALL(gui_window_table.damage_draw, GuiWindowCfg, .window = window);
        const unsigned long long drawn_us = cli_clock_microseconds();
        s_frame_telemetry.frames += 1ULL;
        s_frame_telemetry.draw_us += drawn_us - draw_from_us;
        s_frame_telemetry.draw_max_us = ((drawn_us - draw_from_us) > s_frame_telemetry.draw_max_us)
                                            ? (drawn_us - draw_from_us)
                                            : s_frame_telemetry.draw_max_us;
        s_frame_telemetry.ordered_at_us = drawn_us;
        view_frame_telemetry_log(drawn_us);
        view_part_order(part);
        s_view.damage_ordered = s_view.presenting_main;
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
        const unsigned long long events_from_us = cli_clock_microseconds();
        view_events_take();
        EMBED_CALL(gui_window_table.step, GuiWindowCfg, .window = window);
        const unsigned long long events_took_us = cli_clock_microseconds() - events_from_us;
        s_frame_telemetry.events_us += events_took_us;
        s_frame_telemetry.events_max_us =
            (events_took_us > s_frame_telemetry.events_max_us) ? events_took_us : s_frame_telemetry.events_max_us;
        const unsigned long long now_ms = cli_clock_milliseconds();
        if ((s_view.tick_at_ms != 0ULL) && (now_ms >= s_view.tick_at_ms))
        {
            s_view.tick_at_ms = 0ULL;
            EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = window);
        }
        view_preferences_follow();
        view_dialog_follow();
        // a command asked the window to close
        if (window->closing && !s_view.close_ordered)
        {
            const CliViewOrder order = {.type = CLI_VIEW_ORDER_ACT, .act = GUI_ACT_CLOSE};
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
        // While a part of the window moves, or the editor's scroll moves, the frames follow the showing: the next frame
        // is drawn as the last one is shown, and the device's flip queue holds each frame to the monitor's refresh. A
        // timed wait sleeps to the system timer's step and misses refreshes, so a moving part does not wait on a timer
        if (window->autohide_moving || window->scroll_moving || window->buttons_moving)
        {
            s_view.tick_at_ms = cli_clock_milliseconds();
            s_frame_telemetry.moving_passes += 1ULL;
            if (s_view.presenting_main)
            {
                EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &s_view.wake, .seen = seen);
            }
            continue;
        }
        const unsigned long long since_ms = cli_clock_milliseconds() - s_view.presented_ms;
        const unsigned long long refresh_ms = (since_ms < s_view.frame_ms) ? s_view.frame_ms - since_ms : 0ULL;
        const unsigned long long wait_ms = (after > refresh_ms) ? after : refresh_ms;
        const unsigned int source = gui_window_timeout_source();
        if (source < GUI_WINDOW_WAKES)
        {
            s_frame_telemetry.woken_by[source] += 1ULL;
        }
        s_view.tick_at_ms = cli_clock_milliseconds() + wait_ms;
        (void)EMBED_CALL(cli_wake_internal.wake_wait_seen_for, CliWakeCtx, .wake = &s_view.wake, .seen = seen,
                         .milliseconds_u64 = wait_ms);
    }
    s_view.icon_small.pixels = NULL;
    s_view.icon_small.width = 0u;
    s_view.icon_small.height = 0u;
    s_view.icon_large.pixels = NULL;
    s_view.icon_large.width = 0u;
    s_view.icon_large.height = 0u;
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
    const embed_word event_bytes = (embed_word)CLI_VIEW_EVENTS * CLI_VIEW_EVENT_SEGMENT;
    const embed_word order_bytes = (embed_word)CLI_VIEW_ORDERS * CLI_VIEW_ORDER_SEGMENT;
    const embed_word map_bytes = sizeof(CliViewHits) + (GUI_WINDOW_HIT_CLIENTS * sizeof(GuiRect));
    // a surface's maps: one a slot of its ring, and the one the system's thread keeps
    const embed_word maps_each = (embed_word)CLI_VIEW_HITS_SLOTS + 1u;
    const embed_word maps_bytes = (embed_word)CLI_VIEW_SURFACES * maps_each * map_bytes;
    // the rings and the maps are the view's part of the rings block
    s_view.cell = EMBED_CALL(cli_rings.part, CliRingsPartCfg, .part = CLI_RINGS_VIEW);
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
    embed_u8 *map = s_view.cell + event_bytes + CLI_VIEW_EVENT_BYTES + order_bytes;
    // the maps lie on a boundary of the rings' segments and their rectangles after them
    CliViewHits *const headers = (CliViewHits *)(void *)map;
    GuiRect *const rects = (GuiRect *)(void *)(headers + ((embed_word)CLI_VIEW_SURFACES * maps_each));
    // the map rings' slots after the rectangles, each surface's run of them zeros as the view begins
    EmbedTicketRingSlot *const slots =
        (EmbedTicketRingSlot *)(void *)(rects + ((embed_word)CLI_VIEW_SURFACES * maps_each * GUI_WINDOW_HIT_CLIENTS));
    memor.set(&(MemoriaCfg){.dst = slots,
                            .val = 0,
                            .bytes = (unsigned long long)CLI_VIEW_SURFACES * CLI_VIEW_HITS_SLOTS *
                                     sizeof(EmbedTicketRingSlot)});
    for (unsigned int surface = 0u; surface < CLI_VIEW_SURFACES; surface += 1u)
    {
        CliViewHitsTransfer *const transfer = &s_view.hits[surface];
        transfer->buffer = headers + (surface * maps_each);
        transfer->kept = transfer->buffer + CLI_VIEW_HITS_SLOTS;
        transfer->ticket = 0u;
        transfer->taken = 0u;
        embed_ticket_ring_begin(&transfer->ring, slots + (surface * CLI_VIEW_HITS_SLOTS), CLI_VIEW_HITS_SHIFT);
        for (unsigned int at = 0u; at < maps_each; at += 1u)
        {
            transfer->buffer[at].client = rects + ((((embed_word)surface * maps_each) + at) * GUI_WINDOW_HIT_CLIENTS);
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
    s_view.cell = NULL;
}
