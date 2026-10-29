// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// view.c: the editor's view of a stored text. The views are kept in one cell under a lock of their own; a view reads
// the text's lines under the documents' lock and edits it through cli_document_edit, one edit a time, each recorded in
// a step with the text it took out, so the step is undone by writing that text back over what it put in. A column is
// in UTF-16 units, as the text's lines are kept, and the pointer's cell is read into a column with tabs laid to their
// stops
#include "view.h"

#include "../code/symbols.h"
#include "../command_line/cells.h"
#include "../plugins/plugins.h"
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "document_internal.h"
#include "editorconfig.h"
#include "pages.h"

// the size of the views table, the most edits a step holds before it closes, the kinds of edit, the lines read
// about the cursor each way for the bracket pair at it, the room for the state the first of them is entered in, the
// most pairs and quotes a language has, and the room for its pattern of a line that opens a block
#define CLI_VIEWS 64u
#define CLI_VIEW_STEP_EDITS 256u
#define CLI_VIEW_PAIR_LINES 3000ull
#define CLI_VIEW_STATE 1024u
#define CLI_VIEW_PAIRS 16u
#define CLI_VIEW_QUOTES 8u
#define CLI_VIEW_PATTERN 512u
// the matches find keeps at the most
#define CLI_VIEW_MATCHES 100000ull
#define CLI_VIEW_KIND_NONE 0u
#define CLI_VIEW_KIND_TYPE 1u
#define CLI_VIEW_KIND_SPACE 2u
#define CLI_VIEW_KIND_DELETE 3u
#define CLI_VIEW_KIND_DELETE_FORWARD 4u

// An edit a step holds: the span it replaced, where the text it put in ends, and that text and the text it took out
// in a cell of the edit's own
typedef struct
{
    CliPlace from;
    CliPlace to;
    CliPlace end;
    char *put;
    unsigned long long put_length;
    char *took;
    unsigned long long took_length;
} CliViewEdit;

// A step: its edits in a cell of their own, how many and the room, the kind of edit it holds and when its last was
// made, and where the cursor and the anchor stood before it and after it
typedef struct
{
    CliViewEdit *edit;
    unsigned int edits;
    unsigned int room;
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

// A view: the key of its text, the path of its file, which names its language, and whether the text is paged, its
// lines read through the pages; the language's pairs, each an opener and its closer, its quotes, and its pattern of a
// line that opens a block, as its plugin names them, in units; its top, rows and columns; the cursor and the anchor;
// the column the cursor keeps as it moves up and down, in cells; the indentation; the frames written; the steps, in a
// cell of their own, how many are stored and how many of them are done, those past the done ones being undone; the folds
// closed, in a cell of their own in the order of their first lines, how many and the room; find: whether its bar is
// open and whether the keys are its own, its query in a cell of its own, in units, its length, the room and where its
// cursor stands, whether it seeks a pattern, with case and whole words, whether the matches are to be found anew, the
// matches, each a line, a from and a to column, in a cell of their own, how many and the room, and the current one, -1
// for none; and the line read last, its units in a cell of the view's own, the room it has, which line it is and
// whether it is known: a line's units are read once for the moves along it
typedef struct
{
    int used;
    char key[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    int paged;
    unsigned int pair_open[CLI_VIEW_PAIRS];
    unsigned int pair_close[CLI_VIEW_PAIRS];
    unsigned int pairs;
    unsigned int quote[CLI_VIEW_QUOTES];
    unsigned int quotes;
    char indent_after[CLI_VIEW_PATTERN];
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
    int goal_kept;
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

static CliView *s_views;
static CliLock s_lock = CLI_LOCK_INIT;
static CellOwner s_owner = MMGR_CELL_OWNER("cli_views");

// the view open under `key`, or NULL; under the lock
static CliView *view_of(const char *key)
{
    if (s_views == NULL)
    {
        return NULL;
    }
    for (unsigned int at = 0u; at < CLI_VIEWS; at += 1u)
    {
        if (s_views[at].used && cellul.eq(&(CatenaFinitaCfg){.src = s_views[at].key, .other = key, .cap = CLI_NAME_MAX_LEN}))
        {
            return &s_views[at];
        }
    }
    return NULL;
}

// whether a key's own name is `wanted`
static int view_named(const char *name, const char *wanted)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = name, .other = wanted, .cap = 64ull});
}

// the steps from `from` on let go, their cells given back
static void view_steps_cut(CliView *view, unsigned int from)
{
    for (unsigned int at = from; at < view->steps; at += 1u)
    {
        CliViewStep *const step = &view->step[at];
        for (unsigned int edit = 0u; edit < step->edits; edit += 1u)
        {
            g_cli_cells->persistent_buf_release(step->edit[edit].put, &s_owner);
        }
        g_cli_cells->persistent_buf_release(step->edit, &s_owner);
    }
    view->steps = (from < view->steps) ? from : view->steps;
    view->done = (view->done < view->steps) ? view->done : view->steps;
}

// A language's plugin sought among the plugins: its id, and the view its pairs, quotes and pattern are read into
typedef struct
{
    const char *id;
    CliView *view;
} CliViewPluginSought;

// the first letter of the string node `node` as a unit, 0 where the node is no string or it is empty
static unsigned int view_letter_of(const CliJson *json, unsigned int node)
{
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(json, node, &length);
    unsigned long long took = 0ull;
    return ((text != NULL) && (length != 0ull)) ? cli_text_letter(text, length, 0ull, &took) : 0u;
}

// the pairs, quotes and pattern of the plugin whose id is sought read into the view; where the plugin names none,
// the defaults apply: the brackets, the two quotes and the pattern of an opener at a line's end
static void view_plugin_read(void *context, const CliPlugin *plugin)
{
    CliViewPluginSought *const sought = context;
    CliView *const view = sought->view;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = plugin->id, .other = sought->id, .cap = CLI_NAME_MAX_LEN}))
    {
        return;
    }
    char why_text[256];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    const unsigned int root = cli_json_taken(&json, plugin->text, plugin->size, 0u, 0ull, &why);
    if (root == CLI_JSON_NONE)
    {
        return;
    }
    const unsigned int pairs = cli_json_member(&json, root, "pairs");
    const unsigned int quotes = cli_json_member(&json, root, "quotes");
    const unsigned int after = cli_json_member(&json, root, "indentAfter");
    view->pairs = 0u;
    view->quotes = 0u;
    if (cli_json_is(&json, pairs, CFG_JSON_ARRAY))
    {
        for (unsigned int at = 0u; (at < json.node[pairs].count) && (view->pairs < CLI_VIEW_PAIRS); at += 1u)
        {
            unsigned long long length = 0ull;
            const char *const text = cli_json_string(&json, cli_json_item(&json, pairs, at), &length);
            unsigned long long took = 0ull;
            const unsigned int open = ((text != NULL) && (length != 0ull)) ? cli_text_letter(text, length, 0ull, &took) : 0u;
            const unsigned int close = ((open != 0u) && (took < length)) ? cli_text_letter(text, length, took, &took) : 0u;
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
    if (cli_json_is(&json, quotes, CFG_JSON_ARRAY))
    {
        for (unsigned int at = 0u; (at < json.node[quotes].count) && (view->quotes < CLI_VIEW_QUOTES); at += 1u)
        {
            const unsigned int quote = view_letter_of(&json, cli_json_item(&json, quotes, at));
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
    const char *const text = cli_json_string(&json, after, &length);
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
    cli_json_given(&json);
}

// the view's language read off the plugin the file at its path opens as; none where no plugin does
static void view_language_read(CliView *view)
{
    char id[CLI_NAME_MAX_LEN];
    CliLanguages languages;
    cli_plugins_languages(&languages);
    const int named = cli_plugins_language_for(&languages, view->path, id);
    cli_plugins_languages_given(&languages);
    if (named)
    {
        CliViewPluginSought sought = {id, view};
        cli_plugins_each(view_plugin_read, &sought);
    }
}

int cli_view_opened(const char *key, const char *path)
{
    cli_document_lock_take();
    const CliStoredText *const stored = cli_document_stored(key);
    const int paged = (stored != NULL) && stored->paged;
    cli_document_lock_give();
    if (stored == NULL)
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    if (s_views == NULL)
    {
        s_views = g_cli_cells->persistent_buf_alloc(CLI_VIEWS * sizeof(*s_views), &s_owner);
        if (s_views == NULL)
        {
            cli_lock_give(&s_lock);
            return 0;
        }
        memor.set(&(MemoriaCfg){.dst = s_views, .val = 0, .bytes = CLI_VIEWS * sizeof(*s_views)});
    }
    CliView *view = view_of(key);
    if (view != NULL)
    {
        view_steps_cut(view, 0u);
        g_cli_cells->persistent_buf_release(view->step, &s_owner);
        g_cli_cells->persistent_buf_release(view->fold, &s_owner);
        g_cli_cells->persistent_buf_release(view->find_query, &s_owner);
        g_cli_cells->persistent_buf_release(view->match, &s_owner);
        g_cli_cells->persistent_buf_release(view->line_units, &s_owner);
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
        cli_lock_give(&s_lock);
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
    cli_indent(path, &view->indent);
    view_language_read(view);
    cli_lock_give(&s_lock);
    return 1;
}

void cli_view_closed(const char *key)
{
    cli_lock_take(&s_lock);
    CliView *const view = view_of(key);
    if (view != NULL)
    {
        view_steps_cut(view, 0u);
        g_cli_cells->persistent_buf_release(view->step, &s_owner);
        g_cli_cells->persistent_buf_release(view->fold, &s_owner);
        g_cli_cells->persistent_buf_release(view->find_query, &s_owner);
        g_cli_cells->persistent_buf_release(view->match, &s_owner);
        g_cli_cells->persistent_buf_release(view->line_units, &s_owner);
        memor.set(&(MemoriaCfg){.dst = view, .val = 0, .bytes = sizeof(*view)});
    }
    cli_lock_give(&s_lock);
}

// the text's lines: how many, as far as the pages' index has read for a paged text
static unsigned long long view_lines(const CliView *view)
{
    if (view->paged)
    {
        unsigned long long lines = 0ull;
        int whole = 0;
        return cli_pages_counted(view->key, &lines, &whole) ? lines : 0ull;
    }
    CliShape shape;
    return cli_document_shape(view->key, &shape) ? shape.lines : 0ull;
}

// the closed fold that hides `line`, its first line above the line and its last at or below it, the outermost where
// more do, or NULL where none does
static const CliViewFold *view_hider(const CliView *view, unsigned long long line)
{
    for (unsigned int at = 0u; at < view->folds; at += 1u)
    {
        if ((view->fold[at].first < line) && (line <= view->fold[at].last))
        {
            return &view->fold[at];
        }
    }
    return NULL;
}

// `line` where no closed fold hides it, else going up the first line of the fold that hides it, and going down the
// line after the last it hides, the last line shown where none follows
static unsigned long long view_shown_up(const CliView *view, unsigned long long line)
{
    for (const CliViewFold *hider = view_hider(view, line); hider != NULL; hider = view_hider(view, line))
    {
        line = hider->first;
    }
    return line;
}

static unsigned long long view_shown_down(const CliView *view, unsigned long long line)
{
    const unsigned long long lines = view_lines(view);
    const unsigned long long last = (lines != 0ull) ? lines - 1ull : 0ull;
    for (const CliViewFold *hider = view_hider(view, line); hider != NULL; hider = view_hider(view, line))
    {
        if (hider->last >= last)
        {
            return view_shown_up(view, last);
        }
        line = hider->last + 1ull;
    }
    return (line > last) ? view_shown_up(view, last) : line;
}

// the line `count` lines shown below `line`, the last shown where fewer follow; and the one `count` lines shown above
// it, the first where fewer lead
static unsigned long long view_down(const CliView *view, unsigned long long line, unsigned long long count)
{
    const unsigned long long lines = view_lines(view);
    unsigned long long at = view_shown_up(view, line);
    for (unsigned long long step = 0ull; (step < count) && (at + 1ull < lines); step += 1ull)
    {
        const unsigned long long next = view_shown_down(view, at + 1ull);
        if (next <= at)
        {
            break;
        }
        at = next;
    }
    return at;
}

static unsigned long long view_up(const CliView *view, unsigned long long line, unsigned long long count)
{
    unsigned long long at = view_shown_up(view, line);
    for (unsigned long long step = 0ull; (step < count) && (at > 0ull); step += 1ull)
    {
        at = view_shown_up(view, at - 1ull);
    }
    return at;
}

// how many lines shown stand from `top` to before `line`, `most` at the most
static unsigned long long view_rows_to(const CliView *view, unsigned long long top, unsigned long long line,
                                       unsigned long long most)
{
    unsigned long long rows = 0ull;
    unsigned long long at = view_shown_up(view, top);
    while ((at < line) && (rows < most))
    {
        const unsigned long long next = view_down(view, at, 1ull);
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
static void view_fold_matched(void *context, unsigned long long first, unsigned long long last, int shown)
{
    CliViewFoldSought *const sought = context;
    (void)shown;
    if (first == sought->line)
    {
        sought->first = first;
        sought->last = last;
        sought->found = 1;
    }
}

static void view_fold_innermost(void *context, unsigned long long first, unsigned long long last, int shown)
{
    CliViewFoldSought *const sought = context;
    (void)shown;
    if ((first <= sought->line) && (sought->line <= last) && (!sought->found || (first > sought->first)))
    {
        sought->first = first;
        sought->last = last;
        sought->found = 1;
    }
}

// the closed fold opened on `first` given the last line the text's fold there hides now
static void view_fold_refreshed(void *context, unsigned long long first, unsigned long long last, int shown)
{
    CliView *const view = context;
    (void)shown;
    for (unsigned int at = 0u; at < view->folds; at += 1u)
    {
        if (view->fold[at].first == first)
        {
            view->fold[at].last = last;
        }
    }
}

// the closed folds' last lines read again from the text's folds, which its coloring moves; a closed fold the text no
// longer folds at stays until an edit touches it
static void view_folds_refreshed(CliView *view)
{
    if (view->folds != 0u)
    {
        (void)cli_document_folds_each(view->key, view_fold_refreshed, view);
    }
}

// a fold from `first` to `last` put among the closed, in the order of their first lines, the one there given the last
// where one is: 1, or 0 where the cell could not be taken
static int view_fold_put(CliView *view, unsigned long long first, unsigned long long last)
{
    unsigned int at = 0u;
    while ((at < view->folds) && (view->fold[at].first < first))
    {
        at += 1u;
    }
    if ((at < view->folds) && (view->fold[at].first == first))
    {
        view->fold[at].last = last;
        return 1;
    }
    if (view->folds == view->folds_room)
    {
        const unsigned int room = (view->folds_room != 0u) ? view->folds_room * 2u : 16u;
        CliViewFold *const grown = g_cli_cells->persistent_buf_alloc(room * sizeof(*grown), &s_owner);
        if (grown == NULL)
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = view->fold, .bytes = view->folds * sizeof(*grown)});
        g_cli_cells->persistent_buf_release(view->fold, &s_owner);
        view->fold = grown;
        view->folds_room = room;
    }
    for (unsigned int after = view->folds; after > at; after -= 1u)
    {
        view->fold[after] = view->fold[after - 1u];
    }
    view->fold[at] = (CliViewFold){first, last};
    view->folds += 1u;
    return 1;
}

// the closed folds moved with `edit`: those below the lines it writes over moved by the lines it adds less those it
// takes out; one it touches, its first line with lines added or taken, or the lines it hides, opened
static void view_folds_edited(CliView *view, const CliEdit *edit)
{
    unsigned long long added = 0ull;
    for (unsigned long long at = 0ull; at < edit->length; at += 1ull)
    {
        added += (edit->text[at] == '\n') ? 1ull : 0ull;
    }
    const unsigned long long removed = edit->to.line - edit->from.line;
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < view->folds; at += 1u)
    {
        CliViewFold fold = view->fold[at];
        if (edit->to.line < fold.first)
        {
            // a fold below the edit stands past the lines it takes out: the move stays in the text
            fold.first = fold.first + added - removed;
            fold.last = fold.last + added - removed;
        }
        else if ((edit->from.line < fold.first) || (edit->from.line <= fold.last))
        {
            const int on_first = (edit->from.line == fold.first) && (edit->to.line == fold.first) && (added == 0ull);
            if (!on_first)
            {
                continue;
            }
        }
        view->fold[kept] = fold;
        kept += 1u;
    }
    view->folds = kept;
}

// `size` bytes of UTF-8 as UTF-16 units into `units`, which is sized for `room`: their count, a letter past the basic
// plane two units
static unsigned long long view_units_of(const char *text, unsigned long long size, unsigned short *units,
                                        unsigned long long room)
{
    unsigned long long at = 0ull;
    unsigned long long count = 0ull;
    while ((at < size) && (count < room))
    {
        unsigned long long took = 0ull;
        const unsigned int letter = cli_text_letter(text, size, at, &took);
        at += (took != 0ull) ? took : 1ull;
        if (letter >= 0x10000u)
        {
            if (count + 2ull > room)
            {
                break;
            }
            // a letter past the basic plane as its two surrogates, each a unit
            units[count] = (unsigned short)(0xd800u + ((letter - 0x10000u) >> 10u));
            units[count + 1ull] = (unsigned short)(0xdc00u + ((letter - 0x10000u) & 0x3ffu));
            count += 2ull;
        }
        else
        {
            // a letter of the basic plane is one unit
            units[count] = (unsigned short)letter;
            count += 1ull;
        }
    }
    return count;
}

// the view's line cell given room for `units`: 1, or 0 where it could not be taken
static int view_units_room(CliView *view, unsigned long long units)
{
    if (view->units_room >= units)
    {
        return 1;
    }
    const unsigned long long room = (units > 256ull) ? units : 256ull;
    unsigned short *const grown = g_cli_cells->persistent_buf_alloc(room * sizeof(*grown), &s_owner);
    if (grown == NULL)
    {
        return 0;
    }
    g_cli_cells->persistent_buf_release(view->line_units, &s_owner);
    view->line_units = grown;
    view->units_room = room;
    return 1;
}

// the units of `line` read into the view's line cell, where they are not the line read last: a stored text's copied
// under the documents' lock, a paged text's read through the pages and made units
static void view_line_read(CliView *view, unsigned long long line)
{
    if (view->line_known && (view->line_cached == line))
    {
        return;
    }
    view->line_known = 1;
    view->line_cached = line;
    view->line_length = 0ull;
    if (view->paged)
    {
        const char *text = NULL;
        unsigned long long count = 0ull;
        char *const cell = cli_pages_lines_taken(view->key, line, line + 1ull, &text, &count);
        if ((cell != NULL) && (count != 0ull))
        {
            const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
            if (view_units_room(view, size))
            {
                view->line_length = view_units_of(text, size, view->line_units, view->units_room);
            }
        }
        g_cli_cells->persistent_buf_release(cell, &s_owner);
        return;
    }
    cli_document_lock_take();
    const CliStoredText *const one = cli_document_stored(view->key);
    if ((one != NULL) && (line < one->lines.lines) && view_units_room(view, one->lines.length[line]))
    {
        view->line_length = one->lines.length[line];
        memor.cpy(&(MemoriaCfg){.dst = view->line_units,
                                .src = one->lines.unit + one->lines.start[line],
                                .bytes = view->line_length * sizeof(*view->line_units)});
    }
    cli_document_lock_give();
}

// a line's length in units, and the unit at `col` of it, 0 past the line's end
static unsigned long long view_length(CliView *view, unsigned long long line)
{
    view_line_read(view, line);
    return view->line_length;
}

static unsigned int view_unit(CliView *view, unsigned long long line, unsigned long long col)
{
    view_line_read(view, line);
    return (col < view->line_length) ? view->line_units[col] : 0u;
}

// the tab's width in cells and the indentation's step, as the indentation sets them
static unsigned int view_tab(const CliView *view)
{
    return (view->indent.sized && (view->indent.size != 0u)) ? view->indent.size : CLI_VIEW_TAB;
}

// the cell the unit at `col` of `line` starts in, tabs laid to their stops; and the column of the unit standing at
// `cell`, the line's end where the cell is past it
static unsigned long long view_cell_of(CliView *view, unsigned long long line, unsigned long long col)
{
    const unsigned int tab = view_tab(view);
    unsigned long long cell = 0ull;
    for (unsigned long long at = 0ull; at < col; at += 1ull)
    {
        cell += (view_unit(view, line, at) == '\t') ? (tab - (cell % tab)) : 1ull;
    }
    return cell;
}

static unsigned long long view_col_at(CliView *view, unsigned long long line, unsigned long long cell)
{
    const unsigned int tab = view_tab(view);
    const unsigned long long length = view_length(view, line);
    unsigned long long at = 0ull;
    unsigned long long here = 0ull;
    while (at < length)
    {
        const unsigned long long next = here + ((view_unit(view, line, at) == '\t') ? (tab - (here % tab)) : 1ull);
        if (next > cell)
        {
            // the cell inside a letter's span goes to its nearer side
            return ((cell - here) * 2ull >= (next - here)) ? at + 1ull : at;
        }
        here = next;
        at += 1ull;
    }
    return length;
}

// whether a unit is a letter of a word
static int view_word_unit(unsigned int unit)
{
    return (unit == '_') || cli_text_alphanumeric(unit);
}

static int view_blank_unit(unsigned int unit)
{
    return (unit == ' ') || (unit == '\t');
}

// the place a letter left of `place`, the end of the line shown before it at a line's start; and a letter right, the
// start of the line shown after it at a line's end
static CliPlace view_left(CliView *view, CliPlace place)
{
    if (place.col == 0ull)
    {
        const unsigned long long above = view_up(view, place.line, 1ull);
        return (above == place.line) ? place : (CliPlace){above, view_length(view, above)};
    }
    const unsigned int before = view_unit(view, place.line, place.col - 1ull);
    const int pair = (place.col >= 2ull) && (before >= 0xdc00u) && (before <= 0xdfffu);
    return (CliPlace){place.line, place.col - (pair ? 2ull : 1ull)};
}

static CliPlace view_right(CliView *view, CliPlace place)
{
    const unsigned long long length = view_length(view, place.line);
    if (place.col >= length)
    {
        const unsigned long long below = view_down(view, place.line, 1ull);
        return (below == place.line) ? place : (CliPlace){below, 0ull};
    }
    const unsigned int here = view_unit(view, place.line, place.col);
    const int pair = (place.col + 1ull < length) && (here >= 0xd800u) && (here <= 0xdbffu);
    return (CliPlace){place.line, place.col + (pair ? 2ull : 1ull)};
}

// the place the word left of `place` starts at, and the place past the word right of it: blanks skipped, then a run of
// a word's letters or of other letters
static CliPlace view_word_left(CliView *view, CliPlace place)
{
    if (place.col == 0ull)
    {
        return view_left(view, place);
    }
    unsigned long long col = place.col;
    while ((col > 0ull) && view_blank_unit(view_unit(view, place.line, col - 1ull)))
    {
        col -= 1ull;
    }
    const int word = (col > 0ull) && view_word_unit(view_unit(view, place.line, col - 1ull));
    while ((col > 0ull) && !view_blank_unit(view_unit(view, place.line, col - 1ull)) &&
           (view_word_unit(view_unit(view, place.line, col - 1ull)) == word))
    {
        col -= 1ull;
    }
    return (CliPlace){place.line, col};
}

static CliPlace view_word_right(CliView *view, CliPlace place)
{
    const unsigned long long length = view_length(view, place.line);
    if (place.col >= length)
    {
        return view_right(view, place);
    }
    unsigned long long col = place.col;
    while ((col < length) && view_blank_unit(view_unit(view, place.line, col)))
    {
        col += 1ull;
    }
    const int word = (col < length) && view_word_unit(view_unit(view, place.line, col));
    while ((col < length) && !view_blank_unit(view_unit(view, place.line, col)) &&
           (view_word_unit(view_unit(view, place.line, col)) == word))
    {
        col += 1ull;
    }
    return (CliPlace){place.line, col};
}

// the first letter of `line` that is not blank, its end where every letter is
static unsigned long long view_first_letter(CliView *view, unsigned long long line)
{
    const unsigned long long length = view_length(view, line);
    unsigned long long col = 0ull;
    while ((col < length) && view_blank_unit(view_unit(view, line, col)))
    {
        col += 1ull;
    }
    return col;
}

// whether one place stands before another
static int view_before(CliPlace one, CliPlace other)
{
    return (one.line < other.line) || ((one.line == other.line) && (one.col < other.col));
}

// the selection's ends in order, `from` first
static void view_selection(const CliView *view, CliPlace *from, CliPlace *to)
{
    const int anchored = view_before(view->anchor, view->cursor);
    *from = anchored ? view->anchor : view->cursor;
    *to = anchored ? view->cursor : view->anchor;
}

static int view_selected(const CliView *view)
{
    return (view->cursor.line != view->anchor.line) || (view->cursor.col != view->anchor.col);
}

// the top moved so the cursor stands in view, among the lines shown
static void view_followed(CliView *view)
{
    const unsigned int rows = (view->rows != 0u) ? view->rows : 1u;
    view->top = view_shown_up(view, view->top);
    if (view->cursor.line < view->top)
    {
        view->top = view_shown_up(view, view->cursor.line);
    }
    else if (view_rows_to(view, view->top, view->cursor.line, rows) >= rows)
    {
        view->top = view_up(view, view->cursor.line, rows - 1ull);
    }
}

// the cursor moved to `place`, the anchor with it unless the move extends the selection; the column kept where
// `kept` is 1, for a move up or down
static void view_moved(CliView *view, CliPlace place, int extend, int kept)
{
    view->cursor = place;
    if (!extend)
    {
        view->anchor = place;
    }
    if (!kept)
    {
        view->goal_kept = 0;
    }
    view_followed(view);
}

// the cursor moved `by` lines shown up or down, in the column it keeps
static void view_vertical(CliView *view, long long by, int extend)
{
    if (!view->goal_kept)
    {
        view->goal = view_cell_of(view, view->cursor.line, view->cursor.col);
        view->goal_kept = 1;
    }
    // a move's count is not negative either way
    const unsigned long long line = (by < 0ll) ? view_up(view, view->cursor.line, (unsigned long long)(-by))
                                               : view_down(view, view->cursor.line, (unsigned long long)by);
    view_moved(view, (CliPlace){line, view_col_at(view, line, view->goal)}, extend, 1);
}

// the query's cell grown to hold `more` units past its length: 1, or 0 where the cell could not be taken
static int view_query_room(CliView *view, unsigned long long more)
{
    const unsigned long long wanted = view->find_length + more;
    if (wanted <= view->find_room)
    {
        return 1;
    }
    const unsigned long long room = (wanted > 32ull) ? wanted * 2ull : 64ull;
    unsigned short *const grown = g_cli_cells->persistent_buf_alloc(room * sizeof(*grown), &s_owner);
    if (grown == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = grown, .src = view->find_query, .bytes = view->find_length * sizeof(*grown)});
    g_cli_cells->persistent_buf_release(view->find_query, &s_owner);
    view->find_query = grown;
    view->find_room = room;
    return 1;
}

// `count` units put into the query at its cursor, which moves past them; and the units from `from` to before `to`
// taken out of it, the cursor at `from`; each makes the matches stale
static void view_query_put(CliView *view, const unsigned short *units, unsigned long long count)
{
    if ((count == 0ull) || !view_query_room(view, count))
    {
        return;
    }
    for (unsigned long long at = view->find_length; at > view->find_at; at -= 1ull)
    {
        view->find_query[at + count - 1ull] = view->find_query[at - 1ull];
    }
    memor.cpy(&(MemoriaCfg){.dst = view->find_query + view->find_at, .src = units, .bytes = count * sizeof(*units)});
    view->find_length += count;
    view->find_at += count;
    view->find_stale = 1;
}

static void view_query_cut(CliView *view, unsigned long long from, unsigned long long to)
{
    for (unsigned long long at = to; at < view->find_length; at += 1ull)
    {
        view->find_query[from + at - to] = view->find_query[at];
    }
    view->find_length -= to - from;
    view->find_at = from;
    view->find_stale = 1;
}

// a match found kept among the matches, their cell grown as they come
static void view_match_kept(void *context, unsigned long long line, unsigned long long from, unsigned long long to)
{
    CliView *const view = context;
    if (view->matches == view->match_room)
    {
        const unsigned long long room = (view->match_room != 0ull) ? view->match_room * 2ull : 256ull;
        unsigned long long(*const grown)[3] = g_cli_cells->persistent_buf_alloc(room * sizeof(*grown), &s_owner);
        if (grown == NULL)
        {
            return;
        }
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = view->match, .bytes = view->matches * sizeof(*grown)});
        g_cli_cells->persistent_buf_release(view->match, &s_owner);
        view->match = grown;
        view->match_room = room;
    }
    view->match[view->matches][0] = line;
    view->match[view->matches][1] = from;
    view->match[view->matches][2] = to;
    view->matches += 1ull;
}

// the matches of a paged text read off `size` bytes of the `[[line, from, to], ...]` the pages' find writes
static void view_matches_read(CliView *view, const char *text, unsigned long long size)
{
    unsigned long long number[3] = {0ull, 0ull, 0ull};
    unsigned int part = 0u;
    unsigned long long value = 0ull;
    int in_number = 0;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        const char letter = text[at];
        if ((letter >= '0') && (letter <= '9'))
        {
            // a digit's value is its distance from '0'
            value = (value * 10ull) + (unsigned long long)(letter - '0');
            in_number = 1;
        }
        else if (in_number)
        {
            number[part] = value;
            part += 1u;
            value = 0ull;
            in_number = 0;
            if (part == 3u)
            {
                view_match_kept(view, number[0], number[1], number[2]);
                part = 0u;
            }
        }
    }
}

// the current match settled: the first that starts at or after the selection's start, the first of all where none
// does, none where none was found
static void view_current_settled(CliView *view)
{
    CliPlace from;
    CliPlace to;
    view_selection(view, &from, &to);
    view->current = (view->matches != 0ull) ? 0ll : -1ll;
    for (unsigned long long at = 0ull; at < view->matches; at += 1ull)
    {
        const CliPlace start = {view->match[at][0], view->match[at][1]};
        if (!view_before(start, from))
        {
            // the matches are fewer than CLI_VIEW_MATCHES, which a long long counts
            view->current = (long long)at;
            break;
        }
    }
}

// the text searched for the query where the query or the text changed since it was: the matches found anew, as many
// as CLI_VIEW_MATCHES at the most, a paged text's through the pages, and the current match settled
static void view_searched(CliView *view)
{
    if (!view->find_stale)
    {
        return;
    }
    view->find_stale = 0;
    view->matches = 0ull;
    view->current = -1ll;
    if (view->find_length == 0ull)
    {
        return;
    }
    const unsigned long long query_room = (view->find_length * 4ull) + 1ull;
    char *const query = g_cli_cells->persistent_buf_alloc(query_room, &s_owner);
    if (query == NULL)
    {
        return;
    }
    VerbaLinea line = {query, query_room, 0ull};
    cli_document_units_utf8(&line, view->find_query, view->find_length);
    (void)mmgr_line_finish(&line);
    const CliDocSought sought = {query, view->find_regex, view->find_case, view->find_word};
    char why_text[256];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    if (view->paged)
    {
        const unsigned long long room = CLI_VIEW_MATCHES * 24ull;
        char *const cell = g_cli_cells->persistent_buf_alloc(room, &s_owner);
        if (cell != NULL)
        {
            VerbaLinea out = {cell, room, 0ull};
            if (cli_pages_find(view->key, &sought, CLI_VIEW_MATCHES, &out, &why) && mmgr_line_ok(&out))
            {
                view_matches_read(view, cell, out.at);
            }
            g_cli_cells->persistent_buf_release(cell, &s_owner);
        }
    }
    else
    {
        cli_document_lock_take();
        const CliStoredText *const one = cli_document_stored(view->key);
        if (one != NULL)
        {
            (void)cli_document_find_each(&one->lines, &sought, CLI_VIEW_MATCHES, view_match_kept, view, &why);
        }
        cli_document_lock_give();
    }
    g_cli_cells->persistent_buf_release(query, &s_owner);
    view_current_settled(view);
}

// the match a step from the selection selected, the current one: the first that starts after the selection's start
// going forward, the last that starts before it going back, the first or the last around the ends
static void view_find_stepped(CliView *view, int forward)
{
    view_searched(view);
    if (view->matches == 0ull)
    {
        return;
    }
    CliPlace from;
    CliPlace to;
    view_selection(view, &from, &to);
    unsigned long long chosen = forward ? 0ull : view->matches - 1ull;
    for (unsigned long long at = 0ull; forward && (at < view->matches); at += 1ull)
    {
        const CliPlace start = {view->match[at][0], view->match[at][1]};
        if (view_before(from, start))
        {
            chosen = at;
            break;
        }
    }
    for (unsigned long long at = view->matches; !forward && (at > 0ull); at -= 1ull)
    {
        const CliPlace start = {view->match[at - 1ull][0], view->match[at - 1ull][1]};
        if (view_before(start, from))
        {
            chosen = at - 1ull;
            break;
        }
    }
    // the matches are fewer than CLI_VIEW_MATCHES, which a long long counts
    view->current = (long long)chosen;
    view->anchor = (CliPlace){view->match[chosen][0], view->match[chosen][1]};
    view_moved(view, (CliPlace){view->match[chosen][0], view->match[chosen][2]}, 1, 0);
}

// find opened, the keys its own, a selection on one line its query; and closed
static void view_find_opened(CliView *view)
{
    view->find_open = 1;
    view->find_focus = 1;
    CliPlace from;
    CliPlace to;
    view_selection(view, &from, &to);
    if ((from.line == to.line) && (from.col < to.col))
    {
        view->find_length = 0ull;
        view->find_at = 0ull;
        const unsigned long long length = view_length(view, from.line);
        const unsigned long long end = (to.col < length) ? to.col : length;
        view_query_put(view, view->line_units + from.col, end - from.col);
    }
    view->find_at = view->find_length;
    view->find_stale = 1;
}

static void view_find_closed(CliView *view)
{
    view->find_open = 0;
    view->find_focus = 0;
}

// a key taken by find where its bar is open or the key opens it: Mod+F opens it or gives it the keys, F3 seeks the
// query again, Escape closes it, and with the keys its own Enter seeks the next match, Shift+Enter the one before,
// Left, Right, Home, End, Backspace and Delete edit the query, and Alt+C, Alt+W and Alt+R turn its case, whole words
// and pattern on and off: 1 where the key was find's
static int view_find_keyed(CliView *view, const char *name, int mod, int shift, int alt)
{
    if (mod && !alt && view_named(name, "F"))
    {
        if (view->find_open)
        {
            view->find_focus = 1;
            view->find_at = view->find_length;
        }
        else
        {
            view_find_opened(view);
        }
        return 1;
    }
    if (!mod && !alt && view_named(name, "F3"))
    {
        if (view->find_length != 0ull)
        {
            view_find_stepped(view, !shift);
        }
        else
        {
            view_find_opened(view);
        }
        return 1;
    }
    if (!view->find_open)
    {
        return 0;
    }
    if (!mod && !alt && view_named(name, "Escape"))
    {
        view_find_closed(view);
        return 1;
    }
    if (alt && !mod && (view_named(name, "C") || view_named(name, "W") || view_named(name, "R")))
    {
        view->find_case = view_named(name, "C") ? !view->find_case : view->find_case;
        view->find_word = view_named(name, "W") ? !view->find_word : view->find_word;
        view->find_regex = view_named(name, "R") ? !view->find_regex : view->find_regex;
        view->find_stale = 1;
        return 1;
    }
    if (!view->find_focus || mod || alt)
    {
        return 0;
    }
    if (view_named(name, "Enter"))
    {
        view_find_stepped(view, !shift);
    }
    else if (view_named(name, "Left"))
    {
        view->find_at -= (view->find_at > 0ull) ? 1ull : 0ull;
    }
    else if (view_named(name, "Right"))
    {
        view->find_at += (view->find_at < view->find_length) ? 1ull : 0ull;
    }
    else if (view_named(name, "Home"))
    {
        view->find_at = 0ull;
    }
    else if (view_named(name, "End"))
    {
        view->find_at = view->find_length;
    }
    else if (view_named(name, "Backspace"))
    {
        if (view->find_at > 0ull)
        {
            view_query_cut(view, view->find_at - 1ull, view->find_at);
        }
    }
    else if (view_named(name, "Delete"))
    {
        if (view->find_at < view->find_length)
        {
            view_query_cut(view, view->find_at, view->find_at + 1ull);
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
static int view_folded(CliView *view, unsigned long long line, int exact)
{
    CliViewFoldSought sought = {line, 0ull, 0ull, 0};
    (void)cli_document_folds_each(view->key, exact ? view_fold_matched : view_fold_innermost, &sought);
    if (!sought.found || !view_fold_put(view, sought.first, sought.last))
    {
        return 0;
    }
    if ((view->cursor.line == sought.first) || (view_hider(view, view->cursor.line) != NULL))
    {
        view_moved(view, (CliPlace){sought.first, view_length(view, sought.first)}, 0, 0);
    }
    else
    {
        view_followed(view);
    }
    return 1;
}

static int view_unfolded(CliView *view, unsigned long long line)
{
    for (unsigned int at = 0u; at < view->folds; at += 1u)
    {
        if (view->fold[at].first == line)
        {
            for (unsigned int after = at + 1u; after < view->folds; after += 1u)
            {
                view->fold[after - 1u] = view->fold[after];
            }
            view->folds -= 1u;
            view_followed(view);
            return 1;
        }
    }
    return 0;
}

// one change of one edit written to the text, a paged text's through the pages, the closed folds moved with it: 1, or
// 0 with why given
static int view_written(CliView *view, const CliEdit *const *edits, const unsigned int *counts, VerbaLinea *err_msg)
{
    view->line_known = 0;
    int written = 0;
    if (view->paged)
    {
        char shape_text[128];
        VerbaLinea shape = {shape_text, sizeof(shape_text), 0ull};
        written = cli_document_paged_edit(view->key, edits, counts, 1u, &shape, err_msg);
    }
    else
    {
        CliShape shape;
        written = cli_document_edit(view->key, edits, counts, 1u, &shape, err_msg);
    }
    for (unsigned int at = 0u; written && (at < counts[0]); at += 1u)
    {
        view_folds_edited(view, &edits[0][at]);
    }
    view->find_stale = view->find_stale || written;
    return written;
}

// `length` bytes of `text` written over the span from `from` to `to`, recorded in a step of `kind`, joined to the last
// step where it is of the same kind and came within CLI_VIEW_JOINED milliseconds; the cursor and the anchor put after
// the text written: 1, or 0 with why given
static int view_edited(CliView *view, CliPlace from, CliPlace to, const char *text, unsigned long long length,
                       unsigned int kind, VerbaLinea *err_msg)
{
    // the text taken out, read into a cell of the edit's own with the text put in: each line of the span through the
    // view's line cell, the span's units of it written as UTF-8
    unsigned long long took_room = 0ull;
    for (unsigned long long line = from.line; line <= to.line; line += 1ull)
    {
        took_room += (view_length(view, line) * 4ull) + 1ull;
    }
    char *const cell = g_cli_cells->persistent_buf_alloc(length + 1ull + took_room + 1ull, &s_owner);
    if (cell == NULL)
    {
        mmgr_line_put(err_msg, "the edit's cell could not be taken");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = cell, .src = text, .bytes = length});
    cell[length] = '\0';
    VerbaLinea took = {cell + length + 1ull, took_room + 1ull, 0ull};
    for (unsigned long long line = from.line; line <= to.line; line += 1ull)
    {
        const unsigned long long line_length = view_length(view, line);
        const unsigned long long start = (line == from.line) ? from.col : 0ull;
        const unsigned long long end = (line == to.line) ? to.col : line_length;
        const unsigned long long clipped_end = (end < line_length) ? end : line_length;
        if (start < clipped_end)
        {
            cli_document_units_utf8(&took, view->line_units + start, clipped_end - start);
        }
        if (line < to.line)
        {
            mmgr_line_ch(&took, '\n');
        }
    }
    const unsigned long long took_length = mmgr_line_finish(&took);
    // the edit made, a paged text's through the pages, and the line cell read anew after it
    const CliEdit edit = {from, to, text, length};
    const CliEdit *const edits[1] = {&edit};
    const unsigned int counts[1] = {1u};
    view->line_known = 0;
    if (!view_written(view, edits, counts, err_msg))
    {
        g_cli_cells->persistent_buf_release(cell, &s_owner);
        return 0;
    }
    // where the text put in ends
    CliPlace end = from;
    unsigned long long last_line_start = 0ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        if (text[at] == '\n')
        {
            end.line += 1ull;
            last_line_start = at + 1ull;
        }
    }
    const unsigned long long last_units = cli_text_units(text + last_line_start, length - last_line_start);
    end.col = (end.line == from.line) ? from.col + last_units : last_units;
    // the step the edit joins or begins
    const unsigned long long now = cli_clock_milliseconds();
    view_steps_cut(view, view->done);
    CliViewStep *step = (view->steps != 0u) ? &view->step[view->steps - 1u] : NULL;
    const int joined = (step != NULL) && (kind != CLI_VIEW_KIND_NONE) && (step->kind == kind) &&
                       (now - step->at_ms <= CLI_VIEW_JOINED) && (step->edits < CLI_VIEW_STEP_EDITS);
    if (!joined)
    {
        if (view->steps == view->steps_room)
        {
            const unsigned int room = (view->steps_room != 0u) ? view->steps_room * 2u : 16u;
            CliViewStep *const grown = g_cli_cells->persistent_buf_alloc(room * sizeof(*grown), &s_owner);
            if (grown == NULL)
            {
                g_cli_cells->persistent_buf_release(cell, &s_owner);
                mmgr_line_put(err_msg, "the steps' cell could not be taken");
                return 0;
            }
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = view->step, .bytes = view->steps * sizeof(*grown)});
            g_cli_cells->persistent_buf_release(view->step, &s_owner);
            view->step = grown;
            view->steps_room = room;
        }
        step = &view->step[view->steps];
        memor.set(&(MemoriaCfg){.dst = step, .val = 0, .bytes = sizeof(*step)});
        step->kind = kind;
        step->cursor_before = view->cursor;
        step->anchor_before = view->anchor;
        view->steps += 1u;
    }
    if (step->edits == step->room)
    {
        const unsigned int room = (step->room != 0u) ? step->room * 2u : 4u;
        CliViewEdit *const grown = g_cli_cells->persistent_buf_alloc(room * sizeof(*grown), &s_owner);
        if (grown == NULL)
        {
            g_cli_cells->persistent_buf_release(cell, &s_owner);
            mmgr_line_put(err_msg, "a step's cell could not be taken");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = step->edit, .bytes = step->edits * sizeof(*grown)});
        g_cli_cells->persistent_buf_release(step->edit, &s_owner);
        step->edit = grown;
        step->room = room;
    }
    step->edit[step->edits] = (CliViewEdit){from, to, end, cell, length, cell + length + 1ull, took_length};
    step->edits += 1u;
    step->at_ms = now;
    view->done = view->steps;
    view_moved(view, end, 0, 0);
    step->cursor_after = view->cursor;
    step->anchor_after = view->anchor;
    return 1;
}

// the last step done undone, its edits written back from the last to the first; and the first step undone done again
static int view_undone(CliView *view, VerbaLinea *err_msg)
{
    if (view->done == 0u)
    {
        return 1;
    }
    CliViewStep *const step = &view->step[view->done - 1u];
    for (unsigned int at = step->edits; at > 0u; at -= 1u)
    {
        const CliViewEdit *const edit = &step->edit[at - 1u];
        const CliEdit back = {edit->from, edit->end, edit->took, edit->took_length};
        const CliEdit *const edits[1] = {&back};
        const unsigned int counts[1] = {1u};
        if (!view_written(view, edits, counts, err_msg))
        {
            return 0;
        }
    }
    view->done -= 1u;
    view->cursor = step->cursor_before;
    view->anchor = step->anchor_before;
    view->goal_kept = 0;
    view_followed(view);
    return 1;
}

static int view_redone(CliView *view, VerbaLinea *err_msg)
{
    if (view->done >= view->steps)
    {
        return 1;
    }
    CliViewStep *const step = &view->step[view->done];
    for (unsigned int at = 0u; at < step->edits; at += 1u)
    {
        const CliViewEdit *const edit = &step->edit[at];
        const CliEdit again = {edit->from, edit->to, edit->put, edit->put_length};
        const CliEdit *const edits[1] = {&again};
        const unsigned int counts[1] = {1u};
        if (!view_written(view, edits, counts, err_msg))
        {
            return 0;
        }
    }
    view->done += 1u;
    view->cursor = step->cursor_after;
    view->anchor = step->anchor_after;
    view->goal_kept = 0;
    view_followed(view);
    return 1;
}

// the selection written over with `length` bytes of `text`, or the text put at the cursor where none is selected
static int view_typed(CliView *view, const char *text, unsigned long long length, unsigned int kind,
                      VerbaLinea *err_msg)
{
    CliPlace from;
    CliPlace to;
    view_selection(view, &from, &to);
    return view_edited(view, from, to, text, length, kind, err_msg);
}

// the cursor and the anchor put where an edit leaves them, the last step done recording them so redo puts them back
static void view_settled(CliView *view, CliPlace cursor, CliPlace anchor)
{
    view->cursor = cursor;
    view->anchor = anchor;
    view->goal_kept = 0;
    if (view->done != 0u)
    {
        view->step[view->done - 1u].cursor_after = cursor;
        view->step[view->done - 1u].anchor_after = anchor;
    }
    view_followed(view);
}

// the closer of the pair `unit` opens, a quote's being itself, or 0 where it opens none; whether `unit` closes a pair
// or is a quote; and whether it closes a bracket pair, one whose closer is not its opener
static unsigned int view_closer_of(const CliView *view, unsigned int unit)
{
    for (unsigned int at = 0u; at < view->pairs; at += 1u)
    {
        if (view->pair_open[at] == unit)
        {
            return view->pair_close[at];
        }
    }
    for (unsigned int at = 0u; at < view->quotes; at += 1u)
    {
        if (view->quote[at] == unit)
        {
            return unit;
        }
    }
    return 0u;
}

static int view_closes(const CliView *view, unsigned int unit)
{
    for (unsigned int at = 0u; at < view->pairs; at += 1u)
    {
        if (view->pair_close[at] == unit)
        {
            return 1;
        }
    }
    for (unsigned int at = 0u; at < view->quotes; at += 1u)
    {
        if (view->quote[at] == unit)
        {
            return 1;
        }
    }
    return 0;
}

static int view_closes_bracket(const CliView *view, unsigned int unit)
{
    for (unsigned int at = 0u; at < view->pairs; at += 1u)
    {
        if ((view->pair_close[at] == unit) && (view->pair_open[at] != unit))
        {
            return 1;
        }
    }
    return 0;
}

// whether `open` and `close` are a bracket pair of the language's, or a quote and itself where `quoted` is 1
static int view_paired(const CliView *view, unsigned int open, unsigned int close, int quoted)
{
    for (unsigned int at = 0u; at < view->pairs; at += 1u)
    {
        if ((view->pair_open[at] == open) && (view->pair_close[at] == close) && (open != close))
        {
            return 1;
        }
    }
    for (unsigned int at = 0u; quoted && (at < view->quotes); at += 1u)
    {
        if ((view->quote[at] == open) && (open == close))
        {
            return 1;
        }
    }
    return 0;
}

// the closer of the innermost bracket `line` opens before `col` and leaves open there, or 0 where it leaves none
static unsigned int view_open_before(CliView *view, unsigned long long line, unsigned long long col)
{
    unsigned int open[64];
    unsigned int depth = 0u;
    for (unsigned long long at = 0ull; at < col; at += 1ull)
    {
        const unsigned int unit = view_unit(view, line, at);
        const unsigned int closer = view_closer_of(view, unit);
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
static int view_opens_block(CliView *view, unsigned long long line, unsigned long long col)
{
    if (view->indent_after[0] == '\0')
    {
        return 0;
    }
    const unsigned long long length = view_length(view, line);
    const unsigned long long count = (col < length) ? col : length;
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(sizeof(CliRegexpRoom) + ((count + 1ull) * sizeof(unsigned int)), &s_owner);
    if (cell == NULL)
    {
        return 0;
    }
    unsigned int *const letters = (unsigned int *)(void *)(cell + sizeof(CliRegexpRoom));
    // the units read as letters, a surrogate pair one letter
    unsigned long long read = 0ull;
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        const unsigned int unit = view->line_units[at];
        const int pair = (unit >= 0xd800u) && (unit <= 0xdbffu) && (at + 1ull < count);
        letters[read] = pair ? (0x10000u + ((unit - 0xd800u) << 10u) + (view->line_units[at + 1ull] - 0xdc00u)) : unit;
        at += pair ? 1ull : 0ull;
        read += 1ull;
    }
    char why_text[256];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    CliRegexp regexp;
    cli_regexp_roomed(&regexp, (CliRegexpRoom *)(void *)cell);
    int ok = cli_regexp_read(&regexp, view->indent_after, "u", &why);
    if (!ok)
    {
        why.at = 0ull;
        cli_regexp_roomed(&regexp, (CliRegexpRoom *)(void *)cell);
        ok = cli_regexp_read(&regexp, view->indent_after, "", &why);
    }
    int matched = 0;
    for (unsigned long long at = 0ull; ok && !matched && (at <= read); at += 1ull)
    {
        CliRegexpSpan groups[CLI_REGEXP_GROUPS + 1u];
        const int starts = (!regexp.anchored || (at == 0ull)) &&
                           ((at == read) || regexp.first.empty || cli_regexp_allows(&regexp.first, letters[at]));
        matched = starts && cli_regexp_match_into(&regexp, letters, read, at, groups);
    }
    g_cli_cells->persistent_buf_release(cell, &s_owner);
    return matched;
}

// the indentation's step as text: a tab, or the step's spaces
static unsigned long long view_step_written(const CliView *view, VerbaLinea *out)
{
    const unsigned long long begun = out->at;
    if (view->indent.tabs == 1)
    {
        mmgr_line_ch(out, '\t');
    }
    else
    {
        for (unsigned long long at = 0ull; at < view_tab(view); at += 1ull)
        {
            mmgr_line_ch(out, ' ');
        }
    }
    return out->at - begun;
}

// a line broken at the cursor, the new line indented as the one it leaves, and a step deeper after a bracket the line
// leaves open before the cursor or a line the language's pattern gives as opening a block; where what follows the
// cursor closes that bracket, or the letters about the cursor are a pair, the closer goes to a line of its own at the
// first line's depth
static int view_newline(CliView *view, VerbaLinea *err_msg)
{
    CliPlace from;
    CliPlace to;
    view_selection(view, &from, &to);
    const unsigned long long lead = view_first_letter(view, from.line);
    const unsigned long long kept = (lead < from.col) ? lead : from.col;
    const unsigned int open = view_open_before(view, from.line, from.col);
    const int deeper = (open != 0u) || view_opens_block(view, from.line, from.col);
    unsigned long long before = from.col;
    while ((before > 0ull) && view_blank_unit(view_unit(view, from.line, before - 1ull)))
    {
        before -= 1ull;
    }
    const unsigned int prev = (before > 0ull) ? view_unit(view, from.line, before - 1ull) : 0u;
    const unsigned long long rest_length = view_length(view, to.line);
    unsigned long long gap = 0ull;
    while ((to.col + gap < rest_length) && view_blank_unit(view_unit(view, to.line, to.col + gap)))
    {
        gap += 1ull;
    }
    const unsigned int next = (to.col + gap < rest_length) ? view_unit(view, to.line, to.col + gap) : 0u;
    const int paired = ((prev != 0u) && (next != 0u) && view_paired(view, prev, next, 0)) || ((open != 0u) && (next == open));
    char text[CLI_PATH_MAX_LEN];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_ch(&line, '\n');
    for (unsigned long long at = 0ull; (at < kept) && (line.at + 4ull < sizeof(text)); at += 1ull)
    {
        cli_utf8_put(&line, view_unit(view, from.line, at));
    }
    const unsigned long long lead_bytes = line.at - 1ull;
    unsigned long long step_bytes = 0ull;
    if (deeper)
    {
        step_bytes = view_step_written(view, &line);
    }
    if (paired && deeper)
    {
        const unsigned long long cursor_col = cli_text_units(text + 1, lead_bytes + step_bytes);
        mmgr_line_ch(&line, '\n');
        mmgr_line_put_n(&line, text + 1, lead_bytes);
        if (!view_edited(view, from, (CliPlace){to.line, to.col + gap}, text, line.at, CLI_VIEW_KIND_NONE, err_msg))
        {
            return 0;
        }
        const CliPlace between = {from.line + 1ull, cursor_col};
        view_settled(view, between, between);
        return 1;
    }
    return view_edited(view, from, to, text, line.at, CLI_VIEW_KIND_NONE, err_msg);
}

// the letter before the cursor taken out, or the selection; a pair's opener before the cursor and its closer after
// it both; in a line's indentation of spaces a step of them; at a line's start the line joined to the one before
static int view_backspace(CliView *view, VerbaLinea *err_msg)
{
    if (view_selected(view))
    {
        return view_typed(view, "", 0ull, CLI_VIEW_KIND_DELETE, err_msg);
    }
    const CliPlace at = view->cursor;
    if (at.col == 0ull)
    {
        return (at.line == 0ull) ? 1 : view_edited(view, view_left(view, at), at, "", 0ull, CLI_VIEW_KIND_DELETE, err_msg);
    }
    if ((at.col < view_length(view, at.line)) &&
        view_paired(view, view_unit(view, at.line, at.col - 1ull), view_unit(view, at.line, at.col), 1))
    {
        return view_edited(view, (CliPlace){at.line, at.col - 1ull}, (CliPlace){at.line, at.col + 1ull}, "", 0ull,
                           CLI_VIEW_KIND_DELETE, err_msg);
    }
    if (!view->indent.tabs || (view->indent.tabs == -1))
    {
        unsigned long long col = 0ull;
        while ((col < at.col) && (view_unit(view, at.line, col) == ' '))
        {
            col += 1ull;
        }
        if (col == at.col)
        {
            const unsigned long long step = view_tab(view);
            const unsigned long long back = (at.col % step != 0ull) ? at.col % step : step;
            return view_edited(view, (CliPlace){at.line, at.col - back}, at, "", 0ull, CLI_VIEW_KIND_DELETE, err_msg);
        }
    }
    return view_edited(view, view_left(view, at), at, "", 0ull, CLI_VIEW_KIND_DELETE, err_msg);
}

// the letter at the cursor taken out, or the selection; at a line's end the next line joined to it
static int view_deleted(CliView *view, VerbaLinea *err_msg)
{
    if (view_selected(view))
    {
        return view_typed(view, "", 0ull, CLI_VIEW_KIND_DELETE_FORWARD, err_msg);
    }
    const CliPlace at = view->cursor;
    const CliPlace next = view_right(view, at);
    if ((next.line == at.line) && (next.col == at.col))
    {
        return 1;
    }
    return view_edited(view, at, next, "", 0ull, CLI_VIEW_KIND_DELETE_FORWARD, err_msg);
}

// a tab typed: an indentation's step at the cursor, spaces to the next stop or a tab
static int view_tabbed(CliView *view, VerbaLinea *err_msg)
{
    if (view->indent.tabs == 1)
    {
        return view_typed(view, "\t", 1ull, CLI_VIEW_KIND_SPACE, err_msg);
    }
    const unsigned long long step = view_tab(view);
    CliPlace from;
    CliPlace to;
    view_selection(view, &from, &to);
    const unsigned long long cell = view_cell_of(view, from.line, from.col);
    const unsigned long long spaces = step - (cell % step);
    char text[64];
    const unsigned long long count = (spaces < sizeof(text)) ? spaces : sizeof(text) - 1ull;
    memor.set(&(MemoriaCfg){.dst = text, .val = ' ', .bytes = count});
    return view_typed(view, text, count, CLI_VIEW_KIND_SPACE, err_msg);
}

// the span from `from` to `to` of the text written into `out` as UTF-8, each line but the last followed by "\n"
static void view_span_written(CliView *view, CliPlace from, CliPlace to, VerbaLinea *out)
{
    for (unsigned long long line = from.line; line <= to.line; line += 1ull)
    {
        const unsigned long long line_length = view_length(view, line);
        const unsigned long long start = (line == from.line) ? from.col : 0ull;
        const unsigned long long end = (line == to.line) ? ((to.col < line_length) ? to.col : line_length) : line_length;
        if (start < end)
        {
            cli_document_units_utf8(out, view->line_units + start, end - start);
        }
        if (line < to.line)
        {
            mmgr_line_ch(out, '\n');
        }
    }
}

// one letter typed, `length` bytes of `text` as the unit `letter`: a closer typed before itself steps over it; an
// opener typed around a selection wraps it, the selection kept inside, and typed alone brings its closer where what
// follows allows, the cursor between them, a quote where what comes before allows too; a closing bracket typed on a
// lead of blanks takes the lead back a step; any other letter is written as typed
static int view_letter_typed(CliView *view, const char *text, unsigned long long length, unsigned int letter,
                             unsigned int kind, VerbaLinea *err_msg)
{
    CliPlace from;
    CliPlace to;
    view_selection(view, &from, &to);
    const int empty = !view_selected(view);
    const unsigned long long to_length = view_length(view, to.line);
    const unsigned int next = (to.col < to_length) ? view_unit(view, to.line, to.col) : 0u;
    const unsigned int prev = (from.col > 0ull) ? view_unit(view, from.line, from.col - 1ull) : 0u;
    if (empty && view_closes(view, letter) && (next == letter))
    {
        view_moved(view, (CliPlace){to.line, to.col + 1ull}, 0, 0);
        return 1;
    }
    const unsigned int close = view_closer_of(view, letter);
    if ((close != 0u) && !empty)
    {
        unsigned long long room = 0ull;
        for (unsigned long long line = from.line; line <= to.line; line += 1ull)
        {
            room += (view_length(view, line) * 4ull) + 1ull;
        }
        char *const cell = g_cli_cells->persistent_buf_alloc(room + length + 8ull, &s_owner);
        if (cell == NULL)
        {
            mmgr_line_put(err_msg, "the wrap's cell could not be taken");
            return 0;
        }
        VerbaLinea wrapped = {cell, room + length + 8ull, 0ull};
        mmgr_line_put_n(&wrapped, text, length);
        view_span_written(view, from, to, &wrapped);
        cli_utf8_put(&wrapped, close);
        const int written = view_edited(view, from, to, cell, wrapped.at, CLI_VIEW_KIND_NONE, err_msg);
        g_cli_cells->persistent_buf_release(cell, &s_owner);
        if (!written)
        {
            return 0;
        }
        const CliPlace end = view->cursor;
        view_settled(view, (CliPlace){end.line, end.col - 1ull}, (CliPlace){from.line, from.col + 1ull});
        return 1;
    }
    if (close != 0u)
    {
        const int quote = close == letter;
        const int next_ok = (next == 0u) || view_blank_unit(next) || (next == ';') || (next == ',') || (next == '.') ||
                            (next == ')') || (next == ']') || (next == '}') || (next == '>');
        const int prev_ok = !quote || ((prev != 0u) ? (!view_word_unit(prev) && (prev != '\\') && (prev != letter)) : 1);
        if (next_ok && prev_ok)
        {
            char both[16];
            VerbaLinea line = {both, sizeof(both), 0ull};
            mmgr_line_put_n(&line, text, length);
            cli_utf8_put(&line, close);
            if (!view_edited(view, from, to, both, line.at, kind, err_msg))
            {
                return 0;
            }
            const CliPlace end = view->cursor;
            const CliPlace between = {end.line, end.col - 1ull};
            view_settled(view, between, between);
            return 1;
        }
    }
    if (view_closes_bracket(view, letter) && empty && (from.col > 0ull) && (view_first_letter(view, from.line) >= from.col))
    {
        // the lead less a step: a tab, the step's spaces where the lead ends in them, else the run of blanks it ends in
        unsigned long long less = from.col;
        const unsigned long long step = view_tab(view);
        if (view_unit(view, from.line, less - 1ull) == '\t')
        {
            less -= 1ull;
        }
        else
        {
            unsigned long long spaces = 0ull;
            while ((spaces < less) && (view_unit(view, from.line, less - 1ull - spaces) == ' '))
            {
                spaces += 1ull;
            }
            less -= (spaces >= step) ? step : spaces;
        }
        char lead[CLI_PATH_MAX_LEN];
        VerbaLinea line = {lead, sizeof(lead), 0ull};
        for (unsigned long long at = 0ull; (at < less) && (line.at + 4ull < sizeof(lead)); at += 1ull)
        {
            cli_utf8_put(&line, view_unit(view, from.line, at));
        }
        mmgr_line_put_n(&line, text, length);
        return view_edited(view, (CliPlace){from.line, 0ull}, to, lead, line.at, kind, err_msg);
    }
    return view_edited(view, from, to, text, length, kind, err_msg);
}

// a key's name parsed: the modifier keys down into `mod`, `shift` and `alt`, and the key's own name
static const char *view_key_read(const char *name, int *mod, int *shift, int *alt)
{
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
static int view_keyed(CliView *view, const char *given, VerbaLinea *err_msg)
{
    int mod = 0;
    int shift = 0;
    int alt = 0;
    const char *const name = view_key_read(given, &mod, &shift, &alt);
    const unsigned long long lines = view_lines(view);
    const unsigned long long last = (lines != 0ull) ? lines - 1ull : 0ull;
    const unsigned int rows = (view->rows > 1u) ? view->rows : 1u;
    if (view_find_keyed(view, name, mod, shift, alt))
    {
        return 1;
    }
    if (view_named(name, "Left"))
    {
        CliPlace from;
        CliPlace to;
        view_selection(view, &from, &to);
        const CliPlace place = mod ? view_word_left(view, view->cursor)
                                   : ((!shift && view_selected(view)) ? from : view_left(view, view->cursor));
        view_moved(view, place, shift, 0);
    }
    else if (view_named(name, "Right"))
    {
        CliPlace from;
        CliPlace to;
        view_selection(view, &from, &to);
        const CliPlace place = mod ? view_word_right(view, view->cursor)
                                   : ((!shift && view_selected(view)) ? to : view_right(view, view->cursor));
        view_moved(view, place, shift, 0);
    }
    else if (view_named(name, "Up"))
    {
        view_vertical(view, -1ll, shift);
    }
    else if (view_named(name, "Down"))
    {
        view_vertical(view, 1ll, shift);
    }
    else if (view_named(name, "Home"))
    {
        if (mod)
        {
            view_moved(view, (CliPlace){0ull, 0ull}, shift, 0);
        }
        else
        {
            const unsigned long long first = view_first_letter(view, view->cursor.line);
            view_moved(view, (CliPlace){view->cursor.line, (view->cursor.col == first) ? 0ull : first}, shift, 0);
        }
    }
    else if (view_named(name, "End"))
    {
        const unsigned long long end = view_shown_up(view, last);
        const CliPlace place = mod ? (CliPlace){end, view_length(view, end)}
                                   : (CliPlace){view->cursor.line, view_length(view, view->cursor.line)};
        view_moved(view, place, shift, 0);
    }
    else if (view_named(name, "PageUp") || view_named(name, "PageDown"))
    {
        // a page is the rows less one: the last row seen stays in view
        const unsigned long long by = rows - 1u;
        const int up = view_named(name, "PageUp");
        view_vertical(view, up ? -(long long)by : (long long)by, shift);
        view->top = up ? view_up(view, view->top, by) : view_down(view, view->top, by);
        view_followed(view);
    }
    else if (mod && shift && view_named(name, "["))
    {
        (void)view_folded(view, view->cursor.line, 0);
    }
    else if (mod && shift && view_named(name, "]"))
    {
        (void)view_unfolded(view, view->cursor.line);
    }
    else if (view_named(name, "Backspace"))
    {
        return view_backspace(view, err_msg);
    }
    else if (view_named(name, "Delete"))
    {
        return view_deleted(view, err_msg);
    }
    else if (view_named(name, "Enter"))
    {
        return view_newline(view, err_msg);
    }
    else if (view_named(name, "Tab"))
    {
        return view_tabbed(view, err_msg);
    }
    else if (view_named(name, "Escape"))
    {
        view_moved(view, view->cursor, 0, 0);
    }
    else if (mod && view_named(name, "A"))
    {
        view->anchor = (CliPlace){0ull, 0ull};
        view_moved(view, (CliPlace){last, view_length(view, last)}, 1, 0);
    }
    else if (mod && view_named(name, "Z"))
    {
        return shift ? view_redone(view, err_msg) : view_undone(view, err_msg);
    }
    else if (mod && view_named(name, "Y"))
    {
        return view_redone(view, err_msg);
    }
    return 1;
}

// the pointer taken: a press places the cursor at its cell, a second press takes the word and a third the line, a drag
// extends the selection; a press in the gutter opens the fold closed on its row's line, or closes the one the text
// folds there
static void view_pointed(CliView *view, const CliViewInput *input)
{
    const unsigned long long line = view_down(view, view->top, input->row);
    const CliPlace place = {line, view_col_at(view, line, input->column)};
    if (input->pointer == CLI_VIEW_POINTER_UP)
    {
        return;
    }
    if (input->gutter)
    {
        if ((input->pointer == CLI_VIEW_POINTER_DOWN) && !view_unfolded(view, line))
        {
            (void)view_folded(view, line, 1);
        }
        return;
    }
    if (input->pointer == CLI_VIEW_POINTER_MOVED)
    {
        view_moved(view, place, 1, 0);
        return;
    }
    if (input->clicks >= 3u)
    {
        const unsigned long long below = view_down(view, line, 1ull);
        view->anchor = (CliPlace){line, 0ull};
        view_moved(view, (below > line) ? (CliPlace){below, 0ull} : (CliPlace){line, view_length(view, line)}, 1, 0);
        return;
    }
    if (input->clicks == 2u)
    {
        const CliPlace start = view_word_left(view, view_word_right(view, place));
        view->anchor = (start.line == line) ? start : place;
        view_moved(view, view_word_right(view, view->anchor), 1, 0);
        return;
    }
    view_moved(view, place, input->shift, 0);
}

int cli_view_input(const char *key, const CliViewInput *input, VerbaLinea *err_msg)
{
    cli_lock_take(&s_lock);
    CliView *const view = view_of(key);
    if (view == NULL)
    {
        cli_lock_give(&s_lock);
        mmgr_line_put(err_msg, "no view is open under the key");
        return 0;
    }
    int taken = 1;
    view_folds_refreshed(view);
    if (input->kind == CLI_VIEW_KEY)
    {
        taken = view_keyed(view, input->name, err_msg);
    }
    else if (input->kind == CLI_VIEW_TEXT)
    {
        const int one = (input->length != 0ull) && (cli_text_letters(input->text, input->length) == 1ull);
        const int blank = (input->length != 0ull) && ((input->text[0] == ' ') || (input->text[0] == '\t') ||
                                                      (input->text[0] == '\n'));
        const unsigned int kind = (one && !blank) ? CLI_VIEW_KIND_TYPE : (one ? CLI_VIEW_KIND_SPACE : CLI_VIEW_KIND_NONE);
        unsigned long long took = 0ull;
        const unsigned int letter = one ? cli_text_letter(input->text, input->length, 0ull, &took) : 0u;
        if (view->find_open && view->find_focus)
        {
            // text typed goes into the query, a byte a unit at the most
            unsigned short *const units = g_cli_cells->persistent_buf_alloc((input->length + 1ull) * sizeof(*units), &s_owner);
            if (units != NULL)
            {
                view_query_put(view, units, view_units_of(input->text, input->length, units, input->length + 1ull));
                g_cli_cells->persistent_buf_release(units, &s_owner);
            }
        }
        else
        {
            taken = (one && !blank && (letter < 0x10000u))
                        ? view_letter_typed(view, input->text, input->length, letter, kind, err_msg)
                        : view_typed(view, input->text, input->length, kind, err_msg);
        }
    }
    else if (input->kind == CLI_VIEW_POINTER)
    {
        view_pointed(view, input);
    }
    else if (input->kind == CLI_VIEW_SCROLL)
    {
        const unsigned long long lines = view_lines(view);
        const unsigned long long last = (lines != 0ull) ? lines - 1ull : 0ull;
        const unsigned long long top = (input->top > last) ? last : input->top;
        // a top a closed fold hides goes past the fold the way the view scrolls
        view->top = (top > view->top) ? view_shown_down(view, top) : view_shown_up(view, top);
    }
    else if (input->kind == CLI_VIEW_SIZE)
    {
        // a size the same as the view's changes nothing; one changed moves the top so the cursor stays in view
        const int changed = (view->rows != input->rows) || (view->columns != input->columns);
        view->rows = input->rows;
        view->columns = input->columns;
        if (changed)
        {
            view_followed(view);
        }
    }
    // a paged text's pages are told where the view stands and cache the lines about it
    if (view->paged)
    {
        const unsigned long long rows = (view->rows != 0u) ? view->rows : 1ull;
        const CliPagesView placed_view = {view->top, rows, view->columns, 0ull, 0ull, view->top, view->top + rows, 0ull,
                                          0ull};
        CliPagesPlaced placed;
        (void)cli_pages_viewed(view->key, &placed_view, &placed);
    }
    cli_lock_give(&s_lock);
    return taken;
}

static void view_place_written(VerbaLinea *out, CliPlace place)
{
    mmgr_line_put(out, "{\"line\":");
    mmgr_line_decimal(out, place.line, 1u);
    mmgr_line_put(out, ",\"col\":");
    mmgr_line_decimal(out, place.col, 1u);
    mmgr_line_ch(out, '}');
}

// the bracket pair at the cursor, into `pair`, the bracket that ends at the cursor, else the one that starts at it,
// first and the one it pairs with second: the text's lines from as far above and below the cursor as
// CLI_VIEW_PAIR_LINES reach and none outside those read for it, read as the tree reads a text from the state the first
// is entered in: 1, or 0 where no bracket of a pair stands at the cursor or the lines could not be read
static int view_pair(const CliView *view, CliPlace *pair)
{
    unsigned long long low = 0ull;
    unsigned long long high = 0ull;
    (void)cli_document_bounds(view->key, &low, &high);
    const unsigned long long line = view->cursor.line;
    const unsigned long long reach = (line > CLI_VIEW_PAIR_LINES) ? (line - CLI_VIEW_PAIR_LINES) : 0ull;
    const unsigned long long start = (reach > low) ? reach : low;
    const unsigned long long end = ((high != 0ull) && ((line + CLI_VIEW_PAIR_LINES) >= high)) ? (high - 1ull)
                                                                                               : (line + CLI_VIEW_PAIR_LINES);
    const unsigned long long first = (start < line) ? start : line;
    const unsigned long long last = (end > line) ? end : line;
    char state[CLI_VIEW_STATE];
    unsigned long long size = 0ull;
    char *const text = cli_document_lines_taken(view->key, first, last, &size, state, sizeof(state));
    if (text == NULL)
    {
        return 0;
    }
    CliSymbolPlace found[2] = {{0u, 0u}, {0u, 0u}};
    // a place in a text a view shows stands below 2^32 lines and columns
    const int paired = cli_symbols_pair(view->path, (state[0] != '\0') ? state : "root", text, size,
                                        (CliSymbolPlace){(unsigned int)(line - first), (unsigned int)view->cursor.col}, found);
    g_cli_cells->persistent_buf_release(text, NULL);
    for (unsigned int at = 0u; paired && (at < 2u); at += 1u)
    {
        pair[at].line = first + found[at].line;
        pair[at].col = found[at].col;
    }
    return paired;
}

// The rows a frame shows: their line numbers and, for a paged text's lines read, their texts, in one cell; and how many
typedef struct
{
    unsigned long long *number;
    const char **text;
    unsigned long long count;
} CliViewRows;

// a paged text's rows written into `out` as strings after the opening bracket: read through the pages a run of lines
// in a row at a time, a line that could not be read written empty
static void view_paged_rows_written(const CliView *view, const CliViewRows *shown, VerbaLinea *out)
{
    for (unsigned long long at = 0ull; at < shown->count;)
    {
        unsigned long long end = at;
        while ((end + 1ull < shown->count) && (shown->number[end + 1ull] == shown->number[end] + 1ull))
        {
            end += 1ull;
        }
        unsigned long long count = 0ull;
        char *const cell = cli_pages_lines_taken(view->key, shown->number[at], shown->number[end] + 1ull,
                                                 shown->text + at, &count);
        for (unsigned long long each = at; each <= end; each += 1ull)
        {
            mmgr_line_put(out, (each > 0ull) ? "," : "");
            if ((cell != NULL) && (each - at < count))
            {
                cli_json_quoted(out, shown->text[each],
                                cellul.len(&(CatenaFinitaCfg){.src = shown->text[each], .cap = CLI_CELLS_POOL}));
            }
            else
            {
                mmgr_line_put(out, "\"\"");
            }
        }
        g_cli_cells->persistent_buf_release(cell, &s_owner);
        at = end + 1ull;
    }
}

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

// a fold of the text stored among those seen where it opens on one of the frame's rows; the texts' lock is taken
// here, and no line is read under it
static void view_fold_seen(void *context, unsigned long long first, unsigned long long last, int shown)
{
    CliViewFoldsSeen *const folds = context;
    unsigned long long low = 0ull;
    unsigned long long high = folds->shown->count;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ull);
        if (folds->shown->number[middle] < first)
        {
            low = middle + 1ull;
        }
        else
        {
            high = middle;
        }
    }
    if ((low < folds->shown->count) && (folds->shown->number[low] == first) && (folds->count < folds->shown->count))
    {
        folds->seen[folds->count] = (CliViewFoldSeen){first, last, shown};
        folds->count += 1ull;
    }
}

// the folds seen written into the frame, each `[first, last, shown, closed, closer]`, `closed` 1 where the view holds
// it closed and `closer` the text of the line it closes on, its blanks at either end left off, where it shows that
// line, else empty
static void view_folds_written(CliView *view, const CliViewFoldsSeen *folds, VerbaLinea *out)
{
    for (unsigned long long at = 0ull; at < folds->count; at += 1ull)
    {
        const CliViewFoldSeen *const fold = &folds->seen[at];
        int closed = 0;
        for (unsigned int each = 0u; each < view->folds; each += 1u)
        {
            closed = closed || (view->fold[each].first == fold->first);
        }
        mmgr_line_put(out, (at > 0ull) ? ",[" : "[");
        mmgr_line_decimal(out, fold->first, 1u);
        mmgr_line_ch(out, ',');
        mmgr_line_decimal(out, fold->last, 1u);
        mmgr_line_put(out, fold->shown ? ",1," : ",0,");
        mmgr_line_put(out, closed ? "1," : "0,");
        if (fold->shown)
        {
            const unsigned long long length = view_length(view, fold->last);
            unsigned long long start = 0ull;
            unsigned long long end = length;
            while ((start < end) && view_blank_unit(view->line_units[start]))
            {
                start += 1ull;
            }
            while ((end > start) && view_blank_unit(view->line_units[end - 1ull]))
            {
                end -= 1ull;
            }
            cli_document_units_written(out, view->line_units + start, end - start);
        }
        else
        {
            mmgr_line_put(out, "\"\"");
        }
        mmgr_line_ch(out, ']');
    }
}

int cli_view_frame_written(const char *key, VerbaLinea *out)
{
    cli_lock_take(&s_lock);
    CliView *const view = view_of(key);
    if (view == NULL)
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    view->seq += 1ull;
    view_folds_refreshed(view);
    const unsigned long long lines = view_lines(view);
    view->top = view_shown_up(view, (view->top < lines) ? view->top : ((lines != 0ull) ? lines - 1ull : 0ull));
    const unsigned long long from = view->top;
    const unsigned long long rows = (view->rows != 0u) ? view->rows : 1u;
    // the rows: the lines shown from the top, `rows` of them at the most, by their numbers, in a cell with the room
    // for a paged text's lines read and for the folds that open on them
    CliViewRows shown = {NULL, NULL, 0ull};
    shown.number = g_cli_cells->persistent_buf_alloc(
        rows * (sizeof(*shown.number) + sizeof(*shown.text) + sizeof(CliViewFoldSeen)), &s_owner);
    shown.text = (shown.number != NULL) ? (const char **)(shown.number + rows) : NULL;
    CliViewFoldsSeen folds = {&shown, (shown.text != NULL) ? (CliViewFoldSeen *)(shown.text + rows) : NULL, 0ull};
    for (unsigned long long line = from; (shown.number != NULL) && (shown.count < rows) && (line < lines);)
    {
        shown.number[shown.count] = line;
        shown.count += 1ull;
        const unsigned long long next = view_down(view, line, 1ull);
        if (next <= line)
        {
            break;
        }
        line = next;
    }
    const unsigned long long to = (shown.count != 0ull) ? shown.number[shown.count - 1ull] + 1ull : from;
    mmgr_line_put(out, "{\"key\":");
    cli_json_quoted(out, view->key, cellul.len(&(CatenaFinitaCfg){.src = view->key, .cap = sizeof(view->key)}));
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
        view_paged_rows_written(view, &shown, out);
    }
    else
    {
        cli_document_lock_take();
        const CliStoredText *const one = cli_document_stored(view->key);
        for (unsigned long long at = 0ull; (one != NULL) && (at < shown.count); at += 1ull)
        {
            const unsigned long long line = shown.number[at];
            mmgr_line_put(out, (at > 0ull) ? "," : "");
            if (line < one->lines.lines)
            {
                cli_document_units_written(out, one->lines.unit + one->lines.start[line], one->lines.length[line]);
            }
            else
            {
                mmgr_line_put(out, "\"\"");
            }
        }
        cli_document_lock_give();
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
        (void)cli_document_folds_each(view->key, view_fold_seen, &folds);
        view_folds_written(view, &folds, out);
    }
    mmgr_line_ch(out, ']');
    mmgr_line_put(out, ",\"cursor\":");
    view_place_written(out, view->cursor);
    mmgr_line_put(out, ",\"anchor\":");
    view_place_written(out, view->anchor);
    CliPlace pair[2] = {{0ull, 0ull}, {0ull, 0ull}};
    mmgr_line_put(out, ",\"pair\":");
    if (view_pair(view, pair))
    {
        mmgr_line_ch(out, '[');
        view_place_written(out, pair[0]);
        mmgr_line_ch(out, ',');
        view_place_written(out, pair[1]);
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
    mmgr_line_decimal(out, view_tab(view), 1u);
    mmgr_line_ch(out, '}');
    // find's bar and the matches among the rows
    if (view->find_open)
    {
        view_searched(view);
    }
    mmgr_line_put(out, ",\"find\":");
    if (view->find_open)
    {
        mmgr_line_put(out, "{\"open\":1,\"focus\":");
        mmgr_line_put(out, view->find_focus ? "1" : "0");
        mmgr_line_put(out, ",\"query\":");
        cli_document_units_written(out, view->find_query, view->find_length);
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
        (void)cli_document_paged(view->key, from, to);
    }
    else
    {
        (void)cli_document_held(view->key, from, to);
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
        if (cli_document_runs_written(view->key, shown.number[at], shown.number[end] + 1ull, 1, out) &&
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
    g_cli_cells->persistent_buf_release(shown.number, &s_owner);
    cli_lock_give(&s_lock);
    return 1;
}

int cli_view_cursor(const char *key, CliPlace *cursor, CliPlace *anchor)
{
    cli_lock_take(&s_lock);
    const CliView *const view = view_of(key);
    if (view == NULL)
    {
        cli_lock_give(&s_lock);
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
    cli_lock_give(&s_lock);
    return 1;
}

// the bytes the runs of the minimap's lines may take as they are read for their bars
#define CLI_VIEW_MAP_ROOM (1024ull * 1024ull)

// a whole number read off the JSON node `node`, `otherwise` where it is not one
static unsigned long long view_json_whole(const CliJson *json, unsigned int node, unsigned long long otherwise)
{
    if (!cli_json_is(json, node, CFG_JSON_NUMBER))
    {
        return otherwise;
    }
    const CliJsonNode *const number = &json->node[node];
    const char *const text = json->text + number->text;
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; at < number->length; at += 1ull)
    {
        if ((text[at] < '0') || (text[at] > '9'))
        {
            return otherwise;
        }
        // a digit's value is its distance from '0'
        value = (value * 10ull) + (unsigned long long)(text[at] - '0');
    }
    return (number->length != 0ull) ? value : otherwise;
}

// the flat runs array of the line `line` among the ranges `colors`, each `{"from", "runs"}`, CLI_JSON_NONE where none
// colors it
static unsigned int view_map_runs_of(const CliJson *json, unsigned int colors, unsigned long long line)
{
    const unsigned int ranges = cli_json_is(json, colors, CFG_JSON_ARRAY) ? json->node[colors].count : 0u;
    unsigned int found = CLI_JSON_NONE;
    for (unsigned int range = 0u; range < ranges; range += 1u)
    {
        const unsigned int one = cli_json_item(json, colors, range);
        const unsigned long long start = view_json_whole(json, cli_json_member(json, one, "from"), 0ull);
        const unsigned int runs = cli_json_member(json, one, "runs");
        const unsigned int lines = cli_json_is(json, runs, CFG_JSON_ARRAY) ? json->node[runs].count : 0u;
        if ((line >= start) && (line < start + lines))
        {
            found = cli_json_item(json, runs, (unsigned int)(line - start));
        }
    }
    return found;
}

// one bar written, `[start, width, name]`
static void view_map_bar_written(VerbaLinea *out, int *any, unsigned long long start, unsigned long long width,
                                 unsigned long long name)
{
    mmgr_line_put(out, *any ? ",[" : "[");
    mmgr_line_decimal(out, start, 1u);
    mmgr_line_ch(out, ',');
    mmgr_line_decimal(out, width, 1u);
    mmgr_line_ch(out, ',');
    mmgr_line_decimal(out, name, 1u);
    mmgr_line_ch(out, ']');
    *any = 1;
}

// the bars of `length` bytes of a line's `text` written as an array: each run of letters that are not blanks, within
// one run of `flat`, the line's runs as `[column, name, ...]`, a bar from its first cell as wide as its cells, a tab
// the cells to its next stop of `tab`
static void view_map_line_written(const char *text, unsigned long long length, const CliJson *json, unsigned int flat,
                                  unsigned int tab, VerbaLinea *out)
{
    const unsigned int values = cli_json_is(json, flat, CFG_JSON_ARRAY) ? json->node[flat].count : 0u;
    unsigned int run = 0u;
    unsigned long long unit = 0ull;
    unsigned long long cell = 0ull;
    unsigned long long begun = 0ull;
    unsigned long long begun_name = 0ull;
    int open = 0;
    int any = 0;
    mmgr_line_ch(out, '[');
    for (unsigned long long at = 0ull; at < length;)
    {
        unsigned long long took = 0ull;
        const unsigned int letter = cli_text_letter(text, length, at, &took);
        // the run the letter stands in: the last whose column is at or before it
        while (((2u * (run + 1u)) + 1u < values) &&
               (view_json_whole(json, cli_json_item(json, flat, 2u * (run + 1u)), 0ull) <= unit))
        {
            run += 1u;
        }
        const unsigned long long name = (values >= 2u) ? view_json_whole(json, cli_json_item(json, flat, (2u * run) + 1u), 0ull) : 0ull;
        const int blank = (letter == ' ') || (letter == '\t');
        const unsigned long long wide = (letter == '\t') ? (tab - (cell % tab)) : 1ull;
        if (open && (blank || (name != begun_name)))
        {
            view_map_bar_written(out, &any, begun, cell - begun, begun_name);
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
        view_map_bar_written(out, &any, begun, cell - begun, begun_name);
    }
    mmgr_line_ch(out, ']');
}

int cli_view_map_written(const char *key, unsigned long long first, unsigned long long last, VerbaLinea *out)
{
    cli_lock_take(&s_lock);
    CliView *const view = view_of(key);
    if (view == NULL)
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    const unsigned int tab = view_tab(view);
    const int paged = view->paged;
    mmgr_line_put(out, "{\"from\":");
    mmgr_line_decimal(out, first, 1u);
    mmgr_line_put(out, ",\"bars\":[");
    // the lines' runs read as the branch writes them, and their texts, a paged text's from its pages
    const unsigned long long count = (last >= first) ? last - first + 1ull : 0ull;
    char *const room = g_cli_cells->persistent_buf_alloc(CLI_VIEW_MAP_ROOM, &s_owner);
    const char **const line = g_cli_cells->persistent_buf_alloc(((count != 0ull) ? count : 1ull) * sizeof(*line), &s_owner);
    VerbaLinea runs = {room, CLI_VIEW_MAP_ROOM, 0ull};
    CliJson json;
    unsigned int colors = CLI_JSON_NONE;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    if ((room != NULL) && cli_document_runs_written(key, first, last, 0, &runs) && mmgr_line_ok(&runs))
    {
        colors = cli_json_taken(&json, runs.out, runs.at, 0u, 0ull, &err_msg);
    }
    unsigned long long size = 0ull;
    unsigned long long read = 0ull;
    char state[64];
    char *const texts = (line == NULL) ? NULL
                        : (paged ? cli_pages_lines_taken(key, first, last + 1ull, line, &read)
                                 : cli_document_lines_taken(key, first, last, &size, state, sizeof(state)));
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
            while ((rest < end) && (*rest != '\n'))
            {
                rest += 1;
            }
            length = (unsigned long long)(rest - text);
            rest = (rest < end) ? rest + 1 : NULL;
        }
        view_map_line_written(text, length, &json, (colors != CLI_JSON_NONE) ? view_map_runs_of(&json, colors, first + at) : CLI_JSON_NONE,
                              tab, out);
    }
    mmgr_line_put(out, "]}");
    if (colors != CLI_JSON_NONE)
    {
        cli_json_given(&json);
    }
    g_cli_cells->persistent_buf_release(texts, &s_owner);
    g_cli_cells->persistent_buf_release(line, &s_owner);
    g_cli_cells->persistent_buf_release(room, &s_owner);
    cli_lock_give(&s_lock);
    return 1;
}
