// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// preview_test.c: the preview part's check on a tree of its own, a Markdown file open as a view: with the mode off the
// frame has no preview and nothing is drawn; beside, the items are drawn in the editor's right half and the editor keeps
// its left; alone, the preview fills the editor's rectangle; the wheel moves the top by whole rows and no further than
// the last item fills the rectangle; nothing is drawn outside the rectangle, the draw starting at the item the CLI
// finds for the top; a press on a link hands the CLI's answer on and a press elsewhere does nothing; the cursor's line
// draws its block's mark; and a scroll of the editor moves the preview's top with no scroll going back to the editor
#include "../../../../../ui/cli/src/editor/view.h"
#include "../../../../../ui/cli/src/view/parts/preview.h"
#include "../cli/cli_check.h"

static char s_root[CLI_PATH_MAX_LEN];

// the file `relative` of the tree written with `text`
static void file_written(const char *relative, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = relative, .joined = path);
    cli_check(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text,
                         .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = 1ULL << 20u})),
              relative);
}

// how many pixels of `raster` in the rectangle `rect` are not the empty pixel
static unsigned int drawn_in(const GuiRaster *raster, GuiRect rect)
{
    unsigned int count = 0u;
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            // the rectangle stands inside the raster
            count += (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != 0u) ? 1u : 0u;
        }
    }
    return count;
}

// the raster cleared and the preview drawn on it in `frame`
static void drawn(GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette, const GuiFrame *frame)
{
    memor.set(&(MemoriaCfg){.dst = raster->pixels, .val = 0, .bytes = (unsigned long long)raster->width * raster->height *
                                                                      sizeof(GuiColor)});
    EMBED_CALL(gui_preview.draw, GuiPreviewCfg, .raster = raster, .glyphs = glyphs, .palette = palette, .frame = frame);
}

// the preview stepped for the view `key` in `frame` at `now_ms`
static int stepped(const char *key, const GuiFrame *frame, unsigned long long now_ms)
{
    return EMBED_CALL(cli_preview.step, CliPreviewCfg, .key = key, .now_ms = now_ms, .width_px = frame->preview.width,
                      .height_px = frame->preview.height, .scale = frame->scale);
}

// the item reading `text` whole: its row, or the count where none does
static unsigned int run_reading(const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = 256ULL});
    const unsigned int count = EMBED_CALL(cli_preview_item.count, CliPreviewItemCfg, .at = 0u);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *read = NULL;
        const CliPreviewItem *const item =
            EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = at, .text_out = &read);
        if ((item->type == CLI_PREVIEW_ITEM_TEXT) && (item->length == length) &&
            (memor.cmp(&(MemoriaCfg){.src = read, .other = text, .bytes = length}) == 0))
        {
            return at;
        }
    }
    return count;
}

// the view `key`'s input `input` taken
static void view_input(const char *key, const CliViewInput *input)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    cli_check(EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = input, .err_msg = &err_msg),
              "the view takes the input");
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
    const GuiRect whole = frame.editor;
    // the editor's scrollbar takes its room from the rectangle the editor and the preview share
    const int bar = frame.editor_bar.width;
    const int shared = whole.width + bar;
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");

    cli_check_folder("preview_tree", s_root);
    char notes[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "notes", .joined = notes);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = notes);
    char text[8192];
    VerbaLinea written = {text, sizeof(text), 0ULL};
    mmgr_line_put(&written, "# Notes\n\nSee [next](other.md) for more.\n\n");
    for (unsigned int at = 0u; at < 80u; at += 1u)
    {
        mmgr_line_put(&written, "Paragraph ");
        mmgr_line_decimal(&written, at, 1u);
        mmgr_line_put(&written, " of the notes.\n\n");
    }
    (void)mmgr_line_finish(&written);
    file_written("notes/readme.md", text);
    why.at = 0ULL;
    const int place = cli_view_file_open(s_root, "notes/readme.md", 40u, 120u, NULL, &why);
    cli_check(place >= 0, "the notes open as a view");
    // the view's place is not negative, as checked
    const char *const key = (place >= 0) ? cli_view_key((unsigned int)place) : NULL;
    unsigned long long now_ms = 1000ULL;

    // off: no rectangle, nothing drawn
    EMBED_CALL(cli_preview.mode_set, CliPreviewCfg, .word = "off");
    EMBED_CALL(gui_frame_displayed.preview, GuiFrameDisplayedCfg, .frame = &frame,
               .displayed = (int)EMBED_CALL(cli_preview.mode_get, CliPreviewCfg, .now_ms = now_ms));
    (void)stepped(key, &frame, now_ms);
    drawn(&raster, &glyphs, palette, &frame);
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame.preview) &&
                  (drawn_in(&raster, whole) == 0u),
              "with the mode off the frame has no preview and nothing is drawn");

    // beside: the right half drawn, the left half the editor's
    EMBED_CALL(cli_preview.mode_set, CliPreviewCfg, .word = "beside");
    EMBED_CALL(gui_frame_displayed.preview, GuiFrameDisplayedCfg, .frame = &frame,
               .displayed = (int)EMBED_CALL(cli_preview.mode_get, CliPreviewCfg, .now_ms = now_ms));
    now_ms += CLI_PREVIEW_LOOK_MS;
    cli_check(stepped(key, &frame, now_ms), "the step lays the notes as the preview shows");
    cli_check(run_reading("Notes") < EMBED_CALL(cli_preview_item.count, CliPreviewItemCfg, .at = 0u),
              "the heading is among the items");
    drawn(&raster, &glyphs, palette, &frame);
    cli_check((frame.editor.width + bar == shared - (shared / 2)) && (frame.preview.width == shared / 2) &&
                  (frame.preview.x == frame.editor.x + frame.editor.width + bar),
              "beside, the preview stands in the editor's right half, the editor's scrollbar between them");
    cli_check((drawn_in(&raster, frame.preview) != 0u) && (drawn_in(&raster, frame.editor) == 0u),
              "the items are drawn in the right half and the left half is left to the text");
    const GuiRect over = {frame.preview.x, frame.preview.y - 1, frame.preview.width, 1};
    const GuiRect under = {frame.preview.x, frame.preview.y + frame.preview.height, frame.preview.width, 1};
    cli_check((drawn_in(&raster, over) == 0u) && (drawn_in(&raster, under) == 0u),
              "nothing is drawn above or below the rectangle");

    // alone: the whole rectangle
    EMBED_CALL(cli_preview.mode_set, CliPreviewCfg, .word = "alone");
    EMBED_CALL(gui_frame_displayed.preview, GuiFrameDisplayedCfg, .frame = &frame,
               .displayed = (int)EMBED_CALL(cli_preview.mode_get, CliPreviewCfg, .now_ms = now_ms));
    now_ms += CLI_PREVIEW_LOOK_MS;
    (void)stepped(key, &frame, now_ms);
    drawn(&raster, &glyphs, palette, &frame);
    cli_check((frame.preview.x == whole.x) && (frame.preview.width == shared) && (frame.editor.width == 0) &&
                  (drawn_in(&raster, (GuiRect){whole.x, whole.y, 40, 40}) != 0u),
              "alone, the preview fills the editor's rectangle");
    EMBED_CALL(cli_preview.mode_set, CliPreviewCfg, .word = "beside");
    EMBED_CALL(gui_frame_displayed.preview, GuiFrameDisplayedCfg, .frame = &frame,
               .displayed = (int)EMBED_CALL(cli_preview.mode_get, CliPreviewCfg, .now_ms = now_ms));
    now_ms += CLI_PREVIEW_LOOK_MS;
    (void)stepped(key, &frame, now_ms);

    // the wheel by whole rows, bounded at the last item filling the rectangle
    cli_check(EMBED_CALL(gui_preview.wheel, GuiPreviewCfg, .frame = &frame, .rows = 1) &&
                  (EMBED_CALL(cli_preview_scroll.top, CliPreviewScrollCfg, .delta_px = 0) == GUI_PREVIEW_ROW),
              "a wheel row moves the top by a row");
    (void)EMBED_CALL(gui_preview.wheel, GuiPreviewCfg, .frame = &frame, .rows = 100000);
    const int extent = EMBED_CALL(cli_preview_item.extent, CliPreviewItemCfg, .at = 0u);
    cli_check(EMBED_CALL(cli_preview_scroll.top, CliPreviewScrollCfg, .delta_px = 0) == extent - frame.preview.height,
              "the top ends where the last item fills the rectangle");
    cli_check(!EMBED_CALL(gui_preview.wheel, GuiPreviewCfg, .frame = &frame, .rows = 1),
              "a wheel past the end moves nothing");
    const int top = EMBED_CALL(cli_preview_scroll.top, CliPreviewScrollCfg, .delta_px = 0);
    const unsigned int first = EMBED_CALL(cli_preview_item.first, CliPreviewItemCfg, .top_px = top);
    const CliPreviewItem *const started = EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = first);
    cli_check((started->type == CLI_PREVIEW_ITEM_BLOCK) && (started->y <= top) && (first > 0u),
              "the draw starts at the block standing at the top, past the items above it");
    drawn(&raster, &glyphs, palette, &frame);
    cli_check((drawn_in(&raster, over) == 0u) && (drawn_in(&raster, under) == 0u) &&
                  (drawn_in(&raster, frame.preview) != 0u),
              "scrolled, the items still stay inside the rectangle");

    // a press on the link opens the file it names; one elsewhere does nothing
    (void)EMBED_CALL(gui_preview.wheel, GuiPreviewCfg, .frame = &frame, .rows = -100000);
    const CliPreviewItem *const link =
        EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = run_reading("next"));
    CliPreviewOpen open;
    cli_check((link != NULL) &&
                  (EMBED_CALL(gui_preview.press, GuiPreviewCfg, .frame = &frame, .x = frame.preview.x + link->x + 1,
                              .y = frame.preview.y + link->y + 1, .open_out = &open) == CLI_PREVIEW_OPEN_FILE) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = open.path, .other = "notes/other.md", .cap = CLI_PATH_MAX_LEN}),
              "a press on the link answers the file of the tree it goes to");
    cli_check(EMBED_CALL(gui_preview.press, GuiPreviewCfg, .frame = &frame, .x = frame.preview.x + 2,
                         .y = frame.preview.y + 2, .open_out = &open) == CLI_PREVIEW_OPEN_NONE,
              "a press away from a link does nothing");
    cli_check(EMBED_CALL(gui_preview.press, GuiPreviewCfg, .frame = &frame, .x = frame.editor.x + 2,
                         .y = frame.editor.y + 2, .open_out = &open) == CLI_PREVIEW_OPEN_NONE,
              "a press on the text is not the preview's");

    // the cursor's line marks its block in the signal
    const CliViewInput placed = {.type = CLI_VIEW_POSITION, .row = 2ULL, .column = 0ULL};
    view_input(key, &placed);
    now_ms += CLI_PREVIEW_LOOK_MS;
    cli_check(stepped(key, &frame, now_ms), "the cursor's line moved draws anew");
    unsigned int marked = EMBED_CALL(cli_preview_item.count, CliPreviewItemCfg, .at = 0u);
    for (unsigned int at = 0u; at < EMBED_CALL(cli_preview_item.count, CliPreviewItemCfg, .at = 0u); at += 1u)
    {
        const CliPreviewItem *const item = EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = at);
        marked = ((item->flags & CLI_PREVIEW_HERE) != 0u) ? at : marked;
    }
    const CliPreviewItem *const mark = EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = marked);
    cli_check((mark != NULL) && (mark->type == CLI_PREVIEW_ITEM_BLOCK) && (mark->line == 2u),
              "the block of the cursor's line is marked");
    drawn(&raster, &glyphs, palette, &frame);
    const int top_now = EMBED_CALL(cli_preview_scroll.top, CliPreviewScrollCfg, .delta_px = 0);
    // the mark stands inside the raster, at the block's left less its reach
    const unsigned int edge_x = (unsigned int)(frame.preview.x + mark->x - GUI_PREVIEW_MARK_OUT);
    const unsigned int edge_y = (unsigned int)(frame.preview.y + mark->y - top_now + (mark->height / 2));
    cli_check(raster.pixels[(edge_y * raster.width) + edge_x] !=
                  raster.pixels[(edge_y * raster.width) + edge_x - 2u],
              "the mark is drawn in the signal beside the block");

    // a scroll of the editor moves the preview and none goes back
    const CliViewInput scrolled = {.type = CLI_VIEW_SCROLL, .top = 30ULL};
    view_input(key, &scrolled);
    now_ms += CLI_PREVIEW_LOOK_MS;
    cli_check(stepped(key, &frame, now_ms) &&
                  (EMBED_CALL(cli_preview_scroll.top, CliPreviewScrollCfg, .delta_px = 0) ==
                   EMBED_CALL(cli_preview_scroll.top_of, CliPreviewScrollCfg, .line = 30ULL)),
              "a scroll of the editor moves the preview's top to the same line");
    now_ms += CLI_PREVIEW_LOOK_MS;
    (void)stepped(key, &frame, now_ms);
    cli_check(cli_view_top(key) == 30ULL, "and no scroll goes back to the editor");

    g_cli_guard->persistent_buf_release(raster.pixels, g_gui_raster_owner);
    raster.pixels = NULL;
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
