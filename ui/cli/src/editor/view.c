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
#include "../command_line/cells.h"
#include "../plugins/plugins.h"
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "document_internal.h"
#include "editorconfig.h"
#include "pages.h"

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
    CliPlace from;
    CliPlace to;
    CliPlace end;
    char *put;
    unsigned long long put_length;
    char *took;
    unsigned long long took_length;
    struct CliViewEdit *before;
    struct CliViewEdit *after;
} CliViewEdit;
// A step: its first and last edits, how many, the kind of edit it holds and when its last was
// made, and where the cursor and the anchor stood before it and after it
typedef struct
{
    CliViewEdit *first;
    CliViewEdit *last;
    unsigned int edits;
    unsigned int kind;
    unsigned long long at_ms;
    CliPlace cursor_before;
    CliPlace anchor_before;
    CliPlace cursor_after;
    CliPlace anchor_after;
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
// lines, how many and the room; find: whether its bar is open and whether the keys are its own, its query in a cell of
// its own, in units, its length, the room and where its cursor stands, whether it seeks a pattern, with case and whole
// words, whether the matches are to be found anew, the matches, each a line, a from and a to column, in a cell of their
// own, how many and the room, and the current one, -1 for none; and the line read last, its units in a cell of the
// view's own, the room it has, which line it is and whether it is known: a line's units are read once for the moves
// along it
typedef struct
{
    int used;
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
    unsigned long long find_room;
    unsigned long long find_at;
    int find_regex;
    int find_case;
    int find_word;
    int find_stale;
    unsigned long long (*match)[3];
    unsigned long long matches;
    unsigned long long match_room;
    long long current;
    unsigned long long top;
    unsigned int rows;
    unsigned int columns;
    CliPlace cursor;
    CliPlace anchor;
    unsigned long long goal;
    int goal_saved;
    CliPlace grown[CLI_VIEW_GROWN][2];
    unsigned int grown_count;
    CliPlace grown_cursor;
    CliPlace grown_anchor;
    CliIndent indent;
    unsigned long long seq;
    CliViewStep *step;
    unsigned int steps;
    unsigned int steps_room;
    unsigned int done;
    CliViewFold *fold;
    unsigned int folds;
    unsigned int folds_room;
    unsigned short *line_units;
    unsigned long long units_room;
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
    int shown;
} CliViewFoldSeen;
// The folds among a frame's rows as they are found: the rows, the folds seen, one a row at the most, and how many
typedef struct
{
    const CliViewRows *shown;
    CliViewFoldSeen *seen;
    unsigned long long count;
} CliViewFoldsSeen;
// the size of the views table, the most edits a step holds before it closes, the kinds of edit, the lines read
// about the cursor each way for the bracket pair at it, the room for the state the first of them is entered in, the
// most pairs and quotes a language has, and the room for its pattern of a line that opens a block
#define CLI_VIEWS 64u
#define CLI_VIEW_STEP_EDITS 256u
#define CLI_VIEW_FOLDS 65536u
#define CLI_VIEW_STEPS 4096u
#define CLI_VIEW_UNITS_LEAST 256ull
#define CLI_VIEW_FIND_UNITS 65536ull
#define CLI_VIEW_PAIR_LINES 3000ull
#define CLI_VIEW_STATE 1024u
// the matches find saves at the most
#define CLI_VIEW_MATCHES 100000ull
#define CLI_VIEW_KIND_NONE 0u
#define CLI_VIEW_KIND_TYPE 1u
#define CLI_VIEW_KIND_SPACE 2u
#define CLI_VIEW_KIND_DELETE 3u
#define CLI_VIEW_KIND_DELETE_FORWARD 4u

static CliView *s_views;

typedef struct
{
    const char *key;
    const char *name;
    const char *wanted;
    CliViewStep *step;
    CliView *view;
    unsigned int from_u32;
    const CliJson *json;
    unsigned int node;
    char *mark;
    unsigned long long room;
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
    CliPlace place;
    CliPlace one;
    CliPlace other;
    CliPlace *from_pointer;
    CliPlace *to_pointer;
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
    CliPlace from;
    CliPlace to;
    unsigned long long bytes;
    VerbaLinea *out;
    unsigned long long put_bytes;
    unsigned long long put_spans;
    CliViewEdit *made;
    unsigned int kind;
    unsigned long long length;
    CliPlace cursor;
    CliPlace anchor;
    unsigned int open;
    unsigned int close;
    int quoted;
    unsigned int letter;
    CliPlace *pair;
    unsigned long long *from_u64s;
    unsigned long long *to_u64s;
    unsigned long long *first_pointer;
    unsigned long long *last_pointer;
    CliPlace end;
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
    int *taken;
    int *mod_pointer;
    int *shift_pointer;
    int *alt_pointer;
    const char *given;
    const CliViewInput *input;
    const CliViewRows *shown;
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

// whether a key's own name is `wanted`
EMBED_INLINE int view_by_name(const ViewCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = args->wanted, .cap = 64ull});
}

// the steps from `from` on let go, their cells given back
EMBED_INLINE void view_step_free(const ViewCtx *args)
{
    for (CliViewEdit *edit = args->step->first; edit != NULL;)
    {
        CliViewEdit *const after = edit->after;
        g_cli_cells->persistent_buf_release(edit, &g_cli_views_cells);
        edit = after;
    }
}

EMBED_INLINE void view_steps_cut(const ViewCtx *args)
{
    for (unsigned int at = args->from_u32; at < args->view->steps; at += 1u)
    {
        EMBED_CALL(view_step_free, ViewCtx, .step = &args->view->step[at]);
    }
    args->view->steps = (args->from_u32 < args->view->steps) ? args->from_u32 : args->view->steps;
    args->view->done = (args->view->done < args->view->steps) ? args->view->done : args->view->steps;
}

// A language's plugin sought among the plugins: its id, and the view its pairs, quotes and pattern are read into
typedef struct
{
    const char *id;
    CliView *view;
} CliViewPluginSought;

// the first letter of the string node `node` as a unit, 0 where the node is no string or it is empty
EMBED_INLINE unsigned int view_letter_get(const ViewCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    unsigned long long length = 0ull;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = node, .length_pointer = &length);
    unsigned long long took = 0ull;
    return ((text != NULL) && (length != 0ull))
               ? EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = length, .at = 0ull, .length = &took)
               : 0u;
}

// the pairs, quotes and pattern of the plugin whose id is sought read into the view; where the plugin names none,
// the defaults apply: the brackets, the two quotes and the pattern of an opener at a line's end
// a comment's mark read off the string at `node` of the plugin's tree into `mark`, sized for `room`, empty where none
EMBED_INLINE void view_comment_read(const ViewCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    unsigned long long length = 0ull;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = node, .length_pointer = &length);
    VerbaLinea line = {args->mark, args->room, 0ull};
    mmgr_line_put_n(&line, (text != NULL) ? text : "", (text != NULL) ? length : 0ull);
    if (mmgr_line_finish(&line) == 0ull)
    {
        args->mark[0] = '\0';
    }
}

static void view_plugin_read(const CliEachPluginCfg *args)
{
    CliViewPluginSought *const sought = args->context;
    CliView *const view = sought->view;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = sought->id, .cap = CLI_NAME_MAX_LEN}))
    {
        return;
    }
    char why_text[256];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->plugin->text, .size = args->plugin->size, .nodes_more = 0u, .text_more = 0ull,
                   .err_msg = &why, .owner = &g_cli_views_cells);
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
    EMBED_CALL(view_comment_read, ViewCtx, .mark = view->comment_line, .room = sizeof(view->comment_line),
               .json = &json,
               .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                  .object = comments, .name = "line"));
    const unsigned int block = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = comments, .name = "block");
    EMBED_CALL(view_comment_read, ViewCtx, .mark = view->comment_open, .room = sizeof(view->comment_open),
               .json = &json,
               .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json,
                                  .array = block, .index = 0u));
    EMBED_CALL(view_comment_read, ViewCtx, .mark = view->comment_close, .room = sizeof(view->comment_close),
               .json = &json,
               .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json,
                                  .array = block, .index = 1u));
    view->pairs = 0u;
    view->quotes = 0u;
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = pairs,
                   .kind = CFG_JSON_ARRAY))
    {
        for (unsigned int at = 0u; (at < json.node[pairs].count) && (view->pairs < CLI_VIEW_PAIRS); at += 1u)
        {
            unsigned long long length = 0ull;
            const char *const text =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = &json, .array = pairs, .index = at),
                           .length_pointer = &length);
            unsigned long long took = 0ull;
            const unsigned int open = ((text != NULL) && (length != 0ull))
                                          ? EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = length,
                                                       .at = 0ull, .length = &took)
                                          : 0u;
            const unsigned int close = ((open != 0u) && (took < length))
                                           ? EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = length,
                                                        .at = took, .length = &took)
                                           : 0u;
            if ((open != 0u) && (close != 0u))
            {
                view->pair_open[view->pairs] = open;
                view->pair_close[view->pairs] = close;
                view->pairs += 1u;
            }
        }
    }
    else
    {
        view->pair_open[0] = '(';
        view->pair_close[0] = ')';
        view->pair_open[1] = '[';
        view->pair_close[1] = ']';
        view->pair_open[2] = '{';
        view->pair_close[2] = '}';
        view->pairs = 3u;
    }
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
    VerbaLinea pattern = {view->indent_after, sizeof(view->indent_after), 0ull};
    unsigned long long length = 0ull;
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
    if (mmgr_line_finish(&pattern) == 0ull)
    {
        view->indent_after[0] = '\0';
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
}

// the view's language read off the plugin the file at its path opens as; none where no plugin does
EMBED_INLINE void view_language_read(const ViewCtx *args)
{
    char id[CLI_NAME_MAX_LEN];
    CliLanguages languages;
    EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &languages);
    const int named = EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                 .languages_read = &languages, .path = args->view->path, .id_out = id);
    EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = &languages);
    if (named)
    {
        CliViewPluginSought sought = {id, args->view};
        EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = view_plugin_read, .context = &sought);
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
        s_views = g_cli_cells->persistent_buf_alloc(CLI_VIEWS * sizeof(*s_views), &g_cli_views_cells);
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
        g_cli_cells->persistent_buf_release(view->step, &g_cli_views_cells);
        g_cli_cells->persistent_buf_release(view->fold, &g_cli_views_cells);
        g_cli_cells->persistent_buf_release(view->find_query, &g_cli_views_cells);
        g_cli_cells->persistent_buf_release(view->match, &g_cli_views_cells);
        g_cli_cells->persistent_buf_release(view->line_units, &g_cli_views_cells);
    }
    else
    {
        for (unsigned int at = 0u; (view == NULL) && (at < CLI_VIEWS); at += 1u)
        {
            view = s_views[at].used ? NULL : &s_views[at];
        }
    }
    if (view == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = view, .val = 0, .bytes = sizeof(*view)});
    view->used = 1;
    VerbaLinea named = {view->key, sizeof(view->key), 0ull};
    mmgr_line_put(&named, key);
    (void)mmgr_line_finish(&named);
    VerbaLinea pathed = {view->path, sizeof(view->path), 0ull};
    mmgr_line_put(&pathed, path);
    (void)mmgr_line_finish(&pathed);
    view->rows = 1u;
    view->paged = paged;
    EMBED_CALL(cli_editorconfig.indent, CliEditorconfigCfg, .path = path, .indent = &view->indent);
    EMBED_CALL(view_language_read, ViewCtx, .view = view);
    return 1;
}

void cli_view_close(const CliViewCfg *args)
{
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view != NULL)
    {
        EMBED_CALL(view_steps_cut, ViewCtx, .view = view, .from_u32 = 0u);
        g_cli_cells->persistent_buf_release(view->step, &g_cli_views_cells);
        g_cli_cells->persistent_buf_release(view->fold, &g_cli_views_cells);
        g_cli_cells->persistent_buf_release(view->find_query, &g_cli_views_cells);
        g_cli_cells->persistent_buf_release(view->match, &g_cli_views_cells);
        g_cli_cells->persistent_buf_release(view->line_units, &g_cli_views_cells);
        memor.set(&(MemoriaCfg){.dst = view, .val = 0, .bytes = sizeof(*view)});
    }
}

// the text's lines: how many, as far as the pages' index has read for a paged text
EMBED_INLINE unsigned long long view_lines(const ViewCtx *args)
{
    if (args->view_read->paged)
    {
        unsigned long long lines = 0ull;
        int whole = 0;
        return EMBED_CALL(cli_pages.count, CliPagesCfg, .key = args->view_read->key, .lines = &lines, .whole = &whole)
                   ? lines
                   : 0ull;
    }
    CliShape shape;
    return EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = args->view_read->key, .shape = &shape)
               ? shape.lines
               : 0ull;
}

// the closed fold that hides `line`, its first line above the line and its last at or below it, the outermost where
// more do, or NULL where none does
EMBED_INLINE const CliViewFold *view_hider(const ViewCtx *args)
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
EMBED_INLINE unsigned long long view_shown_up(const ViewCtx *args)
{
    unsigned long long line = args->line;
    for (const CliViewFold *hider = EMBED_CALL(view_hider, ViewCtx, .view_read = args->view_read, .line = line);
         hider != NULL; hider = EMBED_CALL(view_hider, ViewCtx, .view_read = args->view_read, .line = line))
    {
        line = hider->first;
    }
    return line;
}

EMBED_INLINE unsigned long long view_shown_down(const ViewCtx *args)
{
    unsigned long long line = args->line;
    const unsigned long long lines = view_lines(args);
    const unsigned long long last = (lines != 0ull) ? lines - 1ull : 0ull;
    for (const CliViewFold *hider = EMBED_CALL(view_hider, ViewCtx, .view_read = args->view_read, .line = line);
         hider != NULL; hider = EMBED_CALL(view_hider, ViewCtx, .view_read = args->view_read, .line = line))
    {
        if (hider->last >= last)
        {
            return EMBED_CALL(view_shown_up, ViewCtx, .view_read = args->view_read, .line = last);
        }
        line = hider->last + 1ull;
    }
    return (line > last) ? EMBED_CALL(view_shown_up, ViewCtx, .view_read = args->view_read, .line = last) : line;
}

// the line `count` lines shown below `line`, the last shown where fewer follow; and the one `count` lines shown above
// it, the first where fewer lead
EMBED_INLINE unsigned long long view_down(const ViewCtx *args)
{
    const unsigned long long lines = view_lines(args);
    unsigned long long at = view_shown_up(args);
    for (unsigned long long step = 0ull; (step < args->count) && (at + 1ull < lines); step += 1ull)
    {
        const unsigned long long next =
            EMBED_CALL(view_shown_down, ViewCtx, .view_read = args->view_read, .line = at + 1ull);
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
    unsigned long long at = view_shown_up(args);
    for (unsigned long long step = 0ull; (step < args->count) && (at > 0ull); step += 1ull)
    {
        at = EMBED_CALL(view_shown_up, ViewCtx, .view_read = args->view_read, .line = at - 1ull);
    }
    return at;
}

// how many lines shown stand from `top` to before `line`, `most` at the most
EMBED_INLINE unsigned long long view_rows_to(const ViewCtx *args)
{
    unsigned long long rows = 0ull;
    unsigned long long at = EMBED_CALL(view_shown_up, ViewCtx, .view_read = args->view_read, .line = args->top);
    while ((at < args->line) && (rows < args->most))
    {
        const unsigned long long next =
            EMBED_CALL(view_down, ViewCtx, .view_read = args->view_read, .line = at, .count = 1ull);
        if (next <= at)
        {
            break;
        }
        at = next;
        rows += 1ull;
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
} CliViewFoldSought;

// the text's fold opened on the line sought taken; the innermost of those that open on it or above it and reach it
static void view_fold_match(const CliFoldEachCfg *args)
{
    unsigned long long first = args->first;
    unsigned long long last = args->last;
    CliViewFoldSought *const sought = args->context;
    if (first == sought->line)
    {
        sought->first = first;
        sought->last = last;
        sought->found = 1;
    }
}

static void view_fold_innermost(const CliFoldEachCfg *args)
{
    unsigned long long first = args->first;
    unsigned long long last = args->last;
    CliViewFoldSought *const sought = args->context;
    if ((first <= sought->line) && (sought->line <= last) && (!sought->found || (first > sought->first)))
    {
        sought->first = first;
        sought->last = last;
        sought->found = 1;
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
EMBED_INLINE int view_fold_put(const ViewCtx *args)
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
    if (args->view->folds_room == 0u)
    {
        args->view->fold =
            g_cli_cells->persistent_buf_alloc(CLI_VIEW_FOLDS * sizeof(*args->view->fold), &g_cli_views_cells);
        args->view->folds_room = (args->view->fold != NULL) ? CLI_VIEW_FOLDS : 0u;
    }
    if (args->view->folds == args->view->folds_room)
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
    unsigned long long added = 0ull;
    for (unsigned long long at = 0ull; at < args->edit->length; at += 1ull)
    {
        added += (args->edit->text[at] == '\n') ? 1ull : 0ull;
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
                (args->edit->from.line == fold.first) && (args->edit->to.line == fold.first) && (added == 0ull);
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

// `size` bytes of UTF-8 as UTF-16 units into `units`, which is sized for `room`: their count, a letter past the basic
// plane two units
EMBED_INLINE unsigned long long view_units_get(const ViewCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long at = 0ull;
    unsigned long long count = 0ull;
    while ((at < size) && (count < args->room))
    {
        unsigned long long took = 0ull;
        const unsigned int letter =
            EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = size, .at = at, .length = &took);
        at += (took != 0ull) ? took : 1ull;
        if (letter >= 0x10000u)
        {
            if (count + 2ull > args->room)
            {
                break;
            }
            // a letter past the basic plane as its two surrogates, each a unit
            args->units[count] = (unsigned short)(0xd800u + ((letter - 0x10000u) >> 10u));
            args->units[count + 1ull] = (unsigned short)(0xdc00u + ((letter - 0x10000u) & 0x3ffu));
            count += 2ull;
        }
        else
        {
            // a letter of the basic plane is one unit
            args->units[count] = (unsigned short)letter;
            count += 1ull;
        }
    }
    return count;
}

// the view's line cell given room for `units`: 1, or 0 where it could not be taken
EMBED_INLINE int view_units_room(const ViewCtx *args)
{
    if (args->view->units_room >= args->units_u64)
    {
        return 1;
    }
    // the line's units are read anew into a cell of their count
    const unsigned long long room = (args->units_u64 > CLI_VIEW_UNITS_LEAST) ? args->units_u64 : CLI_VIEW_UNITS_LEAST;
    g_cli_cells->persistent_buf_release(args->view->line_units, &g_cli_views_cells);
    args->view->line_units =
        g_cli_cells->persistent_buf_alloc(room * sizeof(*args->view->line_units), &g_cli_views_cells);
    args->view->units_room = (args->view->line_units != NULL) ? room : 0ull;
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
    args->view->line_length = 0ull;
    if (args->view->paged)
    {
        const char *text = NULL;
        unsigned long long count = 0ull;
        char *const cell = EMBED_CALL(cli_pages_from.lines_take, CliPagesFromCfg, .key = args->view->key, .from = line,
                                      .to = line + 1ull, .line = &text, .count = &count, .owner = &g_cli_views_cells);
        if ((cell != NULL) && (count != 0ull))
        {
            const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
            if (EMBED_CALL(view_units_room, ViewCtx, .view = args->view, .units_u64 = size))
            {
                args->view->line_length = EMBED_CALL(view_units_get, ViewCtx, .text = text, .size = size,
                                                     .units = args->view->line_units, .room = args->view->units_room);
            }
        }
        g_cli_cells->persistent_buf_release(cell, &g_cli_views_cells);
        return;
    }
    const CliStoredText *const one =
        EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->view->key);
    if ((one != NULL) && (line < one->lines.lines) &&
        EMBED_CALL(view_units_room, ViewCtx, .view = args->view, .units_u64 = one->lines.length[line]))
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
    unsigned long long cell = 0ull;
    for (unsigned long long at = 0ull; at < args->col; at += 1ull)
    {
        cell += (EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->line, .col = at) == '\t')
                    ? (tab - (cell % tab))
                    : 1ull;
    }
    return cell;
}

EMBED_INLINE unsigned long long view_col_at(const ViewCtx *args)
{
    const unsigned int tab = EMBED_CALL(view_tab, ViewCtx, .view_read = args->view);
    const unsigned long long length = view_length(args);
    unsigned long long at = 0ull;
    unsigned long long here = 0ull;
    while (at < length)
    {
        const unsigned long long next =
            here + ((EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->line, .col = at) == '\t')
                        ? (tab - (here % tab))
                        : 1ull);
        if (next > args->cell)
        {
            // the cell inside a letter's span goes to its nearer side
            return ((args->cell - here) * 2ull >= (next - here)) ? at + 1ull : at;
        }
        here = next;
        at += 1ull;
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
EMBED_INLINE CliPlace view_leave(const ViewCtx *args)
{
    if (args->place.col == 0ull)
    {
        const unsigned long long above =
            EMBED_CALL(view_up, ViewCtx, .view_read = args->view, .line = args->place.line, .count = 1ull);
        return (above == args->place.line)
                   ? args->place
                   : (CliPlace){above, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = above)};
    }
    const unsigned int before =
        EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->place.line, .col = args->place.col - 1ull);
    const int pair = (args->place.col >= 2ull) && (before >= 0xdc00u) && (before <= 0xdfffu);
    return (CliPlace){args->place.line, args->place.col - (pair ? 2ull : 1ull)};
}

EMBED_INLINE CliPlace view_right(const ViewCtx *args)
{
    const unsigned long long length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = args->place.line);
    if (args->place.col >= length)
    {
        const unsigned long long below =
            EMBED_CALL(view_down, ViewCtx, .view_read = args->view, .line = args->place.line, .count = 1ull);
        return (below == args->place.line) ? args->place : (CliPlace){below, 0ull};
    }
    const unsigned int here =
        EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->place.line, .col = args->place.col);
    const int pair = (args->place.col + 1ull < length) && (here >= 0xd800u) && (here <= 0xdbffu);
    return (CliPlace){args->place.line, args->place.col + (pair ? 2ull : 1ull)};
}

// the place the word left of `place` starts at, and the place past the word right of it: blanks skipped, then a run of
// a word's letters or of other letters
EMBED_INLINE CliPlace view_word_leave(const ViewCtx *args)
{
    if (args->place.col == 0ull)
    {
        return view_leave(args);
    }
    unsigned long long col = args->place.col;
    while ((col > 0ull) && EMBED_CALL(view_blank_unit, ViewCtx,
                                      .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view,
                                                         .line = args->place.line, .col = col - 1ull)))
    {
        col -= 1ull;
    }
    const int word = (col > 0ull) && EMBED_CALL(view_word_unit, ViewCtx,
                                                .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view,
                                                                   .line = args->place.line, .col = col - 1ull));
    while ((col > 0ull) &&
           !EMBED_CALL(view_blank_unit, ViewCtx,
                       .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->place.line,
                                          .col = col - 1ull)) &&
           (EMBED_CALL(view_word_unit, ViewCtx,
                       .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->place.line,
                                          .col = col - 1ull)) == word))
    {
        col -= 1ull;
    }
    return (CliPlace){args->place.line, col};
}

EMBED_INLINE CliPlace view_word_right(const ViewCtx *args)
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
        col += 1ull;
    }
    const int word = (col < length) && EMBED_CALL(view_word_unit, ViewCtx,
                                                  .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view,
                                                                     .line = args->place.line, .col = col));
    while (
        (col < length) &&
        !EMBED_CALL(view_blank_unit, ViewCtx,
                    .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->place.line, .col = col)) &&
        (EMBED_CALL(view_word_unit, ViewCtx,
                    .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->place.line, .col = col)) ==
         word))
    {
        col += 1ull;
    }
    return (CliPlace){args->place.line, col};
}

// the first letter of `line` that is not blank, its end where every letter is
EMBED_INLINE unsigned long long view_first_letter(const ViewCtx *args)
{
    const unsigned long long length = view_length(args);
    unsigned long long col = 0ull;
    while ((col < length) &&
           EMBED_CALL(view_blank_unit, ViewCtx,
                      .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->line, .col = col)))
    {
        col += 1ull;
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
    CliPlace *from = args->from_pointer;
    CliPlace *to = args->to_pointer;
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
    args->view->top = EMBED_CALL(view_shown_up, ViewCtx, .view_read = args->view, .line = args->view->top);
    if (args->view->cursor.line < args->view->top)
    {
        args->view->top = EMBED_CALL(view_shown_up, ViewCtx, .view_read = args->view, .line = args->view->cursor.line);
    }
    else if (EMBED_CALL(view_rows_to, ViewCtx, .view_read = args->view, .top = args->view->top,
                        .line = args->view->cursor.line, .most = rows) >= rows)
    {
        args->view->top = EMBED_CALL(view_up, ViewCtx, .view_read = args->view, .line = args->view->cursor.line,
                                     .count = rows - 1ull);
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
        (args->by < 0ll) ? EMBED_CALL(view_up, ViewCtx, .view_read = args->view, .line = args->view->cursor.line,
                                      .count = (unsigned long long)(-args->by))
                         : EMBED_CALL(view_down, ViewCtx, .view_read = args->view, .line = args->view->cursor.line,
                                      .count = (unsigned long long)args->by);
    EMBED_CALL(view_move, ViewCtx, .view = args->view,
               .place = (CliPlace){line, EMBED_CALL(view_col_at, ViewCtx, .view = args->view, .line = line,
                                                    .cell = args->view->goal)},
               .extend = args->extend, .saved = 1);
}

// the query's cell, taken at its most as its first units come, given `more` units past its length: 1, or 0 where it
// could not be taken or they do not fit
EMBED_INLINE int view_query_room(const ViewCtx *args)
{
    if (args->view->find_room == 0ull)
    {
        args->view->find_query = g_cli_cells->persistent_buf_alloc(
            CLI_VIEW_FIND_UNITS * sizeof(*args->view->find_query), &g_cli_views_cells);
        args->view->find_room = (args->view->find_query != NULL) ? CLI_VIEW_FIND_UNITS : 0ull;
    }
    return (args->view->find_length + args->more) <= args->view->find_room;
}

// `count` units put into the query at its cursor, which moves past them; and the units from `from` to before `to`
// taken out of it, the cursor at `from`; each makes the matches stale
EMBED_INLINE void view_query_put(const ViewCtx *args)
{
    if ((args->count == 0ull) || !EMBED_CALL(view_query_room, ViewCtx, .view = args->view, .more = args->count))
    {
        return;
    }
    for (unsigned long long at = args->view->find_length; at > args->view->find_at; at -= 1ull)
    {
        args->view->find_query[at + args->count - 1ull] = args->view->find_query[at - 1ull];
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
    for (unsigned long long at = args->to_u64; at < args->view->find_length; at += 1ull)
    {
        args->view->find_query[args->from_u64 + at - args->to_u64] = args->view->find_query[at];
    }
    args->view->find_length -= args->to_u64 - args->from_u64;
    args->view->find_at = args->from_u64;
    args->view->find_stale = 1;
}

// a match found saved among the matches, where they have room
static void view_match_save(const CliFound3Cfg *args)
{
    CliView *const view = args->context;
    if (view->matches >= view->match_room)
    {
        return;
    }
    view->match[view->matches][0] = args->line;
    view->match[view->matches][1] = args->from;
    view->match[view->matches][2] = args->to;
    view->matches += 1ull;
}

// the matches of a paged text read off `size` bytes of the `[[line, from, to], ...]` the pages' find writes
EMBED_INLINE void view_matches_read(const ViewCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long number[3] = {0ull, 0ull, 0ull};
    unsigned int part = 0u;
    for (unsigned long long at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text,
                                            .at = 0ull, .size = size, .set = "0123456789");
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
    CliPlace from;
    CliPlace to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    args->view->current = (args->view->matches != 0ull) ? 0ll : -1ll;
    for (unsigned long long at = 0ull; at < args->view->matches; at += 1ull)
    {
        const CliPlace start = {args->view->match[at][0], args->view->match[at][1]};
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
    args->view->matches = 0ull;
    if (args->view->match == NULL)
    {
        args->view->match =
            g_cli_cells->persistent_buf_alloc(CLI_VIEW_MATCHES * sizeof(*args->view->match), &g_cli_views_cells);
        args->view->match_room = (args->view->match != NULL) ? CLI_VIEW_MATCHES : 0ull;
    }
    args->view->current = -1ll;
    if (args->view->find_length == 0ull)
    {
        return;
    }
    const unsigned long long query_room = (args->view->find_length * 4ull) + 1ull;
    char *const query = g_cli_cells->temporary_buf_alloc(query_room, &g_cli_views_cells);
    if (query == NULL)
    {
        return;
    }
    VerbaLinea line = {query, query_room, 0ull};
    EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &line,
               .units = args->view->find_query, .count = args->view->find_length);
    (void)mmgr_line_finish(&line);
    const CliDocSought sought = {query, args->view->find_regex, args->view->find_case, args->view->find_word};
    char why_text[256];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    if (args->view->paged)
    {
        const unsigned long long room = CLI_VIEW_MATCHES * 24ull;
        char *const cell = g_cli_cells->temporary_buf_alloc(room, &g_cli_views_cells);
        if (cell != NULL)
        {
            VerbaLinea out = {cell, room, 0ull};
            if (EMBED_CALL(cli_pages_out_err_msg_sought.find, CliPagesOutErrMsgSoughtCfg, .key = args->view->key,
                           .sought = &sought, .most = CLI_VIEW_MATCHES, .out = &out, .err_msg = &why) &&
                mmgr_line_ok(&out))
            {
                EMBED_CALL(view_matches_read, ViewCtx, .view = args->view, .text = cell, .size = out.at);
            }
            g_cli_cells->persistent_buf_release(cell, &g_cli_views_cells);
        }
    }
    else
    {
        const CliStoredText *const one =
            EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = args->view->key);
        if (one != NULL)
        {
            (void)EMBED_CALL(cli_document_lines.find_each, CliDocumentLinesCfg, .lines = &one->lines, .sought = &sought,
                             .most = CLI_VIEW_MATCHES, .each = view_match_save, .context = args->view, .err_msg = &why);
        }
    }
    g_cli_cells->persistent_buf_release(query, &g_cli_views_cells);
    view_current_settle(args);
}

// the match a step from the selection selected, the current one: the first that starts after the selection's start
// going forward, the last that starts before it going back, the first or the last around the ends
EMBED_INLINE void view_find_step(const ViewCtx *args)
{
    view_search(args);
    if (args->view->matches == 0ull)
    {
        return;
    }
    CliPlace from;
    CliPlace to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    unsigned long long chosen = args->forward ? 0ull : args->view->matches - 1ull;
    for (unsigned long long at = 0ull; args->forward && (at < args->view->matches); at += 1ull)
    {
        const CliPlace start = {args->view->match[at][0], args->view->match[at][1]};
        if (EMBED_CALL(view_before, ViewCtx, .one = from, .other = start))
        {
            chosen = at;
            break;
        }
    }
    for (unsigned long long at = args->view->matches; !args->forward && (at > 0ull); at -= 1ull)
    {
        const CliPlace start = {args->view->match[at - 1ull][0], args->view->match[at - 1ull][1]};
        if (EMBED_CALL(view_before, ViewCtx, .one = start, .other = from))
        {
            chosen = at - 1ull;
            break;
        }
    }
    // the matches are fewer than CLI_VIEW_MATCHES, which a long long counts
    args->view->current = (long long)chosen;
    args->view->anchor = (CliPlace){args->view->match[chosen][0], args->view->match[chosen][1]};
    EMBED_CALL(view_move, ViewCtx, .view = args->view,
               .place = (CliPlace){args->view->match[chosen][0], args->view->match[chosen][2]}, .extend = 1,
               .saved = 0);
}

// find opened, the keys its own, a selection on one line its query; and closed
EMBED_INLINE void view_find_open(const ViewCtx *args)
{
    args->view->find_open = 1;
    args->view->find_focus = 1;
    CliPlace from;
    CliPlace to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    if ((from.line == to.line) && (from.col < to.col))
    {
        args->view->find_length = 0ull;
        args->view->find_at = 0ull;
        const unsigned long long length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = from.line);
        const unsigned long long end = (to.col < length) ? to.col : length;
        EMBED_CALL(view_query_put, ViewCtx, .view = args->view, .units_read = args->view->line_units + from.col,
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

// a key taken by find where its bar is open or the key opens it: Mod+F opens it or gives it the keys, F3 seeks the
// query again, Escape closes it, and with the keys its own Enter seeks the next match, Shift+Enter the one before,
// Left, Right, Home, End, Backspace and Delete edit the query, and Alt+C, Alt+W and Alt+R turn its case, whole words
// and pattern on and off: 1 where the key was find's
EMBED_INLINE int view_find_key(const ViewCtx *args)
{
    if (args->mod && !args->alt && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "F"))
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
    if (!args->mod && !args->alt && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "F3"))
    {
        if (args->view->find_length != 0ull)
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
    if (!args->mod && !args->alt && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Escape"))
    {
        view_find_close(args);
        return 1;
    }
    if (args->alt && !args->mod &&
        (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "C") ||
         EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "W") ||
         EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "R")))
    {
        args->view->find_case = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "C")
                                    ? !args->view->find_case
                                    : args->view->find_case;
        args->view->find_word = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "W")
                                    ? !args->view->find_word
                                    : args->view->find_word;
        args->view->find_regex = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "R")
                                     ? !args->view->find_regex
                                     : args->view->find_regex;
        args->view->find_stale = 1;
        return 1;
    }
    if (!args->view->find_focus || args->mod || args->alt)
    {
        return 0;
    }
    if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Enter"))
    {
        EMBED_CALL(view_find_step, ViewCtx, .view = args->view, .forward = !args->shift);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Left"))
    {
        args->view->find_at -= (args->view->find_at > 0ull) ? 1ull : 0ull;
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Right"))
    {
        args->view->find_at += (args->view->find_at < args->view->find_length) ? 1ull : 0ull;
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Home"))
    {
        args->view->find_at = 0ull;
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "End"))
    {
        args->view->find_at = args->view->find_length;
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Backspace"))
    {
        if (args->view->find_at > 0ull)
        {
            EMBED_CALL(view_query_cut, ViewCtx, .view = args->view, .from_u64 = args->view->find_at - 1ull,
                       .to_u64 = args->view->find_at);
        }
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Delete"))
    {
        if (args->view->find_at < args->view->find_length)
        {
            EMBED_CALL(view_query_cut, ViewCtx, .view = args->view, .from_u64 = args->view->find_at,
                       .to_u64 = args->view->find_at + 1ull);
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
    CliViewFoldSought sought = {args->line, 0ull, 0ull, 0};
    (void)EMBED_CALL(cli_document_context.folds_each, CliDocumentContextCfg, .key = args->view->key,
                     .each_fold_each = args->exact ? view_fold_match : view_fold_innermost, .context = &sought);
    if (!sought.found ||
        !EMBED_CALL(view_fold_put, ViewCtx, .view = args->view, .first = sought.first, .last = sought.last))
    {
        return 0;
    }
    if ((args->view->cursor.line == sought.first) ||
        (EMBED_CALL(view_hider, ViewCtx, .view_read = args->view, .line = args->view->cursor.line) != NULL))
    {
        EMBED_CALL(view_move, ViewCtx, .view = args->view,
                   .place = (CliPlace){sought.first,
                                       EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = sought.first)},
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
        VerbaLinea shape = {shape_text, sizeof(shape_text), 0ull};
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
    unsigned long long bytes = 0ull;
    for (unsigned long long line = args->from.line; line <= args->to.line; line += 1ull)
    {
        const unsigned long long length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = line);
        const unsigned long long start = (line == args->from.line) ? args->from.col : 0ull;
        const unsigned long long end = ((line == args->to.line) && (args->to.col < length)) ? args->to.col : length;
        for (unsigned long long at = start; at < end; at += 1ull)
        {
            const unsigned int unit = args->view->line_units[at];
            bytes += (unit < 0x80u) ? 1ull : ((unit < 0x800u) ? 2ull : 3ull);
        }
        bytes += (line < args->to.line) ? 1ull : 0ull;
    }
    return bytes;
}

// the place `bytes` bytes of UTF-8 `text` end at when they begin at `from`
EMBED_INLINE CliPlace view_place_past(const ViewCtx *args)
{
    const char *text = args->text;
    CliPlace place = args->from;
    unsigned long long start = 0ull;
    for (unsigned long long at = 0ull; at < args->bytes; at += 1ull)
    {
        if (text[at] == '\n')
        {
            place.line += 1ull;
            start = at + 1ull;
        }
    }
    const unsigned long long units =
        EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = text + start, .size = args->bytes - start);
    place.col = (place.line == args->from.line) ? args->from.col + units : units;
    return place;
}

EMBED_INLINE void view_span_write(const ViewCtx *args);

// one cell for an edit of the span from `from` to `to`, the exact size it takes: the edit, room for the text to put in,
// `put_bytes` bytes and `put_spans` times what the span holds, and the text the span holds read in after that room: the
// cell, or NULL with why given
EMBED_INLINE CliViewEdit *view_edit_cell(const ViewCtx *args)
{
    const unsigned long long took_room = view_span_bytes(args);
    const unsigned long long put_room = args->put_bytes + (args->put_spans * took_room);
    CliViewEdit *const made =
        g_cli_cells->persistent_buf_alloc(sizeof(CliViewEdit) + put_room + 1ull + took_room + 1ull, &g_cli_views_cells);
    if (made == NULL)
    {
        mmgr_line_put(args->err_msg, "the edit's cell could not be taken");
        return NULL;
    }
    char *const cell = (char *)(made + 1);
    VerbaLinea took = {cell + put_room + 1ull, took_room + 1ull, 0ull};
    EMBED_CALL(view_span_write, ViewCtx, .view = args->view, .from = args->from, .to = args->to, .out = &took);
    const unsigned long long took_length = mmgr_line_finish(&took);
    *made = (CliViewEdit){args->from, args->to, args->from, cell, 0ull, took.out, took_length, NULL, NULL};
    return made;
}

// the edit in its cell written to the text, `put_length` bytes at its `put`, and recorded in a step of `kind`, joined
// to the last step where it is of the same kind and came within CLI_VIEW_JOINED milliseconds; the cursor and the anchor
// put after the text written: 1, or 0 with why given and the cell given back
static int view_edit_cell_write(const ViewCtx *args)
{
    unsigned int kind = args->kind;
    // the edit made, a paged text's through the pages, and the line cell read anew after it
    const CliEdit edit = {args->made->from, args->made->to, args->made->put, args->made->put_length};
    const CliEdit *const edits[1] = {&edit};
    const unsigned int counts[1] = {1u};
    args->view->line_known = 0;
    if (!EMBED_CALL(view_write, ViewCtx, .view = args->view, .edits = edits, .counts = counts,
                    .err_msg = args->err_msg))
    {
        g_cli_cells->persistent_buf_release(args->made, &g_cli_views_cells);
        return 0;
    }
    args->made->end = EMBED_CALL(view_place_past, ViewCtx, .from = args->made->from, .text = args->made->put,
                                 .bytes = args->made->put_length);
    // the step the edit joins or begins
    const unsigned long long now = cli_clock_milliseconds();
    EMBED_CALL(view_steps_cut, ViewCtx, .view = args->view, .from_u32 = args->view->done);
    CliViewStep *step = (args->view->steps != 0u) ? &args->view->step[args->view->steps - 1u] : NULL;
    const int joined = (step != NULL) && (kind != CLI_VIEW_KIND_NONE) && (step->kind == kind) &&
                       (now - step->at_ms <= CLI_VIEW_JOINED) && (step->edits < CLI_VIEW_STEP_EDITS);
    if (!joined)
    {
        if (args->view->steps_room == 0u)
        {
            args->view->step =
                g_cli_cells->persistent_buf_alloc(CLI_VIEW_STEPS * sizeof(*args->view->step), &g_cli_views_cells);
            if (args->view->step == NULL)
            {
                g_cli_cells->persistent_buf_release(args->made, &g_cli_views_cells);
                mmgr_line_put(args->err_msg, "the steps' cell could not be taken");
                return 0;
            }
            args->view->steps_room = CLI_VIEW_STEPS;
        }
        // the steps full, the oldest goes and those after it come down
        if (args->view->steps == args->view->steps_room)
        {
            EMBED_CALL(view_step_free, ViewCtx, .step = &args->view->step[0]);
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
                   .put_bytes = args->length, .put_spans = 0ull, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = made->put, .src = args->text, .bytes = args->length});
    made->put[args->length] = '\0';
    made->put_length = args->length;
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
    CliViewStep *const step = &args->view->step[args->view->done - 1u];
    for (const CliViewEdit *edit = step->last; edit != NULL; edit = edit->before)
    {
        const CliEdit back = {edit->from, edit->end, edit->took, edit->took_length};
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
    CliViewStep *const step = &args->view->step[args->view->done];
    for (const CliViewEdit *edit = step->first; edit != NULL; edit = edit->after)
    {
        const CliEdit again = {edit->from, edit->to, edit->put, edit->put_length};
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
    CliPlace from;
    CliPlace to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    return EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = from, .to = to, .text = args->text,
                      .length = args->length, .kind = args->kind, .err_msg = args->err_msg);
}

// the cursor and the anchor put where an edit leaves them, the last step done recording them so redo puts them back
EMBED_INLINE void view_settle(const ViewCtx *args)
{
    CliPlace cursor = args->cursor;
    CliPlace anchor = args->anchor;
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
    for (unsigned long long at = 0ull; at < args->col; at += 1ull)
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
    unsigned char *const cell = g_cli_cells->temporary_buf_alloc(
        sizeof(CliRegexpRoom) + ((count + 1ull) * sizeof(unsigned int)), &g_cli_views_cells);
    if (cell == NULL)
    {
        return 0;
    }
    unsigned int *const letters = (unsigned int *)(void *)(cell + sizeof(CliRegexpRoom));
    // the units read as letters, a surrogate pair one letter
    unsigned long long read = 0ull;
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        const unsigned int unit = args->view->line_units[at];
        const int pair = (unit >= 0xd800u) && (unit <= 0xdbffu) && (at + 1ull < count);
        letters[read] =
            pair ? (0x10000u + ((unit - 0xd800u) << 10u) + (args->view->line_units[at + 1ull] - 0xdc00u)) : unit;
        at += pair ? 1ull : 0ull;
        read += 1ull;
    }
    char why_text[256];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    CliRegexp regexp;
    EMBED_CALL(cli_regexp_room.room, CliRegexpRoomCfg, .regexp_out = &regexp, .room = (CliRegexpRoom *)(void *)cell);
    int ok = EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = &regexp,
                        .source = args->view->indent_after, .flags = "u", .err_msg = &why);
    if (!ok)
    {
        why.at = 0ull;
        EMBED_CALL(cli_regexp_room.room, CliRegexpRoomCfg, .regexp_out = &regexp,
                   .room = (CliRegexpRoom *)(void *)cell);
        ok = EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = &regexp,
                        .source = args->view->indent_after, .flags = "", .err_msg = &why);
    }
    int matched = 0;
    for (unsigned long long at = 0ull; ok && !matched && (at <= read); at += 1ull)
    {
        CliRegexpSpan groups[CLI_REGEXP_GROUPS + 1u];
        const int starts = (!regexp.anchored || (at == 0ull)) &&
                           ((at == read) || regexp.first.empty ||
                            EMBED_CALL(cli_regexp.allows, CliRegexpCfg, .first = &regexp.first, .letter = letters[at]));
        matched = starts && EMBED_CALL(cli_regexp_text_u_32s.match_into, CliRegexpTextU32sCfg, .regexp = &regexp,
                                       .text_u32s = letters, .length = read, .at = at, .groups = groups);
    }
    g_cli_cells->persistent_buf_release(cell, &g_cli_views_cells);
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
        for (unsigned long long at = 0ull; at < view_tab(args); at += 1ull)
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
    CliPlace from;
    CliPlace to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    const unsigned long long lead = EMBED_CALL(view_first_letter, ViewCtx, .view = args->view, .line = from.line);
    const unsigned long long saved = (lead < from.col) ? lead : from.col;
    const unsigned int open =
        EMBED_CALL(view_open_before, ViewCtx, .view = args->view, .line = from.line, .col = from.col);
    const int deeper =
        (open != 0u) || EMBED_CALL(view_opens_block, ViewCtx, .view = args->view, .line = from.line, .col = from.col);
    unsigned long long before = from.col;
    while ((before > 0ull) && EMBED_CALL(view_blank_unit, ViewCtx,
                                         .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line,
                                                            .col = before - 1ull)))
    {
        before -= 1ull;
    }
    const unsigned int prev =
        (before > 0ull) ? EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line, .col = before - 1ull)
                        : 0u;
    const unsigned long long rest_length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = to.line);
    unsigned long long gap = 0ull;
    while ((to.col + gap < rest_length) &&
           EMBED_CALL(view_blank_unit, ViewCtx,
                      .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = to.line, .col = to.col + gap)))
    {
        gap += 1ull;
    }
    const unsigned int next = (to.col + gap < rest_length) ? EMBED_CALL(view_unit, ViewCtx, .view = args->view,
                                                                        .line = to.line, .col = to.col + gap)
                                                           : 0u;
    const int paired =
        ((prev != 0u) && (next != 0u) &&
         EMBED_CALL(view_on_pair, ViewCtx, .view_read = args->view, .open = prev, .close = next, .quoted = 0)) ||
        ((open != 0u) && (next == open));
    char text[CLI_PATH_MAX_LEN];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_ch(&line, '\n');
    for (unsigned long long at = 0ull; (at < saved) && (line.at + 4ull < sizeof(text)); at += 1ull)
    {
        EMBED_CALL(cli_line_letter_internal.utf8_put, CliLineLetterCtx, .line = &line,
                   .letter = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line, .col = at));
    }
    const unsigned long long lead_bytes = line.at - 1ull;
    unsigned long long step_bytes = 0ull;
    if (deeper)
    {
        step_bytes = EMBED_CALL(view_step_write, ViewCtx, .view_read = args->view, .out = &line);
    }
    if (paired && deeper)
    {
        const unsigned long long cursor_col =
            EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = text + 1, .size = lead_bytes + step_bytes);
        mmgr_line_ch(&line, '\n');
        mmgr_line_put_n(&line, text + 1, lead_bytes);
        if (!EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = from, .to = (CliPlace){to.line, to.col + gap},
                        .text = text, .length = line.at, .kind = CLI_VIEW_KIND_NONE, .err_msg = args->err_msg))
        {
            return 0;
        }
        const CliPlace between = {from.line + 1ull, cursor_col};
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
        return EMBED_CALL(view_type, ViewCtx, .view = args->view, .text = "", .length = 0ull,
                          .kind = CLI_VIEW_KIND_DELETE, .err_msg = args->err_msg);
    }
    const CliPlace at = args->view->cursor;
    if (at.col == 0ull)
    {
        return (at.line == 0ull)
                   ? 1
                   : EMBED_CALL(view_edit, ViewCtx, .view = args->view,
                                .from = EMBED_CALL(view_leave, ViewCtx, .view = args->view, .place = at), .to = at,
                                .text = "", .length = 0ull, .kind = CLI_VIEW_KIND_DELETE, .err_msg = args->err_msg);
    }
    if ((at.col < EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = at.line)) &&
        EMBED_CALL(view_on_pair, ViewCtx, .view_read = args->view,
                   .open = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = at.line, .col = at.col - 1ull),
                   .close = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = at.line, .col = at.col),
                   .quoted = 1))
    {
        return EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = (CliPlace){at.line, at.col - 1ull},
                          .to = (CliPlace){at.line, at.col + 1ull}, .text = "", .length = 0ull,
                          .kind = CLI_VIEW_KIND_DELETE, .err_msg = args->err_msg);
    }
    if (!args->view->indent.tabs || (args->view->indent.tabs == -1))
    {
        unsigned long long col = 0ull;
        while ((col < at.col) &&
               (EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = at.line, .col = col) == ' '))
        {
            col += 1ull;
        }
        if (col == at.col)
        {
            const unsigned long long step = EMBED_CALL(view_tab, ViewCtx, .view_read = args->view);
            const unsigned long long back = (at.col % step != 0ull) ? at.col % step : step;
            return EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = (CliPlace){at.line, at.col - back},
                              .to = at, .text = "", .length = 0ull, .kind = CLI_VIEW_KIND_DELETE,
                              .err_msg = args->err_msg);
        }
    }
    return EMBED_CALL(view_edit, ViewCtx, .view = args->view,
                      .from = EMBED_CALL(view_leave, ViewCtx, .view = args->view, .place = at), .to = at, .text = "",
                      .length = 0ull, .kind = CLI_VIEW_KIND_DELETE, .err_msg = args->err_msg);
}

// the letter at the cursor taken out, or the selection; at a line's end the next line joined to it
EMBED_INLINE int view_delete(const ViewCtx *args)
{
    if (EMBED_CALL(view_select, ViewCtx, .view_read = args->view))
    {
        return EMBED_CALL(view_type, ViewCtx, .view = args->view, .text = "", .length = 0ull,
                          .kind = CLI_VIEW_KIND_DELETE_FORWARD, .err_msg = args->err_msg);
    }
    const CliPlace at = args->view->cursor;
    const CliPlace next = EMBED_CALL(view_right, ViewCtx, .view = args->view, .place = at);
    if ((next.line == at.line) && (next.col == at.col))
    {
        return 1;
    }
    return EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = at, .to = next, .text = "", .length = 0ull,
                      .kind = CLI_VIEW_KIND_DELETE_FORWARD, .err_msg = args->err_msg);
}

// a tab typed: an indentation's step at the cursor, spaces to the next stop or a tab
EMBED_INLINE int view_on_tab(const ViewCtx *args)
{
    if (args->view->indent.tabs == 1)
    {
        return EMBED_CALL(view_type, ViewCtx, .view = args->view, .text = "\t", .length = 1ull,
                          .kind = CLI_VIEW_KIND_SPACE, .err_msg = args->err_msg);
    }
    const unsigned long long step = EMBED_CALL(view_tab, ViewCtx, .view_read = args->view);
    CliPlace from;
    CliPlace to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    const unsigned long long cell =
        EMBED_CALL(view_cell_get, ViewCtx, .view = args->view, .line = from.line, .col = from.col);
    const unsigned long long spaces = step - (cell % step);
    char text[64];
    const unsigned long long count = (spaces < sizeof(text)) ? spaces : sizeof(text) - 1ull;
    memor.set(&(MemoriaCfg){.dst = text, .val = ' ', .bytes = count});
    return EMBED_CALL(view_type, ViewCtx, .view = args->view, .text = text, .length = count,
                      .kind = CLI_VIEW_KIND_SPACE, .err_msg = args->err_msg);
}

// the span from `from` to `to` of the text written into `out` as UTF-8, each line but the last followed by "\n"
EMBED_INLINE void view_span_write(const ViewCtx *args)
{
    VerbaLinea *out = args->out;
    for (unsigned long long line = args->from.line; line <= args->to.line; line += 1ull)
    {
        const unsigned long long line_length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = line);
        const unsigned long long start = (line == args->from.line) ? args->from.col : 0ull;
        const unsigned long long end =
            (line == args->to.line) ? ((args->to.col < line_length) ? args->to.col : line_length) : line_length;
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

// one letter typed, `length` bytes of `text` as the unit `letter`: a closer typed before itself steps over it; an
// opener typed around a selection wraps it, the selection saved inside, and typed alone brings its closer where what
// follows allows, the cursor between them, a quote where what comes before allows too; a closing bracket typed on a
// lead of blanks takes the lead back a step; any other letter is written as typed
EMBED_INLINE int view_letter_type(const ViewCtx *args)
{
    unsigned int letter = args->letter;
    CliPlace from;
    CliPlace to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    const int empty = !EMBED_CALL(view_select, ViewCtx, .view_read = args->view);
    const unsigned long long to_length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = to.line);
    const unsigned int next =
        (to.col < to_length) ? EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = to.line, .col = to.col) : 0u;
    const unsigned int prev = (from.col > 0ull) ? EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line,
                                                             .col = from.col - 1ull)
                                                : 0u;
    if (empty && EMBED_CALL(view_closes, ViewCtx, .view_read = args->view, .unit = letter) && (next == letter))
    {
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = (CliPlace){to.line, to.col + 1ull}, .extend = 0,
                   .saved = 0);
        return 1;
    }
    const unsigned int close = EMBED_CALL(view_closer_get, ViewCtx, .view_read = args->view, .unit = letter);
    if ((close != 0u) && !empty)
    {
        unsigned long long room = 0ull;
        for (unsigned long long line = from.line; line <= to.line; line += 1ull)
        {
            room += (EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = line) * 4ull) + 1ull;
        }
        char *const cell = g_cli_cells->temporary_buf_alloc(room + args->length + 8ull, &g_cli_views_cells);
        if (cell == NULL)
        {
            mmgr_line_put(args->err_msg, "the wrap's cell could not be taken");
            return 0;
        }
        VerbaLinea wrapped = {cell, room + args->length + 8ull, 0ull};
        mmgr_line_put_n(&wrapped, args->text, args->length);
        EMBED_CALL(view_span_write, ViewCtx, .view = args->view, .from = from, .to = to, .out = &wrapped);
        EMBED_CALL(cli_line_letter_internal.utf8_put, CliLineLetterCtx, .line = &wrapped, .letter = close);
        const int written = EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = from, .to = to, .text = cell,
                                       .length = wrapped.at, .kind = CLI_VIEW_KIND_NONE, .err_msg = args->err_msg);
        g_cli_cells->persistent_buf_release(cell, &g_cli_views_cells);
        if (!written)
        {
            return 0;
        }
        const CliPlace end = args->view->cursor;
        EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = (CliPlace){end.line, end.col - 1ull},
                   .anchor = (CliPlace){from.line, from.col + 1ull});
        return 1;
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
            VerbaLinea line = {both, sizeof(both), 0ull};
            mmgr_line_put_n(&line, args->text, args->length);
            EMBED_CALL(cli_line_letter_internal.utf8_put, CliLineLetterCtx, .line = &line, .letter = close);
            if (!EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = from, .to = to, .text = both,
                            .length = line.at, .kind = args->kind, .err_msg = args->err_msg))
            {
                return 0;
            }
            const CliPlace end = args->view->cursor;
            const CliPlace between = {end.line, end.col - 1ull};
            EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = between, .anchor = between);
            return 1;
        }
    }
    if (EMBED_CALL(view_closes_bracket, ViewCtx, .view_read = args->view, .unit = letter) && empty &&
        (from.col > 0ull) &&
        (EMBED_CALL(view_first_letter, ViewCtx, .view = args->view, .line = from.line) >= from.col))
    {
        // the lead less a step: a tab, the step's spaces where the lead ends in them, else the run of blanks it ends in
        unsigned long long less = from.col;
        const unsigned long long step = EMBED_CALL(view_tab, ViewCtx, .view_read = args->view);
        if (EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line, .col = less - 1ull) == '\t')
        {
            less -= 1ull;
        }
        else
        {
            unsigned long long spaces = 0ull;
            while ((spaces < less) && (EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line,
                                                  .col = less - 1ull - spaces) == ' '))
            {
                spaces += 1ull;
            }
            less -= (spaces >= step) ? step : spaces;
        }
        char lead[CLI_PATH_MAX_LEN];
        VerbaLinea line = {lead, sizeof(lead), 0ull};
        for (unsigned long long at = 0ull; (at < less) && (line.at + 4ull < sizeof(lead)); at += 1ull)
        {
            EMBED_CALL(cli_line_letter_internal.utf8_put, CliLineLetterCtx, .line = &line,
                       .letter = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = from.line, .col = at));
        }
        mmgr_line_put_n(&line, args->text, args->length);
        return EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = (CliPlace){from.line, 0ull}, .to = to,
                          .text = lead, .length = line.at, .kind = args->kind, .err_msg = args->err_msg);
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
    CliPlace from;
    CliPlace to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view_read, .from_pointer = &from, .to_pointer = &to);
    *first = from.line;
    *last = ((to.line > from.line) && (to.col == 0ull)) ? to.line - 1ull : to.line;
}

// a place carried through the edit of the span from `from` to `to` by text ending at `end`: before it it stays, inside
// it it goes to the span's start, or with `after` to the text's end, and past it it moves with the span's end; a place
// at the span's start goes on only with `after`
EMBED_INLINE CliPlace view_place_carry(const ViewCtx *args)
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
        return (CliPlace){args->end.line, args->end.col + args->place.col - args->to.col};
    }
    return (CliPlace){args->place.line + args->end.line - args->to.line, args->place.col};
}

// the edit cut to the whole lines it changes, those it leaves as they stand taken off its head and its foot: the text
// is one array, an edit moving all that lies after it, and the smaller the span the less the line tables and the
// walk redo: 1, or 0 with its cell given back where it changes nothing
static int view_edit_trim(const ViewCtx *args)
{
    const unsigned long long shorter =
        (args->made->put_length < args->made->took_length) ? args->made->put_length : args->made->took_length;
    unsigned long long head = 0ull;
    unsigned long long head_lines = 0ull;
    unsigned long long same = 0ull;
    while ((same < shorter) && (args->made->put[same] == args->made->took[same]))
    {
        if (args->made->put[same] == '\n')
        {
            head_lines += 1ull;
            head = same + 1ull;
        }
        same += 1ull;
    }
    if ((same == shorter) && (args->made->put_length == args->made->took_length))
    {
        g_cli_cells->persistent_buf_release(args->made, &g_cli_views_cells);
        return 0;
    }
    args->made->put += head;
    args->made->took += head;
    args->made->put_length -= head;
    args->made->took_length -= head;
    if (head_lines != 0ull)
    {
        args->made->from = (CliPlace){args->made->from.line + head_lines, 0ull};
    }
    const unsigned long long rest =
        (args->made->put_length < args->made->took_length) ? args->made->put_length : args->made->took_length;
    unsigned long long foot = 0ull;
    unsigned long long foot_lines = 0ull;
    unsigned long long back = 0ull;
    while ((back < rest) && (args->made->put[args->made->put_length - 1ull - back] ==
                             args->made->took[args->made->took_length - 1ull - back]))
    {
        if (args->made->put[args->made->put_length - 1ull - back] == '\n')
        {
            foot_lines += 1ull;
            foot = back + 1ull;
        }
        back += 1ull;
    }
    args->made->put_length -= foot;
    args->made->took_length -= foot;
    if (foot_lines != 0ull)
    {
        const unsigned long long line = args->made->to.line - foot_lines;
        args->made->to = (CliPlace){line, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = line)};
    }
    return 1;
}

// the edit its cell holds written as a step of its own, cut to the lines it changes, the selection carried through the
// span it covers whole: the cursor to the text's end where it stood in the span, and the anchor to its start, or its
// end where the anchor stood after the cursor
EMBED_INLINE int view_cell_replace(const ViewCtx *args)
{
    const CliPlace from = args->made->from;
    const CliPlace to = args->made->to;
    const CliPlace end =
        EMBED_CALL(view_place_past, ViewCtx, .from = from, .text = args->made->put, .bytes = args->made->put_length);
    const CliPlace cursor = args->view->cursor;
    const CliPlace anchor = args->view->anchor;
    const int after = EMBED_CALL(view_select, ViewCtx, .view_read = args->view) &&
                      EMBED_CALL(view_before, ViewCtx, .one = cursor, .other = anchor);
    const CliPlace cursor_carried =
        EMBED_CALL(view_place_carry, ViewCtx, .place = cursor, .from = from, .to = to, .end = end, .after = 1);
    const CliPlace anchor_carried =
        EMBED_CALL(view_place_carry, ViewCtx, .place = anchor, .from = from, .to = to, .end = end, .after = after);
    if (!view_edit_trim(args))
    {
        args->view->anchor = anchor_carried;
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = cursor_carried, .extend = 1, .saved = 0);
        return 1;
    }
    if (!EMBED_CALL(view_edit_cell_write, ViewCtx, .view = args->view, .made = args->made, .kind = CLI_VIEW_KIND_NONE,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = cursor_carried, .anchor = anchor_carried);
    return 1;
}

// the selection's lines joined into one, and a cursor's line with the line after it: the space where two lines meet
// made one space, or none where either side is blank
static int view_lines_join(const ViewCtx *args)
{
    CliPlace from;
    CliPlace to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    const unsigned long long count = EMBED_CALL(view_lines, ViewCtx, .view_read = args->view);
    const unsigned long long first = from.line;
    const unsigned long long wanted = (to.line > first + 1ull) ? to.line : first + 1ull;
    const unsigned long long last = ((count != 0ull) && (wanted > count - 1ull)) ? count - 1ull : wanted;
    if (last == first)
    {
        return 1;
    }
    CliViewEdit *const made =
        EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = (CliPlace){first, 0ull},
                   .to = (CliPlace){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)},
                   .put_bytes = 0ull, .put_spans = 1ull, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    // a line's break gives way to the one space, so the joined text never outgrows what was read
    const char *const took = made->took;
    const unsigned long long size = made->took_length;
    char *const put = made->put;
    unsigned long long at = 0ull;
    unsigned long long read = 0ull;
    while ((read < size) && (took[read] != '\n'))
    {
        put[at] = took[read];
        at += 1ull;
        read += 1ull;
    }
    while (read < size)
    {
        read += 1ull;
        while ((read < size) && ((took[read] == ' ') || (took[read] == '\t')))
        {
            read += 1ull;
        }
        while ((at > 0ull) && ((put[at - 1ull] == ' ') || (put[at - 1ull] == '\t')))
        {
            at -= 1ull;
        }
        if ((at != 0ull) && (read < size) && (took[read] != '\n'))
        {
            put[at] = ' ';
            at += 1ull;
        }
        while ((read < size) && (took[read] != '\n'))
        {
            put[at] = took[read];
            at += 1ull;
            read += 1ull;
        }
    }
    made->put_length = at;
    return EMBED_CALL(view_cell_replace, ViewCtx, .view = args->view, .made = made, .err_msg = args->err_msg);
}

// the selection's lines moved a line up or down over their neighbour, the selection with them
static int view_lines_move(const ViewCtx *args)
{
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    EMBED_CALL(view_lines_cover, ViewCtx, .view_read = args->view, .first_pointer = &first, .last_pointer = &last);
    const unsigned long long count = EMBED_CALL(view_lines, ViewCtx, .view_read = args->view);
    if ((args->up && (first == 0ull)) || (!args->up && (last + 1ull >= count)))
    {
        return 1;
    }
    const unsigned long long from_line = args->up ? first - 1ull : first;
    const unsigned long long to_line = args->up ? last : last + 1ull;
    CliViewEdit *const made =
        EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = (CliPlace){from_line, 0ull},
                   .to = (CliPlace){to_line, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = to_line)},
                   .put_bytes = 0ull, .put_spans = 1ull, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    // the neighbour's line and the block change places: up, the first line goes to the foot; down, the last to the head
    const char *const took = made->took;
    const unsigned long long size = made->took_length;
    unsigned long long split = args->up ? 0ull : size - 1ull;
    while (took[split] != '\n')
    {
        split = args->up ? split + 1ull : split - 1ull;
    }
    const unsigned long long after = size - split - 1ull;
    memor.cpy(&(MemoriaCfg){.dst = made->put, .src = took + split + 1ull, .bytes = after});
    made->put[after] = '\n';
    memor.cpy(&(MemoriaCfg){.dst = made->put + after + 1ull, .src = took, .bytes = split});
    made->put_length = size;
    const CliPlace cursor = {args->up ? args->view->cursor.line - 1ull : args->view->cursor.line + 1ull,
                             args->view->cursor.col};
    const CliPlace anchor = {args->up ? args->view->anchor.line - 1ull : args->view->anchor.line + 1ull,
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
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    EMBED_CALL(view_lines_cover, ViewCtx, .view_read = args->view, .first_pointer = &first, .last_pointer = &last);
    const CliPlace head = {first, 0ull};
    const CliPlace foot = {last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)};
    const unsigned long long bytes = EMBED_CALL(view_span_bytes, ViewCtx, .view = args->view, .from = head, .to = foot);
    const CliPlace place = args->up ? head : foot;
    CliViewEdit *const made = EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = place, .to = place,
                                         .put_bytes = bytes + 1ull, .put_spans = 0ull, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    VerbaLinea copy = {made->put, bytes + 2ull, 0ull};
    if (!args->up)
    {
        mmgr_line_ch(&copy, '\n');
    }
    EMBED_CALL(view_span_write, ViewCtx, .view = args->view, .from = head, .to = foot, .out = &copy);
    if (args->up)
    {
        mmgr_line_ch(&copy, '\n');
    }
    made->put_length = copy.at;
    const unsigned long long by = last - first + 1ull;
    const CliPlace cursor = {args->view->cursor.line + (((args->view->cursor.line > last) || !args->up) ? by : 0ull),
                             args->view->cursor.col};
    const CliPlace anchor = {args->view->anchor.line + (((args->view->anchor.line > last) || !args->up) ? by : 0ull),
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
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    EMBED_CALL(view_lines_cover, ViewCtx, .view_read = args->view, .first_pointer = &first, .last_pointer = &last);
    const unsigned long long count = EMBED_CALL(view_lines, ViewCtx, .view_read = args->view);
    const unsigned long long col = args->view->cursor.col;
    const int follows = last + 1ull < count;
    const CliPlace from =
        (follows || (first == 0ull))
            ? (CliPlace){first, 0ull}
            : (CliPlace){first - 1ull, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = first - 1ull)};
    const CliPlace to = follows ? (CliPlace){last + 1ull, 0ull}
                                : (CliPlace){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)};
    if (!EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = from, .to = to, .text = "", .length = 0ull,
                    .kind = CLI_VIEW_KIND_NONE, .err_msg = args->err_msg))
    {
        return 0;
    }
    const unsigned long long length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = from.line);
    const CliPlace place = {from.line, (col < length) ? col : length};
    EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = place, .anchor = place);
    return 1;
}

// the selection's lines a step of indentation deeper, a blank line among several left as it is, or a step shallower: a
// tab or as many spaces of a step as lead the line; the selection carried, a place at a line's start staying there
// where a selection stands
static int view_lines_indent(const ViewCtx *args)
{
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    EMBED_CALL(view_lines_cover, ViewCtx, .view_read = args->view, .first_pointer = &first, .last_pointer = &last);
    const int tabs = args->view->indent.tabs == 1;
    const unsigned long long stop = EMBED_CALL(view_tab, ViewCtx, .view_read = args->view);
    const unsigned long long step = tabs ? 1ull : stop;
    const unsigned long long lines = last - first + 1ull;
    CliViewEdit *const made =
        EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = (CliPlace){first, 0ull},
                   .to = (CliPlace){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)},
                   .put_bytes = args->deeper ? lines * step : 0ull, .put_spans = 1ull, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    const char *const took = made->took;
    const unsigned long long size = made->took_length;
    char *const put = made->put;
    const int selected = EMBED_CALL(view_select, ViewCtx, .view_read = args->view);
    CliPlace cursor = args->view->cursor;
    CliPlace anchor = args->view->anchor;
    unsigned long long at = 0ull;
    unsigned long long read = 0ull;
    for (unsigned long long line = first; line <= last; line += 1ull)
    {
        unsigned long long end = read;
        int filled = 0;
        while ((end < size) && (took[end] != '\n'))
        {
            filled = filled || ((took[end] != ' ') && (took[end] != '\t'));
            end += 1ull;
        }
        unsigned long long cut = 0ull;
        unsigned long long added = 0ull;
        if (args->deeper)
        {
            added = (filled || (lines == 1ull)) ? step : 0ull;
            memor.set(&(MemoriaCfg){.dst = put + at, .val = tabs ? '\t' : ' ', .bytes = added});
            at += added;
        }
        else if ((read < end) && (took[read] == '\t'))
        {
            cut = 1ull;
        }
        else
        {
            while ((cut < stop) && (read + cut < end) && (took[read + cut] == ' '))
            {
                cut += 1ull;
            }
        }
        memor.cpy(&(MemoriaCfg){.dst = put + at, .src = took + read + cut, .bytes = end - read - cut});
        at += end - read - cut;
        if ((added != 0ull) || (cut != 0ull))
        {
            const CliPlace from = {line, 0ull};
            const CliPlace to = {line, cut};
            const CliPlace ends = {line, added};
            if (args->view->cursor.line == line)
            {
                cursor = EMBED_CALL(view_place_carry, ViewCtx, .place = args->view->cursor, .from = from, .to = to,
                                    .end = ends, .after = (args->view->cursor.col > 0ull) || !selected);
            }
            if (args->view->anchor.line == line)
            {
                anchor = EMBED_CALL(view_place_carry, ViewCtx, .place = args->view->anchor, .from = from, .to = to,
                                    .end = ends, .after = (args->view->anchor.col > 0ull) || !selected);
            }
        }
        if (end < size)
        {
            put[at] = '\n';
            at += 1ull;
        }
        read = end + 1ull;
    }
    made->put_length = at;
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

// the selection's filled lines marked as comments with the language's line mark, at the least indentation among them,
// or where each is marked already the mark and a space after it taken out; the selection carried through each line's
// edit
EMBED_INLINE int view_lines_comment(const ViewCtx *args)
{
    const unsigned long long token =
        cellul.len(&(CatenaFinitaCfg){.src = args->view->comment_line, .cap = sizeof(args->view->comment_line)});
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    EMBED_CALL(view_lines_cover, ViewCtx, .view_read = args->view, .first_pointer = &first, .last_pointer = &last);
    const unsigned long long lines = last - first + 1ull;
    CliViewEdit *const made =
        EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = (CliPlace){first, 0ull},
                   .to = (CliPlace){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)},
                   .put_bytes = lines * (token + 1ull), .put_spans = 1ull, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    const char *const took = made->took;
    const unsigned long long size = made->took_length;
    char *const put = made->put;
    // the filled lines read: whether each begins with the mark past its blanks, and the least of their blanks
    int any = 0;
    int marked = 1;
    unsigned long long lead = ~0ull;
    unsigned long long read = 0ull;
    while (read <= size)
    {
        unsigned long long end = read;
        while ((end < size) && (took[end] != '\n'))
        {
            end += 1ull;
        }
        unsigned long long blanks = read;
        while ((blanks < end) && ((took[blanks] == ' ') || (took[blanks] == '\t')))
        {
            blanks += 1ull;
        }
        if (blanks < end)
        {
            any = 1;
            lead = (blanks - read < lead) ? blanks - read : lead;
            marked = marked && (end - blanks >= token) &&
                     (memor.cmp(
                          &(MemoriaCfg){.src = took + blanks, .other = args->view->comment_line, .bytes = token}) == 0);
        }
        read = end + 1ull;
    }
    if (!any)
    {
        g_cli_cells->persistent_buf_release(made, &g_cli_views_cells);
        return 1;
    }
    const int after = EMBED_CALL(view_select, ViewCtx, .view_read = args->view) &&
                      EMBED_CALL(view_before, ViewCtx, .one = args->view->cursor, .other = args->view->anchor);
    const unsigned long long token_units =
        EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = args->view->comment_line, .size = token);
    CliPlace cursor = args->view->cursor;
    CliPlace anchor = args->view->anchor;
    unsigned long long at = 0ull;
    read = 0ull;
    for (unsigned long long line = first; line <= last; line += 1ull)
    {
        unsigned long long end = read;
        while ((end < size) && (took[end] != '\n'))
        {
            end += 1ull;
        }
        unsigned long long blanks = read;
        while ((blanks < end) && ((took[blanks] == ' ') || (took[blanks] == '\t')))
        {
            blanks += 1ull;
        }
        if (blanks < end)
        {
            // the blanks are a byte a unit: a place among them is their count
            const unsigned long long place = marked ? blanks - read : lead;
            const unsigned long long cut = (marked && (blanks + token < end) && (took[blanks + token] == ' '))
                                               ? token + 1ull
                                               : (marked ? token : 0ull);
            const CliPlace from = {line, place};
            const CliPlace to = {line, place + (marked ? (cut - token) + token_units : 0ull)};
            const CliPlace ends = {line, marked ? place : place + token_units + 1ull};
            if (args->view->cursor.line == line)
            {
                cursor = EMBED_CALL(view_place_carry, ViewCtx, .place = args->view->cursor, .from = from, .to = to,
                                    .end = ends, .after = 1);
            }
            if (args->view->anchor.line == line)
            {
                anchor = EMBED_CALL(view_place_carry, ViewCtx, .place = args->view->anchor, .from = from, .to = to,
                                    .end = ends, .after = after);
            }
            memor.cpy(&(MemoriaCfg){.dst = put + at, .src = took + read, .bytes = place});
            at += place;
            if (!marked)
            {
                memor.cpy(&(MemoriaCfg){.dst = put + at, .src = args->view->comment_line, .bytes = token});
                at += token;
                put[at] = ' ';
                at += 1ull;
            }
            const unsigned long long rest = read + place + cut;
            memor.cpy(&(MemoriaCfg){.dst = put + at, .src = took + rest, .bytes = end - rest});
            at += end - rest;
        }
        else
        {
            memor.cpy(&(MemoriaCfg){.dst = put + at, .src = took + read, .bytes = end - read});
            at += end - read;
        }
        if (end < size)
        {
            put[at] = '\n';
            at += 1ull;
        }
        read = end + 1ull;
    }
    made->put_length = at;
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
    CliPlace from;
    CliPlace to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    const unsigned long long open =
        cellul.len(&(CatenaFinitaCfg){.src = args->view->comment_open, .cap = sizeof(args->view->comment_open)});
    const unsigned long long close =
        cellul.len(&(CatenaFinitaCfg){.src = args->view->comment_close, .cap = sizeof(args->view->comment_close)});
    CliViewEdit *const made = EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = from, .to = to,
                                         .put_bytes = open + close + 2ull, .put_spans = 1ull, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    const char *const took = made->took;
    const unsigned long long size = made->took_length;
    char *const put = made->put;
    const int wrapped =
        (size >= open + close) &&
        (memor.cmp(&(MemoriaCfg){.src = took, .other = args->view->comment_open, .bytes = open}) == 0) &&
        (memor.cmp(&(MemoriaCfg){.src = took + size - close, .other = args->view->comment_close, .bytes = close}) == 0);
    unsigned long long saved_from = 0ull;
    unsigned long long saved_bytes = 0ull;
    if (wrapped)
    {
        unsigned long long inner = open;
        unsigned long long stop = size - close;
        inner += ((inner < stop) && (took[inner] == ' ')) ? 1ull : 0ull;
        stop -= ((inner < stop) && (took[stop - 1ull] == ' ')) ? 1ull : 0ull;
        saved_bytes = stop - inner;
        memor.cpy(&(MemoriaCfg){.dst = put, .src = took + inner, .bytes = saved_bytes});
        made->put_length = saved_bytes;
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = put, .src = args->view->comment_open, .bytes = open});
        put[open] = ' ';
        memor.cpy(&(MemoriaCfg){.dst = put + open + 1ull, .src = took, .bytes = size});
        put[open + 1ull + size] = ' ';
        memor.cpy(&(MemoriaCfg){.dst = put + open + 2ull + size, .src = args->view->comment_close, .bytes = close});
        made->put_length = open + 2ull + size + close;
        saved_from = open + 1ull;
        saved_bytes = size;
    }
    const CliPlace start = EMBED_CALL(view_place_past, ViewCtx, .from = from, .text = put, .bytes = saved_from);
    const CliPlace end =
        EMBED_CALL(view_place_past, ViewCtx, .from = from, .text = put, .bytes = saved_from + saved_bytes);
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
    const CliPlace place = args->below
                               ? (CliPlace){line, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = line)}
                               : (CliPlace){line, 0ull};
    // the lead is blanks, a byte a unit
    CliViewEdit *const made = EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = place, .to = place,
                                         .put_bytes = lead + 1ull, .put_spans = 0ull, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    char *const put = made->put;
    unsigned long long at = 0ull;
    if (args->below)
    {
        put[0] = '\n';
        at = 1ull;
    }
    for (unsigned long long col = 0ull; col < lead; col += 1ull)
    {
        // a blank unit is a byte
        put[at] = (char)EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = line, .col = col);
        at += 1ull;
    }
    if (!args->below)
    {
        put[at] = '\n';
        at += 1ull;
    }
    made->put_length = at;
    if (!EMBED_CALL(view_edit_cell_write, ViewCtx, .view = args->view, .made = made, .kind = CLI_VIEW_KIND_NONE,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!args->below)
    {
        const CliPlace saved = {line, lead};
        EMBED_CALL(view_settle, ViewCtx, .view = args->view, .cursor = saved, .anchor = saved);
    }
    return 1;
}

// the whole lines the selection touches selected, through the next line's start, the next line taken where the
// selection is whole lines already
EMBED_INLINE void view_line_select(const ViewCtx *args)
{
    CliPlace from;
    CliPlace to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    const unsigned long long count = EMBED_CALL(view_lines, ViewCtx, .view_read = args->view);
    if (EMBED_CALL(view_select, ViewCtx, .view_read = args->view) && (from.col == 0ull) && (to.col == 0ull))
    {
        to.col = 1ull;
    }
    const unsigned long long last = ((to.col == 0ull) && (to.line > from.line)) ? to.line - 1ull : to.line;
    args->view->anchor = (CliPlace){from.line, 0ull};
    EMBED_CALL(view_move, ViewCtx, .view = args->view,
               .place = (last + 1ull < count)
                            ? (CliPlace){last + 1ull, 0ull}
                            : (CliPlace){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)},
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
static int view_lines_collated(const CliCompareCfg *args)
{
    const char *other = args->other;
    unsigned long long left = 0ull;
    unsigned long long right = 0ull;
    while ((left < args->one_length) && (right < args->other_length))
    {
        const unsigned char a = (unsigned char)args->one[left];
        const unsigned char b = (unsigned char)other[right];
        if (((unsigned int)(a - '0') < 10u) && ((unsigned int)(b - '0') < 10u))
        {
            unsigned long long a_end = left;
            unsigned long long b_end = right;
            while ((a_end < args->one_length) && ((unsigned int)((unsigned char)args->one[a_end] - '0') < 10u))
            {
                a_end += 1ull;
            }
            while ((b_end < args->other_length) && ((unsigned int)((unsigned char)other[b_end] - '0') < 10u))
            {
                b_end += 1ull;
            }
            while ((left + 1ull < a_end) && (args->one[left] == '0'))
            {
                left += 1ull;
            }
            while ((right + 1ull < b_end) && (other[right] == '0'))
            {
                right += 1ull;
            }
            if (a_end - left != b_end - right)
            {
                return (a_end - left < b_end - right) ? -1 : 1;
            }
            const int digits =
                memor.cmp(&(MemoriaCfg){.src = args->one + left, .other = other + right, .bytes = a_end - left});
            if (digits != 0)
            {
                return digits;
            }
            left = a_end;
            right = b_end;
            continue;
        }
        if (s_view_ranks[a] != s_view_ranks[b])
        {
            return (s_view_ranks[a] < s_view_ranks[b]) ? -1 : 1;
        }
        left += 1ull;
        right += 1ull;
    }
    if ((left < args->one_length) || (right < args->other_length))
    {
        return (left < args->one_length) ? 1 : -1;
    }
    // level but for case: a small letter, the greater byte, first
    const unsigned long long shorter = (args->one_length < args->other_length) ? args->one_length : args->other_length;
    for (unsigned long long at = 0ull; at < shorter; at += 1ull)
    {
        if (args->one[at] != other[at])
        {
            return ((unsigned char)args->one[at] > (unsigned char)other[at]) ? -1 : 1;
        }
    }
    return 0;
}

// two lines by their bytes
static int view_lines_bytes(const CliCompareCfg *args)
{
    const char *other = args->other;
    const unsigned long long shorter = (args->one_length < args->other_length) ? args->one_length : args->other_length;
    const int bytes = memor.cmp(&(MemoriaCfg){.src = args->one, .other = other, .bytes = shorter});
    if (bytes != 0)
    {
        return bytes;
    }
    return (args->one_length == args->other_length) ? 0 : ((args->one_length < args->other_length) ? -1 : 1);
}

// the `count` lines of `text`, each at its `start` and ending a byte before the next, their numbers in `order` merge
// sorted, level ones saving their order, by `compare`, or its reverse with `descending`, `work` a table of as many
EMBED_INLINE void view_lines_merge(const ViewCtx *args)
{
    unsigned int *from = args->order;
    unsigned int *into = args->work;
    for (unsigned int width = 1u; width < args->count_u32; width *= 2u)
    {
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
                    const int compared =
                        EMBED_CALL(args->compare, CliCompareCfg, .one = args->text + args->start[one],
                                   .one_length = args->start[one + 1u] - args->start[one] - 1ull,
                                   .other = args->text + args->start[other],
                                   .other_length = args->start[other + 1u] - args->start[other] - 1ull);
                    take_left = (args->descending ? -compared : compared) <= 0;
                }
                into[at] = take_left ? from[left] : from[right];
                left += take_left ? 1u : 0u;
                right += take_left ? 0u : 1u;
            }
        }
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
    unsigned long long first = 0ull;
    unsigned long long last = (EMBED_CALL(view_lines, ViewCtx, .view_read = args->view) != 0ull)
                                  ? EMBED_CALL(view_lines, ViewCtx, .view_read = args->view) - 1ull
                                  : 0ull;
    if (EMBED_CALL(view_select, ViewCtx, .view_read = args->view))
    {
        EMBED_CALL(view_lines_cover, ViewCtx, .view_read = args->view, .first_pointer = &first, .last_pointer = &last);
    }
    // the lines a text holds are counted by an unsigned int
    const unsigned int count = (unsigned int)(last - first + 1ull);
    if (count < 2u)
    {
        return 1;
    }
    CliViewEdit *const made =
        EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = (CliPlace){first, 0ull},
                   .to = (CliPlace){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)},
                   .put_bytes = 0ull, .put_spans = 1ull, .err_msg = args->err_msg);
    const unsigned long long table = (((unsigned long long)count + 1ull) * sizeof(unsigned long long)) +
                                     ((unsigned long long)count * ((2ull * sizeof(unsigned int)) + 1ull));
    unsigned long long *const start =
        (made != NULL) ? g_cli_cells->temporary_buf_alloc(table, &g_cli_views_cells) : NULL;
    if (start == NULL)
    {
        g_cli_cells->persistent_buf_release(made, &g_cli_views_cells);
        return 0;
    }
    unsigned int *const order = (unsigned int *)(start + count + 1u);
    unsigned int *const work = order + count;
    unsigned char *const repeats = (unsigned char *)(work + count);
    const char *const took = made->took;
    const unsigned long long size = made->took_length;
    unsigned int line = 0u;
    start[0] = 0ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        if (took[at] == '\n')
        {
            line += 1u;
            start[line] = at + 1ull;
        }
    }
    start[count] = size + 1ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        order[at] = at;
    }
    char *const put = made->put;
    unsigned long long at = 0ull;
    if (args->unique)
    {
        EMBED_CALL(view_lines_merge, ViewCtx, .text = took, .start = start, .order = order, .work = work,
                   .count_u32 = count, .compare = view_lines_bytes, .descending = 0);
        memor.set(&(MemoriaCfg){.dst = repeats, .val = 0, .bytes = count});
        for (unsigned int sorted = 1u; sorted < count; sorted += 1u)
        {
            const unsigned int one = order[sorted - 1u];
            const unsigned int other = order[sorted];
            if (EMBED_CALL(view_lines_bytes, CliCompareCfg, .one = took + start[one],
                           .one_length = start[one + 1u] - start[one] - 1ull, .other = took + start[other],
                           .other_length = start[other + 1u] - start[other] - 1ull) == 0)
            {
                repeats[other] = 1u;
            }
        }
    }
    else
    {
        view_ranks_init();
        EMBED_CALL(view_lines_merge, ViewCtx, .text = took, .start = start, .order = order, .work = work,
                   .count_u32 = count, .compare = view_lines_collated, .descending = args->descending);
    }
    for (unsigned int written = 0u; written < count; written += 1u)
    {
        const unsigned int each = args->unique ? written : order[written];
        if (!args->unique || !repeats[each])
        {
            if (at != 0ull)
            {
                put[at] = '\n';
                at += 1ull;
            }
            const unsigned long long length = start[each + 1u] - start[each] - 1ull;
            memor.cpy(&(MemoriaCfg){.dst = put + at, .src = took + start[each], .bytes = length});
            at += length;
        }
    }
    made->put_length = at;
    g_cli_cells->persistent_buf_release(start, &g_cli_views_cells);
    return EMBED_CALL(view_cell_replace, ViewCtx, .view = args->view, .made = made, .err_msg = args->err_msg);
}

// a letter in its other case: `upper` its capital, else its small letter, the letters of ASCII, Latin-1, Latin
// Extended-A, Greek and Cyrillic; any other as it is; each stays as many bytes of UTF-8 as it was
EMBED_INLINE unsigned int view_unit_case(const ViewCtx *args)
{
    const int small_run = ((args->unit >= 'a') && (args->unit <= 'z')) ||
                          ((args->unit >= 0xe0u) && (args->unit <= 0xfeu) && (args->unit != 0xf7u)) ||
                          ((args->unit >= 0x3b1u) && (args->unit <= 0x3c9u) && (args->unit != 0x3c2u)) ||
                          ((args->unit >= 0x430u) && (args->unit <= 0x44fu));
    const int capital_run = ((args->unit >= 'A') && (args->unit <= 'Z')) ||
                            ((args->unit >= 0xc0u) && (args->unit <= 0xdeu) && (args->unit != 0xd7u)) ||
                            ((args->unit >= 0x391u) && (args->unit <= 0x3a9u)) ||
                            ((args->unit >= 0x410u) && (args->unit <= 0x42fu));
    if (args->upper && small_run)
    {
        return args->unit - 0x20u;
    }
    if (!args->upper && capital_run)
    {
        return args->unit + 0x20u;
    }
    if (args->upper && (args->unit >= 0x450u) && (args->unit <= 0x45fu))
    {
        return args->unit - 0x50u;
    }
    if (!args->upper && (args->unit >= 0x400u) && (args->unit <= 0x40fu))
    {
        return args->unit + 0x50u;
    }
    // Latin Extended-A pairs each capital with the letter after it, an even or an odd capital by its run
    const int even_run =
        ((args->unit >= 0x100u) && (args->unit <= 0x137u)) || ((args->unit >= 0x14au) && (args->unit <= 0x177u));
    const int odd_run =
        ((args->unit >= 0x139u) && (args->unit <= 0x148u)) || ((args->unit >= 0x179u) && (args->unit <= 0x17eu));
    if (even_run || odd_run)
    {
        const int capital = even_run ? ((args->unit & 1u) == 0u) : ((args->unit & 1u) == 1u);
        return (args->upper && !capital) ? args->unit - 1u : ((!args->upper && capital) ? args->unit + 1u : args->unit);
    }
    return args->unit;
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
    CliPlace from;
    CliPlace to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    if (!EMBED_CALL(view_select, ViewCtx, .view_read = args->view))
    {
        unsigned long long start = 0ull;
        unsigned long long end = 0ull;
        if (!EMBED_CALL(view_word_span, ViewCtx, .view = args->view, .line = args->view->cursor.line,
                        .col = args->view->cursor.col, .from_u64s = &start, .to_u64s = &end))
        {
            return 1;
        }
        from = (CliPlace){args->view->cursor.line, start};
        to = (CliPlace){args->view->cursor.line, end};
    }
    CliViewEdit *const made = EMBED_CALL(view_edit_cell, ViewCtx, .view = args->view, .from = from, .to = to,
                                         .put_bytes = 0ull, .put_spans = 1ull, .err_msg = args->err_msg);
    if (made == NULL)
    {
        return 0;
    }
    VerbaLinea cased = {made->put, made->took_length + 1ull, 0ull};
    int in_word = 0;
    for (unsigned long long at = 0ull; at < made->took_length;)
    {
        unsigned long long bytes = 0ull;
        const unsigned int unit = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = made->took,
                                             .size = made->took_length, .at = at, .length = &bytes);
        at += bytes;
        if (args->mode == 2u)
        {
            const int begins = !in_word && EMBED_CALL(view_unit_letter, ViewCtx, .unit = unit);
            in_word = begins || (in_word && EMBED_CALL(view_unit_word_on, ViewCtx, .unit = unit));
            EMBED_CALL(cli_line_letter_internal.utf8_put, CliLineLetterCtx, .line = &cased,
                       .letter =
                           begins ? EMBED_CALL(view_unit_case, ViewCtx, .unit = unit, .upper = 1)
                                  : (in_word ? EMBED_CALL(view_unit_case, ViewCtx, .unit = unit, .upper = 0) : unit));
        }
        else
        {
            EMBED_CALL(cli_line_letter_internal.utf8_put, CliLineLetterCtx, .line = &cased,
                       .letter = EMBED_CALL(view_unit_case, ViewCtx, .unit = unit, .upper = args->mode == 1u));
        }
    }
    made->put_length = cased.at;
    const CliPlace cursor = args->view->cursor;
    const CliPlace anchor = args->view->anchor;
    const CliPlace end =
        EMBED_CALL(view_place_past, ViewCtx, .from = from, .text = made->put, .bytes = made->put_length);
    if (!EMBED_CALL(view_edit_cell_write, ViewCtx, .view = args->view, .made = made, .kind = CLI_VIEW_KIND_NONE,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(view_settle, ViewCtx, .view = args->view,
               .cursor = EMBED_CALL(view_place_carry, ViewCtx, .place = cursor, .from = from, .to = to, .end = end,
                                    .after = 1),
               .anchor = EMBED_CALL(view_place_carry, ViewCtx, .place = anchor, .from = from, .to = to, .end = end,
                                    .after = 0));
    return 1;
}

// the cursor taken past the bracket the one at it pairs with, where one stands there
EMBED_INLINE void view_bracket_jump(const ViewCtx *args)
{
    CliPlace pair[2];
    if (EMBED_CALL(view_pair, ViewCtx, .view_read = args->view, .pair = pair))
    {
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = (CliPlace){pair[1].line, pair[1].col + 1ull},
                   .extend = 0, .saved = 0);
    }
}

// the span Expand Selection takes: the tree hands the least first, and that one is saved
typedef struct
{
    int found;
    CliSymbolPlace from;
    CliSymbolPlace to;
} CliViewSpan;

static void view_span_save(const CliSpannedCfg *args)
{
    CliSymbolPlace from = args->from;
    CliSymbolPlace to = args->to;
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
    unsigned long long size = 0ull;
    char *const text = args->view->paged
                           ? NULL
                           : EMBED_CALL(cli_document_key.text_take, CliDocumentKeyCfg, .key = args->view->key,
                                        .size_pointer = &size, .owner = &g_cli_views_cells);
    if (text == NULL)
    {
        return 1;
    }
    CliPlace from;
    CliPlace to;
    EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
    CliViewSpan span;
    span.found = 0;
    // a place in a text a view shows stands below 2^32 lines and columns
    const int read =
        EMBED_CALL(cli_symbols_path_text_size_context_err_msg_from_symbol_place.spans,
                   CliSymbolsPathTextSizeContextErrMsgFromSymbolPlaceCfg, .path = args->view->path, .text = text,
                   .size = size, .from_symbol_place = (CliSymbolPlace){(unsigned int)from.line, (unsigned int)from.col},
                   .to_symbol_place = (CliSymbolPlace){(unsigned int)to.line, (unsigned int)to.col},
                   .each_spanned = view_span_save, .context = &span, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(text, &g_cli_views_cells);
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
        args->view->anchor = (CliPlace){span.from.line, span.from.col};
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = (CliPlace){span.to.line, span.to.col}, .extend = 1,
                   .saved = 0);
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

// `name` run where it names a line edit of the menus: 1 with whether it was taken into `taken`, or 0 where it names
// none
static int view_command_run(const ViewCtx *args)
{
    int *taken = args->taken;
    if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "comment"))
    {
        *taken = (args->view->comment_line[0] != '\0')
                     ? view_lines_comment(args)
                     : ((args->view->comment_open[0] != '\0') ? view_block_comment(args) : 1);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "select-line"))
    {
        view_line_select(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "join-lines"))
    {
        *taken = view_lines_join(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "sort-ascending") ||
             EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "sort-descending"))
    {
        *taken =
            EMBED_CALL(view_lines_sort, ViewCtx, .view = args->view,
                       .descending = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "sort-descending"),
                       .unique = 0, .err_msg = args->err_msg);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "unique-lines"))
    {
        *taken = EMBED_CALL(view_lines_sort, ViewCtx, .view = args->view, .descending = 0, .unique = 1,
                            .err_msg = args->err_msg);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "upper-case") ||
             EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "lower-case") ||
             EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "title-case"))
    {
        *taken = EMBED_CALL(
            view_case_change, ViewCtx, .view = args->view,
            .mode = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "upper-case")
                        ? 1u
                        : (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "title-case") ? 2u : 0u),
            .err_msg = args->err_msg);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "copy-line-up") ||
             EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "copy-line-down"))
    {
        *taken = EMBED_CALL(view_lines_copy, ViewCtx, .view = args->view,
                            .up = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "copy-line-up"),
                            .err_msg = args->err_msg);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "move-line-up") ||
             EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "move-line-down"))
    {
        *taken = EMBED_CALL(view_lines_move, ViewCtx, .view = args->view,
                            .up = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "move-line-up"),
                            .err_msg = args->err_msg);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "bracket"))
    {
        view_bracket_jump(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "expand-selection"))
    {
        *taken = view_selection_grow(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "shrink-selection"))
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
// taken into `taken`, or 0 where the key is none of them
EMBED_INLINE int view_line_key(const ViewCtx *args)
{
    int *taken = args->taken;
    const int vertical = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Up") ||
                         EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Down");
    if (vertical && (args->mod || args->alt))
    {
        const int up = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Up");
        if (args->mod && !args->alt && !args->shift)
        {
            // Mod and an arrow scroll a line, the cursor staying; with Alt they would add a cursor, which the view has
            // not
            args->view->top =
                up ? EMBED_CALL(view_up, ViewCtx, .view_read = args->view, .line = args->view->top, .count = 1ull)
                   : EMBED_CALL(view_down, ViewCtx, .view_read = args->view, .line = args->view->top, .count = 1ull);
        }
        else if (args->alt && !args->mod)
        {
            *taken = args->shift
                         ? EMBED_CALL(view_lines_copy, ViewCtx, .view = args->view, .up = up, .err_msg = args->err_msg)
                         : EMBED_CALL(view_lines_move, ViewCtx, .view = args->view, .up = up, .err_msg = args->err_msg);
        }
        return 1;
    }
    if (args->alt && args->shift && !args->mod &&
        EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Right"))
    {
        *taken = view_selection_grow(args);
        return 1;
    }
    if (args->alt && args->shift && !args->mod &&
        EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Left"))
    {
        view_selection_shrunk(args);
        return 1;
    }
    if (args->mod && !args->alt)
    {
        if (!args->shift && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "L"))
        {
            view_line_select(args);
            return 1;
        }
        if (!args->shift && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "J"))
        {
            *taken = view_lines_join(args);
            return 1;
        }
        if (!args->shift && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "/"))
        {
            return EMBED_CALL(view_command_run, ViewCtx, .view = args->view, .name = "comment", .taken = taken,
                              .err_msg = args->err_msg);
        }
        if (args->shift && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "K"))
        {
            *taken = view_lines_delete(args);
            return 1;
        }
        if (!args->shift && (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "]") ||
                             EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "[")))
        {
            *taken = EMBED_CALL(view_lines_indent, ViewCtx, .view = args->view,
                                .deeper = EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "]"),
                                .err_msg = args->err_msg);
            return 1;
        }
        if (args->shift && EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "\\"))
        {
            view_bracket_jump(args);
            return 1;
        }
        if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Enter"))
        {
            *taken = EMBED_CALL(view_line_insert, ViewCtx, .view = args->view, .below = !args->shift,
                                .err_msg = args->err_msg);
            return 1;
        }
        if (!EMBED_CALL(view_select, ViewCtx, .view_read = args->view) &&
            EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Backspace") &&
            (args->view->cursor.col != 0ull))
        {
            *taken = EMBED_CALL(
                view_edit, ViewCtx, .view = args->view,
                .from = EMBED_CALL(view_word_leave, ViewCtx, .view = args->view, .place = args->view->cursor),
                .to = args->view->cursor, .text = "", .length = 0ull, .kind = CLI_VIEW_KIND_DELETE,
                .err_msg = args->err_msg);
            return 1;
        }
        if (!EMBED_CALL(view_select, ViewCtx, .view_read = args->view) &&
            EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Delete") &&
            (args->view->cursor.col <
             EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = args->view->cursor.line)))
        {
            *taken =
                EMBED_CALL(view_edit, ViewCtx, .view = args->view, .from = args->view->cursor,
                           .to = EMBED_CALL(view_word_right, ViewCtx, .view = args->view, .place = args->view->cursor),
                           .text = "", .length = 0ull, .kind = CLI_VIEW_KIND_DELETE_FORWARD, .err_msg = args->err_msg);
            return 1;
        }
    }
    if (EMBED_CALL(view_by_name, ViewCtx, .name = args->name, .wanted = "Tab"))
    {
        CliPlace from;
        CliPlace to;
        EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
        if (args->shift || (from.line != to.line))
        {
            *taken = EMBED_CALL(view_lines_indent, ViewCtx, .view = args->view, .deeper = !args->shift,
                                .err_msg = args->err_msg);
            return 1;
        }
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

// a key taken: 1, or 0 with why given where an edit it makes fails; a key the view has no use for is taken and does
// nothing
EMBED_INLINE int view_on_key(const ViewCtx *args)
{
    int mod = 0;
    int shift = 0;
    int alt = 0;
    const char *const name = EMBED_CALL(view_key_read, ViewCtx, .name = args->given, .mod_pointer = &mod,
                                        .shift_pointer = &shift, .alt_pointer = &alt);
    const unsigned long long lines = EMBED_CALL(view_lines, ViewCtx, .view_read = args->view);
    const unsigned long long last = (lines != 0ull) ? lines - 1ull : 0ull;
    const unsigned int rows = (args->view->rows > 1u) ? args->view->rows : 1u;
    if (EMBED_CALL(view_find_key, ViewCtx, .view = args->view, .name = name, .mod = mod, .shift = shift, .alt = alt))
    {
        return 1;
    }
    int taken = 1;
    if (EMBED_CALL(view_command_run, ViewCtx, .view = args->view, .name = name, .taken = &taken,
                   .err_msg = args->err_msg) ||
        EMBED_CALL(view_line_key, ViewCtx, .view = args->view, .name = name, .mod = mod, .shift = shift, .alt = alt,
                   .taken = &taken, .err_msg = args->err_msg))
    {
        return taken;
    }
    if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "Left"))
    {
        CliPlace from;
        CliPlace to;
        EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
        const CliPlace place =
            mod ? EMBED_CALL(view_word_leave, ViewCtx, .view = args->view, .place = args->view->cursor)
                : ((!shift && EMBED_CALL(view_select, ViewCtx, .view_read = args->view))
                       ? from
                       : EMBED_CALL(view_leave, ViewCtx, .view = args->view, .place = args->view->cursor));
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = place, .extend = shift, .saved = 0);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "Right"))
    {
        CliPlace from;
        CliPlace to;
        EMBED_CALL(view_selection, ViewCtx, .view_read = args->view, .from_pointer = &from, .to_pointer = &to);
        const CliPlace place =
            mod ? EMBED_CALL(view_word_right, ViewCtx, .view = args->view, .place = args->view->cursor)
                : ((!shift && EMBED_CALL(view_select, ViewCtx, .view_read = args->view))
                       ? to
                       : EMBED_CALL(view_right, ViewCtx, .view = args->view, .place = args->view->cursor));
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = place, .extend = shift, .saved = 0);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "Up"))
    {
        EMBED_CALL(view_vertical, ViewCtx, .view = args->view, .by = -1ll, .extend = shift);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "Down"))
    {
        EMBED_CALL(view_vertical, ViewCtx, .view = args->view, .by = 1ll, .extend = shift);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "Home"))
    {
        if (mod)
        {
            EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = (CliPlace){0ull, 0ull}, .extend = shift,
                       .saved = 0);
        }
        else
        {
            const unsigned long long first =
                EMBED_CALL(view_first_letter, ViewCtx, .view = args->view, .line = args->view->cursor.line);
            EMBED_CALL(view_move, ViewCtx, .view = args->view,
                       .place = (CliPlace){args->view->cursor.line, (args->view->cursor.col == first) ? 0ull : first},
                       .extend = shift, .saved = 0);
        }
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "End"))
    {
        const unsigned long long end = EMBED_CALL(view_shown_up, ViewCtx, .view_read = args->view, .line = last);
        const CliPlace place =
            mod ? (CliPlace){end, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = end)}
                : (CliPlace){args->view->cursor.line,
                             EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = args->view->cursor.line)};
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = place, .extend = shift, .saved = 0);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "PageUp") ||
             EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "PageDown"))
    {
        // a page is the rows less one: the last row seen stays in view
        const unsigned long long by = rows - 1u;
        const int up = EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "PageUp");
        EMBED_CALL(view_vertical, ViewCtx, .view = args->view, .by = up ? -(long long)by : (long long)by,
                   .extend = shift);
        args->view->top =
            up ? EMBED_CALL(view_up, ViewCtx, .view_read = args->view, .line = args->view->top, .count = by)
               : EMBED_CALL(view_down, ViewCtx, .view_read = args->view, .line = args->view->top, .count = by);
        view_follow(args);
    }
    else if (mod && shift && EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "["))
    {
        (void)EMBED_CALL(view_fold, ViewCtx, .view = args->view, .line = args->view->cursor.line, .exact = 0);
    }
    else if (mod && shift && EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "]"))
    {
        (void)EMBED_CALL(view_unfolded, ViewCtx, .view = args->view, .line = args->view->cursor.line);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "Backspace"))
    {
        return view_backspace(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "Delete"))
    {
        return view_delete(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "Enter"))
    {
        return view_newline(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "Tab"))
    {
        return view_on_tab(args);
    }
    else if (EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "Escape"))
    {
        EMBED_CALL(view_move, ViewCtx, .view = args->view, .place = args->view->cursor, .extend = 0, .saved = 0);
    }
    else if (mod && EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "A"))
    {
        args->view->anchor = (CliPlace){0ull, 0ull};
        EMBED_CALL(view_move, ViewCtx, .view = args->view,
                   .place = (CliPlace){last, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = last)},
                   .extend = 1, .saved = 0);
    }
    else if (mod && EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "Z"))
    {
        return shift ? view_redone(args) : view_undone(args);
    }
    else if (mod && EMBED_CALL(view_by_name, ViewCtx, .name = name, .wanted = "Y"))
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
    const CliPlace place = {
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
            EMBED_CALL(view_down, ViewCtx, .view_read = args->view, .line = line, .count = 1ull);
        args->view->anchor = (CliPlace){line, 0ull};
        EMBED_CALL(view_move, ViewCtx, .view = args->view,
                   .place = (below > line)
                                ? (CliPlace){below, 0ull}
                                : (CliPlace){line, EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = line)},
                   .extend = 1, .saved = 0);
        return;
    }
    if (args->input->clicks == 2u)
    {
        const CliPlace start =
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

int cli_view_input(const CliViewInputCfg *args)
{
    const char *key = args->key;
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = key);
    if (view == NULL)
    {
        mmgr_line_put(args->err_msg, "no view is open under the key");
        return 0;
    }
    int taken = 1;
    EMBED_CALL(view_folds_refresh, ViewCtx, .view = view);
    if (args->input->kind == CLI_VIEW_KEY)
    {
        taken = EMBED_CALL(view_on_key, ViewCtx, .view = view, .given = args->input->name, .err_msg = args->err_msg);
    }
    else if (args->input->kind == CLI_VIEW_TEXT)
    {
        const int one = (args->input->length != 0ull) &&
                        (EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = args->input->text,
                                    .size = args->input->length) == 1ull);
        const int blank =
            (args->input->length != 0ull) &&
            ((args->input->text[0] == ' ') || (args->input->text[0] == '\t') || (args->input->text[0] == '\n'));
        const unsigned int kind =
            (one && !blank) ? CLI_VIEW_KIND_TYPE : (one ? CLI_VIEW_KIND_SPACE : CLI_VIEW_KIND_NONE);
        unsigned long long took = 0ull;
        const unsigned int letter = one ? EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->input->text,
                                                     .size = args->input->length, .at = 0ull, .length = &took)
                                        : 0u;
        if (view->find_open && view->find_focus)
        {
            // text typed goes into the query, a byte a unit at the most
            unsigned short *const units =
                g_cli_cells->temporary_buf_alloc((args->input->length + 1ull) * sizeof(*units), &g_cli_views_cells);
            if (units != NULL)
            {
                EMBED_CALL(view_query_put, ViewCtx, .view = view, .units_read = units,
                           .count = EMBED_CALL(view_units_get, ViewCtx, .text = args->input->text,
                                               .size = args->input->length, .units = units,
                                               .room = args->input->length + 1ull));
                g_cli_cells->persistent_buf_release(units, &g_cli_views_cells);
            }
        }
        else
        {
            taken = (one && !blank && (letter < 0x10000u))
                        ? EMBED_CALL(view_letter_type, ViewCtx, .view = view, .text = args->input->text,
                                     .length = args->input->length, .letter = letter, .kind = kind,
                                     .err_msg = args->err_msg)
                        : EMBED_CALL(view_type, ViewCtx, .view = view, .text = args->input->text,
                                     .length = args->input->length, .kind = kind, .err_msg = args->err_msg);
        }
    }
    else if (args->input->kind == CLI_VIEW_POINTER)
    {
        EMBED_CALL(view_point, ViewCtx, .view = view, .input = args->input);
    }
    else if (args->input->kind == CLI_VIEW_SCROLL)
    {
        const unsigned long long lines = EMBED_CALL(view_lines, ViewCtx, .view_read = view);
        const unsigned long long last = (lines != 0ull) ? lines - 1ull : 0ull;
        const unsigned long long top = (args->input->top > last) ? last : args->input->top;
        // a top a closed fold hides goes past the fold the way the view scrolls
        view->top = (top > view->top) ? EMBED_CALL(view_shown_down, ViewCtx, .view_read = view, .line = top)
                                      : EMBED_CALL(view_shown_up, ViewCtx, .view_read = view, .line = top);
    }
    else if (args->input->kind == CLI_VIEW_SIZE)
    {
        // a size the same as the view's changes nothing; one changed moves the top so the cursor stays in view
        const int changed = (view->rows != args->input->rows) || (view->columns != args->input->columns);
        view->rows = args->input->rows;
        view->columns = args->input->columns;
        if (changed)
        {
            EMBED_CALL(view_follow, ViewCtx, .view = view);
        }
    }
    else if (args->input->kind == CLI_VIEW_COMPLETE)
    {
        // the completion's text written over the units before the cursor it replaces, no more than stand before it
        const unsigned long long back =
            (args->input->column < view->cursor.col) ? args->input->column : view->cursor.col;
        const CliPlace from = {view->cursor.line, view->cursor.col - back};
        taken =
            EMBED_CALL(view_edit, ViewCtx, .view = view, .from = from, .to = view->cursor, .text = args->input->text,
                       .length = args->input->length, .kind = CLI_VIEW_KIND_NONE, .err_msg = args->err_msg);
    }
    else if (args->input->kind == CLI_VIEW_PLACE)
    {
        // the cursor placed at the line `row` and the unit `column`, each cut to the text, the top following it
        const unsigned long long lines = EMBED_CALL(view_lines, ViewCtx, .view_read = view);
        const unsigned long long line =
            (args->input->row < lines) ? args->input->row : ((lines > 0ull) ? lines - 1ull : 0ull);
        const unsigned long long length = EMBED_CALL(view_length, ViewCtx, .view = view, .line = line);
        EMBED_CALL(view_move, ViewCtx, .view = view,
                   .place = (CliPlace){line, (args->input->column < length) ? args->input->column : length},
                   .extend = 0, .saved = 0);
    }
    // a paged text's pages are told where the view stands and cache the lines about it
    if (view->paged)
    {
        const unsigned long long rows = (view->rows != 0u) ? view->rows : 1ull;
        const CliPagesView placed_view = {view->top,        rows, view->columns, 0ull, 0ull, view->top,
                                          view->top + rows, 0ull, 0ull};
        CliPagesPlaced placed;
        (void)EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = view->key, .view = &placed_view,
                         .placed = &placed);
    }
    return taken;
}

EMBED_INLINE void view_place_write(const ViewCtx *args)
{
    mmgr_line_put(args->out, "{\"line\":");
    mmgr_line_decimal(args->out, args->place.line, 1u);
    mmgr_line_put(args->out, ",\"col\":");
    mmgr_line_decimal(args->out, args->place.col, 1u);
    mmgr_line_ch(args->out, '}');
}

// the bracket pair at the cursor, into `pair`, the bracket that ends at the cursor, else the one that starts at it,
// first and the one it pairs with second: the text's lines from as far above and below the cursor as
// CLI_VIEW_PAIR_LINES reach and none outside those read for it, read as the tree reads a text from the state the first
// is entered in: 1, or 0 where no bracket of a pair stands at the cursor or the lines could not be read
EMBED_INLINE int view_pair(const ViewCtx *args)
{
    CliPlace *pair = args->pair;
    unsigned long long low = 0ull;
    unsigned long long high = 0ull;
    (void)EMBED_CALL(cli_document_key_low.bounds, CliDocumentKeyLowCfg, .key = args->view_read->key, .low = &low,
                     .high = &high);
    const unsigned long long line = args->view_read->cursor.line;
    const unsigned long long reach = (line > CLI_VIEW_PAIR_LINES) ? (line - CLI_VIEW_PAIR_LINES) : 0ull;
    const unsigned long long start = (reach > low) ? reach : low;
    const unsigned long long end =
        ((high != 0ull) && ((line + CLI_VIEW_PAIR_LINES) >= high)) ? (high - 1ull) : (line + CLI_VIEW_PAIR_LINES);
    const unsigned long long first = (start < line) ? start : line;
    const unsigned long long last = (end > line) ? end : line;
    char state[CLI_VIEW_STATE];
    unsigned long long size = 0ull;
    char *const text = EMBED_CALL(cli_document_key.lines_take, CliDocumentKeyCfg, .key = args->view_read->key,
                                  .first = first, .last = last, .size_pointer = &size, .state = state,
                                  .room = sizeof(state), .owner = &g_cli_views_cells);
    if (text == NULL)
    {
        return 0;
    }
    CliSymbolPlace found[2] = {{0u, 0u}, {0u, 0u}};
    // a place in a text a view shows stands below 2^32 lines and columns
    const int paired = EMBED_CALL(
        cli_symbols_path_text_size_state.pair, CliSymbolsPathTextSizeStateCfg, .path = args->view_read->path,
        .state = (state[0] != '\0') ? state : "root", .text = text, .size = size,
        .at_symbol_place = (CliSymbolPlace){(unsigned int)(line - first), (unsigned int)args->view_read->cursor.col},
        .pair = found);
    g_cli_cells->persistent_buf_release(text, &g_cli_views_cells);
    for (unsigned int at = 0u; paired && (at < 2u); at += 1u)
    {
        pair[at].line = first + found[at].line;
        pair[at].col = found[at].col;
    }
    return paired;
}

// a paged text's rows written into `out` as strings after the opening bracket: read through the pages a run of lines
// in a row at a time, a line that could not be read written empty
EMBED_INLINE void view_paged_rows_write(const ViewCtx *args)
{
    VerbaLinea *out = args->out;
    for (unsigned long long at = 0ull; at < args->shown->count;)
    {
        unsigned long long end = at;
        while ((end + 1ull < args->shown->count) &&
               (args->shown->number[end + 1ull] == args->shown->number[end] + 1ull))
        {
            end += 1ull;
        }
        unsigned long long count = 0ull;
        char *const cell = EMBED_CALL(cli_pages_from.lines_take, CliPagesFromCfg, .key = args->view_read->key,
                                      .from = args->shown->number[at], .to = args->shown->number[end] + 1ull,
                                      .line = args->shown->text + at, .count = &count, .owner = &g_cli_views_cells);
        for (unsigned long long each = at; each <= end; each += 1ull)
        {
            mmgr_line_put(out, (each > 0ull) ? "," : "");
            if ((cell != NULL) && (each - at < count))
            {
                EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                           .text = args->shown->text[each],
                           .length =
                               cellul.len(&(CatenaFinitaCfg){.src = args->shown->text[each], .cap = CLI_CELLS_POOL}));
            }
            else
            {
                mmgr_line_put(out, "\"\"");
            }
        }
        g_cli_cells->persistent_buf_release(cell, &g_cli_views_cells);
        at = end + 1ull;
    }
}

// a fold of the text stored among those seen where it opens on one of the frame's rows; the texts' lock is taken
// here, and no line is read under it
static void view_fold_visit(const CliFoldEachCfg *args)
{
    CliViewFoldsSeen *const folds = args->context;
    unsigned long long low = 0ull;
    unsigned long long high = folds->shown->count;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ull);
        if (folds->shown->number[middle] < args->first)
        {
            low = middle + 1ull;
        }
        else
        {
            high = middle;
        }
    }
    if ((low < folds->shown->count) && (folds->shown->number[low] == args->first) &&
        (folds->count < folds->shown->count))
    {
        folds->seen[folds->count] = (CliViewFoldSeen){args->first, args->last, args->shown};
        folds->count += 1ull;
    }
}

// the folds seen written into the frame, each `[first, last, shown, closed, closer]`, `closed` 1 where the view holds
// it closed and `closer` the text of the line it closes on, its blanks at either end left off, where it shows that
// line, else empty
EMBED_INLINE void view_folds_write(const ViewCtx *args)
{
    VerbaLinea *out = args->out;
    for (unsigned long long at = 0ull; at < args->folds->count; at += 1ull)
    {
        const CliViewFoldSeen *const fold = &args->folds->seen[at];
        int closed = 0;
        for (unsigned int each = 0u; each < args->view->folds; each += 1u)
        {
            closed = closed || (args->view->fold[each].first == fold->first);
        }
        mmgr_line_put(out, (at > 0ull) ? ",[" : "[");
        mmgr_line_decimal(out, fold->first, 1u);
        mmgr_line_ch(out, ',');
        mmgr_line_decimal(out, fold->last, 1u);
        mmgr_line_put(out, fold->shown ? ",1," : ",0,");
        mmgr_line_put(out, closed ? "1," : "0,");
        if (fold->shown)
        {
            const unsigned long long length = EMBED_CALL(view_length, ViewCtx, .view = args->view, .line = fold->last);
            unsigned long long start = 0ull;
            unsigned long long end = length;
            while ((start < end) && EMBED_CALL(view_blank_unit, ViewCtx, .unit = args->view->line_units[start]))
            {
                start += 1ull;
            }
            while ((end > start) && EMBED_CALL(view_blank_unit, ViewCtx, .unit = args->view->line_units[end - 1ull]))
            {
                end -= 1ull;
            }
            EMBED_CALL(cli_document_units_count.units_write, CliDocumentUnitsCountCfg, .out = out,
                       .units = args->view->line_units + start, .count = end - start);
        }
        else
        {
            mmgr_line_put(out, "\"\"");
        }
        mmgr_line_ch(out, ']');
    }
}

int cli_view_frame_write(const CliViewOutCfg *args)
{
    const char *key = args->key;
    VerbaLinea *out = args->out;
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = key);
    if (view == NULL)
    {
        return 0;
    }
    view->seq += 1ull;
    EMBED_CALL(view_folds_refresh, ViewCtx, .view = view);
    const unsigned long long lines = EMBED_CALL(view_lines, ViewCtx, .view_read = view);
    view->top = EMBED_CALL(view_shown_up, ViewCtx, .view_read = view,
                           .line = (view->top < lines) ? view->top : ((lines != 0ull) ? lines - 1ull : 0ull));
    const unsigned long long from = view->top;
    const unsigned long long rows = (view->rows != 0u) ? view->rows : 1u;
    // the rows: the lines shown from the top, `rows` of them at the most, by their numbers, in a cell with the room
    // for a paged text's lines read and for the folds that open on them
    CliViewRows shown = {NULL, NULL, 0ull};
    shown.number = g_cli_cells->persistent_buf_alloc(
        rows * (sizeof(*shown.number) + sizeof(*shown.text) + sizeof(CliViewFoldSeen)), &g_cli_views_cells);
    shown.text = (shown.number != NULL) ? (const char **)(shown.number + rows) : NULL;
    CliViewFoldsSeen folds = {&shown, (shown.text != NULL) ? (CliViewFoldSeen *)(shown.text + rows) : NULL, 0ull};
    for (unsigned long long line = from; (shown.number != NULL) && (shown.count < rows) && (line < lines);)
    {
        shown.number[shown.count] = line;
        shown.count += 1ull;
        const unsigned long long next = EMBED_CALL(view_down, ViewCtx, .view_read = view, .line = line, .count = 1ull);
        if (next <= line)
        {
            break;
        }
        line = next;
    }
    const unsigned long long to = (shown.count != 0ull) ? shown.number[shown.count - 1ull] + 1ull : from;
    mmgr_line_put(out, "{\"key\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = view->key,
               .length = cellul.len(&(CatenaFinitaCfg){.src = view->key, .cap = sizeof(view->key)}));
    mmgr_line_put(out, ",\"seq\":");
    mmgr_line_decimal(out, view->seq, 1u);
    mmgr_line_put(out, ",\"top\":");
    mmgr_line_decimal(out, view->top, 1u);
    mmgr_line_put(out, ",\"lines\":");
    mmgr_line_decimal(out, lines, 1u);
    mmgr_line_put(out, ",\"from\":");
    mmgr_line_decimal(out, from, 1u);
    mmgr_line_put(out, ",\"rows\":[");
    if (view->paged)
    {
        EMBED_CALL(view_paged_rows_write, ViewCtx, .view_read = view, .shown = &shown, .out = out);
    }
    else
    {
        const CliStoredText *const one =
            EMBED_CALL(cli_document_key_internal.store, CliDocumentKeyCtx, .key = view->key);
        for (unsigned long long at = 0ull; (one != NULL) && (at < shown.count); at += 1ull)
        {
            const unsigned long long line = shown.number[at];
            mmgr_line_put(out, (at > 0ull) ? "," : "");
            if (line < one->lines.lines)
            {
                EMBED_CALL(cli_document_units_count.units_write, CliDocumentUnitsCountCfg, .out = out,
                           .units = one->lines.unit + one->lines.start[line], .count = one->lines.length[line]);
            }
            else
            {
                mmgr_line_put(out, "\"\"");
            }
        }
    }
    mmgr_line_put(out, "],\"numbers\":[");
    for (unsigned long long at = 0ull; at < shown.count; at += 1ull)
    {
        mmgr_line_put(out, (at > 0ull) ? "," : "");
        mmgr_line_decimal(out, shown.number[at], 1u);
    }
    mmgr_line_put(out, "],\"folds\":[");
    if (folds.seen != NULL)
    {
        (void)EMBED_CALL(cli_document_context.folds_each, CliDocumentContextCfg, .key = view->key,
                         .each_fold_each = view_fold_visit, .context = &folds);
        EMBED_CALL(view_folds_write, ViewCtx, .view = view, .folds = &folds, .out = out);
    }
    mmgr_line_ch(out, ']');
    mmgr_line_put(out, ",\"cursor\":");
    EMBED_CALL(view_place_write, ViewCtx, .out = out, .place = view->cursor);
    mmgr_line_put(out, ",\"anchor\":");
    EMBED_CALL(view_place_write, ViewCtx, .out = out, .place = view->anchor);
    CliPlace pair[2] = {{0ull, 0ull}, {0ull, 0ull}};
    mmgr_line_put(out, ",\"pair\":");
    if (EMBED_CALL(view_pair, ViewCtx, .view_read = view, .pair = pair))
    {
        mmgr_line_ch(out, '[');
        EMBED_CALL(view_place_write, ViewCtx, .out = out, .place = pair[0]);
        mmgr_line_ch(out, ',');
        EMBED_CALL(view_place_write, ViewCtx, .out = out, .place = pair[1]);
        mmgr_line_ch(out, ']');
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    // the indentation: 1 for tabs, 0 for spaces, null where nothing set it, and the step
    mmgr_line_put(out, ",\"indent\":{\"tabs\":");
    mmgr_line_put(out, (view->indent.tabs == 1) ? "1" : ((view->indent.tabs == 0) ? "0" : "null"));
    mmgr_line_put(out, ",\"size\":");
    mmgr_line_decimal(out, EMBED_CALL(view_tab, ViewCtx, .view_read = view), 1u);
    mmgr_line_ch(out, '}');
    // find's bar and the matches among the rows
    if (view->find_open)
    {
        EMBED_CALL(view_search, ViewCtx, .view = view);
    }
    mmgr_line_put(out, ",\"find\":");
    if (view->find_open)
    {
        mmgr_line_put(out, "{\"open\":1,\"focus\":");
        mmgr_line_put(out, view->find_focus ? "1" : "0");
        mmgr_line_put(out, ",\"query\":");
        EMBED_CALL(cli_document_units_count.units_write, CliDocumentUnitsCountCfg, .out = out,
                   .units = view->find_query, .count = view->find_length);
        mmgr_line_put(out, ",\"at\":");
        mmgr_line_decimal(out, view->find_at, 1u);
        mmgr_line_put(out, view->find_regex ? ",\"regex\":1" : ",\"regex\":0");
        mmgr_line_put(out, view->find_case ? ",\"case\":1" : ",\"case\":0");
        mmgr_line_put(out, view->find_word ? ",\"word\":1" : ",\"word\":0");
        mmgr_line_put(out, ",\"count\":");
        mmgr_line_decimal(out, view->matches, 1u);
        mmgr_line_put(out, ",\"current\":");
        if (view->current >= 0ll)
        {
            // the current match is among the matches, which are counted
            mmgr_line_decimal(out, (unsigned long long)view->current, 1u);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_ch(out, '}');
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"matches\":[");
    int any_match = 0;
    for (unsigned long long at = 0ull; view->find_open && (at < view->matches); at += 1ull)
    {
        unsigned long long low = 0ull;
        unsigned long long high = shown.count;
        while (low < high)
        {
            const unsigned long long middle = low + ((high - low) / 2ull);
            if (shown.number[middle] < view->match[at][0])
            {
                low = middle + 1ull;
            }
            else
            {
                high = middle;
            }
        }
        if ((low < shown.count) && (shown.number[low] == view->match[at][0]))
        {
            mmgr_line_put(out, any_match ? ",[" : "[");
            mmgr_line_decimal(out, view->match[at][0], 1u);
            mmgr_line_ch(out, ',');
            mmgr_line_decimal(out, view->match[at][1], 1u);
            mmgr_line_ch(out, ',');
            mmgr_line_decimal(out, view->match[at][2], 1u);
            mmgr_line_ch(out, ']');
            any_match = 1;
        }
    }
    mmgr_line_ch(out, ']');
    mmgr_line_put(out, ",\"colors\":");
    // the lines from the top to the last row's are the lines the walk hands runs on for, and the rows' runs are written
    // a run of rows at a time, each run's lines one after another
    if (view->paged)
    {
        (void)EMBED_CALL(cli_document_key_from.page, CliDocumentKeyFromCfg, .key = view->key, .from = from, .to = to);
    }
    else
    {
        (void)EMBED_CALL(cli_document_key_from.cache, CliDocumentKeyFromCfg, .key = view->key, .from = from, .to = to);
    }
    mmgr_line_ch(out, '[');
    int any = 0;
    for (unsigned long long at = 0ull; at < shown.count;)
    {
        unsigned long long end = at;
        while ((end + 1ull < shown.count) && (shown.number[end + 1ull] == shown.number[end] + 1ull))
        {
            end += 1ull;
        }
        const unsigned long long begun = out->at;
        if (EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = view->key,
                       .first = shown.number[at], .last = shown.number[end] + 1ull, .mirrored = 1, .out = out) &&
            mmgr_line_ok(out) && (out->at > begun + 2ull))
        {
            // the run's array joined to the one written: its opening bracket a comma after another's, else taken out,
            // and its closing bracket taken out
            if (any)
            {
                out->out[begun] = ',';
            }
            else
            {
                for (unsigned long long moved = begun; moved + 1ull < out->at; moved += 1ull)
                {
                    out->out[moved] = out->out[moved + 1ull];
                }
                out->at -= 1ull;
            }
            out->at -= 1ull;
            any = 1;
        }
        else if (mmgr_line_ok(out))
        {
            out->at = begun;
        }
        at = end + 1ull;
    }
    mmgr_line_ch(out, ']');
    mmgr_line_ch(out, '}');
    g_cli_cells->persistent_buf_release(shown.number, &g_cli_views_cells);
    return 1;
}

int cli_view_cursor(const CliViewCursorCfg *args)
{
    CliPlace *cursor = args->cursor;
    CliPlace *anchor = args->anchor;
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
#define CLI_VIEW_MAP_ROOM (1024ull * 1024ull)

// a whole number read off the JSON node `node`, `otherwise` where it is not one
EMBED_INLINE unsigned long long view_json_whole(const ViewCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = node,
                    .kind = CFG_JSON_NUMBER))
    {
        return args->otherwise;
    }
    const CliJsonNode *const number = &json->node[node];
    const char *const text = json->text + number->text;
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; at < number->length; at += 1ull)
    {
        if ((text[at] < '0') || (text[at] > '9'))
        {
            return args->otherwise;
        }
        // a digit's value is its distance from '0'
        value = (value * 10ull) + (unsigned long long)(text[at] - '0');
    }
    return (number->length != 0ull) ? value : args->otherwise;
}

// the flat runs array of the line `line` among the ranges `colors`, each `{"from", "runs"}`, CLI_JSON_NONE where none
// colors it
EMBED_INLINE unsigned int view_map_runs_get(const ViewCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int ranges = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = args->colors, .kind = CFG_JSON_ARRAY)
                                    ? json->node[args->colors].count
                                    : 0u;
    unsigned int found = CLI_JSON_NONE;
    for (unsigned int range = 0u; range < ranges; range += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = args->colors, .index = range);
        const unsigned long long start =
            EMBED_CALL(view_json_whole, ViewCtx, .json = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = one, .name = "from"),
                       .otherwise = 0ull);
        const unsigned int runs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = one, .name = "runs");
        const unsigned int lines = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                              .json_read = json, .node = runs, .kind = CFG_JSON_ARRAY)
                                       ? json->node[runs].count
                                       : 0u;
        if ((args->line >= start) && (args->line < start + lines))
        {
            found = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                               .array = runs, .index = (unsigned int)(args->line - start));
        }
    }
    return found;
}

// one bar written, `[start, width, name]`
EMBED_INLINE void view_map_bar_write(const ViewCtx *args)
{
    int *any = args->any;
    mmgr_line_put(args->out, *any ? ",[" : "[");
    mmgr_line_decimal(args->out, args->start_value, 1u);
    mmgr_line_ch(args->out, ',');
    mmgr_line_decimal(args->out, args->width, 1u);
    mmgr_line_ch(args->out, ',');
    mmgr_line_decimal(args->out, args->name_u64, 1u);
    mmgr_line_ch(args->out, ']');
    *any = 1;
}

// the bars of `length` bytes of a line's `text` written as an array: each run of letters that are not blanks, within
// one run of `flat`, the line's runs as `[column, name, ...]`, a bar from its first cell as wide as its cells, a tab
// the cells to its next stop of `tab`
EMBED_INLINE void view_map_line_write(const ViewCtx *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    const CliJson *json = args->json;
    const unsigned int values = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = args->flat, .kind = CFG_JSON_ARRAY)
                                    ? json->node[args->flat].count
                                    : 0u;
    unsigned int run = 0u;
    unsigned long long unit = 0ull;
    unsigned long long cell = 0ull;
    unsigned long long begun = 0ull;
    unsigned long long begun_name = 0ull;
    int open = 0;
    int any = 0;
    mmgr_line_ch(args->out, '[');
    for (unsigned long long at = 0ull; at < length;)
    {
        unsigned long long took = 0ull;
        const unsigned int letter =
            EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = length, .at = at, .length = &took);
        // the run the letter stands in: the last whose column is at or before it
        while (((2u * (run + 1u)) + 1u < values) &&
               (EMBED_CALL(view_json_whole, ViewCtx, .json = json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = args->flat, .index = 2u * (run + 1u)),
                           .otherwise = 0ull) <= unit))
        {
            run += 1u;
        }
        const unsigned long long name =
            (values >= 2u)
                ? EMBED_CALL(view_json_whole, ViewCtx, .json = json,
                             .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                .json_read = json, .array = args->flat, .index = (2u * run) + 1u),
                             .otherwise = 0ull)
                : 0ull;
        const int blank = (letter == ' ') || (letter == '\t');
        const unsigned long long wide = (letter == '\t') ? (args->tab - (cell % args->tab)) : 1ull;
        if (open && (blank || (name != begun_name)))
        {
            EMBED_CALL(view_map_bar_write, ViewCtx, .out = args->out, .any = &any, .start_value = begun,
                       .width = cell - begun, .name_u64 = begun_name);
            open = 0;
        }
        if (!blank && !open)
        {
            begun = cell;
            begun_name = name;
            open = 1;
        }
        cell += wide;
        unit += (letter >= 0x10000u) ? 2ull : 1ull;
        at += (took != 0ull) ? took : 1ull;
    }
    if (open)
    {
        EMBED_CALL(view_map_bar_write, ViewCtx, .out = args->out, .any = &any, .start_value = begun,
                   .width = cell - begun, .name_u64 = begun_name);
    }
    mmgr_line_ch(args->out, ']');
}

int cli_view_map_write(const CliViewOutCfg *args)
{
    const char *key = args->key;
    unsigned long long first = args->first;
    unsigned long long last = args->last;
    VerbaLinea *out = args->out;
    CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = key);
    if (view == NULL)
    {
        return 0;
    }
    const unsigned int tab = EMBED_CALL(view_tab, ViewCtx, .view_read = view);
    const int paged = view->paged;
    mmgr_line_put(out, "{\"from\":");
    mmgr_line_decimal(out, first, 1u);
    mmgr_line_put(out, ",\"bars\":[");
    // the lines' runs read as the branch writes them, and their texts, a paged text's from its pages
    const unsigned long long count = (last >= first) ? last - first + 1ull : 0ull;
    char *const room = g_cli_cells->temporary_buf_alloc(CLI_VIEW_MAP_ROOM, &g_cli_views_cells);
    const char **const line =
        g_cli_cells->temporary_buf_alloc(((count != 0ull) ? count : 1ull) * sizeof(*line), &g_cli_views_cells);
    VerbaLinea runs = {room, CLI_VIEW_MAP_ROOM, 0ull};
    CliJson json;
    unsigned int colors = CLI_JSON_NONE;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    if ((room != NULL) &&
        EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = key, .first = first,
                   .last = last, .mirrored = 0, .out = &runs) &&
        mmgr_line_ok(&runs))
    {
        colors = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take_temporary, CliJsonTreeTextSizeErrMsgCfg,
                            .json = &json, .text = runs.out, .size = runs.at, .nodes_more = 0u, .text_more = 0ull,
                            .err_msg = &err_msg, .owner = &g_cli_views_cells);
    }
    unsigned long long size = 0ull;
    unsigned long long read = 0ull;
    char state[64];
    char *const texts =
        (line == NULL)
            ? NULL
            : (paged ? EMBED_CALL(cli_pages_from.lines_take, CliPagesFromCfg, .key = key, .from = first,
                                  .to = last + 1ull, .line = line, .count = &read, .owner = &g_cli_views_cells)
                     : EMBED_CALL(cli_document_key.lines_take, CliDocumentKeyCfg, .key = key, .first = first,
                                  .last = last, .size_pointer = &size, .state = state, .room = sizeof(state),
                                  .owner = &g_cli_views_cells));
    const char *rest = texts;
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        mmgr_line_put(out, (at > 0ull) ? "," : "");
        const char *text = "";
        unsigned long long length = 0ull;
        if (paged && (texts != NULL) && (at < read))
        {
            text = line[at];
            length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
        }
        else if (!paged && (rest != NULL))
        {
            // the lines come joined by newlines, each but the last followed by one
            const char *const end = texts + size;
            text = rest;
            rest += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = rest, .at = 0ull,
                               .size = (unsigned long long)(end - rest), .set = "\n");
            length = (unsigned long long)(rest - text);
            rest = (rest < end) ? rest + 1 : NULL;
        }
        EMBED_CALL(view_map_line_write, ViewCtx, .text = text, .length = length, .json = &json,
                   .flat = (colors != CLI_JSON_NONE) ? EMBED_CALL(view_map_runs_get, ViewCtx, .json = &json,
                                                                  .colors = colors, .line = first + at)
                                                     : CLI_JSON_NONE,
                   .tab = tab, .out = out);
    }
    mmgr_line_put(out, "]}");
    if (colors != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    }
    g_cli_cells->persistent_buf_release(texts, &g_cli_views_cells);
    g_cli_cells->persistent_buf_release(line, &g_cli_views_cells);
    g_cli_cells->persistent_buf_release(room, &g_cli_views_cells);
    return 1;
}

int cli_view_path(const CliViewPathKindCfg *args)
{
    const CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = args->key);
    if (view != NULL)
    {
        VerbaLinea line = {args->path_out, CLI_PATH_MAX_LEN, 0ull};
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
        VerbaLinea line = {view->served, sizeof(view->served), 0ull};
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
        VerbaLinea line = {args->full_out, CLI_PATH_MAX_LEN, 0ull};
        mmgr_line_put(&line, view->served);
        (void)mmgr_line_finish(&line);
    }
    return served;
}

int cli_view_serving(const CliViewFullCfg *args)
{
    int serving = 0;
    for (unsigned int at = 0u; !serving && (s_views != NULL) && (at < CLI_VIEWS); at += 1u)
    {
        serving =
            s_views[at].used && cellul.eq(&(CatenaFinitaCfg){
                                    .src = s_views[at].served, .other = args->full, .cap = sizeof(s_views[at].served)});
    }
    return serving;
}

int cli_view_changed_take(const CliViewCfg *args)
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
    while ((start > 0ull) && EMBED_CALL(view_word_unit, ViewCtx,
                                        .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->line,
                                                           .col = start - 1ull)))
    {
        start -= 1ull;
    }
    while ((end < length) &&
           EMBED_CALL(view_word_unit, ViewCtx,
                      .unit = EMBED_CALL(view_unit, ViewCtx, .view = args->view, .line = args->line, .col = end)))
    {
        end += 1ull;
    }
    *from = start;
    *to = end;
    return end > start;
}

int cli_view_place_at(const CliViewRowCfg *args)
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
        VerbaLinea text = {args->word, args->room, 0ull};
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
    while ((start > 0ull) &&
           EMBED_CALL(view_word_unit, ViewCtx,
                      .unit = EMBED_CALL(view_unit, ViewCtx, .view = view, .line = line, .col = start - 1ull)))
    {
        start -= 1ull;
    }
    *from = start;
    VerbaLinea text = {args->prefix, args->room, 0ull};
    if (end > start)
    {
        EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &text,
                   .units = view->line_units + start, .count = end - start);
    }
    (void)mmgr_line_finish(&text);
    return 1;
}

// whether a byte of UTF-8 is part of a word: a letter, a digit, `_`, `$`, or any byte of a letter past ASCII
EMBED_INLINE int view_word_byte(const ViewCtx *args)
{
    const unsigned char value = (unsigned char)args->byte;
    return ((value >= 'a') && (value <= 'z')) || ((value >= 'A') && (value <= 'Z')) ||
           ((value >= '0') && (value <= '9')) || (value == '_') || (value == '$') || (value >= 0x80u);
}

// the units a byte of UTF-8 adds: one for a byte of ASCII or the lead byte of a letter in the basic plane, two for
// the lead byte of a letter past it, none for a continuation byte
EMBED_INLINE unsigned long long view_byte_units(const ViewCtx *args)
{
    const unsigned char value = (unsigned char)args->byte;
    if (value < 0x80u)
    {
        return 1ull;
    }
    if (value >= 0xF0u)
    {
        return 2ull;
    }
    return (value >= 0xC0u) ? 1ull : 0ull;
}

// the hash table's entries, a power of two above the most words, each a word's offset in the text plus one, 0 for an
// empty entry, and its length
#define CLI_VIEW_WORD_TABLE 8192ull

// whether `length` bytes of `word` at `text + at` are in the table, put there where not: 1 where they were already
EMBED_INLINE int view_word_visit(const ViewCtx *args)
{
    unsigned long long hash = 1469598103934665603ull;
    for (unsigned long long each = 0ull; each < args->length; each += 1ull)
    {
        hash = (hash ^ (unsigned char)args->text[args->at + each]) * 1099511628211ull;
    }
    unsigned long long entry = hash & (CLI_VIEW_WORD_TABLE - 1ull);
    for (;;)
    {
        if (args->table[entry][0] == 0ull)
        {
            args->table[entry][0] = args->at + 1ull;
            args->table[entry][1] = args->length;
            return 0;
        }
        if ((args->table[entry][1] == args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = args->text + args->table[entry][0] - 1ull,
                                     .other = args->text + args->at,
                                     .bytes = args->length}) == 0))
        {
            return 1;
        }
        entry = (entry + 1ull) & (CLI_VIEW_WORD_TABLE - 1ull);
    }
}

int cli_view_words_write(const CliViewOutCfg *args)
{
    const char *key = args->key;
    const CliView *const view = EMBED_CALL(view_get, ViewCtx, .key = key);
    if (view == NULL)
    {
        return 0;
    }
    const CliPlace cursor = view->cursor;
    unsigned long long low = 0ull;
    unsigned long long high = 0ull;
    (void)EMBED_CALL(cli_document_key_low.bounds, CliDocumentKeyLowCfg, .key = key, .low = &low, .high = &high);
    const unsigned long long reach = (cursor.line > CLI_VIEW_WORD_LINES) ? cursor.line - CLI_VIEW_WORD_LINES : 0ull;
    const unsigned long long first = (reach > low) ? reach : low;
    const unsigned long long far = cursor.line + CLI_VIEW_WORD_LINES;
    const unsigned long long last = ((high != 0ull) && (far >= high)) ? high - 1ull : far;
    char state[CLI_VIEW_STATE];
    unsigned long long size = 0ull;
    char *const text = (last >= first) ? EMBED_CALL(cli_document_key.lines_take, CliDocumentKeyCfg, .key = key,
                                                    .first = first, .last = last, .size_pointer = &size, .state = state,
                                                    .room = sizeof(state), .owner = &g_cli_views_cells)
                                       : NULL;
    unsigned long long (*const table)[2] =
        g_cli_cells->temporary_buf_alloc(CLI_VIEW_WORD_TABLE * sizeof(*table), &g_cli_views_cells);
    mmgr_line_ch(args->out, '[');
    if ((text != NULL) && (table != NULL))
    {
        memor.set(&(MemoriaCfg){.dst = table, .val = 0, .bytes = CLI_VIEW_WORD_TABLE * sizeof(*table)});
        unsigned int written = 0u;
        unsigned long long line = first;
        unsigned long long units = 0ull;
        for (unsigned long long at = 0ull; (at < size) && (written < CLI_VIEW_WORDS);)
        {
            if (text[at] == '\n')
            {
                line += 1ull;
                units = 0ull;
                at += 1ull;
                continue;
            }
            if (!EMBED_CALL(view_word_byte, ViewCtx, .byte = text[at]))
            {
                units += EMBED_CALL(view_byte_units, ViewCtx, .byte = text[at]);
                at += 1ull;
                continue;
            }
            const unsigned long long start = at;
            const unsigned long long start_units = units;
            unsigned long long letters = 0ull;
            while ((at < size) && EMBED_CALL(view_word_byte, ViewCtx, .byte = text[at]))
            {
                const unsigned long long added = EMBED_CALL(view_byte_units, ViewCtx, .byte = text[at]);
                letters += (added != 0ull) ? 1ull : 0ull;
                units += added;
                at += 1ull;
            }
            // the word the cursor stands in is being typed and is left out, as is any word of one letter
            const int typed = (line == cursor.line) && (start_units <= cursor.col) && (cursor.col <= units);
            if (typed || (letters < 2ull) ||
                EMBED_CALL(view_word_visit, ViewCtx, .text = text, .table = table, .at = start, .length = at - start))
            {
                continue;
            }
            mmgr_line_put(args->out, (written > 0u) ? ",{\"label\":" : "{\"label\":");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                       .text = text + start, .length = at - start);
            mmgr_line_put(args->out, ",\"kind\":\"word\",\"insert\":");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                       .text = text + start, .length = at - start);
            mmgr_line_ch(args->out, '}');
            written += 1u;
        }
    }
    mmgr_line_ch(args->out, ']');
    g_cli_cells->persistent_buf_release(table, &g_cli_views_cells);
    g_cli_cells->persistent_buf_release(text, &g_cli_views_cells);
    return 1;
}
