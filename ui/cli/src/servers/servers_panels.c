// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_panels.c: the panels over the editor and the language server of the view shown, on the window's thread. The
// hover rests on the pointer's cell, the tree answering first and the server after; the completion ranks the file's
// words at once and the server's items after; the signature waits on a bracket or a comma, the tree's first and the
// server's over it. Where each panel stands on the screen is the window's: these carry what each panel shows
#include "servers.h"

#include "../code/symbols.h"
#include "../command_line/pool.h"
#include "../editor/view.h"
#include "../plugins/plugins.h"
#include "../text/text.h"
#include "servers_internal.h"

#include <stdlib.h>

// how long the pointer rests before a hover is asked for and lingers off it before the hover goes; how long the
// signature waits after a bracket or a comma; and how long the view rests before its server is told where
#define CLI_PANELS_HOVER_REST_MS 380ULL
#define CLI_PANELS_HOVER_LINGER_MS 300ULL
#define CLI_PANELS_SIGNATURE_WAIT_MS 120ULL
#define CLI_PANELS_VIEW_REST_MS 250ULL
// the most declarations the tree is asked for and the most of them a hover shows; the bytes a word takes; the bytes a
// server's hover is written in; the lines above the cursor a signature is read from; the bytes of the state they are
// entered in; and the regions a view's first line is told as standing inside
#define CLI_PANELS_DECLARATIONS 200u
#define CLI_PANELS_HOVER_DECLARATIONS 6u
#define CLI_PANELS_WORD 256u
#define CLI_PANELS_ANSWER_CAPACITY (4ULL << 20u)
#define CLI_PANELS_SIGNATURE_LINES 4000ULL
#define CLI_PANELS_STATE 1024u
#define CLI_PANELS_OPENERS 64u
// what sets the parts of a hover apart
#define CLI_PANELS_HOVER_BETWEEN "\n---\n"

// One completion item: its label and the label in small letters, its kind, what it states of itself and what it
// inserts, each with its length, and how it scores against the prefix
typedef struct
{
    const char *label;
    const char *lowered;
    const char *kind;
    const char *detail;
    const char *insert;
    unsigned long long label_length;
    unsigned long long detail_length;
    unsigned long long insert_length;
    unsigned int score;
} PanelsItem;

// The completion's list in one cell: the server's items and the file's words its texts stand in, each released with the
// list; the items; those the prefix answers in rank order; the row chosen and the first row shown; and the prefix, as
// typed and in small letters, and where it stands
typedef struct
{
    CliServerItems *server;
    CliDocumentWords *words;
    PanelsItem *item;
    unsigned int items;
    unsigned int *ranked;
    unsigned int count;
    unsigned int chosen;
    unsigned int first;
    char *prefix;
    char *lowered;
    unsigned long long prefix_length;
    unsigned long long line;
    unsigned long long from;
} PanelsCompletion;

// The panels: the pointer's cell over the editor's text; when the hover is asked for and when it goes, 0 for neither;
// the ask whose answer stands; the hover's Markdown in a cell and the word's place; the list; whether a list is wanted
// as the next frame places the cursor and whether it was forced open; the signature, when it is asked for, the place it
// was asked at and whether it is being asked for; the cursor; whether the text changed since the server last took it;
// when the server is sent where the view rests, whether it was sent, and the view's top, rows and lines then; and the
// tree and the editor's rows
typedef struct
{
    int pointer_in_text;
    unsigned long long pointer_row;
    unsigned long long pointer_column;
    unsigned long long hover_due_ms;
    unsigned long long hover_hide_ms;
    unsigned long long hover_ask;
    char *hover_text;
    unsigned long long hover_length;
    unsigned long long hover_line;
    unsigned long long hover_from;
    unsigned long long hover_to;
    PanelsCompletion *completion;
    int completion_wanted;
    int completion_forced;
    CliSignaturePanel *signature;
    unsigned long long signature_due_ms;
    CliPosition signature_position;
    int signature_asking;
    CliPosition cursor;
    int changed;
    unsigned long long view_due_ms;
    int view_sent;
    unsigned long long view_top;
    unsigned int view_rows;
    unsigned long long view_lines;
    const char *root;
    unsigned int rows;
} Panels;

static Panels s_panels;

// A hover being asked for: the ask it answers; the word, the tree and the path the server has the file at, after it in
// its cell; the word's place; and the Markdown answered, in a cell of its own
typedef struct
{
    unsigned long long ask;
    const char *word_text;
    const char *root;
    const char *full;
    unsigned long long line;
    unsigned long long col;
    unsigned long long from;
    unsigned long long to;
    char *text;
    unsigned long long length;
} PanelsHoverAsk;

// A signature being asked for: the view's key, the file's tree path and the path the server has it at, after it in its
// cell; the cursor's place; and what came
typedef struct
{
    const char *key;
    const char *path;
    const char *full;
    CliPosition cursor;
    CliSignaturePanel *panel;
} PanelsSignatureAsk;

// A view being handed to its server or let go of, or the server told of it: the key, the file's tree path, the tree and
// the whole path, after it in its cell; and whether the server took it
typedef struct
{
    const char *key;
    const char *path;
    const char *root;
    const char *full;
    unsigned long long first;
    unsigned long long last;
    int served;
} PanelsServeAsk;

typedef struct
{
    const char *key;
    const char *text;
    unsigned long long length;
    const char *lowered;
    const char *prefix;
    const char *prefix_lowered;
    PanelsItem *item;
    VerbaLinea *out;
    const CliFoundDeclaration *found;
    CliServerItems *server;
    CliDocumentWords *words;
    unsigned long long line;
    unsigned long long from;
    int on_server;
    unsigned int at;
    const CliSignaturePanel *own;
    const CliSignaturePanel *panel;
} PanelsCtx;

// the key of the view shown, else NULL
EMBED_INLINE const char *panels_viewed_key(const PanelsCtx *args)
{
    (void)args;
    const int shown = cli_view_shown();
    // a view's place is not negative past the test
    return (shown >= 0) ? cli_view_key((unsigned int)shown) : NULL;
}

// whether the view shown is served by its language server
EMBED_INLINE int panels_served(const PanelsCtx *args)
{
    (void)args;
    char full[CLI_PATH_MAX_LEN];
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    return (key != NULL) && EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = key, .full_out = full);
}

// a byte a word holds
EMBED_INLINE int panels_word_byte(const PanelsCtx *args)
{
    const unsigned char value = (unsigned char)args->text[0];
    return ((value >= 'a') && (value <= 'z')) || ((value >= 'A') && (value <= 'Z')) ||
           ((value >= '0') && (value <= '9')) || (value == '_') || (value == '$') || (value >= 0x80u);
}

// the hover gone, a timer of its going with it
EMBED_INLINE void panels_hover_hide(const PanelsCtx *args)
{
    (void)args;
    if (s_panels.hover_text != NULL)
    {
        g_cli_guard->persistent_buf_release(s_panels.hover_text, g_cli_servers_owner);
        s_panels.hover_text = NULL;
        s_panels.hover_length = 0ULL;
    }
    s_panels.hover_hide_ms = 0ULL;
}

// the list closed, its cells released, and nothing forced open
EMBED_INLINE void panels_completion_close(const PanelsCtx *args)
{
    (void)args;
    PanelsCompletion *const list = s_panels.completion;
    s_panels.completion_forced = 0;
    if (list == NULL)
    {
        return;
    }
    s_panels.completion = NULL;
    g_cli_guard->persistent_buf_release(list->server, g_cli_servers_owner);
    cli_view_words_release(list->words);
    g_cli_guard->persistent_buf_release(list, g_cli_servers_owner);
}

// the signature gone, a wait for one with it
EMBED_INLINE void panels_signature_hide(const PanelsCtx *args)
{
    (void)args;
    if (s_panels.signature != NULL)
    {
        g_cli_guard->persistent_buf_release(s_panels.signature, g_cli_servers_owner);
        s_panels.signature = NULL;
    }
    s_panels.signature_due_ms = 0ULL;
}

void cli_servers_panels_hide(void)
{
    EMBED_CALL(panels_hover_hide, PanelsCtx, .at = 0u);
    EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
    EMBED_CALL(panels_signature_hide, PanelsCtx, .at = 0u);
    s_panels.hover_due_ms = 0ULL;
    s_panels.hover_ask += 1ULL;
    s_panels.completion_wanted = 0;
    s_panels.view_sent = 0;
}

// whether the pointer stands on the hover's word, a unit either side
EMBED_INLINE int panels_pointer_on_word(const PanelsCtx *args)
{
    (void)args;
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    unsigned long long line = 0ULL;
    unsigned long long col = 0ULL;
    return s_panels.pointer_in_text && (s_panels.hover_text != NULL) && (key != NULL) &&
           EMBED_CALL(cli_view_row.position_at, CliViewRowCfg, .key = key, .row = s_panels.pointer_row,
                      .column = s_panels.pointer_column, .line = &line, .col = &col) &&
           (line == s_panels.hover_line) && ((col + 1ULL) >= s_panels.hover_from) && (col <= s_panels.hover_to);
}

void cli_servers_panels_pointer(const CliServersPanelsPointerCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    const int in_text = args->in_text && (EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u) != NULL);
    const int moved =
        !s_panels.pointer_in_text || (args->row != s_panels.pointer_row) || (args->column != s_panels.pointer_column);
    if (in_text)
    {
        if (moved)
        {
            s_panels.hover_due_ms = now_ms + CLI_PANELS_HOVER_REST_MS;
        }
        else
        {
            s_panels.hover_due_ms = s_panels.hover_due_ms;
        }
    }
    else
    {
        s_panels.hover_due_ms = 0ULL;
    }
    s_panels.hover_ask += (moved || !in_text) ? 1ULL : 0ULL;
    s_panels.pointer_in_text = in_text;
    s_panels.pointer_row = in_text ? args->row : s_panels.pointer_row;
    s_panels.pointer_column = in_text ? args->column : s_panels.pointer_column;
    if (s_panels.hover_text == NULL)
    {
        return;
    }
    if (args->on_hover || EMBED_CALL(panels_pointer_on_word, PanelsCtx, .at = 0u))
    {
        s_panels.hover_hide_ms = 0ULL;
    }
    else if (s_panels.hover_hide_ms == 0ULL)
    {
        s_panels.hover_hide_ms = now_ms + CLI_PANELS_HOVER_LINGER_MS;
    }
}

// a declaration of the hover's word written: its own line as code, then its kind and place
EMBED_INLINE void panels_declaration_write(const PanelsCtx *args)
{
    const CliFoundDeclaration *const found = args->found;
    if (found->text[0] != '\0')
    {
        mmgr_line_put(args->out, "```\n");
        mmgr_line_put(args->out, found->text);
        mmgr_line_put(args->out, "\n```\n\n");
    }
    mmgr_line_put(args->out, found->kind);
    mmgr_line_put(args->out, " `");
    mmgr_line_put(args->out, found->path);
    mmgr_line_ch(args->out, ':');
    mmgr_line_decimal(args->out, (unsigned long long)found->line + 1ULL, 1u);
    mmgr_line_ch(args->out, '`');
}

// the tree's declarations of the word laid as Markdown in a cell sized to them
static void panels_hover_tree_run(void *context)
{
    PanelsHoverAsk *const ask = (PanelsHoverAsk *)context;
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = ask->root);
    CliFoundDeclarations *const found =
        EMBED_CALL(cli_symbols_query_max.tree_declarations_alloc, CliSymbolsQueryMaxCfg, .query = ask->word_text,
                   .most = CLI_PANELS_DECLARATIONS, .owner = g_cli_servers_owner);
    if (found == NULL)
    {
        return;
    }
    unsigned long long bytes = 1ULL;
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliFoundDeclaration *const one = &found->found[at];
        bytes += cellul.len(&(CatenaFinitaCfg){.src = one->text, .cap = CLI_POOL_BYTES}) +
                 cellul.len(&(CatenaFinitaCfg){.src = one->kind, .cap = CLI_POOL_BYTES}) +
                 cellul.len(&(CatenaFinitaCfg){.src = one->path, .cap = CLI_POOL_BYTES}) + 64ULL;
    }
    char *const cell = g_cli_guard->persistent_buf_alloc(bytes, g_cli_servers_owner);
    VerbaLinea joined = mmgr_line_over(cell, bytes);
    unsigned int parts = 0u;
    for (unsigned int at = 0u; (cell != NULL) && (at < found->count) && (parts < CLI_PANELS_HOVER_DECLARATIONS);
         at += 1u)
    {
        const CliFoundDeclaration *const one = &found->found[at];
        if (cellul.eq(&(CatenaFinitaCfg){.src = one->name, .other = ask->word_text, .cap = CLI_PANELS_WORD}))
        {
            mmgr_line_put(&joined, (parts > 0u) ? CLI_PANELS_HOVER_BETWEEN : "");
            EMBED_CALL(panels_declaration_write, PanelsCtx, .found = one, .out = &joined);
            parts += 1u;
        }
    }
    g_cli_guard->persistent_buf_release(found, g_cli_servers_owner);
    if ((parts == 0u) || !mmgr_line_ok(&joined))
    {
        g_cli_guard->persistent_buf_release(cell, g_cli_servers_owner);
        return;
    }
    ask->text = cell;
    ask->length = joined.at;
}

// the server's hover, its Markdown in a cell sized to it
static void panels_hover_server_run(void *context)
{
    PanelsHoverAsk *const ask = (PanelsHoverAsk *)context;
    char *const cell = g_cli_guard->temporary_buf_alloc(CLI_PANELS_ANSWER_CAPACITY, g_cli_servers_owner);
    VerbaLinea output = mmgr_line_over(cell, CLI_PANELS_ANSWER_CAPACITY);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    // a place in a text a view shows stands below 2^32 lines and columns
    if ((cell != NULL) &&
        EMBED_CALL(cli_servers_path_out.hover, CliServersPathOutCfg, .path = ask->full, .line = (unsigned int)ask->line,
                   .col = (unsigned int)ask->col, .out = &output, .err_msg = &ignored) &&
        mmgr_line_ok(&output) && (output.at > 0ULL))
    {
        ask->text = g_cli_guard->persistent_buf_alloc(output.at, g_cli_servers_owner);
        if (ask->text != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = ask->text, .src = output.out, .bytes = output.at});
            ask->length = output.at;
        }
    }
    g_cli_guard->persistent_buf_release(cell, g_cli_servers_owner);
}

static void panels_hover_server_done(void *context);

// the tree's answer: it stands as the hover, and the server is asked after it
static void panels_hover_tree_done(void *context)
{
    PanelsHoverAsk *const ask = (PanelsHoverAsk *)context;
    EMBED_CALL(panels_hover_hide, PanelsCtx, .at = 0u);
    s_panels.hover_text = ask->text;
    s_panels.hover_length = ask->length;
    s_panels.hover_line = ask->line;
    s_panels.hover_from = ask->from;
    s_panels.hover_to = ask->to;
    ask->text = NULL;
    ask->length = 0ULL;
    if (ask->full[0] != '\0')
    {
        panels_hover_server_run(ask);
        panels_hover_server_done(ask);
        return;
    }
    g_cli_guard->persistent_buf_release(ask, g_cli_servers_owner);
}

// the server's answer: it stands after the tree's
static void panels_hover_server_done(void *context)
{
    PanelsHoverAsk *const ask = (PanelsHoverAsk *)context;
    if (ask->text != NULL)
    {
        const unsigned long long between =
            (s_panels.hover_text != NULL) ? (sizeof(CLI_PANELS_HOVER_BETWEEN) - 1ULL) : 0ULL;
        char *const joined =
            g_cli_guard->persistent_buf_alloc(s_panels.hover_length + between + ask->length, g_cli_servers_owner);
        if (joined != NULL)
        {
            if (s_panels.hover_text != NULL)
            {
                memor.cpy(&(MemoriaCfg){.dst = joined, .src = s_panels.hover_text, .bytes = s_panels.hover_length});
                memor.cpy(&(MemoriaCfg){
                    .dst = joined + s_panels.hover_length, .src = CLI_PANELS_HOVER_BETWEEN, .bytes = between});
            }
            memor.cpy(
                &(MemoriaCfg){.dst = joined + s_panels.hover_length + between, .src = ask->text, .bytes = ask->length});
            const unsigned long long length = s_panels.hover_length + between + ask->length;
            EMBED_CALL(panels_hover_hide, PanelsCtx, .at = 0u);
            s_panels.hover_text = joined;
            s_panels.hover_length = length;
            s_panels.hover_line = ask->line;
            s_panels.hover_from = ask->from;
            s_panels.hover_to = ask->to;
        }
    }
    g_cli_guard->persistent_buf_release(ask->text, g_cli_servers_owner);
    g_cli_guard->persistent_buf_release(ask, g_cli_servers_owner);
}

// the hover asked for at the pointer's cell: a pointer resting on the word shown does not ask again, and a place no
// word stands at lets the hover shown go unless the pointer stands on it
EMBED_INLINE void panels_hover_ask(const PanelsCtx *args)
{
    (void)args;
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    if ((key == NULL) || (s_panels.root == NULL) || EMBED_CALL(panels_pointer_on_word, PanelsCtx, .at = 0u))
    {
        return;
    }
    unsigned long long line = 0ULL;
    unsigned long long col = 0ULL;
    unsigned long long from = 0ULL;
    unsigned long long to = 0ULL;
    char word_text[CLI_PANELS_WORD];
    if (!EMBED_CALL(cli_view_row.position_at, CliViewRowCfg, .key = key, .row = s_panels.pointer_row,
                    .column = s_panels.pointer_column, .line = &line, .col = &col) ||
        !EMBED_CALL(cli_view_line.word_at, CliViewLineCfg, .key = key, .line_value = line, .col_value = col,
                    .from = &from, .to = &to, .word_text = word_text, .capacity = sizeof(word_text)))
    {
        EMBED_CALL(panels_hover_hide, PanelsCtx, .at = 0u);
        return;
    }
    char full[CLI_PATH_MAX_LEN];
    full[0] = '\0';
    if (!EMBED_CALL(panels_served, PanelsCtx, .at = 0u) ||
        !EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = key, .full_out = full))
    {
        full[0] = '\0';
    }
    PanelsHoverAsk ask = {s_panels.hover_ask, word_text, s_panels.root, full, line, col, from, to, NULL, 0ULL};
    PanelsHoverAsk *const asked = g_cli_guard->persistent_buf_alloc(sizeof(ask), g_cli_servers_owner);
    if (asked == NULL)
    {
        return;
    }
    *asked = ask;
    panels_hover_tree_run(asked);
    panels_hover_tree_done(asked);
}

// how the item scores against the prefix: 5 its label starts with it, 4 starts with it case aside, 3 holds it case
// aside, 2 holds its letters in order case aside, 1 for no prefix, 0 for no answer
EMBED_INLINE unsigned int panels_item_score(const PanelsCtx *args)
{
    const PanelsItem *const item = args->item;
    const unsigned long long want = args->length;
    if (want == 0ULL)
    {
        return 1u;
    }
    if (want > item->label_length)
    {
        return 0u;
    }
    if (memor.cmp(&(MemoriaCfg){.src = item->label, .other = args->prefix, .bytes = want}) == 0)
    {
        return 5u;
    }
    if (memor.cmp(&(MemoriaCfg){.src = item->lowered, .other = args->prefix_lowered, .bytes = want}) == 0)
    {
        return 4u;
    }
    char first[2] = {args->prefix_lowered[0], '\0'};
    const unsigned long long last = item->label_length - want + 1ULL;
    for (unsigned long long start = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                               .text = item->lowered, .at = 0ULL, .size = last, .set = first);
         start < last; start = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                          .text = item->lowered, .at = start + 1ULL, .size = last, .set = first))
    {
        if (memor.cmp(&(MemoriaCfg){.src = item->lowered + start, .other = args->prefix_lowered, .bytes = want}) == 0)
        {
            return 3u;
        }
    }
    unsigned long long at = 0ULL;
    for (unsigned long long letter = 0ULL; letter < want; letter += 1ULL)
    {
        first[0] = args->prefix_lowered[letter];
        at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = item->lowered, .at = at,
                        .size = item->label_length, .set = first);
        if (at >= item->label_length)
        {
            return 0u;
        }
        at += 1ULL;
    }
    return 2u;
}

// the list whose items are being ranked, read by the order as it compares two
static const PanelsCompletion *s_ranking;

// two items in rank order: the higher score first, then the shorter label, then the labels' bytes
static int panels_item_order(const void *one, const void *other)
{
    const PanelsItem *const first = &s_ranking->item[*(const unsigned int *)one];
    const PanelsItem *const second = &s_ranking->item[*(const unsigned int *)other];
    if (first->score != second->score)
    {
        return (first->score > second->score) ? -1 : 1;
    }
    if (first->label_length != second->label_length)
    {
        return (first->label_length < second->label_length) ? -1 : 1;
    }
    const embed_iword order =
        memor.cmp(&(MemoriaCfg){.src = first->label, .other = second->label, .bytes = first->label_length});
    if (order == 0)
    {
        return 0;
    }
    return (order < 0) ? -1 : 1;
}

// the items the prefix answers ranked in `list`, the row chosen saved on the item it was on where it is still shown
EMBED_INLINE void panels_completion_rank(PanelsCompletion *list)
{
    const unsigned int saved = (list->count > 0u) ? list->ranked[list->chosen] : list->items;
    list->count = 0u;
    for (unsigned int at = 0u; at < list->items; at += 1u)
    {
        PanelsItem *const item = &list->item[at];
        item->score = EMBED_CALL(panels_item_score, PanelsCtx, .item = item, .length = list->prefix_length,
                                 .prefix = list->prefix, .prefix_lowered = list->lowered);
        if (item->score > 0u)
        {
            list->ranked[list->count] = at;
            list->count += 1u;
        }
    }
    s_ranking = list;
    qsort(list->ranked, list->count, sizeof(*list->ranked), panels_item_order);
    s_ranking = NULL;
    list->chosen = 0u;
    for (unsigned int at = 0u; at < list->count; at += 1u)
    {
        if (list->ranked[at] == saved)
        {
            list->chosen = at;
        }
    }
}

// the first row shown moved as far as it must to show the row chosen
EMBED_INLINE void panels_completion_scrolled(PanelsCompletion *list)
{
    if (list->chosen < list->first)
    {
        list->first = list->chosen;
    }
    else if (list->chosen >= (list->first + CLI_COMPLETION_ROWS))
    {
        list->first = list->chosen - CLI_COMPLETION_ROWS + 1u;
    }
    const unsigned int most = (list->count > CLI_COMPLETION_ROWS) ? (list->count - CLI_COMPLETION_ROWS) : 0u;
    list->first = (list->first > most) ? most : list->first;
}

// the placeholder of a tab stop, from `from` in `length` bytes of `text`, written to `out` up to the brace that closes
// the stop, each stop nested in it flattened to its own placeholder and an escaped byte written as itself: the place
// past that brace, or the text's end
EMBED_INLINE unsigned long long panels_placeholder_write(const PanelsCtx *args)
{
    const char *const text = args->text;
    const unsigned long long length = args->length;
    VerbaLinea *const out = args->out;
    unsigned long long each = args->from;
    unsigned long long depth = 1ULL;
    while ((each < length) && (depth > 0ULL))
    {
        const unsigned long long inner = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                    .text = text, .at = each, .size = length, .set = "\\$}");
        mmgr_line_put_n(out, text + each, inner - each);
        each = inner;
        if (each >= length)
        {
            return each;
        }
        if ((text[each] == '\\') && ((each + 1ULL) < length))
        {
            mmgr_line_ch(out, text[each + 1ULL]);
            each += 2ULL;
        }
        else if ((text[each] == '$') && ((each + 1ULL) < length) && (text[each + 1ULL] == '{'))
        {
            depth += 1ULL;
            each = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = text,
                              .at = each + 2ULL, .size = length, .classes = CLI_TEXT_DIGITS, .set = "");
            each += ((each < length) && (text[each] == ':')) ? 1ULL : 0ULL;
        }
        else if (text[each] == '}')
        {
            depth -= 1ULL;
            each += 1ULL;
        }
        else
        {
            mmgr_line_ch(out, text[each]);
            each += 1ULL;
        }
    }
    return each;
}

// the tab stop whose `$` stands just before `from` in `length` bytes of `text` written to `out` as the text it
// inserts: `$n` and `${n}` as nothing, `${n:text}` as its placeholder and `${n|a,b|}` as its first choice: the place
// past it
EMBED_INLINE unsigned long long panels_stop_write(const PanelsCtx *args)
{
    const char *const text = args->text;
    const unsigned long long length = args->length;
    unsigned long long each = args->from;
    if ((each < length) && (text[each] != '{'))
    {
        return EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = text, .at = each,
                          .size = length, .classes = CLI_TEXT_DIGITS, .set = "");
    }
    each += (each < length) ? 1ULL : 0ULL;
    each = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = text, .at = each,
                      .size = length, .classes = CLI_TEXT_DIGITS, .set = "");
    if ((each < length) && (text[each] == ':'))
    {
        return EMBED_CALL(panels_placeholder_write, PanelsCtx, .text = text, .length = length, .out = args->out,
                          .from = each + 1ULL);
    }
    if ((each < length) && (text[each] == '|'))
    {
        each += 1ULL;
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = text, .at = each, .size = length, .set = ",|");
        mmgr_line_put_n(args->out, text + each, stop - each);
        each = stop;
    }
    each = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = each,
                      .size = length, .set = "}");
    return each + ((each < length) ? 1ULL : 0ULL);
}

// `length` bytes of a snippet `text` written as the text it inserts: a tab stop with a placeholder, `${n:text}`, as its
// placeholder, nested stops flattened the same way; one with choices, `${n|a,b|}`, as its first choice; a bare stop,
// `$n` or `${n}`, as nothing; and an escaped byte as itself
EMBED_INLINE void panels_snippet_write(const PanelsCtx *args)
{
    const char *const text = args->text;
    const unsigned long long length = args->length;
    VerbaLinea *const out = args->out;
    unsigned long long each = 0ULL;
    while (each < length)
    {
        const unsigned long long plain = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                    .text = text, .at = each, .size = length, .set = "\\$");
        mmgr_line_put_n(out, text + each, plain - each);
        each = plain;
        if ((each < length) && (text[each] == '\\'))
        {
            const unsigned long long escaped = (each + 1ULL < length) ? (each + 1ULL) : each;
            mmgr_line_ch(out, text[escaped]);
            each = escaped + 1ULL;
        }
        else if (each < length)
        {
            each = EMBED_CALL(panels_stop_write, PanelsCtx, .text = text, .length = length, .out = out,
                              .from = each + 1ULL);
        }
    }
}

// the list laid in one cell from the server's items and the file's words, a word the server gave left out, the labels
// lowered once and each snippet flattened once: the list, holding both from here, or NULL with both let go where the
// cells cannot meet it
EMBED_INLINE PanelsCompletion *panels_completion_alloc(const PanelsCtx *args)
{
    const unsigned int from_server = (args->server != NULL) ? args->server->count : 0u;
    const unsigned int from_words = (args->words != NULL) ? args->words->count : 0u;
    const unsigned int most = from_server + from_words;
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; at < from_server; at += 1u)
    {
        const CliServerItem *const one = &args->server->item[at];
        bytes += cellul.len(&(CatenaFinitaCfg){.src = one->label, .cap = CLI_POOL_BYTES}) +
                 cellul.len(&(CatenaFinitaCfg){.src = one->insert, .cap = CLI_POOL_BYTES}) + 2ULL;
    }
    for (unsigned int at = 0u; at < from_words; at += 1u)
    {
        bytes += (unsigned long long)args->words->length[at] + 1ULL;
    }
    PanelsCompletion *const list = g_cli_guard->persistent_buf_alloc(
        sizeof(*list) + (most * sizeof(PanelsItem)) + (most * sizeof(unsigned int)) + (2ULL * CLI_PANELS_WORD) + bytes,
        g_cli_servers_owner);
    if (list == NULL)
    {
        g_cli_guard->persistent_buf_release(args->server, g_cli_servers_owner);
        cli_view_words_release(args->words);
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = list, .val = 0, .bytes = sizeof(*list)});
    list->server = args->server;
    list->words = args->words;
    list->item = (PanelsItem *)(void *)(list + 1);
    list->ranked = (unsigned int *)(void *)(list->item + most);
    list->prefix = (char *)(void *)(list->ranked + most);
    list->lowered = list->prefix + CLI_PANELS_WORD;
    list->line = args->line;
    list->from = args->from;
    list->prefix_length = (args->length < (CLI_PANELS_WORD - 1ULL)) ? args->length : (CLI_PANELS_WORD - 1ULL);
    memor.cpy(&(MemoriaCfg){.dst = list->prefix, .src = args->prefix, .bytes = list->prefix_length});
    memor.cpy(&(MemoriaCfg){.dst = list->lowered, .src = args->prefix, .bytes = list->prefix_length});
    list->prefix[list->prefix_length] = '\0';
    list->lowered[list->prefix_length] = '\0';
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .size = list->prefix_length, .text_out = list->lowered);
    char *text = list->lowered + CLI_PANELS_WORD;
    for (unsigned int at = 0u; at < from_server; at += 1u)
    {
        const CliServerItem *const one = &args->server->item[at];
        const unsigned long long label_length =
            cellul.len(&(CatenaFinitaCfg){.src = one->label, .cap = CLI_POOL_BYTES});
        if (label_length == 0ULL)
        {
            continue;
        }
        PanelsItem *const item = &list->item[list->items];
        item->label = one->label;
        item->label_length = label_length;
        item->kind = one->kind;
        item->detail = one->detail;
        item->detail_length = cellul.len(&(CatenaFinitaCfg){.src = one->detail, .cap = CLI_POOL_BYTES});
        const unsigned long long insert_length =
            cellul.len(&(CatenaFinitaCfg){.src = one->insert, .cap = CLI_POOL_BYTES});
        const char *const insert = (insert_length != 0ULL) ? one->insert : one->label;
        const unsigned long long inserted = (insert_length != 0ULL) ? insert_length : label_length;
        if (one->snippet)
        {
            VerbaLinea flat = {text, inserted + 1ULL, 0ULL};
            EMBED_CALL(panels_snippet_write, PanelsCtx, .text = insert, .length = inserted, .out = &flat);
            item->insert = text;
            item->insert_length = flat.at;
            text += flat.at + 1ULL;
        }
        else
        {
            item->insert = insert;
            item->insert_length = inserted;
        }
        memor.cpy(&(MemoriaCfg){.dst = text, .src = one->label, .bytes = label_length});
        EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .size = label_length, .text_out = text);
        item->lowered = text;
        text += label_length + 1ULL;
        list->items += 1u;
    }
    const unsigned int supplied = list->items;
    {
        unsigned int at = 0u;
        while (at < from_words)
        {
            const char *const label = args->words->text + args->words->at[at];
            const unsigned long long label_length = args->words->length[at];
            int supplied_already = 0;
            unsigned int other = 0u;
            while (!supplied_already && (other < supplied))
            {
                supplied_already =
                    (list->item[other].label_length == label_length) &&
                    (memor.cmp(&(MemoriaCfg){.src = list->item[other].label, .other = label, .bytes = label_length}) ==
                     0);
                other += 1u;
            }
            if (supplied_already)
            {
                at += 1u;
                continue;
            }
            PanelsItem *const item = &list->item[list->items];
            item->label = label;
            item->label_length = label_length;
            item->kind = "word";
            item->detail = "";
            item->detail_length = 0ULL;
            item->insert = label;
            item->insert_length = label_length;
            memor.cpy(&(MemoriaCfg){.dst = text, .src = label, .bytes = label_length});
            EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .size = label_length, .text_out = text);
            item->lowered = text;
            text += label_length + 1ULL;
            list->items += 1u;
            at += 1u;
        }
    }
    return list;
}

// the list `list` made the one shown in place of the one that stood, the row chosen saved on its item: closed where
// nothing answers, where the one answer is what is typed and nothing forced it, or where nothing is typed and no list
// stood or forced it open
EMBED_INLINE void panels_completion_stand(PanelsCompletion *list)
{
    PanelsCompletion *const was = s_panels.completion;
    panels_completion_rank(list);
    if ((was != NULL) && (was->count > 0u))
    {
        const PanelsItem *const chosen = &was->item[was->ranked[was->chosen]];
        for (unsigned int at = 0u; at < list->count; at += 1u)
        {
            const PanelsItem *const item = &list->item[list->ranked[at]];
            if ((item->label_length == chosen->label_length) &&
                (memor.cmp(&(MemoriaCfg){.src = item->label, .other = chosen->label, .bytes = item->label_length}) ==
                 0))
            {
                list->chosen = at;
                break;
            }
        }
        list->first = was->first;
    }
    const PanelsItem *const only = (list->count == 1u) ? &list->item[list->ranked[0]] : NULL;
    const int alone =
        (only != NULL) && (only->label_length == list->prefix_length) &&
        (memor.cmp(&(MemoriaCfg){.src = only->label, .other = list->prefix, .bytes = list->prefix_length}) == 0);
    const int empty = (list->prefix_length == 0ULL) && !s_panels.completion_forced && (was == NULL);
    if ((list->count == 0u) || (alone && !s_panels.completion_forced) || empty)
    {
        g_cli_guard->persistent_buf_release(list->server, g_cli_servers_owner);
        cli_view_words_release(list->words);
        g_cli_guard->persistent_buf_release(list, g_cli_servers_owner);
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
        return;
    }
    panels_completion_scrolled(list);
    if (was != NULL)
    {
        g_cli_guard->persistent_buf_release(was->server, g_cli_servers_owner);
        cli_view_words_release(was->words);
        g_cli_guard->persistent_buf_release(was, g_cli_servers_owner);
    }
    s_panels.completion = list;
}

// the server asked for the items at the cursor; where the list still stands on the prefix asked about, it is laid
// again with the server's items and the words it had
EMBED_INLINE void panels_items_post(const PanelsCtx *args)
{
    char full[CLI_PATH_MAX_LEN];
    PanelsCompletion *const list = s_panels.completion;
    if ((list == NULL) || !EMBED_CALL(panels_served, PanelsCtx, .at = 0u) ||
        !EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = args->key, .full_out = full))
    {
        return;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    // a place in a text a view shows stands below 2^32 lines and columns
    CliServerItems *const items =
        EMBED_CALL(cli_servers_ask.items_alloc, CliServersAskCfg, .path = full, .line = (unsigned int)list->line,
                   .col = (unsigned int)s_panels.cursor.col, .owner = g_cli_servers_owner, .err_msg = &ignored);
    if (items == NULL)
    {
        return;
    }
    CliDocumentWords *const words = list->words;
    list->words = NULL;
    PanelsCompletion *const laid =
        EMBED_CALL(panels_completion_alloc, PanelsCtx, .server = items, .words = words, .line = list->line,
                   .from = list->from, .prefix = list->prefix, .length = list->prefix_length);
    if (laid != NULL)
    {
        panels_completion_stand(laid);
    }
}

// the completions asked for the word before the cursor: the file's words at once and the server's items after, those it
// gave for the same prefix's start standing until it answers again
EMBED_INLINE void panels_completion_ask(const PanelsCtx *args)
{
    (void)args;
    s_panels.completion_wanted = 0;
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    unsigned long long from = 0ULL;
    char prefix[CLI_PANELS_WORD];
    CliPosition cursor;
    if ((key == NULL) ||
        !EMBED_CALL(cli_view_from.prefix, CliViewFromCfg, .key = key, .from = &from, .prefix = prefix,
                    .capacity = sizeof(prefix)) ||
        !EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = key, .cursor = &cursor))
    {
        return;
    }
    s_panels.cursor = cursor;
    PanelsCompletion *const was = s_panels.completion;
    CliServerItems *server = NULL;
    if ((was != NULL) && (was->line == cursor.line) && (was->from == from))
    {
        server = was->server;
        was->server = NULL;
    }
    PanelsCompletion *const list = EMBED_CALL(
        panels_completion_alloc, PanelsCtx, .server = server, .words = cli_view_words_alloc(key), .line = cursor.line,
        .from = from, .prefix = prefix, .length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = sizeof(prefix)}));
    if (list != NULL)
    {
        panels_completion_stand(list);
    }
    EMBED_CALL(panels_items_post, PanelsCtx, .key = key);
}

// the list, where it is open, followed to the frame's cursor: filtered again by the word before the cursor, and closed
// where the cursor has left the line or that word no longer starts where the prefix did
EMBED_INLINE void panels_completion_follow(const PanelsCtx *args)
{
    PanelsCompletion *const list = s_panels.completion;
    if (list == NULL)
    {
        return;
    }
    unsigned long long from = 0ULL;
    char prefix[CLI_PANELS_WORD];
    if ((s_panels.cursor.line != list->line) ||
        !EMBED_CALL(cli_view_from.prefix, CliViewFromCfg, .key = args->key, .from = &from, .prefix = prefix,
                    .capacity = sizeof(prefix)) ||
        (from != list->from))
    {
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = sizeof(prefix)});
    if ((length == 0ULL) && !s_panels.completion_forced)
    {
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = list->prefix, .src = prefix, .bytes = length + 1ULL});
    memor.cpy(&(MemoriaCfg){.dst = list->lowered, .src = prefix, .bytes = length + 1ULL});
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .size = length, .text_out = list->lowered);
    list->prefix_length = length;
    panels_completion_rank(list);
    if (list->count == 0u)
    {
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
        return;
    }
    panels_completion_scrolled(list);
}

// the row `at` of the list accepted: its text written over the prefix before the cursor, the list closed
EMBED_INLINE void panels_completion_accept(const PanelsCtx *args)
{
    const PanelsCompletion *const list = s_panels.completion;
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    if ((list == NULL) || (key == NULL) || (args->at >= list->count))
    {
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
        return;
    }
    const PanelsItem *const item = &list->item[list->ranked[args->at]];
    const unsigned long long back = ((s_panels.cursor.line == list->line) && (s_panels.cursor.col >= list->from))
                                        ? (s_panels.cursor.col - list->from)
                                        : list->prefix_length;
    const CliViewInput accepted = {
        .kind = CLI_VIEW_COMPLETE, .text = item->insert, .length = item->insert_length, .column = back};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &accepted, .err_msg = &err_msg);
    s_panels.changed = 1;
    EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
}

void cli_servers_panels_accept(unsigned int at)
{
    EMBED_CALL(panels_completion_accept, PanelsCtx, .at = at);
}

void cli_servers_panels_wheel(int rows)
{
    PanelsCompletion *const list = s_panels.completion;
    if ((list == NULL) || (list->count <= CLI_COMPLETION_ROWS))
    {
        return;
    }
    const unsigned int most = list->count - CLI_COMPLETION_ROWS;
    if (rows < 0)
    {
        // the rows turned up, a count far inside what an unsigned int represents
        const unsigned int up = (unsigned int)(-rows);
        list->first = (list->first > up) ? (list->first - up) : 0u;
    }
    else
    {
        // the rows turned are not negative
        list->first += (unsigned int)rows;
        list->first = (list->first > most) ? most : list->first;
    }
}

// a key's effect on the open list, 1 where the key was the list's
EMBED_INLINE int panels_completion_key(const PanelsCtx *args)
{
    PanelsCompletion *const list = s_panels.completion;
    if ((list == NULL) || (list->count == 0u))
    {
        return 0;
    }
    const char *const name = args->text;
    const unsigned int count = list->count;
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Up", .cap = 64ULL}))
    {
        list->chosen = (list->chosen + count - 1u) % count;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Down", .cap = 64ULL}))
    {
        list->chosen = (list->chosen + 1u) % count;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "PageUp", .cap = 64ULL}))
    {
        list->chosen = (list->chosen > CLI_COMPLETION_ROWS) ? (list->chosen - CLI_COMPLETION_ROWS) : 0u;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "PageDown", .cap = 64ULL}))
    {
        list->chosen =
            ((list->chosen + CLI_COMPLETION_ROWS) < count) ? (list->chosen + CLI_COMPLETION_ROWS) : (count - 1u);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Enter", .cap = 64ULL}) ||
             cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Tab", .cap = 64ULL}))
    {
        EMBED_CALL(panels_completion_accept, PanelsCtx, .at = list->chosen);
        return 1;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Escape", .cap = 64ULL}))
    {
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
        return 1;
    }
    else
    {
        return 0;
    }
    panels_completion_scrolled(list);
    return 1;
}

int cli_servers_panels_key(const char *name)
{
    if (EMBED_CALL(panels_completion_key, PanelsCtx, .text = name))
    {
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Escape", .cap = 64ULL}) &&
        ((s_panels.signature != NULL) || (s_panels.hover_text != NULL)))
    {
        cli_servers_panels_hide();
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Mod+Space", .cap = 64ULL}))
    {
        s_panels.completion_forced = 1;
        EMBED_CALL(panels_completion_ask, PanelsCtx, .at = 0u);
        return 1;
    }
    EMBED_CALL(panels_hover_hide, PanelsCtx, .at = 0u);
    // a key that takes letters out saves the list open on the prefix left; any other key closes it
    if (s_panels.completion != NULL)
    {
        const int edits = cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Backspace", .cap = 64ULL}) ||
                          cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Delete", .cap = 64ULL});
        s_panels.completion_wanted = edits;
        if (!edits)
        {
            EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
        }
    }
    return 0;
}

void cli_servers_panels_text(const char *text, unsigned long long length)
{
    s_panels.changed = 1;
    EMBED_CALL(panels_hover_hide, PanelsCtx, .at = 0u);
    if ((length > 0ULL) && EMBED_CALL(panels_word_byte, PanelsCtx, .text = text + length - 1ULL))
    {
        s_panels.completion_wanted = 1;
    }
    else
    {
        s_panels.completion_wanted = 0;
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
    }
    if (EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = 0ULL, .size = length,
                   .set = "(,") < length)
    {
        s_panels.signature_due_ms = cli_clock_milliseconds() + CLI_PANELS_SIGNATURE_WAIT_MS;
    }
}

// the panel of `panel`, with the parameters the tree's `own` found the arguments to differ from, in a cell of its own
EMBED_INLINE CliSignaturePanel *panels_signature_joined(const PanelsCtx *args)
{
    const CliSignaturePanel *const panel = args->panel;
    const unsigned int differing = (args->own != NULL) ? args->own->differing : 0u;
    CliSignaturePanel *const joined = g_cli_guard->persistent_buf_alloc(
        sizeof(*joined) + (panel->parameters * sizeof(*joined->parameter)) + (differing * sizeof(*joined->differs)) +
            panel->length + panel->doc_length + 2ULL,
        g_cli_servers_owner);
    if (joined == NULL)
    {
        return NULL;
    }
    *joined = *panel;
    joined->parameter = (CliSignatureParameter *)(void *)(joined + 1);
    joined->differs = (unsigned int *)(void *)(joined->parameter + panel->parameters);
    char *const text = (char *)(void *)(joined->differs + differing);
    memor.cpy(&(MemoriaCfg){
        .dst = joined->parameter, .src = panel->parameter, .bytes = panel->parameters * sizeof(*joined->parameter)});
    joined->differing = differing;
    if (differing != 0u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = joined->differs, .src = args->own->differs, .bytes = differing * sizeof(*joined->differs)});
    }
    memor.cpy(&(MemoriaCfg){.dst = text, .src = panel->label, .bytes = panel->length});
    text[panel->length] = '\0';
    memor.cpy(&(MemoriaCfg){.dst = text + panel->length + 1ULL, .src = panel->doc, .bytes = panel->doc_length});
    text[panel->length + 1ULL + panel->doc_length] = '\0';
    joined->label = text;
    joined->doc = text + panel->length + 1ULL;
    return joined;
}

static void panels_signature_tree_run(void *context);
static void panels_signature_tree_done(void *context);
static void panels_signature_server_run(void *context);
static void panels_signature_server_done(void *context);

// the signature asked for the call the cursor is in, the tree's first, the cursor's place recorded as the one asked
// about
EMBED_INLINE void panels_signature_ask(const PanelsCtx *args)
{
    (void)args;
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    char path[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    CliPosition cursor;
    if ((key == NULL) || s_panels.signature_asking ||
        !EMBED_CALL(cli_view_path_kind.path, CliViewPathKindCfg, .key = key, .path_out = path) ||
        !EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = key, .cursor = &cursor))
    {
        return;
    }
    full[0] = '\0';
    if (!EMBED_CALL(panels_served, PanelsCtx, .at = 0u) ||
        !EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = key, .full_out = full))
    {
        full[0] = '\0';
    }
    PanelsSignatureAsk ask = {key, path, full, cursor, NULL};
    s_panels.signature_position = cursor;
    s_panels.signature_asking = 1;
    panels_signature_tree_run(&ask);
    panels_signature_tree_done(&ask);
}

// the text's lines from as far above as CLI_PANELS_SIGNATURE_LINES reach to the cursor's, a paged text's those the page
// has read, from the state the first was entered in, and the tree's signature of the call
static void panels_signature_tree_run(void *context)
{
    PanelsSignatureAsk *const ask = (PanelsSignatureAsk *)context;
    unsigned long long low = 0ULL;
    unsigned long long high = 0ULL;
    (void)EMBED_CALL(cli_document_key_low.bounds, CliDocumentKeyLowCfg, .key = ask->key, .low = &low, .high = &high);
    const unsigned long long line = ask->cursor.line;
    const unsigned long long reach =
        (line > (CLI_PANELS_SIGNATURE_LINES - 1ULL)) ? (line - (CLI_PANELS_SIGNATURE_LINES - 1ULL)) : 0ULL;
    const unsigned long long start = (reach > low) ? reach : low;
    const unsigned long long top = (start < line) ? start : line;
    char state[CLI_PANELS_STATE];
    unsigned long long size = 0ULL;
    char *const text =
        EMBED_CALL(cli_document_key.lines_alloc, CliDocumentKeyCfg, .key = ask->key, .first = top, .last = line,
                   .size_pointer = &size, .state = state, .capacity = sizeof(state), .owner = g_cli_servers_owner);
    if (text == NULL)
    {
        return;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    ask->panel = EMBED_CALL(cli_symbols_path_text_size_out.signature_alloc, CliSymbolsPathTextSizeOutCfg,
                            .path = ask->path, .text = text, .size = size, .owner = g_cli_servers_owner,
                            .state = (state[0] != '\0') ? state : "root", .line = line - top, .col = ask->cursor.col,
                            .err_msg = &ignored);
    g_cli_guard->persistent_buf_release(text, g_cli_servers_owner);
}

// the tree's answer stands, and the server is asked over it where the file is served
static void panels_signature_tree_done(void *context)
{
    PanelsSignatureAsk *const ask = (PanelsSignatureAsk *)context;
    EMBED_CALL(panels_signature_hide, PanelsCtx, .at = 0u);
    s_panels.signature = ask->panel;
    ask->panel = NULL;
    if (ask->full[0] != '\0')
    {
        panels_signature_server_run(ask);
        panels_signature_server_done(ask);
        return;
    }
    s_panels.signature_asking = 0;
}

// the server's signature at the place
static void panels_signature_server_run(void *context)
{
    PanelsSignatureAsk *const ask = (PanelsSignatureAsk *)context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    // a place in a text a view shows stands below 2^32 lines and columns
    ask->panel = EMBED_CALL(cli_servers_ask.signature_alloc, CliServersAskCfg, .path = ask->full,
                            .line = (unsigned int)ask->cursor.line, .col = (unsigned int)ask->cursor.col,
                            .owner = g_cli_servers_owner, .err_msg = &ignored);
}

// the server's answer stands over the tree's, whose differing parameters it saves
static void panels_signature_server_done(void *context)
{
    PanelsSignatureAsk *const ask = (PanelsSignatureAsk *)context;
    s_panels.signature_asking = 0;
    if (ask->panel != NULL)
    {
        CliSignaturePanel *const joined =
            EMBED_CALL(panels_signature_joined, PanelsCtx, .panel = ask->panel, .own = s_panels.signature);
        if (joined != NULL)
        {
            EMBED_CALL(panels_signature_hide, PanelsCtx, .at = 0u);
            s_panels.signature = joined;
        }
    }
    g_cli_guard->persistent_buf_release(ask->panel, g_cli_servers_owner);
}

// the view's text handed to its language's server, where it is read whole and its language has a server that takes
// it, and the view marked served
static void panels_serve_run(PanelsServeAsk *ask)
{
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = ask->root, .relative = ask->path, .joined = full);
    char id[CLI_NAME_MAX_LEN];
    CliLanguages languages;
    EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &languages);
    const int known = EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                 .languages_read = &languages, .path = ask->path, .id_out = id);
    g_cli_guard->persistent_buf_release(languages.pair, g_cli_plugins_owner);
    *(&languages) = (CliLanguages){NULL, 0u, 0u};
    unsigned long long size = 0ULL;
    char *const text = known ? EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = ask->key,
                                          .size_pointer = &size, .owner = g_cli_servers_owner)
                             : NULL;
    if (text == NULL)
    {
        return;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea refused = {why, sizeof(why), 0ULL};
    const CliServersSink sink = cli_servers_problems_sink(ask->root);
    int served = 0;
    (void)EMBED_CALL(cli_servers_root.open, CliServersRootCfg, .root = ask->root, .path = full, .language = id,
                     .text = text, .size = size, .sink = &sink, .served = &served, .err_msg = &refused);
    // a view closed while the server took its file leaves the file with no view to let it go
    if (served && !EMBED_CALL(cli_view_full.served_set, CliViewFullCfg, .key = ask->key, .full = full))
    {
        served = 0;
        if (!EMBED_CALL(cli_view_full.serving, CliViewFullCfg, .full = full))
        {
            refused.at = 0ULL;
            (void)EMBED_CALL(cli_servers_path_out_err_msg_from_to.close, CliServersPathOutErrMsgFromToCfg, .path = full,
                             .err_msg = &refused);
        }
    }
    ask->served = served;
    g_cli_guard->persistent_buf_release(text, g_cli_servers_owner);
}

// a view opened, under `key` on the file at `path` from the tree's top, handed to its language's server; a view on no
// file, as a terminal's, served by none
static void panels_opened(const char *key, const char *path)
{
    if ((s_panels.root == NULL) || (path == NULL) || (path[0] == '\0'))
    {
        return;
    }
    PanelsServeAsk ask = {key, path, s_panels.root, "", 0ULL, 0ULL, 0};
    panels_serve_run(&ask);
    if (ask.served)
    {
        s_panels.view_sent = 0;
        s_panels.view_due_ms = cli_clock_milliseconds() + CLI_PANELS_VIEW_REST_MS;
    }
}

// the view under `key`, about to close, let go of its server, the server closed where no view stands served at its file
static void panels_closing(const char *key)
{
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = key, .full_out = full))
    {
        return;
    }
    // the view let go of its server first, so a view of the same file still served saves the server's copy
    (void)EMBED_CALL(cli_view_full.served_set, CliViewFullCfg, .key = key, .full = "");
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_view_full.serving, CliViewFullCfg, .full = full))
    {
        (void)EMBED_CALL(cli_servers_path_out_err_msg_from_to.close, CliServersPathOutErrMsgFromToCfg, .path = full,
                         .err_msg = &ignored);
    }
}

// the view's text sent to the server, where the view is served and its text changed since the server last took it
static void panels_sync_post(const PanelsCtx *args)
{
    (void)args;
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    char full[CLI_PATH_MAX_LEN];
    if ((key == NULL) || !EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = key, .full_out = full) ||
        !EMBED_CALL(cli_view_table.changed_alloc, CliViewCfg, .key = key))
    {
        return;
    }
    s_panels.changed = 0;
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = key, .size_pointer = &size,
                                  .owner = g_cli_servers_owner);
    if (text != NULL)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea ignored = {why, sizeof(why), 0ULL};
        (void)EMBED_CALL(cli_servers_root.change, CliServersRootCfg, .path = full, .text = text, .size = size,
                         .err_msg = &ignored);
        g_cli_guard->persistent_buf_release(text, g_cli_servers_owner);
    }
}

// the view's first and last lines sent to the server, with the lines opening the regions its first line stands inside,
// outermost first, read off the branch as its lines were colored
static void panels_view_run(const PanelsServeAsk *ask)
{
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = ask->key, .full_out = full))
    {
        return;
    }
    unsigned int opener[CLI_PANELS_OPENERS];
    // a line of a text a view shows is below 2^32
    const unsigned int openers =
        EMBED_CALL(cli_document_key_line.openers, CliDocumentKeyLineCfg, .key = ask->key,
                   .line = (unsigned int)ask->first, .opener = opener, .max_u32 = CLI_PANELS_OPENERS);
    char output_text[16];
    VerbaLinea output = {output_text, sizeof(output_text), 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_servers_path_out_err_msg.view, CliServersPathOutErrMsgCfg, .path = full,
                     .first = (unsigned int)ask->first, .last = (unsigned int)ask->last, .opener = opener,
                     .openers = openers, .out = &output, .err_msg = &ignored);
}

// where the view of the tab shown rests sent to the server
EMBED_INLINE void panels_view_send(const PanelsCtx *args)
{
    (void)args;
    s_panels.view_due_ms = 0ULL;
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    if ((key == NULL) || !EMBED_CALL(panels_served, PanelsCtx, .at = 0u))
    {
        return;
    }
    const unsigned long long first = s_panels.view_top;
    const unsigned int rows = s_panels.rows;
    unsigned long long last = first + ((rows > 0u) ? (rows - 1u) : 0u);
    last = ((s_panels.view_lines > 0ULL) && (last >= s_panels.view_lines)) ? (s_panels.view_lines - 1ULL) : last;
    const PanelsServeAsk ask = {key, "", "", "", first, last, 0};
    panels_view_run(&ask);
    s_panels.view_sent = 1;
    s_panels.view_rows = rows;
}

// a frame laid of a view: where it is the view shown's, its cursor, its top line of its lines and its rows taken as the
// panels follow them, the server asked again where the cursor or the view moved
static void panels_drawn(const CliViewDrawn *args)
{
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    if ((key == NULL) || !cellul.eq(&(CatenaFinitaCfg){.src = key, .other = args->key, .cap = CLI_NAME_MAX_LEN}))
    {
        return;
    }
    s_panels.rows = args->rows;
    s_panels.cursor = args->cursor;
    const int rested = s_panels.view_sent && (s_panels.view_top == args->top) && (s_panels.view_rows == args->rows);
    s_panels.view_top = args->top;
    s_panels.view_lines = args->lines;
    if (!rested && EMBED_CALL(panels_served, PanelsCtx, .at = 0u))
    {
        s_panels.view_sent = 0;
        s_panels.view_due_ms = cli_clock_milliseconds() + CLI_PANELS_VIEW_REST_MS;
    }
    EMBED_CALL(panels_completion_follow, PanelsCtx, .key = key);
    EMBED_CALL(panels_sync_post, PanelsCtx, .at = 0u);
    if (s_panels.completion_wanted)
    {
        EMBED_CALL(panels_completion_ask, PanelsCtx, .at = 0u);
    }
    const int moved = (args->cursor.line != s_panels.signature_position.line) ||
                      (args->cursor.col != s_panels.signature_position.col);
    if ((s_panels.signature != NULL) && !s_panels.signature_asking && moved)
    {
        EMBED_CALL(panels_signature_ask, PanelsCtx, .at = 0u);
    }
}

unsigned long long cli_servers_panels_due_ms(void)
{
    const unsigned long long dues[] = {s_panels.hover_due_ms, s_panels.hover_hide_ms, s_panels.signature_due_ms,
                                       s_panels.view_due_ms};
    unsigned long long earliest = 0ULL;
    for (unsigned int at = 0u; at < (sizeof(dues) / sizeof(dues[0])); at += 1u)
    {
        earliest = ((dues[at] != 0ULL) && ((earliest == 0ULL) || (dues[at] < earliest))) ? dues[at] : earliest;
    }
    return earliest;
}

int cli_servers_panels_step(void)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    int did = 0;
    if ((s_panels.hover_hide_ms != 0ULL) && (now_ms >= s_panels.hover_hide_ms))
    {
        EMBED_CALL(panels_hover_hide, PanelsCtx, .at = 0u);
        did = 1;
    }
    if ((s_panels.hover_due_ms != 0ULL) && (now_ms >= s_panels.hover_due_ms))
    {
        s_panels.hover_due_ms = 0ULL;
        s_panels.hover_ask += 1ULL;
        EMBED_CALL(panels_hover_ask, PanelsCtx, .at = 0u);
        did = 1;
    }
    if ((s_panels.signature_due_ms != 0ULL) && (now_ms >= s_panels.signature_due_ms))
    {
        s_panels.signature_due_ms = 0ULL;
        EMBED_CALL(panels_signature_ask, PanelsCtx, .at = 0u);
        did = 1;
    }
    if ((s_panels.view_due_ms != 0ULL) && (now_ms >= s_panels.view_due_ms))
    {
        EMBED_CALL(panels_view_send, PanelsCtx, .at = 0u);
        did = 1;
    }
    return did;
}

// the views' moments the panels take: each frame laid, each view opened and each view about to close
static const CliViewWatcher s_panels_watcher = {panels_drawn, panels_opened, panels_closing};

void cli_servers_panels_begin(const char *root)
{
    s_panels.root = root;
    cli_view_watch(&s_panels_watcher);
}

void cli_servers_panels_end(void)
{
    cli_view_watch(NULL);
    cli_servers_panels_hide();
    s_panels.root = NULL;
}

int cli_servers_panels_hover(CliHoverPanel *panel)
{
    if (s_panels.hover_text == NULL)
    {
        return 0;
    }
    *panel = (CliHoverPanel){s_panels.hover_text, s_panels.hover_length, s_panels.hover_line, s_panels.hover_from,
                             s_panels.hover_to};
    return 1;
}

int cli_servers_panels_completion(CliCompletionPanel *panel)
{
    const PanelsCompletion *const list = s_panels.completion;
    if ((list == NULL) || (list->count == 0u))
    {
        return 0;
    }
    const unsigned int rows =
        ((list->count - list->first) < CLI_COMPLETION_ROWS) ? (list->count - list->first) : CLI_COMPLETION_ROWS;
    *panel = (CliCompletionPanel){list->line, list->from, list->prefix, list->count, list->first, rows, list->chosen};
    return 1;
}

int cli_servers_panels_completion_row(unsigned int row, CliCompletionRow *out)
{
    const PanelsCompletion *const list = s_panels.completion;
    if ((list == NULL) || ((list->first + row) >= list->count))
    {
        return 0;
    }
    const PanelsItem *const item = &list->item[list->ranked[list->first + row]];
    // the prefix marks a label it starts, case aside; a prefix is far inside what an unsigned int represents
    *out = (CliCompletionRow){
        item->label,
        item->label_length,
        item->kind,
        item->detail,
        item->detail_length,
        ((list->prefix_length > 0ULL) && (item->score >= 4u)) ? (unsigned int)list->prefix_length : 0u};
    return 1;
}

const CliSignaturePanel *cli_servers_panels_signature(void)
{
    return s_panels.signature;
}
