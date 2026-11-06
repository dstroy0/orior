// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms_test.c: the Comms pane's check through its table's hooks: the Comms tool's table stands in the panel at the
// work's foot; no parts and no channels draw two empty groups; two parts registered and a message sent list the Parts
// group with 2, the Channels group with 1 and the rows by name; the part refused draws its dot in the error's color and
// the other in signal; a press on a channel opens its messages under it, the oldest first, and a second closes them;
// the head's choices change the view the table hands its rows to and Channels returns; the deadline is a second on
// while in sight and none while not, the step answering 1 only as the layer's stamp moves; and the eight Comms commands
// run by name, Copy as curl filling the clipboard's text from a view of the check's own file of requests
#include "../../../../../ui/cli/src/comms/comms.h"
#include "../../../../../ui/cli/src/editor/view.h"
#include "../../../../../ui/cli/src/requests/requests.h"
#include "../../../../../ui/cli/src/view/parts/comms.h"
#include "../cli/cli_check.h"

#define COMMS_FOOT 260

// a part `name` registered listening on and sending on `grant`, its key into `key`
static void part_registered(const char *name, const char *grant, char *key)
{
    const char *const grants[1] = {grant};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    cli_check(EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = name, .listen = grants, .listens = 1u,
                         .send_grant = grants, .send_grants = 1u, .key_out = key, .err_msg = &err_msg),
              name);
}

// `body` sent on `channel` as the part of `key`: whether it went
static int sent(const char *key, const char *channel, const char *body)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    unsigned long long delivered = 0ULL;
    return EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = key, .channel = channel,
                      .body = body, .size = cellul.len(&(CatenaFinitaCfg){.src = body, .cap = 256ULL}),
                      .delivered = &delivered, .err_msg = &err_msg);
}

// the row `at` of the Channels view as the CLI lists it, a row of no kind where there is none
static CliCommsRow row_at(unsigned int at)
{
    CliCommsRow row = {0};
    (void)EMBED_CALL(cli_comms_pane.row, CliCommsPaneCfg, .at = at, .row = &row);
    return row;
}

// whether `length` bytes of `text` are `required`
static int same(const char *text, unsigned long long length, const char *required)
{
    const unsigned long long required_length = cellul.len(&(CatenaFinitaCfg){.src = required, .cap = 256ULL});
    return (length == required_length) &&
           ((length == 0ULL) || (memor.cmp(&(MemoriaCfg){.src = text, .other = required, .bytes = length}) == 0));
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why))
    {
        (void)mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &frame, .height = COMMS_FOOT, .away = 0);
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    GuiPane pane;
    EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &pane);

    const GuiPaneTable *const table = &gui_comms_table;
    cli_check((EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = "comms") == table) && table->foot,
              "the Comms tool has the table, a foot table");
    EMBED_CALL(gui_pane.read, GuiPaneCfg, .table = table, .event = GUI_PANE_SHOWN, .root = NULL);
    cli_check((table->rows(&(GuiPaneCfg){.table = table}) == 2u) && (row_at(0u).kind == CLI_COMMS_ROW_GROUP) &&
                  same(row_at(0u).name, row_at(0u).name_length, "Parts") &&
                  same(row_at(0u).status, row_at(0u).status_length, "0") &&
                  same(row_at(1u).name, row_at(1u).name_length, "Channels"),
              "no parts and no channels draw two empty groups");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Comms", .tree = "");

    char zeta[CLI_COMMS_KEY];
    char alpha[CLI_COMMS_KEY];
    part_registered("zeta", "demo/*", zeta);
    part_registered("alpha", "demo/*", alpha);
    cli_check(sent(alpha, "demo/one", "{\"n\":1}") && !sent(zeta, "other/x", "{}"),
              "one part sends and the other is refused");
    cli_check(EMBED_CALL(cli_comms_pane.step, CliCommsPaneCfg, .shown = 1, .now_ms = 1000ULL),
              "the step lists the layer again as its stamp moved");
    cli_check(!EMBED_CALL(cli_comms_pane.step, CliCommsPaneCfg, .shown = 1, .now_ms = 1500ULL),
              "the step lists nothing again while the stamp stands");
    cli_check(EMBED_CALL(cli_comms_pane.deadline_ms, CliCommsPaneCfg, .shown = 1) == 2500ULL,
              "the deadline is a second past the last look while in sight");
    cli_check(EMBED_CALL(cli_comms_pane.deadline_ms, CliCommsPaneCfg, .shown = 0) == 0ULL,
              "there is no deadline while the pane is out of sight");
    cli_check((table->rows(&(GuiPaneCfg){.table = table}) == 6u) &&
                  same(row_at(0u).status, row_at(0u).status_length, "2") &&
                  same(row_at(1u).name, row_at(1u).name_length, "alpha") &&
                  same(row_at(2u).name, row_at(2u).name_length, "zeta") &&
                  same(row_at(3u).status, row_at(3u).status_length, "2") &&
                  same(row_at(4u).name, row_at(4u).name_length, "demo/one"),
              "the parts and the channels stand under their groups by name");

    // the dots: an opaque ground under the pane, and the pixel at each part's dot read after the draw
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster,
               .rect = (GuiRect){0, 0, 1440, 920}, .color = 0xFF000000u);
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Comms", .tree = "");
    const GuiRect alpha_rect =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 1u);
    const GuiRect zeta_rect =
        EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = table, .frame = &frame, .pane_read = &pane, .row = 2u);
    // the dots stand inside the raster
    const unsigned int dot_x = (unsigned int)(alpha_rect.x + GUI_PANE_PAD_LEFT + (GUI_COMMS_DOT / 2));
    const GuiColor alpha_dot =
        raster.pixels[((unsigned int)(alpha_rect.y + (alpha_rect.height / 2)) * raster.width) + dot_x];
    const GuiColor zeta_dot = raster.pixels[((unsigned int)(zeta_rect.y + (zeta_rect.height / 2)) * raster.width) + dot_x];
    // the dots are drawn faded by the panel's share over the ground, and each is told by which of its two colors it
    // stands nearer to
    const int alpha_red = (int)((alpha_dot >> 16u) & 0xFFu);
    const int zeta_red = (int)((zeta_dot >> 16u) & 0xFFu);
    const int signal_red = (int)((palette->signal >> 16u) & 0xFFu);
    const int error_red = (int)((palette->error >> 16u) & 0xFFu);
    cli_check((alpha_dot != 0xFF000000u) && (zeta_dot != 0xFF000000u) && (alpha_dot != zeta_dot) &&
                  (((error_red - signal_red) * (zeta_red - alpha_red)) > 0),
              "the part refused draws its dot in the error's color and the other in signal");

    GuiPaneOpen open = {NULL, {0ULL, 0ULL}, 0};
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 4u, .open_out = &open) &&
                  (table->rows(&(GuiPaneCfg){.table = table}) == 7u) &&
                  (row_at(5u).kind == CLI_COMMS_ROW_MESSAGE) &&
                  same(row_at(5u).name, row_at(5u).name_length, "alpha") &&
                  same(row_at(5u).status, row_at(5u).status_length, "{\"n\":1}") &&
                  (table->current(&(GuiPaneCfg){.table = table}) == 4),
              "a press on a channel opens its stored messages under it, the channel current");
    cli_check(sent(alpha, "demo/one", "{\"n\":2}") &&
                  EMBED_CALL(cli_comms_pane.step, CliCommsPaneCfg, .shown = 1, .now_ms = 3000ULL) &&
                  same(row_at(6u).status, row_at(6u).status_length, "{\"n\":2}"),
              "a message sent after stands last");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Comms", .tree = "");
    cli_check(!EMBED_CALL(gui_pane.press, GuiPaneCfg, .table = table, .row = 4u, .open_out = &open) &&
                  (table->rows(&(GuiPaneCfg){.table = table}) == 6u),
              "a second press closes them");

    // the head: the choices' row at the head's top, Requests the second quarter and Channels the first
    const int head_y = frame.terminal.y + GUI_PANE_SIDE_HEAD + GUI_PANE_LINE + (GUI_PANE_CHOICE / 2);
    const int quarter = (frame.terminal.width - 1) / 4;
    const unsigned int requests_hit = EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane,
                                                 .frame = &frame, .x = frame.terminal.x + quarter + 20, .y = head_y);
    cli_check(requests_hit == CLI_COMMS_CHOOSE + CLI_COMMS_VIEW_REQUESTS, "a press on the second choice is Requests");
    EMBED_CALL(gui_pane.action, GuiPaneCfg, .table = table, .pane = &pane, .which = requests_hit);
    cli_check((EMBED_CALL(cli_comms_pane.view, CliCommsPaneCfg, .at = 0u) == CLI_COMMS_VIEW_REQUESTS) &&
                  (table->rows(&(GuiPaneCfg){.table = table}) ==
                   EMBED_CALL(cli_requests_pane.rows, CliRequestsPaneCfg, .at = 0u)),
              "Requests chosen hands the rows to the Requests view");
    EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame, .raster = &raster,
               .glyphs = &glyphs, .palette = palette, .title = "Comms", .tree = "");
    const unsigned int network_hit = EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane,
                                                .frame = &frame, .x = frame.terminal.x + (2 * quarter) + 20,
                                                .y = head_y);
    EMBED_CALL(gui_pane.action, GuiPaneCfg, .table = table, .pane = &pane, .which = network_hit);
    cli_check((EMBED_CALL(cli_comms_pane.view, CliCommsPaneCfg, .at = 0u) == CLI_COMMS_VIEW_NETWORK) &&
                  (table->rows(&(GuiPaneCfg){.table = table}) == 0u),
              "Network chosen is the view shown");
    const unsigned int channels_hit = EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane,
                                                 .frame = &frame, .x = frame.terminal.x + 20, .y = head_y);
    EMBED_CALL(gui_pane.action, GuiPaneCfg, .table = table, .pane = &pane, .which = channels_hit);
    cli_check((EMBED_CALL(cli_comms_pane.view, CliCommsPaneCfg, .at = 0u) == CLI_COMMS_VIEW_CHANNELS) &&
                  (table->rows(&(GuiPaneCfg){.table = table}) == 6u),
              "Channels returns with its rows");
    const int again_y = head_y + GUI_PANE_CHOICE;
    cli_check(EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = &pane, .frame = &frame,
                         .x = frame.terminal.x + frame.terminal.width - 20, .y = again_y) == CLI_COMMS_READ,
              "a press at the right under the choices reads the layer again");

    // the commands: the eight run by name, another not; Copy as curl on a view of a file of requests fills the
    // clipboard's text with its curl line
    static char s_root[CLI_PATH_MAX_LEN];
    cli_check_folder("comms_tree", s_root);
    char calls[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "calls.http", .joined = calls);
    static const char s_calls[] = "### make\nGET http://h/a\n";
    cli_check(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = calls,
                         .bytes = s_calls, .size = sizeof(s_calls) - 1ULL),
              "the file of requests is written");
    why.at = 0ULL;
    cli_check(cli_view_file_open(s_root, "calls.http", 40u, 120u, NULL, &why) >= 0, "a view opens on the file");
    static const char *const s_names[8] = {"send-request", "copy-as-curl", "private-environment", "network",
                                           "network-off",  "report",       "remote-rejoin",       "remote-copy-address"};
    int every = 1;
    char clipboard_text[4096];
    char open_text[CLI_PATH_MAX_LEN];
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        VerbaLinea clipboard = {clipboard_text, sizeof(clipboard_text), 0ULL};
        VerbaLinea open_out = {open_text, sizeof(open_text), 0ULL};
        int shown = 0;
        every = every && EMBED_CALL(cli_comms_command.run, CliCommsCommandCfg, .name = s_names[at],
                                    .clipboard = &clipboard, .open_out = &open_out, .shown_out = &shown);
    }
    VerbaLinea clipboard = {clipboard_text, sizeof(clipboard_text), 0ULL};
    VerbaLinea open_out = {open_text, sizeof(open_text), 0ULL};
    int shown = 0;
    cli_check(every && !EMBED_CALL(cli_comms_command.run, CliCommsCommandCfg, .name = "save", .clipboard = &clipboard,
                                   .open_out = &open_out, .shown_out = &shown),
              "the eight Comms commands run by name and no other");
    cli_check(EMBED_CALL(cli_comms_command.run, CliCommsCommandCfg, .name = "copy-as-curl", .clipboard = &clipboard,
                         .open_out = &open_out, .shown_out = &shown) &&
                  (mmgr_line_finish(&clipboard) != 0ULL) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = clipboard_text, .cap = sizeof(clipboard_text),
                                                  .other = "'http://h/a'", .other_cap = 16ULL}) != NULL),
              "Copy as curl fills the clipboard's text with the request's curl line");
    cli_check(EMBED_CALL(cli_comms_command.run, CliCommsCommandCfg, .name = "send-request", .clipboard = &clipboard,
                         .open_out = &open_out, .shown_out = &shown) &&
                  shown && (EMBED_CALL(cli_comms_pane.view, CliCommsPaneCfg, .at = 0u) == CLI_COMMS_VIEW_REQUESTS),
              "Send Request shows the Requests view");
    const unsigned long long until = cli_clock_milliseconds() + 10000ULL;
    while (EMBED_CALL(cli_requests_pane.sending, CliRequestsPaneCfg, .at = 0u) && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_comms_pane.step, CliCommsPaneCfg, .shown = 0);
        cli_thread_yield();
    }
    EMBED_CALL(cli_comms_pane.choose, CliCommsPaneCfg, .view = CLI_COMMS_VIEW_CHANNELS);

    g_cli_guard->persistent_buf_release(raster.pixels, g_gui_raster_owner);
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
