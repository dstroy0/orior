// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// preview.c: the preview's state, one for the window: the mode, the view it is bound to with its path in a cell sized
// to the two, the text last laid out in a cell of its own, the items laid from it, the width they were laid for and the
// rectangle's height, the scroll top, when the text was last looked at, the view's top and cursor's line last seen, and
// the block marked. A look reads the view's text through the document store and lays it out again only where it
// differs from the text last laid; blocks are found by a binary search over their starts, sorted by top and by line as
// the layout lays them in the source's order
#include "preview.h"

#include "../editor/document.h"
#include "../editor/view.h"
#include "../machine/report.h"
#include "preview_markdown.h"

// a scale's whole, in percent; a line's fraction's whole; and no block
#define PREVIEW_SCALE_ONE 100
#define PREVIEW_FRACTION 65536ULL
#define PREVIEW_NONE_AT 0xffffffffu

// A path's ending and the type of preview it has
typedef struct
{
    const char *ending;
    unsigned int type;
} PreviewEnding;

// the endings of preview.js TYPES, in its order
static const PreviewEnding s_endings[] = {
    {"md", CLI_PREVIEW_MARKDOWN},   {"markdown", CLI_PREVIEW_MARKDOWN}, {"mdown", CLI_PREVIEW_MARKDOWN},
    {"mkd", CLI_PREVIEW_MARKDOWN},  {"mdx", CLI_PREVIEW_MARKDOWN},      {"html", CLI_PREVIEW_HTML},
    {"htm", CLI_PREVIEW_HTML},      {"xhtml", CLI_PREVIEW_HTML},        {"css", CLI_PREVIEW_CSS},
    {"svg", CLI_PREVIEW_SVG},       {"mmd", CLI_PREVIEW_MERMAID},       {"mermaid", CLI_PREVIEW_MERMAID},
    {"tex", CLI_PREVIEW_LATEX},     {"latex", CLI_PREVIEW_LATEX},       {"ltx", CLI_PREVIEW_LATEX},
    {"http", CLI_PREVIEW_REQUESTS}, {"rest", CLI_PREVIEW_REQUESTS},
};

// the modes' words, as the setting saves them and the command reads them, in the modes' order
static const char *const s_mode_words[3] = {"off", "beside", "alone"};

// The preview: its mode; the type of the view bound; the cell of the view's key and its path, each with its end, NULL
// while none is bound; the text last laid with its bytes, in a cell of its own; the items laid from it; whether they
// stand for it; the width they were laid for and the rectangle's height, in the page's pixels; the scroll top; the
// millisecond the text was last looked at; the view's top last seen; whether the cursor's line was seen and which it
// was; and the block marked, PREVIEW_NONE_AT for none
typedef struct
{
    unsigned int mode;
    unsigned int type;
    char *bound;
    const char *key;
    const char *path;
    char *source;
    unsigned long long source_length;
    CliPreviewItems items;
    int laid;
    int width_px;
    int height_px;
    int top_px;
    unsigned long long looked_ms;
    unsigned long long editor_top;
    int cursor_seen;
    unsigned long long cursor_line;
    unsigned int marked;
} Preview;

static Preview s_preview = {.marked = PREVIEW_NONE_AT};

typedef struct
{
    const char *key;
    const char *path;
    int top_px;
    unsigned long long line;
    unsigned int mode;
    const char *text;
    unsigned int length;
    CliPreviewOpen *open_out;
    unsigned int at;
} PreviewCtx;

// the items let go with the text they were laid from, none laid and none marked
EMBED_INLINE void items_released(const PreviewCtx *args)
{
    (void)args;
    g_cli_guard->persistent_buf_release(s_preview.items.item, g_cli_preview_owner);
    g_cli_guard->persistent_buf_release(s_preview.items.text, g_cli_preview_owner);
    g_cli_guard->persistent_buf_release(s_preview.items.point, g_cli_preview_owner);
    s_preview.items = (CliPreviewItems){0};
    g_cli_guard->persistent_buf_release(s_preview.source, g_cli_preview_owner);
    s_preview.source = NULL;
    s_preview.source_length = 0ULL;
    s_preview.laid = 0;
    s_preview.marked = PREVIEW_NONE_AT;
}

// `top_px` no higher than the first item and no lower than the last fills the rectangle
EMBED_INLINE int top_clamped(const PreviewCtx *args)
{
    const int most = s_preview.items.height - s_preview.height_px;
    const int lowest = (most > 0) ? most : 0;
    if (args->top_px < 0)
    {
        return 0;
    }
    return (args->top_px > lowest) ? lowest : args->top_px;
}

// the last block whose top is at most `top_px`, PREVIEW_NONE_AT where none is
EMBED_INLINE unsigned int block_by_top(const PreviewCtx *args)
{
    const CliPreviewBlock *const block = s_preview.items.block;
    const unsigned int count = s_preview.items.blocks;
    if ((count == 0u) || (block[0].top > args->top_px))
    {
        return PREVIEW_NONE_AT;
    }
    unsigned int low = 0u;
    unsigned int high = count;
    while ((high - low) > 1u)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (block[middle].top <= args->top_px)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// the last block whose line is at most `line`, PREVIEW_NONE_AT where none is
EMBED_INLINE unsigned int block_by_line(const PreviewCtx *args)
{
    const CliPreviewBlock *const block = s_preview.items.block;
    const unsigned int count = s_preview.items.blocks;
    if ((count == 0u) || (block[0].line > args->line))
    {
        return PREVIEW_NONE_AT;
    }
    unsigned int low = 0u;
    unsigned int high = count;
    while ((high - low) > 1u)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (block[middle].line <= args->line)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// the top put at `top_px`, bounded, and in the beside mode the source line at it handed to the bound view as its
// scroll, the view's top read back so the follow leads nothing
EMBED_INLINE void top_led(const PreviewCtx *args)
{
    s_preview.top_px = EMBED_CALL(top_clamped, PreviewCtx, .top_px = args->top_px);
    if ((s_preview.mode != CLI_PREVIEW_BESIDE) || (s_preview.key == NULL))
    {
        return;
    }
    const CliViewInput scrolled = {
        .type = CLI_VIEW_SCROLL,
        .top = EMBED_CALL(cli_preview_scroll.line_at, CliPreviewScrollCfg, .top_px = s_preview.top_px)};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = s_preview.key, .input = &scrolled,
                     .err_msg = &err_msg);
    s_preview.editor_top = cli_view_top(s_preview.key);
}

// the slug of the letter `at` of a heading's text as GitHub writes an anchor, a letter or digit lowered, a space a
// dash, a dash or an underscore itself: the slug's letter, 0 where it has none
EMBED_INLINE unsigned char slug_letter(const PreviewCtx *args)
{
    const unsigned char one = (unsigned char)args->text[args->at];
    if ((one >= 'A') && (one <= 'Z'))
    {
        // a capital lowered stays a byte
        return (unsigned char)(one + 32u);
    }
    if (((one >= 'a') && (one <= 'z')) || ((one >= '0') && (one <= '9')) || (one == '-') || (one == '_') ||
        (one >= 0x80u))
    {
        return one;
    }
    return (one == ' ') ? (unsigned char)'-' : 0u;
}

// the heading block whose text's slug is the `length` bytes of `text`: the block, PREVIEW_NONE_AT where none is
EMBED_INLINE unsigned int heading_of(const PreviewCtx *args)
{
    const CliPreviewItems *const items = &s_preview.items;
    for (unsigned int block = 0u; block < items->blocks; block += 1u)
    {
        if (!items->block[block].heading)
        {
            continue;
        }
        const unsigned int end = (block + 1u < items->blocks) ? items->block[block + 1u].item : items->items;
        unsigned int matched = 0u;
        int same = 1;
        for (unsigned int at = items->block[block].item + 1u; same && (at < end); at += 1u)
        {
            const CliPreviewItem *const item = &items->item[at];
            for (unsigned int letter = 0u; same && (item->type == CLI_PREVIEW_ITEM_TEXT) && (letter < item->length);
                 letter += 1u)
            {
                const unsigned char slug =
                    EMBED_CALL(slug_letter, PreviewCtx, .text = items->text + item->text, .at = letter);
                if (slug == 0u)
                {
                    continue;
                }
                same = (matched < args->length) && ((unsigned char)args->text[matched] == slug);
                matched += 1u;
            }
        }
        if (same && (matched == args->length))
        {
            return block;
        }
    }
    return PREVIEW_NONE_AT;
}

// whether the address `text`, `length` bytes, leaves the tree: a scheme, letters then a colon, or `//` at its start
EMBED_INLINE int address_out(const PreviewCtx *args)
{
    if ((args->length >= 2u) && (args->text[0] == '/') && (args->text[1] == '/'))
    {
        return 1;
    }
    for (unsigned int at = 0u; at < args->length; at += 1u)
    {
        const unsigned char one = (unsigned char)args->text[at];
        if (one == ':')
        {
            return at != 0u;
        }
        const int scheme =
            ((one >= 'a') && (one <= 'z')) || ((one >= 'A') && (one <= 'Z')) ||
            ((at != 0u) && (((one >= '0') && (one <= '9')) || (one == '+') || (one == '.') || (one == '-')));
        if (!scheme)
        {
            return 0;
        }
    }
    return 0;
}

// the address `text`, `length` bytes, of a file of the tree read against the bound file's folder, its `#` part left
// off, its `.` and `..` parts walked, into `open_out`'s path: 1, or 0 where it does not fit
EMBED_INLINE int path_resolved(const PreviewCtx *args)
{
    char *const path = args->open_out->path;
    unsigned int used = 0u;
    const char *const bound = (s_preview.path != NULL) ? s_preview.path : "";
    const unsigned long long bound_length = cellul.len(&(CatenaFinitaCfg){.src = bound, .cap = CLI_PATH_MAX_LEN});
    unsigned int folder = 0u;
    for (unsigned int at = 0u; at < bound_length; at += 1u)
    {
        folder = (bound[at] == '/') ? at : folder;
    }
    const int rooted = (args->length != 0u) && (args->text[0] == '/');
    // the folder and the address are both read part by part, `/` between, the folder's parts first
    for (unsigned int side = rooted ? 1u : 0u; side < 2u; side += 1u)
    {
        const char *const from = (side == 0u) ? bound : args->text;
        unsigned int end = args->length;
        if (side == 0u)
        {
            end = (folder != 0u) ? folder : 0u;
        }
        for (unsigned int at = 0u; (side == 1u) && (at < end); at += 1u)
        {
            end = (from[at] == '#') ? at : end;
        }
        unsigned int part = 0u;
        for (unsigned int at = 0u; at <= end; at += 1u)
        {
            if ((at < end) && (from[at] != '/'))
            {
                continue;
            }
            const unsigned int length = at - part;
            const int dot = (length == 1u) && (from[part] == '.');
            const int up = (length == 2u) && (from[part] == '.') && (from[part + 1u] == '.');
            if (up)
            {
                while ((used > 0u) && (path[used - 1u] != '/'))
                {
                    used -= 1u;
                }
                used -= (used > 0u) ? 1u : 0u;
            }
            else if ((length != 0u) && !dot)
            {
                if (used + length + 2u > CLI_PATH_MAX_LEN)
                {
                    return 0;
                }
                if (used != 0u)
                {
                    path[used] = '/';
                    used += 1u;
                }
                memor.cpy(&(MemoriaCfg){.dst = path + used, .src = from + part, .bytes = length});
                used += length;
            }
            part = at + 1u;
        }
    }
    path[used] = '\0';
    return used != 0u;
}

// the address `text`, `length` bytes, of a link pressed resolved into `open_out`: a place of the preview scrolled to,
// an address out of the tree opened in the reader's browser, or a file of the tree: the target
EMBED_INLINE unsigned int address_followed(const PreviewCtx *args)
{
    CliPreviewOpen *const open = args->open_out;
    if (args->text[0] == '#')
    {
        const unsigned int block =
            EMBED_CALL(heading_of, PreviewCtx, .text = args->text + 1, .length = args->length - 1u);
        if (block == PREVIEW_NONE_AT)
        {
            return CLI_PREVIEW_OPEN_NONE;
        }
        EMBED_CALL(top_led, PreviewCtx, .top_px = s_preview.items.block[block].top);
        open->top_px = s_preview.top_px;
        open->target = CLI_PREVIEW_OPEN_PLACE;
        return open->target;
    }
    if (EMBED_CALL(address_out, PreviewCtx, .text = args->text, .length = args->length))
    {
        // the address with its end, in a cell for the opening alone
        char *const address = g_cli_guard->temporary_buf_alloc(args->length + 1ULL, g_cli_preview_owner);
        if (address == NULL)
        {
            return CLI_PREVIEW_OPEN_NONE;
        }
        memor.cpy(&(MemoriaCfg){.dst = address, .src = args->text, .bytes = args->length});
        address[args->length] = '\0';
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        const int opened =
            EMBED_CALL(cli_report_err_msg.open_url, CliReportErrMsgCfg, .url = address, .err_msg = &err_msg);
        g_cli_guard->persistent_buf_release(address, g_cli_preview_owner);
        open->target = opened ? CLI_PREVIEW_OPEN_OUT : CLI_PREVIEW_OPEN_NONE;
        return open->target;
    }
    open->target = EMBED_CALL(path_resolved, PreviewCtx, .text = args->text, .length = args->length, .open_out = open)
                       ? CLI_PREVIEW_OPEN_FILE
                       : CLI_PREVIEW_OPEN_NONE;
    return open->target;
}

void cli_preview_begin(const CliPreviewCfg *args)
{
    const char *const saved =
        (args->entries != NULL) ? cli_state_entry_value(args->entries, CLI_PREVIEW_MODE_KEY) : NULL;
    s_preview.mode = CLI_PREVIEW_OFF;
    for (unsigned int mode = 0u; (saved != NULL) && (mode < 3u); mode += 1u)
    {
        s_preview.mode = cellul.eq(&(CatenaFinitaCfg){.src = saved, .other = s_mode_words[mode], .cap = 16ULL})
                             ? mode
                             : s_preview.mode;
    }
}

unsigned int cli_preview_type(const CliPreviewCfg *args)
{
    const char *const path = (args->path != NULL) ? args->path : "";
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
    unsigned long long dot = length;
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        dot = (path[at] == '.') ? at : ((path[at] == '/') ? length : dot);
    }
    char ending[16];
    unsigned int ending_length = 0u;
    for (unsigned long long at = dot + 1ULL; (at < length) && (ending_length + 1u < sizeof(ending)); at += 1ULL)
    {
        const char one = path[at];
        // a capital lowered stays a byte
        ending[ending_length] = ((one >= 'A') && (one <= 'Z')) ? (char)(one + 32) : one;
        ending_length += 1u;
    }
    ending[ending_length] = '\0';
    if ((dot >= length) || (ending_length == 0u))
    {
        return CLI_PREVIEW_NONE;
    }
    for (unsigned int at = 0u; at < sizeof(s_endings) / sizeof(s_endings[0]); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = ending, .other = s_endings[at].ending, .cap = sizeof(ending)}))
        {
            return s_endings[at].type;
        }
    }
    return CLI_PREVIEW_NONE;
}

unsigned int cli_preview_mode_get(const CliPreviewCfg *args)
{
    (void)args;
    return s_preview.mode;
}

void cli_preview_mode_set(const CliPreviewCfg *args)
{
    unsigned int mode = (s_preview.mode == args->mode) ? CLI_PREVIEW_OFF : args->mode;
    for (unsigned int each = 0u; (args->word != NULL) && (each < 3u); each += 1u)
    {
        mode =
            cellul.eq(&(CatenaFinitaCfg){.src = args->word, .other = s_mode_words[each], .cap = 16ULL}) ? each : mode;
    }
    s_preview.mode = (mode < 3u) ? mode : CLI_PREVIEW_OFF;
    // the folder it is saved in and the error line, for the save alone
    char *const folder = g_cli_guard->temporary_buf_alloc(CLI_PATH_MAX_LEN + CLI_ERR_MSG_MAX_LEN, g_cli_preview_owner);
    if (folder == NULL)
    {
        return;
    }
    VerbaLinea err_msg = {folder + CLI_PATH_MAX_LEN, CLI_ERR_MSG_MAX_LEN, 0ULL};
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        (void)cli_state_value_save(&(CliStateValueCfg){
            .folder = folder, .key = CLI_PREVIEW_MODE_KEY, .value = s_mode_words[s_preview.mode], .err_msg = &err_msg});
    }
    g_cli_guard->persistent_buf_release(folder, g_cli_preview_owner);
}

int cli_preview_show(const CliPreviewCfg *args)
{
    char path[CLI_PATH_MAX_LEN];
    const int viewed = (args->key != NULL) &&
                       EMBED_CALL(cli_view_path_type.path, CliViewPathTypeCfg, .key = args->key, .path_out = path);
    if (!viewed)
    {
        const int was = s_preview.bound != NULL;
        EMBED_CALL(items_released, PreviewCtx, .key = NULL);
        g_cli_guard->persistent_buf_release(s_preview.bound, g_cli_preview_owner);
        s_preview.bound = NULL;
        s_preview.key = NULL;
        s_preview.path = NULL;
        s_preview.type = CLI_PREVIEW_NONE;
        s_preview.top_px = 0;
        return was;
    }
    if ((s_preview.bound != NULL) &&
        cellul.eq(&(CatenaFinitaCfg){.src = s_preview.key, .other = args->key, .cap = CLI_PATH_MAX_LEN}) &&
        cellul.eq(&(CatenaFinitaCfg){.src = s_preview.path, .other = path, .cap = CLI_PATH_MAX_LEN}))
    {
        return 0;
    }
    const unsigned long long key_bytes =
        cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long path_bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    char *const bound = g_cli_guard->persistent_buf_alloc(key_bytes + path_bytes, g_cli_preview_owner);
    if (bound == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = bound, .src = args->key, .bytes = key_bytes});
    memor.cpy(&(MemoriaCfg){.dst = bound + key_bytes, .src = path, .bytes = path_bytes});
    EMBED_CALL(items_released, PreviewCtx, .key = NULL);
    g_cli_guard->persistent_buf_release(s_preview.bound, g_cli_preview_owner);
    s_preview.bound = bound;
    s_preview.key = bound;
    s_preview.path = bound + key_bytes;
    s_preview.type = EMBED_CALL(cli_preview.type, CliPreviewCfg, .path = s_preview.path);
    s_preview.top_px = 0;
    s_preview.looked_ms = 0ULL;
    s_preview.editor_top = cli_view_top(s_preview.key);
    s_preview.cursor_seen = 0;
    return 1;
}

int cli_preview_step(const CliPreviewCfg *args)
{
    int redrawn = EMBED_CALL(cli_preview.show, CliPreviewCfg, .key = args->key);
    if ((s_preview.mode == CLI_PREVIEW_OFF) || (s_preview.type != CLI_PREVIEW_MARKDOWN) || (s_preview.key == NULL))
    {
        return redrawn;
    }
    const int scale = (args->scale > 0) ? args->scale : PREVIEW_SCALE_ONE;
    // a rectangle's sides in the page's pixels are smaller than in the frame's at any scale past a whole
    const int width = (int)(((long long)args->width_px * PREVIEW_SCALE_ONE) / scale);
    const int height = (int)(((long long)args->height_px * PREVIEW_SCALE_ONE) / scale);
    s_preview.height_px = height;
    if (width != s_preview.width_px)
    {
        s_preview.width_px = width;
        s_preview.laid = 0;
    }
    if (width <= 0)
    {
        return redrawn;
    }
    if (!s_preview.laid || (args->now_ms >= s_preview.looked_ms + CLI_PREVIEW_LOOK_MS))
    {
        s_preview.looked_ms = args->now_ms;
        unsigned long long size = 0ULL;
        char *const text = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = s_preview.key,
                                      .size_pointer = &size, .owner = g_cli_preview_owner);
        const int same =
            (text != NULL) && s_preview.laid && (size == s_preview.source_length) &&
            ((size == 0ULL) || (memor.cmp(&(MemoriaCfg){.src = text, .other = s_preview.source, .bytes = size}) == 0));
        CliPreviewItems fresh = {0};
        if ((text != NULL) && !same &&
            EMBED_CALL(cli_preview_markdown.layout, CliPreviewMarkdownCfg, .text = text, .length = size,
                       .first_line = 0u, .width_px = width, .items = &fresh))
        {
            EMBED_CALL(items_released, PreviewCtx, .key = NULL);
            s_preview.items = fresh;
            s_preview.source = text;
            s_preview.source_length = size;
            s_preview.laid = 1;
            s_preview.cursor_seen = 0;
            s_preview.top_px = EMBED_CALL(top_clamped, PreviewCtx, .top_px = s_preview.top_px);
            redrawn = 1;
        }
        else
        {
            g_cli_guard->persistent_buf_release(text, g_cli_preview_owner);
        }
    }
    // the editor's top moved by the editor: the preview follows it to the same place between two blocks
    const unsigned long long editor_top = cli_view_top(s_preview.key);
    if (editor_top != s_preview.editor_top)
    {
        s_preview.editor_top = editor_top;
        s_preview.top_px =
            EMBED_CALL(top_clamped, PreviewCtx,
                       .top_px = EMBED_CALL(cli_preview_scroll.top_of, CliPreviewScrollCfg, .line = editor_top));
        redrawn = 1;
    }
    CliPosition cursor = {0ULL, 0ULL};
    if (EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = s_preview.key, .cursor = &cursor) &&
        (!s_preview.cursor_seen || (cursor.line != s_preview.cursor_line)))
    {
        s_preview.cursor_seen = 1;
        s_preview.cursor_line = cursor.line;
        redrawn = EMBED_CALL(cli_preview_mark.mark, CliPreviewMarkCfg, .line = cursor.line) || redrawn;
    }
    return redrawn;
}

unsigned long long cli_preview_deadline_ms(const CliPreviewCfg *args)
{
    (void)args;
    return ((s_preview.mode != CLI_PREVIEW_OFF) && (s_preview.type == CLI_PREVIEW_MARKDOWN) && (s_preview.key != NULL))
               ? s_preview.looked_ms + CLI_PREVIEW_LOOK_MS
               : 0ULL;
}

unsigned int cli_preview_item_count(const CliPreviewItemCfg *args)
{
    (void)args;
    return s_preview.items.items;
}

const CliPreviewItem *cli_preview_item_row(const CliPreviewItemCfg *args)
{
    if (args->at >= s_preview.items.items)
    {
        return NULL;
    }
    const CliPreviewItem *const item = &s_preview.items.item[args->at];
    if (args->text_out != NULL)
    {
        *args->text_out = (s_preview.items.text != NULL) ? s_preview.items.text + item->text : "";
    }
    return item;
}

unsigned int cli_preview_item_first(const CliPreviewItemCfg *args)
{
    unsigned int block = EMBED_CALL(block_by_top, PreviewCtx, .top_px = args->top_px);
    if (block == PREVIEW_NONE_AT)
    {
        return 0u;
    }
    while ((block > 0u) && (s_preview.items.block[block].depth > 0u))
    {
        block -= 1u;
    }
    return s_preview.items.block[block].item;
}

int cli_preview_item_extent(const CliPreviewItemCfg *args)
{
    (void)args;
    return s_preview.items.height;
}

int cli_preview_scroll_by(const CliPreviewScrollCfg *args)
{
    const int top = EMBED_CALL(top_clamped, PreviewCtx, .top_px = s_preview.top_px + args->delta_px);
    if (top == s_preview.top_px)
    {
        return 0;
    }
    EMBED_CALL(top_led, PreviewCtx, .top_px = top);
    return 1;
}

int cli_preview_scroll_top(const CliPreviewScrollCfg *args)
{
    (void)args;
    return s_preview.top_px;
}

unsigned long long cli_preview_scroll_line_at(const CliPreviewScrollCfg *args)
{
    if (args->fraction_out != NULL)
    {
        *args->fraction_out = 0u;
    }
    const unsigned int at = EMBED_CALL(block_by_top, PreviewCtx, .top_px = args->top_px);
    if (at == PREVIEW_NONE_AT)
    {
        return (s_preview.items.blocks != 0u) ? s_preview.items.block[0].line : 0ULL;
    }
    const CliPreviewBlock *const here = &s_preview.items.block[at];
    if ((at + 1u >= s_preview.items.blocks) || (s_preview.items.block[at + 1u].top <= here->top))
    {
        return here->line;
    }
    const CliPreviewBlock *const next = &s_preview.items.block[at + 1u];
    // the pixels past the block's top are fewer than to the next block's, and the lines between the two a text's
    const unsigned long long share = ((unsigned long long)(args->top_px - here->top) *
                                      (unsigned long long)(next->line - here->line) * PREVIEW_FRACTION) /
                                     (unsigned long long)(next->top - here->top);
    if (args->fraction_out != NULL)
    {
        // a fraction is less than the whole, below 65536
        *args->fraction_out = (unsigned int)(share % PREVIEW_FRACTION);
    }
    return here->line + (share / PREVIEW_FRACTION);
}

int cli_preview_scroll_top_of(const CliPreviewScrollCfg *args)
{
    const unsigned int at = EMBED_CALL(block_by_line, PreviewCtx, .line = args->line);
    if (at == PREVIEW_NONE_AT)
    {
        return 0;
    }
    const CliPreviewBlock *const here = &s_preview.items.block[at];
    if ((at + 1u >= s_preview.items.blocks) || (s_preview.items.block[at + 1u].line <= here->line))
    {
        return here->top;
    }
    const CliPreviewBlock *const next = &s_preview.items.block[at + 1u];
    const unsigned long long past = ((args->line - here->line) * PREVIEW_FRACTION) + args->line_fraction;
    const unsigned long long span = (unsigned long long)(next->line - here->line) * PREVIEW_FRACTION;
    // the share of the pixels between the two blocks is no more than they are, an int
    return here->top + (int)((past * (unsigned long long)(next->top - here->top)) / span);
}

int cli_preview_mark_line(const CliPreviewMarkCfg *args)
{
    const unsigned int block = EMBED_CALL(block_by_line, PreviewCtx, .line = args->line);
    if (block == s_preview.marked)
    {
        return 0;
    }
    CliPreviewItems *const items = &s_preview.items;
    if (s_preview.marked < items->blocks)
    {
        items->item[items->block[s_preview.marked].item].flags &= ~CLI_PREVIEW_HERE;
    }
    if (block != PREVIEW_NONE_AT)
    {
        items->item[items->block[block].item].flags |= CLI_PREVIEW_HERE;
    }
    s_preview.marked = block;
    return 1;
}

unsigned int cli_preview_mark_press(const CliPreviewMarkCfg *args)
{
    CliPreviewOpen *const open = args->open_out;
    open->path[0] = '\0';
    open->line = 0ULL;
    open->placed = 0;
    open->target = CLI_PREVIEW_OPEN_NONE;
    open->top_px = s_preview.top_px;
    const CliPreviewItems *const items = &s_preview.items;
    const int y = args->y + s_preview.top_px;
    for (unsigned int at = EMBED_CALL(cli_preview_item.first, CliPreviewItemCfg, .top_px = y); at < items->items;
         at += 1u)
    {
        const CliPreviewItem *const item = &items->item[at];
        if ((item->type == CLI_PREVIEW_ITEM_BLOCK) && (item->y > y))
        {
            break;
        }
        const int inside =
            (args->x >= item->x) && (args->x < item->x + item->width) && (y >= item->y) && (y < item->y + item->height);
        if (inside && (item->type == CLI_PREVIEW_ITEM_TEXT) && (item->link_length != 0u))
        {
            return EMBED_CALL(address_followed, PreviewCtx, .text = items->text + item->link,
                              .length = item->link_length, .open_out = open);
        }
    }
    return CLI_PREVIEW_OPEN_NONE;
}
