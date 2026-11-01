// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// explorer.c: the explorer's nodes stand in the tree's order, each folder's entries after it one level deeper, and a
// row is a node whose folders above are all open; a folder's entries are kept once listed, shown and hidden as it
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

int gui_explorer_made(GuiExplorer *explorer)
{
    memor.set(&(MemoriaCfg){.dst = explorer, .val = 0, .bytes = sizeof(*explorer)});
    explorer->pointed = GUI_EXPLORER_NONE;
    explorer->node =
        g_cli_cells->persistent_buf_alloc(GUI_EXPLORER_NODES * sizeof(*explorer->node), &g_gui_explorer_cells);
    explorer->text = g_cli_cells->persistent_buf_alloc(GUI_EXPLORER_TEXT, &g_gui_explorer_cells);
    if ((explorer->node == NULL) || (explorer->text == NULL))
    {
        gui_explorer_given(explorer);
        return 0;
    }
    return 1;
}

void gui_explorer_given(GuiExplorer *explorer)
{
    g_cli_cells->persistent_buf_release(explorer->node, &g_gui_explorer_cells);
    g_cli_cells->persistent_buf_release(explorer->text, &g_gui_explorer_cells);
    explorer->node = NULL;
    explorer->text = NULL;
    explorer->nodes = 0u;
    explorer->used = 0ull;
}

void gui_explorer_emptied(GuiExplorer *explorer)
{
    explorer->nodes = 0u;
    explorer->used = 0ull;
    explorer->pointed = GUI_EXPLORER_NONE;
    explorer->scroll = 0u;
    explorer->root_listed = 0;
    explorer->root_ticket = 0ull;
}

// the node at `path`, or GUI_EXPLORER_NONE
static int explorer_node_at(const GuiExplorer *explorer, const char *path)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
    for (unsigned int at = 0u; at < explorer->nodes; at += 1u)
    {
        const GuiNode *const node = &explorer->node[at];
        if ((node->length == length) &&
            (memor.cmp(&(MemoriaCfg){.src = explorer->text + node->path, .other = path, .bytes = length}) == 0))
        {
            // the nodes are fewer than an int counts
            return (int)at;
        }
    }
    return GUI_EXPLORER_NONE;
}

void gui_explorer_listing(GuiExplorer *explorer, const char *dir, unsigned long long ticket)
{
    if (dir[0] == '\0')
    {
        explorer->root_ticket = ticket;
        return;
    }
    const int at = explorer_node_at(explorer, dir);
    if (at != GUI_EXPLORER_NONE)
    {
        explorer->node[at].ticket = ticket;
    }
}

int gui_explorer_root_asked(const GuiExplorer *explorer)
{
    return explorer->root_listed || (explorer->root_ticket != 0ull);
}

// a node for the entry `item` put at `at`, the nodes from there moved on by one, its path kept in the text: 1, or 0
// where the entry is not one or there is no room
static int explorer_node_put(GuiExplorer *explorer, const CliJson *json, unsigned int item, unsigned int at,
                             unsigned int depth)
{
    unsigned long long path_length = 0ull;
    const char *const path = cli_json_string(json, cli_json_member(json, item, "path"), &path_length);
    unsigned long long name_length = 0ull;
    const char *const name = cli_json_string(json, cli_json_member(json, item, "name"), &name_length);
    if ((path == NULL) || (path_length == 0ull) || (explorer->nodes >= GUI_EXPLORER_NODES) ||
        (explorer->used + path_length + 1ull > GUI_EXPLORER_TEXT))
    {
        return 0;
    }
    for (unsigned int each = explorer->nodes; each > at; each -= 1u)
    {
        explorer->node[each] = explorer->node[each - 1u];
    }
    GuiNode *const node = &explorer->node[at];
    memor.set(&(MemoriaCfg){.dst = node, .val = 0, .bytes = sizeof(*node)});
    memor.cpy(&(MemoriaCfg){.dst = explorer->text + explorer->used, .src = path, .bytes = path_length});
    explorer->text[explorer->used + path_length] = '\0';
    // the text is far inside what an unsigned int counts
    node->path = (unsigned int)explorer->used;
    node->length = (unsigned int)path_length;
    node->name = ((name != NULL) && (name_length <= path_length)) ? (unsigned int)(path_length - name_length) : 0u;
    node->depth = depth;
    node->dir = gui_answer_member_true(json, item, "dir");
    node->ignored = gui_answer_member_true(json, item, "ignored");
    explorer->used += path_length + 1ull;
    explorer->nodes += 1u;
    return 1;
}

int gui_explorer_listed(GuiExplorer *explorer, unsigned long long ticket, const CliJson *json, unsigned int root)
{
    if ((ticket == 0ull) || !cli_json_is(json, root, CFG_JSON_ARRAY))
    {
        return 0;
    }
    unsigned int at = explorer->nodes;
    unsigned int depth = 0u;
    if (ticket == explorer->root_ticket)
    {
        explorer->root_ticket = 0ull;
        explorer->root_listed = 1;
    }
    else
    {
        int parent = GUI_EXPLORER_NONE;
        for (unsigned int each = 0u; each < explorer->nodes; each += 1u)
        {
            if (explorer->node[each].ticket == ticket)
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
        explorer->node[parent].ticket = 0ull;
        explorer->node[parent].listed = 1;
        at = (unsigned int)parent + 1u;
        depth = explorer->node[parent].depth + 1u;
    }
    for (unsigned int each = 0u; each < json->node[root].count; each += 1u)
    {
        if (explorer_node_put(explorer, json, cli_json_item(json, root, each), at, depth))
        {
            at += 1u;
        }
    }
    return 1;
}

// the rows walked: the node of the row `wanted` into `node`, or the rows counted where `wanted` is past them
static unsigned int explorer_rows_walked(const GuiExplorer *explorer, unsigned int wanted, unsigned int *node)
{
    unsigned int rows = 0u;
    unsigned int hidden_below = GUI_EXPLORER_NODES;
    for (unsigned int at = 0u; at < explorer->nodes; at += 1u)
    {
        const GuiNode *const one = &explorer->node[at];
        if (one->depth > hidden_below)
        {
            continue;
        }
        hidden_below = (one->dir && !one->open) ? one->depth : GUI_EXPLORER_NODES;
        if (rows == wanted)
        {
            *node = at;
            return rows;
        }
        rows += 1u;
    }
    return rows;
}

unsigned int gui_explorer_rows(const GuiExplorer *explorer)
{
    unsigned int node = 0u;
    return explorer_rows_walked(explorer, GUI_EXPLORER_NODES, &node);
}

int gui_explorer_row(const GuiExplorer *explorer, unsigned int row, unsigned int *node)
{
    *node = GUI_EXPLORER_NODES;
    (void)explorer_rows_walked(explorer, row, node);
    return *node < GUI_EXPLORER_NODES;
}

const char *gui_explorer_path(const GuiExplorer *explorer, unsigned int node, unsigned long long *length)
{
    *length = explorer->node[node].length;
    return explorer->text + explorer->node[node].path;
}

// where the rows begin in the side, below the explorer's head and the pane's
static int explorer_rows_top(const GuiFrame *frame)
{
    return frame->side.y +
           gui_scaled(GUI_EXPLORER_HEAD + GUI_EXPLORER_PANE_LINE + GUI_EXPLORER_PANE_HEAD, frame->scale);
}

GuiRect gui_explorer_row_rect(const GuiFrame *frame, const GuiExplorer *explorer, unsigned int row)
{
    const int tall = gui_scaled(GUI_EXPLORER_ROW, frame->scale);
    // the rows and the scroll are far fewer than an int counts
    const int from_top = (int)row - (int)explorer->scroll;
    return (GuiRect){frame->side.x, explorer_rows_top(frame) + (from_top * tall),
                     frame->side.width - gui_scaled(1, frame->scale), tall};
}

int gui_explorer_hit(const GuiExplorer *explorer, const GuiFrame *frame, int x, int y, unsigned int *row)
{
    const int top = explorer_rows_top(frame);
    if (!gui_rect_holds(frame->side, x, y) || (y < top))
    {
        return 0;
    }
    // the pixel stands below the rows' top: the row counted is not negative
    const unsigned int at = (unsigned int)((y - top) / gui_scaled(GUI_EXPLORER_ROW, frame->scale)) + explorer->scroll;
    if (at >= gui_explorer_rows(explorer))
    {
        return 0;
    }
    *row = at;
    return 1;
}

int gui_explorer_moved(GuiExplorer *explorer, const GuiFrame *frame, int x, int y)
{
    unsigned int row = 0u;
    // the rows are far fewer than an int counts
    const int pointed = gui_explorer_hit(explorer, frame, x, y, &row) ? (int)row : GUI_EXPLORER_NONE;
    if (pointed == explorer->pointed)
    {
        return 0;
    }
    explorer->pointed = pointed;
    return 1;
}

int gui_explorer_pressed(GuiExplorer *explorer, const GuiFrame *frame, int x, int y, GuiPick *pick)
{
    pick->kind = GUI_PICK_NONE;
    pick->path[0] = '\0';
    if (!gui_rect_holds(frame->side, x, y))
    {
        return 0;
    }
    unsigned int row = 0u;
    unsigned int at = 0u;
    if (!gui_explorer_hit(explorer, frame, x, y, &row) || !gui_explorer_row(explorer, row, &at))
    {
        return 1;
    }
    GuiNode *const node = &explorer->node[at];
    VerbaLinea path = {pick->path, sizeof(pick->path), 0ull};
    mmgr_line_put_n(&path, explorer->text + node->path, node->length);
    (void)mmgr_line_finish(&path);
    if (!node->dir)
    {
        pick->kind = GUI_PICK_OPEN;
        return 1;
    }
    node->open = !node->open;
    if (node->open && !node->listed && (node->ticket == 0ull))
    {
        pick->kind = GUI_PICK_LIST;
    }
    return 1;
}

int gui_explorer_wheeled(GuiExplorer *explorer, const GuiFrame *frame, int rows)
{
    const int tall = gui_scaled(GUI_EXPLORER_ROW, frame->scale);
    const int room = frame->side.y + frame->side.height - explorer_rows_top(frame);
    const int shown = (tall > 0) ? room / tall : 0;
    // the rows are far fewer than an int counts
    const int count = (int)gui_explorer_rows(explorer);
    const int most = (count > shown) ? count - shown : 0;
    const int wanted = (int)explorer->scroll + rows;
    const int clamped = (wanted < 0) ? 0 : ((wanted > most) ? most : wanted);
    // the scroll clamped is not negative
    if ((unsigned int)clamped == explorer->scroll)
    {
        return 0;
    }
    explorer->scroll = (unsigned int)clamped;
    return 1;
}

static int explorer_lower(int byte)
{
    return ((byte >= 'A') && (byte <= 'Z')) ? byte - 'A' + 'a' : byte;
}

// the mark of the file named by `length` bytes of `name`: its text into `text`, sized EXPLORER_NAME_MOST, and the
// mark's color, 0 for the quieter one
static unsigned int explorer_marked(const char *name, unsigned long long length, char *text)
{
    char lower[EXPLORER_NAME_MOST];
    const unsigned long long kept = (length < EXPLORER_NAME_MOST - 1ull) ? length : EXPLORER_NAME_MOST - 1ull;
    for (unsigned long long at = 0ull; at < kept; at += 1ull)
    {
        lower[at] = (char)explorer_lower(name[at]);
    }
    lower[kept] = '\0';
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
    unsigned long long dot = kept;
    for (unsigned long long at = 1ull; at < kept; at += 1ull)
    {
        dot = (lower[at] == '.') ? at : dot;
    }
    const char *const type = (dot < kept) ? lower + dot + 1ull : "";
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
    from += cli_text_past_any(from, 0ull, cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}), ".");
    VerbaLinea line = {text, EXPLORER_NAME_MOST, 0ull};
    mmgr_line_put_n(&line, from,
                    (cellul.len(&(CatenaFinitaCfg){.src = from, .cap = EXPLORER_NAME_MOST}) < 2ull) ? 1ull : 2ull);
    (void)mmgr_line_finish(&line);
    return 0u;
}

// an arrow drawn in a box at x, y of `side` pixels, pointing right where closed and down where open
static void explorer_arrow_drawn(GuiRaster *raster, int x, int y, int side, int open, GuiColor color)
{
    const int half = side / 2;
    if (open)
    {
        gui_raster_line(raster, x, y + 1, x + half, y + half + 1, color);
        gui_raster_line(raster, x + half, y + half + 1, x + side - 1, y + 1, color);
    }
    else
    {
        gui_raster_line(raster, x + 1, y, x + half + 1, y + half, color);
        gui_raster_line(raster, x + half + 1, y + half, x + 1, y + side - 1, color);
    }
}

// a baseline that centers a face's letters in a row from `top`, `tall` pixels
static int explorer_baseline(GuiGlyphs *glyphs, unsigned int face, unsigned int size, int top, int tall)
{
    const float ascent = gui_glyphs_ascent(glyphs, face, size);
    const float descent = gui_glyphs_descent(glyphs, face, size);
    return top + (int)((((float)tall - ascent - descent) / 2.0f) + ascent + 0.5f);
}

// one row drawn: its ground where pointed or shown, its guides, its arrow, its icon or mark, and its name
static void explorer_row_drawn(const GuiExplorer *explorer, GuiRaster *raster, GuiGlyphs *glyphs, GuiIcons *icons,
                               const GuiPalette *palette, const GuiFrame *frame, unsigned int row, unsigned int at,
                               GuiRect clip, const char *shown)
{
    const float scale = frame->scale;
    const GuiNode *const node = &explorer->node[at];
    const GuiRect rect = gui_explorer_row_rect(frame, explorer, row);
    const GuiRect met = gui_rect_met(rect, clip);
    if (gui_rect_empty(met))
    {
        return;
    }
    const char *const path = explorer->text + node->path;
    const int current = (shown != NULL) &&
                        (cellul.len(&(CatenaFinitaCfg){.src = shown, .cap = CLI_PATH_MAX_LEN}) == node->length) &&
                        (memor.cmp(&(MemoriaCfg){.src = shown, .other = path, .bytes = node->length}) == 0);
    // the rows are far fewer than an int counts
    if (explorer->pointed == (int)row)
    {
        gui_raster_filled(raster, met, palette->line);
    }
    else if (current)
    {
        gui_raster_filled(raster, met, palette->selection_idle);
    }
    const int guide = gui_scaled(GUI_EXPLORER_GUIDE, scale);
    int x = rect.x + gui_scaled(GUI_EXPLORER_PAD_LEFT, scale);
    for (unsigned int level = 0u; level < node->depth; level += 1u)
    {
        gui_raster_filled(
            raster, gui_rect_met((GuiRect){x + gui_scaled(7, scale), rect.y, gui_scaled(1, scale), rect.height}, clip),
            palette->line);
        x += guide;
    }
    const int twisty = gui_scaled(GUI_EXPLORER_TWISTY, scale);
    if (node->dir)
    {
        const int arrow = gui_scaled(GUI_EXPLORER_ARROW, scale);
        explorer_arrow_drawn(raster, x + ((twisty - arrow) / 2), rect.y + ((rect.height - arrow) / 2), arrow,
                             node->open, palette->text_2);
    }
    x += twisty;
    const int icon = gui_scaled(GUI_EXPLORER_ICON, scale);
    if (node->dir)
    {
        const unsigned int size = (unsigned int)gui_scaled(14, scale);
        const GuiMask *const mask = gui_icon(icons, "folder", size, GUI_ICON_STROKE * (float)size / GUI_ICON_UNITS);
        if (mask != NULL)
        {
            // the icon's side is far inside what an int represents
            gui_raster_masked(raster, mask, x + ((icon - (int)size) / 2), rect.y + ((rect.height - (int)size) / 2),
                              palette->link, met);
        }
    }
    else
    {
        char text[EXPLORER_NAME_MOST];
        const unsigned int color = explorer_marked(path + node->name, node->length - node->name, text);
        const unsigned int size = (unsigned int)gui_scaled((int)GUI_EXPLORER_MARK_SIZE, scale);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = EXPLORER_NAME_MOST});
        const float wide = gui_text_width(glyphs, GUI_FACE_CODE_BOLD, size, text, length);
        (void)gui_text_drawn(raster, glyphs, GUI_FACE_CODE_BOLD, size, (float)x + (((float)icon - wide) / 2.0f),
                             explorer_baseline(glyphs, GUI_FACE_CODE_BOLD, size, rect.y, rect.height), text, length,
                             (color != 0u) ? palette->mark[color - 1u] : palette->text_3, met);
    }
    x += icon + gui_scaled(GUI_EXPLORER_ICON_GAP, scale);
    const unsigned int size = (unsigned int)gui_scaled((int)GUI_EXPLORER_ROW_SIZE, scale);
    const GuiColor color = node->dir ? palette->text : (node->ignored ? palette->text_3 : palette->text_2);
    (void)gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, size, (float)x,
                         explorer_baseline(glyphs, GUI_FACE_HEAD, size, rect.y, rect.height), path + node->name,
                         node->length - node->name, color, met);
}

void gui_explorer_drawn(const GuiExplorer *explorer, GuiRaster *raster, GuiGlyphs *glyphs, GuiIcons *icons,
                        const GuiPalette *palette, const GuiFrame *frame, const char *title, int folders,
                        const char *tree, const char *shown)
{
    const float scale = frame->scale;
    const GuiRect side = frame->side;
    if (gui_rect_empty(side))
    {
        return;
    }
    const int one = gui_scaled(1, scale);
    gui_raster_filled(raster, (GuiRect){side.x + side.width - one, side.y, one, side.height}, palette->line);
    const GuiRect inside = {side.x, side.y, side.width - one, side.height};
    const int head = gui_scaled(GUI_EXPLORER_HEAD, scale);
    const unsigned int head_size = (unsigned int)gui_scaled((int)GUI_EXPLORER_HEAD_SIZE, scale);
    (void)gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, head_size, (float)(side.x + gui_scaled(15, scale)),
                         explorer_baseline(glyphs, GUI_FACE_HEAD, head_size, side.y, head), title,
                         cellul.len(&(CatenaFinitaCfg){.src = title, .cap = 64ull}), palette->text_2, inside);
    if (!folders)
    {
        return;
    }
    // the pane's line, its head with the arrow open and the tree's name, and its body on the editor's ground
    const int line_y = side.y + head;
    gui_raster_filled(raster, gui_rect_met((GuiRect){side.x, line_y, inside.width, one}, inside), palette->line);
    const int pane_tall = gui_scaled(GUI_EXPLORER_PANE_HEAD, scale);
    const int pane_y = line_y + one;
    const int arrow = gui_scaled(GUI_EXPLORER_ARROW, scale);
    explorer_arrow_drawn(raster, side.x + gui_scaled(7, scale), pane_y + ((pane_tall - arrow) / 2), arrow, 1,
                         palette->text);
    (void)gui_text_drawn(raster, glyphs, GUI_FACE_HEAD, head_size, (float)(side.x + gui_scaled(19, scale)),
                         explorer_baseline(glyphs, GUI_FACE_HEAD, head_size, pane_y, pane_tall), tree,
                         cellul.len(&(CatenaFinitaCfg){.src = tree, .cap = CLI_NAME_MAX_LEN}), palette->text, inside);
    const int rows_top = explorer_rows_top(frame);
    const GuiRect body =
        gui_rect_met((GuiRect){side.x, rows_top, inside.width, side.y + side.height - rows_top}, inside);
    if (gui_rect_empty(body))
    {
        return;
    }
    gui_raster_filled(raster, body, palette->editor_ground);
    const unsigned int rows = gui_explorer_rows(explorer);
    for (unsigned int row = explorer->scroll; row < rows; row += 1u)
    {
        unsigned int at = 0u;
        if (gui_explorer_row_rect(frame, explorer, row).y >= body.y + body.height)
        {
            break;
        }
        if (gui_explorer_row(explorer, row, &at))
        {
            explorer_row_drawn(explorer, raster, glyphs, icons, palette, frame, row, at, body, shown);
        }
    }
}
