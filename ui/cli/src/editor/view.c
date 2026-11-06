// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// view.c: the editor's view of a stored text. The views are saved in one cell under a lock of their own; a view reads
// the text's lines under the documents' lock and edits it through cli_document_edit, one edit a time, each recorded in
// a step with the text it took out, so the step is undone by writing that text back over what it put in. A column is
// in UTF-16 units, as the text's lines are saved, and the pointer's cell is read into a column with tabs laid to their
// stops
#include "view.h"
#include "../command_line/callback_cfg.h"

#include "../code/symbols.h"
#include "../command_line/pool.h"
#include "../plugins/plugins.h"
#include "../text/edits.h"
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "document_internal.h"
#include "editorconfig.h"
#include "pages.h"
#include "view_edits.h"
#include "vim.h"

#define CLI_VIEW_PAIRS 16u
#define CLI_VIEW_QUOTES 8u
#define CLI_VIEW_PATTERN 512u
// the bytes a language's comment mark takes at the most
#define CLI_VIEW_COMMENT 16u
// the steps Shrink Selection can take back; past them the oldest goes
#define CLI_VIEW_GROWN 32u
// An edit a step holds, one cell with its texts after it: the span it replaced, where the text it put in ends, that
// text and the text it took out, and the step's edits before and after it
typedef struct CliViewEdit
{
    CliPosition from;
    CliPosition to;
    CliPosition end;
    char *put;
    unsigned long long append_length;
    char *consumed;
    unsigned long long consumed_length;
    struct CliViewEdit *before;
    struct CliViewEdit *after;
} CliViewEdit;
// A step: its first and last edits, how many, the kind of edit it holds and when its last was written, and where the
// cursor and the anchor stood before it and after it
typedef struct
{
    CliViewEdit *first;
    CliViewEdit *last;
    unsigned int edits;
    unsigned int kind;
    unsigned long long at_ms;
    CliPosition cursor_before;
    CliPosition anchor_before;
    CliPosition cursor_after;
    CliPosition anchor_after;
} CliViewStep;
// A fold closed: the line it opens on, shown, and the last line it hides
typedef struct
{
    unsigned long long first;
    unsigned long long last;
} CliViewFold;
// A view: the key of its text, the path of its file, which names its language, the whole path its language server
// was handed, empty where none was, whether the text changed since the server was last told, and whether the text is
// paged, its lines read through the pages; the language's pairs, each an opener and its closer, its quotes, and its
// pattern of a line that opens a block and its comment marks, as its plugin names them, in units; its top, rows and
// columns; the cursor and the anchor; the column the cursor saves as it moves up and down, in cells; the selections
// Expand Selection grew from, each its cursor and its anchor, how many, and the selection it last made; the
// indentation; the frames written; the steps, in a cell of their own, how many are stored and how many of them are
// done, those past the done ones being undone; the folds closed, in a cell of their own in the order of their first
// lines, how many and the work state; find: whether its bar is open and whether the keys are its own, its query in a
// cell of its own, in units, its length, the work state and where its cursor stands, whether it seeks a pattern, with
// case and whole words, whether the matches are to be found anew, the matches, each a line, a from and a to column, in
// a cell of their own, how many and the work state, and the current one, -1 for none; and the line read last, its units
// in a cell of the view's own, the work state it has, which line it is and whether it is known: a line's units are read
// once for the moves along it; and whether its close waits on a second press, its text changed
typedef struct
{
    int used;
    int closing;
    char key[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char served[CLI_PATH_MAX_LEN];
    int changed;
    int paged;
    unsigned int pair_open[CLI_VIEW_PAIRS];
    unsigned int pair_close[CLI_VIEW_PAIRS];
    unsigned int pairs;
    unsigned int quote[CLI_VIEW_QUOTES];
    unsigned int quotes;
    char indent_after[CLI_VIEW_PATTERN];
    char comment_line[CLI_VIEW_COMMENT];
    char comment_open[CLI_VIEW_COMMENT];
    char comment_close[CLI_VIEW_COMMENT];
    int find_open;
    int find_focus;
    unsigned short *find_query;
    unsigned long long find_length;
    unsigned long long find_capacity;
    unsigned long long find_at;
    int find_regex;
    int find_case;
    int find_word;
    int find_stale;
    unsigned long long (*match)[3];
    unsigned long long matches;
    unsigned long long match_capacity;
    long long current;
    unsigned long long top;
    unsigned int rows;
    unsigned int columns;
    CliPosition cursor;
    CliPosition anchor;
    unsigned long long goal;
    int goal_saved;
    CliPosition grown[CLI_VIEW_GROWN][2];
    unsigned int grown_count;
    CliPosition grown_cursor;
    CliPosition grown_anchor;
    CliIndent indent;
    unsigned long long seq;
    CliViewStep *step;
    unsigned int steps;
    unsigned int steps_capacity;
    unsigned int done;
    const CliViewEdit *save_point;
    int save_point_lost;
    CliViewFold *fold;
    unsigned int folds;
    unsigned int folds_capacity;
    unsigned short *line_units;
    unsigned long long units_capacity;
    unsigned long long line_length;
    unsigned long long line_cached;
    int line_known;
} CliView;
// The rows a frame shows: their line numbers and, for a paged text's lines read, their texts, in one cell; and how many
typedef struct
{
    unsigned long long *number;
    const char **text;
    unsigned long long count;
} CliViewRows;
// A fold of the text that opens on one of a frame's rows: its first line, the last it hides and whether it shows the
// line it closes on
typedef struct
{
    unsigned long long first;
    unsigned long long last;
    int displayed;
} CliViewFoldSeen;
// The folds among a frame's rows as they are found: the rows, the folds seen, one a row at the most, and how many
typedef struct
{
    const CliViewRows *displayed;
    CliViewFoldSeen *seen;
    unsigned long long count;
} CliViewFoldsSeen;
// the size of the views table, the most edits a step holds before it closes, the kinds of edit, the lines read
// about the cursor each way for the bracket pair at it, the work state for the state the first of them is entered in,
// the most pairs and quotes a language has, and the work state for its pattern of a line that opens a block
#define CLI_VIEWS 64u
// the columns a frame cuts its rows at while nothing has told the view its size
#define CLI_VIEW_UNSIZED_COLUMNS 256ULL
#define CLI_VIEW_STEP_EDITS 256u
#define CLI_VIEW_FOLDS 65536u
#define CLI_VIEW_STEPS 4096u
#define CLI_VIEW_UNITS_MIN 256ULL
#define CLI_VIEW_FIND_UNITS 65536ULL
#define CLI_VIEW_PAIR_LINES 3000ULL
#define CLI_VIEW_STATE 1024u
// the matches find saves at the most
#define CLI_VIEW_MATCHES 100000ULL
#define CLI_VIEW_KIND_NONE 0u
#define CLI_VIEW_KIND_TYPE 1u
#define CLI_VIEW_KIND_SPACE 2u
#define CLI_VIEW_KIND_DELETE 3u
#define CLI_VIEW_KIND_DELETE_FORWARD 4u

static CliView *s_views;
// the place of the view shown and of the one shown before it, each -1 for none
static int s_shown = -1;
static int s_last = -1;
// what watches the views, NULL for none
static const CliViewWatcher *s_watcher;

typedef struct
{
    const char *key;
    const char *name;
    const char *required;
    CliViewStep *step;
    CliView *view;
    unsigned int from_u32;
    const CliJson *json;
    unsigned int node;
    char *mark;
    unsigned long long capacity;
    const CliView *view_read;
    unsigned long long line;
    unsigned long long count;
    unsigned long long top;
    unsigned long long most;
    unsigned long long first;
    unsigned long long last;
    const CliEdit *edit;
    const char *text;
    unsigned long long size;
    unsigned short *units;
    unsigned long long units_u64;
    unsigned long long col;
    unsigned long long cell;
    unsigned int unit;
    CliPosition place;
    CliPosition one;
    CliPosition other;
    CliPosition *from_pointer;
    CliPosition *to_pointer;
    int extend;
    int saved;
    long long by;
    unsigned long long more;
    const unsigned short *units_read;
    unsigned long long from_u64;
    unsigned long long to_u64;
    int forward;
    int mod;
    int shift;
    int alt;
    int exact;
    const CliEdit *const *edits;
    const unsigned int *counts;
    VerbaLinea *err_msg;
    CliPosition from;
    CliPosition to;
    unsigned long long bytes;
    VerbaLinea *out;
    unsigned long long append_bytes;
    unsigned long long append_spans;
    CliViewEdit *made;
    unsigned int kind;
    unsigned long long length;
    CliPosition cursor;
    CliPosition anchor;
    unsigned int open;
    unsigned int close;
    int quoted;
    unsigned int letter;
    CliPosition *pair;
    unsigned long long *from_u64s;
    unsigned long long *to_u64s;
    unsigned long long *first_pointer;
    unsigned long long *last_pointer;
    CliPosition end;
    int after;
    int up;
    int deeper;
    int below;
    const unsigned long long *start;
    unsigned int *order;
    unsigned int *work;
    unsigned int count_u32;
    int (*compare)(const CliCompareCfg *args);
    int descending;
    int unique;
    int upper;
    unsigned int mode;
    int *reserved;
    int *mod_pointer;
    int *shift_pointer;
    int *alt_pointer;
    const char *supplied;
    const CliViewInput *input;
    const CliViewRows *displayed;
    const CliViewFoldsSeen *folds;
    unsigned long long otherwise;
    unsigned int colors;
    int *any;
    unsigned long long start_value;
    unsigned long long width;
    unsigned long long name_u64;
    unsigned int flat;
    unsigned int tab;
    char byte;
    unsigned long long (*table)[2];
    unsigned long long at;
    CliViewRows *rows_out;
    CliViewFrame *frame;
    int tabs;
    int marked;
    int selected;
    CliPosition *cursor_pointer;
    CliPosition *anchor_pointer;
} ViewCtx;
// the view open under `key`, or NULL; under the lock
EMBED_INLINE CliView *view_get(const ViewCtx *args)
{
    if (s_views == NULL)
    {
        return NULL;
    }
    for (unsigned int at = 0u; at < CLI_VIEWS; at += 1u)
    {
        if (s_views[at].used &&
            cellul.eq(&(CatenaFinitaCfg){.src = s_views[at].key, .other = args->key, .cap = CLI_NAME_MAX_LEN}))
        {
            return &s_views[at];
        }
    }
    return NULL;
}

// whether a key's own name is `required`
EMBED_INLINE int view_by_name(const ViewCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = args->required, .cap = 64ULL});
}

EMBED_INLINE void view_steps_cut(const ViewCtx *args)
{
    for (unsigned int at = args->from_u32; at < args->view->steps; at += 1u)
    {
        for (CliViewEdit *edit = (&args->view->step[at])->first; edit != NULL;)
        {
            CliViewEdit *const after = edit->after;
            args->view->save_point_lost = args->view->save_point_lost || (edit == args->view->save_point);
            g_cli_guard->persistent_buf_release(edit, g_cli_views_owner);
            edit = after;
        }
    }
    args->view->steps = (args->from_u32 < args->view->steps) ? args->from_u32 : args->view->steps;
    args->view->done = (args->view->done < args->view->steps) ? args->view->done : args->view->steps;
}

// A language's plugin sought among the plugins: its id, and the view its pairs, quotes and pattern are read into
typedef struct
{
    const char *id;
    CliView *view;
} CliViewPluginNeedle;

// the first letter of the string node `node` as a unit, 0 where the node is no string or it is empty
EMBED_INLINE unsigned int view_letter_get(const ViewCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = node, .length_pointer = &length);
    unsigned long long consumed = 0ULL;
    return ((text != NULL) && (length != 0ULL)) ? EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text,
                                                             .size = length, .at = 0ULL, .length = &consumed)
                                                : 0u;
}

// the pairs, quotes and pattern of the plugin whose id is sought read into the view; where the plugin names none,
// the defaults apply: the brackets, the two quotes and the pattern of an opener at a line's end
// a comment's mark read off the string at `node` of the plugin's tree into `mark`, sized for `capacity`, empty where
// none
EMBED_INLINE void view_comment_read(const ViewCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = node, .length_pointer = &length);
    VerbaLinea line = {args->mark, args->capacity, 0ULL};
    mmgr_line_put_n(&line, (text != NULL) ? text : "", (text != NULL) ? length : 0ULL);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        args->mark[0] = '\0';
    }
}

// the brackets a language pairs, the array at `node` of the plugin's `json`, each two letters, an opener and its
// closer, put among the view's; the round, square and curly brackets where the plugin gives none
EMBED_INLINE void view_pairs_read(const ViewCtx *args)
{
    CliView *const view = args->view;
    const CliJson *const json = args->json;
    view->pairs = 0u;
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = args->node,
                    .kind = CFG_JSON_ARRAY))
    {
        view->pair_open[0] = '(';
        view->pair_close[0] = ')';
        view->pair_open[1] = '[';
        view->pair_close[1] = ']';
        view->pair_open[2] = '{';
        view->pair_close[2] = '}';
        view->pairs = 3u;
        return;
    }
    for (unsigned int at = 0u; (at < json->node[args->node].count) && (view->pairs < CLI_VIEW_PAIRS); at += 1u)
    {
        unsigned long long length = 0ULL;
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = args->node, .index = at),
                       .length_pointer = &length);
        unsigned long long consumed = 0ULL;
        const unsigned int open = ((text != NULL) && (length != 0ULL))
                                      ? EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = length,
                                                   .at = 0ULL, .length = &consumed)
                                      : 0u;
        const unsigned int close = ((open != 0u) && (consumed < length))
                                       ? EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = length,
                                                    .at = consumed, .length = &consumed)
                                       : 0u;
        if ((open != 0u) && (close != 0u))
        {
            view->pair_open[view->pairs] = open;
            view->pair_close[view->pairs] = close;
            view->pairs += 1u;
        }
    }
}

static void view_plugin_read(const CliEachPluginCfg *args)
{
    CliViewPluginNeedle *const needle = args->context;
    CliView *const view = needle->view;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = needle->id, .cap = CLI_NAME_MAX_LEN}))
    {
        return;
    }
    char error_text[256];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->plugin->text, .size = args->plugin->size, .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = &why, .owner = g_cli_views_owner);
    if (root == CLI_JSON_NONE)
    {
        return;
    }
    const unsigned int pairs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = root, .name = "pairs");
    const unsigned int quotes = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                           .object = root, .name = "quotes");
    const unsigned int after = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = root, .name = "indentAfter");
    const unsigned int comments = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                             .object = root, .name = "comments");
    EMBED_CALL(view_comment_read, ViewCtx, .mark = view->comment_line, .capacity = sizeof(view->comment_line),
               .json = &json,
               .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                  .object = comments, .name = "line"));
    const unsigned int block = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = comments, .name = "block");
    EMBED_CALL(view_comment_read, ViewCtx, .mark = view->comment_open, .capacity = sizeof(view->comment_open),
               .json = &json,
               .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json,
                                  .array = block, .index = 0u));
    EMBED_CALL(view_comment_read, ViewCtx, .mark = view->comment_close, .capacity = sizeof(view->comment_close),
               .json = &json,
               .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json,
                                  .array = block, .index = 1u));
    view->quotes = 0u;
    EMBED_CALL(view_pairs_read, ViewCtx, .view = view, .json = &json, .node = pairs);
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = quotes,
                   .kind = CFG_JSON_ARRAY))
    {
        for (unsigned int at = 0u; (at < json.node[quotes].count) && (view->quotes < CLI_VIEW_QUOTES); at += 1u)
        {
            const unsigned int quote =
                EMBED_CALL(view_letter_get, ViewCtx, .json = &json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = &json, .array = quotes, .index = at));
            if (quote != 0u)
            {
                view->quote[view->quotes] = quote;
                view->quotes += 1u;
            }
        }
    }
    else
    {
        view->quote[0] = '"';
        view->quote[1] = '\'';
        view->quotes = 2u;
    }
    VerbaLinea pattern = {view->indent_after, sizeof(view->indent_after), 0ULL};
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = &json, .node = after, .length_pointer = &length);
    if (text != NULL)
    {
        mmgr_line_put_n(&pattern, text, length);
    }
    else
    {
        mmgr_line_put(&pattern, "[{[(]\\s*$");
    }
    if (mmgr_line_finish(&pattern) == 0ULL)
    {
        view->indent_after[0] = '\0';
    }
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
}

// the view's language read off the plugin the file at its path opens as; none where no plugin does
EMBED_INLINE void view_language_read(const ViewCtx *args)
{
    char id[CLI_NAME_MAX_LEN];
    CliLanguages languages;
    EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &languages);
    const int named = EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                 .languages_read = &languages, .path = args->view->path, .id_out = id);
    g_cli_guard->persistent_buf_release(languages.pair, g_cli_plugins_owner);
    *(&languages) = (CliLanguages){NULL, 0u, 0u};
    if (named)
    {
        CliViewPluginNeedle needle = {id, args->view};
        EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = view_plugin_read, .context = &needle);
    }
}

int cli_view_open(const CliViewCfg *args)
{
    const char *key = args->key;
    const char *path = args->path;
    const CliStoredText *const stored = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = key);
    const int paged = (stored != NULL) && stored->paged;
    if (stored == NULL)
    {
        return 0;
    }
    if (s_views == NULL)
    {
        s_views = g_cli_guard->persistent_buf_alloc(CLI_VIEWS * sizeof(*s_views), g_cli_views_owner);
        if (s_views == NULL)
        {
            return 0;
        }
        memor.set(&(MemoriaCfg){.dst = s_views, .val = 0, .bytes = CLI_VIEWS * sizeof(*s_views)});
    }
    CliView *view = EMBED_CALL(view_get, ViewCtx, .key = key);
    if (view != NULL)
    {
        EMBED_CALL(view_steps_cut, ViewCtx, .view = view, .from_u32 = 0u);
        g_cli_guard->persistent_buf_release(view->step, g_cli_views_owner);
        g_cli_guard->persistent_buf_release(view->fold, g_cli_views_owner);
        g_cli_guard->persistent_buf_release(view->find_query, g_cli_views_owner);
        g_cli_guard->persistent_buf_release(view->match, g_cli_views_owner);
        g_cli_guard->persistent_buf_release(view->line_units, g_cli_views_owner);
    }
    else
    {
        unsigned int at = 0u;
        while ((view == NULL) && (at < CLI_VIEWS))
        {
            view = s_views[at].used ? NULL : &s_views[at];
            at += 1u;
        }
    }
    if (view == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = view, .val = 0, .bytes = sizeof(*view)});
    view->used = 1;
    VerbaLinea named = {view->key, sizeof(view->key), 0ULL};
    mmgr_line_put(&named, key);
    (void)mmgr_line_finish(&named);
    VerbaLinea pathed = {view->path, sizeof(view->path), 0ULL};
    mmgr_line_put(&pathed, path);
    (void)mmgr_line_finish(&pathed);
    view->rows = 1u;
    view->paged = paged;
    EMBED_CALL(cli_editorconfig.indent, CliEditorconfigCfg, .path = path, .indent = &view->indent);
    EMBED_CALL(view_language_read, ViewCtx, .view = view);
    if ((s_watcher != NULL) && (s_watcher->opened != NULL))
    {
        s_watcher->opened(view->key, view->path);
    }
    return 1;
}

void cli_view_watch(const CliViewWatcher *watcher)
{
    s_watcher = watcher;
}

void cli_view_close(const CliViewCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if ((view != NULL) && (s_watcher != NULL) && (s_watcher->closing != NULL))
    {
        s_watcher->closing(view->key);
    }
    if (view != NULL)
    {
        EMBED_CALL(cli_vim_view.close, CliVimCfg, .key = view->key);
        EMBED_CALL(view_steps_cut, ViewCtx, .view = view, .from_u32 = 0u);
        g_cli_guard->persistent_buf_release(view->step, g_cli_views_owner);
        g_cli_guard->persistent_buf_release(view->fold, g_cli_views_owner);
        g_cli_guard->persistent_buf_release(view->find_query, g_cli_views_owner);
        g_cli_guard->persistent_buf_release(view->match, g_cli_views_owner);
        g_cli_guard->persistent_buf_release(view->line_units, g_cli_views_owner);
        // the views after it move up a place, so the table stands in the order the views were opened; a view's place
        // is far inside what an int counts
        const int closed = (int)(view - s_views);
        const unsigned int count = cli_view_count();
        for (unsigned int at = (unsigned int)closed + 1u; at < count; at += 1u)
        {
            s_views[at - 1u] = s_views[at];
        }
        memor.set(&(MemoriaCfg){.dst = &s_views[count - 1u], .val = 0, .bytes = sizeof(*view)});
        s_last = (s_last == closed) ? -1 : (s_last - ((s_last > closed) ? 1 : 0));
        if (s_shown == closed)
        {
            // the view beside it shown, the one after it where there is one
            s_shown = (closed < (int)count - 1) ? closed : (closed - 1);
        }
        else
        {
            s_shown -= (s_shown > closed) ? 1 : 0;
        }
    }
}

unsigned int cli_view_count(void)
{
    unsigned int count = 0u;
    while ((s_views != NULL) && (count < CLI_VIEWS) && s_views[count].used)
    {
        count += 1u;
    }
    return count;
}

const char *cli_view_key(unsigned int at)
{
    return (at < cli_view_count()) ? s_views[at].key : NULL;
}

const char *cli_view_path_at(unsigned int at)
{
    return (at < cli_view_count()) ? s_views[at].path : NULL;
}

const char *cli_view_name_at(unsigned int at)
{
    const char *const path = cli_view_path_at(at);
    if (path == NULL)
    {
        return NULL;
    }
    return path + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = path,
                             .at = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}), .set = "/\\");
}

int cli_view_shown(void)
{
    return s_shown;
}

int cli_view_last(void)
{
    return s_last;
}

void cli_view_show(unsigned int at)
{
    if (at < cli_view_count())
    {
        // a view's place is far inside what an int counts
        s_last = ((s_shown >= 0) && (s_shown != (int)at)) ? s_shown : s_last;
        s_shown = (int)at;
    }
}

int cli_view_closing(unsigned int at)
{
    return (at < cli_view_count()) && s_views[at].closing;
}

void cli_view_closing_set(unsigned int at, int closing)
{
    if (at < cli_view_count())
    {
        s_views[at].closing = closing;
    }
}

// the text's lines: how many, as far as the pages' index has read for a paged text
EMBED_INLINE unsigned long long view_lines(const ViewCtx *args)
{
    if (args->view_read->paged)
    {
        unsigned long long lines = 0ULL;
        int whole = 0;
        return EMBED_CALL(cli_pages.count, CliPagesCfg, .key = args->view_read->key, .lines = &lines, .whole = &whole)
                   ? lines
                   : 0ULL;
    }
    CliShape shape;
    return EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = args->view_read->key, .shape = &shape)
               ? shape.lines
               : 0ULL;
}

// the closed fold that hides `line`, its first line above the line and its last at or below it, the outermost where
// more do, or NULL where none does
EMBED_INLINE const CliViewFold *view_hide_timer(const ViewCtx *args)
{
    for (unsigned int at = 0u; at < args->view_read->folds; at += 1u)
    {
        if ((args->view_read->fold[at].first < args->line) && (args->line <= args->view_read->fold[at].last))
        {
            return &args->view_read->fold[at];
        }
    }
    return NULL;
}

// `line` where no closed fold hides it, else going up the first line of the fold that hides it, and going down the
// line after the last it hides, the last line shown where none follows
EMBED_INLINE unsigned long long view_displayed_up(const ViewCtx *args)
{
    unsigned long long line = args->line;
    for (const CliViewFold *hide_timer =
             EMBED_CALL(view_hide_timer, ViewCtx, .view_read = args->view_read, .line = line);
         hide_timer != NULL;
         hide_timer = EMBED_CALL(view_hide_timer, ViewCtx, .view_read = args->view_read, .line = line))
    {
        line = hide_timer->first;
    }
    return line;
}

EMBED_INLINE unsigned long long view_displayed_down(const ViewCtx *args)
{
    unsigned long long line = args->line;
    const unsigned long long lines = view_lines(args);
    const unsigned long long last = (lines != 0ULL) ? lines - 1ULL : 0ULL;
    for (const CliViewFold *hide_timer =
             EMBED_CALL(view_hide_timer, ViewCtx, .view_read = args->view_read, .line = line);
         hide_timer != NULL;
         hide_timer = EMBED_CALL(view_hide_timer, ViewCtx, .view_read = args->view_read, .line = line))
    {
        if (hide_timer->last >= last)
        {
            return EMBED_CALL(view_displayed_up, ViewCtx, .view_read = args->view_read, .line = last);
        }
        line = hide_timer->last + 1ULL;
    }
    return (line > last) ? EMBED_CALL(view_displayed_up, ViewCtx, .view_read = args->view_read, .line = last) : line;
}

// the line `count` lines shown below `line`, the last shown where fewer follow; and the one `count` lines shown above
// it, the first where fewer lead
EMBED_INLINE unsigned long long view_down(const ViewCtx *args)
{
    const unsigned long long lines = view_lines(args);
    unsigned long long at = view_displayed_up(args);
    for (unsigned long long step = 0ULL; (step < args->count) && (at + 1ULL < lines); step += 1ULL)
    {
        const unsigned long long next =
            EMBED_CALL(view_displayed_down, ViewCtx, .view_read = args->view_read, .line = at + 1ULL);
        if (next <= at)
        {
            break;
        }
        at = next;
    }
    return at;
}

EMBED_INLINE unsigned long long view_up(const ViewCtx *args)
{
    unsigned long long at = view_displayed_up(args);
    for (unsigned long long step = 0ULL; (step < args->count) && (at > 0ULL); step += 1ULL)
    {
        at = EMBED_CALL(view_displayed_up, ViewCtx, .view_read = args->view_read, .line = at - 1ULL);
    }
    return at;
}

// how many lines shown stand from `top` to before `line`, `most` at the most
EMBED_INLINE unsigned long long view_rows_to(const ViewCtx *args)
{
    unsigned long long rows = 0ULL;
    unsigned long long at = EMBED_CALL(view_displayed_up, ViewCtx, .view_read = args->view_read, .line = args->top);
    while ((at < args->line) && (rows < args->most))
    {
        const unsigned long long next =
            EMBED_CALL(view_down, ViewCtx, .view_read = args->view_read, .line = at, .count = 1ULL);
        if (next <= at)
        {
            break;
        }
        at = next;
        rows += 1ULL;
    }
    return rows;
}

// A fold sought among the text's: the line sought, the fold found, its first line and the last it hides, and whether
// one was
typedef struct
{
    unsigned long long line;
    unsigned long long first;
    unsigned long long last;
    int found;
} CliViewFoldNeedle;

// the text's fold opened on the line sought taken; the innermost of those that open on it or above it and reach it
static void view_fold_match(const CliFoldEachCfg *args)
{
    unsigned long long first = args->first;
    unsigned long long last = args->last;
    CliViewFoldNeedle *const needle = args->context;
    if (first == needle->line)
    {
        needle->first = first;
        needle->last = last;
        needle->found = 1;
    }
}

static void view_fold_innermost(const CliFoldEachCfg *args)
{
    unsigned long long first = args->first;
    unsigned long long last = args->last;
    CliViewFoldNeedle *const needle = args->context;
    if ((first <= needle->line) && (needle->line <= last) && (!needle->found || (first > needle->first)))
    {
        needle->first = first;
        needle->last = last;
        needle->found = 1;
    }
}

// the closed fold opened on `first` given the last line the text's fold there hides now
static void view_fold_refresh(const CliFoldEachCfg *args)
{
    unsigned long long last = args->last;
    CliView *const view = args->context;
    for (unsigned int at = 0u; at < view->folds; at += 1u)
    {
        if (view->fold[at].first == args->first)
        {
            view->fold[at].last = last;
        }
    }
}

// the closed folds' last lines read again from the text's folds, which its coloring moves; a closed fold the text no
// longer folds at stays until an edit touches it
EMBED_INLINE void view_folds_refresh(const ViewCtx *args)
{
    if (args->view->folds != 0u)
    {
        (void)EMBED_CALL(cli_document_context.folds_each, CliDocumentContextCfg, .key = args->view->key,
                         .each_fold_each = view_fold_refresh, .context = args->view);
    }
}

// a fold from `first` to `last` put among the closed, in the order of their first lines, the one there given the last
// where one is: 1, or 0 where the cell could not be taken
EMBED_INLINE int view_fold_append(const ViewCtx *args)
{
    unsigned long long last = args->last;
    unsigned int at = 0u;
    while ((at < args->view->folds) && (args->view->fold[at].first < args->first))
    {
        at += 1u;
    }
    if ((at < args->view->folds) && (args->view->fold[at].first == args->first))
    {
        args->view->fold[at].last = last;
        return 1;
    }
    if (args->view->folds_capacity == 0u)
    {
        args->view->fold =
            g_cli_guard->persistent_buf_alloc(CLI_VIEW_FOLDS * sizeof(*args->view->fold), g_cli_views_owner);
        args->view->folds_capacity = (args->view->fold != NULL) ? CLI_VIEW_FOLDS : 0u;
    }
    if ((args->view->fold == NULL) || (args->view->folds == args->view->folds_capacity))
    {
        return 0;
    }
    for (unsigned int after = args->view->folds; after > at; after -= 1u)
    {
        args->view->fold[after] = args->view->fold[after - 1u];
    }
    args->view->fold[at] = (CliViewFold){args->first, last};
    args->view->folds += 1u;
    return 1;
}

// the closed folds moved with `edit`: those below the lines it writes over moved by the lines it adds less those it
// takes out; one it touches, its first line with lines added or taken, or the lines it hides, opened
EMBED_INLINE void view_folds_edit(const ViewCtx *args)
{
    unsigned long long added = 0ULL;
    for (unsigned long long at = 0ULL; at < args->edit->length; at += 1ULL)
    {
        added += (args->edit->text[at] == '\n') ? 1ULL : 0ULL;
    }
    const unsigned long long removed = args->edit->to.line - args->edit->from.line;
    unsigned int saved = 0u;
    for (unsigned int at = 0u; at < args->view->folds; at += 1u)
    {
        CliViewFold fold = args->view->fold[at];
        if (args->edit->to.line < fold.first)
        {
            // a fold below the edit stands past the lines it takes out: the move stays in the text
            fold.first = fold.first + added - removed;
            fold.last = fold.last + added - removed;
        }
        else if ((args->edit->from.line < fold.first) || (args->edit->from.line <= fold.last))
        {
            const int on_first =
                (args->edit->from.line == fold.first) && (args->edit->to.line == fold.first) && (added == 0ULL);
            if (!on_first)
            {
                continue;
            }
        }
        args->view->fold[saved] = fold;
        saved += 1u;
    }
    args->view->folds = saved;
}

// `size` bytes of UTF-8 as UTF-16 units into `units`, which is sized for `capacity`: their count, a letter past the
// basic plane two units
EMBED_INLINE unsigned long long view_units_get(const ViewCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long at = 0ULL;
    unsigned long long count = 0ULL;
    while ((at < size) && (count < args->capacity))
    {
        unsigned long long consumed = 0ULL;
        const unsigned int letter =
            EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = size, .at = at, .length = &consumed);
        at += (consumed != 0ULL) ? consumed : 1ULL;
        if (letter >= 0x10000u)
        {
            if (count + 2ULL > args->capacity)
            {
                break;
            }
            // a letter past the basic plane as its two surrogates, each a unit
            args->units[count] = (unsigned short)(0xd800u + ((letter - 0x10000u) >> 10u));
            args->units[count + 1ULL] = (unsigned short)(0xdc00u + ((letter - 0x10000u) & 0x3ffu));
            count += 2ULL;
        }
        else
        {
            // a letter of the basic plane is one unit
            args->units[count] = (unsigned short)letter;
            count += 1ULL;
        }
    }
    return count;
}

// the view's line cell given work state for `units`: 1, or 0 where it could not be taken
EMBED_INLINE int view_units_capacity(const ViewCtx *args)
{
    if (args->view->units_capacity >= args->units_u64)
    {
        return 1;
    }
    // the line's units are read anew into a cell of their count
    const unsigned long long capacity = (args->units_u64 > CLI_VIEW_UNITS_MIN) ? args->units_u64 : CLI_VIEW_UNITS_MIN;
    g_cli_guard->persistent_buf_release(args->view->line_units, g_cli_views_owner);
    args->view->line_units =
        g_cli_guard->persistent_buf_alloc(capacity * sizeof(*args->view->line_units), g_cli_views_owner);
    args->view->units_capacity = (args->view->line_units != NULL) ? capacity : 0ULL;
    return args->view->line_units != NULL;
}

// the units of `line` read into the view's line cell, where they are not the line read last: a stored text's copied
// under the documents' lock, a paged text's read through the pages and made units
EMBED_INLINE void view_line_read(const ViewCtx *args)
{
    unsigned long long line = args->line;
    if (args->view->line_known && (args->view->line_cached == line))
    {
        return;
    }
    args->view->line_known = 1;
    args->view->line_cached = line;
    args->view->line_length = 0ULL;
    if (args->view->paged)
    {
        const char *text = NULL;
        unsigned long long count = 0ULL;
        char *const cell = EMBED_CALL(cli_pages_from.lines_alloc, CliPagesFromCfg, .key = args->view->key, .from = line,
                                      .to = line + 1ULL, .line = &text, .count = &count, .owner = g_cli_views_owner);
        if ((cell != NULL) && (count != 0ULL))
        {
            const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES});
            if (EMBED_CALL(view_units_capacity, ViewCtx, .view = args->view, .units_u64 = size))
            {
                args->view->line_length =
                    EMBED_CALL(view_units_get, ViewCtx, .text = text, .size = size, .units = args->view->line_units,
                               .capacity = args->view->units_capacity);
            }
        }
        g_cli_guard->persistent_buf_release(cell, g_cli_views_owner);
        return;
    }
    const CliStoredText *const one =
        EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->view->key);
    if ((one != NULL) && (line < one->lines.lines) &&
        EMBED_CALL(view_units_capacity, ViewCtx, .view = args->view, .units_u64 = one->lines.length[line]))
    {
        args->view->line_length = one->lines.length[line];
        memor.cpy(&(MemoriaCfg){.dst = args->view->line_units,
                                .src = one->lines.unit + one->lines.start[line],
                                .bytes = args->view->line_length * sizeof(*args->view->line_units)});
    }
}

// a line's length in units, and the unit at `col` of it, 0 past the line's end
EMBED_INLINE unsigned long long view_length(const ViewCtx *args)
{
    view_line_read(args);
    return args->view->line_length;
}

EMBED_INLINE unsigned int view_unit(const ViewCtx *args)
{
    view_line_read(args);
    return (args->col < args->view->line_length) ? args->view->line_units[args->col] : 0u;
}

// the tab's width in cells and the indentation's step, as the indentation sets them
EMBED_INLINE unsigned int view_tab(const ViewCtx *args)
{
    return (args->view_read->indent.sized && (args->view_read->indent.size != 0u)) ? args->view_read->indent.size
                                                                                   : CLI_VIEW_TAB;
}

// the cell the unit at `col` of `line` starts in, tabs laid to their stops; and the column of the unit standing at
// `cell`, the line's end where the cell is past it
EMBED_INLINE unsigned long long view_cell_get(const ViewCtx *args)
{
    const unsigned int tab = EMBED_CALL(view_tab, ViewCtx, .view_read = args->view);
    unsigned long long cell = 0ULL;
    for (unsigned long long at = 0ULL; at < args->col; at += 1ULL)
    {
        cell += (EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->line, .col = at) == '\t')
                    ? (tab - (cell % tab))
                    : 1ULL;
    }
    return cell;
}

EMBED_INLINE unsigned long long view_col_at(const ViewCtx *args)
{
    const unsigned int tab = EMBED_CALL(view_tab, ViewCtx, .view_read = args->view);
    const unsigned long long length = view_length(args);
    unsigned long long at = 0ULL;
    unsigned long long here = 0ULL;
    while (at < length)
    {
        const unsigned long long next =
            here + ((EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->line, .col = at) == '\t')
                        ? (tab - (here % tab))
                        : 1ULL);
        if (next > args->cell)
        {
            // the cell inside a letter's span goes to its nearer side
            return ((args->cell - here) * 2ULL >= (next - here)) ? at + 1ULL : at;
        }
        here = next;
        at += 1ULL;
    }
    return length;
}

// whether a unit is a letter of a word
EMBED_INLINE int view_word_unit(const ViewCtx *args)
{
    return (args->unit == '_') || EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = args->unit);
}

EMBED_INLINE int view_blank_unit(const ViewCtx *args)
{
    return (args->unit == ' ') || (args->unit == '\t');
}

// the place a letter left of `place`, the end of the line shown before it at a line's start; and a letter right, the
// start of the line shown after it at a line's end
EMBED_INLINE CliPosition view_leave(const ViewCtx *args)
{
    if (args->place.col == 0ULL)
    {
        const unsigned long long above =
            EMBED_CALL(view_up, ViewCtx, .view_read = args->view, .line = args->place.line, .count = 1ULL);
        return (above == args->place.line)
                   ? args->place
                   : (CliPosition){above, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = above)};
    }
    const unsigned int before =
        EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->place.line, .col = args->place.col - 1ULL);
    const int pair = (args->place.col >= 2ULL) && (before >= 0xdc00u) && (before <= 0xdfffu);
    return (CliPosition){args->place.line, args->place.col - (pair ? 2ULL : 1ULL)};
}

EMBED_INLINE CliPosition view_right(const ViewCtx *args)
{
    const unsigned long long length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = args->place.line);
    if (args->place.col >= length)
    {
        const unsigned long long below =
            EMBED_CALL(view_down, ViewCtx, .view_read = args->view, .line = args->place.line, .count = 1ULL);
        return (below == args->place.line) ? args->place : (CliPosition){below, 0ULL};
    }
    const unsigned int here =
        EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->place.line, .col = args->place.col);
    const int pair = (args->place.col + 1ULL < length) && (here >= 0xd800u) && (here <= 0xdbffu);
    return (CliPosition){args->place.line, args->place.col + (pair ? 2ULL : 1ULL)};
}

// the place the word left of `place` starts at, and the place past the word right of it: blanks skipped, then a run of
// a word's letters or of other letters
EMBED_INLINE CliPosition view_word_leave(const ViewCtx *args)
{
    if (args->place.col == 0ULL)
    {
        return view_leave(args);
    }
    unsigned long long col = args->place.col;
    while ((col > 0ULL) && EMBED_CALL(view_blank_unit, ViewCtx,
                                      .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view,
                                                         .line = args->place.line, .col = col - 1ULL)))
    {
        col -= 1ULL;
    }
    const int word_text = (col > 0ULL) && EMBED_CALL(view_word_unit, ViewCtx,
                                                     .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view,
                                                                        .line = args->place.line, .col = col - 1ULL));
    while ((col > 0ULL) &&
           !EMBED_CALL(view_blank_unit, ViewCtx,
                       .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->place.line,
                                          .col = col - 1ULL)) &&
           (EMBED_CALL(view_word_unit, ViewCtx,
                       .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->place.line,
                                          .col = col - 1ULL)) == word_text))
    {
        col -= 1ULL;
    }
    return (CliPosition){args->place.line, col};
}

EMBED_INLINE CliPosition view_word_right(const ViewCtx *args)
{
    const unsigned long long length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = args->place.line);
    if (args->place.col >= length)
    {
        return view_right(args);
    }
    unsigned long long col = args->place.col;
    while ((col < length) &&
           EMBED_CALL(view_blank_unit, ViewCtx,
                      .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->place.line, .col = col)))
    {
        col += 1ULL;
    }
    const int word_text = (col < length) && EMBED_CALL(view_word_unit, ViewCtx,
                                                       .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view,
                                                                          .line = args->place.line, .col = col));
    while (
        (col < length) &&
        !EMBED_CALL(view_blank_unit, ViewCtx,
                    .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->place.line, .col = col)) &&
        (EMBED_CALL(view_word_unit, ViewCtx,
                    .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->place.line, .col = col)) ==
         word_text))
    {
        col += 1ULL;
    }
    return (CliPosition){args->place.line, col};
}

// the first letter of `line` that is not blank, its end where every letter is
EMBED_INLINE unsigned long long view_first_letter(const ViewCtx *args)
{
    const unsigned long long length = view_length(args);
    unsigned long long col = 0ULL;
    while ((col < length) &&
           EMBED_CALL(view_blank_unit, ViewCtx,
                      .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->line, .col = col)))
    {
        col += 1ULL;
    }
    return col;
}

// whether one place stands before another
EMBED_INLINE int view_before(const ViewCtx *args)
{
    return (args->one.line < args->other.line) ||
           ((args->one.line == args->other.line) && (args->one.col < args->other.col));
}

// the selection's ends in order, `from` first
EMBED_INLINE void view_selection(const ViewCtx *args)
{
    CliPosition *from = args->from_pointer;
    CliPosition *to = args->to_pointer;
    const int anchored =
        EMBED_CALL(view_before, ViewCtx, .one = args->view_read->anchor, .other = args->view_read->cursor);
    *from = anchored ? args->view_read->anchor : args->view_read->cursor;
    *to = anchored ? args->view_read->cursor : args->view_read->anchor;
}

EMBED_INLINE int view_select(const ViewCtx *args)
{
    return (args->view_read->cursor.line != args->view_read->anchor.line) ||
           (args->view_read->cursor.col != args->view_read->anchor.col);
}

// the top moved so the cursor stands in view, among the lines shown
EMBED_INLINE void view_follow(const ViewCtx *args)
{
    const unsigned int rows = (args->view->rows != 0u) ? args->view->rows : 1u;
    args->view->top = EMBED_CALL(view_displayed_up, ViewCtx, .view_read = args->view, .line = args->view->top);
    if (args->view->cursor.line < args->view->top)
    {
        args->view->top =
            EMBED_CALL(view_displayed_up, ViewCtx, .view_read = args->view, .line = args->view->cursor.line);
    }
    else if (EMBED_CALL(view_rows_to, ViewCtx, .view_read = args->view, .top = args->view->top,
                        .line = args->view->cursor.line, .most = rows) >= rows)
    {
        args->view->top = EMBED_CALL(view_up, ViewCtx, .view_read = args->view, .line = args->view->cursor.line,
                                     .count = rows - 1ULL);
    }
}

// the cursor moved to `place`, the anchor with it unless the move extends the selection; the column saved where
// `saved` is 1, for a move up or down
EMBED_INLINE void view_move(const ViewCtx *args)
{
    args->view->cursor = args->place;
    if (!args->extend)
    {
        args->view->anchor = args->place;
    }
    if (!args->saved)
    {
        args->view->goal_saved = 0;
    }
    view_follow(args);
}

// the cursor moved `by` lines shown up or down, in the column it saves
EMBED_INLINE void view_vertical(const ViewCtx *args)
{
    if (!args->view->goal_saved)
    {
        args->view->goal = EMBED_CALL(view_cell_get, ViewCtx, .view = args->view, .line = args->view->cursor.line,
                                      .col = args->view->cursor.col);
        args->view->goal_saved = 1;
    }
    // a move's count is not negative either way
    const unsigned long long line =
        (args->by < 0LL) ? EMBED_CALL(view_up, ViewCtx, .view_read = args->view, .line = args->view->cursor.line,
                                      .count = (unsigned long long)(-args->by))
                         : EMBED_CALL(view_down, ViewCtx, .view_read = args->view, .line = args->view->cursor.line,
                                      .count = (unsigned long long)args->by);
    EMBED_CALL(view_move, ViewCtx, .view = args->view,
               .place = (CliPosition){line, EMBED_CALL(view_col_at, ViewCtx, .view = args->view, .line = line,
                                                       .cell = args->view->goal)},
               .extend = args->extend, .saved = 1);
}

// the query's cell, taken at its most as its first units come, given `more` units past its length: 1, or 0 where it
// could not be taken or they do not fit
EMBED_INLINE int view_query_capacity(const ViewCtx *args)
{
    if (args->view->find_capacity == 0ULL)
    {
        args->view->find_query =
            g_cli_guard->persistent_buf_alloc(CLI_VIEW_FIND_UNITS * sizeof(*args->view->find_query), g_cli_views_owner);
        args->view->find_capacity = (args->view->find_query != NULL) ? CLI_VIEW_FIND_UNITS : 0ULL;
    }
    return (args->view->find_length + args->more) <= args->view->find_capacity;
}

// `count` units put into the query at its cursor, which moves past them; and the units from `from` to before `to`
// taken out of it, the cursor at `from`; each makes the matches stale
EMBED_INLINE void view_query_append(const ViewCtx *args)
{
    if ((args->count == 0ULL) || !EMBED_CALL(view_query_capacity, ViewCtx, .view = args->view, .more = args->count))
    {
        return;
    }
    for (unsigned long long at = args->view->find_length; at > args->view->find_at; at -= 1ULL)
    {
        args->view->find_query[at + args->count - 1ULL] = args->view->find_query[at - 1ULL];
    }
    memor.cpy(&(MemoriaCfg){.dst = args->view->find_query + args->view->find_at,
                            .src = args->units_read,
                            .bytes = args->count * sizeof(*args->units_read)});
    args->view->find_length += args->count;
    args->view->find_at += args->count;
    args->view->find_stale = 1;
}

EMBED_INLINE void view_query_cut(const ViewCtx *args)
{
    for (unsigned long long at = args->to_u64; at < args->view->find_length; at += 1ULL)
    {
        args->view->find_query[args->from_u64 + at - args->to_u64] = args->view->find_query[at];
    }
    args->view->find_length -= args->to_u64 - args->from_u64;
    args->view->find_at = args->from_u64;
    args->view->find_stale = 1;
}

// a match found saved among the matches, where they have work state
static void view_match_save(const CliFound3Cfg *args)
{
    CliView *const view = args->context;
    if (view->matches >= view->match_capacity)
    {
        return;
    }
    view->match[view->matches][0] = args->line;
    view->match[view->matches][1] = args->from;
    view->match[view->matches][2] = args->to;
    view->matches += 1ULL;
}

// the matches of a paged text read off `size` bytes of the `[[line, from, to], ...]` the pages' find writes
EMBED_INLINE void view_matches_read(const ViewCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long number[3] = {0ULL, 0ULL, 0ULL};
    unsigned int part = 0u;
    for (unsigned long long at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text,
                                            .at = 0ULL, .size = size, .set = "0123456789");
         at < size; at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = at,
                                    .size = size, .set = "0123456789"))
    {
        number[part] =
            EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = text, .at = at, .size = size, .end = &at);
        part += 1u;
        if (part == 3u)
        {
            EMBED_CALL(view_match_save, CliFound3Cfg, .context = args->view, .line = number[0], .from = number[1],
                       .to = number[2]);
            part = 0u;
        }
    }
}

// the current match settled: the first that starts at or after the selection's start, the first of all where none
// does, none where none was found
EMBED_INLINE void view_current_settle(const ViewCtx *args)
{
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    args->view->current = (args->view->matches != 0ULL) ? 0LL : -1LL;
    for (unsigned long long at = 0ULL; at < args->view->matches; at += 1ULL)
    {
        const CliPosition start = {args->view->match[at][0], args->view->match[at][1]};
        if (!EMBED_CALL(view_before, ViewCtx, .one = start, .other = from))
        {
            // the matches are fewer than CLI_VIEW_MATCHES, which a long long counts
            args->view->current = (long long)at;
            break;
        }
    }
}

// the text searched for the query where the query or the text changed since it was: the matches found anew, as many
// as CLI_VIEW_MATCHES at the most, a paged text's through the pages, and the current match settled
static void view_search(const ViewCtx *args)
{
    if (!args->view->find_stale)
    {
        return;
    }
    args->view->find_stale = 0;
    args->view->matches = 0ULL;
    if (args->view->match == NULL)
    {
        args->view->match =
            g_cli_guard->persistent_buf_alloc(CLI_VIEW_MATCHES * sizeof(*args->view->match), g_cli_views_owner);
        args->view->match_capacity = (args->view->match != NULL) ? CLI_VIEW_MATCHES : 0ULL;
    }
    args->view->current = -1LL;
    if (args->view->find_length == 0ULL)
    {
        return;
    }
    const unsigned long long query_capacity = (args->view->find_length * 4ULL) + 1ULL;
    char *const query = g_cli_guard->temporary_buf_alloc(query_capacity, g_cli_views_owner);
    if (query == NULL)
    {
        return;
    }
    VerbaLinea line = {query, query_capacity, 0ULL};
    EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &line,
               .units = args->view->find_query, .count = args->view->find_length);
    (void)mmgr_line_finish(&line);
    const CliDocNeedle needle = {query, args->view->find_regex, args->view->find_case, args->view->find_word};
    char error_text[256];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
    if (args->view->paged)
    {
        const unsigned long long capacity = CLI_VIEW_MATCHES * 24ULL;
        char *const cell = g_cli_guard->temporary_buf_alloc(capacity, g_cli_views_owner);
        if (cell != NULL)
        {
            VerbaLinea out = {cell, capacity, 0ULL};
            if (EMBED_CALL(cli_pages_out_err_msg_needle.find, CliPagesOutErrMsgNeedleCfg, .key = args->view->key,
                           .needle = &needle, .most = CLI_VIEW_MATCHES, .out = &out, .err_msg = &why) &&
                mmgr_line_ok(&out))
            {
                EMBED_CALL(view_matches_read, ViewCtx, .view = args->view, .text = cell, .size = out.at);
            }
            g_cli_guard->persistent_buf_release(cell, g_cli_views_owner);
        }
    }
    else
    {
        const CliStoredText *const one =
            EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->view->key);
        if (one != NULL)
        {
            (void)EMBED_CALL(cli_document_lines.find_each, CliDocumentLinesCfg, .lines = &one->lines, .needle = &needle,
                             .most = CLI_VIEW_MATCHES, .each = view_match_save, .context = args->view, .err_msg = &why);
        }
    }
    g_cli_guard->persistent_buf_release(query, g_cli_views_owner);
    view_current_settle(args);
}

// the match a step from the selection selected, the current one: the first that starts after the selection's start
// going forward, the last that starts before it going back, the first or the last around the ends
EMBED_INLINE void view_find_step(const ViewCtx *args)
{
    view_search(args);
    if (args->view->matches == 0ULL)
    {
        return;
    }
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    unsigned long long chosen = args->forward ? 0ULL : args->view->matches - 1ULL;
    for (unsigned long long at = 0ULL; args->forward && (at < args->view->matches); at += 1ULL)
    {
        const CliPosition start = {args->view->match[at][0], args->view->match[at][1]};
        if (EMBED_CALL(view_before, ViewCtx, .one = from, .other = start))
        {
            chosen = at;
            break;
        }
    }
    for (unsigned long long at = args->view->matches; !args->forward && (at > 0ULL); at -= 1ULL)
    {
        const CliPosition start = {args->view->match[at - 1ULL][0], args->view->match[at - 1ULL][1]};
        if (EMBED_CALL(view_before, ViewCtx, .one = start, .other = from))
        {
            chosen = at - 1ULL;
            break;
        }
    }
    // the matches are fewer than CLI_VIEW_MATCHES, which a long long counts
    args->view->current = (long long)chosen;
    args->view->anchor = (CliPosition){args->view->match[chosen][0], args->view->match[chosen][1]};
    EMBED_CALL(view_move, ViewCtx, .view = args->view,
               .place = (CliPosition){args->view->match[chosen][0], args->view->match[chosen][2]}, .extend = 1,
               .saved = 0);
}

// find opened, the keys its own, a selection on one line its query; and closed
EMBED_INLINE void view_find_open(const ViewCtx *args)
{
    args->view->find_open = 1;
    args->view->find_focus = 1;
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    if ((from.line == to.line) && (from.col < to.col))
    {
        args->view->find_length = 0ULL;
        args->view->find_at = 0ULL;
        const unsigned long long length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = from.line);
        const unsigned long long end = (to.col < length) ? to.col : length;
        EMBED_CALL(view_query_append, ViewCtx, .view = args->view, .units_read = args->view->line_units + from.col,
                   .count = end - from.col);
    }
    args->view->find_at = args->view->find_length;
    args->view->find_stale = 1;
}

EMBED_INLINE void view_find_close(const ViewCtx *args)
{
    args->view->find_open = 0;
    args->view->find_focus = 0;
}

EMBED_INLINE int view_find_toggle_key(const ViewCtx *args);
EMBED_INLINE int view_find_query_key(const ViewCtx *args);

// a key taken by find where its bar is open or the key opens it: Mod+F opens it or gives it the keys, F3 seeks the
// query again, Escape closes it, and with the keys its own Enter seeks the next match, Shift+Enter the one before,
// Left, Right, Home, End, Backspace and Delete edit the query, and Alt+C, Alt+W and Alt+R turn its case, whole words
// and pattern on and off: 1 where the key was find's
EMBED_INLINE int view_find_key(const ViewCtx *args)
{
    if (args->mod && !args->alt && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "F"))
    {
        if (args->view->find_open)
        {
            args->view->find_focus = 1;
            args->view->find_at = args->view->find_length;
        }
        else
        {
            view_find_open(args);
        }
        return 1;
    }
    if (!args->mod && !args->alt && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "F3"))
    {
        if (args->view->find_length != 0ULL)
        {
            EMBED_CALL(view_find_step, ViewCtx, .view = args->view, .forward = !args->shift);
        }
        else
        {
            view_find_open(args);
        }
        return 1;
    }
    if (!args->view->find_open)
    {
        return 0;
    }
    if (!args->mod && !args->alt && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Escape"))
    {
        view_find_close(args);
        return 1;
    }
    if (args->alt && !args->mod)
    {
        return EMBED_CALL(view_find_toggle_key, ViewCtx, .view = args->view, .name = args->name);
    }
    if (!args->view->find_focus || args->mod || args->alt)
    {
        return 0;
    }
    return EMBED_CALL(view_find_query_key, ViewCtx, .view = args->view, .name = args->name, .shift = args->shift);
}

// a key held with Alt while find's bar is open: C, W and R turn its case, whole words and pattern on and off, and the
// matches are sought again; 1 where the key was one of them
EMBED_INLINE int view_find_toggle_key(const ViewCtx *args)
{
    CliView *const view = args->view;
    const int case_key = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "C");
    const int word_key = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "W");
    const int regex_key = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "R");
    if (!case_key && !word_key && !regex_key)
    {
        return 0;
    }
    view->find_case = case_key ? !view->find_case : view->find_case;
    view->find_word = word_key ? !view->find_word : view->find_word;
    view->find_regex = regex_key ? !view->find_regex : view->find_regex;
    view->find_stale = 1;
    return 1;
}

// a key while find's query has the keys: Enter seeks the next match, Shift+Enter the one before, and Left, Right,
// Home, End, Backspace and Delete edit the query; 1 where the key was one of them
EMBED_INLINE int view_find_query_key(const ViewCtx *args)
{
    if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Enter"))
    {
        EMBED_CALL(view_find_step, ViewCtx, .view = args->view, .forward = !args->shift);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Left"))
    {
        args->view->find_at -= (args->view->find_at > 0ULL) ? 1ULL : 0ULL;
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Right"))
    {
        args->view->find_at += (args->view->find_at < args->view->find_length) ? 1ULL : 0ULL;
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Home"))
    {
        args->view->find_at = 0ULL;
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "End"))
    {
        args->view->find_at = args->view->find_length;
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Backspace"))
    {
        if (args->view->find_at > 0ULL)
        {
            EMBED_CALL(view_query_cut, ViewCtx, .view = args->view, .from_u64 = args->view->find_at - 1ULL,
                       .to_u64 = args->view->find_at);
        }
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Delete"))
    {
        if (args->view->find_at < args->view->find_length)
        {
            EMBED_CALL(view_query_cut, ViewCtx, .view = args->view, .from_u64 = args->view->find_at,
                       .to_u64 = args->view->find_at + 1ULL);
        }
    }
    else
    {
        return 0;
    }
    return 1;
}

// the fold at `line` closed, the one opened on it where `exact` is 1, else the innermost that reaches it, where the
// text folds there; the cursor put at the fold's first line's end where it stood on that line or among the lines
// hidden: 1, or 0 where the text folds nothing there or the cell could not be taken; and the fold opened on `line`
// opened again: 1, or 0 where none is closed there
EMBED_INLINE int view_fold(const ViewCtx *args)
{
    CliViewFoldNeedle needle = {args->line, 0ULL, 0ULL, 0};
    (void)EMBED_CALL(cli_document_context.folds_each, CliDocumentContextCfg, .key = args->view->key,
                     .each_fold_each = args->exact ? view_fold_match : view_fold_innermost, .context = &needle);
    if (!needle.found ||
        !EMBED_CALL(view_fold_append, ViewCtx, .view = args->view, .first = needle.first, .last = needle.last))
    {
        return 0;
    }
    if ((args->view->cursor.line == needle.first) ||
        (EMBED_CALL(view_hide_timer, ViewCtx, .view_read = args->view, .line = args->view->cursor.line) != NULL))
    {
        EMBED_CALL(view_move, ViewCtx, .view = args->view,
                   .place = (CliPosition){needle.first,
                                          EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = needle.first)},
                   .extend = 0, .saved = 0);
    }
    else
    {
        view_follow(args);
    }
    return 1;
}

EMBED_INLINE int view_unfolded(const ViewCtx *args)
{
    for (unsigned int at = 0u; at < args->view->folds; at += 1u)
    {
        if (args->view->fold[at].first == args->line)
        {
            for (unsigned int after = at + 1u; after < args->view->folds; after += 1u)
            {
                args->view->fold[after - 1u] = args->view->fold[after];
            }
            args->view->folds -= 1u;
            view_follow(args);
            return 1;
        }
    }
    return 0;
}

// one change of one edit written to the text, a paged text's through the pages, the closed folds moved with it: 1, or
// 0 with why given
EMBED_INLINE int view_write(const ViewCtx *args)
{
    const CliEdit *const *edits = args->edits;
    VerbaLinea *err_msg = args->err_msg;
    args->view->line_known = 0;
    int written = 0;
    if (args->view->paged)
    {
        char shape_text[128];
        VerbaLinea shape = {shape_text, sizeof(shape_text), 0ULL};
        written = EMBED_CALL(cli_document_out.paged_edit, CliDocumentOutCfg, .key = args->view->key, .changes = edits,
                             .edits = args->counts, .count_u32 = 1u, .out = &shape, .err_msg = err_msg);
    }
    else
    {
        CliShape shape;
        written = EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = args->view->key, .changes = edits,
                             .edits = args->counts, .count_u32 = 1u, .shape = &shape, .err_msg = err_msg);
    }
    for (unsigned int at = 0u; written && (at < args->counts[0]); at += 1u)
    {
        EMBED_CALL(view_folds_edit, ViewCtx, .view = args->view, .edit = &edits[0][at]);
    }
    args->view->find_stale = args->view->find_stale || written;
    args->view->changed = args->view->changed || written;
    return written;
}

// the bytes of UTF-8 the span from `from` to `to` of the text takes, each line but the last with a byte for its break
EMBED_INLINE unsigned long long view_span_bytes(const ViewCtx *args)
{
    unsigned long long bytes = 0ULL;
    for (unsigned long long line = args->from.line; line <= args->to.line; line += 1ULL)
    {
        const unsigned long long length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = line);
        const unsigned long long start = (line == args->from.line) ? args->from.col : 0ULL;
        const unsigned long long end = ((line == args->to.line) && (args->to.col < length)) ? args->to.col : length;
        for (unsigned long long at = start; at < end; at += 1ULL)
        {
            const unsigned int unit = args->view->line_units[at];
            if (unit < 0x80u)
            {
                bytes += 1ULL;
            }
            else if (unit < 0x800u)
            {
                bytes += 2ULL;
            }
            else
            {
                bytes += 3ULL;
            }
        }
        bytes += (line < args->to.line) ? 1ULL : 0ULL;
    }
    return bytes;
}

// the place `bytes` bytes of UTF-8 `text` end at when they begin at `from`
EMBED_INLINE CliPosition view_position_end(const ViewCtx *args)
{
    const char *text = args->text;
    CliPosition place = args->from;
    unsigned long long start = 0ULL;
    for (unsigned long long at = 0ULL; at < args->bytes; at += 1ULL)
    {
        if (text[at] == '\n')
        {
            place.line += 1ULL;
            start = at + 1ULL;
        }
    }
    const unsigned long long units =
        EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = text + start, .size = args->bytes - start);
    place.col = (place.line == args->from.line) ? args->from.col + units : units;
    return place;
}

EMBED_INLINE void view_span_write(const ViewCtx *args);

// one cell for an edit of the span from `from` to `to`, the exact size it takes: the edit, work state for the text to
// put in, `append_bytes` bytes and `append_spans` times what the span holds, and the text the span holds read in after
// that work state: the cell, or NULL with why given
EMBED_INLINE CliViewEdit *view_edit_cell(const ViewCtx *args)
{
    const unsigned long long consumed_capacity = view_span_bytes(args);
    const unsigned long long append_capacity = args->append_bytes + (args->append_spans * consumed_capacity);
    CliViewEdit *const made = g_cli_guard->persistent_buf_alloc(
        sizeof(CliViewEdit) + append_capacity + 1ULL + consumed_capacity + 1ULL, g_cli_views_owner);
    if (made == NULL)
    {
        mmgr_line_put(args->err_msg, "the edit's cell could not be taken");
        return NULL;
    }
    char *const cell = (char *)(made + 1);
    VerbaLinea consumed = {cell + append_capacity + 1ULL, consumed_capacity + 1ULL, 0ULL};
    EMBED_CALL(view_span_write, ViewCtx, .view = args->view, .from = args->from, .to = args->to, .out = &consumed);
    const unsigned long long consumed_length = mmgr_line_finish(&consumed);
    *made = (CliViewEdit){args->from, args->to, args->from, cell, 0ULL, consumed.out, consumed_length, NULL, NULL};
    return made;
}

// the edit in its cell written to the text, `append_length` bytes at its `put`, and recorded in a step of `kind`,
// joined to the last step where it is of the same kind and came within CLI_VIEW_JOINED milliseconds; the cursor and the
// anchor put after the text written: 1, or 0 with why given and the cell given back
static int view_edit_cell_write(const ViewCtx *args)
{
    unsigned int kind = args->kind;
    // the edit made, a paged text's through the pages, and the line cell read anew after it
    const CliEdit edit = {args->made->from, args->made->to, args->made->put, args->made->append_length};
    const CliEdit *const edits[1] = {&edit};
    const unsigned int counts[1] = {1u};
    args->view->line_known = 0;
    if (!EMBED_CALL(view_write, ViewCtx, .view = args->view, .edits = edits, .counts = counts,
                    .err_msg = args->err_msg))
    {
        g_cli_guard->persistent_buf_release(args->made, g_cli_views_owner);
        return 0;
    }
    args->made->end = EMBED_CALL(view_position_end, ViewCtx, .from = args->made->from, .text = args->made->put,
                                 .bytes = args->made->append_length);
    // the step the edit joins or begins
    const unsigned long long now = cli_clock_milliseconds();
    EMBED_CALL(view_steps_cut, ViewCtx, .view = args->view, .from_u32 = args->view->done);
    CliViewStep *step = (args->view->steps != 0u) ? &args->view->step[args->view->steps - 1u] : NULL;
    const int joined = (step != NULL) && (kind != CLI_VIEW_KIND_NONE) && (step->kind == kind) &&
                       (now - step->at_ms <= CLI_VIEW_JOINED) && (step->edits < CLI_VIEW_STEP_EDITS);
    if (!joined)
    {
        if (args->view->step == NULL)
        {
            args->view->step =
                g_cli_guard->persistent_buf_alloc(CLI_VIEW_STEPS * sizeof(*args->view->step), g_cli_views_owner);
            if (args->view->step == NULL)
            {
                g_cli_guard->persistent_buf_release(args->made, g_cli_views_owner);
                mmgr_line_put(args->err_msg, "the steps' cell could not be taken");
                return 0;
            }
            args->view->steps_capacity = CLI_VIEW_STEPS;
        }
        // the steps full, the oldest goes and those after it come down, a save point at the text as it opened lost
        // with it
        if (args->view->steps == args->view->steps_capacity)
        {
            args->view->save_point_lost = args->view->save_point_lost || (args->view->save_point == NULL);
            for (CliViewEdit *edit = (&args->view->step[0])->first; edit != NULL;)
            {
                CliViewEdit *const after = edit->after;
                args->view->save_point_lost = args->view->save_point_lost || (edit == args->view->save_point);
                g_cli_guard->persistent_buf_release(edit, g_cli_views_owner);
                edit = after;
            }
            memor.move_down(&(MemoriaCfg){.dst = &args->view->step[0],
                                          .src = &args->view->step[1],
                                          .bytes = (size_t)(args->view->steps - 1u) * sizeof(*args->view->step)});
            args->view->steps -= 1u;
        }
        step = &args->view->step[args->view->steps];
        memor.set(&(MemoriaCfg){.dst = step, .val = 0, .bytes = sizeof(*step)});
        step->kind = kind;
        step->cursor_before = args->view->cursor;
        step->anchor_before = args->view->anchor;
        args->view->steps += 1u;
    }
    args->made->before = step->last;
    args->made->after = NULL;
    if (step->last != NULL)
    {
        step->last->after = args->made;
    }
    step->first = (step->first != NULL) ? step->first : args->made;
    step->last = args->made;
    step->edits += 1u;
    step->at_ms = now;
    args->view->done = args->view->steps;
    EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = args->made->end, .extend = 0, .saved = 0);
    step->cursor_after = args->view->cursor;
    step->anchor_after = args->view->anchor;
    return 1;
}

// `length` bytes of `text` written over the span from `from` to `to`, as view_edit_cell_write writes them: 1, or 0
// with why given
EMBED_INLINE int view_edit(const ViewCtx *args)
{
    CliViewEdit *const made =
        EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = args->from, .to = args->to,
                   .append_bytes = args->length, .append_spans = 0ULL, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = made->put, .src = args->text, .bytes = args->length});
    made->put[args->length] = '\0';
    made->append_length = args->length;
    return EMBED_CALL(view_edit_cell_write, ViewCtx, .view = args->view, .made = made, .kind = args->kind,
                      .err_msg = args->err_msg);
}

// the last step done undone, its edits written back from the last to the first; and the first step undone done again
EMBED_INLINE int view_undone(const ViewCtx *args)
{
    if (args->view->done == 0u)
    {
        return 1;
    }
    const CliViewStep *const step = &args->view->step[args->view->done - 1u];
    for (const CliViewEdit *edit = step->last; edit != NULL; edit = edit->before)
    {
        const CliEdit back = {edit->from, edit->end, edit->consumed, edit->consumed_length};
        const CliEdit *const edits[1] = {&back};
        const unsigned int counts[1] = {1u};
        if (!EMBED_CALL(view_write, ViewCtx, .view = args->view, .edits = edits, .counts = counts,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
    }
    args->view->done -= 1u;
    args->view->cursor = step->cursor_before;
    args->view->anchor = step->anchor_before;
    args->view->goal_saved = 0;
    view_follow(args);
    return 1;
}

EMBED_INLINE int view_redone(const ViewCtx *args)
{
    if (args->view->done >= args->view->steps)
    {
        return 1;
    }
    const CliViewStep *const step = &args->view->step[args->view->done];
    for (const CliViewEdit *edit = step->first; edit != NULL; edit = edit->after)
    {
        const CliEdit again = {edit->from, edit->to, edit->put, edit->append_length};
        const CliEdit *const edits[1] = {&again};
        const unsigned int counts[1] = {1u};
        if (!EMBED_CALL(view_write, ViewCtx, .view = args->view, .edits = edits, .counts = counts,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
    }
    args->view->done += 1u;
    args->view->cursor = step->cursor_after;
    args->view->anchor = step->anchor_after;
    args->view->goal_saved = 0;
    view_follow(args);
    return 1;
}

// the selection written over with `length` bytes of `text`, or the text put at the cursor where none is selected
EMBED_INLINE int view_type(const ViewCtx *args)
{
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    return EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = from, .to = to, .text = args->text,
                      .length = args->length, .kind = args->kind, .err_msg = args->err_msg);
}

// the cursor and the anchor put where an edit leaves them, the last step done recording them so redo puts them back
EMBED_INLINE void view_settle(const ViewCtx *args)
{
    CliPosition cursor = args->cursor;
    CliPosition anchor = args->anchor;
    args->view->cursor = cursor;
    args->view->anchor = anchor;
    args->view->goal_saved = 0;
    if (args->view->done != 0u)
    {
        args->view->step[args->view->done - 1u].cursor_after = cursor;
        args->view->step[args->view->done - 1u].anchor_after = anchor;
    }
    view_follow(args);
}

// the closer of the pair `unit` opens, a quote's being itself, or 0 where it opens none; whether `unit` closes a pair
// or is a quote; and whether it closes a bracket pair, one whose closer is not its opener
EMBED_INLINE unsigned int view_closer_get(const ViewCtx *args)
{
    for (unsigned int at = 0u; at < args->view_read->pairs; at += 1u)
    {
        if (args->view_read->pair_open[at] == args->unit)
        {
            return args->view_read->pair_close[at];
        }
    }
    for (unsigned int at = 0u; at < args->view_read->quotes; at += 1u)
    {
        if (args->view_read->quote[at] == args->unit)
        {
            return args->unit;
        }
    }
    return 0u;
}

EMBED_INLINE int view_closes(const ViewCtx *args)
{
    for (unsigned int at = 0u; at < args->view_read->pairs; at += 1u)
    {
        if (args->view_read->pair_close[at] == args->unit)
        {
            return 1;
        }
    }
    for (unsigned int at = 0u; at < args->view_read->quotes; at += 1u)
    {
        if (args->view_read->quote[at] == args->unit)
        {
            return 1;
        }
    }
    return 0;
}

EMBED_INLINE int view_closes_bracket(const ViewCtx *args)
{
    for (unsigned int at = 0u; at < args->view_read->pairs; at += 1u)
    {
        if ((args->view_read->pair_close[at] == args->unit) && (args->view_read->pair_open[at] != args->unit))
        {
            return 1;
        }
    }
    return 0;
}

// whether `open` and `close` are a bracket pair of the language's, or a quote and itself where `quoted` is 1
EMBED_INLINE int view_on_pair(const ViewCtx *args)
{
    for (unsigned int at = 0u; at < args->view_read->pairs; at += 1u)
    {
        if ((args->view_read->pair_open[at] == args->open) && (args->view_read->pair_close[at] == args->close) &&
            (args->open != args->close))
        {
            return 1;
        }
    }
    for (unsigned int at = 0u; args->quoted && (at < args->view_read->quotes); at += 1u)
    {
        if ((args->view_read->quote[at] == args->open) && (args->open == args->close))
        {
            return 1;
        }
    }
    return 0;
}

// the closer of the innermost bracket `line` opens before `col` and leaves open there, or 0 where it leaves none
EMBED_INLINE unsigned int view_open_before(const ViewCtx *args)
{
    unsigned int open[64];
    unsigned int depth = 0u;
    for (unsigned long long at = 0ULL; at < args->col; at += 1ULL)
    {
        const unsigned int unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->line, .col = at);
        const unsigned int closer = EMBED_CALL(view_closer_get, ViewCtx, .view_read = args->view, .unit = unit);
        if ((closer != 0u) && (closer != unit))
        {
            if (depth < sizeof(open) / sizeof(open[0]))
            {
                open[depth] = closer;
                depth += 1u;
            }
        }
        else if ((depth != 0u) && (open[depth - 1u] == unit))
        {
            depth -= 1u;
        }
    }
    return (depth != 0u) ? open[depth - 1u] : 0u;
}

// whether the language's pattern of a line that opens a block matches `line` before `col`
EMBED_INLINE int view_opens_block(const ViewCtx *args)
{
    if (args->view->indent_after[0] == '\0')
    {
        return 0;
    }
    const unsigned long long length = view_length(args);
    const unsigned long long count = (args->col < length) ? args->col : length;
    unsigned char *const cell = g_cli_guard->temporary_buf_alloc(
        sizeof(CliRegexpWorkState) + ((count + 1ULL) * sizeof(unsigned int)), g_cli_views_owner);
    if (cell == NULL)
    {
        return 0;
    }
    unsigned int *const letters = (unsigned int *)(void *)(cell + sizeof(CliRegexpWorkState));
    // the units read as letters, a surrogate pair one letter
    unsigned long long read = 0ULL;
    {
        unsigned long long at = 0ULL;
        while (at < count)
        {
            const unsigned int unit = args->view->line_units[at];
            const int pair = (unit >= 0xd800u) && (unit <= 0xdbffu) && (at + 1ULL < count);
            letters[read] =
                pair ? (0x10000u + ((unit - 0xd800u) << 10u) + (args->view->line_units[at + 1ULL] - 0xdc00u)) : unit;
            at += pair ? 1ULL : 0ULL;
            read += 1ULL;
            at += 1ULL;
        }
    }
    char error_text[256];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
    CliRegexp regexp;
    EMBED_CALL(cli_regexp_capacity.capacity, CliRegexpWorkStateCfg, .regexp_out = &regexp,
               .work_state = (CliRegexpWorkState *)(void *)cell);
    int ok = EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = &regexp,
                        .source = args->view->indent_after, .flags = "u", .err_msg = &why);
    if (!ok)
    {
        why.at = 0ULL;
        EMBED_CALL(cli_regexp_capacity.capacity, CliRegexpWorkStateCfg, .regexp_out = &regexp,
                   .work_state = (CliRegexpWorkState *)(void *)cell);
        ok = EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = &regexp,
                        .source = args->view->indent_after, .flags = "", .err_msg = &why);
    }
    int matched = 0;
    {
        unsigned long long at = 0ULL;
        while (ok && !matched && (at <= read))
        {
            CliRegexpSpan groups[CLI_REGEXP_GROUPS + 1u];
            const int starts =
                (!regexp.anchored || (at == 0ULL)) &&
                ((at == read) || regexp.first.empty ||
                 EMBED_CALL(cli_regexp.allows, CliRegexpCfg, .first = &regexp.first, .letter = letters[at]));
            matched = starts && EMBED_CALL(cli_regexp_text_u_32s.match_into, CliRegexpTextU32sCfg, .regexp = &regexp,
                                           .text_u32s = letters, .length = read, .at = at, .groups = groups);
            at += 1ULL;
        }
    }
    g_cli_guard->persistent_buf_release(cell, g_cli_views_owner);
    return matched;
}

// the indentation's step as text: a tab, or the step's spaces
EMBED_INLINE unsigned long long view_step_write(const ViewCtx *args)
{
    const unsigned long long begun = args->out->at;
    if (args->view_read->indent.tabs == 1)
    {
        mmgr_line_ch(args->out, '\t');
    }
    else
    {
        for (unsigned long long at = 0ULL; at < view_tab(args); at += 1ULL)
        {
            mmgr_line_ch(args->out, ' ');
        }
    }
    return args->out->at - begun;
}

// a line broken at the cursor, the new line indented as the one it leaves, and a step deeper after a bracket the line
// leaves open before the cursor or a line the language's pattern gives as opening a block; where what follows the
// cursor closes that bracket, or the letters about the cursor are a pair, the closer goes to a line of its own at the
// first line's depth
EMBED_INLINE int view_newline(const ViewCtx *args)
{
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    const unsigned long long lead = EMBED_CALL(view_first_letter, ViewCtx, .view = args->view, .line = from.line);
    const unsigned long long saved = (lead < from.col) ? lead : from.col;
    const unsigned int open =
        EMBED_CALL(view_open_before, ViewCtx, .view = args->view, .line = from.line, .col = from.col);
    const int deeper =
        (open != 0u) || EMBED_CALL(view_opens_block, ViewCtx, .view = args->view, .line = from.line, .col = from.col);
    unsigned long long before = from.col;
    while ((before > 0ULL) && EMBED_CALL(view_blank_unit, ViewCtx,
                                         .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line,
                                                            .col = before - 1ULL)))
    {
        before -= 1ULL;
    }
    const unsigned int prev =
        (before > 0ULL) ? EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line, .col = before - 1ULL)
                        : 0u;
    const unsigned long long remainder_length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = to.line);
    unsigned long long gap = 0ULL;
    while ((to.col + gap < remainder_length) &&
           EMBED_CALL(view_blank_unit, ViewCtx,
                      .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = to.line, .col = to.col + gap)))
    {
        gap += 1ULL;
    }
    const unsigned int next = (to.col + gap < remainder_length) ? EMBED_CALL(view_unit, ViewCtx, .view = args->view,
                                                                             .line = to.line, .col = to.col + gap)
                                                                : 0u;
    const int paired =
        ((prev != 0u) && (next != 0u) &&
         EMBED_CALL(view_on_pair, ViewCtx, .view_read = args->view, .open = prev, .close = next, .quoted = 0)) ||
        ((open != 0u) && (next == open));
    char text[CLI_PATH_MAX_LEN];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_ch(&line, '\n');
    for (unsigned long long at = 0ULL; (at < saved) && (line.at + 4ULL < sizeof(text)); at += 1ULL)
    {
        EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line,
                   .letter = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line, .col = at));
    }
    const unsigned long long prefix_bytes = line.at - 1ULL;
    unsigned long long step_bytes = 0ULL;
    if (deeper)
    {
        step_bytes = EMBED_CALL(view_step_write, ViewCtx, .view_read = args->view, .out = &line);
    }
    if (paired && deeper)
    {
        const unsigned long long cursor_col = EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = text + 1,
                                                         .size = prefix_bytes + step_bytes);
        mmgr_line_ch(&line, '\n');
        mmgr_line_put_n(&line, text + 1, prefix_bytes);
        if (!EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = from,
                        .to = (CliPosition){to.line, to.col + gap}, .text = text, .length = line.at,
                        .kind = CLI_VIEW_KIND_NONE, .err_msg = args->err_msg))
        {
            return 0;
        }
        const CliPosition between = {from.line + 1ULL, cursor_col};
        EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = between, .anchor = between);
        return 1;
    }
    return EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = from, .to = to, .text = text, .length = line.at,
                      .kind = CLI_VIEW_KIND_NONE, .err_msg = args->err_msg);
}

// the letter before the cursor taken out, or the selection; a pair's opener before the cursor and its closer after
// it both; in a line's indentation of spaces a step of them; at a line's start the line joined to the one before
EMBED_INLINE int view_backspace(const ViewCtx *args)
{
    if (EMBED_CALL(view_select, ViewCtx, .view_read = args->view))
    {
        return EMBED_CALL(view_type, ViewCtx, .view = args->view, .text = "", .length = 0ULL,
                          .kind = CLI_VIEW_KIND_DELETE, .err_msg = args->err_msg);
    }
    const CliPosition at = args->view->cursor;
    if (at.col == 0ULL)
    {
        return (at.line == 0ULL)
                   ? 1
                   : EMBED_CALL(view_edit, ViewCtx, .view = args->view,
                                .from = EMBED_CALL(view_leave, ViewCtx, .view = args->view, .place = at), .to = at,
                                .text = "", .length = 0ULL, .kind = CLI_VIEW_KIND_DELETE, .err_msg = args->err_msg);
    }
    if ((at.col < EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = at.line)) &&
        EMBED_CALL(view_on_pair, ViewCtx, .view_read = args->view,
                   .open = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = at.line, .col = at.col - 1ULL),
                   .close = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = at.line, .col = at.col),
                   .quoted = 1))
    {
        return EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = (CliPosition){at.line, at.col - 1ULL},
                          .to = (CliPosition){at.line, at.col + 1ULL}, .text = "", .length = 0ULL,
                          .kind = CLI_VIEW_KIND_DELETE, .err_msg = args->err_msg);
    }
    if (!args->view->indent.tabs || (args->view->indent.tabs == -1))
    {
        unsigned long long col = 0ULL;
        while ((col < at.col) &&
               (EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = at.line, .col = col) == ' '))
        {
            col += 1ULL;
        }
        if (col == at.col)
        {
            const unsigned long long step = EMBED_CALL(view_tab, ViewCtx, .view_read = args->view);
            const unsigned long long back = (at.col % step != 0ULL) ? at.col % step : step;
            return EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = (CliPosition){at.line, at.col - back},
                              .to = at, .text = "", .length = 0ULL, .kind = CLI_VIEW_KIND_DELETE,
                              .err_msg = args->err_msg);
        }
    }
    return EMBED_CALL(view_edit, ViewCtx, .view = args->view,
                      .from = EMBED_CALL(view_leave, ViewCtx, .view = args->view, .place = at), .to = at, .text = "",
                      .length = 0ULL, .kind = CLI_VIEW_KIND_DELETE, .err_msg = args->err_msg);
}

// the letter at the cursor taken out, or the selection; at a line's end the next line joined to it
EMBED_INLINE int view_delete(const ViewCtx *args)
{
    if (EMBED_CALL(view_select, ViewCtx, .view_read = args->view))
    {
        return EMBED_CALL(view_type, ViewCtx, .view = args->view, .text = "", .length = 0ULL,
                          .kind = CLI_VIEW_KIND_DELETE_FORWARD, .err_msg = args->err_msg);
    }
    const CliPosition at = args->view->cursor;
    const CliPosition next = EMBED_CALL(view_right, ViewCtx, .view = args->view, .place = at);
    if ((next.line == at.line) && (next.col == at.col))
    {
        return 1;
    }
    return EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = at, .to = next, .text = "", .length = 0ULL,
                      .kind = CLI_VIEW_KIND_DELETE_FORWARD, .err_msg = args->err_msg);
}

// a tab typed: an indentation's step at the cursor, spaces to the next stop or a tab
EMBED_INLINE int view_on_tab(const ViewCtx *args)
{
    if (args->view->indent.tabs == 1)
    {
        return EMBED_CALL(view_type, ViewCtx, .view = args->view, .text = "\t", .length = 1ULL,
                          .kind = CLI_VIEW_KIND_SPACE, .err_msg = args->err_msg);
    }
    const unsigned long long step = EMBED_CALL(view_tab, ViewCtx, .view_read = args->view);
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    const unsigned long long cell =
        EMBED_CALL(view_cell_get, ViewCtx, .view = args->view, .line = from.line, .col = from.col);
    const unsigned long long spaces = step - (cell % step);
    char text[64];
    const unsigned long long count = (spaces < sizeof(text)) ? spaces : sizeof(text) - 1ULL;
    memor.set(&(MemoriaCfg){.dst = text, .val = ' ', .bytes = count});
    return EMBED_CALL(view_type, ViewCtx, .view = args->view, .text = text, .length = count,
                      .kind = CLI_VIEW_KIND_SPACE, .err_msg = args->err_msg);
}

// the span from `from` to `to` of the text written into `out` as UTF-8, each line but the last followed by "\n"
EMBED_INLINE void view_span_write(const ViewCtx *args)
{
    VerbaLinea *out = args->out;
    for (unsigned long long line = args->from.line; line <= args->to.line; line += 1ULL)
    {
        const unsigned long long line_length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = line);
        const unsigned long long start = (line == args->from.line) ? args->from.col : 0ULL;
        unsigned long long end;
        if (line == args->to.line)
        {
            if (args->to.col < line_length)
            {
                end = args->to.col;
            }
            else
            {
                end = line_length;
            }
        }
        else
        {
            end = line_length;
        }
        if (start < end)
        {
            EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = out,
                       .units = args->view->line_units + start, .count = end - start);
        }
        if (line < args->to.line)
        {
            mmgr_line_ch(out, '\n');
        }
    }
}

// an opener, `length` bytes of `text`, typed around the selection from `from` to `to`: the selection wrapped in it and
// its closer `letter`, the selection saved inside: 1, or 0 with why given where the edit fails
EMBED_INLINE int view_letter_wrap(const ViewCtx *args)
{
    const CliPosition from = args->from;
    unsigned long long capacity = 0ULL;
    for (unsigned long long line = from.line; line <= args->to.line; line += 1ULL)
    {
        capacity += (EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = line) * 4ULL) + 1ULL;
    }
    char *const cell = g_cli_guard->temporary_buf_alloc(capacity + args->length + 8ULL, g_cli_views_owner);
    if (cell == NULL)
    {
        mmgr_line_put(args->err_msg, "the wrap's cell could not be taken");
        return 0;
    }
    VerbaLinea wrapped = {cell, capacity + args->length + 8ULL, 0ULL};
    mmgr_line_put_n(&wrapped, args->text, args->length);
    EMBED_CALL(view_span_write, ViewCtx, .view = args->view, .from = from, .to = args->to, .out = &wrapped);
    EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &wrapped, .letter = args->letter);
    const int written = EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = from, .to = args->to, .text = cell,
                                   .length = wrapped.at, .kind = CLI_VIEW_KIND_NONE, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(cell, g_cli_views_owner);
    if (!written)
    {
        return 0;
    }
    const CliPosition end = args->view->cursor;
    EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = (CliPosition){end.line, end.col - 1ULL},
               .anchor = (CliPosition){from.line, from.col + 1ULL});
    return 1;
}

// a closing bracket, `length` bytes of `text`, typed at `from` on a lead of blanks: the lead taken back a step, a tab,
// the step's spaces where the lead ends in them, else the run of blanks it ends in, and the bracket written after it
EMBED_INLINE int view_letter_outdent(const ViewCtx *args)
{
    const CliPosition from = args->from;
    unsigned long long less = from.col;
    const unsigned long long step = EMBED_CALL(view_tab, ViewCtx, .view_read = args->view);
    if (EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line, .col = less - 1ULL) == '\t')
    {
        less -= 1ULL;
    }
    else
    {
        unsigned long long spaces = 0ULL;
        while ((spaces < less) && (EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line,
                                              .col = less - 1ULL - spaces) == ' '))
        {
            spaces += 1ULL;
        }
        less -= (spaces >= step) ? step : spaces;
    }
    char lead[CLI_PATH_MAX_LEN];
    VerbaLinea line = {lead, sizeof(lead), 0ULL};
    for (unsigned long long at = 0ULL; (at < less) && (line.at + 4ULL < sizeof(lead)); at += 1ULL)
    {
        EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line,
                   .letter = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line, .col = at));
    }
    mmgr_line_put_n(&line, args->text, args->length);
    return EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = (CliPosition){from.line, 0ULL}, .to = args->to,
                      .text = lead, .length = line.at, .kind = args->kind, .err_msg = args->err_msg);
}

// one letter typed, `length` bytes of `text` as the unit `letter`: a closer typed before itself steps over it; an
// opener typed around a selection wraps it, the selection saved inside, and typed alone brings its closer where what
// follows allows, the cursor between them, a quote where what comes before allows too; a closing bracket typed on a
// lead of blanks takes the lead back a step; any other letter is written as typed
EMBED_INLINE int view_letter_type(const ViewCtx *args)
{
    unsigned int letter = args->letter;
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    const int empty = !EMBED_CALL(view_select, ViewCtx, .view_read = args->view);
    const unsigned long long to_length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = to.line);
    const unsigned int next =
        (to.col < to_length) ? EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = to.line, .col = to.col) : 0u;
    const unsigned int prev = (from.col > 0ULL) ? EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line,
                                                             .col = from.col - 1ULL)
                                                : 0u;
    if (empty && EMBED_CALL(view_closes, ViewCtx, .view_read = args->view, .unit = letter) && (next == letter))
    {
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = (CliPosition){to.line, to.col + 1ULL}, .extend = 0,
                   .saved = 0);
        return 1;
    }
    const unsigned int close = EMBED_CALL(view_closer_get, ViewCtx, .view_read = args->view, .unit = letter);
    if ((close != 0u) && !empty)
    {
        return EMBED_CALL(view_letter_wrap, ViewCtx, .view = args->view, .text = args->text, .length = args->length,
                          .letter = close, .from = from, .to = to, .err_msg = args->err_msg);
    }
    if (close != 0u)
    {
        const int quote = close == letter;
        const int next_ok = (next == 0u) || EMBED_CALL(view_blank_unit, ViewCtx, .unit = next) || (next == ';') ||
                            (next == ',') || (next == '.') || (next == ')') || (next == ']') || (next == '}') ||
                            (next == '>');
        const int prev_ok =
            !quote ||
            ((prev != 0u) ? (!EMBED_CALL(view_word_unit, ViewCtx, .unit = prev) && (prev != '\\') && (prev != letter))
                          : 1);
        if (next_ok && prev_ok)
        {
            char both[16];
            VerbaLinea line = {both, sizeof(both), 0ULL};
            mmgr_line_put_n(&line, args->text, args->length);
            EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line, .letter = close);
            if (!EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = from, .to = to, .text = both,
                            .length = line.at, .kind = args->kind, .err_msg = args->err_msg))
            {
                return 0;
            }
            const CliPosition end = args->view->cursor;
            const CliPosition between = {end.line, end.col - 1ULL};
            EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = between, .anchor = between);
            return 1;
        }
    }
    if (EMBED_CALL(view_closes_bracket, ViewCtx, .view_read = args->view, .unit = letter) && empty &&
        (from.col > 0ULL) &&
        (EMBED_CALL(view_first_letter, ViewCtx, .view = args->view, .line = from.line) >= from.col))
    {
        return EMBED_CALL(view_letter_outdent, ViewCtx, .view = args->view, .text = args->text, .length = args->length,
                          .from = from, .to = to, .kind = args->kind, .err_msg = args->err_msg);
    }
    return EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = from, .to = to, .text = args->text,
                      .length = args->length, .kind = args->kind, .err_msg = args->err_msg);
}

EMBED_INLINE int view_pair(const ViewCtx *args);
EMBED_INLINE int view_word_span(const ViewCtx *args);

// the lines the selection covers, a selection ending at a later line's start leaving that line out
EMBED_INLINE void view_lines_cover(const ViewCtx *args)
{
    unsigned long long *first = args->first_pointer;
    unsigned long long *last = args->last_pointer;
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view_read, .from_pointer = &from, .to_pointer = &to);
    *first = from.line;
    *last = ((to.line > from.line) && (to.col == 0ULL)) ? to.line - 1ULL : to.line;
}

// a place carried through the edit of the span from `from` to `to` by text ending at `end`: before it it stays, inside
// it it goes to the span's start, or with `after` to the text's end, and past it it moves with the span's end; a place
// at the span's start goes on only with `after`
EMBED_INLINE CliPosition view_position_machine_transfer(const ViewCtx *args)
{
    if (EMBED_CALL(view_before, ViewCtx, .one = args->place, .other = args->from) ||
        (!EMBED_CALL(view_before, ViewCtx, .one = args->from, .other = args->place) && !args->after))
    {
        return args->place;
    }
    if (EMBED_CALL(view_before, ViewCtx, .one = args->place, .other = args->to))
    {
        return args->after ? args->end : args->from;
    }
    if (args->place.line == args->to.line)
    {
        return (CliPosition){args->end.line, args->end.col + args->place.col - args->to.col};
    }
    return (CliPosition){args->place.line + args->end.line - args->to.line, args->place.col};
}

// the edit cut to the whole lines it changes, those it leaves as they stand taken off its head and its foot: the text
// is one array, an edit moving all that lies after it, and the smaller the span the less the line tables and the
// walk redo: 1, or 0 with its cell given back where it changes nothing
static int view_edit_trim(const ViewCtx *args)
{
    const unsigned long long shorter = (args->made->append_length < args->made->consumed_length)
                                           ? args->made->append_length
                                           : args->made->consumed_length;
    unsigned long long head = 0ULL;
    unsigned long long head_lines = 0ULL;
    unsigned long long same = 0ULL;
    while ((same < shorter) && (args->made->put[same] == args->made->consumed[same]))
    {
        if (args->made->put[same] == '\n')
        {
            head_lines += 1ULL;
            head = same + 1ULL;
        }
        same += 1ULL;
    }
    if ((same == shorter) && (args->made->append_length == args->made->consumed_length))
    {
        g_cli_guard->persistent_buf_release(args->made, g_cli_views_owner);
        return 0;
    }
    args->made->put += head;
    args->made->consumed += head;
    args->made->append_length -= head;
    args->made->consumed_length -= head;
    if (head_lines != 0ULL)
    {
        args->made->from = (CliPosition){args->made->from.line + head_lines, 0ULL};
    }
    const unsigned long long rest = (args->made->append_length < args->made->consumed_length)
                                        ? args->made->append_length
                                        : args->made->consumed_length;
    unsigned long long foot = 0ULL;
    unsigned long long foot_lines = 0ULL;
    unsigned long long back = 0ULL;
    while ((back < rest) && (args->made->put[args->made->append_length - 1ULL - back] ==
                             args->made->consumed[args->made->consumed_length - 1ULL - back]))
    {
        if (args->made->put[args->made->append_length - 1ULL - back] == '\n')
        {
            foot_lines += 1ULL;
            foot = back + 1ULL;
        }
        back += 1ULL;
    }
    args->made->append_length -= foot;
    args->made->consumed_length -= foot;
    if (foot_lines != 0ULL)
    {
        const unsigned long long line = args->made->to.line - foot_lines;
        args->made->to = (CliPosition){line, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = line)};
    }
    return 1;
}

// the edit its cell holds written as a step of its own, cut to the lines it changes, the selection carried through the
// span it covers whole: the cursor to the text's end where it stood in the span, and the anchor to its start, or its
// end where the anchor stood after the cursor
EMBED_INLINE int view_cell_replace(const ViewCtx *args)
{
    const CliPosition from = args->made->from;
    const CliPosition to = args->made->to;
    const CliPosition end = EMBED_CALL(view_position_end, ViewCtx, .from = from, .text = args->made->put,
                                       .bytes = args->made->append_length);
    const CliPosition cursor = args->view->cursor;
    const CliPosition anchor = args->view->anchor;
    const int after = EMBED_CALL(view_select, ViewCtx, .view_read = args->view) &&
                      EMBED_CALL(view_before, ViewCtx, .one = cursor, .other = anchor);
    const CliPosition cursor_transferred = EMBED_CALL(view_position_machine_transfer, ViewCtx, .place = cursor,
                                                      .from = from, .to = to, .end = end, .after = 1);
    const CliPosition anchor_transferred = EMBED_CALL(view_position_machine_transfer, ViewCtx, .place = anchor,
                                                      .from = from, .to = to, .end = end, .after = after);
    if (!view_edit_trim(args))
    {
        args->view->anchor = anchor_transferred;
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = cursor_transferred, .extend = 1, .saved = 0);
        return 1;
    }
    if (!EMBED_CALL(view_edit_cell_write, ViewCtx, .view = args->view, .made = args->made, .kind = CLI_VIEW_KIND_NONE,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = cursor_transferred, .anchor = anchor_transferred);
    return 1;
}

// the selection's lines joined into one, and a cursor's line with the line after it: the space where two lines meet
// made one space, or none where either side is blank
static int view_lines_join(const ViewCtx *args)
{
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    const unsigned long long count = EMBED_CALL(view_lines, ViewCtx, .view_read = args->view);
    const unsigned long long first = from.line;
    const unsigned long long required = (to.line > first + 1ULL) ? to.line : first + 1ULL;
    const unsigned long long last = ((count != 0ULL) && (required > count - 1ULL)) ? count - 1ULL : required;
    if (last == first)
    {
        return 1;
    }
    CliViewEdit *const made =
        EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = (CliPosition){first, 0ULL},
                   .to = (CliPosition){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)},
                   .append_bytes = 0ULL, .append_spans = 1ULL, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    // a line's break gives way to the one space, so the joined text never outgrows what was read
    const char *const consumed = made->consumed;
    const unsigned long long size = made->consumed_length;
    char *const put = made->put;
    unsigned long long at = 0ULL;
    unsigned long long read = 0ULL;
    while ((read < size) && (consumed[read] != '\n'))
    {
        put[at] = consumed[read];
        at += 1ULL;
        read += 1ULL;
    }
    while (read < size)
    {
        read += 1ULL;
        while ((read < size) && ((consumed[read] == ' ') || (consumed[read] == '\t')))
        {
            read += 1ULL;
        }
        while ((at > 0ULL) && ((put[at - 1ULL] == ' ') || (put[at - 1ULL] == '\t')))
        {
            at -= 1ULL;
        }
        if ((at != 0ULL) && (read < size) && (consumed[read] != '\n'))
        {
            put[at] = ' ';
            at += 1ULL;
        }
        while ((read < size) && (consumed[read] != '\n'))
        {
            put[at] = consumed[read];
            at += 1ULL;
            read += 1ULL;
        }
    }
    made->append_length = at;
    return EMBED_CALL(view_cell_replace, ViewCtx, .view = args->view, .made = made, .err_msg = args->err_msg);
}

// the selection's lines moved a line up or down over their neighbour, the selection with them
static int view_lines_move(const ViewCtx *args)
{
    unsigned long long first = 0ULL;
    unsigned long long last = 0ULL;
    EMBED_CALL(view_lines_cover, ViewCtx, .view_read = args->view, .first_pointer = &first, .last_pointer = &last);
    const unsigned long long count = EMBED_CALL(view_lines, ViewCtx, .view_read = args->view);
    if ((args->up && (first == 0ULL)) || (!args->up && (last + 1ULL >= count)))
    {
        return 1;
    }
    const unsigned long long from_line = args->up ? first - 1ULL : first;
    const unsigned long long to_line = args->up ? last : last + 1ULL;
    CliViewEdit *const made =
        EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = (CliPosition){from_line, 0ULL},
                   .to = (CliPosition){to_line, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = to_line)},
                   .append_bytes = 0ULL, .append_spans = 1ULL, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    // the neighbour's line and the block change places: up, the first line goes to the foot; down, the last to the head
    const char *const consumed = made->consumed;
    const unsigned long long size = made->consumed_length;
    unsigned long long split = args->up ? 0ULL : size - 1ULL;
    while (consumed[split] != '\n')
    {
        split = args->up ? split + 1ULL : split - 1ULL;
    }
    const unsigned long long after = size - split - 1ULL;
    memor.cpy(&(MemoriaCfg){.dst = made->put, .src = consumed + split + 1ULL, .bytes = after});
    made->put[after] = '\n';
    memor.cpy(&(MemoriaCfg){.dst = made->put + after + 1ULL, .src = consumed, .bytes = split});
    made->append_length = size;
    const CliPosition cursor = {args->up ? args->view->cursor.line - 1ULL : args->view->cursor.line + 1ULL,
                                args->view->cursor.col};
    const CliPosition anchor = {args->up ? args->view->anchor.line - 1ULL : args->view->anchor.line + 1ULL,
                                args->view->anchor.col};
    if (!EMBED_CALL(view_edit_cell_write, ViewCtx, .view = args->view, .made = made, .kind = CLI_VIEW_KIND_NONE,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = cursor, .anchor = anchor);
    return 1;
}

// the selection's lines copied above or below themselves: below, the selection goes with the copy; above, it stays
EMBED_INLINE int view_lines_copy(const ViewCtx *args)
{
    unsigned long long first = 0ULL;
    unsigned long long last = 0ULL;
    EMBED_CALL(view_lines_cover, ViewCtx, .view_read = args->view, .first_pointer = &first, .last_pointer = &last);
    const CliPosition head = {first, 0ULL};
    const CliPosition foot = {last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)};
    const unsigned long long bytes = EMBED_CALL(view_span_bytes, ViewCtx, .view = args->view, .from = head, .to = foot);
    const CliPosition place = args->up ? head : foot;
    CliViewEdit *const made = EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = place, .to = place,
                                         .append_bytes = bytes + 1ULL, .append_spans = 0ULL, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    VerbaLinea copy = {made->put, bytes + 2ULL, 0ULL};
    if (!args->up)
    {
        mmgr_line_ch(&copy, '\n');
    }
    EMBED_CALL(view_span_write, ViewCtx, .view = args->view, .from = head, .to = foot, .out = &copy);
    if (args->up)
    {
        mmgr_line_ch(&copy, '\n');
    }
    made->append_length = copy.at;
    const unsigned long long by = last - first + 1ULL;
    const CliPosition cursor = {args->view->cursor.line + (((args->view->cursor.line > last) || !args->up) ? by : 0ULL),
                                args->view->cursor.col};
    const CliPosition anchor = {args->view->anchor.line + (((args->view->anchor.line > last) || !args->up) ? by : 0ULL),
                                args->view->anchor.col};
    if (!EMBED_CALL(view_edit_cell_write, ViewCtx, .view = args->view, .made = made, .kind = CLI_VIEW_KIND_NONE,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = cursor, .anchor = anchor);
    return 1;
}

// the selection's lines deleted with a line's break, the one after them where one follows and else the one before: a
// cursor left on what comes up into their place, at its column as far as that line reaches
EMBED_INLINE int view_lines_delete(const ViewCtx *args)
{
    unsigned long long first = 0ULL;
    unsigned long long last = 0ULL;
    EMBED_CALL(view_lines_cover, ViewCtx, .view_read = args->view, .first_pointer = &first, .last_pointer = &last);
    const unsigned long long count = EMBED_CALL(view_lines, ViewCtx, .view_read = args->view);
    const unsigned long long col = args->view->cursor.col;
    const int follows = last + 1ULL < count;
    const CliPosition from =
        (follows || (first == 0ULL))
            ? (CliPosition){first, 0ULL}
            : (CliPosition){first - 1ULL, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = first - 1ULL)};
    const CliPosition to =
        follows ? (CliPosition){last + 1ULL, 0ULL}
                : (CliPosition){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)};
    if (!EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = from, .to = to, .text = "", .length = 0ULL,
                    .kind = CLI_VIEW_KIND_NONE, .err_msg = args->err_msg))
    {
        return 0;
    }
    const unsigned long long length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = from.line);
    const CliPosition place = {from.line, (col < length) ? col : length};
    EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = place, .anchor = place);
    return 1;
}

// the line `line` of the selection, its bytes from `from_u64` to `to_u64` of the edit's `text`, written into `mark`
// from `at`: where `deeper`, `count` tabs or spaces put before it, else a tab or as many spaces of a step of `width` as
// lead it taken out; the cursor and anchor on it carried into `cursor_pointer` and `anchor_pointer`, a place at the
// line's start staying there where `selected`. Where the writing goes on
EMBED_INLINE unsigned long long view_indent_line(const ViewCtx *args)
{
    const CliView *const view = args->view_read;
    const char *const text = args->text;
    const unsigned long long read = args->from_u64;
    const unsigned long long end = args->to_u64;
    const unsigned long long line = args->line;
    unsigned long long at = args->at;
    unsigned long long cut = 0ULL;
    unsigned long long added = 0ULL;
    if (args->deeper)
    {
        added = args->count;
        memor.set(&(MemoriaCfg){.dst = args->mark + at, .val = args->tabs ? '\t' : ' ', .bytes = added});
        at += added;
    }
    else if ((read < end) && (text[read] == '\t'))
    {
        cut = 1ULL;
    }
    else
    {
        while ((cut < args->width) && (read + cut < end) && (text[read + cut] == ' '))
        {
            cut += 1ULL;
        }
    }
    memor.cpy(&(MemoriaCfg){.dst = args->mark + at, .src = text + read + cut, .bytes = end - read - cut});
    at += end - read - cut;
    if ((added == 0ULL) && (cut == 0ULL))
    {
        return at;
    }
    const CliPosition from = {line, 0ULL};
    const CliPosition to = {line, cut};
    const CliPosition ends = {line, added};
    if (view->cursor.line == line)
    {
        *args->cursor_pointer =
            EMBED_CALL(view_position_machine_transfer, ViewCtx, .place = view->cursor, .from = from, .to = to,
                       .end = ends, .after = (view->cursor.col > 0ULL) || !args->selected);
    }
    if (view->anchor.line == line)
    {
        *args->anchor_pointer =
            EMBED_CALL(view_position_machine_transfer, ViewCtx, .place = view->anchor, .from = from, .to = to,
                       .end = ends, .after = (view->anchor.col > 0ULL) || !args->selected);
    }
    return at;
}

// the selection's lines a step of indentation deeper, a blank line among several left as it is, or a step shallower: a
// tab or as many spaces of a step as lead the line; the selection carried, a place at a line's start staying there
// where a selection stands
static int view_lines_indent(const ViewCtx *args)
{
    unsigned long long first = 0ULL;
    unsigned long long last = 0ULL;
    EMBED_CALL(view_lines_cover, ViewCtx, .view_read = args->view, .first_pointer = &first, .last_pointer = &last);
    const int tabs = args->view->indent.tabs == 1;
    const unsigned long long stop = EMBED_CALL(view_tab, ViewCtx, .view_read = args->view);
    const unsigned long long step = tabs ? 1ULL : stop;
    const unsigned long long lines = last - first + 1ULL;
    CliViewEdit *const made =
        EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = (CliPosition){first, 0ULL},
                   .to = (CliPosition){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)},
                   .append_bytes = args->deeper ? lines * step : 0ULL, .append_spans = 1ULL, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    const char *const consumed = made->consumed;
    const unsigned long long size = made->consumed_length;
    char *const put = made->put;
    const int selected = EMBED_CALL(view_select, ViewCtx, .view_read = args->view);
    CliPosition cursor = args->view->cursor;
    CliPosition anchor = args->view->anchor;
    unsigned long long at = 0ULL;
    unsigned long long read = 0ULL;
    for (unsigned long long line = first; line <= last; line += 1ULL)
    {
        unsigned long long end = read;
        int filled = 0;
        while ((end < size) && (consumed[end] != '\n'))
        {
            filled = filled || ((consumed[end] != ' ') && (consumed[end] != '\t'));
            end += 1ULL;
        }
        at = EMBED_CALL(view_indent_line, ViewCtx, .view_read = args->view, .text = consumed, .from_u64 = read,
                        .to_u64 = end, .line = line, .mark = put, .at = at, .deeper = args->deeper,
                        .count = (filled || (lines == 1ULL)) ? step : 0ULL, .width = stop, .tabs = tabs,
                        .selected = selected, .cursor_pointer = &cursor, .anchor_pointer = &anchor);
        if (end < size)
        {
            put[at] = '\n';
            at += 1ULL;
        }
        read = end + 1ULL;
    }
    made->append_length = at;
    if (!EMBED_CALL(view_edit_trim, ViewCtx, .view = args->view, .made = made))
    {
        return 1;
    }
    if (!EMBED_CALL(view_edit_cell_write, ViewCtx, .view = args->view, .made = made, .kind = CLI_VIEW_KIND_NONE,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = cursor, .anchor = anchor);
    return 1;
}

// the end of the line of `text`, `size` bytes, that starts at `from_u64`: its newline, or the text's end
EMBED_INLINE unsigned long long view_line_end(const ViewCtx *args)
{
    unsigned long long end = args->from_u64;
    while ((end < args->size) && (args->text[end] != '\n'))
    {
        end += 1ULL;
    }
    return end;
}

// the first byte of `text` from `from_u64` to before `to_u64` that is neither a space nor a tab, `to_u64` where none is
EMBED_INLINE unsigned long long view_line_blanks(const ViewCtx *args)
{
    unsigned long long blanks = args->from_u64;
    while ((blanks < args->to_u64) && ((args->text[blanks] == ' ') || (args->text[blanks] == '\t')))
    {
        blanks += 1ULL;
    }
    return blanks;
}

// the filled line `line` of the selection, its bytes from `from_u64` to `to_u64` of the edit's `text` and its first
// filled byte at `start_value`, written into `mark` from `at`: marked as a comment with the language's line mark,
// `count` bytes and `units_u64` units, at the indentation `col`, or where `marked` already, the mark and a space after
// it taken out; the cursor and anchor on it carried into `cursor_pointer` and `anchor_pointer`, the anchor after the
// edit where `after`. Where the writing goes on
EMBED_INLINE unsigned long long view_comment_line(const ViewCtx *args)
{
    const CliView *const view = args->view_read;
    const char *const text = args->text;
    const unsigned long long read = args->from_u64;
    const unsigned long long end = args->to_u64;
    const unsigned long long blanks = args->start_value;
    const unsigned long long token = args->count;
    const unsigned long long line = args->line;
    char *const put = args->mark;
    unsigned long long at = args->at;
    // the blanks are a byte a unit: a place among them is their count
    const unsigned long long place = args->marked ? blanks - read : args->col;
    const int spaced = (blanks + token < end) && (text[blanks + token] == ' ');
    const unsigned long long space_after = spaced ? 1ULL : 0ULL;
    const unsigned long long cut = args->marked ? (token + space_after) : 0ULL;
    const CliPosition from = {line, place};
    const CliPosition to = {line, place + (args->marked ? (cut - token) + args->units_u64 : 0ULL)};
    const CliPosition ends = {line, args->marked ? place : place + args->units_u64 + 1ULL};
    if (view->cursor.line == line)
    {
        *args->cursor_pointer = EMBED_CALL(view_position_machine_transfer, ViewCtx, .place = view->cursor, .from = from,
                                           .to = to, .end = ends, .after = 1);
    }
    if (view->anchor.line == line)
    {
        *args->anchor_pointer = EMBED_CALL(view_position_machine_transfer, ViewCtx, .place = view->anchor, .from = from,
                                           .to = to, .end = ends, .after = args->after);
    }
    memor.cpy(&(MemoriaCfg){.dst = put + at, .src = text + read, .bytes = place});
    at += place;
    if (!args->marked)
    {
        memor.cpy(&(MemoriaCfg){.dst = put + at, .src = view->comment_line, .bytes = token});
        at += token;
        put[at] = ' ';
        at += 1ULL;
    }
    const unsigned long long rest = read + place + cut;
    memor.cpy(&(MemoriaCfg){.dst = put + at, .src = text + rest, .bytes = end - rest});
    return at + (end - rest);
}

// the selection's filled lines marked as comments with the language's line mark, at the least indentation among them,
// or where each is marked already the mark and a space after it taken out; the selection carried through each line's
// edit
EMBED_INLINE int view_lines_comment(const ViewCtx *args)
{
    const unsigned long long token =
        cellul.len(&(CatenaFinitaCfg){.src = args->view->comment_line, .cap = sizeof(args->view->comment_line)});
    unsigned long long first = 0ULL;
    unsigned long long last = 0ULL;
    EMBED_CALL(view_lines_cover, ViewCtx, .view_read = args->view, .first_pointer = &first, .last_pointer = &last);
    const unsigned long long lines = last - first + 1ULL;
    CliViewEdit *const made =
        EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = (CliPosition){first, 0ULL},
                   .to = (CliPosition){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)},
                   .append_bytes = lines * (token + 1ULL), .append_spans = 1ULL, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    const char *const consumed = made->consumed;
    const unsigned long long size = made->consumed_length;
    char *const put = made->put;
    // the filled lines read: whether each begins with the mark past its blanks, and the least of their blanks
    int any = 0;
    int marked = 1;
    unsigned long long lead = ~0ULL;
    unsigned long long read = 0ULL;
    while (read <= size)
    {
        const unsigned long long end =
            EMBED_CALL(view_line_end, ViewCtx, .text = consumed, .size = size, .from_u64 = read);
        const unsigned long long blanks =
            EMBED_CALL(view_line_blanks, ViewCtx, .text = consumed, .from_u64 = read, .to_u64 = end);
        if (blanks < end)
        {
            any = 1;
            lead = (blanks - read < lead) ? blanks - read : lead;
            marked = marked && (end - blanks >= token) &&
                     (memor.cmp(&(MemoriaCfg){
                          .src = consumed + blanks, .other = args->view->comment_line, .bytes = token}) == 0);
        }
        read = end + 1ULL;
    }
    if (!any)
    {
        g_cli_guard->persistent_buf_release(made, g_cli_views_owner);
        return 1;
    }
    const int after = EMBED_CALL(view_select, ViewCtx, .view_read = args->view) &&
                      EMBED_CALL(view_before, ViewCtx, .one = args->view->cursor, .other = args->view->anchor);
    const unsigned long long token_units =
        EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = args->view->comment_line, .size = token);
    CliPosition cursor = args->view->cursor;
    CliPosition anchor = args->view->anchor;
    unsigned long long at = 0ULL;
    read = 0ULL;
    for (unsigned long long line = first; line <= last; line += 1ULL)
    {
        const unsigned long long end =
            EMBED_CALL(view_line_end, ViewCtx, .text = consumed, .size = size, .from_u64 = read);
        const unsigned long long blanks =
            EMBED_CALL(view_line_blanks, ViewCtx, .text = consumed, .from_u64 = read, .to_u64 = end);
        if (blanks < end)
        {
            at = EMBED_CALL(view_comment_line, ViewCtx, .view_read = args->view, .text = consumed, .from_u64 = read,
                            .to_u64 = end, .start_value = blanks, .line = line, .mark = put, .at = at, .marked = marked,
                            .col = lead, .count = token, .units_u64 = token_units, .after = after,
                            .cursor_pointer = &cursor, .anchor_pointer = &anchor);
        }
        else
        {
            memor.cpy(&(MemoriaCfg){.dst = put + at, .src = consumed + read, .bytes = end - read});
            at += end - read;
        }
        if (end < size)
        {
            put[at] = '\n';
            at += 1ULL;
        }
        read = end + 1ULL;
    }
    made->append_length = at;
    if (!EMBED_CALL(view_edit_trim, ViewCtx, .view = args->view, .made = made))
    {
        return 1;
    }
    if (!EMBED_CALL(view_edit_cell_write, ViewCtx, .view = args->view, .made = made, .kind = CLI_VIEW_KIND_NONE,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = cursor, .anchor = anchor);
    return 1;
}

// the selection wrapped in the language's block marks, the selection saved on the same text; or where it stands wrapped
// already the marks and a space inside each taken out
EMBED_INLINE int view_block_comment(const ViewCtx *args)
{
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    const unsigned long long open =
        cellul.len(&(CatenaFinitaCfg){.src = args->view->comment_open, .cap = sizeof(args->view->comment_open)});
    const unsigned long long close =
        cellul.len(&(CatenaFinitaCfg){.src = args->view->comment_close, .cap = sizeof(args->view->comment_close)});
    CliViewEdit *const made =
        EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = from, .to = to,
                   .append_bytes = open + close + 2ULL, .append_spans = 1ULL, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    const char *const consumed = made->consumed;
    const unsigned long long size = made->consumed_length;
    char *const put = made->put;
    const int wrapped =
        (size >= open + close) &&
        (memor.cmp(&(MemoriaCfg){.src = consumed, .other = args->view->comment_open, .bytes = open}) == 0) &&
        (memor.cmp(&(MemoriaCfg){.src = consumed + size - close, .other = args->view->comment_close, .bytes = close}) ==
         0);
    unsigned long long saved_from = 0ULL;
    unsigned long long saved_bytes = 0ULL;
    if (wrapped)
    {
        unsigned long long inner = open;
        unsigned long long stop = size - close;
        inner += ((inner < stop) && (consumed[inner] == ' ')) ? 1ULL : 0ULL;
        stop -= ((inner < stop) && (consumed[stop - 1ULL] == ' ')) ? 1ULL : 0ULL;
        saved_bytes = stop - inner;
        memor.cpy(&(MemoriaCfg){.dst = put, .src = consumed + inner, .bytes = saved_bytes});
        made->append_length = saved_bytes;
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = put, .src = args->view->comment_open, .bytes = open});
        put[open] = ' ';
        memor.cpy(&(MemoriaCfg){.dst = put + open + 1ULL, .src = consumed, .bytes = size});
        put[open + 1ULL + size] = ' ';
        memor.cpy(&(MemoriaCfg){.dst = put + open + 2ULL + size, .src = args->view->comment_close, .bytes = close});
        made->append_length = open + 2ULL + size + close;
        saved_from = open + 1ULL;
        saved_bytes = size;
    }
    const CliPosition start = EMBED_CALL(view_position_end, ViewCtx, .from = from, .text = put, .bytes = saved_from);
    const CliPosition end =
        EMBED_CALL(view_position_end, ViewCtx, .from = from, .text = put, .bytes = saved_from + saved_bytes);
    if (!EMBED_CALL(view_edit_cell_write, ViewCtx, .view = args->view, .made = made, .kind = CLI_VIEW_KIND_NONE,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = end, .anchor = start);
    return 1;
}

// a line opened below the cursor's line at its indentation, the cursor on it; or above it, the cursor on that line
EMBED_INLINE int view_line_insert(const ViewCtx *args)
{
    const unsigned long long line = args->view->cursor.line;
    const unsigned long long lead = EMBED_CALL(view_first_letter, ViewCtx, .view = args->view, .line = line);
    const CliPosition place =
        args->below ? (CliPosition){line, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = line)}
                    : (CliPosition){line, 0ULL};
    // the lead is blanks, a byte a unit
    CliViewEdit *const made = EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = place, .to = place,
                                         .append_bytes = lead + 1ULL, .append_spans = 0ULL, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    char *const put = made->put;
    unsigned long long at = 0ULL;
    if (args->below)
    {
        put[0] = '\n';
        at = 1ULL;
    }
    for (unsigned long long col = 0ULL; col < lead; col += 1ULL)
    {
        // a blank unit is a byte
        put[at] = (char)EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = line, .col = col);
        at += 1ULL;
    }
    if (!args->below)
    {
        put[at] = '\n';
        at += 1ULL;
    }
    made->append_length = at;
    if (!EMBED_CALL(view_edit_cell_write, ViewCtx, .view = args->view, .made = made, .kind = CLI_VIEW_KIND_NONE,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!args->below)
    {
        const CliPosition saved = {line, lead};
        EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = saved, .anchor = saved);
    }
    return 1;
}

// the whole lines the selection touches selected, through the next line's start, the next line taken where the
// selection is whole lines already
EMBED_INLINE void view_line_select(const ViewCtx *args)
{
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    const unsigned long long count = EMBED_CALL(view_lines, ViewCtx, .view_read = args->view);
    if (EMBED_CALL(view_select, ViewCtx, .view_read = args->view) && (from.col == 0ULL) && (to.col == 0ULL))
    {
        to.col = 1ULL;
    }
    const unsigned long long last = ((to.col == 0ULL) && (to.line > from.line)) ? to.line - 1ULL : to.line;
    args->view->anchor = (CliPosition){from.line, 0ULL};
    EMBED_CALL(view_move, ViewCtx, .view = args->view,
               .place = (last + 1ULL < count)
                            ? (CliPosition){last + 1ULL, 0ULL}
                            : (CliPosition){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)},
               .extend = 1, .saved = 0);
}

// the ASCII marks in the order a collation of the root locale orders them, blanks before them, digits and letters after
static const char s_view_marks[] = "_-,;:!?.'\"()[]{}@*/\\&#%`^+<=>|~$";

// each byte's first rank in the collation: a blank, a mark in its order, a digit, a letter whatever its case, and a
// byte past ASCII past them all; made once
static unsigned short s_view_ranks[256];

static void view_ranks_init(void)
{
    if (s_view_ranks['a'] != 0u)
    {
        return;
    }
    for (unsigned int byte = 0u; byte < 256u; byte += 1u)
    {
        s_view_ranks[byte] = (unsigned short)(1000u + byte);
    }
    s_view_ranks[' '] = 1u;
    s_view_ranks['\t'] = 1u;
    for (unsigned int at = 0u; at + 1u < sizeof(s_view_marks); at += 1u)
    {
        s_view_ranks[(unsigned char)s_view_marks[at]] = (unsigned short)(2u + at);
    }
    for (unsigned int digit = '0'; digit <= '9'; digit += 1u)
    {
        s_view_ranks[digit] = (unsigned short)(100u + digit);
    }
    for (unsigned int letter = 'a'; letter <= 'z'; letter += 1u)
    {
        s_view_ranks[letter] = (unsigned short)(300u + letter);
        s_view_ranks[letter - ('a' - 'A')] = (unsigned short)(300u + letter);
    }
}

// two lines as the numeric collator orders them: by rank, a run of digits by the number it writes, and where those are
// level a small letter before its capital: below 0, 0 or above 0
// the runs of digits at `from_u64s` in `text`, `size` bytes, and at `to_u64s` in `supplied`, `length` bytes, compared
// as the numbers they write, their leading zeros passed over, each place moved past its run: -1 or 1 where the first
// is less or more, 0 where they are equal
EMBED_INLINE int view_numbers_compared(const ViewCtx *args)
{
    const char *const one = args->text;
    const char *const other = args->supplied;
    unsigned long long left = *args->from_u64s;
    unsigned long long right = *args->to_u64s;
    unsigned long long a_end = left;
    unsigned long long b_end = right;
    while ((a_end < args->size) && ((unsigned int)((unsigned char)one[a_end] - '0') < 10u))
    {
        a_end += 1ULL;
    }
    while ((b_end < args->length) && ((unsigned int)((unsigned char)other[b_end] - '0') < 10u))
    {
        b_end += 1ULL;
    }
    while ((left + 1ULL < a_end) && (one[left] == '0'))
    {
        left += 1ULL;
    }
    while ((right + 1ULL < b_end) && (other[right] == '0'))
    {
        right += 1ULL;
    }
    *args->from_u64s = a_end;
    *args->to_u64s = b_end;
    if (a_end - left != b_end - right)
    {
        return (a_end - left < b_end - right) ? -1 : 1;
    }
    const embed_iword digits =
        memor.cmp(&(MemoriaCfg){.src = one + left, .other = other + right, .bytes = a_end - left});
    if (digits == 0)
    {
        return 0;
    }
    return (digits < 0) ? -1 : 1;
}

// two lines level but for case: a small letter, the greater byte, first
EMBED_INLINE int view_case_compared(const ViewCtx *args)
{
    const unsigned long long shorter = (args->size < args->length) ? args->size : args->length;
    for (unsigned long long at = 0ULL; at < shorter; at += 1ULL)
    {
        if (args->text[at] != args->supplied[at])
        {
            return ((unsigned char)args->text[at] > (unsigned char)args->supplied[at]) ? -1 : 1;
        }
    }
    return 0;
}

static int view_lines_collated(const CliCompareCfg *args)
{
    const char *other = args->other;
    unsigned long long left = 0ULL;
    unsigned long long right = 0ULL;
    while ((left < args->one_length) && (right < args->other_length))
    {
        const unsigned char a = (unsigned char)args->one[left];
        const unsigned char b = (unsigned char)other[right];
        if (((unsigned int)(a - '0') < 10u) && ((unsigned int)(b - '0') < 10u))
        {
            const int numbers =
                EMBED_CALL(view_numbers_compared, ViewCtx, .text = args->one, .size = args->one_length,
                           .supplied = other, .length = args->other_length, .from_u64s = &left, .to_u64s = &right);
            if (numbers != 0)
            {
                return numbers;
            }
            continue;
        }
        if (s_view_ranks[a] != s_view_ranks[b])
        {
            return (s_view_ranks[a] < s_view_ranks[b]) ? -1 : 1;
        }
        left += 1ULL;
        right += 1ULL;
    }
    if ((left < args->one_length) || (right < args->other_length))
    {
        return (left < args->one_length) ? 1 : -1;
    }
    return EMBED_CALL(view_case_compared, ViewCtx, .text = args->one, .size = args->one_length, .supplied = other,
                      .length = args->other_length);
}

// two lines by their bytes
static int view_lines_bytes(const CliCompareCfg *args)
{
    const char *other = args->other;
    const unsigned long long shorter = (args->one_length < args->other_length) ? args->one_length : args->other_length;
    const embed_iword bytes = memor.cmp(&(MemoriaCfg){.src = args->one, .other = other, .bytes = shorter});
    if (bytes != 0)
    {
        return (bytes < 0) ? -1 : 1;
    }
    if (args->one_length == args->other_length)
    {
        return 0;
    }
    return (args->one_length < args->other_length) ? -1 : 1;
}

// one pass of the merge sort: each two runs of `width` lines in `order` merged into `work`, level ones saving their
// order
EMBED_INLINE void view_lines_merge_pass(const ViewCtx *args)
{
    const unsigned int *const from = args->order;
    unsigned int *const into = args->work;
    // the width of a pass is below the count of lines, which an unsigned int counts
    const unsigned int width = (unsigned int)args->width;
    for (unsigned int first = 0u; first < args->count_u32; first += 2u * width)
    {
        const unsigned int middle = (first + width < args->count_u32) ? first + width : args->count_u32;
        const unsigned int end = (first + (2u * width) < args->count_u32) ? first + (2u * width) : args->count_u32;
        unsigned int left = first;
        unsigned int right = middle;
        for (unsigned int at = first; at < end; at += 1u)
        {
            int take_left = left < middle;
            if (take_left && (right < end))
            {
                const unsigned int one = from[left];
                const unsigned int other = from[right];
                const int compared = EMBED_CALL(args->compare, CliCompareCfg, .one = args->text + args->start[one],
                                                .one_length = args->start[one + 1u] - args->start[one] - 1ULL,
                                                .other = args->text + args->start[other],
                                                .other_length = args->start[other + 1u] - args->start[other] - 1ULL);
                take_left = (args->descending ? -compared : compared) <= 0;
            }
            into[at] = take_left ? from[left] : from[right];
            left += take_left ? 1u : 0u;
            right += take_left ? 0u : 1u;
        }
    }
}

// the `count` lines of `text`, each at its `start` and ending a byte before the next, their numbers in `order` merge
// sorted, level ones saving their order, by `compare`, or its reverse with `descending`, `work` a table of as many
EMBED_INLINE void view_lines_merge(const ViewCtx *args)
{
    unsigned int *from = args->order;
    unsigned int *into = args->work;
    for (unsigned int width = 1u; width < args->count_u32; width *= 2u)
    {
        EMBED_CALL(view_lines_merge_pass, ViewCtx, .order = from, .work = into, .width = width,
                   .count_u32 = args->count_u32, .compare = args->compare, .text = args->text, .start = args->start,
                   .descending = args->descending);
        unsigned int *const was = from;
        from = into;
        into = was;
    }
    if (from != args->order)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = args->order, .src = from, .bytes = (unsigned long long)args->count_u32 * sizeof(*args->order)});
    }
}

// the selection's lines, or with a cursor alone the whole text's, sorted by the numeric collator, or with `unique` the
// lines that repeat one before them taken out, the first of each saved; a table of the lines' places and numbers lasts
// the call, the text being the edit's own
static int view_lines_sort(const ViewCtx *args)
{
    unsigned long long first = 0ULL;
    unsigned long long last = (EMBED_CALL(view_lines, ViewCtx, .view_read = args->view) != 0ULL)
                                  ? EMBED_CALL(view_lines, ViewCtx, .view_read = args->view) - 1ULL
                                  : 0ULL;
    if (EMBED_CALL(view_select, ViewCtx, .view_read = args->view))
    {
        EMBED_CALL(view_lines_cover, ViewCtx, .view_read = args->view, .first_pointer = &first, .last_pointer = &last);
    }
    // the lines a text holds are counted by an unsigned int
    const unsigned int count = (unsigned int)(last - first + 1ULL);
    if (count < 2u)
    {
        return 1;
    }
    CliViewEdit *const made =
        EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = (CliPosition){first, 0ULL},
                   .to = (CliPosition){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)},
                   .append_bytes = 0ULL, .append_spans = 1ULL, .err_msg = args->err_msg);
    const unsigned long long table = (((unsigned long long)count + 1ULL) * sizeof(unsigned long long)) +
                                     ((unsigned long long)count * ((2ULL * sizeof(unsigned int)) + 1ULL));
    unsigned long long *const start =
        (made != NULL) ? g_cli_guard->temporary_buf_alloc(table, g_cli_views_owner) : NULL;
    if (start == NULL)
    {
        g_cli_guard->persistent_buf_release(made, g_cli_views_owner);
        return 0;
    }
    unsigned int *const order = (unsigned int *)(start + count + 1u);
    unsigned int *const work = order + count;
    unsigned char *const repeats = (unsigned char *)(work + count);
    const char *const consumed = made->consumed;
    const unsigned long long size = made->consumed_length;
    unsigned int line = 0u;
    start[0] = 0ULL;
    for (unsigned long long at = 0ULL; at < size; at += 1ULL)
    {
        if (consumed[at] == '\n')
        {
            line += 1u;
            start[line] = at + 1ULL;
        }
    }
    start[count] = size + 1ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        order[at] = at;
    }
    char *const put = made->put;
    unsigned long long at = 0ULL;
    if (args->unique)
    {
        EMBED_CALL(view_lines_merge, ViewCtx, .text = consumed, .start = start, .order = order, .work = work,
                   .count_u32 = count, .compare = view_lines_bytes, .descending = 0);
        memor.set(&(MemoriaCfg){.dst = repeats, .val = 0, .bytes = count});
        for (unsigned int sorted = 1u; sorted < count; sorted += 1u)
        {
            const unsigned int one = order[sorted - 1u];
            const unsigned int other = order[sorted];
            if (EMBED_CALL(view_lines_bytes, CliCompareCfg, .one = consumed + start[one],
                           .one_length = start[one + 1u] - start[one] - 1ULL, .other = consumed + start[other],
                           .other_length = start[other + 1u] - start[other] - 1ULL) == 0)
            {
                repeats[other] = 1u;
            }
        }
    }
    else
    {
        view_ranks_init();
        EMBED_CALL(view_lines_merge, ViewCtx, .text = consumed, .start = start, .order = order, .work = work,
                   .count_u32 = count, .compare = view_lines_collated, .descending = args->descending);
    }
    for (unsigned int written = 0u; written < count; written += 1u)
    {
        const unsigned int each = args->unique ? written : order[written];
        if (!args->unique || !repeats[each])
        {
            if (at != 0ULL)
            {
                put[at] = '\n';
                at += 1ULL;
            }
            const unsigned long long length = start[each + 1u] - start[each] - 1ULL;
            memor.cpy(&(MemoriaCfg){.dst = put + at, .src = consumed + start[each], .bytes = length});
            at += length;
        }
    }
    made->append_length = at;
    g_cli_guard->persistent_buf_release(start, g_cli_views_owner);
    return EMBED_CALL(view_cell_replace, ViewCtx, .view = args->view, .made = made, .err_msg = args->err_msg);
}

// a letter in its other case: `upper` its capital, else its small letter, the letters of ASCII, Latin-1, Latin
// Extended-A, Greek and Cyrillic; any other as it is; each stays as many bytes of UTF-8 as it was
// whether a unit is a small letter of a run of Latin-1, Greek or Cyrillic whose capitals stand 0x20 before its small
// letters
EMBED_INLINE int view_unit_small(const ViewCtx *args)
{
    const unsigned int unit = args->unit;
    return ((unit >= 'a') && (unit <= 'z')) || ((unit >= 0xe0u) && (unit <= 0xfeu) && (unit != 0xf7u)) ||
           ((unit >= 0x3b1u) && (unit <= 0x3c9u) && (unit != 0x3c2u)) || ((unit >= 0x430u) && (unit <= 0x44fu));
}

// whether a unit is a capital of a run of Latin-1, Greek or Cyrillic whose small letters stand 0x20 after its capitals
EMBED_INLINE int view_unit_capital(const ViewCtx *args)
{
    const unsigned int unit = args->unit;
    return ((unit >= 'A') && (unit <= 'Z')) || ((unit >= 0xc0u) && (unit <= 0xdeu) && (unit != 0xd7u)) ||
           ((unit >= 0x391u) && (unit <= 0x3a9u)) || ((unit >= 0x410u) && (unit <= 0x42fu));
}

// a unit of Latin Extended-A, which pairs each capital with the letter after it, an even or an odd capital by its run,
// in capitals where `upper`, else in small letters; any other unit as it is
EMBED_INLINE unsigned int view_unit_case_paired(const ViewCtx *args)
{
    const unsigned int unit = args->unit;
    const int even_run = ((unit >= 0x100u) && (unit <= 0x137u)) || ((unit >= 0x14au) && (unit <= 0x177u));
    const int odd_run = ((unit >= 0x139u) && (unit <= 0x148u)) || ((unit >= 0x179u) && (unit <= 0x17eu));
    if (!even_run && !odd_run)
    {
        return unit;
    }
    const int capital = even_run ? ((unit & 1u) == 0u) : ((unit & 1u) == 1u);
    if (args->upper && !capital)
    {
        return unit - 1u;
    }
    if (!args->upper && capital)
    {
        return unit + 1u;
    }
    return unit;
}

EMBED_INLINE unsigned int view_unit_case(const ViewCtx *args)
{
    const unsigned int unit = args->unit;
    if (args->upper && EMBED_CALL(view_unit_small, ViewCtx, .unit = unit))
    {
        return unit - 0x20u;
    }
    if (!args->upper && EMBED_CALL(view_unit_capital, ViewCtx, .unit = unit))
    {
        return unit + 0x20u;
    }
    if (args->upper && (unit >= 0x450u) && (unit <= 0x45fu))
    {
        return unit - 0x50u;
    }
    if (!args->upper && (unit >= 0x400u) && (unit <= 0x40fu))
    {
        return unit + 0x50u;
    }
    return EMBED_CALL(view_unit_case_paired, ViewCtx, .unit = unit, .upper = args->upper);
}

// whether a unit is a letter, and whether it carries a title's word on, a letter, a digit or an apostrophe
EMBED_INLINE int view_unit_letter(const ViewCtx *args)
{
    return EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = args->unit) &&
           !((args->unit >= '0') && (args->unit <= '9'));
}

EMBED_INLINE int view_unit_word_on(const ViewCtx *args)
{
    return EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = args->unit) || (args->unit == '\'') ||
           (args->unit == 0x2019u);
}

// the selection, or the word at the cursor, in capitals (`mode` 1), small letters (0) or with each word's first letter
// a capital and the rest small (2)
EMBED_INLINE int view_case_change(const ViewCtx *args)
{
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    if (!EMBED_CALL(view_select, ViewCtx, .view_read = args->view))
    {
        unsigned long long start = 0ULL;
        unsigned long long end = 0ULL;
        if (!EMBED_CALL(view_word_span, ViewCtx, .view = args->view, .line = args->view->cursor.line,
                        .col = args->view->cursor.col, .from_u64s = &start, .to_u64s = &end))
        {
            return 1;
        }
        from = (CliPosition){args->view->cursor.line, start};
        to = (CliPosition){args->view->cursor.line, end};
    }
    CliViewEdit *const made = EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = from, .to = to,
                                         .append_bytes = 0ULL, .append_spans = 1ULL, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    VerbaLinea cased = {made->put, made->consumed_length + 1ULL, 0ULL};
    int in_word = 0;
    for (unsigned long long at = 0ULL; at < made->consumed_length;)
    {
        unsigned long long bytes = 0ULL;
        const unsigned int unit = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = made->consumed,
                                             .size = made->consumed_length, .at = at, .length = &bytes);
        at += bytes;
        if (args->mode == 2u)
        {
            const int begins = !in_word && EMBED_CALL(view_unit_letter, ViewCtx, .unit = unit);
            in_word = begins || (in_word && EMBED_CALL(view_unit_word_on, ViewCtx, .unit = unit));
            const unsigned int inside_unit =
                in_word ? EMBED_CALL(view_unit_case, ViewCtx, .unit = unit, .upper = 0) : unit;
            EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &cased,
                       .letter = begins ? EMBED_CALL(view_unit_case, ViewCtx, .unit = unit, .upper = 1) : inside_unit);
        }
        else
        {
            EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &cased,
                       .letter = EMBED_CALL(view_unit_case, ViewCtx, .unit = unit, .upper = args->mode == 1u));
        }
    }
    made->append_length = cased.at;
    const CliPosition cursor = args->view->cursor;
    const CliPosition anchor = args->view->anchor;
    const CliPosition end =
        EMBED_CALL(view_position_end, ViewCtx, .from = from, .text = made->put, .bytes = made->append_length);
    if (!EMBED_CALL(view_edit_cell_write, ViewCtx, .view = args->view, .made = made, .kind = CLI_VIEW_KIND_NONE,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(view_settle, ViewCtx, .view = args->view,
               .cursor = EMBED_CALL(view_position_machine_transfer, ViewCtx, .place = cursor, .from = from, .to = to,
                                    .end = end, .after = 1),
               .anchor = EMBED_CALL(view_position_machine_transfer, ViewCtx, .place = anchor, .from = from, .to = to,
                                    .end = end, .after = 0));
    return 1;
}

// the cursor taken past the bracket the one at it pairs with, where one stands there
EMBED_INLINE void view_bracket_jump(const ViewCtx *args)
{
    CliPosition pair[2];
    if (EMBED_CALL(view_pair, ViewCtx, .view_read = args->view, .pair = pair))
    {
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = (CliPosition){pair[1].line, pair[1].col + 1ULL},
                   .extend = 0, .saved = 0);
    }
}

// the span Expand Selection takes: the tree hands the least first, and that one is saved
typedef struct
{
    int found;
    CliSymbolPosition from;
    CliSymbolPosition to;
} CliViewSpan;

static void view_span_save(const CliSpannedCfg *args)
{
    CliSymbolPosition from = args->from;
    CliSymbolPosition to = args->to;
    CliViewSpan *const span = (CliViewSpan *)args->context;
    if (!span->found)
    {
        span->found = 1;
        span->from = from;
        span->to = to;
    }
}

// whether the selection is the one Expand Selection last made, with a step to take back
EMBED_INLINE int view_still_grow(const ViewCtx *args)
{
    return (args->view_read->grown_count != 0u) &&
           (args->view_read->cursor.line == args->view_read->grown_cursor.line) &&
           (args->view_read->cursor.col == args->view_read->grown_cursor.col) &&
           (args->view_read->anchor.line == args->view_read->grown_anchor.line) &&
           (args->view_read->anchor.col == args->view_read->grown_anchor.col);
}

// the selection grown to the least span the tree reads off the text that holds it and is more than it, the selection it
// grew from saved for Shrink Selection; a paged text is not in memory whole and has no spans to grow through
static int view_selection_grow(const ViewCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long size = 0ULL;
    char *const text = args->view->paged
                           ? NULL
                           : EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = args->view->key,
                                        .size_pointer = &size, .owner = g_cli_views_owner);
    if (text == NULL)
    {
        return 1;
    }
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    CliViewSpan span;
    span.found = 0;
    // a place in a text a view shows stands below 2^32 lines and columns
    const int read = EMBED_CALL(
        cli_symbols_path_text_size_context_err_msg_from_symbol_position.spans,
        CliSymbolsPathTextSizeContextErrMsgFromSymbolPositionCfg, .path = args->view->path, .text = text, .size = size,
        .from_symbol_position = (CliSymbolPosition){(unsigned int)from.line, (unsigned int)from.col},
        .to_symbol_position = (CliSymbolPosition){(unsigned int)to.line, (unsigned int)to.col},
        .each_spanned = view_span_save, .context = &span, .err_msg = err_msg);
    g_cli_guard->persistent_buf_release(text, g_cli_views_owner);
    if (!read)
    {
        return 0;
    }
    if (span.found)
    {
        args->view->grown_count =
            EMBED_CALL(view_still_grow, ViewCtx, .view_read = args->view) ? args->view->grown_count : 0u;
        if (args->view->grown_count == CLI_VIEW_GROWN)
        {
            for (unsigned int at = 1u; at < CLI_VIEW_GROWN; at += 1u)
            {
                args->view->grown[at - 1u][0] = args->view->grown[at][0];
                args->view->grown[at - 1u][1] = args->view->grown[at][1];
            }
            args->view->grown_count -= 1u;
        }
        args->view->grown[args->view->grown_count][0] = args->view->cursor;
        args->view->grown[args->view->grown_count][1] = args->view->anchor;
        args->view->grown_count += 1u;
        args->view->anchor = (CliPosition){span.from.line, span.from.col};
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = (CliPosition){span.to.line, span.to.col},
                   .extend = 1, .saved = 0);
        args->view->grown_cursor = args->view->cursor;
        args->view->grown_anchor = args->view->anchor;
    }
    return 1;
}

// the last step Expand Selection took taken back, where the selection is still the one it made
EMBED_INLINE void view_selection_shrunk(const ViewCtx *args)
{
    if (EMBED_CALL(view_still_grow, ViewCtx, .view_read = args->view))
    {
        args->view->grown_count -= 1u;
        args->view->anchor = args->view->grown[args->view->grown_count][1];
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = args->view->grown[args->view->grown_count][0],
                   .extend = 1, .saved = 0);
        args->view->grown_cursor = args->view->cursor;
        args->view->grown_anchor = args->view->anchor;
    }
}

// `name` run where it names a line edit of the menus: 1 with whether it was taken into `reserved`, or 0 where it names
// none
static int view_command_run(const ViewCtx *args)
{
    int *reserved = args->reserved;
    if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "comment"))
    {
        if (args->view->comment_line[0] != '\0')
        {
            *reserved = view_lines_comment(args);
        }
        else if (args->view->comment_open[0] != '\0')
        {
            *reserved = view_block_comment(args);
        }
        else
        {
            *reserved = 1;
        }
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "select-line"))
    {
        view_line_select(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "join-lines"))
    {
        *reserved = view_lines_join(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "sort-ascending") ||
             EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "sort-descending"))
    {
        *reserved = EMBED_CALL(view_lines_sort, ViewCtx, .view = args->view,
                               .descending =
                                   EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "sort-descending"),
                               .unique = 0, .err_msg = args->err_msg);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "unique-lines"))
    {
        *reserved = EMBED_CALL(view_lines_sort, ViewCtx, .view = args->view, .descending = 0, .unique = 1,
                               .err_msg = args->err_msg);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "upper-case") ||
             EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "lower-case") ||
             EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "title-case"))
    {
        const unsigned int title_mode =
            EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "title-case") ? 2u : 0u;
        *reserved = EMBED_CALL(
            view_case_change, ViewCtx, .view = args->view,
            .mode = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "upper-case") ? 1u : title_mode,
            .err_msg = args->err_msg);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "copy-line-up") ||
             EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "copy-line-down"))
    {
        *reserved = EMBED_CALL(view_lines_copy, ViewCtx, .view = args->view,
                               .up = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "copy-line-up"),
                               .err_msg = args->err_msg);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "move-line-up") ||
             EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "move-line-down"))
    {
        *reserved = EMBED_CALL(view_lines_move, ViewCtx, .view = args->view,
                               .up = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "move-line-up"),
                               .err_msg = args->err_msg);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "bracket"))
    {
        view_bracket_jump(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "expand-selection"))
    {
        *reserved = view_selection_grow(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "shrink-selection"))
    {
        view_selection_shrunk(args);
    }
    else
    {
        return 0;
    }
    return 1;
}

// a key's name taken where it runs a line edit, by the keys the page's editor binds them to: 1 with whether it was
// taken into `reserved`, or 0 where the key is none of them
// an Up or Down arrow held with Mod or Alt taken: with Mod alone, the view scrolled a line, the cursor staying; with
// Alt alone, the selection's lines moved a line, or with Shift copied; with both, nothing, as they would add a cursor,
// which the view has not
EMBED_INLINE void view_vertical_key(const ViewCtx *args)
{
    CliView *const view = args->view;
    const int up = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Up");
    if (args->mod && !args->alt && !args->shift)
    {
        view->top = up ? EMBED_CALL(view_up, ViewCtx, .view_read = view, .line = view->top, .count = 1ULL)
                       : EMBED_CALL(view_down, ViewCtx, .view_read = view, .line = view->top, .count = 1ULL);
    }
    else if (args->alt && !args->mod)
    {
        *args->reserved = args->shift
                              ? EMBED_CALL(view_lines_copy, ViewCtx, .view = view, .up = up, .err_msg = args->err_msg)
                              : EMBED_CALL(view_lines_move, ViewCtx, .view = view, .up = up, .err_msg = args->err_msg);
    }
}

// a key held with Mod and not Alt that selects, joins, comments, deletes, indents, jumps or inserts lines, or deletes a
// word, taken: 1, or 0 where it is none of them
EMBED_INLINE int view_mod_line_key(const ViewCtx *args);

EMBED_INLINE int view_line_key(const ViewCtx *args)
{
    int *reserved = args->reserved;
    const int vertical = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Up") ||
                         EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Down");
    if (vertical && (args->mod || args->alt))
    {
        view_vertical_key(args);
        return 1;
    }
    if (args->alt && args->shift && !args->mod &&
        EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Right"))
    {
        *reserved = view_selection_grow(args);
        return 1;
    }
    if (args->alt && args->shift && !args->mod &&
        EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Left"))
    {
        view_selection_shrunk(args);
        return 1;
    }
    if (args->mod && !args->alt && view_mod_line_key(args))
    {
        return 1;
    }
    if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Tab"))
    {
        CliPosition from;
        CliPosition to;
        EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
        if (args->shift || (from.line != to.line))
        {
            *reserved = EMBED_CALL(view_lines_indent, ViewCtx, .view = args->view, .deeper = !args->shift,
                                   .err_msg = args->err_msg);
            return 1;
        }
    }
    return 0;
}

EMBED_INLINE int view_mod_line_key(const ViewCtx *args)
{
    int *reserved = args->reserved;
    if (!args->shift && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "L"))
    {
        view_line_select(args);
        return 1;
    }
    if (!args->shift && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "J"))
    {
        *reserved = view_lines_join(args);
        return 1;
    }
    if (!args->shift && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "/"))
    {
        return EMBED_CALL(view_command_run, ViewCtx, .view = args->view, .name = "comment", .reserved = reserved,
                          .err_msg = args->err_msg);
    }
    if (args->shift && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "K"))
    {
        *reserved = view_lines_delete(args);
        return 1;
    }
    if (!args->shift && (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "]") ||
                         EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "[")))
    {
        *reserved = EMBED_CALL(view_lines_indent, ViewCtx, .view = args->view,
                               .deeper = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "]"),
                               .err_msg = args->err_msg);
        return 1;
    }
    if (args->shift && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "\\"))
    {
        view_bracket_jump(args);
        return 1;
    }
    if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Enter"))
    {
        *reserved =
            EMBED_CALL(view_line_insert, ViewCtx, .view = args->view, .below = !args->shift, .err_msg = args->err_msg);
        return 1;
    }
    if (!EMBED_CALL(view_select, ViewCtx, .view_read = args->view) &&
        EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Backspace") &&
        (args->view->cursor.col != 0ULL))
    {
        *reserved =
            EMBED_CALL(view_edit, ViewCtx, .view = args->view,
                       .from = EMBED_CALL(view_word_leave, ViewCtx, .view = args->view, .place = args->view->cursor),
                       .to = args->view->cursor, .text = "", .length = 0ULL, .kind = CLI_VIEW_KIND_DELETE,
                       .err_msg = args->err_msg);
        return 1;
    }
    if (!EMBED_CALL(view_select, ViewCtx, .view_read = args->view) &&
        EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .required = "Delete") &&
        (args->view->cursor.col <
         EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = args->view->cursor.line)))
    {
        *reserved =
            EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = args->view->cursor,
                       .to = EMBED_CALL(view_word_right, ViewCtx, .view = args->view, .place = args->view->cursor),
                       .text = "", .length = 0ULL, .kind = CLI_VIEW_KIND_DELETE_FORWARD, .err_msg = args->err_msg);
        return 1;
    }
    return 0;
}

// a key's name parsed: the modifier keys down into `mod`, `shift` and `alt`, and the key's own name
EMBED_INLINE const char *view_key_read(const ViewCtx *args)
{
    const char *name = args->name;
    int *mod = args->mod_pointer;
    int *shift = args->shift_pointer;
    int *alt = args->alt_pointer;
    *mod = 0;
    *shift = 0;
    *alt = 0;
    for (;;)
    {
        if (memor.cmp(&(MemoriaCfg){.src = name, .other = "Mod+", .bytes = 4u}) == 0)
        {
            *mod = 1;
            name += 4;
        }
        else if (memor.cmp(&(MemoriaCfg){.src = name, .other = "Shift+", .bytes = 6u}) == 0)
        {
            *shift = 1;
            name += 6;
        }
        else if (memor.cmp(&(MemoriaCfg){.src = name, .other = "Alt+", .bytes = 4u}) == 0)
        {
            *alt = 1;
            name += 4;
        }
        else if (memor.cmp(&(MemoriaCfg){.src = name, .other = "Ctrl+", .bytes = 5u}) == 0)
        {
            *mod = 1;
            name += 5;
        }
        else
        {
            return name;
        }
    }
}

// the cursor moved a unit, or with `mod` a word, left, or where `forward` right, or a selection collapsed to its end
// that way where `shift` is not held; the selection extended where it is
EMBED_INLINE void view_key_across(const ViewCtx *args)
{
    CliView *const view = args->view;
    CliPosition from;
    CliPosition to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = view, .from_pointer = &from, .to_pointer = &to);
    CliPosition place;
    if (args->mod)
    {
        place = args->forward ? EMBED_CALL(view_word_right, ViewCtx, .view = view, .place = view->cursor)
                              : EMBED_CALL(view_word_leave, ViewCtx, .view = view, .place = view->cursor);
    }
    else if (!args->shift && EMBED_CALL(view_select, ViewCtx, .view_read = view))
    {
        place = args->forward ? to : from;
    }
    else
    {
        place = args->forward ? EMBED_CALL(view_right, ViewCtx, .view = view, .place = view->cursor)
                              : EMBED_CALL(view_leave, ViewCtx, .view = view, .place = view->cursor);
    }
    EMBED_CALL(view_move, ViewCtx, .view = view, .place = place, .extend = args->shift, .saved = 0);
}

// the cursor moved to the line's first letter, or its start where it stands there, or with `mod` to the text's start
EMBED_INLINE void view_key_home(const ViewCtx *args)
{
    CliView *const view = args->view;
    CliPosition place = {0ULL, 0ULL};
    if (!args->mod)
    {
        const unsigned long long first =
            EMBED_CALL(view_first_letter, ViewCtx, .view = view, .line = view->cursor.line);
        place = (CliPosition){view->cursor.line, (view->cursor.col == first) ? 0ULL : first};
    }
    EMBED_CALL(view_move, ViewCtx, .view = view, .place = place, .extend = args->shift, .saved = 0);
}

// the cursor moved to its line's end, or with `mod` to the text's end
EMBED_INLINE void view_key_end(const ViewCtx *args)
{
    CliView *const view = args->view;
    const unsigned long long lines = EMBED_CALL(view_lines, ViewCtx, .view_read = view);
    const unsigned long long last = (lines != 0ULL) ? lines - 1ULL : 0ULL;
    const unsigned long long line =
        args->mod ? EMBED_CALL(view_displayed_up, ViewCtx, .view_read = view, .line = last) : view->cursor.line;
    const CliPosition place = {line, EMBED_CALL(view_length, ViewCtx, .view = view, .line = line)};
    EMBED_CALL(view_move, ViewCtx, .view = view, .place = place, .extend = args->shift, .saved = 0);
}

// the cursor and the top moved a page up, or where `forward` down. A page is the rows less one: the last row seen
// stays in view
EMBED_INLINE void view_key_page(const ViewCtx *args)
{
    CliView *const view = args->view;
    const unsigned int rows = (view->rows > 1u) ? view->rows : 1u;
    const unsigned long long by = rows - 1u;
    EMBED_CALL(view_vertical, ViewCtx, .view = view, .by = args->forward ? (long long)by : -(long long)by,
               .extend = args->shift);
    view->top = args->forward ? EMBED_CALL(view_down, ViewCtx, .view_read = view, .line = view->top, .count = by)
                              : EMBED_CALL(view_up, ViewCtx, .view_read = view, .line = view->top, .count = by);
    EMBED_CALL(view_follow, ViewCtx, .view = view);
}

// a key that moves the cursor taken: an arrow, Home, End or a page key; 1, or 0 where the key is none of them
EMBED_INLINE int view_move_key(const ViewCtx *args)
{
    const char *const name = args->name;
    const int left = EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "Left");
    const int up = EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "Up");
    const int page_up = EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "PageUp");
    if (left || EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "Right"))
    {
        EMBED_CALL(view_key_across, ViewCtx, .view = args->view, .mod = args->mod, .shift = args->shift,
                   .forward = !left);
    }
    else if (up || EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "Down"))
    {
        EMBED_CALL(view_vertical, ViewCtx, .view = args->view, .by = up ? -1LL : 1LL, .extend = args->shift);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "Home"))
    {
        EMBED_CALL(view_key_home, ViewCtx, .view = args->view, .mod = args->mod, .shift = args->shift);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "End"))
    {
        EMBED_CALL(view_key_end, ViewCtx, .view = args->view, .mod = args->mod, .shift = args->shift);
    }
    else if (page_up || EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "PageDown"))
    {
        EMBED_CALL(view_key_page, ViewCtx, .view = args->view, .shift = args->shift, .forward = !page_up);
    }
    else
    {
        return 0;
    }
    return 1;
}

// a key taken: 1, or 0 with why given where an edit it makes fails; a key the view has no use for is taken and does
// nothing
EMBED_INLINE int view_on_key(const ViewCtx *args)
{
    // Vim's keys are asked first, while find's bar does not have the keys
    if (!(args->view->find_open && args->view->find_focus) &&
        EMBED_CALL(cli_vim_view.key, CliVimCfg, .key = args->view->key, .name = args->supplied,
                   .err_msg = args->err_msg))
    {
        return 1;
    }
    int mod = 0;
    int shift = 0;
    int alt = 0;
    const char *const name = EMBED_CALL(view_key_read, ViewCtx, .name = args->supplied, .mod_pointer = &mod,
                                        .shift_pointer = &shift, .alt_pointer = &alt);
    const unsigned long long lines = EMBED_CALL(view_lines, ViewCtx, .view_read = args->view);
    const unsigned long long last = (lines != 0ULL) ? lines - 1ULL : 0ULL;
    if (EMBED_CALL(view_find_key, ViewCtx, .view = args->view, .name = name, .mod = mod, .shift = shift, .alt = alt))
    {
        return 1;
    }
    int reserved = 1;
    if (EMBED_CALL(view_command_run, ViewCtx, .view = args->view, .name = name, .reserved = &reserved,
                   .err_msg = args->err_msg) ||
        EMBED_CALL(view_line_key, ViewCtx, .view = args->view, .name = name, .mod = mod, .shift = shift, .alt = alt,
                   .reserved = &reserved, .err_msg = args->err_msg))
    {
        return reserved;
    }
    if (EMBED_CALL(view_move_key, ViewCtx, .view = args->view, .name = name, .mod = mod, .shift = shift))
    {
        return 1;
    }
    if (mod && shift && EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "["))
    {
        (void)EMBED_CALL(view_fold, ViewCtx, .view = args->view, .line = args->view->cursor.line, .exact = 0);
    }
    else if (mod && shift && EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "]"))
    {
        (void)EMBED_CALL(view_unfolded, ViewCtx, .view = args->view, .line = args->view->cursor.line);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "Backspace"))
    {
        return view_backspace(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "Delete"))
    {
        return view_delete(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "Enter"))
    {
        return view_newline(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "Tab"))
    {
        return view_on_tab(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "Escape"))
    {
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = args->view->cursor, .extend = 0, .saved = 0);
    }
    else if (mod && EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "A"))
    {
        args->view->anchor = (CliPosition){0ULL, 0ULL};
        EMBED_CALL(view_move, ViewCtx, .view = args->view,
                   .place = (CliPosition){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)},
                   .extend = 1, .saved = 0);
    }
    else if (mod && EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "Z"))
    {
        return shift ? view_redone(args) : view_undone(args);
    }
    else if (mod && EMBED_CALL(view_by_name, ViewCtx, .name = name, .required = "Y"))
    {
        return view_redone(args);
    }
    return 1;
}

// the pointer taken: a press places the cursor at its cell, a second press takes the word and a third the line, a drag
// extends the selection; a press in the gutter opens the fold closed on its row's line, or closes the one the text
// folds there
EMBED_INLINE void view_point(const ViewCtx *args)
{
    const unsigned long long line =
        EMBED_CALL(view_down, ViewCtx, .view_read = args->view, .line = args->view->top, .count = args->input->row);
    const CliPosition place = {
        line, EMBED_CALL(view_col_at, ViewCtx, .view = args->view, .line = line, .cell = args->input->column)};
    if (args->input->pointer == CLI_VIEW_POINTER_UP)
    {
        return;
    }
    if (args->input->gutter)
    {
        if ((args->input->pointer == CLI_VIEW_POINTER_DOWN) &&
            !EMBED_CALL(view_unfolded, ViewCtx, .view = args->view, .line = line))
        {
            (void)EMBED_CALL(view_fold, ViewCtx, .view = args->view, .line = line, .exact = 1);
        }
        return;
    }
    if (args->input->pointer == CLI_VIEW_POINTER_MOVED)
    {
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = place, .extend = 1, .saved = 0);
        return;
    }
    if (args->input->clicks >= 3u)
    {
        const unsigned long long below =
            EMBED_CALL(view_down, ViewCtx, .view_read = args->view, .line = line, .count = 1ULL);
        args->view->anchor = (CliPosition){line, 0ULL};
        EMBED_CALL(view_move, ViewCtx, .view = args->view,
                   .place = (below > line) ? (CliPosition){below, 0ULL}
                                           : (CliPosition){line, EMBED_CALL(view_length, ViewCtx, .view = args->view,
                                                                            .line = line)},
                   .extend = 1, .saved = 0);
        return;
    }
    if (args->input->clicks == 2u)
    {
        const CliPosition start =
            EMBED_CALL(view_word_leave, ViewCtx, .view = args->view,
                       .place = EMBED_CALL(view_word_right, ViewCtx, .view = args->view, .place = place));
        args->view->anchor = (start.line == line) ? start : place;
        EMBED_CALL(view_move, ViewCtx, .view = args->view,
                   .place = EMBED_CALL(view_word_right, ViewCtx, .view = args->view, .place = args->view->anchor),
                   .extend = 1, .saved = 0);
        return;
    }
    EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = place, .extend = args->input->shift, .saved = 0);
}

// text the input gives taken: typed into find's query where find has the focus, a byte a unit at the most, else into
// the text at the cursor, one letter as a letter typed and a space or more as an edit of its own kind: 1, or 0 with
// why given where the edit fails
EMBED_INLINE int view_input_text(const ViewCtx *args)
{
    CliView *const view = args->view;
    const CliViewInput *const input = args->input;
    // Vim's keys are asked first, while find's bar does not have the keys
    if (!(view->find_open && view->find_focus) &&
        EMBED_CALL(cli_vim_view.text, CliVimCfg, .key = view->key, .text = input->text, .length = input->length,
                   .err_msg = args->err_msg))
    {
        return 1;
    }
    const int one = (input->length != 0ULL) && (EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg,
                                                           .text = input->text, .size = input->length) == 1ULL);
    const int blank =
        (input->length != 0ULL) && ((input->text[0] == ' ') || (input->text[0] == '\t') || (input->text[0] == '\n'));
    unsigned int kind = CLI_VIEW_KIND_NONE;
    if (one)
    {
        kind = blank ? CLI_VIEW_KIND_SPACE : CLI_VIEW_KIND_TYPE;
    }
    unsigned long long consumed = 0ULL;
    const unsigned int letter = one ? EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = input->text,
                                                 .size = input->length, .at = 0ULL, .length = &consumed)
                                    : 0u;
    if (view->find_open && view->find_focus)
    {
        unsigned short *const units =
            g_cli_guard->temporary_buf_alloc((input->length + 1ULL) * sizeof(*units), g_cli_views_owner);
        if (units != NULL)
        {
            EMBED_CALL(view_query_append, ViewCtx, .view = view, .units_read = units,
                       .count = EMBED_CALL(view_units_get, ViewCtx, .text = input->text, .size = input->length,
                                           .units = units, .capacity = input->length + 1ULL));
            g_cli_guard->persistent_buf_release(units, g_cli_views_owner);
        }
        return 1;
    }
    if (one && !blank && (letter < 0x10000u))
    {
        return EMBED_CALL(view_letter_type, ViewCtx, .view = view, .text = input->text, .length = input->length,
                          .letter = letter, .kind = kind, .err_msg = args->err_msg);
    }
    return EMBED_CALL(view_type, ViewCtx, .view = view, .text = input->text, .length = input->length, .kind = kind,
                      .err_msg = args->err_msg);
}

// the view's top put at the line `line`, no further down than the text's last line; a line a closed fold hides goes
// past the fold the way the view scrolls
EMBED_INLINE void view_top_put(const ViewCtx *args)
{
    CliView *const view = args->view;
    const unsigned long long lines = EMBED_CALL(view_lines, ViewCtx, .view_read = view);
    const unsigned long long last = (lines != 0ULL) ? lines - 1ULL : 0ULL;
    const unsigned long long top = (args->line > last) ? last : args->line;
    view->top = (top > view->top) ? EMBED_CALL(view_displayed_down, ViewCtx, .view_read = view, .line = top)
                                  : EMBED_CALL(view_displayed_up, ViewCtx, .view_read = view, .line = top);
}

int cli_view_input(const CliViewInputCfg *args)
{
    const char *key = args->key;
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = key);
    if (view == NULL)
    {
        mmgr_line_put(args->err_msg, "no view is open under the key");
        return 0;
    }
    int reserved = 1;
    EMBED_CALL(view_folds_refresh, ViewCtx, .view = view);
    switch (args->input->kind)
    {
    case CLI_VIEW_KEY: {
        reserved =
            EMBED_CALL(view_on_key, ViewCtx, .view = view, .supplied = args->input->name, .err_msg = args->err_msg);
        break;
    }
    case CLI_VIEW_TEXT: {
        reserved = EMBED_CALL(view_input_text, ViewCtx, .view = view, .input = args->input, .err_msg = args->err_msg);
        break;
    }
    case CLI_VIEW_POINTER: {
        EMBED_CALL(view_point, ViewCtx, .view = view, .input = args->input);
        break;
    }
    case CLI_VIEW_SCROLL: {
        EMBED_CALL(view_top_put, ViewCtx, .view = view, .line = args->input->top);
        break;
    }
    case CLI_VIEW_WHEEL: {
        // a wheel's rows are far inside what a long long represents; up past the first line the top stands at it, and
        // down past the line it is bounded at it stands at that
        const long long rows = args->input->wheel_rows;
        const unsigned long long up = (rows < 0LL) ? (unsigned long long)(-rows) : 0ULL;
        const unsigned long long down = (rows > 0LL) ? (unsigned long long)rows : 0ULL;
        const unsigned long long line = (view->top > up) ? (view->top - up + down) : down;
        EMBED_CALL(view_top_put, ViewCtx, .view = view, .line = (line < args->input->top) ? line : args->input->top);
        break;
    }
    case CLI_VIEW_SIZE: {
        // a size the same as the view's changes nothing; one changed moves the top so the cursor stays in view
        const int changed = (view->rows != args->input->rows) || (view->columns != args->input->columns);
        view->rows = args->input->rows;
        view->columns = args->input->columns;
        if (changed)
        {
            EMBED_CALL(view_follow, ViewCtx, .view = view);
        }
        break;
    }
    case CLI_VIEW_COMPLETE: {
        // the completion's text written over the units before the cursor it replaces, no more than stand before it
        const unsigned long long back =
            (args->input->column < view->cursor.col) ? args->input->column : view->cursor.col;
        const CliPosition from = {view->cursor.line, view->cursor.col - back};
        reserved =
            EMBED_CALL(view_edit, ViewCtx, .view = view, .from = from, .to = view->cursor, .text = args->input->text,
                       .length = args->input->length, .kind = CLI_VIEW_KIND_NONE, .err_msg = args->err_msg);
        break;
    }
    case CLI_VIEW_POSITION: {
        // the cursor placed at the line `row` and the unit `column`, each cut to the text, the top following it
        const unsigned long long lines = EMBED_CALL(view_lines, ViewCtx, .view_read = view);
        const unsigned long long last = (lines > 0ULL) ? lines - 1ULL : 0ULL;
        const unsigned long long line = (args->input->row < lines) ? args->input->row : last;
        const unsigned long long length = EMBED_CALL(view_length, ViewCtx, .view = view, .line = line);
        EMBED_CALL(view_move, ViewCtx, .view = view,
                   .place = (CliPosition){line, (args->input->column < length) ? args->input->column : length},
                   .extend = 0, .saved = 0);
        break;
    }
    default: {
        break;
    }
    }
    // a paged text's pages are told where the view stands and cache the lines about it
    if (view->paged)
    {
        const unsigned long long rows = (view->rows != 0u) ? view->rows : 1ULL;
        const CliPagesView positioned_view = {view->top,        rows, view->columns, 0ULL, 0ULL, view->top,
                                              view->top + rows, 0ULL, 0ULL};
        CliPagesPositioned positioned;
        (void)EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = view->key, .view = &positioned_view,
                         .positioned = &positioned);
    }
    return reserved;
}

// the bracket pair at the cursor, into `pair`, the bracket that ends at the cursor, else the one that starts at it,
// first and the one it pairs with second: the text's lines from as far above and below the cursor as
// CLI_VIEW_PAIR_LINES reach and none outside those read for it, read as the tree reads a text from the state the first
// is entered in: 1, or 0 where no bracket of a pair stands at the cursor or the lines could not be read
EMBED_INLINE int view_pair(const ViewCtx *args)
{
    CliPosition *pair = args->pair;
    unsigned long long low = 0ULL;
    unsigned long long high = 0ULL;
    (void)EMBED_CALL(cli_document_key_low.bounds, CliDocumentKeyLowCfg, .key = args->view_read->key, .low = &low,
                     .high = &high);
    const unsigned long long line = args->view_read->cursor.line;
    const unsigned long long reach = (line > CLI_VIEW_PAIR_LINES) ? (line - CLI_VIEW_PAIR_LINES) : 0ULL;
    const unsigned long long start = (reach > low) ? reach : low;
    const unsigned long long end =
        ((high != 0ULL) && ((line + CLI_VIEW_PAIR_LINES) >= high)) ? (high - 1ULL) : (line + CLI_VIEW_PAIR_LINES);
    const unsigned long long first = (start < line) ? start : line;
    const unsigned long long last = (end > line) ? end : line;
    char state[CLI_VIEW_STATE];
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.lines_alloc, CliDocumentKeyCfg, .key = args->view_read->key,
                                  .first = first, .last = last, .size_pointer = &size, .state = state,
                                  .capacity = sizeof(state), .owner = g_cli_views_owner);
    if (text == NULL)
    {
        return 0;
    }
    CliSymbolPosition found[2] = {{0u, 0u}, {0u, 0u}};
    // a place in a text a view shows stands below 2^32 lines and columns
    const int paired =
        EMBED_CALL(cli_symbols_path_text_size_state.pair, CliSymbolsPathTextSizeStateCfg, .path = args->view_read->path,
                   .state = (state[0] != '\0') ? state : "root", .text = text, .size = size,
                   .at_symbol_position =
                       (CliSymbolPosition){(unsigned int)(line - first), (unsigned int)args->view_read->cursor.col},
                   .pair = found);
    g_cli_guard->persistent_buf_release(text, g_cli_views_owner);
    for (unsigned int at = 0u; paired && (at < 2u); at += 1u)
    {
        pair[at].line = first + found[at].line;
        pair[at].col = found[at].col;
    }
    return paired;
}

// the place among the frame's rows `displayed` of the first row at or past the line `line`, found by halving
EMBED_INLINE unsigned long long view_rows_find(const ViewCtx *args)
{
    unsigned long long low = 0ULL;
    unsigned long long high = args->displayed->count;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ULL);
        if (args->displayed->number[middle] < args->line)
        {
            low = middle + 1ULL;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// a fold of the text stored among those seen where it opens on one of the frame's rows; the texts' lock is taken
// here, and no line is read under it
static void view_fold_visit(const CliFoldEachCfg *args)
{
    CliViewFoldsSeen *const folds = args->context;
    const unsigned long long low =
        EMBED_CALL(view_rows_find, ViewCtx, .displayed = folds->displayed, .line = args->first);
    if ((low < folds->displayed->count) && (folds->displayed->number[low] == args->first) &&
        (folds->count < folds->displayed->count))
    {
        folds->seen[folds->count] = (CliViewFoldSeen){args->first, args->last, args->displayed};
        folds->count += 1ULL;
    }
}

void cli_view_frame_release(CliViewFrame *frame)
{
    if (frame != NULL)
    {
        g_cli_guard->persistent_buf_release(frame->runs, g_cli_views_owner);
        g_cli_guard->persistent_buf_release(frame, g_cli_views_owner);
    }
}

// the frame's rows from the line `line` filled into `rows_out`: each line shown from it, a closed fold's lines passed
// over, `count` rows at the most and none past the text's `most` lines
EMBED_INLINE void view_rows_fill(const ViewCtx *args)
{
    CliViewRows *const displayed = args->rows_out;
    for (unsigned long long line = args->line; (displayed->count < args->count) && (line < args->most);)
    {
        displayed->number[displayed->count] = line;
        displayed->count += 1ULL;
        const unsigned long long next =
            EMBED_CALL(view_down, ViewCtx, .view_read = args->view_read, .line = line, .count = 1ULL);
        if (next <= line)
        {
            break;
        }
        line = next;
    }
}

// the rows' texts of a paged text put in the frame after the bytes `out` holds, each cut at `width` columns, the lines
// read a run of consecutive rows at a time
EMBED_INLINE void view_frame_paged(const ViewCtx *args)
{
    const CliViewRows *const displayed = args->displayed;
    CliViewFrame *const frame = args->frame;
    VerbaLinea *const bytes = args->out;
    for (unsigned long long at = 0ULL; at < displayed->count;)
    {
        unsigned long long end = at;
        while ((end + 1ULL < displayed->count) && (displayed->number[end + 1ULL] == displayed->number[end] + 1ULL))
        {
            end += 1ULL;
        }
        unsigned long long count = 0ULL;
        char *const cell = EMBED_CALL(cli_pages_from.lines_alloc, CliPagesFromCfg, .key = args->key,
                                      .from = displayed->number[at], .to = displayed->number[end] + 1ULL,
                                      .line = displayed->text + at, .count = &count, .owner = g_cli_views_owner);
        for (unsigned long long each = at; each <= end; each += 1ULL)
        {
            const char *const row_text = ((cell != NULL) && ((each - at) < count)) ? displayed->text[each] : "";
            const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = row_text, .cap = CLI_POOL_BYTES});
            // the columns a view shows stand below 2^32
            const unsigned long long cut = EMBED_CALL(cli_edits_line.text_byte_get, CliEditsLineCfg, .line = row_text,
                                                      .size = whole, .col = (unsigned int)args->width);
            frame->row[each] = (CliViewFrameRow){displayed->number[each], bytes->at, cut};
            memor.cpy(&(MemoriaCfg){.dst = frame->bytes + bytes->at, .src = row_text, .bytes = cut});
            bytes->at += cut;
        }
        g_cli_guard->persistent_buf_release(cell, g_cli_views_owner);
        at = end + 1ULL;
    }
}

// the rows' texts of a text the store holds put in the frame after the bytes `out` holds, each cut at `width` units
EMBED_INLINE void view_frame_stored(const ViewCtx *args)
{
    const CliViewRows *const displayed = args->displayed;
    CliViewFrame *const frame = args->frame;
    VerbaLinea *const bytes = args->out;
    const CliStoredText *const one = EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->key);
    for (unsigned long long at = 0ULL; at < displayed->count; at += 1ULL)
    {
        const unsigned long long line = displayed->number[at];
        const unsigned long long whole = ((one != NULL) && (line < one->lines.lines)) ? one->lines.length[line] : 0ULL;
        const unsigned long long units = (whole < args->width) ? whole : args->width;
        frame->row[at] = (CliViewFrameRow){line, bytes->at, 0ULL};
        if (units != 0ULL)
        {
            const unsigned long long began = bytes->at;
            EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = bytes,
                       .units = one->lines.unit + one->lines.start[line], .count = units);
            frame->row[at].length = bytes->at - began;
        }
    }
}

// the folds that open on the frame's rows put in it, each with whether the view has it closed, and where it shows the
// line it closes on, that line's text after the bytes `out` holds, its blanks at either end left off and the rest cut
// at `width` units
EMBED_INLINE void view_frame_folds(const ViewCtx *args)
{
    CliView *const view = args->view;
    CliViewFrame *const frame = args->frame;
    VerbaLinea *const bytes = args->out;
    for (unsigned long long at = 0ULL; at < args->folds->count; at += 1ULL)
    {
        const CliViewFoldSeen *const one_fold = &args->folds->seen[at];
        int closed = 0;
        for (unsigned int each = 0u; each < view->folds; each += 1u)
        {
            closed = closed || (view->fold[each].first == one_fold->first);
        }
        frame->fold[at] = (CliViewFrameFold){
            one_fold->first, one_fold->last, one_fold->displayed, closed, bytes->at - frame->closers, 0ULL};
        if (!one_fold->displayed)
        {
            continue;
        }
        const unsigned long long length = EMBED_CALL(view_length, ViewCtx, .view = view, .line = one_fold->last);
        unsigned long long start = 0ULL;
        unsigned long long end = length;
        while ((start < end) && EMBED_CALL(view_blank_unit, ViewCtx, .unit = view->line_units[start]))
        {
            start += 1ULL;
        }
        while ((end > start) && EMBED_CALL(view_blank_unit, ViewCtx, .unit = view->line_units[end - 1ULL]))
        {
            end -= 1ULL;
        }
        const unsigned long long shown = ((end - start) < args->width) ? (end - start) : args->width;
        const unsigned long long began = bytes->at;
        EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = bytes,
                   .units = view->line_units + start, .count = shown);
        frame->fold[at].closer_length = bytes->at - began;
    }
}

// find's state put in the frame: its query as UTF-8, `count` bytes at the most, its choices, and its matches on the
// frame's rows
EMBED_INLINE void view_frame_find(const ViewCtx *args)
{
    const CliView *const view = args->view_read;
    CliViewFrame *const frame = args->frame;
    VerbaLinea query_line = {frame->find.query, args->count, 0ULL};
    EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &query_line,
               .units = view->find_query, .count = view->find_length);
    frame->find.query_length = query_line.at;
    frame->find.focus = view->find_focus;
    frame->find.at = view->find_at;
    frame->find.regex = view->find_regex;
    frame->find.match_case = view->find_case;
    frame->find.word_text = view->find_word;
    frame->find.count = view->matches;
    frame->find.current = view->current;
    for (unsigned long long at = 0ULL; at < view->matches; at += 1ULL)
    {
        const unsigned long long low =
            EMBED_CALL(view_rows_find, ViewCtx, .displayed = args->displayed, .line = view->match[at][0]);
        if ((low < args->displayed->count) && (args->displayed->number[low] == view->match[at][0]))
        {
            frame->match[frame->matches][0] = view->match[at][0];
            frame->match[frame->matches][1] = view->match[at][1];
            frame->match[frame->matches][2] = view->match[at][2];
            frame->matches += 1ULL;
        }
    }
}

CliViewFrame *cli_view_frame_alloc(const char *key)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = key);
    if (view == NULL)
    {
        return NULL;
    }
    view->seq += 1ULL;
    EMBED_CALL(view_folds_refresh, ViewCtx, .view = view);
    const unsigned long long lines = EMBED_CALL(view_lines, ViewCtx, .view_read = view);
    const unsigned long long last_line = (lines != 0ULL) ? lines - 1ULL : 0ULL;
    view->top =
        EMBED_CALL(view_displayed_up, ViewCtx, .view_read = view, .line = (view->top < lines) ? view->top : last_line);
    const unsigned long long from = view->top;
    const unsigned long long rows = (view->rows != 0u) ? view->rows : 1u;
    if (view->find_open)
    {
        EMBED_CALL(view_search, ViewCtx, .view = view);
    }
    // the row numbers and texts the folds and a paged text's lines are read through, and the folds seen, while the
    // frame is laid
    unsigned long long *const number = g_cli_guard->persistent_buf_alloc(
        rows * (sizeof(unsigned long long) + sizeof(const char *) + sizeof(CliViewFoldSeen)), g_cli_views_owner);
    if (number == NULL)
    {
        return NULL;
    }
    const char **const text = (const char **)(void *)(number + rows);
    CliViewFoldSeen *const seen = (CliViewFoldSeen *)(void *)(text + rows);
    CliViewRows displayed = {number, text, 0ULL};
    EMBED_CALL(view_rows_fill, ViewCtx, .view_read = view, .rows_out = &displayed, .line = from, .count = rows,
               .most = lines);
    const unsigned long long to = (displayed.count != 0ULL) ? displayed.number[displayed.count - 1ULL] + 1ULL : from;
    // the folds that open on the rows
    CliViewFoldsSeen folds = {&displayed, seen, 0ULL};
    (void)EMBED_CALL(cli_document_context.folds_each, CliDocumentContextCfg, .key = view->key,
                     .each_fold_each = view_fold_visit, .context = &folds);
    // the frame in one cell: the rows, the folds, the matches among the rows, find's query, and the rows' text and the
    // closers, a row or a closer the view's columns at the most and three bytes a unit
    const unsigned long long columns = (view->columns != 0u) ? view->columns : CLI_VIEW_UNSIZED_COLUMNS;
    const unsigned long long matches_most = view->find_open ? view->matches : 0ULL;
    const unsigned long long query_bytes = view->find_open ? ((3ULL * view->find_length) + 1ULL) : 1ULL;
    const unsigned long long text_bytes = (displayed.count + folds.count) * columns * 3ULL;
    CliViewFrame *const frame = g_cli_guard->persistent_buf_alloc(
        sizeof(*frame) + (displayed.count * sizeof(CliViewFrameRow)) + (folds.count * sizeof(CliViewFrameFold)) +
            (matches_most * sizeof(*frame->match)) + query_bytes + text_bytes,
        g_cli_views_owner);
    if (frame == NULL)
    {
        g_cli_guard->persistent_buf_release(number, g_cli_views_owner);
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = frame, .val = 0, .bytes = sizeof(*frame)});
    frame->row = (CliViewFrameRow *)(void *)(frame + 1);
    frame->fold = (CliViewFrameFold *)(void *)(frame->row + displayed.count);
    frame->match = (unsigned long long (*)[3])(void *)(frame->fold + folds.count);
    frame->find.query = (char *)(void *)(frame->match + matches_most);
    frame->bytes = frame->find.query + query_bytes;
    frame->seq = view->seq;
    frame->top = view->top;
    frame->lines = lines;
    frame->from = (displayed.count != 0ULL) ? displayed.number[0] : from;
    // a count of rows a window shows is far inside what an unsigned int represents
    frame->count = (unsigned int)displayed.count;
    VerbaLinea bytes = {frame->bytes, text_bytes, 0ULL};
    if (view->paged)
    {
        EMBED_CALL(view_frame_paged, ViewCtx, .key = view->key, .displayed = &displayed, .frame = frame, .out = &bytes,
                   .width = columns);
    }
    else
    {
        EMBED_CALL(view_frame_stored, ViewCtx, .key = view->key, .displayed = &displayed, .frame = frame, .out = &bytes,
                   .width = columns);
    }
    // the closers after the rows' text; a count of folds among the rows is far inside what an unsigned int represents
    frame->folds = (unsigned int)folds.count;
    frame->closers = bytes.at;
    EMBED_CALL(view_frame_folds, ViewCtx, .view = view, .folds = &folds, .frame = frame, .out = &bytes,
               .width = columns);
    frame->cursor = view->cursor;
    frame->anchor = view->anchor;
    frame->has_pair = EMBED_CALL(view_pair, ViewCtx, .view_read = view, .pair = frame->pair);
    // a tab's use the settings give is 1 or 0, and -1 where they give none
    frame->indent_tabs = ((view->indent.tabs == 0) || (view->indent.tabs == 1)) ? view->indent.tabs : -1;
    frame->indent_size = EMBED_CALL(view_tab, ViewCtx, .view_read = view);
    frame->find.open = view->find_open;
    frame->find.current = -1LL;
    if (view->find_open)
    {
        EMBED_CALL(view_frame_find, ViewCtx, .view_read = view, .displayed = &displayed, .frame = frame,
                   .count = query_bytes);
    }
    // the lines from the top to the last row's are the lines the walk hands runs on for, and the rows' runs are read a
    // run of rows at a time
    if (view->paged)
    {
        (void)EMBED_CALL(cli_document_key_from.page, CliDocumentKeyFromCfg, .key = view->key, .from = from, .to = to);
    }
    else
    {
        (void)EMBED_CALL(cli_document_key_from.cache, CliDocumentKeyFromCfg, .key = view->key, .from = from, .to = to);
    }
    frame->runs =
        EMBED_CALL(cli_document_runs.runs_alloc, CliDocumentRunsCfg, .key = view->key, .number = displayed.number,
                   .count = displayed.count, .mirrored = 1, .owner = g_cli_views_owner);
    g_cli_guard->persistent_buf_release(number, g_cli_views_owner);
    if (frame->runs == NULL)
    {
        cli_view_frame_release(frame);
        return NULL;
    }
    if ((s_watcher != NULL) && (s_watcher->drawn != NULL))
    {
        const CliViewDrawn drawn = {view->key, view->cursor, view->top, lines, view->rows};
        s_watcher->drawn(&drawn);
    }
    return frame;
}

unsigned long long cli_view_top(const char *key)
{
    const CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = key);
    return (view != NULL) ? view->top : 0ULL;
}

int cli_view_cursor(const CliViewCursorCfg *args)
{
    CliPosition *cursor = args->cursor;
    CliPosition *anchor = args->anchor;
    const CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view == NULL)
    {
        return 0;
    }
    if (cursor != NULL)
    {
        *cursor = view->cursor;
    }
    if (anchor != NULL)
    {
        *anchor = view->anchor;
    }
    return 1;
}

// the bytes the runs of the minimap's lines may take as they are read for their bars
#define CLI_VIEW_MAP_CAPACITY (1024ULL * 1024ULL)

CliDocumentMap *cli_view_map_alloc(const char *key, unsigned long long first, unsigned long long last)
{
    const CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = key);
    if (view == NULL)
    {
        return NULL;
    }
    const unsigned int tab = (view->indent.tabs == 1) ? EMBED_CALL(view_tab, ViewCtx, .view_read = view) : 1u;
    return EMBED_CALL(cli_document_map.map_alloc, CliDocumentMapCfg, .key = key, .first = first, .last = last,
                      .tab = tab, .owner = g_cli_views_owner);
}

void cli_view_map_release(CliDocumentMap *map)
{
    g_cli_guard->persistent_buf_release(map, g_cli_views_owner);
}

int cli_view_path(const CliViewPathKindCfg *args)
{
    const CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view != NULL)
    {
        VerbaLinea line = {args->path_out, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, view->path);
        (void)mmgr_line_finish(&line);
    }
    return view != NULL;
}

int cli_view_served_set(const CliViewFullCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view != NULL)
    {
        VerbaLinea line = {view->served, sizeof(view->served), 0ULL};
        mmgr_line_put(&line, args->full);
        (void)mmgr_line_finish(&line);
        view->changed = 0;
    }
    return view != NULL;
}

int cli_view_serve(const CliViewFullKindCfg *args)
{
    const CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    const int served = (view != NULL) && (view->served[0] != '\0');
    if (served)
    {
        VerbaLinea line = {args->full_out, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, view->served);
        (void)mmgr_line_finish(&line);
    }
    return served;
}

int cli_view_serving(const CliViewFullCfg *args)
{
    int serving = 0;
    unsigned int at = 0u;
    while (!serving && (s_views != NULL) && (at < CLI_VIEWS))
    {
        serving =
            s_views[at].used && cellul.eq(&(CatenaFinitaCfg){
                                    .src = s_views[at].served, .other = args->full, .cap = sizeof(s_views[at].served)});
        at += 1u;
    }
    return serving;
}

// the last edit of the step the view stands after, NULL where it stands before every step
EMBED_INLINE const CliViewEdit *view_place(const ViewCtx *args)
{
    return (args->view_read->done != 0u) ? args->view_read->step[args->view_read->done - 1u].last : NULL;
}

void cli_view_saved(const CliViewCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view != NULL)
    {
        view->save_point = EMBED_CALL(view_place, ViewCtx, .view_read = view);
        view->save_point_lost = 0;
    }
}

void cli_view_unsaved(const CliViewCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view != NULL)
    {
        view->save_point_lost = 1;
    }
}

int cli_view_modified(const CliViewCfg *args)
{
    const CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    return (view != NULL) &&
           (view->save_point_lost || (EMBED_CALL(view_place, ViewCtx, .view_read = view) != view->save_point));
}

// what the file writer's line is handed as a text read whole is written: the text's lines put into it
static int view_text_put(VerbaLinea *line, void *context)
{
    const CliViewSaveCfg *const saving = (const CliViewSaveCfg *)context;
    return EMBED_CALL(cli_document_out.file_put, CliDocumentOutCfg, .key = saving->key, .out = line,
                      .err_msg = saving->err_msg);
}

int cli_view_save(const CliViewSaveCfg *args)
{
    const CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view == NULL)
    {
        mmgr_line_put(args->err_msg, "no view is open under ");
        mmgr_line_put(args->err_msg, args->key);
        return 0;
    }
    const int written =
        view->paged ? EMBED_CALL(cli_pages_path.save, CliPagesPathCfg, .key = args->key, .err_msg = args->err_msg)
                    : EMBED_CALL(cli_path_put_internal.file_put_write, CliPathPutCtx, .path = args->full,
                                 .put = view_text_put, .context = (void *)args);
    if (written)
    {
        EMBED_CALL(cli_view_table.saved, CliViewCfg, .key = args->key);
    }
    else
    {
        EMBED_CALL(cli_view_table.unsaved, CliViewCfg, .key = args->key);
        if (!view->paged)
        {
            mmgr_line_put(args->err_msg, args->full);
            mmgr_line_put(args->err_msg, ": it could not be written");
        }
    }
    return written;
}

int cli_view_changed_alloc(const CliViewCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    const int changed = (view != NULL) && view->changed;
    if (view != NULL)
    {
        view->changed = 0;
    }
    return changed;
}

// the span of the word the place at `line`, `col` touches, inside it or at either end: its first unit into `from` and
// the unit after it into `to`: 1, or 0 where no word stands there
EMBED_INLINE int view_word_span(const ViewCtx *args)
{
    unsigned long long *from = args->from_u64s;
    unsigned long long *to = args->to_u64s;
    const unsigned long long length = view_length(args);
    unsigned long long start = (args->col < length) ? args->col : length;
    unsigned long long end = start;
    while ((start > 0ULL) && EMBED_CALL(view_word_unit, ViewCtx,
                                        .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->line,
                                                           .col = start - 1ULL)))
    {
        start -= 1ULL;
    }
    while ((end < length) &&
           EMBED_CALL(view_word_unit, ViewCtx,
                      .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->line, .col = end)))
    {
        end += 1ULL;
    }
    *from = start;
    *to = end;
    return end > start;
}

int cli_view_position_at(const CliViewRowCfg *args)
{
    unsigned long long *line = args->line;
    unsigned long long *col = args->col;
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view != NULL)
    {
        *line = EMBED_CALL(view_down, ViewCtx, .view_read = view, .line = view->top, .count = args->row);
        *col = EMBED_CALL(view_col_at, ViewCtx, .view = view, .line = *line, .cell = args->column);
    }
    return view != NULL;
}

int cli_view_word_at(const CliViewLineCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    const int found =
        (view != NULL) && EMBED_CALL(view_word_span, ViewCtx, .view = view, .line = args->line_value,
                                     .col = args->col_value, .from_u64s = args->from, .to_u64s = args->to);
    if (found)
    {
        VerbaLinea text = {args->word_text, args->capacity, 0ULL};
        EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &text,
                   .units = view->line_units + *args->from, .count = *args->to - *args->from);
        (void)mmgr_line_finish(&text);
    }
    return found;
}

int cli_view_prefix(const CliViewFromCfg *args)
{
    unsigned long long *from = args->from;
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view == NULL)
    {
        return 0;
    }
    const unsigned long long line = view->cursor.line;
    const unsigned long long end = view->cursor.col;
    unsigned long long start = (end < EMBED_CALL(view_length, ViewCtx, .view = view, .line = line))
                                   ? end
                                   : EMBED_CALL(view_length, ViewCtx, .view = view, .line = line);
    while ((start > 0ULL) &&
           EMBED_CALL(view_word_unit, ViewCtx,
                      .unit = EMBED_CALL(view_unit, ViewCtx, .view = view, .line = line, .col = start - 1ULL)))
    {
        start -= 1ULL;
    }
    *from = start;
    VerbaLinea text = {args->prefix, args->capacity, 0ULL};
    if (end > start)
    {
        EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &text,
                   .units = view->line_units + start, .count = end - start);
    }
    (void)mmgr_line_finish(&text);
    return 1;
}

// the hash table's entries, a power of two above the most words, each a word's offset in the text plus one, 0 for an
// empty entry, and its length
#define CLI_VIEW_WORD_TABLE 8192ULL

CliDocumentWords *cli_view_words_alloc(const char *key)
{
    const CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = key);
    if (view == NULL)
    {
        return NULL;
    }
    const CliPosition cursor = view->cursor;
    const unsigned long long first = (cursor.line > CLI_VIEW_WORD_LINES) ? cursor.line - CLI_VIEW_WORD_LINES : 0ULL;
    return EMBED_CALL(cli_document_words.words_alloc, CliDocumentWordsCfg, .key = key, .first = first,
                      .last = cursor.line + CLI_VIEW_WORD_LINES, .cursor = cursor, .owner = g_cli_views_owner);
}

void cli_view_words_release(CliDocumentWords *words)
{
    g_cli_guard->persistent_buf_release(words, g_cli_views_owner);
}

// the kind of a step of edits the CLI answers while its edits are written, which joins them; the step stands of no
// kind once they are, so no edit after joins it
#define CLI_VIEW_KIND_EDITS 5u

int cli_view_edits_apply(const CliViewEditsCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view == NULL)
    {
        mmgr_line_put(args->err_msg, "no view is open under the key");
        return 0;
    }
    if (args->count == 0u)
    {
        return 1;
    }
    EMBED_CALL(view_folds_refresh, ViewCtx, .view = view);
    int written = 1;
    CliViewStep *step = NULL;
    for (unsigned int at = args->count; written && (at > 0u); at -= 1u)
    {
        const CliEdit *const edit = &args->edit[at - 1u];
        CliViewEdit *const made =
            EMBED_CALL(view_edit_cell, ViewCtx, .view = view, .from = edit->from, .to = edit->to,
                       .append_bytes = edit->length, .append_spans = 0ULL, .err_msg = args->err_msg);
        if (made == NULL)
        {
            written = 0;
            break;
        }
        memor.cpy(&(MemoriaCfg){.dst = made->put, .src = edit->text, .bytes = edit->length});
        made->put[edit->length] = '\0';
        made->append_length = edit->length;
        // the first edit begins a step of its own, and each after it joins that step
        written =
            EMBED_CALL(view_edit_cell_write, ViewCtx, .view = view, .made = made,
                       .kind = (step == NULL) ? CLI_VIEW_KIND_NONE : CLI_VIEW_KIND_EDITS, .err_msg = args->err_msg);
        if (written && (step == NULL))
        {
            step = &view->step[view->steps - 1u];
            step->kind = CLI_VIEW_KIND_EDITS;
        }
    }
    if (step != NULL)
    {
        step->kind = CLI_VIEW_KIND_NONE;
        EMBED_CALL(view_move, ViewCtx, .view = view, .place = args->edit[0].from, .extend = 0, .saved = 0);
        step->cursor_after = view->cursor;
        step->anchor_after = view->anchor;
    }
    return written;
}

unsigned long long cli_view_vim_lines(const CliViewVimCfg *args)
{
    const CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    return (view != NULL) ? EMBED_CALL(view_lines, ViewCtx, .view_read = view) : 0ULL;
}

const unsigned short *cli_view_vim_line(const CliViewVimCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    *args->count = 0ULL;
    if ((view == NULL) || (args->line >= EMBED_CALL(view_lines, ViewCtx, .view_read = view)))
    {
        return NULL;
    }
    *args->count = EMBED_CALL(view_length, ViewCtx, .view = view, .line = args->line);
    return view->line_units;
}

unsigned int cli_view_vim_rows(const CliViewVimCfg *args)
{
    const CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    return (view != NULL) ? view->rows : 0u;
}

unsigned long long cli_view_vim_span_bytes(const CliViewVimCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    return (view != NULL) ? EMBED_CALL(view_span_bytes, ViewCtx, .view = view, .from = args->from, .to = args->to)
                          : 0ULL;
}

void cli_view_vim_span(const CliViewVimCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view != NULL)
    {
        EMBED_CALL(view_span_write, ViewCtx, .view = view, .from = args->from, .to = args->to, .out = args->out);
    }
}

int cli_view_vim_edit(const CliViewVimCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea own = {why, sizeof(why), 0ULL};
    VerbaLinea *const err_msg = (args->err_msg != NULL) ? args->err_msg : &own;
    return (view != NULL) &&
           EMBED_CALL(view_edit, ViewCtx, .view = view, .from = args->from, .to = args->to, .text = args->text,
                      .length = args->length, .kind = CLI_VIEW_KIND_NONE, .err_msg = err_msg);
}

int cli_view_vim_indent(const CliViewVimCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view == NULL)
    {
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea own = {why, sizeof(why), 0ULL};
    // the selection laid over the lines from the first's start to the last's end, the lines the indentation covers
    view->anchor = (CliPosition){args->from.line, 0ULL};
    view->cursor = (CliPosition){args->to.line, EMBED_CALL(view_length, ViewCtx, .view = view, .line = args->to.line)};
    return EMBED_CALL(view_lines_indent, ViewCtx, .view = view, .deeper = args->deeper,
                      .err_msg = (args->err_msg != NULL) ? args->err_msg : &own);
}

void cli_view_vim_place(const CliViewVimCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view == NULL)
    {
        return;
    }
    view->cursor = args->cursor;
    view->anchor = args->anchor;
    view->goal_saved = 0;
    EMBED_CALL(view_follow, ViewCtx, .view = view);
}

int cli_view_vim_step(const CliViewVimCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view == NULL)
    {
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea own = {why, sizeof(why), 0ULL};
    VerbaLinea *const err_msg = (args->err_msg != NULL) ? args->err_msg : &own;
    return args->redo ? EMBED_CALL(view_redone, ViewCtx, .view = view, .err_msg = err_msg)
                      : EMBED_CALL(view_undone, ViewCtx, .view = view, .err_msg = err_msg);
}

// the first of the view's matches whose start does not stand before `place`, by halving the matches, which stand in
// the order of their starts
EMBED_INLINE unsigned long long view_match_after(const ViewCtx *args)
{
    unsigned long long low = 0ULL;
    unsigned long long high = args->view_read->matches;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ULL);
        const CliPosition start = {args->view_read->match[middle][0], args->view_read->match[middle][1]};
        if (EMBED_CALL(view_before, ViewCtx, .one = start, .other = args->place) ||
            (args->exact && (start.line == args->place.line) && (start.col == args->place.col)))
        {
            low = middle + 1ULL;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

int cli_view_vim_find(const CliViewVimCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if ((view == NULL) || (args->length == 0ULL))
    {
        return 0;
    }
    unsigned short *const units =
        g_cli_guard->temporary_buf_alloc((args->length + 1ULL) * sizeof(*units), g_cli_views_owner);
    if (units == NULL)
    {
        return 0;
    }
    const unsigned long long count = EMBED_CALL(view_units_get, ViewCtx, .text = args->text, .size = args->length,
                                                .units = units, .capacity = args->length + 1ULL);
    // the query stands where it is the same, with case, whole words as asked, and a pattern or the text itself
    const int same =
        (view->find_query != NULL) && (view->find_length == count) && view->find_case &&
        (view->find_word == args->word_text) && (!args->word_text || !view->find_regex) &&
        (memor.cmp(&(MemoriaCfg){.src = view->find_query, .other = units, .bytes = count * sizeof(*units)}) == 0);
    if (!same)
    {
        view->find_length = 0ULL;
        view->find_at = 0ULL;
        view->find_case = 1;
        view->find_word = args->word_text;
        view->find_regex = !args->word_text;
        EMBED_CALL(view_query_append, ViewCtx, .view = view, .units_read = units, .count = count);
        view->find_stale = 1;
    }
    g_cli_guard->persistent_buf_release(units, g_cli_views_owner);
    EMBED_CALL(view_search, ViewCtx, .view = view);
    if (!same && (view->matches == 0ULL) && view->find_regex)
    {
        // a text that reads as no pattern, or finds nothing as one, is sought as the text itself
        view->find_regex = 0;
        view->find_stale = 1;
        EMBED_CALL(view_search, ViewCtx, .view = view);
    }
    if (view->matches == 0ULL)
    {
        return 0;
    }
    unsigned long long chosen = 0ULL;
    if (args->back)
    {
        const unsigned long long before =
            EMBED_CALL(view_match_after, ViewCtx, .view_read = view, .place = args->from, .exact = 0);
        chosen = (before > 0ULL) ? before - 1ULL : view->matches - 1ULL;
    }
    else
    {
        const unsigned long long after =
            EMBED_CALL(view_match_after, ViewCtx, .view_read = view, .place = args->from, .exact = 1);
        chosen = (after < view->matches) ? after : 0ULL;
    }
    // the matches are fewer than CLI_VIEW_MATCHES, which a long long counts
    view->current = (long long)chosen;
    *args->place = (CliPosition){view->match[chosen][0], view->match[chosen][1]};
    return 1;
}
