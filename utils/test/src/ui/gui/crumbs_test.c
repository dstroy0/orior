// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// crumbs_test.c: the status bar's crumbs: a view on src/code/a.py with the cursor inside a method lays the folders, the
// file and the declarations it stands in, in that order, with the CLI's texts; their rectangles stand in the order of
// x, a press finds the crumb under it and none in a gap; the row cut short leaves out the crumbs that do not fit
// whole; a folder's crumb lists its entries, each opening its path, and a declaration's its neighbors, each going to
// its line; and with the setting off nothing is laid
#include "../../../../../ui/cli/src/command_line/commands.h"
#include "../../../../../ui/cli/src/view/frame.h"
#include "../../../../../ui/cli/src/view/parts/crumbs.h"
#include "../cli/cli_check.h"

static const char s_key[] = "crumbs_check";
static const char s_path[] = "src/code/a.py";
static const char s_text[] = "import os\n"
                             "\n"
                             "class Box:\n"
                             "    def put(self):\n"
                             "        return 1\n"
                             "\n"
                             "    def take(self):\n"
                             "        return 2\n";

// the file `relative` of the tree at `root` written with `text`
static void file_written(const char *root, const char *relative, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = relative, .joined = path);
    cli_check(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text,
                         .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES})),
              relative);
}

// the crumb `at` is of `type` and reads `text`
static int crumb_is(unsigned int at, unsigned int type, const char *text)
{
    CliCrumbRow row;
    return EMBED_CALL(cli_crumbs.row, CliCrumbsCfg, .at = at, .row = &row) && (row.type == type) &&
           (row.length == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES})) &&
           (memor.cmp(&(MemoriaCfg){.src = row.text, .other = text, .bytes = row.length}) == 0);
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
    static char s_root[CLI_PATH_MAX_LEN];
    cli_check_folder("crumbs_tree", s_root);
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "src", .joined = folder);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "src/code", .joined = folder);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    file_written(s_root, s_path, s_text);
    file_written(s_root, "src/code/b.py", "x = 1\n");

    CliShape shape;
    cli_check(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = s_text,
                         .size = sizeof(s_text) - 1u, .shape = &shape),
              "the text is stored");
    EMBED_CALL(cli_crumbs.begin, CliCrumbsCfg, .entries = NULL);
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = s_key, .path = s_path, .line = 4ULL);
    cli_check(EMBED_CALL(cli_crumbs.rows, CliCrumbsCfg, .at = 0u) == 5u, "five crumbs stand");
    cli_check(crumb_is(0u, CLI_CRUMB_FOLDER, "src") && crumb_is(1u, CLI_CRUMB_FOLDER, "code") &&
                  crumb_is(2u, CLI_CRUMB_FILE, "a.py") && crumb_is(3u, CLI_CRUMB_SYMBOL, "Box") &&
                  crumb_is(4u, CLI_CRUMB_SYMBOL, "put"),
              "the folders, the file and the declarations stand in order with the CLI's texts");

    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 900u, .height = 40u),
              "the raster is taken");
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 12,
                                                       .scale = 100);
    const GuiRect wide = {0, 0, 900, 40};
    const float after = EMBED_CALL(gui_crumbs.draw, GuiCrumbsCfg, .raster = &raster, .glyphs = &glyphs,
                                   .palette = palette, .size = size, .x_f32 = 10.0f, .baseline = 26, .clip = wide,
                                   .scale = 100);
    cli_check((EMBED_CALL(gui_crumbs.count, GuiCrumbsCfg, .index = 0u) == 5u) && (after > 10.0f),
              "every crumb is laid in a wide row");
    int ordered = 1;
    for (unsigned int at = 1u; at < 5u; at += 1u)
    {
        const GuiRect before = EMBED_CALL(gui_crumbs.place, GuiCrumbsCfg, .index = at - 1u);
        const GuiRect each = EMBED_CALL(gui_crumbs.place, GuiCrumbsCfg, .index = at);
        ordered = ordered && (each.x > before.x + before.width);
    }
    cli_check(ordered, "the crumbs' rectangles stand in the order of x with a gap between each");
    const GuiRect file = EMBED_CALL(gui_crumbs.place, GuiCrumbsCfg, .index = 2u);
    unsigned int index = 99u;
    cli_check(EMBED_CALL(gui_crumbs.hit, GuiCrumbsCfg, .x = file.x + 2, .y = 20, .index_out = &index) && (index == 2u),
              "a press on the file's crumb finds it");
    const GuiRect folder_rect = EMBED_CALL(gui_crumbs.place, GuiCrumbsCfg, .index = 0u);
    cli_check(!EMBED_CALL(gui_crumbs.hit, GuiCrumbsCfg, .x = folder_rect.x + folder_rect.width + 2, .y = 20,
                          .index_out = &index),
              "a press in the gap after a crumb finds none");

    // a row cut short: the crumbs that fit whole, none drawn in part
    const GuiRect second = EMBED_CALL(gui_crumbs.place, GuiCrumbsCfg, .index = 1u);
    const GuiRect narrow = {0, 0, second.x + second.width + 4, 40};
    (void)EMBED_CALL(gui_crumbs.draw, GuiCrumbsCfg, .raster = NULL, .glyphs = &glyphs, .palette = palette,
                     .size = size, .x_f32 = 10.0f, .baseline = 26, .clip = narrow, .scale = 100);
    const GuiRect last = EMBED_CALL(gui_crumbs.place, GuiCrumbsCfg, .index = 1u);
    cli_check((EMBED_CALL(gui_crumbs.count, GuiCrumbsCfg, .index = 0u) == 2u) &&
                  (last.x + last.width <= narrow.x + narrow.width),
              "a crumb that does not fit whole is left out");

    // what stands beside a folder and a declaration
    unsigned int count = 0u;
    const CliItem *rows = NULL;
    cli_check(EMBED_CALL(cli_crumbs.menu, CliCrumbsCfg, .at = 1u, .root = s_root) &&
                  ((rows = cli_menus_own_rows(&count)) != NULL) && (count == 2u) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].command, .cap = CLI_POOL_BYTES, .other = "file-open", .other_cap = 10u}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].args, .cap = CLI_POOL_BYTES, .other = "src/code/a.py", .other_cap = 14u}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[1].args, .cap = CLI_POOL_BYTES, .other = "src/code/b.py", .other_cap = 14u}),
              "a folder's crumb lists its entries, each opening its path");
    cli_check(EMBED_CALL(cli_crumbs.menu, CliCrumbsCfg, .at = 4u, .root = s_root) &&
                  ((rows = cli_menus_own_rows(&count)) != NULL) && (count == 2u) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].command, .cap = CLI_POOL_BYTES, .other = "symbol-go", .other_cap = 10u}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[0].label, .cap = CLI_POOL_BYTES, .other = "put", .other_cap = 4u}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = rows[1].label, .cap = CLI_POOL_BYTES, .other = "take", .other_cap = 5u}) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = rows[1].args, .cap = CLI_POOL_BYTES, .other = "6", .other_cap = 2u}),
              "a declaration's crumb lists those beside it, each going to its line");

    // the setting off: nothing laid and the row's x unmoved
    EMBED_CALL(cli_crumbs.shown_set, CliCrumbsCfg, .on = 0);
    const float bare = EMBED_CALL(gui_crumbs.draw, GuiCrumbsCfg, .raster = &raster, .glyphs = &glyphs,
                                  .palette = palette, .size = size, .x_f32 = 10.0f, .baseline = 26, .clip = wide,
                                  .scale = 100);
    cli_check((EMBED_CALL(gui_crumbs.count, GuiCrumbsCfg, .index = 0u) == 0u) && (bare == 10.0f) &&
                  (EMBED_CALL(cli_crumbs.rows, CliCrumbsCfg, .at = 0u) == 0u),
              "with the setting off no crumb is laid");
    EMBED_CALL(cli_crumbs.shown_set, CliCrumbsCfg, .on = 1);
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .path = NULL);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_key);
    return cli_on_check();
}
