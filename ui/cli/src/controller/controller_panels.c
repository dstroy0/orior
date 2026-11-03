// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_panels.c: the panels over the editor and the language server of the tabs. The hover rests on the pointer's
// cell, the tree answering first on a lane of its own and the server after on the servers' lane; the completion ranks
// the file's words at once and the server's items as they come; the signature waits on a bracket or a comma, the tree's
// first and the server's over it. Where each panel stands on the screen is the window's: the model holds what they say,
// and a press or a move names what it hit
#include "controller_internal.h"
#include "controller_work.h"

#include "../code/symbols.h"
#include "../command_line/cells.h"
#include "../editor/view.h"
#include "../plugins/plugins.h"
#include "../servers/servers.h"
#include "../servers/servers_internal.h"
#include "../text/text.h"

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
} ControllerItem;

// The completion's list in one cell: the server's items and the file's words its texts stand in, each released with the
// list; the items; those the prefix answers in rank order; the row chosen and the first row shown; and the prefix, as
// typed and in small letters, and where it stands
typedef struct
{
    CliServerItems *server;
    CliDocumentWords *words;
    ControllerItem *item;
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
} ControllerCompletion;

// The panels: the pointer's cell over the editor's text; when the hover is asked for and when it goes, 0 for neither;
// the ask whose answer stands; the hover's Markdown in a cell and the word's place; the list; whether a list is wanted
// as the next frame places the cursor and whether it was forced open; whether the server's items are being asked for
// and are to be asked for again; the signature, when it is asked for, the place it was asked at and whether it is being
// asked for; whether the text changed since the server was told and whether it is being told; and when the server is
// told where the view rests, and where it was last told
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
    ControllerCompletion *completion;
    int completion_wanted;
    int completion_forced;
    int items_asking;
    int items_again;
    CliSignaturePanel *signature;
    unsigned long long signature_due_ms;
    CliPosition signature_position;
    int signature_asking;
    CliPosition cursor;
    int changed;
    int telling;
    unsigned long long view_due_ms;
    int view_told;
    unsigned long long view_top;
    unsigned int view_rows;
    unsigned long long view_lines;
} ControllerPanels;

static ControllerPanels s_panels;

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
} ControllerHoverAsk;

// The server's items being asked for: the path, after it in its cell; the place; and what came
typedef struct
{
    const char *full;
    unsigned long long line;
    unsigned long long col;
    unsigned long long from;
    CliServerItems *items;
} ControllerItemsAsk;

// A signature being asked for: the view's key, the file's tree path and the path the server has it at, after it in its
// cell; the cursor's place; and what came
typedef struct
{
    const char *key;
    const char *path;
    const char *full;
    CliPosition cursor;
    CliSignaturePanel *panel;
} ControllerSignatureAsk;

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
} ControllerServeAsk;

typedef struct
{
    const char *key;
    const char *text;
    unsigned long long length;
    const char *lowered;
    const char *prefix;
    const char *prefix_lowered;
    ControllerItem *item;
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
} ControllerPanelsCtx;

// the key of the tab shown where its view stands, else NULL
EMBED_INLINE const char *panels_viewed_key(const ControllerPanelsCtx *args)
{
    (void)args;
    const char *const key = EMBED_CALL(cli_model.key, CliModelCtx, .at = 0u);
    return ((key != NULL) && g_cli_model.tab[g_cli_model.active]->viewed) ? key : NULL;
}

// whether the tab shown is served by its language server
EMBED_INLINE int panels_served(const ControllerPanelsCtx *args)
{
    (void)args;
    return (g_cli_model.active >= 0) && g_cli_model.tab[g_cli_model.active]->served;
}

// a byte a word holds
EMBED_INLINE int panels_word_byte(const ControllerPanelsCtx *args)
{
    const unsigned char value = (unsigned char)args->text[0];
    return ((value >= 'a') && (value <= 'z')) || ((value >= 'A') && (value <= 'Z')) ||
           ((value >= '0') && (value <= '9')) || (value == '_') || (value == '$') || (value >= 0x80u);
}

// the hover gone, a timer of its going with it
EMBED_INLINE void panels_hover_hide(const ControllerPanelsCtx *args)
{
    (void)args;
    if (s_panels.hover_text != NULL)
    {
        g_cli_cells->persistent_buf_release(s_panels.hover_text, &g_cli_controller_cells);
        s_panels.hover_text = NULL;
        s_panels.hover_length = 0ULL;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_HOVER);
    }
    s_panels.hover_hide_ms = 0ULL;
}

// the list closed, its cells released, and nothing forced open
EMBED_INLINE void panels_completion_close(const ControllerPanelsCtx *args)
{
    (void)args;
    ControllerCompletion *const list = s_panels.completion;
    s_panels.completion_forced = 0;
    if (list == NULL)
    {
        return;
    }
    s_panels.completion = NULL;
    g_cli_cells->persistent_buf_release(list->server, &g_cli_controller_cells);
    cli_view_words_release(list->words);
    g_cli_cells->persistent_buf_release(list, &g_cli_controller_cells);
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_COMPLETIONS);
}

// the signature gone, a wait for one with it
EMBED_INLINE void panels_signature_hide(const ControllerPanelsCtx *args)
{
    (void)args;
    if (s_panels.signature != NULL)
    {
        g_cli_cells->persistent_buf_release(s_panels.signature, &g_cli_controller_cells);
        s_panels.signature = NULL;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_SIGNATURE);
    }
    s_panels.signature_due_ms = 0ULL;
}

void cli_model_panels_hide(const CliModelCtx *args)
{
    (void)args;
    EMBED_CALL(panels_hover_hide, ControllerPanelsCtx, .at = 0u);
    EMBED_CALL(panels_completion_close, ControllerPanelsCtx, .at = 0u);
    EMBED_CALL(panels_signature_hide, ControllerPanelsCtx, .at = 0u);
    s_panels.hover_due_ms = 0ULL;
    s_panels.hover_ask += 1ULL;
    s_panels.completion_wanted = 0;
    s_panels.view_told = 0;
}

// whether the pointer stands on the hover's word, a unit either side
EMBED_INLINE int panels_pointer_on_word(const ControllerPanelsCtx *args)
{
    (void)args;
    const char *const key = EMBED_CALL(panels_viewed_key, ControllerPanelsCtx, .at = 0u);
    unsigned long long line = 0ULL;
    unsigned long long col = 0ULL;
    return s_panels.pointer_in_text && (s_panels.hover_text != NULL) && (key != NULL) &&
           EMBED_CALL(cli_view_row.position_at, CliViewRowCfg, .key = key, .row = s_panels.pointer_row,
                      .column = s_panels.pointer_column, .line = &line, .col = &col) &&
           (line == s_panels.hover_line) && ((col + 1ULL) >= s_panels.hover_from) && (col <= s_panels.hover_to);
}

void cli_model_panels_pointer(const CliModelCtx *args)
{
    const CliInput *const input = (const CliInput *)args->input;
    const unsigned long long now_ms = cli_clock_milliseconds();
    const int in_text =
        (input->hit == CLI_HIT_EDITOR) && (EMBED_CALL(panels_viewed_key, ControllerPanelsCtx, .at = 0u) != NULL);
    const int moved =
        !s_panels.pointer_in_text || (input->row != s_panels.pointer_row) || (input->column != s_panels.pointer_column);
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
    s_panels.pointer_row = in_text ? input->row : s_panels.pointer_row;
    s_panels.pointer_column = in_text ? input->column : s_panels.pointer_column;
    if (s_panels.hover_text == NULL)
    {
        return;
    }
    if ((input->hit == CLI_HIT_HOVER_BOX) || EMBED_CALL(panels_pointer_on_word, ControllerPanelsCtx, .at = 0u))
    {
        s_panels.hover_hide_ms = 0ULL;
    }
    else if (s_panels.hover_hide_ms == 0ULL)
    {
        s_panels.hover_hide_ms = now_ms + CLI_PANELS_HOVER_LINGER_MS;
    }
}

// a declaration of the hover's word written: its own line as code, then its kind and place
EMBED_INLINE void panels_declaration_write(const ControllerPanelsCtx *args)
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

// what runs on the tree's lane: the tree's declarations of the word laid as Markdown in a cell sized to them
static void panels_hover_tree_run(void *context)
{
    ControllerHoverAsk *const ask = (ControllerHoverAsk *)context;
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = ask->root);
    CliFoundDeclarations *const found =
        EMBED_CALL(cli_symbols_query_max.tree_declarations_alloc, CliSymbolsQueryMaxCfg, .query = ask->word_text,
                   .most = CLI_PANELS_DECLARATIONS, .owner = &g_cli_controller_cells);
    if (found == NULL)
    {
        return;
    }
    unsigned long long bytes = 1ULL;
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliFoundDeclaration *const one = &found->found[at];
        bytes += cellul.len(&(CatenaFinitaCfg){.src = one->text, .cap = CLI_CELLS_POOL}) +
                 cellul.len(&(CatenaFinitaCfg){.src = one->kind, .cap = CLI_CELLS_POOL}) +
                 cellul.len(&(CatenaFinitaCfg){.src = one->path, .cap = CLI_CELLS_POOL}) + 64ULL;
    }
    char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_controller_cells);
    VerbaLinea joined = mmgr_line_over(cell, bytes);
    unsigned int parts = 0u;
    for (unsigned int at = 0u; (cell != NULL) && (at < found->count) && (parts < CLI_PANELS_HOVER_DECLARATIONS);
         at += 1u)
    {
        const CliFoundDeclaration *const one = &found->found[at];
        if (cellul.eq(&(CatenaFinitaCfg){.src = one->name, .other = ask->word_text, .cap = CLI_PANELS_WORD}))
        {
            mmgr_line_put(&joined, (parts > 0u) ? CLI_PANELS_HOVER_BETWEEN : "");
            EMBED_CALL(panels_declaration_write, ControllerPanelsCtx, .found = one, .out = &joined);
            parts += 1u;
        }
    }
    g_cli_cells->persistent_buf_release(found, &g_cli_controller_cells);
    if ((parts == 0u) || !mmgr_line_ok(&joined))
    {
        g_cli_cells->persistent_buf_release(cell, &g_cli_controller_cells);
        return;
    }
    ask->text = cell;
    ask->length = joined.at;
}

// what runs on the servers' lane: the server's hover, its Markdown in a cell sized to it
static void panels_hover_server_run(void *context)
{
    ControllerHoverAsk *const ask = (ControllerHoverAsk *)context;
    char *const cell = g_cli_cells->temporary_buf_alloc(CLI_PANELS_ANSWER_CAPACITY, &g_cli_controller_cells);
    VerbaLinea output = mmgr_line_over(cell, CLI_PANELS_ANSWER_CAPACITY);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    // a place in a text a view shows stands below 2^32 lines and columns
    if ((cell != NULL) &&
        EMBED_CALL(cli_servers_path_out.hover, CliServersPathOutCfg, .path = ask->full, .line = (unsigned int)ask->line,
                   .col = (unsigned int)ask->col, .out = &output, .err_msg = &ignored) &&
        mmgr_line_ok(&output) && (output.at > 0ULL))
    {
        ask->text = g_cli_cells->persistent_buf_alloc(output.at, &g_cli_controller_cells);
        if (ask->text != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = ask->text, .src = output.out, .bytes = output.at});
            ask->length = output.at;
        }
    }
    g_cli_cells->persistent_buf_release(cell, &g_cli_controller_cells);
}

static void panels_hover_server_done(void *context);

// the tree's answer on the controller's thread, where the pointer rests where it was asked: it stands as the hover, and
// the server is asked after it
static void panels_hover_tree_done(void *context)
{
    ControllerHoverAsk *const ask = (ControllerHoverAsk *)context;
    const int current = ask->ask == s_panels.hover_ask;
    if (current)
    {
        EMBED_CALL(panels_hover_hide, ControllerPanelsCtx, .at = 0u);
        s_panels.hover_text = ask->text;
        s_panels.hover_length = ask->length;
        s_panels.hover_line = ask->line;
        s_panels.hover_from = ask->from;
        s_panels.hover_to = ask->to;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_HOVER);
    }
    else
    {
        g_cli_cells->persistent_buf_release(ask->text, &g_cli_controller_cells);
    }
    ask->text = NULL;
    ask->length = 0ULL;
    if (current && (ask->full[0] != '\0'))
    {
        EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_SERVERS,
                   .run = panels_hover_server_run, .done = panels_hover_server_done, .context = ask, .busy = 1);
        return;
    }
    g_cli_cells->persistent_buf_release(ask, &g_cli_controller_cells);
}

// the server's answer on the controller's thread, where the pointer rests where it was asked: it stands after the
// tree's
static void panels_hover_server_done(void *context)
{
    ControllerHoverAsk *const ask = (ControllerHoverAsk *)context;
    if ((ask->ask == s_panels.hover_ask) && (ask->text != NULL))
    {
        const unsigned long long between =
            (s_panels.hover_text != NULL) ? (sizeof(CLI_PANELS_HOVER_BETWEEN) - 1ULL) : 0ULL;
        char *const joined =
            g_cli_cells->persistent_buf_alloc(s_panels.hover_length + between + ask->length, &g_cli_controller_cells);
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
            EMBED_CALL(panels_hover_hide, ControllerPanelsCtx, .at = 0u);
            s_panels.hover_text = joined;
            s_panels.hover_length = length;
            s_panels.hover_line = ask->line;
            s_panels.hover_from = ask->from;
            s_panels.hover_to = ask->to;
            g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_HOVER);
        }
    }
    g_cli_cells->persistent_buf_release(ask->text, &g_cli_controller_cells);
    g_cli_cells->persistent_buf_release(ask, &g_cli_controller_cells);
}

// the hover asked for at the pointer's cell: a pointer resting on the word shown does not ask again, and a place no
// word stands at lets the hover shown go unless the pointer stands on it
EMBED_INLINE void panels_hover_ask(const ControllerPanelsCtx *args)
{
    (void)args;
    const char *const key = EMBED_CALL(panels_viewed_key, ControllerPanelsCtx, .at = 0u);
    if ((key == NULL) || (g_cli_model.root == NULL) ||
        EMBED_CALL(panels_pointer_on_word, ControllerPanelsCtx, .at = 0u))
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
        EMBED_CALL(panels_hover_hide, ControllerPanelsCtx, .at = 0u);
        return;
    }
    char full[CLI_PATH_MAX_LEN];
    full[0] = '\0';
    if (!EMBED_CALL(panels_served, ControllerPanelsCtx, .at = 0u) ||
        !EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = key, .full_out = full))
    {
        full[0] = '\0';
    }
    const unsigned long long word_size =
        cellul.len(&(CatenaFinitaCfg){.src = word_text, .cap = sizeof(word_text)}) + 1ULL;
    const unsigned long long root_size =
        cellul.len(&(CatenaFinitaCfg){.src = g_cli_model.root, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long full_size = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = sizeof(full)}) + 1ULL;
    ControllerHoverAsk *const ask =
        g_cli_cells->persistent_buf_alloc(sizeof(*ask) + word_size + root_size + full_size, &g_cli_controller_cells);
    if (ask == NULL)
    {
        return;
    }
    char *const text = (char *)(void *)(ask + 1);
    memor.cpy(&(MemoriaCfg){.dst = text, .src = word_text, .bytes = word_size});
    memor.cpy(&(MemoriaCfg){.dst = text + word_size, .src = g_cli_model.root, .bytes = root_size});
    memor.cpy(&(MemoriaCfg){.dst = text + word_size + root_size, .src = full, .bytes = full_size});
    *ask = (ControllerHoverAsk){
        s_panels.hover_ask, text, text + word_size, text + word_size + root_size, line, col, from, to, NULL, 0ULL};
    EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_TREE, .run = panels_hover_tree_run,
               .done = panels_hover_tree_done, .context = ask, .busy = 1);
}

// how the item scores against the prefix: 5 its label starts with it, 4 starts with it case aside, 3 holds it case
// aside, 2 holds its letters in order case aside, 1 for no prefix, 0 for no answer
EMBED_INLINE unsigned int panels_item_score(const ControllerPanelsCtx *args)
{
    const ControllerItem *const item = args->item;
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
static const ControllerCompletion *s_ranking;

// two items in rank order: the higher score first, then the shorter label, then the labels' bytes
static int panels_item_order(const void *one, const void *other)
{
    const ControllerItem *const first = &s_ranking->item[*(const unsigned int *)one];
    const ControllerItem *const second = &s_ranking->item[*(const unsigned int *)other];
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
EMBED_INLINE void panels_completion_rank(ControllerCompletion *list)
{
    const unsigned int saved = (list->count > 0u) ? list->ranked[list->chosen] : list->items;
    list->count = 0u;
    for (unsigned int at = 0u; at < list->items; at += 1u)
    {
        ControllerItem *const item = &list->item[at];
        item->score = EMBED_CALL(panels_item_score, ControllerPanelsCtx, .item = item, .length = list->prefix_length,
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
EMBED_INLINE void panels_completion_scrolled(ControllerCompletion *list)
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

// `length` bytes of a snippet `text` written as the text it inserts: a tab stop with a placeholder, `${n:text}`, as its
// placeholder, nested stops flattened the same way; one with choices, `${n|a,b|}`, as its first choice; a bare stop,
// `$n` or `${n}`, as nothing; and an escaped byte as itself
EMBED_INLINE void panels_snippet_write(const ControllerPanelsCtx *args)
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
        if (each >= length)
        {
            break;
        }
        if (text[each] == '\\')
        {
            const unsigned long long escaped = (each + 1ULL < length) ? (each + 1ULL) : each;
            mmgr_line_ch(out, text[escaped]);
            each = escaped + 1ULL;
            continue;
        }
        each += 1ULL;
        if ((each < length) && (text[each] != '{'))
        {
            each = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = text, .at = each,
                              .size = length, .classes = CLI_TEXT_DIGITS, .set = "");
            continue;
        }
        each += (each < length) ? 1ULL : 0ULL;
        each = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = text, .at = each,
                          .size = length, .classes = CLI_TEXT_DIGITS, .set = "");
        if ((each < length) && (text[each] == ':'))
        {
            each += 1ULL;
            unsigned long long depth = 1ULL;
            while ((each < length) && (depth > 0ULL))
            {
                const unsigned long long inner = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                            .text = text, .at = each, .size = length, .set = "\\$}");
                mmgr_line_put_n(out, text + each, inner - each);
                each = inner;
                if (each >= length)
                {
                    break;
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
            continue;
        }
        if ((each < length) && (text[each] == '|'))
        {
            each += 1ULL;
            const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                       .text = text, .at = each, .size = length, .set = ",|");
            mmgr_line_put_n(out, text + each, stop - each);
            each = stop;
        }
        each = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = each,
                          .size = length, .set = "}");
        each += (each < length) ? 1ULL : 0ULL;
    }
}

// the list laid in one cell from the server's items and the file's words, a word the server gave left out, the labels
// lowered once and each snippet flattened once: the list, holding both from here, or NULL with both let go where the
// cells cannot meet it
EMBED_INLINE ControllerCompletion *panels_completion_alloc(const ControllerPanelsCtx *args)
{
    const unsigned int from_server = (args->server != NULL) ? args->server->count : 0u;
    const unsigned int from_words = (args->words != NULL) ? args->words->count : 0u;
    const unsigned int most = from_server + from_words;
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; at < from_server; at += 1u)
    {
        const CliServerItem *const one = &args->server->item[at];
        bytes += cellul.len(&(CatenaFinitaCfg){.src = one->label, .cap = CLI_CELLS_POOL}) +
                 cellul.len(&(CatenaFinitaCfg){.src = one->insert, .cap = CLI_CELLS_POOL}) + 2ULL;
    }
    for (unsigned int at = 0u; at < from_words; at += 1u)
    {
        bytes += (unsigned long long)args->words->length[at] + 1ULL;
    }
    ControllerCompletion *const list =
        g_cli_cells->persistent_buf_alloc(sizeof(*list) + (most * sizeof(ControllerItem)) +
                                              (most * sizeof(unsigned int)) + (2ULL * CLI_PANELS_WORD) + bytes,
                                          &g_cli_controller_cells);
    if (list == NULL)
    {
        g_cli_cells->persistent_buf_release(args->server, &g_cli_controller_cells);
        cli_view_words_release(args->words);
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = list, .val = 0, .bytes = sizeof(*list)});
    list->server = args->server;
    list->words = args->words;
    list->item = (ControllerItem *)(void *)(list + 1);
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
            cellul.len(&(CatenaFinitaCfg){.src = one->label, .cap = CLI_CELLS_POOL});
        if (label_length == 0ULL)
        {
            continue;
        }
        ControllerItem *const item = &list->item[list->items];
        item->label = one->label;
        item->label_length = label_length;
        item->kind = one->kind;
        item->detail = one->detail;
        item->detail_length = cellul.len(&(CatenaFinitaCfg){.src = one->detail, .cap = CLI_CELLS_POOL});
        const unsigned long long insert_length =
            cellul.len(&(CatenaFinitaCfg){.src = one->insert, .cap = CLI_CELLS_POOL});
        const char *const insert = (insert_length != 0ULL) ? one->insert : one->label;
        const unsigned long long inserted = (insert_length != 0ULL) ? insert_length : label_length;
        if (one->snippet)
        {
            VerbaLinea flat = {text, inserted + 1ULL, 0ULL};
            EMBED_CALL(panels_snippet_write, ControllerPanelsCtx, .text = insert, .length = inserted, .out = &flat);
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
            for (unsigned int other = 0u; !supplied_already && (other < supplied); other += 1u)
            {
                supplied_already =
                    (list->item[other].label_length == label_length) &&
                    (memor.cmp(&(MemoriaCfg){.src = list->item[other].label, .other = label, .bytes = label_length}) ==
                     0);
            }
            if (supplied_already)
            {
                at += 1u;
                continue;
            }
            ControllerItem *const item = &list->item[list->items];
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
EMBED_INLINE void panels_completion_stand(ControllerCompletion *list)
{
    ControllerCompletion *const was = s_panels.completion;
    panels_completion_rank(list);
    if ((was != NULL) && (was->count > 0u))
    {
        const ControllerItem *const chosen = &was->item[was->ranked[was->chosen]];
        for (unsigned int at = 0u; at < list->count; at += 1u)
        {
            const ControllerItem *const item = &list->item[list->ranked[at]];
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
    const ControllerItem *const only = (list->count == 1u) ? &list->item[list->ranked[0]] : NULL;
    const int alone =
        (only != NULL) && (only->label_length == list->prefix_length) &&
        (memor.cmp(&(MemoriaCfg){.src = only->label, .other = list->prefix, .bytes = list->prefix_length}) == 0);
    const int empty = (list->prefix_length == 0ULL) && !s_panels.completion_forced && (was == NULL);
    if ((list->count == 0u) || (alone && !s_panels.completion_forced) || empty)
    {
        g_cli_cells->persistent_buf_release(list->server, &g_cli_controller_cells);
        cli_view_words_release(list->words);
        g_cli_cells->persistent_buf_release(list, &g_cli_controller_cells);
        EMBED_CALL(panels_completion_close, ControllerPanelsCtx, .at = 0u);
        return;
    }
    panels_completion_scrolled(list);
    if (was != NULL)
    {
        g_cli_cells->persistent_buf_release(was->server, &g_cli_controller_cells);
        cli_view_words_release(was->words);
        g_cli_cells->persistent_buf_release(was, &g_cli_controller_cells);
    }
    s_panels.completion = list;
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_COMPLETIONS);
}

static void panels_items_run(void *context);
static void panels_items_done(void *context);

// the server asked for the items at the cursor, one ask in flight at a time, the next wanted as the answer comes
EMBED_INLINE void panels_items_post(const ControllerPanelsCtx *args)
{
    if (s_panels.completion == NULL)
    {
        return;
    }
    if (s_panels.items_asking)
    {
        s_panels.items_again = 1;
        return;
    }
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(panels_served, ControllerPanelsCtx, .at = 0u) ||
        !EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = args->key, .full_out = full))
    {
        return;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = sizeof(full)}) + 1ULL;
    ControllerItemsAsk *const ask = g_cli_cells->persistent_buf_alloc(sizeof(*ask) + size, &g_cli_controller_cells);
    if (ask == NULL)
    {
        return;
    }
    char *const copy = (char *)(void *)(ask + 1);
    memor.cpy(&(MemoriaCfg){.dst = copy, .src = full, .bytes = size});
    *ask = (ControllerItemsAsk){copy, s_panels.completion->line, s_panels.cursor.col, s_panels.completion->from, NULL};
    s_panels.items_asking = 1;
    s_panels.items_again = 0;
    EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_SERVERS, .run = panels_items_run,
               .done = panels_items_done, .context = ask, .busy = 1);
}

// what runs on the servers' lane: the server's items
static void panels_items_run(void *context)
{
    ControllerItemsAsk *const ask = (ControllerItemsAsk *)context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    // a place in a text a view shows stands below 2^32 lines and columns
    ask->items =
        EMBED_CALL(cli_servers_ask.items_alloc, CliServersAskCfg, .path = ask->full, .line = (unsigned int)ask->line,
                   .col = (unsigned int)ask->col, .owner = &g_cli_controller_cells, .err_msg = &ignored);
}

// what comes back on the controller's thread: where the list still stands on the prefix asked about, it is laid again
// with the server's items and the words it had, and an answer for a prefix the cursor has left is dropped
static void panels_items_done(void *context)
{
    ControllerItemsAsk *const ask = (ControllerItemsAsk *)context;
    ControllerCompletion *const list = s_panels.completion;
    s_panels.items_asking = 0;
    if ((list != NULL) && (ask->items != NULL) && (list->line == ask->line) && (list->from == ask->from))
    {
        CliDocumentWords *const words = list->words;
        list->words = NULL;
        ControllerCompletion *const laid =
            EMBED_CALL(panels_completion_alloc, ControllerPanelsCtx, .server = ask->items, .words = words,
                       .line = list->line, .from = list->from, .prefix = list->prefix, .length = list->prefix_length);
        if (laid != NULL)
        {
            panels_completion_stand(laid);
        }
    }
    else
    {
        g_cli_cells->persistent_buf_release(ask->items, &g_cli_controller_cells);
    }
    g_cli_cells->persistent_buf_release(ask, &g_cli_controller_cells);
    const char *const key = EMBED_CALL(panels_viewed_key, ControllerPanelsCtx, .at = 0u);
    if (s_panels.items_again && (key != NULL))
    {
        EMBED_CALL(panels_items_post, ControllerPanelsCtx, .key = key);
    }
}

// the completions asked for the word before the cursor: the file's words at once and the server's items after, those it
// gave for the same prefix's start standing until it answers again
EMBED_INLINE void panels_completion_ask(const ControllerPanelsCtx *args)
{
    (void)args;
    s_panels.completion_wanted = 0;
    const char *const key = EMBED_CALL(panels_viewed_key, ControllerPanelsCtx, .at = 0u);
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
    ControllerCompletion *const was = s_panels.completion;
    CliServerItems *server = NULL;
    if ((was != NULL) && (was->line == cursor.line) && (was->from == from))
    {
        server = was->server;
        was->server = NULL;
    }
    ControllerCompletion *const list =
        EMBED_CALL(panels_completion_alloc, ControllerPanelsCtx, .server = server, .words = cli_view_words_alloc(key),
                   .line = cursor.line, .from = from, .prefix = prefix,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = sizeof(prefix)}));
    if (list != NULL)
    {
        panels_completion_stand(list);
    }
    EMBED_CALL(panels_items_post, ControllerPanelsCtx, .key = key);
}

// the list, where it is open, followed to the frame's cursor: filtered again by the word before the cursor, and closed
// where the cursor has left the line or that word no longer starts where the prefix did
EMBED_INLINE void panels_completion_follow(const ControllerPanelsCtx *args)
{
    ControllerCompletion *const list = s_panels.completion;
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
        EMBED_CALL(panels_completion_close, ControllerPanelsCtx, .at = 0u);
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = sizeof(prefix)});
    if ((length == 0ULL) && !s_panels.completion_forced)
    {
        EMBED_CALL(panels_completion_close, ControllerPanelsCtx, .at = 0u);
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = list->prefix, .src = prefix, .bytes = length + 1ULL});
    memor.cpy(&(MemoriaCfg){.dst = list->lowered, .src = prefix, .bytes = length + 1ULL});
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .size = length, .text_out = list->lowered);
    list->prefix_length = length;
    panels_completion_rank(list);
    if (list->count == 0u)
    {
        EMBED_CALL(panels_completion_close, ControllerPanelsCtx, .at = 0u);
        return;
    }
    panels_completion_scrolled(list);
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_COMPLETIONS);
}

// the row `at` of the list accepted: its text written over the prefix before the cursor, the list closed
EMBED_INLINE void panels_completion_accept(const ControllerPanelsCtx *args)
{
    const ControllerCompletion *const list = s_panels.completion;
    if ((list == NULL) || (args->at >= list->count))
    {
        EMBED_CALL(panels_completion_close, ControllerPanelsCtx, .at = 0u);
        return;
    }
    const ControllerItem *const item = &list->item[list->ranked[args->at]];
    const unsigned long long back = ((s_panels.cursor.line == list->line) && (s_panels.cursor.col >= list->from))
                                        ? (s_panels.cursor.col - list->from)
                                        : list->prefix_length;
    const CliViewInput accepted = {
        .kind = CLI_VIEW_COMPLETE, .text = item->insert, .length = item->insert_length, .column = back};
    EMBED_CALL(cli_model.view_input, CliModelCtx, .input = &accepted, .on = 1);
    EMBED_CALL(panels_completion_close, ControllerPanelsCtx, .at = 0u);
}

void cli_model_completion_accept(const CliModelCtx *args)
{
    EMBED_CALL(panels_completion_accept, ControllerPanelsCtx, .at = args->at);
}

void cli_model_completion_wheel(const CliModelCtx *args)
{
    ControllerCompletion *const list = s_panels.completion;
    if ((list == NULL) || (list->count <= CLI_COMPLETION_ROWS))
    {
        return;
    }
    const unsigned int before = list->first;
    const unsigned int most = list->count - CLI_COMPLETION_ROWS;
    if (args->on < 0)
    {
        // the rows turned up, a count far inside what an unsigned int represents
        const unsigned int up = (unsigned int)(-args->on);
        list->first = (list->first > up) ? (list->first - up) : 0u;
    }
    else
    {
        // the rows turned are not negative
        list->first += (unsigned int)args->on;
        list->first = (list->first > most) ? most : list->first;
    }
    panels_completion_scrolled(list);
    g_cli_model.dirty |= (list->first != before) ? CLI_MODEL_DIRTY(CLI_SNAPSHOT_COMPLETIONS) : 0u;
}

// a key's effect on the open list, 1 where the key was the list's
EMBED_INLINE int panels_completion_key(const ControllerPanelsCtx *args)
{
    ControllerCompletion *const list = s_panels.completion;
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
        EMBED_CALL(panels_completion_accept, ControllerPanelsCtx, .at = list->chosen);
        return 1;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Escape", .cap = 64ULL}))
    {
        EMBED_CALL(panels_completion_close, ControllerPanelsCtx, .at = 0u);
        return 1;
    }
    else
    {
        return 0;
    }
    panels_completion_scrolled(list);
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_COMPLETIONS);
    return 1;
}

int cli_model_panels_key(const CliModelCtx *args)
{
    const char *const name = args->path;
    if (EMBED_CALL(panels_completion_key, ControllerPanelsCtx, .text = name))
    {
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Escape", .cap = 64ULL}) &&
        ((s_panels.signature != NULL) || (s_panels.hover_text != NULL)))
    {
        EMBED_CALL(cli_model.panels_hide, CliModelCtx, .at = 0u);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Mod+Space", .cap = 64ULL}))
    {
        s_panels.completion_forced = 1;
        EMBED_CALL(panels_completion_ask, ControllerPanelsCtx, .at = 0u);
        return 1;
    }
    EMBED_CALL(panels_hover_hide, ControllerPanelsCtx, .at = 0u);
    // a key that takes letters out saves the list open on the prefix left; any other key closes it
    if (s_panels.completion != NULL)
    {
        const int edits = cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Backspace", .cap = 64ULL}) ||
                          cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Delete", .cap = 64ULL});
        s_panels.completion_wanted = edits;
        if (!edits)
        {
            EMBED_CALL(panels_completion_close, ControllerPanelsCtx, .at = 0u);
        }
    }
    return 0;
}

void cli_model_panels_text(const CliModelCtx *args)
{
    EMBED_CALL(panels_hover_hide, ControllerPanelsCtx, .at = 0u);
    if ((args->length > 0ULL) &&
        EMBED_CALL(panels_word_byte, ControllerPanelsCtx, .text = args->path + args->length - 1ULL))
    {
        s_panels.completion_wanted = 1;
    }
    else
    {
        s_panels.completion_wanted = 0;
        EMBED_CALL(panels_completion_close, ControllerPanelsCtx, .at = 0u);
    }
    if (EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->path, .at = 0ULL,
                   .size = args->length, .set = "(,") < args->length)
    {
        s_panels.signature_due_ms = cli_clock_milliseconds() + CLI_PANELS_SIGNATURE_WAIT_MS;
    }
}

// the panel of `panel`, with the parameters the tree's `own` found the arguments to differ from, in a cell of its own
EMBED_INLINE CliSignaturePanel *panels_signature_joined(const ControllerPanelsCtx *args)
{
    const CliSignaturePanel *const panel = args->panel;
    const unsigned int differing = (args->own != NULL) ? args->own->differing : 0u;
    CliSignaturePanel *const joined = g_cli_cells->persistent_buf_alloc(
        sizeof(*joined) + (panel->parameters * sizeof(*joined->parameter)) + (differing * sizeof(*joined->differs)) +
            panel->length + panel->doc_length + 2ULL,
        &g_cli_controller_cells);
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
EMBED_INLINE void panels_signature_ask(const ControllerPanelsCtx *args)
{
    (void)args;
    const char *const key = EMBED_CALL(panels_viewed_key, ControllerPanelsCtx, .at = 0u);
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
    if (!EMBED_CALL(panels_served, ControllerPanelsCtx, .at = 0u) ||
        !EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = key, .full_out = full))
    {
        full[0] = '\0';
    }
    const unsigned long long key_size = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long path_size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = sizeof(path)}) + 1ULL;
    const unsigned long long full_size = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = sizeof(full)}) + 1ULL;
    ControllerSignatureAsk *const ask =
        g_cli_cells->persistent_buf_alloc(sizeof(*ask) + key_size + path_size + full_size, &g_cli_controller_cells);
    if (ask == NULL)
    {
        return;
    }
    char *const text = (char *)(void *)(ask + 1);
    memor.cpy(&(MemoriaCfg){.dst = text, .src = key, .bytes = key_size});
    memor.cpy(&(MemoriaCfg){.dst = text + key_size, .src = path, .bytes = path_size});
    memor.cpy(&(MemoriaCfg){.dst = text + key_size + path_size, .src = full, .bytes = full_size});
    *ask = (ControllerSignatureAsk){text, text + key_size, text + key_size + path_size, cursor, NULL};
    s_panels.signature_position = cursor;
    s_panels.signature_asking = 1;
    EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_TREE,
               .run = panels_signature_tree_run, .done = panels_signature_tree_done, .context = ask, .busy = 1);
}

// what runs on the tree's lane: the text's lines from as far above as CLI_PANELS_SIGNATURE_LINES reach to the cursor's,
// a paged text's those the page holds, read from the state the first was entered in, and the tree's signature of the
// call
static void panels_signature_tree_run(void *context)
{
    ControllerSignatureAsk *const ask = (ControllerSignatureAsk *)context;
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
                   .size_pointer = &size, .state = state, .capacity = sizeof(state), .owner = &g_cli_controller_cells);
    if (text == NULL)
    {
        return;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    ask->panel = EMBED_CALL(cli_symbols_path_text_size_out.signature_alloc, CliSymbolsPathTextSizeOutCfg,
                            .path = ask->path, .text = text, .size = size, .owner = &g_cli_controller_cells,
                            .state = (state[0] != '\0') ? state : "root", .line = line - top, .col = ask->cursor.col,
                            .err_msg = &ignored);
    g_cli_cells->persistent_buf_release(text, &g_cli_controller_cells);
}

// what comes back on the controller's thread: the tree's answer stands, and where the cursor stands where it was asked
// the server is asked over it; a cursor moved with a signature shown asks again
static void panels_signature_tree_done(void *context)
{
    ControllerSignatureAsk *const ask = (ControllerSignatureAsk *)context;
    EMBED_CALL(panels_signature_hide, ControllerPanelsCtx, .at = 0u);
    s_panels.signature = ask->panel;
    ask->panel = NULL;
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_SIGNATURE);
    const int still = (s_panels.cursor.line == ask->cursor.line) && (s_panels.cursor.col == ask->cursor.col);
    if (still && (ask->full[0] != '\0'))
    {
        EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_SERVERS,
                   .run = panels_signature_server_run, .done = panels_signature_server_done, .context = ask, .busy = 1);
        return;
    }
    s_panels.signature_asking = 0;
    g_cli_cells->persistent_buf_release(ask, &g_cli_controller_cells);
    if (!still && (s_panels.signature != NULL))
    {
        EMBED_CALL(panels_signature_ask, ControllerPanelsCtx, .at = 0u);
    }
}

// what runs on the servers' lane: the server's signature at the place
static void panels_signature_server_run(void *context)
{
    ControllerSignatureAsk *const ask = (ControllerSignatureAsk *)context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    // a place in a text a view shows stands below 2^32 lines and columns
    ask->panel = EMBED_CALL(cli_servers_ask.signature_alloc, CliServersAskCfg, .path = ask->full,
                            .line = (unsigned int)ask->cursor.line, .col = (unsigned int)ask->cursor.col,
                            .owner = &g_cli_controller_cells, .err_msg = &ignored);
}

// what comes back on the controller's thread: the server's answer stands over the tree's, whose differing parameters it
// keeps
static void panels_signature_server_done(void *context)
{
    ControllerSignatureAsk *const ask = (ControllerSignatureAsk *)context;
    const int still = (s_panels.cursor.line == ask->cursor.line) && (s_panels.cursor.col == ask->cursor.col);
    s_panels.signature_asking = 0;
    if (still && (ask->panel != NULL))
    {
        CliSignaturePanel *const joined =
            EMBED_CALL(panels_signature_joined, ControllerPanelsCtx, .panel = ask->panel, .own = s_panels.signature);
        if (joined != NULL)
        {
            EMBED_CALL(panels_signature_hide, ControllerPanelsCtx, .at = 0u);
            s_panels.signature = joined;
            g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_SIGNATURE);
        }
    }
    g_cli_cells->persistent_buf_release(ask->panel, &g_cli_controller_cells);
    g_cli_cells->persistent_buf_release(ask, &g_cli_controller_cells);
    if (!still && (s_panels.signature != NULL))
    {
        EMBED_CALL(panels_signature_ask, ControllerPanelsCtx, .at = 0u);
    }
}

// a cell of a view's ask to its server: the texts copied after it, `key`, `path`, `root` and `full`, each empty for
// none
EMBED_INLINE ControllerServeAsk *panels_serve_ask_alloc(const char *key, const char *path, const char *root,
                                                        const char *full)
{
    const unsigned long long key_size = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long path_size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long root_size = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long full_size = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    ControllerServeAsk *const ask = g_cli_cells->persistent_buf_alloc(
        sizeof(*ask) + key_size + path_size + root_size + full_size, &g_cli_controller_cells);
    if (ask == NULL)
    {
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = ask, .val = 0, .bytes = sizeof(*ask)});
    char *const text = (char *)(void *)(ask + 1);
    memor.cpy(&(MemoriaCfg){.dst = text, .src = key, .bytes = key_size});
    memor.cpy(&(MemoriaCfg){.dst = text + key_size, .src = path, .bytes = path_size});
    memor.cpy(&(MemoriaCfg){.dst = text + key_size + path_size, .src = root, .bytes = root_size});
    memor.cpy(&(MemoriaCfg){.dst = text + key_size + path_size + root_size, .src = full, .bytes = full_size});
    ask->key = text;
    ask->path = text + key_size;
    ask->root = text + key_size + path_size;
    ask->full = text + key_size + path_size + root_size;
    return ask;
}

// what runs on the servers' lane: the view's text handed to its language's server, where it is read whole and its
// language has a server that takes it, and the view marked served as one step against a server letting a file go
static void panels_serve_run(void *context)
{
    ControllerServeAsk *const ask = (ControllerServeAsk *)context;
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = ask->root, .relative = ask->path, .joined = full);
    char id[CLI_NAME_MAX_LEN];
    CliLanguages languages;
    EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &languages);
    const int known = EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                 .languages_read = &languages, .path = ask->path, .id_out = id);
    EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = &languages);
    unsigned long long size = 0ULL;
    char *const text = known ? EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = ask->key,
                                          .size_pointer = &size, .owner = &g_cli_controller_cells)
                             : NULL;
    if (text == NULL)
    {
        return;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea refused = {why, sizeof(why), 0ULL};
    const CliServersSink sink = EMBED_CALL(cli_model_problems.sink, CliModelCtx, .path = ask->root);
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
    g_cli_cells->persistent_buf_release(text, &g_cli_controller_cells);
}

// what comes back on the controller's thread: the tab marked served, and a view served told to the server as its rows
// rest
static void panels_serve_done(void *context)
{
    ControllerServeAsk *const ask = (ControllerServeAsk *)context;
    for (unsigned int at = 0u; ask->served && (at < g_cli_model.tabs); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_model.tab[at]->key, .other = ask->key, .cap = CLI_PATH_MAX_LEN}))
        {
            g_cli_model.tab[at]->served = 1;
            s_panels.view_told = 0;
            s_panels.view_due_ms = cli_clock_milliseconds() + CLI_PANELS_VIEW_REST_MS;
        }
    }
    g_cli_cells->persistent_buf_release(ask, &g_cli_controller_cells);
}

void cli_model_server_serve(const CliModelCtx *args)
{
    if ((args->at >= g_cli_model.tabs) || (g_cli_model.root == NULL))
    {
        return;
    }
    const CliModelTab *const tab = g_cli_model.tab[args->at];
    ControllerServeAsk *const ask = panels_serve_ask_alloc(tab->key, tab->path, g_cli_model.root, "");
    if (ask != NULL)
    {
        EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_SERVERS, .run = panels_serve_run,
                   .done = panels_serve_done, .context = ask, .busy = 1);
    }
}

// what runs on the servers' lane: the server of the file let go of it, unless a view of the file is served by now
static void panels_close_run(void *context)
{
    const ControllerServeAsk *const ask = (const ControllerServeAsk *)context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_view_full.serving, CliViewFullCfg, .full = ask->full))
    {
        (void)EMBED_CALL(cli_servers_path_out_err_msg_from_to.close, CliServersPathOutErrMsgFromToCfg,
                         .path = ask->full, .err_msg = &ignored);
    }
}

// what comes back on the controller's thread of any of the servers' asks whose answer no one reads
static void panels_ask_release(void *context)
{
    g_cli_cells->persistent_buf_release(context, &g_cli_controller_cells);
}

void cli_model_server_close(const CliModelCtx *args)
{
    if ((args->at >= g_cli_model.tabs) || !g_cli_model.tab[args->at]->served || (g_cli_model.root == NULL))
    {
        return;
    }
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = g_cli_model.root,
               .relative = g_cli_model.tab[args->at]->path, .joined = full);
    ControllerServeAsk *const ask = panels_serve_ask_alloc("", "", "", full);
    if (ask != NULL)
    {
        EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_SERVERS, .run = panels_close_run,
                   .done = panels_ask_release, .context = ask);
    }
}

void cli_model_server_changed(const CliModelCtx *args)
{
    (void)args;
    s_panels.changed = 1;
}

// what runs on the servers' lane: the server told the view's text, where the view is served and its text changed since
// it was last told
static void panels_tell_run(void *context)
{
    const ControllerServeAsk *const ask = (const ControllerServeAsk *)context;
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = ask->key, .full_out = full) ||
        !EMBED_CALL(cli_view_table.changed_alloc, CliViewCfg, .key = ask->key))
    {
        return;
    }
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = ask->key,
                                  .size_pointer = &size, .owner = &g_cli_controller_cells);
    if (text != NULL)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea ignored = {why, sizeof(why), 0ULL};
        (void)EMBED_CALL(cli_servers_root.change, CliServersRootCfg, .path = full, .text = text, .size = size,
                         .err_msg = &ignored);
        g_cli_cells->persistent_buf_release(text, &g_cli_controller_cells);
    }
}

static void panels_tell_done(void *context);

// the server told the text changed where it is served, one telling in flight at a time
static void panels_tell_post(const ControllerPanelsCtx *args)
{
    (void)args;
    const char *const key = EMBED_CALL(panels_viewed_key, ControllerPanelsCtx, .at = 0u);
    if (!s_panels.changed || s_panels.telling || (key == NULL) ||
        !EMBED_CALL(panels_served, ControllerPanelsCtx, .at = 0u))
    {
        return;
    }
    ControllerServeAsk *const ask = panels_serve_ask_alloc(key, "", "", "");
    if (ask != NULL)
    {
        s_panels.changed = 0;
        s_panels.telling = 1;
        EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_SERVERS, .run = panels_tell_run,
                   .done = panels_tell_done, .context = ask);
    }
}

static void panels_tell_done(void *context)
{
    g_cli_cells->persistent_buf_release(context, &g_cli_controller_cells);
    s_panels.telling = 0;
    EMBED_CALL(panels_tell_post, ControllerPanelsCtx, .at = 0u);
}

// what runs on the servers' lane: the server told the view's first and last lines and the lines opening the regions its
// first line stands inside, outermost first, read off the branch as its lines were colored
static void panels_view_run(void *context)
{
    const ControllerServeAsk *const ask = (const ControllerServeAsk *)context;
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

// the server told where the view of the tab shown rests
EMBED_INLINE void panels_view_tell(const ControllerPanelsCtx *args)
{
    (void)args;
    s_panels.view_due_ms = 0ULL;
    const char *const key = EMBED_CALL(panels_viewed_key, ControllerPanelsCtx, .at = 0u);
    if ((key == NULL) || !EMBED_CALL(panels_served, ControllerPanelsCtx, .at = 0u))
    {
        return;
    }
    const unsigned long long first = s_panels.view_top;
    const unsigned int rows = g_cli_model.rows;
    unsigned long long last = first + ((rows > 0u) ? (rows - 1u) : 0u);
    last = ((s_panels.view_lines > 0ULL) && (last >= s_panels.view_lines)) ? (s_panels.view_lines - 1ULL) : last;
    ControllerServeAsk *const ask = panels_serve_ask_alloc(key, "", "", "");
    if (ask == NULL)
    {
        return;
    }
    ask->first = first;
    ask->last = last;
    s_panels.view_told = 1;
    s_panels.view_rows = rows;
    EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_SERVERS, .run = panels_view_run,
               .done = panels_ask_release, .context = ask);
}

void cli_model_panels_frame(const CliModelCtx *args)
{
    const CliViewFrame *const frame = (const CliViewFrame *)args->input;
    const char *const key = EMBED_CALL(panels_viewed_key, ControllerPanelsCtx, .at = 0u);
    if ((frame == NULL) || (key == NULL))
    {
        return;
    }
    s_panels.cursor = frame->cursor;
    const int rested =
        s_panels.view_told && (s_panels.view_top == frame->top) && (s_panels.view_rows == g_cli_model.rows);
    s_panels.view_top = frame->top;
    s_panels.view_lines = frame->lines;
    if (!rested && EMBED_CALL(panels_served, ControllerPanelsCtx, .at = 0u))
    {
        s_panels.view_told = 0;
        s_panels.view_due_ms = cli_clock_milliseconds() + CLI_PANELS_VIEW_REST_MS;
    }
    EMBED_CALL(panels_completion_follow, ControllerPanelsCtx, .key = key);
    EMBED_CALL(panels_tell_post, ControllerPanelsCtx, .at = 0u);
    if (s_panels.completion_wanted)
    {
        EMBED_CALL(panels_completion_ask, ControllerPanelsCtx, .at = 0u);
    }
    const int moved = (frame->cursor.line != s_panels.signature_position.line) ||
                      (frame->cursor.col != s_panels.signature_position.col);
    if ((s_panels.signature != NULL) && !s_panels.signature_asking && moved)
    {
        EMBED_CALL(panels_signature_ask, ControllerPanelsCtx, .at = 0u);
    }
}

unsigned long long cli_model_panels_due_ms(const CliModelCtx *args)
{
    (void)args;
    const unsigned long long dues[] = {s_panels.hover_due_ms, s_panels.hover_hide_ms, s_panels.signature_due_ms,
                                       s_panels.view_due_ms};
    unsigned long long earliest = 0ULL;
    for (unsigned int at = 0u; at < (sizeof(dues) / sizeof(dues[0])); at += 1u)
    {
        earliest = ((dues[at] != 0ULL) && ((earliest == 0ULL) || (dues[at] < earliest))) ? dues[at] : earliest;
    }
    return earliest;
}

int cli_model_panels_step(const CliModelCtx *args)
{
    (void)args;
    const unsigned long long now_ms = cli_clock_milliseconds();
    int did = 0;
    if ((s_panels.hover_hide_ms != 0ULL) && (now_ms >= s_panels.hover_hide_ms))
    {
        EMBED_CALL(panels_hover_hide, ControllerPanelsCtx, .at = 0u);
        did = 1;
    }
    if ((s_panels.hover_due_ms != 0ULL) && (now_ms >= s_panels.hover_due_ms))
    {
        s_panels.hover_due_ms = 0ULL;
        s_panels.hover_ask += 1ULL;
        EMBED_CALL(panels_hover_ask, ControllerPanelsCtx, .at = 0u);
        did = 1;
    }
    if ((s_panels.signature_due_ms != 0ULL) && (now_ms >= s_panels.signature_due_ms))
    {
        s_panels.signature_due_ms = 0ULL;
        EMBED_CALL(panels_signature_ask, ControllerPanelsCtx, .at = 0u);
        did = 1;
    }
    if ((s_panels.view_due_ms != 0ULL) && (now_ms >= s_panels.view_due_ms))
    {
        EMBED_CALL(panels_view_tell, ControllerPanelsCtx, .at = 0u);
        did = 1;
    }
    return did;
}

void *cli_model_hover_alloc(const CliModelCtx *args)
{
    (void)args;
    if (s_panels.hover_text == NULL)
    {
        return NULL;
    }
    CliHoverPanel *const panel =
        g_cli_cells->persistent_buf_alloc(sizeof(*panel) + s_panels.hover_length, &g_cli_controller_cells);
    if (panel == NULL)
    {
        return NULL;
    }
    char *const text = (char *)(void *)(panel + 1);
    memor.cpy(&(MemoriaCfg){.dst = text, .src = s_panels.hover_text, .bytes = s_panels.hover_length});
    *panel = (CliHoverPanel){text, s_panels.hover_length, s_panels.hover_line, s_panels.hover_from, s_panels.hover_to};
    return panel;
}

void *cli_model_completions_alloc(const CliModelCtx *args)
{
    (void)args;
    const ControllerCompletion *const list = s_panels.completion;
    if ((list == NULL) || (list->count == 0u))
    {
        return NULL;
    }
    const unsigned int rows =
        ((list->count - list->first) < CLI_COMPLETION_ROWS) ? (list->count - list->first) : CLI_COMPLETION_ROWS;
    unsigned long long bytes = list->prefix_length + 1ULL;
    for (unsigned int row = 0u; row < rows; row += 1u)
    {
        const ControllerItem *const item = &list->item[list->ranked[list->first + row]];
        bytes += item->label_length + item->detail_length +
                 cellul.len(&(CatenaFinitaCfg){.src = item->kind, .cap = CLI_CELLS_POOL}) + 3ULL;
    }
    CliCompletionPanel *const panel = g_cli_cells->persistent_buf_alloc(
        sizeof(*panel) + (rows * sizeof(CliCompletionRow)) + bytes, &g_cli_controller_cells);
    if (panel == NULL)
    {
        return NULL;
    }
    CliCompletionRow *const row_of = (CliCompletionRow *)(void *)(panel + 1);
    char *text = (char *)(void *)(row_of + rows);
    memor.cpy(&(MemoriaCfg){.dst = text, .src = list->prefix, .bytes = list->prefix_length + 1ULL});
    *panel = (CliCompletionPanel){list->line, list->from, text, list->count, list->first, rows, list->chosen, row_of};
    text += list->prefix_length + 1ULL;
    for (unsigned int row = 0u; row < rows; row += 1u)
    {
        const ControllerItem *const item = &list->item[list->ranked[list->first + row]];
        const unsigned long long kind = cellul.len(&(CatenaFinitaCfg){.src = item->kind, .cap = CLI_CELLS_POOL}) + 1ULL;
        row_of[row].label = text;
        row_of[row].label_length = item->label_length;
        memor.cpy(&(MemoriaCfg){.dst = text, .src = item->label, .bytes = item->label_length});
        text[item->label_length] = '\0';
        text += item->label_length + 1ULL;
        row_of[row].kind = text;
        memor.cpy(&(MemoriaCfg){.dst = text, .src = item->kind, .bytes = kind});
        text += kind;
        row_of[row].detail = text;
        row_of[row].detail_length = item->detail_length;
        memor.cpy(&(MemoriaCfg){.dst = text, .src = item->detail, .bytes = item->detail_length});
        text[item->detail_length] = '\0';
        text += item->detail_length + 1ULL;
        // the prefix marks a label it starts, case aside; a prefix is far inside what an unsigned int represents
        row_of[row].marked =
            ((list->prefix_length > 0ULL) && (item->score >= 4u)) ? (unsigned int)list->prefix_length : 0u;
    }
    return panel;
}

void *cli_model_signature_alloc(const CliModelCtx *args)
{
    (void)args;
    return (s_panels.signature != NULL) ? EMBED_CALL(panels_signature_joined, ControllerPanelsCtx,
                                                     .panel = s_panels.signature, .own = s_panels.signature)
                                        : NULL;
}
