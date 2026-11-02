// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// explorer.c: the explorer's nodes stand in the tree's order, each folder's entries after it one level deeper, and a
// row is a node whose folders above are all open; a folder's entries are saved once listed, shown and hidden as it
// opens and closes. A file's mark is its type's as the page's icons give them, else the first two letters of its type
// quieter
#include "explorer.h"

#include "../../../cli/src/command_line/cli_internal.h"
#include "../../../cli/src/text/text.h"
#include "../answer.h"

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

// the most bytes of a name a mark is looked up by
#define EXPLORER_NAME_MOST 64u

int gui_explorer_init(const GuiExplorerCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->explorer, .val = 0, .bytes = sizeof(*args->explorer)});
    args->explorer->pointed = GUI_EXPLORER_NONE;
    args->explorer->node =
        g_cli_cells->persistent_buf_alloc(GUI_EXPLORER_NODES * sizeof(*args->explorer->node), &g_gui_explorer_cells);
    args->explorer->text = g_cli_cells->persistent_buf_alloc(GUI_EXPLORER_TEXT, &g_gui_explorer_cells);
    if ((args->explorer->node == NULL) || (args->explorer->text == NULL))
    {
        cli_explorer.free(args);
        return 0;
    }
    return 1;
}

void gui_explorer_free(const GuiExplorerCfg *args)
{
    g_cli_cells->persistent_buf_release(args->explorer->node, &g_gui_explorer_cells);
    g_cli_cells->persistent_buf_release(args->explorer->text, &g_gui_explorer_cells);
    args->explorer->node = NULL;
    args->explorer->text = NULL;
    args->explorer->nodes = 0u;
    args->explorer->used = 0ull;
}

void gui_explorer_clear(const GuiExplorerCfg *args)
{
    args->explorer->nodes = 0u;
    args->explorer->used = 0ull;
    args->explorer->pointed = GUI_EXPLORER_NONE;
    args->explorer->scroll = 0u;
    args->explorer->root_listed = 0;
    args->explorer->root_ticket = 0ull;
}

typedef struct
{
    const GuiExplorer *explorer;
    const char *path;
    GuiExplorer *explorer_out;
    const CliJson *json;
    unsigned int item;
    unsigned int at;
    unsigned int depth;
    unsigned int wanted;
    unsigned int *node;
    const GuiFrame *frame;
    int byte;
    const char *name;
    unsigned long long length;
    char *text;
    GuiRaster *raster;
    int x;
    int y;
    int side;
    int open;
    GuiColor color;
    GuiGlyphs *glyphs;
    unsigned int face;
    unsigned int size;
    int top;
    int tall;
    GuiIcons *icons;
    const GuiPalette *palette;
    unsigned int row;
    GuiRect clip;
    const char *shown;
} ExplorerCtx;
// the node at `path`, or GUI_EXPLORER_NONE
EMBED_INLINE int explorer_node_at(const ExplorerCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN});
    for (unsigned int at = 0u; at < args->explorer->nodes; at += 1u)
    {
        const GuiNode *const node = &args->explorer->node[at];
        if ((node->length == length) &&
            (memor.cmp(&(MemoriaCfg){.src = args->explorer->text + node->path, .other = args->path, .bytes = length}) ==
             0))
        {
            // the nodes are fewer than an int counts
            return (int)at;
        }
    }
    return GUI_EXPLORER_NONE;
}

void gui_explorer_listing(const GuiExplorerCfg *args)
{
    unsigned long long ticket = args->ticket;
    if (args->dir[0] == '\0')
    {
        args->explorer->root_ticket = ticket;
        return;
    }
    const int at = EMBED_CALL(explorer_node_at, ExplorerCtx, .explorer = args->explorer, .path = args->dir);
    if (at != GUI_EXPLORER_NONE)
    {
        args->explorer->node[at].ticket = ticket;
    }
}

int gui_explorer_root_request(const GuiExplorerNodeCfg *args)
{
    return args->explorer_read->root_listed || (args->explorer_read->root_ticket != 0ull);
}

// a node for the entry `item` put at `at`, the nodes from there moved on by one, its path saved in the text: 1, or 0
// where the entry is not one or there is no room
EMBED_INLINE int explorer_node_put(const ExplorerCtx *args)
{
    const CliJson *json = args->json;
    unsigned int depth = args->depth;
    unsigned long long path_length = 0ull;
    const char *const path =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->item, .name = "path"),
                   .length_pointer = &path_length);
    unsigned long long name_length = 0ull;
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->item, .name = "name"),
                   .length_pointer = &name_length);
    if ((path == NULL) || (path_length == 0ull) || (args->explorer_out->nodes >= GUI_EXPLORER_NODES) ||
        (args->explorer_out->used + path_length + 1ull > GUI_EXPLORER_TEXT))
    {
        return 0;
    }
    for (unsigned int each = args->explorer_out->nodes; each > args->at; each -= 1u)
    {
        args->explorer_out->node[each] = args->explorer_out->node[each - 1u];
    }
    GuiNode *const node = &args->explorer_out->node[args->at];
    memor.set(&(MemoriaCfg){.dst = node, .val = 0, .bytes = sizeof(*node)});
    memor.cpy(
        &(MemoriaCfg){.dst = args->explorer_out->text + args->explorer_out->used, .src = path, .bytes = path_length});
    args->explorer_out->text[args->explorer_out->used + path_length] = '\0';
    // the text is far inside what an unsigned int counts
    node->path = (unsigned int)args->explorer_out->used;
    node->length = (unsigned int)path_length;
    node->name = ((name != NULL) && (name_length <= path_length)) ? (unsigned int)(path_length - name_length) : 0u;
    node->depth = depth;
    node->dir = EMBED_CALL(gui_answer_object.member_true, GuiAnswerObjectCfg, .json = json, .object = args->item,
                           .name = "dir");
    node->ignored = EMBED_CALL(gui_answer_object.member_true, GuiAnswerObjectCfg, .json = json, .object = args->item,
                               .name = "ignored");
    args->explorer_out->used += path_length + 1ull;
    args->explorer_out->nodes += 1u;
    return 1;
}

int gui_explorer_list(const GuiExplorerTicketCfg *args)
{
    unsigned long long ticket = args->ticket;
    if ((ticket == 0ull) || !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = args->json, .node = args->root, .kind = CFG_JSON_ARRAY))
    {
        return 0;
    }
    unsigned int at = args->explorer->nodes;
    unsigned int depth = 0u;
    if (ticket == args->explorer->root_ticket)
    {
        args->explorer->root_ticket = 0ull;
        args->explorer->root_listed = 1;
    }
    else
    {
        int parent = GUI_EXPLORER_NONE;
        for (unsigned int each = 0u; each < args->explorer->nodes; each += 1u)
        {
            if (args->explorer->node[each].ticket == ticket)
            {
                // the nodes are fewer than an int counts
                parent = (int)each;
                break;
            }
        }
        if (parent == GUI_EXPLORER_NONE)
        {
            return 0;
        }
        args->explorer->node[parent].ticket = 0ull;
        args->explorer->node[parent].listed = 1;
        at = (unsigned int)parent + 1u;
        depth = args->explorer->node[parent].depth + 1u;
    }
    for (unsigned int each = 0u; each < args->json->node[args->root].count; each += 1u)
    {
        if (EMBED_CALL(explorer_node_put, ExplorerCtx, .explorer_out = args->explorer, .json = args->json,
                       .item = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = args->json, .array = args->root, .index = each),
                       .at = at, .depth = depth))
        {
            at += 1u;
        }
    }
    return 1;
}

// the rows walked: the node of the row `wanted` into `node`, or the rows counted where `wanted` is past them
EMBED_INLINE unsigned int explorer_rows_walk(const ExplorerCtx *args)
{
    unsigned int *node = args->node;
    unsigned int rows = 0u;
    unsigned int hidden_below = GUI_EXPLORER_NODES;
    for (unsigned int at = 0u; at < args->explorer->nodes; at += 1u)
    {
        const GuiNode *const one = &args->explorer->node[at];
        if (one->depth > hidden_below)
        {
            continue;
        }
        hidden_below = (one->dir && !one->open) ? one->depth : GUI_EXPLORER_NODES;
        if (rows == args->wanted)
        {
            *node = at;
            return rows;
        }
        rows += 1u;
    }
    return rows;
}

unsigned int gui_explorer_rows(const GuiExplorerFrameXYCfg *args)
{
    unsigned int node = 0u;
    return EMBED_CALL(explorer_rows_walk, ExplorerCtx, .explorer = args->explorer_read, .wanted = GUI_EXPLORER_NODES,
                      .node = &node);
}

int gui_explorer_row(const GuiExplorerRowCfg *args)
{
    unsigned int *node = args->node;
    *node = GUI_EXPLORER_NODES;
    (void)EMBED_CALL(explorer_rows_walk, ExplorerCtx, .explorer = args->explorer_read, .wanted = args->row,
                     .node = node);
    return *node < GUI_EXPLORER_NODES;
}

const char *gui_explorer_path(const GuiExplorerNodeCfg *args)
{
    unsigned long long *length = args->length;
    *length = args->explorer_read->node[args->node_value].length;
    return args->explorer_read->text + args->explorer_read->node[args->node_value].path;
}

// where the rows begin in the side, below the explorer's head and the pane's
EMBED_INLINE int explorer_rows_top(const ExplorerCtx *args)
{
    return args->frame->side.y +
           EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                      .pixels = GUI_EXPLORER_HEAD + GUI_EXPLORER_PANE_LINE + GUI_EXPLORER_PANE_HEAD,
                      .scale = args->frame->scale);
}

GuiRect gui_explorer_row_rect(const GuiExplorerRowFrameCfg *args)
{
    const int tall =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_ROW, .scale = args->frame->scale);
    // the rows and the scroll are far fewer than an int counts
    const int from_top = (int)args->row - (int)args->explorer_read->scroll;
    return (GuiRect){args->frame->side.x,
                     EMBED_CALL(explorer_rows_top, ExplorerCtx, .frame = args->frame) + (from_top * tall),
                     args->frame->side.width - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1,
                                                          .scale = args->frame->scale),
                     tall};
}

int gui_explorer_hit(const GuiExplorerFrameXYCfg *args)
{
    unsigned int *row = args->row_pointer;
    const int top = EMBED_CALL(explorer_rows_top, ExplorerCtx, .frame = args->frame);
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->side, .x = args->x,
                    .y = args->y) ||
        (args->y < top))
    {
        return 0;
    }
    // the pixel stands below the rows' top: the row counted is not negative
    const unsigned int at =
        (unsigned int)((args->y - top) / EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                    .pixels = GUI_EXPLORER_ROW, .scale = args->frame->scale)) +
        args->explorer_read->scroll;
    if (at >= gui_explorer_frame_xy.rows(args))
    {
        return 0;
    }
    *row = at;
    return 1;
}

int gui_explorer_move(const GuiExplorerFrameXCfg *args)
{
    unsigned int row = 0u;
    // the rows are far fewer than an int counts
    const int pointed = EMBED_CALL(gui_explorer_frame_xy.hit, GuiExplorerFrameXYCfg, .explorer_read = args->explorer,
                                   .frame = args->frame, .x = args->x, .y = args->y, .row_pointer = &row)
                            ? (int)row
                            : GUI_EXPLORER_NONE;
    if (pointed == args->explorer->pointed)
    {
        return 0;
    }
    args->explorer->pointed = pointed;
    return 1;
}

int gui_explorer_press(const GuiExplorerFrameXCfg *args)
{
    args->pick->kind = GUI_PICK_NONE;
    args->pick->path[0] = '\0';
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->side, .x = args->x,
                    .y = args->y))
    {
        return 0;
    }
    unsigned int row = 0u;
    unsigned int at = 0u;
    if (!EMBED_CALL(gui_explorer_frame_xy.hit, GuiExplorerFrameXYCfg, .explorer_read = args->explorer,
                    .frame = args->frame, .x = args->x, .y = args->y, .row_pointer = &row) ||
        !EMBED_CALL(cli_explorer_row.row, GuiExplorerRowCfg, .explorer_read = args->explorer, .row = row, .node = &at))
    {
        return 1;
    }
    GuiNode *const node = &args->explorer->node[at];
    VerbaLinea path = {args->pick->path, sizeof(args->pick->path), 0ull};
    mmgr_line_put_n(&path, args->explorer->text + node->path, node->length);
    (void)mmgr_line_finish(&path);
    if (!node->dir)
    {
        args->pick->kind = GUI_PICK_OPEN;
        return 1;
    }
    node->open = !node->open;
    if (node->open && !node->listed && (node->ticket == 0ull))
    {
        args->pick->kind = GUI_PICK_LIST;
    }
    return 1;
}

int gui_explorer_wheel(const GuiExplorerFrameCfg *args)
{
    const int tall =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_ROW, .scale = args->frame->scale);
    const int room = args->frame->side.y + args->frame->side.height -
                     EMBED_CALL(explorer_rows_top, ExplorerCtx, .frame = args->frame);
    const int shown = (tall > 0) ? room / tall : 0;
    // the rows are far fewer than an int counts
    const int count =
        (int)EMBED_CALL(gui_explorer_frame_xy.rows, GuiExplorerFrameXYCfg, .explorer_read = args->explorer);
    const int most = (count > shown) ? count - shown : 0;
    const int wanted = (int)args->explorer->scroll + args->rows;
    const int clamped = (wanted < 0) ? 0 : ((wanted > most) ? most : wanted);
    // the scroll clamped is not negative
    if ((unsigned int)clamped == args->explorer->scroll)
    {
        return 0;
    }
    args->explorer->scroll = (unsigned int)clamped;
    return 1;
}

EMBED_INLINE int explorer_lower(const ExplorerCtx *args)
{
    return ((args->byte >= 'A') && (args->byte <= 'Z')) ? args->byte - 'A' + 'a' : args->byte;
}

// the mark of the file named by `length` bytes of `name`: its text into `text`, sized EXPLORER_NAME_MOST, and the
// mark's color, 0 for the quieter one
EMBED_INLINE unsigned int explorer_mark(const ExplorerCtx *args)
{
    char *text = args->text;
    char lower[EXPLORER_NAME_MOST];
    const unsigned long long saved =
        (args->length < EXPLORER_NAME_MOST - 1ull) ? args->length : EXPLORER_NAME_MOST - 1ull;
    for (unsigned long long at = 0ull; at < saved; at += 1ull)
    {
        lower[at] = (char)EMBED_CALL(explorer_lower, ExplorerCtx, .byte = args->name[at]);
    }
    lower[saved] = '\0';
    for (unsigned int at = 0u; at < sizeof(s_named) / sizeof(s_named[0]); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = s_named[at].type, .cap = EXPLORER_NAME_MOST}))
        {
            VerbaLinea line = {text, EXPLORER_NAME_MOST, 0ull};
            mmgr_line_put(&line, s_named[at].text);
            (void)mmgr_line_finish(&line);
            return s_named[at].color;
        }
    }
    // the type is what follows the last dot after the first letter; a name of dots alone has none
    unsigned long long dot = saved;
    for (unsigned long long at = 1ull; at < saved; at += 1ull)
    {
        dot = (lower[at] == '.') ? at : dot;
    }
    const char *const type = (dot < saved) ? lower + dot + 1ull : "";
    for (unsigned int at = 0u; at < sizeof(s_marks) / sizeof(s_marks[0]); at += 1u)
    {
        if ((type[0] != '\0') &&
            cellul.eq(&(CatenaFinitaCfg){.src = type, .other = s_marks[at].type, .cap = EXPLORER_NAME_MOST}))
        {
            VerbaLinea line = {text, EXPLORER_NAME_MOST, 0ull};
            mmgr_line_put(&line, s_marks[at].text);
            (void)mmgr_line_finish(&line);
            return s_marks[at].color;
        }
    }
    // the quieter mark: the first two letters of the type, or of the name past its leading dots
    const char *from = (type[0] != '\0') ? type : lower;
    from += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ull,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}), .set = ".");
    VerbaLinea line = {text, EXPLORER_NAME_MOST, 0ull};
    mmgr_line_put_n(&line, from,
                    (cellul.len(&(CatenaFinitaCfg){.src = from, .cap = EXPLORER_NAME_MOST}) < 2ull) ? 1ull : 2ull);
    (void)mmgr_line_finish(&line);
    return 0u;
}

// an arrow drawn in a box at x, y of `side` pixels, pointing right where closed and down where open
EMBED_INLINE void explorer_arrow_draw(const ExplorerCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiColor color = args->color;
    const int half = args->side / 2;
    if (args->open)
    {
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x, .y0 = args->y + 1,
                   .x1 = args->x + half, .y1 = args->y + half + 1, .color = color);
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + half,
                   .y0 = args->y + half + 1, .x1 = args->x + args->side - 1, .y1 = args->y + 1, .color = color);
    }
    else
    {
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + 1, .y0 = args->y,
                   .x1 = args->x + half + 1, .y1 = args->y + half, .color = color);
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + half + 1,
                   .y0 = args->y + half, .x1 = args->x + 1, .y1 = args->y + args->side - 1, .color = color);
    }
}

// a baseline that centers a face's letters in a row from `top`, `tall` pixels
EMBED_INLINE int explorer_baseline(const ExplorerCtx *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    unsigned int face = args->face;
    unsigned int size = args->size;
    const float ascent =
        EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = face, .size = size);
    const float descent =
        EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = face, .size = size);
    return args->top + (int)((((float)args->tall - ascent - descent) / 2.0f) + ascent + 0.5f);
}

// one row drawn: its ground where pointed or shown, its guides, its arrow, its icon or mark, and its name
EMBED_INLINE void explorer_row_draw(const ExplorerCtx *args)
{
    const GuiExplorer *explorer = args->explorer;
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    GuiIcons *icons = args->icons;
    const GuiFrame *frame = args->frame;
    unsigned int row = args->row;
    GuiRect clip = args->clip;
    const int scale = frame->scale;
    const GuiNode *const node = &explorer->node[args->at];
    const GuiRect rect = EMBED_CALL(gui_explorer_row_frame.row_rect, GuiExplorerRowFrameCfg, .frame = frame,
                                    .explorer_read = explorer, .row = row);
    const GuiRect met = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = rect, .other = clip);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = met))
    {
        return;
    }
    const char *const path = explorer->text + node->path;
    const int current = (args->shown != NULL) &&
                        (cellul.len(&(CatenaFinitaCfg){.src = args->shown, .cap = CLI_PATH_MAX_LEN}) == node->length) &&
                        (memor.cmp(&(MemoriaCfg){.src = args->shown, .other = path, .bytes = node->length}) == 0);
    // the rows are far fewer than an int counts
    if (explorer->pointed == (int)row)
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = met,
                   .color = args->palette->line);
    }
    else if (current)
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = met,
                   .color = args->palette->selection_idle);
    }
    const int guide =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_GUIDE, .scale = scale);
    int x =
        rect.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_PAD_LEFT, .scale = scale);
    for (unsigned int level = 0u; level < node->depth; level += 1u)
    {
        EMBED_CALL(
            gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
            .rect = EMBED_CALL(
                gui_raster_one.rect_intersect, GuiRasterOneCfg,
                .one = (GuiRect){x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 7, .scale = scale),
                                 rect.y,
                                 EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                                 rect.height},
                .other = clip),
            .color = args->palette->line);
        x += guide;
    }
    const int twisty =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_TWISTY, .scale = scale);
    if (node->dir)
    {
        const int arrow =
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_ARROW, .scale = scale);
        EMBED_CALL(explorer_arrow_draw, ExplorerCtx, .raster = raster, .x = x + ((twisty - arrow) / 2),
                   .y = rect.y + ((rect.height - arrow) / 2), .side = arrow, .open = node->open,
                   .color = args->palette->text_2);
    }
    x += twisty;
    const int icon = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_ICON, .scale = scale);
    if (node->dir)
    {
        const unsigned int size =
            (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 14, .scale = scale);
        const GuiMask *const mask = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = icons, .name = "folder",
                                               .size = size, .stroke = GUI_ICON_STROKE * (float)size / GUI_ICON_UNITS);
        if (mask != NULL)
        {
            // the icon's side is far inside what an int represents
            EMBED_CALL(gui_raster_xy_color.mask, GuiRasterXYColorCfg, .raster = raster, .mask = mask,
                       .x = x + ((icon - (int)size) / 2), .y = rect.y + ((rect.height - (int)size) / 2),
                       .color = args->palette->link, .clip = met);
        }
    }
    else
    {
        char text[EXPLORER_NAME_MOST];
        const unsigned int color = EMBED_CALL(explorer_mark, ExplorerCtx, .name = path + node->name,
                                              .length = node->length - node->name, .text = text);
        const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                           .pixels = (int)GUI_EXPLORER_MARK_SIZE, .scale = scale);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = EXPLORER_NAME_MOST});
        const float wide = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                      .face = GUI_FACE_CODE_BOLD, .size = size, .text = text, .length = length);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_CODE_BOLD, .size = size, .x_f32 = (float)x + (((float)icon - wide) / 2.0f),
                         .y = EMBED_CALL(explorer_baseline, ExplorerCtx, .glyphs = glyphs, .face = GUI_FACE_CODE_BOLD,
                                         .size = size, .top = rect.y, .tall = rect.height),
                         .text = text, .length = length,
                         .color = (color != 0u) ? args->palette->mark[color - 1u] : args->palette->text_3, .clip = met);
    }
    x += icon + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_ICON_GAP, .scale = scale);
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_EXPLORER_ROW_SIZE, .scale = scale);
    const GuiColor color =
        node->dir ? args->palette->text : (node->ignored ? args->palette->text_3 : args->palette->text_2);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)x,
                     .y = EMBED_CALL(explorer_baseline, ExplorerCtx, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                                     .size = size, .top = rect.y, .tall = rect.height),
                     .text = path + node->name, .length = node->length - node->name, .color = color, .clip = met);
}

void gui_explorer_draw(const GuiExplorerFrameXYCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const GuiRect side = args->frame->side;
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = side))
    {
        return;
    }
    const int one = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){side.x + side.width - one, side.y, one, side.height}, .color = args->palette->line);
    const GuiRect inside = {side.x, side.y, side.width - one, side.height};
    const int head = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_HEAD, .scale = scale);
    const unsigned int head_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                            .pixels = (int)GUI_EXPLORER_HEAD_SIZE, .scale = scale);
    (void)EMBED_CALL(
        gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs, .face = GUI_FACE_HEAD,
        .size = head_size,
        .x_f32 = (float)(side.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 15, .scale = scale)),
        .y = EMBED_CALL(explorer_baseline, ExplorerCtx, .glyphs = glyphs, .face = GUI_FACE_HEAD, .size = head_size,
                        .top = side.y, .tall = head),
        .text = args->title, .length = cellul.len(&(CatenaFinitaCfg){.src = args->title, .cap = 64ull}),
        .color = args->palette->text_2, .clip = inside);
    if (!args->folders)
    {
        return;
    }
    // the pane's line, its head with the arrow open and the tree's name, and its body on the editor's ground
    const int line_y = side.y + head;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){side.x, line_y, inside.width, one}, .other = inside),
               .color = args->palette->line);
    const int pane_tall =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_PANE_HEAD, .scale = scale);
    const int pane_y = line_y + one;
    const int arrow =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_EXPLORER_ARROW, .scale = scale);
    EMBED_CALL(explorer_arrow_draw, ExplorerCtx, .raster = raster,
               .x = side.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 7, .scale = scale),
               .y = pane_y + ((pane_tall - arrow) / 2), .side = arrow, .open = 1, .color = args->palette->text);
    (void)EMBED_CALL(
        gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs, .face = GUI_FACE_HEAD,
        .size = head_size,
        .x_f32 = (float)(side.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 19, .scale = scale)),
        .y = EMBED_CALL(explorer_baseline, ExplorerCtx, .glyphs = glyphs, .face = GUI_FACE_HEAD, .size = head_size,
                        .top = pane_y, .tall = pane_tall),
        .text = args->tree, .length = cellul.len(&(CatenaFinitaCfg){.src = args->tree, .cap = CLI_NAME_MAX_LEN}),
        .color = args->palette->text, .clip = inside);
    const int rows_top = EMBED_CALL(explorer_rows_top, ExplorerCtx, .frame = args->frame);
    const GuiRect body =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){side.x, rows_top, inside.width, side.y + side.height - rows_top}, .other = inside);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = body))
    {
        return;
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = body,
               .color = args->palette->editor_ground);
    const unsigned int rows = gui_explorer_frame_xy.rows(args);
    for (unsigned int row = args->explorer_read->scroll; row < rows; row += 1u)
    {
        unsigned int at = 0u;
        if (EMBED_CALL(gui_explorer_row_frame.row_rect, GuiExplorerRowFrameCfg, .frame = args->frame,
                       .explorer_read = args->explorer_read, .row = row)
                .y >= body.y + body.height)
        {
            break;
        }
        if (EMBED_CALL(cli_explorer_row.row, GuiExplorerRowCfg, .explorer_read = args->explorer_read, .row = row,
                       .node = &at))
        {
            EMBED_CALL(explorer_row_draw, ExplorerCtx, .explorer = args->explorer_read, .raster = raster,
                       .glyphs = glyphs, .icons = args->icons, .palette = args->palette, .frame = args->frame,
                       .row = row, .at = at, .clip = body, .shown = args->shown);
        }
    }
}
