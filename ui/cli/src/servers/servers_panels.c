// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_panels.c: the panels over the editor and the language server of the view shown, on the window's thread. The
// hover rests on the pointer's cell, the tree answering first and the server after; the completion ranks the file's
// words at once and the server's items after; the signature waits on a bracket or a comma, the tree's first and the
// server's over it. Where each panel stands on the screen is the window's: these carry what each panel shows
#include "../text/sort.h"
#include "servers.h"

#include "../code/symbols.h"
#include "../command_line/pool.h"
#include "../editor/view.h"
#include "../plugins/plugins.h"
#include "../text/text.h"
#include "servers_internal.h"

// how long the pointer rests before a hover is asked for and lingers off it before the hover goes; how long the
// signature waits after a bracket or a comma; and how long the view rests before its server is told where
#define CLI_PANELS_HOVER_REST_MS 380ULL
#define CLI_PANELS_HOVER_LINGER_MS 300ULL
#define CLI_PANELS_SIGNATURE_WAIT_MS 120ULL
#define CLI_PANELS_VIEW_REST_MS 250ULL
// the most declarations the tree is asked for and the most of them a hover shows; the bytes a word takes; the lines
// above the cursor a signature is read from; the bytes of the state they are entered in; and the regions a view's first
// line is told as standing inside
#define CLI_PANELS_DECLARATIONS 200u
#define CLI_PANELS_HOVER_DECLARATIONS 6u
#define CLI_PANELS_WORD 256u
#define CLI_PANELS_SIGNATURE_LINES 4000ULL
#define CLI_PANELS_STATE 1024u
#define CLI_PANELS_OPENERS 64u
// what sets the parts of a hover apart
#define CLI_PANELS_HOVER_BETWEEN "\n---\n"

// the most items the completion's list holds, the bytes of its texts, and the most bytes of an item's label, type,
// detail and insert kept, each with its end; and the most bytes of the hover's Markdown with its end
#define CLI_PANELS_ITEMS_MAX 2048u
#define CLI_PANELS_TEXT_MAX (256u << 10u)
#define CLI_PANELS_LABEL_MAX 256u
#define CLI_PANELS_TYPE_MAX 64u
#define CLI_PANELS_DETAIL_MAX 1024u
#define CLI_PANELS_INSERT_MAX 4096u
#define CLI_PANELS_HOVER_MAX (16u << 10u)

// One completion item: the places among the list's text of its label, the label in small letters, its type, what it
// states of itself and what it inserts, the bytes of each but the lowered label, and how it scores against the prefix
typedef struct
{
    unsigned int label;
    unsigned int lowered;
    unsigned int type;
    unsigned int detail;
    unsigned int insert;
    unsigned int label_length;
    unsigned int type_length;
    unsigned int detail_length;
    unsigned int insert_length;
    unsigned int score;
} PanelsItem;

// The head of the completion's list: the items; those the prefix answers; the row chosen and the first row shown; the
// prefix, as typed and in small letters, its bytes and where it stands; and the bytes of the list's text in use
typedef struct
{
    unsigned int items;
    unsigned int count;
    unsigned int chosen;
    unsigned int first;
    char prefix[CLI_PANELS_WORD];
    char lowered[CLI_PANELS_WORD];
    unsigned long long prefix_length;
    unsigned long long line;
    unsigned long long from;
    unsigned long long text_used;
} PanelsListHead;

// The completion's list, one movable cell taken once at its most as the panels begin: its head, the items, those the
// prefix answers in rank order, and the text every item's bytes stand in with their ends. A change reads the list whole
// into a cell of the call's own and writes it back; a reader of the rows copies their texts out
typedef struct
{
    PanelsListHead head;
    PanelsItem item[CLI_PANELS_ITEMS_MAX];
    unsigned int ranked[CLI_PANELS_ITEMS_MAX];
    char text[CLI_PANELS_TEXT_MAX];
} PanelsCompletion;

// The panels: the pointer's cell over the editor's text; when the hover is asked for and when it goes, 0 for neither;
// the ask whose answer stands; the hover's Markdown in the panels' own bytes, whether it shows, and the word's place;
// the list's cell and whether a list is open in it; whether a list is wanted as the next frame places the cursor and
// whether it was forced open; the signature's store and whether it shows, when it is asked for, the place it was asked
// at and whether it is being asked for; the cursor; whether the text changed since the server last took it; when the
// server is sent where the view rests, whether it was sent, and the view's top, rows and lines then; when the changed
// text is read again; the key of the view last opened in its server; and the tree and the editor's rows
typedef struct
{
    int pointer_in_text;
    unsigned long long pointer_row;
    unsigned long long pointer_column;
    unsigned long long hover_due_ms;
    unsigned long long hover_hide_ms;
    unsigned long long hover_ask;
    char hover_text[CLI_PANELS_HOVER_MAX];
    int hover_shown;
    unsigned long long hover_length;
    unsigned long long hover_line;
    unsigned long long hover_from;
    unsigned long long hover_to;
    const CarcerMovableEntry *completion;
    int completion_open;
    int completion_wanted;
    int completion_forced;
    CliSignatureStore signature;
    int signature_shown;
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
    char open_key[CLI_NAME_MAX_LEN];
    const char *root;
    unsigned int rows;
} Panels;

static Panels s_panels;

// A hover being asked for: the ask it answers; the word and the tree; the word's place; the Markdown answered, in a
// cell of the ask's own; whether the tree's walk was still reading as the ask found no declaration; and the view's key
// and its file's tree path, whose language the answer is held to
typedef struct
{
    unsigned long long ask;
    const char *word_text;
    const char *root;
    unsigned long long line;
    unsigned long long from;
    unsigned long long to;
    char *text;
    unsigned long long length;
    int reading;
    const char *key;
    char path[CLI_PATH_MAX_LEN];
} PanelsHoverAsk;

// the most lines above the pointer's a parameter or a local is sought in, and the lines read at once
#define CLI_PANELS_LOCAL_LINES 512ULL
#define CLI_PANELS_LOCAL_CHUNK 64ULL

// A signature being asked for: the view's key and the file's tree path; the cursor's place; and whether one was written
// into the panels' store
typedef struct
{
    const char *key;
    const char *path;
    CliPosition cursor;
    int found;
} PanelsSignatureAsk;

typedef struct
{
    const char *key;
    const char *text;
    unsigned long long length;
    const char *lowered;
    const char *prefix;
    const char *prefix_lowered;
    const PanelsItem *item;
    PanelsCompletion *list;
    const char *type;
    const char *detail;
    const char *insert;
    unsigned long long insert_length;
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
    const CliDocumentNames *names;
    PanelsHoverAsk *hover_ask;
} PanelsCtx;

// the key of the view shown, else NULL
EMBED_INLINE const char *panels_viewed_key(const PanelsCtx *args)
{
    (void)args;
    const int shown = cli_view_shown();
    // a view's place is not negative past the test; the key is copied into the panels' own buffer
    static char s_viewed_key[CLI_NAME_MAX_LEN];
    return ((shown >= 0) && cli_view_key((unsigned int)shown, s_viewed_key)) ? s_viewed_key : NULL;
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
    s_panels.hover_shown = 0;
    s_panels.hover_length = 0ULL;
    s_panels.hover_hide_ms = 0ULL;
}

// the list closed, and nothing forced open; its cell stands for the next
EMBED_INLINE void panels_completion_close(const PanelsCtx *args)
{
    (void)args;
    s_panels.completion_forced = 0;
    s_panels.completion_open = 0;
}

// the signature gone, a wait for one with it
EMBED_INLINE void panels_signature_hide(const PanelsCtx *args)
{
    (void)args;
    s_panels.signature_shown = 0;
    s_panels.signature_due_ms = 0ULL;
}

// the open list read whole into a cell of the call's own, which the caller gives back: it, or NULL where no list is
// open or the cells cannot meet it
static PanelsCompletion *panels_list_copy(void)
{
    if (!s_panels.completion_open || (s_panels.completion == NULL))
    {
        return NULL;
    }
    PanelsCompletion *const list = g_cli_guard->temporary_buf_alloc(sizeof(*list), g_cli_servers_owner);
    if (list != NULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_panels.completion, .offset = 0ULL, .dst = list, .bytes = sizeof(*list)});
    }
    return list;
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
    return s_panels.pointer_in_text && s_panels.hover_shown && (key != NULL) &&
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
    if (!s_panels.hover_shown)
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

// a declaration of the hover's word written: its own line as code, then its type and place
EMBED_INLINE void panels_declaration_write(const PanelsCtx *args)
{
    const CliFoundDeclaration *const found = args->found;
    if (found->text[0] != '\0')
    {
        mmgr_line_put(args->out, "```\n");
        mmgr_line_put(args->out, found->text);
        mmgr_line_put(args->out, "\n```\n\n");
    }
    mmgr_line_put(args->out, found->type);
    mmgr_line_put(args->out, " `");
    mmgr_line_put(args->out, found->path);
    mmgr_line_ch(args->out, ':');
    mmgr_line_decimal(args->out, (unsigned long long)found->line + 1ULL, 1u);
    mmgr_line_ch(args->out, '`');
}

// the place among `names` of the token named `text`, the names' count where none is
EMBED_INLINE unsigned int panels_name_place(const PanelsCtx *args)
{
    const CliDocumentNames *const names = args->names;
    for (unsigned int at = 0u; at < names->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = names->name[at], .other = args->text, .cap = CLI_POOL_BYTES}))
        {
            return at;
        }
    }
    return names->count;
}

// The hover's word as a parameter or a local of the function the pointer stands in, off the runs the walk saved: the
// lines above the pointer's read upward to the line the grammar marks identifier.declaration.function, that line's
// own parameters read too, and the topmost run the grammar names identifier.parameter or identifier.local whose text
// is the word is its declaration, as the walk names every later use of a declared name the same. The declaration's
// line written as the tree's are, with its kind and place: 1, or 0 where none stands
EMBED_INLINE int panels_hover_local_run(const PanelsCtx *args)
{
    PanelsHoverAsk *const ask = args->hover_ask;
    CliDocumentNames *const names =
        EMBED_CALL(cli_document_names.names_alloc, CliDocumentNamesCfg, .key = ask->key, .owner = g_cli_servers_owner);
    if (names == NULL)
    {
        return 0;
    }
    const unsigned int parameter =
        EMBED_CALL(panels_name_place, PanelsCtx, .names = names, .text = "identifier.parameter");
    const unsigned int local = EMBED_CALL(panels_name_place, PanelsCtx, .names = names, .text = "identifier.local");
    const unsigned int head =
        EMBED_CALL(panels_name_place, PanelsCtx, .names = names, .text = "identifier.declaration.function");
    g_cli_guard->persistent_buf_release(names, g_cli_servers_owner);
    const unsigned long long low = (ask->line > CLI_PANELS_LOCAL_LINES) ? (ask->line - CLI_PANELS_LOCAL_LINES) : 0ULL;
    unsigned long long end = ask->line + 1ULL;
    unsigned long long found_line = 0ULL;
    const char *kind = NULL;
    int ended = 0;
    while (!ended && (end > low))
    {
        const unsigned long long first = ((end - low) > CLI_PANELS_LOCAL_CHUNK) ? (end - CLI_PANELS_LOCAL_CHUNK) : low;
        unsigned long long number[CLI_PANELS_LOCAL_CHUNK];
        const unsigned long long count = end - first;
        for (unsigned long long at = 0ULL; at < count; at += 1ULL)
        {
            number[at] = first + at;
        }
        CliDocumentRuns *const runs =
            EMBED_CALL(cli_document_runs.runs_alloc, CliDocumentRunsCfg, .key = ask->key, .number = number,
                       .count = count, .mirrored = 0, .owner = g_cli_servers_owner);
        if (runs == NULL)
        {
            break;
        }
        for (unsigned long long at = count; (at > 0ULL) && !ended; at -= 1ULL)
        {
            const unsigned long long line = first + at - 1ULL;
            const CliDocumentLineRuns one = runs->line[at - 1ULL];
            const unsigned int *const col = runs->word_text + one.at;
            const unsigned int *const name = col + one.runs;
            unsigned long long units_count = 0ULL;
            (void)EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = ask->key, .line = line,
                             .count = &units_count);
            // the line's units copied out into a cell of the temporary tier
            unsigned short *const units =
                (units_count != 0ULL)
                    ? g_cli_guard->temporary_buf_alloc(units_count * sizeof(*units), g_cli_servers_owner)
                    : NULL;
            if (units != NULL)
            {
                (void)EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = ask->key, .line = line,
                                 .count = &units_count, .dst = units, .capacity = units_count);
            }
            for (unsigned int run = 0u; run < one.runs; run += 1u)
            {
                ended = ended || ((name[run] == head) && (line < ask->line));
                const int is_parameter = name[run] == parameter;
                if ((!is_parameter && (name[run] != local)) || (units == NULL))
                {
                    continue;
                }
                const unsigned long long from = col[run];
                const unsigned long long to = ((run + 1u) < one.runs) ? col[run + 1u] : units_count;
                if ((to > units_count) || (from >= to))
                {
                    continue;
                }
                char text[CLI_PANELS_WORD];
                VerbaLinea out = {text, sizeof(text), 0ULL};
                EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &out,
                           .units = units + from, .count = to - from);
                if ((mmgr_line_finish(&out) != 0ULL) &&
                    cellul.eq(&(CatenaFinitaCfg){.src = text, .other = ask->word_text, .cap = sizeof(text)}))
                {
                    kind = is_parameter ? "parameter" : "local";
                    found_line = line;
                }
            }
            g_cli_guard->persistent_buf_release(units, g_cli_servers_owner);
        }
        g_cli_guard->persistent_buf_release(runs, g_cli_servers_owner);
        end = first;
    }
    if (kind == NULL)
    {
        return 0;
    }
    unsigned long long size = 0ULL;
    char state[CLI_NAME_MAX_LEN];
    char *const line_text = EMBED_CALL(cli_document_key.lines_alloc, CliDocumentKeyCfg, .key = ask->key,
                                       .first = found_line, .last = found_line, .size_pointer = &size, .state = state,
                                       .capacity = sizeof(state), .owner = g_cli_servers_owner);
    if (line_text == NULL)
    {
        return 0;
    }
    const unsigned long long start = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                .text = line_text, .at = 0ULL, .size = size, .set = " \t");
    const unsigned long long bytes =
        size + cellul.len(&(CatenaFinitaCfg){.src = ask->path, .cap = CLI_POOL_BYTES}) + 64ULL;
    char *const cell = g_cli_guard->temporary_buf_alloc(bytes, g_cli_servers_owner);
    VerbaLinea joined = mmgr_line_over(cell, bytes);
    if (cell != NULL)
    {
        const CliFoundDeclaration one = {ask->word_text, kind, ask->path, (unsigned int)found_line, ""};
        mmgr_line_put(&joined, "```\n");
        mmgr_line_put_n(&joined, line_text + start, size - start);
        mmgr_line_put(&joined, "\n```\n\n");
        EMBED_CALL(panels_declaration_write, PanelsCtx, .found = &one, .out = &joined);
    }
    g_cli_guard->persistent_buf_release(line_text, g_cli_servers_owner);
    if ((cell == NULL) || !mmgr_line_ok(&joined))
    {
        g_cli_guard->persistent_buf_release(cell, g_cli_servers_owner);
        return 0;
    }
    ask->text = cell;
    ask->length = joined.at;
    return 1;
}

// the tree's declarations of the word laid as Markdown in a cell sized to them, those of files that open in the
// hovered file's language plugin alone
static void panels_hover_tree_run(void *context)
{
    PanelsHoverAsk *const ask = (PanelsHoverAsk *)context;
    if (EMBED_CALL(panels_hover_local_run, PanelsCtx, .hover_ask = ask))
    {
        return;
    }
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = ask->root);
    CliFoundDeclarations *const found =
        EMBED_CALL(cli_symbols_query_max.tree_declarations_alloc, CliSymbolsQueryMaxCfg, .query = ask->word_text,
                   .most = CLI_PANELS_DECLARATIONS, .owner = g_cli_servers_owner);
    if (found == NULL)
    {
        return;
    }
    CliLanguages languages;
    EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &languages);
    char wanted[CLI_NAME_MAX_LEN];
    const int known =
        (languages.pair != NULL) && EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                               .languages_read = &languages, .path = ask->path, .id_out = wanted);
    unsigned long long bytes = 1ULL;
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliFoundDeclaration *const one = &found->found[at];
        bytes += cellul.len(&(CatenaFinitaCfg){.src = one->text, .cap = CLI_POOL_BYTES}) +
                 cellul.len(&(CatenaFinitaCfg){.src = one->type, .cap = CLI_POOL_BYTES}) +
                 cellul.len(&(CatenaFinitaCfg){.src = one->path, .cap = CLI_POOL_BYTES}) + 64ULL;
    }
    char *const cell = g_cli_guard->temporary_buf_alloc(bytes, g_cli_servers_owner);
    VerbaLinea joined = mmgr_line_over(cell, bytes);
    unsigned int parts = 0u;
    for (unsigned int at = 0u; (cell != NULL) && (at < found->count) && (parts < CLI_PANELS_HOVER_DECLARATIONS);
         at += 1u)
    {
        const CliFoundDeclaration *const one = &found->found[at];
        char id[CLI_NAME_MAX_LEN];
        const int same = !known || (EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                               .languages_read = &languages, .path = one->path, .id_out = id) &&
                                    cellul.eq(&(CatenaFinitaCfg){.src = id, .other = wanted, .cap = sizeof(id)}));
        if (same && cellul.eq(&(CatenaFinitaCfg){.src = one->name, .other = ask->word_text, .cap = CLI_PANELS_WORD}))
        {
            mmgr_line_put(&joined, (parts > 0u) ? CLI_PANELS_HOVER_BETWEEN : "");
            EMBED_CALL(panels_declaration_write, PanelsCtx, .found = one, .out = &joined);
            parts += 1u;
        }
    }
    ask->reading = found->reading;
    g_cli_guard->persistent_buf_release(found, g_cli_servers_owner);
    g_cli_guard->persistent_buf_release(languages.pair, g_cli_plugins_owner);
    if ((parts == 0u) || !mmgr_line_ok(&joined))
    {
        g_cli_guard->persistent_buf_release(cell, g_cli_servers_owner);
        return;
    }
    ask->text = cell;
    ask->length = joined.at;
}

// the tree's answer: it stands as the hover, its Markdown copied into the hover's own bytes, cut to them, and the ask's
// cell given back
static void panels_hover_tree_done(void *context)
{
    PanelsHoverAsk *const ask = (PanelsHoverAsk *)context;
    EMBED_CALL(panels_hover_hide, PanelsCtx, .at = 0u);
    if (ask->text != NULL)
    {
        const unsigned long long kept =
            (ask->length < CLI_PANELS_HOVER_MAX) ? ask->length : CLI_PANELS_HOVER_MAX - 1ULL;
        if (kept != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = s_panels.hover_text, .src = ask->text, .bytes = kept});
        }
        s_panels.hover_text[kept] = '\0';
        s_panels.hover_length = kept;
        s_panels.hover_shown = 1;
        g_cli_guard->persistent_buf_release(ask->text, g_cli_servers_owner);
        ask->text = NULL;
    }
    s_panels.hover_line = ask->line;
    s_panels.hover_from = ask->from;
    s_panels.hover_to = ask->to;
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
    const int positioned = EMBED_CALL(cli_view_row.position_at, CliViewRowCfg, .key = key, .row = s_panels.pointer_row,
                                      .column = s_panels.pointer_column, .line = &line, .col = &col);
    const int worded = positioned && EMBED_CALL(cli_view_line.word_at, CliViewLineCfg, .key = key, .line_value = line,
                                                .col_value = col, .from = &from, .to = &to, .word_text = word_text,
                                                .capacity = sizeof(word_text));
    if (!worded)
    {
        EMBED_CALL(panels_hover_hide, PanelsCtx, .at = 0u);
        return;
    }
    PanelsHoverAsk asked = {s_panels.hover_ask, word_text, s_panels.root, line, from, to, NULL, 0ULL, 0, key, ""};
    if (!EMBED_CALL(cli_view_path_type.path, CliViewPathTypeCfg, .key = key, .path_out = asked.path))
    {
        asked.path[0] = '\0';
    }
    panels_hover_tree_run(&asked);
    // a word the tree's walk has not reached yet is asked for again after the rest, while the pointer stays on it
    const int again = (asked.text == NULL) && asked.reading;
    panels_hover_tree_done(&asked);
    if (again)
    {
        s_panels.hover_due_ms = cli_clock_milliseconds() + CLI_PANELS_HOVER_REST_MS;
    }
}

// how the item scores against the prefix: 5 its label starts with it, 4 starts with it case aside, 3 holds it case
// aside, 2 holds its letters in order case aside, 1 for no prefix, 0 for no answer
EMBED_INLINE unsigned int panels_item_score(const PanelsCtx *args)
{
    const PanelsItem *const item = args->item;
    const char *const label = args->list->text + item->label;
    const char *const lowered = args->list->text + item->lowered;
    const unsigned long long want = args->length;
    if (want == 0ULL)
    {
        return 1u;
    }
    if (want > item->label_length)
    {
        return 0u;
    }
    if (memor.cmp(&(MemoriaCfg){.src = label, .other = args->prefix, .bytes = want}) == 0)
    {
        return 5u;
    }
    if (memor.cmp(&(MemoriaCfg){.src = lowered, .other = args->prefix_lowered, .bytes = want}) == 0)
    {
        return 4u;
    }
    char first[2] = {args->prefix_lowered[0], '\0'};
    const unsigned long long last = item->label_length - want + 1ULL;
    for (unsigned long long start = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                               .text = lowered, .at = 0ULL, .size = last, .set = first);
         start < last; start = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = lowered,
                                          .at = start + 1ULL, .size = last, .set = first))
    {
        if (memor.cmp(&(MemoriaCfg){.src = lowered + start, .other = args->prefix_lowered, .bytes = want}) == 0)
        {
            return 3u;
        }
    }
    unsigned long long at = 0ULL;
    for (unsigned long long letter = 0ULL; letter < want; letter += 1ULL)
    {
        first[0] = args->prefix_lowered[letter];
        at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = lowered, .at = at,
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
    const embed_iword order = memor.cmp(&(MemoriaCfg){
        .src = s_ranking->text + first->label, .other = s_ranking->text + second->label, .bytes = first->label_length});
    if (order == 0)
    {
        return 0;
    }
    return (order < 0) ? -1 : 1;
}

// the items the prefix answers ranked in `list`, the row chosen saved on the item it was on where it is still shown
EMBED_INLINE void panels_completion_rank(PanelsCompletion *list)
{
    PanelsListHead *const head = &list->head;
    const unsigned int saved = (head->count > 0u) ? list->ranked[head->chosen] : head->items;
    head->count = 0u;
    for (unsigned int at = 0u; at < head->items; at += 1u)
    {
        PanelsItem *const item = &list->item[at];
        item->score =
            EMBED_CALL(panels_item_score, PanelsCtx, .item = item, .list = list, .length = head->prefix_length,
                       .prefix = head->prefix, .prefix_lowered = head->lowered);
        if (item->score > 0u)
        {
            list->ranked[head->count] = at;
            head->count += 1u;
        }
    }
    s_ranking = list;
    EMBED_CALL(cli_sort, CliSortCfg, .base = list->ranked, .count = head->count, .size = sizeof(*list->ranked),
               .order = panels_item_order);
    s_ranking = NULL;
    head->chosen = 0u;
    for (unsigned int at = 0u; at < head->count; at += 1u)
    {
        if (list->ranked[at] == saved)
        {
            head->chosen = at;
        }
    }
}

// the first row shown moved as far as it must to show the row chosen
EMBED_INLINE void panels_completion_scrolled(PanelsListHead *head)
{
    if (head->chosen < head->first)
    {
        head->first = head->chosen;
    }
    else if (head->chosen >= (head->first + CLI_COMPLETION_ROWS))
    {
        head->first = head->chosen - CLI_COMPLETION_ROWS + 1u;
    }
    const unsigned int most = (head->count > CLI_COMPLETION_ROWS) ? (head->count - CLI_COMPLETION_ROWS) : 0u;
    head->first = (head->first > most) ? most : head->first;
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

// `length` bytes of `text` copied into the list's text after what it holds, cut to `most` with their end: the copy's
// place among the text, its bytes kept into `length_out`; CLI_PANELS_TEXT_MAX where the text has no room
static unsigned int panels_text_put(PanelsCompletion *list, const char *text, unsigned long long length,
                                    unsigned long long most, unsigned int *length_out)
{
    PanelsListHead *const head = &list->head;
    const unsigned long long kept = (length < most) ? length : most - 1ULL;
    if (head->text_used + kept + 1ULL > CLI_PANELS_TEXT_MAX)
    {
        *length_out = 0u;
        return CLI_PANELS_TEXT_MAX;
    }
    // the text in use is below CLI_PANELS_TEXT_MAX, inside an unsigned int, as is a text cut to its most
    const unsigned int at = (unsigned int)head->text_used;
    if (kept != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = list->text + at, .src = text, .bytes = kept});
    }
    list->text[at + kept] = '\0';
    head->text_used += kept + 1ULL;
    *length_out = (unsigned int)kept;
    return at;
}

// an item put after the list's, its label of `length` bytes at `text`, its type, its detail and its insert of
// `insert_length` bytes copied into the list's text and the label lowered once: 1, or 0 where the items or the text are
// full, the text as it stood
EMBED_INLINE int panels_item_put(const PanelsCtx *args)
{
    PanelsCompletion *const list = args->list;
    PanelsListHead *const head = &list->head;
    const unsigned long long used = head->text_used;
    if (head->items == CLI_PANELS_ITEMS_MAX)
    {
        return 0;
    }
    PanelsItem item = {0};
    unsigned int lowered_length = 0u;
    item.label = panels_text_put(list, args->text, args->length, CLI_PANELS_LABEL_MAX, &item.label_length);
    item.lowered = panels_text_put(list, args->text, args->length, CLI_PANELS_LABEL_MAX, &lowered_length);
    item.type =
        panels_text_put(list, args->type, cellul.len(&(CatenaFinitaCfg){.src = args->type, .cap = CLI_POOL_BYTES}),
                        CLI_PANELS_TYPE_MAX, &item.type_length);
    item.detail =
        panels_text_put(list, args->detail, cellul.len(&(CatenaFinitaCfg){.src = args->detail, .cap = CLI_POOL_BYTES}),
                        CLI_PANELS_DETAIL_MAX, &item.detail_length);
    item.insert = panels_text_put(list, args->insert, args->insert_length, CLI_PANELS_INSERT_MAX, &item.insert_length);
    if ((item.label == CLI_PANELS_TEXT_MAX) || (item.lowered == CLI_PANELS_TEXT_MAX) ||
        (item.type == CLI_PANELS_TEXT_MAX) || (item.detail == CLI_PANELS_TEXT_MAX) ||
        (item.insert == CLI_PANELS_TEXT_MAX))
    {
        head->text_used = used;
        return 0;
    }
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .size = lowered_length,
               .text_out = list->text + item.lowered);
    list->item[head->items] = item;
    head->items += 1u;
    return 1;
}

// the list laid in `list`, a cell of the call's own, from the server's items and the file's words, a word the server
// gave left out, the labels lowered once and each snippet flattened once, every text copied into the list's own, the
// items ending where the items or the text are full
EMBED_INLINE void panels_completion_laid(const PanelsCtx *args)
{
    PanelsCompletion *const list = args->list;
    PanelsListHead *const head = &list->head;
    const unsigned int from_server = (args->server != NULL) ? args->server->count : 0u;
    const unsigned int from_words = (args->words != NULL) ? args->words->count : 0u;
    memor.set(&(MemoriaCfg){.dst = head, .val = 0, .bytes = sizeof(*head)});
    head->line = args->line;
    head->from = args->from;
    head->prefix_length = (args->length < (CLI_PANELS_WORD - 1ULL)) ? args->length : (CLI_PANELS_WORD - 1ULL);
    if (head->prefix_length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = head->prefix, .src = args->prefix, .bytes = head->prefix_length});
        memor.cpy(&(MemoriaCfg){.dst = head->lowered, .src = args->prefix, .bytes = head->prefix_length});
    }
    head->prefix[head->prefix_length] = '\0';
    head->lowered[head->prefix_length] = '\0';
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .size = head->prefix_length, .text_out = head->lowered);
    int room = 1;
    for (unsigned int at = 0u; room && (at < from_server); at += 1u)
    {
        const CliServerItem *const one = &args->server->item[at];
        const unsigned long long label_length =
            cellul.len(&(CatenaFinitaCfg){.src = one->label, .cap = CLI_POOL_BYTES});
        if (label_length == 0ULL)
        {
            continue;
        }
        const unsigned long long insert_length =
            cellul.len(&(CatenaFinitaCfg){.src = one->insert, .cap = CLI_POOL_BYTES});
        const char *const insert = (insert_length != 0ULL) ? one->insert : one->label;
        const unsigned long long inserted = (insert_length != 0ULL) ? insert_length : label_length;
        // a snippet flattened to what it inserts, in a buffer of the insert's most; one past it inserts its label
        char flat_text[CLI_PANELS_INSERT_MAX];
        VerbaLinea flat = {flat_text, sizeof(flat_text), 0ULL};
        if (one->snippet)
        {
            EMBED_CALL(panels_snippet_write, PanelsCtx, .text = insert, .length = inserted, .out = &flat);
        }
        const int flattened = one->snippet && mmgr_line_ok(&flat);
        room = EMBED_CALL(panels_item_put, PanelsCtx, .list = list, .text = one->label, .length = label_length,
                          .type = one->type, .detail = one->detail,
                          .insert = flattened ? flat_text : (one->snippet ? one->label : insert),
                          .insert_length = flattened ? flat.at : (one->snippet ? label_length : inserted));
    }
    const unsigned int supplied = head->items;
    for (unsigned int at = 0u; room && (at < from_words); at += 1u)
    {
        const char *const label = args->words->text + args->words->at[at];
        const unsigned long long label_length = args->words->length[at];
        int supplied_already = 0;
        for (unsigned int other = 0u; !supplied_already && (other < supplied); other += 1u)
        {
            supplied_already =
                (list->item[other].label_length == label_length) &&
                (memor.cmp(&(MemoriaCfg){
                     .src = list->text + list->item[other].label, .other = label, .bytes = label_length}) == 0);
        }
        if (supplied_already)
        {
            continue;
        }
        room = EMBED_CALL(panels_item_put, PanelsCtx, .list = list, .text = label, .length = label_length,
                          .type = "word", .detail = "", .insert = label, .insert_length = label_length);
    }
}

// the list `list` made the one shown in place of the one that stood, the row chosen saved on its item: closed where
// nothing answers, where the one answer is what is typed and nothing forced it, or where nothing is typed and no list
// stood or forced it open; else written into the cell as the open list
EMBED_INLINE void panels_completion_stand(PanelsCompletion *list)
{
    PanelsListHead *const head = &list->head;
    // the list that stood: its first row and its row chosen's label, read out of the cell before it is written over
    PanelsListHead was = {0};
    const int stood = s_panels.completion_open && (s_panels.completion != NULL);
    if (stood)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_panels.completion, .offset = 0ULL, .dst = &was, .bytes = sizeof(was)});
    }
    char was_label[CLI_PANELS_LABEL_MAX] = "";
    unsigned int was_label_length = 0u;
    if (stood && (was.count > 0u))
    {
        PanelsItem chosen;
        {
            unsigned int ranked_at = 0u;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = s_panels.completion,
                .offset = offsetof(PanelsCompletion, ranked) + ((unsigned long long)was.chosen * sizeof(ranked_at)),
                .dst = &ranked_at,
                .bytes = sizeof(ranked_at)});
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = s_panels.completion,
                .offset = offsetof(PanelsCompletion, item) + ((unsigned long long)ranked_at * sizeof(PanelsItem)),
                .dst = &chosen,
                .bytes = sizeof(PanelsItem)});
        }
        was_label_length = chosen.label_length;
        if (chosen.label_length != 0u)
        {
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = s_panels.completion,
                                    .offset = offsetof(PanelsCompletion, text) + chosen.label,
                                    .dst = was_label,
                                    .bytes = chosen.label_length});
        }
        was_label[chosen.label_length] = '\0';
    }
    panels_completion_rank(list);
    if (stood && (was.count > 0u))
    {
        for (unsigned int at = 0u; at < head->count; at += 1u)
        {
            const PanelsItem *const item = &list->item[list->ranked[at]];
            if ((item->label_length == was_label_length) &&
                (memor.cmp(&(MemoriaCfg){
                     .src = list->text + item->label, .other = was_label, .bytes = item->label_length}) == 0))
            {
                head->chosen = at;
                break;
            }
        }
        head->first = was.first;
    }
    const PanelsItem *const only = (head->count == 1u) ? &list->item[list->ranked[0]] : NULL;
    const int alone = (only != NULL) && (only->label_length == head->prefix_length) &&
                      (memor.cmp(&(MemoriaCfg){
                           .src = list->text + only->label, .other = head->prefix, .bytes = head->prefix_length}) == 0);
    const int empty = (head->prefix_length == 0ULL) && !s_panels.completion_forced && !stood;
    if ((head->count == 0u) || (alone && !s_panels.completion_forced) || empty)
    {
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
        return;
    }
    panels_completion_scrolled(head);
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = s_panels.completion, .offset = 0ULL, .src = list, .bytes = sizeof(PanelsCompletion)});
    s_panels.completion_open = 1;
}

// the completions asked for the word before the cursor: the file's words
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
    if (s_panels.completion == NULL)
    {
        return;
    }
    // the list laid in a cell of the call's own from the file's words, which go once their texts are copied
    PanelsCompletion *const list = g_cli_guard->temporary_buf_alloc(sizeof(*list), g_cli_servers_owner);
    CliDocumentWords *const words = cli_view_words_alloc(key);
    if (list != NULL)
    {
        EMBED_CALL(panels_completion_laid, PanelsCtx, .list = list, .server = NULL, .words = words, .line = cursor.line,
                   .from = from, .prefix = prefix,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = sizeof(prefix)}));
    }
    cli_view_words_release(words);
    if (list != NULL)
    {
        panels_completion_stand(list);
        g_cli_guard->persistent_buf_release(list, g_cli_servers_owner);
    }
}

// the list, where it is open, followed to the frame's cursor: filtered again by the word before the cursor where it
// changed, and closed where the cursor has left the line or that word no longer starts where the prefix did
EMBED_INLINE void panels_completion_follow(const PanelsCtx *args)
{
    PanelsListHead head = {0};
    const int opened = s_panels.completion_open && (s_panels.completion != NULL);
    if (!opened)
    {
        return;
    }
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_panels.completion, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    unsigned long long from = 0ULL;
    char prefix[CLI_PANELS_WORD];
    if ((s_panels.cursor.line != head.line) ||
        !EMBED_CALL(cli_view_from.prefix, CliViewFromCfg, .key = args->key, .from = &from, .prefix = prefix,
                    .capacity = sizeof(prefix)) ||
        (from != head.from))
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
    if ((length == head.prefix_length) &&
        (memor.cmp(&(MemoriaCfg){.src = prefix, .other = head.prefix, .bytes = length}) == 0))
    {
        return;
    }
    PanelsCompletion *const list = panels_list_copy();
    if (list == NULL)
    {
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = list->head.prefix, .src = prefix, .bytes = length + 1ULL});
    memor.cpy(&(MemoriaCfg){.dst = list->head.lowered, .src = prefix, .bytes = length + 1ULL});
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .size = length, .text_out = list->head.lowered);
    list->head.prefix_length = length;
    panels_completion_rank(list);
    if (list->head.count == 0u)
    {
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
    }
    else
    {
        panels_completion_scrolled(&list->head);
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = s_panels.completion, .offset = 0ULL, .src = list, .bytes = sizeof(PanelsCompletion)});
        s_panels.completion_open = 1;
    }
    g_cli_guard->persistent_buf_release(list, g_cli_servers_owner);
}

// the row `at` of the list accepted: its insert copied out of the cell and written over the prefix before the cursor,
// the list closed
EMBED_INLINE void panels_completion_accept(const PanelsCtx *args)
{
    PanelsListHead head = {0};
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    const int opened = s_panels.completion_open && (s_panels.completion != NULL);
    if (opened)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_panels.completion, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    }
    if (!opened || (key == NULL) || (args->at >= head.count))
    {
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
        return;
    }
    PanelsItem item;
    {
        unsigned int ranked_at = 0u;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_panels.completion,
            .offset = offsetof(PanelsCompletion, ranked) + ((unsigned long long)args->at * sizeof(ranked_at)),
            .dst = &ranked_at,
            .bytes = sizeof(ranked_at)});
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_panels.completion,
            .offset = offsetof(PanelsCompletion, item) + ((unsigned long long)ranked_at * sizeof(PanelsItem)),
            .dst = &item,
            .bytes = sizeof(PanelsItem)});
    }
    char insert[CLI_PANELS_INSERT_MAX];
    if (item.insert_length != 0u)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_panels.completion,
                                .offset = offsetof(PanelsCompletion, text) + item.insert,
                                .dst = insert,
                                .bytes = item.insert_length});
    }
    insert[item.insert_length] = '\0';
    const unsigned long long back = ((s_panels.cursor.line == head.line) && (s_panels.cursor.col >= head.from))
                                        ? (s_panels.cursor.col - head.from)
                                        : head.prefix_length;
    const CliViewInput accepted = {
        .type = CLI_VIEW_COMPLETE, .text = insert, .length = item.insert_length, .column = back};
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
    PanelsListHead head = {0};
    const int opened = s_panels.completion_open && (s_panels.completion != NULL);
    if (opened)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_panels.completion, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    }
    if (!opened || (head.count <= CLI_COMPLETION_ROWS))
    {
        return;
    }
    const unsigned int most = head.count - CLI_COMPLETION_ROWS;
    if (rows < 0)
    {
        // the rows turned up, a count far inside what an unsigned int represents
        const unsigned int up = (unsigned int)(-rows);
        head.first = (head.first > up) ? (head.first - up) : 0u;
    }
    else
    {
        // the rows turned are not negative
        head.first += (unsigned int)rows;
        head.first = (head.first > most) ? most : head.first;
    }
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = s_panels.completion, .offset = 0ULL, .src = &head, .bytes = sizeof(PanelsListHead)});
}

// a key's effect on the open list, 1 where the key was the list's
EMBED_INLINE int panels_completion_key(const PanelsCtx *args)
{
    PanelsListHead head = {0};
    const int opened = s_panels.completion_open && (s_panels.completion != NULL);
    if (opened)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_panels.completion, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    }
    if (!opened || (head.count == 0u))
    {
        return 0;
    }
    const char *const name = args->text;
    const unsigned int count = head.count;
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Up", .cap = 64ULL}))
    {
        head.chosen = (head.chosen + count - 1u) % count;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Down", .cap = 64ULL}))
    {
        head.chosen = (head.chosen + 1u) % count;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "PageUp", .cap = 64ULL}))
    {
        head.chosen = (head.chosen > CLI_COMPLETION_ROWS) ? (head.chosen - CLI_COMPLETION_ROWS) : 0u;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "PageDown", .cap = 64ULL}))
    {
        head.chosen =
            ((head.chosen + CLI_COMPLETION_ROWS) < count) ? (head.chosen + CLI_COMPLETION_ROWS) : (count - 1u);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Enter", .cap = 64ULL}) ||
             cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Tab", .cap = 64ULL}))
    {
        EMBED_CALL(panels_completion_accept, PanelsCtx, .at = head.chosen);
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
    panels_completion_scrolled(&head);
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = s_panels.completion, .offset = 0ULL, .src = &head, .bytes = sizeof(PanelsListHead)});
    return 1;
}

int cli_servers_panels_key(const char *name)
{
    if (EMBED_CALL(panels_completion_key, PanelsCtx, .text = name))
    {
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Escape", .cap = 64ULL}) &&
        (s_panels.signature_shown || s_panels.hover_shown))
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
    if (s_panels.completion_open)
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

static void panels_signature_tree_run(void *context);
static void panels_signature_tree_done(void *context);

// the signature asked for the call the cursor is in, the tree's first, the cursor's place recorded as the one asked
// about
EMBED_INLINE void panels_signature_ask(const PanelsCtx *args)
{
    (void)args;
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    char path[CLI_PATH_MAX_LEN];
    CliPosition cursor;
    if ((key == NULL) || s_panels.signature_asking ||
        !EMBED_CALL(cli_view_path_type.path, CliViewPathTypeCfg, .key = key, .path_out = path) ||
        !EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = key, .cursor = &cursor))
    {
        return;
    }
    PanelsSignatureAsk ask = {key, path, cursor, 0};
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
    ask->found =
        EMBED_CALL(cli_symbols_path_text_size_out.signature_read, CliSymbolsPathTextSizeOutCfg, .path = ask->path,
                   .text = text, .size = size, .state = (state[0] != '\0') ? state : "root", .line = line - top,
                   .col = ask->cursor.col, .err_msg = &ignored, .store = &s_panels.signature);
    g_cli_guard->persistent_buf_release(text, g_cli_servers_owner);
}

// the tree's answer stands, where one was written into the store
static void panels_signature_tree_done(void *context)
{
    PanelsSignatureAsk *const ask = (PanelsSignatureAsk *)context;
    EMBED_CALL(panels_signature_hide, PanelsCtx, .at = 0u);
    s_panels.signature_shown = ask->found;
    ask->found = 0;
    s_panels.signature_asking = 0;
}

// What a serve job does: open the file in its server with its text, send its changed text, or send where its view rests
#define PANELS_SERVE_OPEN 0u
#define PANELS_SERVE_CHANGE 1u
#define PANELS_SERVE_VIEW 2u

// A serve job: what it does; the file's whole path and its language; the view's first and last lines and the lines
// that open the regions around its first line, outermost first; the problems' sink; and the size of the text that
// follows the job in its cell
typedef struct
{
    unsigned int does;
    char full[CLI_PATH_MAX_LEN];
    char language[CLI_NAME_MAX_LEN];
    unsigned int first;
    unsigned int last;
    unsigned int openers;
    unsigned int opener[CLI_PANELS_OPENERS];
    CliServersSink sink;
    unsigned long long size;
} PanelsServeJob;

// one serve job on a thread of the servers, lined up by the view thread, which goes on at once. The server's open and
// change cut a slice, and the slice cut walks the file and its includes
static void panels_serve_job(const CliServersJobCfg *args)
{
    const PanelsServeJob *const job = (const PanelsServeJob *)(const void *)args->job;
    const char *const text = args->job + sizeof(*job);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    if (job->does == PANELS_SERVE_OPEN)
    {
        int served = 0;
        (void)EMBED_CALL(cli_servers_root.open, CliServersRootCfg, .root = s_panels.root, .path = job->full,
                         .language = job->language, .text = text, .size = job->size, .sink = &job->sink,
                         .served = &served, .err_msg = &ignored);
    }
    else if (job->does == PANELS_SERVE_CHANGE)
    {
        (void)EMBED_CALL(cli_servers_root.change, CliServersRootCfg, .path = job->full, .text = text, .size = job->size,
                         .err_msg = &ignored);
    }
    else
    {
        char output_text[16];
        VerbaLinea output = {output_text, sizeof(output_text), 0ULL};
        (void)EMBED_CALL(cli_servers_path_out_err_msg.view, CliServersPathOutErrMsgCfg, .path = job->full,
                         .first = job->first, .last = job->last, .opener = job->opener, .openers = job->openers,
                         .out = &output, .err_msg = &ignored);
    }
}

// the serve job `job` with `size` bytes of `text` after it laid in a cell of this call's own and handed to a thread of
// the servers in a cell of theirs: 1, or 0 where the cells cannot meet it
static int panels_serve_later(const PanelsServeJob *job, const char *text, unsigned long long size)
{
    char *const laid = g_cli_guard->temporary_buf_alloc(sizeof(*job) + size, g_cli_servers_owner);
    if (laid == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = laid, .src = job, .bytes = sizeof(*job)});
    ((PanelsServeJob *)(void *)laid)->size = size;
    if (size != 0ULL)
    {
        // a text the editor holds fits a embed_word
        memor.cpy(&(MemoriaCfg){.dst = laid + sizeof(*job), .src = text, .bytes = (embed_word)size});
    }
    const int handed = EMBED_CALL(cli_servers_run_internal.later, CliServersRunCtx, .run = panels_serve_job,
                                  .job = laid, .size = sizeof(*job) + size);
    g_cli_guard->persistent_buf_release(laid, g_cli_servers_owner);
    return handed;
}

// the view under `key` opened in its language's server: the open lined up for a thread of the servers with the view's
// text, the view marked served, and its view sent at the next step, where the view is on a file of the tree in a
// language the plugins know
static void panels_open_send(const char *key)
{
    char path[CLI_PATH_MAX_LEN];
    if ((s_panels.root == NULL) ||
        !EMBED_CALL(cli_view_path_type.path, CliViewPathTypeCfg, .key = key, .path_out = path) || (path[0] == '\0'))
    {
        return;
    }
    PanelsServeJob job = {.does = PANELS_SERVE_OPEN};
    CliLanguages languages;
    EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &languages);
    const int known = EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                 .languages_read = &languages, .path = path, .id_out = job.language);
    g_cli_guard->persistent_buf_release(languages.pair, g_cli_plugins_owner);
    if (!known)
    {
        return;
    }
    // the document has no lock of its own, so its text is read on the view thread
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = key, .size_pointer = &size,
                                  .owner = g_cli_servers_owner);
    if (text == NULL)
    {
        return;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_panels.root, .relative = path, .joined = job.full);
    job.sink = cli_servers_problems_sink(s_panels.root);
    const int lined = panels_serve_later(&job, text, size);
    g_cli_guard->persistent_buf_release(text, g_cli_servers_owner);
    if (lined && EMBED_CALL(cli_view_full.served_set, CliViewFullCfg, .key = key, .full = job.full))
    {
        s_panels.view_sent = 0;
        s_panels.view_due_ms = cli_clock_milliseconds();
    }
}

// the view under `key`, about to close, let go of its server, the server closed where no view stands served at its file
static void panels_closing(const char *key)
{
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_view_full_type.serve, CliViewFullTypeCfg, .key = key, .full_out = full))
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

// the view's text sent to the server on a thread of the servers, where the view is served and its text changed since
// the server last took it
static void panels_sync_post(const PanelsCtx *args)
{
    (void)args;
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    PanelsServeJob job = {.does = PANELS_SERVE_CHANGE};
    if ((key == NULL) || !EMBED_CALL(cli_view_full_type.serve, CliViewFullTypeCfg, .key = key, .full_out = job.full))
    {
        return;
    }
    if (!EMBED_CALL(cli_view_table.changed_alloc, CliViewCfg, .key = key))
    {
        return;
    }
    s_panels.changed = 0;
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = key, .size_pointer = &size,
                                  .owner = g_cli_servers_owner);
    if (text != NULL)
    {
        (void)panels_serve_later(&job, text, size);
        g_cli_guard->persistent_buf_release(text, g_cli_servers_owner);
    }
}

// where the view of the tab shown rests sent to the server on a thread of the servers, the view opened in its server
// first where it is not served yet
EMBED_INLINE void panels_view_send(const PanelsCtx *args)
{
    (void)args;
    s_panels.view_due_ms = 0ULL;
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    if (key == NULL)
    {
        return;
    }
    PanelsServeJob job = {.does = PANELS_SERVE_VIEW};
    if (!EMBED_CALL(cli_view_full_type.serve, CliViewFullTypeCfg, .key = key, .full_out = job.full))
    {
        // a view is opened in its server once, the first time it rests
        if (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = s_panels.open_key, .cap = CLI_NAME_MAX_LEN}))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = s_panels.open_key,
                .src = key,
                .bytes = (embed_word)cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_NAME_MAX_LEN - 1u}) + 1u});
            panels_open_send(key);
        }
        return;
    }
    const unsigned long long first = s_panels.view_top;
    const unsigned int rows = s_panels.rows;
    unsigned long long last = first + ((rows > 0u) ? (rows - 1u) : 0u);
    last = ((s_panels.view_lines > 0ULL) && (last >= s_panels.view_lines)) ? (s_panels.view_lines - 1ULL) : last;
    // a line of a text a view shows is below 2^32
    job.first = (unsigned int)first;
    job.last = (unsigned int)last;
    job.openers = EMBED_CALL(cli_document_key_line.openers, CliDocumentKeyLineCfg, .key = key, .line = job.first,
                             .opener = job.opener, .max_u32 = CLI_PANELS_OPENERS);
    if (panels_serve_later(&job, NULL, 0ULL))
    {
        s_panels.view_sent = 1;
        s_panels.view_rows = rows;
    }
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
    if (!rested)
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
    if (s_panels.signature_shown && !s_panels.signature_asking && moved)
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

// the views' moments the panels take: each frame laid and each view about to close. A view is opened in its server as
// it first rests, not as it opens
static const CliViewWatcher s_panels_watcher = {panels_drawn, NULL, panels_closing};

void cli_servers_panels_begin(const char *root)
{
    s_panels.root = root;
    // the list's cell taken once at its most
    if (s_panels.completion == NULL)
    {
        s_panels.completion = g_cli_guard->movable_buf_alloc(sizeof(PanelsCompletion), g_cli_servers_owner);
    }
    cli_view_watch(&s_panels_watcher);
}

void cli_servers_panels_end(void)
{
    cli_view_watch(NULL);
    cli_servers_panels_hide();
    (void)g_cli_guard->movable_buf_release(s_panels.completion, g_cli_servers_owner);
    s_panels.completion = NULL;
    s_panels.root = NULL;
}

int cli_servers_panels_hover(CliHoverPanel *panel)
{
    if (!s_panels.hover_shown)
    {
        return 0;
    }
    *panel = (CliHoverPanel){s_panels.hover_text, s_panels.hover_length, s_panels.hover_line, s_panels.hover_from,
                             s_panels.hover_to};
    return 1;
}

int cli_servers_panels_completion(CliCompletionPanel *panel)
{
    // the prefix copied into the panels' own bytes, which the caller reads before the next read
    static char s_prefix[CLI_PANELS_WORD];
    PanelsListHead head = {0};
    const int opened = s_panels.completion_open && (s_panels.completion != NULL);
    if (opened)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_panels.completion, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    }
    if (!opened || (head.count == 0u))
    {
        return 0;
    }
    const unsigned int rows =
        ((head.count - head.first) < CLI_COMPLETION_ROWS) ? (head.count - head.first) : CLI_COMPLETION_ROWS;
    memor.cpy(&(MemoriaCfg){.dst = s_prefix, .src = head.prefix, .bytes = head.prefix_length + 1ULL});
    *panel = (CliCompletionPanel){head.line, head.from, s_prefix, head.count, head.first, rows, head.chosen};
    return 1;
}

int cli_servers_panels_completion_row(unsigned int row, CliCompletionRow *out)
{
    // a row's texts copied out of the cell into the rows' own bytes, one set a row shown, which the caller reads
    // before the list changes
    static char s_label[CLI_COMPLETION_ROWS][CLI_PANELS_LABEL_MAX];
    static char s_type[CLI_COMPLETION_ROWS][CLI_PANELS_TYPE_MAX];
    static char s_detail[CLI_COMPLETION_ROWS][CLI_PANELS_DETAIL_MAX];
    PanelsListHead head = {0};
    const int opened = s_panels.completion_open && (s_panels.completion != NULL);
    if (opened)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_panels.completion, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    }
    if ((row >= CLI_COMPLETION_ROWS) || !opened || ((head.first + row) >= head.count))
    {
        return 0;
    }
    PanelsItem item;
    {
        unsigned int ranked_at = 0u;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_panels.completion,
            .offset = offsetof(PanelsCompletion, ranked) + ((unsigned long long)(head.first + row) * sizeof(ranked_at)),
            .dst = &ranked_at,
            .bytes = sizeof(ranked_at)});
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_panels.completion,
            .offset = offsetof(PanelsCompletion, item) + ((unsigned long long)ranked_at * sizeof(PanelsItem)),
            .dst = &item,
            .bytes = sizeof(PanelsItem)});
    }
    if (item.label_length != 0u)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_panels.completion,
                                                                .offset = offsetof(PanelsCompletion, text) + item.label,
                                                                .dst = s_label[row],
                                                                .bytes = item.label_length});
    }
    s_label[row][item.label_length] = '\0';
    if (item.type_length != 0u)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_panels.completion,
                                                                .offset = offsetof(PanelsCompletion, text) + item.type,
                                                                .dst = s_type[row],
                                                                .bytes = item.type_length});
    }
    s_type[row][item.type_length] = '\0';
    if (item.detail_length != 0u)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_panels.completion,
                                .offset = offsetof(PanelsCompletion, text) + item.detail,
                                .dst = s_detail[row],
                                .bytes = item.detail_length});
    }
    s_detail[row][item.detail_length] = '\0';
    // the prefix marks a label it starts, case aside; a prefix is far inside what an unsigned int represents
    *out =
        (CliCompletionRow){s_label[row],
                           item.label_length,
                           s_type[row],
                           s_detail[row],
                           item.detail_length,
                           ((head.prefix_length > 0ULL) && (item.score >= 4u)) ? (unsigned int)head.prefix_length : 0u};
    return 1;
}

const CliSignaturePanel *cli_servers_panels_signature(void)
{
    return s_panels.signature_shown ? &s_panels.signature.panel : NULL;
}
