// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// explorer.c: the explorer's rows as the listing stands. A file's mark is its type's as the page's icons draw them,
// else the first two letters of its type quieter
#include "explorer.h"

#include "../../command_line/cli_internal.h"
#include "../../editor/view.h"
#include "../../text/text.h"

// A mark: the type or name it stands for, its text, and which of the palette's mark colors it takes, 0 for the
// quieter text color
typedef struct
{
    const char *type;
    const char *text;
    unsigned int color;
} GuiMark;

// the marks by a file's type
static const GuiMark s_marks[] = {
    {"js", "JS", 3u},
    {"mjs", "JS", 3u},
    {"cjs", "JS", 3u},
    {"jsx", "JS", 3u},
    {"ts", "TS", 4u},
    {"tsx", "TS", 4u},
    {"mts", "TS", 4u},
    {"cts", "TS", 4u},
    {"json", "{}", 3u},
    {"jsonc", "{}", 3u},
    {"cfg", "{}", 3u},
    {"rs", "rs", 1u},
    {"py", "py", 4u},
    {"pyi", "py", 5u},
    {"ipynb", "nb", 3u},
    {"sh", "$", 2u},
    {"bash", "$", 2u},
    {"zsh", "$", 2u},
    {"fish", "$", 2u},
    {"ps1", "PS", 4u},
    {"psm1", "PS", 4u},
    {"bat", "$", 7u},
    {"cmd", "$", 7u},
    {"c", "C", 4u},
    {"h", "h", 5u},
    {"cu", "cu", 2u},
    {"cuh", "cu", 5u},
    {"cpp", "C", 4u},
    {"cc", "C", 4u},
    {"cxx", "C", 4u},
    {"hpp", "h", 5u},
    {"hh", "h", 5u},
    {"cs", "C#", 6u},
    {"fs", "F#", 4u},
    {"go", "go", 4u},
    {"java", "jv", 1u},
    {"kt", "kt", 6u},
    {"kts", "kt", 6u},
    {"scala", "sc", 1u},
    {"swift", "sw", 1u},
    {"rb", "rb", 1u},
    {"php", "ph", 6u},
    {"lua", "lu", 4u},
    {"r", "R", 4u},
    {"jl", "jl", 6u},
    {"hs", "hs", 6u},
    {"ml", "ml", 3u},
    {"ex", "ex", 6u},
    {"exs", "ex", 6u},
    {"erl", "er", 1u},
    {"dart", "dt", 4u},
    {"zig", "zg", 3u},
    {"nim", "nm", 3u},
    {"lean", "L", 4u},
    {"m", "m", 3u},
    {"mojo", "mo", 1u},
    {"v", "v", 2u},
    {"sv", "sv", 2u},
    {"vhd", "vh", 2u},
    {"vhdl", "vh", 2u},
    {"asm", "as", 8u},
    {"s", "as", 8u},
    {"sql", "sq", 5u},
    {"qry", "q", 2u},
    {"graphql", "gq", 6u},
    {"proto", "pb", 4u},
    {"md", "M", 4u},
    {"mdx", "M", 4u},
    {"rst", "rs", 7u},
    {"txt", "tx", 8u},
    {"log", "lg", 8u},
    {"html", "<>", 1u},
    {"htm", "<>", 1u},
    {"svg", "<>", 3u},
    {"xml", "<>", 1u},
    {"vue", "<>", 2u},
    {"svelte", "<>", 1u},
    {"css", "#", 4u},
    {"scss", "#", 6u},
    {"less", "#", 4u},
    {"tex", "T", 2u},
    {"sty", "T", 2u},
    {"cls", "T", 2u},
    {"bib", "T", 6u},
    {"toml", "\xe2\x89\xa1", 7u},
    {"ini", "\xe2\x89\xa1", 7u},
    {"env", "\xe2\x89\xa1", 7u},
    {"yml", "\xe2\x89\xa1", 5u},
    {"yaml", "\xe2\x89\xa1", 5u},
    {"tsv", "\xe2\x8a\x9e", 2u},
    {"csv", "\xe2\x8a\x9e", 2u},
    {"parquet", "\xe2\x8a\x9e", 5u},
    {"lock", "\xe2\x89\xa1", 8u},
    {"png", "\xe2\x96\xa3", 5u},
    {"jpg", "\xe2\x96\xa3", 5u},
    {"jpeg", "\xe2\x96\xa3", 5u},
    {"gif", "\xe2\x96\xa3", 5u},
    {"webp", "\xe2\x96\xa3", 5u},
    {"bmp", "\xe2\x96\xa3", 5u},
    {"ico", "\xe2\x96\xa3", 5u},
    {"pdf", "\xe2\x96\xa4", 1u},
    {"wav", "au", 6u},
    {"mp3", "au", 6u},
    {"flac", "au", 6u},
    {"ogg", "au", 6u},
    {"mp4", "av", 6u},
    {"mkv", "av", 6u},
    {"webm", "av", 6u},
    {"zip", "zp", 7u},
    {"tar", "zp", 7u},
    {"gz", "zp", 7u},
    {"tgz", "zp", 7u},
    {"xz", "zp", 7u},
    {"bz2", "zp", 7u},
    {"7z", "zp", 7u},
    {"rar", "zp", 7u},
    {"bin", "01", 8u},
    {"exe", "01", 8u},
    {"dll", "01", 8u},
    {"so", "01", 8u},
    {"dylib", "01", 8u},
    {"o", "01", 8u},
    {"a", "01", 8u},
    {"obj", "01", 8u},
    {"wasm", "01", 3u},
};

// the marks by a file's whole name, lower case
static const GuiMark s_named[] = {
    {".gitignore", "\xe2\x97\x86", 1u},
    {".gitattributes", "\xe2\x97\x86", 1u},
    {".gitmodules", "\xe2\x97\x86", 1u},
    {".clang-format", "\xe2\x89\xa1", 7u},
    {".editorconfig", "\xe2\x89\xa1", 7u},
    {"makefile", "mk", 7u},
    {"gnumakefile", "mk", 7u},
    {"cmakelists.txt", "cm", 2u},
    {"dockerfile", "dk", 4u},
    {"containerfile", "dk", 4u},
    {"license", "Li", 8u},
    {"licence", "Li", 8u},
    {"copying", "Li", 8u},
    {"readme", "M", 4u},
    {"changelog", "M", 4u},
    {"justfile", "mk", 7u},
    {"procfile", "\xe2\x89\xa1", 7u},
};

typedef struct
{
    const char *name;
    unsigned long long length;
    char *text;
    unsigned int at;
} ExplorerCtx;

// the mark of the file `name`, `length` bytes, its text into `text`, sized GUI_EXPLORER_MARK_MOST, and the mark's
// color, 0 for the quieter one
unsigned int gui_explorer_mark(const GuiExplorerMarkCfg *args)
{
    char *text = args->text;
    char lower[GUI_EXPLORER_MARK_MOST];
    const unsigned long long saved =
        (args->length < GUI_EXPLORER_MARK_MOST - 1ULL) ? args->length : GUI_EXPLORER_MARK_MOST - 1ULL;
    memor.cpy(&(MemoriaCfg){.dst = lower, .src = args->name, .bytes = saved});
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .size = saved, .text_out = lower);
    lower[saved] = '\0';
    for (unsigned int at = 0u; at < sizeof(s_named) / sizeof(s_named[0]); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = s_named[at].type, .cap = GUI_EXPLORER_MARK_MOST}))
        {
            VerbaLinea line = {text, GUI_EXPLORER_MARK_MOST, 0ULL};
            mmgr_line_put(&line, s_named[at].text);
            (void)mmgr_line_finish(&line);
            return s_named[at].color;
        }
    }
    // the type is what follows the last dot after the first letter; a name of dots alone has none
    const unsigned long long past_dot = (saved > 1ULL)
                                            ? EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                         .text = lower + 1, .at = saved - 1ULL, .set = ".")
                                            : 0ULL;
    const char *const type = (past_dot != 0ULL) ? (lower + 1ULL + past_dot) : "";
    for (unsigned int at = 0u; at < sizeof(s_marks) / sizeof(s_marks[0]); at += 1u)
    {
        if ((type[0] != '\0') &&
            cellul.eq(&(CatenaFinitaCfg){.src = type, .other = s_marks[at].type, .cap = GUI_EXPLORER_MARK_MOST}))
        {
            VerbaLinea line = {text, GUI_EXPLORER_MARK_MOST, 0ULL};
            mmgr_line_put(&line, s_marks[at].text);
            (void)mmgr_line_finish(&line);
            return s_marks[at].color;
        }
    }
    // the quieter mark: the first two letters of the type, or of the name past its leading dots
    const char *from = (type[0] != '\0') ? type : lower;
    from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}), .set = ".");
    VerbaLinea line = {text, GUI_EXPLORER_MARK_MOST, 0ULL};
    mmgr_line_put_n(&line, from,
                    (cellul.len(&(CatenaFinitaCfg){.src = from, .cap = GUI_EXPLORER_MARK_MOST}) < 2ULL) ? 1ULL : 2ULL);
    (void)mmgr_line_finish(&line);
    return 0u;
}

// the pane's head: the tree's name
static const char *explorer_title(const GuiPaneCfg *args)
{
    return (args->tree != NULL) ? args->tree : "";
}

static unsigned int explorer_rows(const GuiPaneCfg *args)
{
    (void)args;
    return cli_files_explorer_rows();
}

// the row of the file the editor shows, its path the shown view's; a view's place is far inside an int
static int explorer_current(const GuiPaneCfg *args)
{
    (void)args;
    const int shown = cli_view_shown();
    const char *const shown_path = (shown >= 0) ? cli_view_path_at((unsigned int)shown) : NULL;
    CliExplorerRow listed;
    for (unsigned int at = 0u; (shown_path != NULL) && cli_files_explorer_row(at, &listed); at += 1u)
    {
        if (!listed.dir &&
            cellul.eq(&(CatenaFinitaCfg){.src = listed.path, .other = shown_path, .cap = CLI_PATH_MAX_LEN}))
        {
            // the rows are far fewer than an int counts
            return (int)at;
        }
    }
    return GUI_PANE_NONE;
}

// one row drawn: its guides, its arrow, its icon or mark, and its name
static void explorer_row_draw(const GuiPaneCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiFrame *const frame = args->frame;
    const GuiRect rect = args->rect;
    const GuiRect clip = args->clip;
    const int scale = frame->scale;
    CliExplorerRow listed;
    if (!cli_files_explorer_row(args->row, &listed))
    {
        return;
    }
    const CliExplorerRow *const node = &listed;
    const unsigned long long name_length = cellul.len(&(CatenaFinitaCfg){.src = node->name, .cap = CLI_PATH_MAX_LEN});
    int x = gui_pane.guides(args);
    const int twisty =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_TWISTY, .scale = scale);
    if (node->dir)
    {
        const int arrow =
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANE_ARROW, .scale = scale);
        EMBED_CALL(gui_pane.arrow_draw, GuiPaneCfg, .raster = raster, .x = x + ((twisty - arrow) / 2),
                   .y = rect.y + ((rect.height - arrow) / 2), .side = arrow, .open = node->open,
                   .color = GUI_FRAME_FADED(args->palette->text_2, frame->side_share), .clip = clip);
    }
    x += twisty;
    const int icon = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_ICON, .scale = scale);
    if (node->dir)
    {
        const unsigned int size =
            (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 14, .scale = scale);
        const GuiMask *const mask_bits =
            EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = args->icons, .name = "folder", .size = size,
                       .stroke = GUI_ICON_STROKE * (float)size / GUI_ICON_UNITS);
        if (mask_bits != NULL)
        {
            // the icon's side is far inside what an int represents
            EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = raster, .mask_bits = mask_bits,
                       .x = x + ((icon - (int)size) / 2), .y = rect.y + ((rect.height - (int)size) / 2),
                       .color = GUI_FRAME_FADED(args->palette->link, frame->side_share), .clip = clip);
        }
    }
    else
    {
        char text[GUI_EXPLORER_MARK_MOST];
        const unsigned int color = EMBED_CALL(gui_explorer_mark_table.mark, GuiExplorerMarkCfg, .name = node->name,
                                              .length = name_length, .text = text);
        const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                           .pixels = (int)GUI_EXPLORER_MARK_SIZE, .scale = scale);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = GUI_EXPLORER_MARK_MOST});
        const float pixel_width = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                             .face = GUI_FACE_CODE_BOLD, .size = size, .text = text, .length = length);
        (void)EMBED_CALL(
            gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
            .face = GUI_FACE_CODE_BOLD, .size = size, .x_f32 = (float)x + (((float)icon - pixel_width) / 2.0f),
            .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                            .face = GUI_FACE_CODE_BOLD, .size = size, .top = rect.y, .pixel_height = rect.height),
            .text = text, .length = length,
            .color = GUI_FRAME_FADED((color != 0u) ? args->palette->mark[color - 1u] : args->palette->text_3,
                                     frame->side_share),
            .clip = clip);
    }
    x += icon + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_ICON_GAP, .scale = scale);
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_PANE_ROW_SIZE, .scale = scale);
    GuiColor color;
    if (node->dir)
    {
        color = args->palette->text;
    }
    else if (node->ignored)
    {
        color = args->palette->text_3;
    }
    else
    {
        color = args->palette->text_2;
    }
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)x,
                     .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_HEAD, .size = size, .top = rect.y, .pixel_height = rect.height),
                     .text = node->name, .length = name_length, .color = GUI_FRAME_FADED(color, frame->side_share),
                     .clip = clip);
}

// a folder's row pressed opens or closes it; a file's opens the file
static int explorer_press(const GuiPaneCfg *args)
{
    CliExplorerRow row;
    const int listed = cli_files_explorer_row(args->row, &row);
    if (cli_files_explorer_press(args->row) || !listed || row.dir)
    {
        return 0;
    }
    *args->open_out = (GuiPaneOpen){row.path, {0ULL, 0ULL}, 0};
    return 1;
}

// the tree listed as the pane shows
static void explorer_read(const GuiPaneCfg *args)
{
    if (args->event == GUI_PANE_SHOWN)
    {
        cli_files_explorer_show(args->root);
    }
}

const GuiPaneKind gui_explorer_kind = {
    .tool = "explorer",
    .title = explorer_title,
    .rows = explorer_rows,
    .row_draw = explorer_row_draw,
    .current = explorer_current,
    .press = explorer_press,
    .read = explorer_read,
};
