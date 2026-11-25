// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_panels.c: the panels over the editor and the language server of the view displayed, on the window's thread.
// The hover rests on the pointer's cell, the tree answering first and the server after; the completion ranks the file's
// words at once and the server's items after; the signature waits on a bracket or a comma, the tree's first and the
// server's over it. Where each panel is on the screen is the window's: these carry what each panel shows
#include "../text/sort.h"
#include "servers.h"

#include "../code/references.h"
#include "../code/symbols.h"
#include "../command_line/pool.h"
#include "../command_line/rings.h"
#include "../editor/view.h"
#include "../plugins/plugins.h"
#include "../text/text.h"
#include "servers_internal.h"

// how long the pointer rests before the panels ask for a hover, and stays off it before the hover goes. How long the
// signature waits after a bracket or a comma; and how long the view rests before the panels tell its server where
#define CLI_PANELS_HOVER_REST_MS 380ULL
#define CLI_PANELS_HOVER_LINGER_MS 300ULL
#define CLI_PANELS_SIGNATURE_WAIT_MS 120ULL
#define CLI_PANELS_VIEW_REST_MS 250ULL
// While a worker reads the hover the panels ask for, the view thread comes back each CLI_PANELS_HOVER_TAKE_MS to take
// the answer. It stops when it takes the answer, or CLI_PANELS_HOVER_ANSWER_MS after the ask
#define CLI_PANELS_HOVER_TAKE_MS 8ULL
#define CLI_PANELS_HOVER_ANSWER_MS 1000ULL
// how long Go to Definition waits on the tree's first reading before it answers that it knows no declaration
#define CLI_PANELS_DEFINITION_WAIT_MS 10000ULL
// the max declarations the panels ask the tree for, and the max of them a hover shows. The bytes a word takes. The
// lines above the cursor that a signature reads, and the bytes of the state they start in. The regions around a view's
// first line that the panels tell the server of
#define CLI_PANELS_DECLARATIONS 200u
#define CLI_PANELS_HOVER_DECLARATIONS 6u
#define CLI_PANELS_WORD 256u
#define CLI_PANELS_SIGNATURE_LINES 4000ULL
#define CLI_PANELS_STATE 1024u
#define CLI_PANELS_OPENERS 64u
// what sets the parts of a hover apart
#define CLI_PANELS_HOVER_BETWEEN "\n---\n"

// the max items of the completion's list, and the bytes of its texts. The max bytes the list keeps of an item's label,
// type, detail and insert, each with its end. The max bytes of the hover's Markdown with its end
#define CLI_PANELS_ITEMS_MAX 2048u
#define CLI_PANELS_TEXT_MAX (256u << 10u)
#define CLI_PANELS_LABEL_MAX 256u
#define CLI_PANELS_TYPE_MAX 64u
#define CLI_PANELS_DETAIL_MAX 1024u
#define CLI_PANELS_INSERT_MAX 4096u
#define CLI_PANELS_HOVER_MAX (16u << 10u)
// the max bytes of a declaration's own line as the tree's search writes it, with its end
#define CLI_PANELS_LINE_MAX 1024u

// One completion item: the places in the list's text of its label and of the label in small characters. Then its type,
// what it states of itself and what it inserts. Then the bytes of each but the lowered label, and its score for the
// prefix
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

// The head of the completion's list: the items, and the items the prefix answers. The row chosen and the first row
// displayed. The prefix, as typed and in small characters, with its bytes and its place. The bytes of the list's text
// in use
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

// The completion's list, in a movable cell the panels take once at its max as they begin. It has its head and the
// items. It has the items the prefix answers, in rank order, and the text of every item's bytes with their ends. The
// panels take two such cells: the list displayed, and the cell for the build of a new list. The two change places as
// the built list opens. Every read and write goes through a cell's token at the offset of what it reaches
typedef struct
{
    PanelsListHead head;
    PanelsItem item[CLI_PANELS_ITEMS_MAX];
    unsigned int ranked[CLI_PANELS_ITEMS_MAX];
    char text[CLI_PANELS_TEXT_MAX];
} PanelsCompletion;

// The panels: the pointer's cell over the editor's text. The time of the hover's ask and the time it goes, 0 for
// neither. The ask the answer is for. The hover's Markdown in the panels' own bytes, whether it shows, and the word's
// place. The list's cell, and whether a list is open in it. Whether the next frame opens a list as it places the
// cursor, and whether a key forced it open.
//
// The signature's store and whether it shows. The time of its ask, the place of the ask, and whether an ask runs. The
// cursor, and whether the text changed since the server last took it. The time the panels tell the server where the
// view rests, whether they told it, and the view's top, rows and lines then. The time of the next read of the changed
// text.
//
// The key of the view last opened in its server, and the key of the view last sent. The tree, and the editor's
// rows
typedef struct
{
    int pointer_in_text;
    unsigned long long pointer_row;
    unsigned long long pointer_column;
    unsigned long long hover_due_ms;
    unsigned long long hover_hide_ms;
    // when the view thread next takes the hover's answer, 0 where no answer comes, and the time of the hover's ask
    unsigned long long hover_take_ms;
    unsigned long long hover_asked_ms;
    unsigned long long hover_ask;
    char hover_text[CLI_PANELS_HOVER_MAX];
    int hover_displayed;
    unsigned long long hover_length;
    unsigned long long hover_line;
    unsigned long long hover_from;
    unsigned long long hover_to;
    const CarcerMovableEntry *completion;
    const CarcerMovableEntry *completion_built;
    int completion_open;
    int completion_wanted;
    int completion_forced;
    CliSignatureStore signature;
    int signature_displayed;
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
    char sent_key[CLI_NAME_MAX_LEN];
    const char *root;
    unsigned int rows;
} Panels;

static Panels s_panels;

// A hover ask: the ask it answers, the word and the tree, and the word's place. The asker's line for the Markdown, NULL
// where the asker wants none, and the bytes the call writes. Whether the tree's walk still ran as the ask found no
// declaration. The view's key and its file's tree path, whose language the answer keeps to. The first declaration the
// ask finds, where Go to Definition goes: its file, its line and the word's column on that line. Whether the ask found
// one
typedef struct
{
    unsigned long long ask;
    const char *word_text;
    const char *root;
    unsigned long long line;
    unsigned long long from;
    unsigned long long to;
    VerbaLinea *out;
    unsigned long long length;
    int reading;
    const char *key;
    char path[CLI_PATH_MAX_LEN];
    char defined[CLI_PATH_MAX_LEN];
    unsigned long long defined_line;
    unsigned long long defined_col;
    int defined_found;
} PanelsHoverAsk;

// A hover ask on its way to a worker: the ask it answers, the word, and the view's key. Then the file's tree path and
// the word's place in the text.
typedef struct
{
    unsigned long long ask;
    char word[CLI_PANELS_WORD];
    char key[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    unsigned long long line;
    unsigned long long from;
    unsigned long long to;
} PanelsHoverJob;

// A hover answer for the view thread: the ask it answers, the word's place, and the bytes of Markdown after this head.
// Then whether the tree's walk still ran as the ask found no declaration.
typedef struct
{
    unsigned long long ask;
    unsigned long long line;
    unsigned long long from;
    unsigned long long to;
    unsigned long long length;
    int reading;
} PanelsHoverLanded;

// The hover answers on their way from the workers to the view thread: a ticket ring of movable cells, each an answer.
// The workers take tickets and push, and the view thread pops in order. The ring's turnstiles, and its entries in the
// hover's part of the rings block. On the view thread only: the next ticket to pop. A worker that finds no free entry
// gives its answer back, and nothing waits.
static EmbedTurnstiles s_hover_turnstiles;
static EmbedTicketRing s_hover_ring;
static embed_atomic_u32 s_hover_ticket;
static embed_u32 s_hover_taken;
static int s_hover_ring_positioned;

// On the view thread: the hover answers' ring put over its part of the rings block, once. That comes before the first
// take or the first ask, with no answer on it.
static void panels_hover_ring_positioned(void)
{
    if (s_hover_ring_positioned)
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = &s_hover_turnstiles, .val = 0, .bytes = sizeof(s_hover_turnstiles)});
    EmbedTicketRingSlot *const slots =
        (EmbedTicketRingSlot *)(void *)EMBED_CALL(cli_rings.part, CliRingsPartCfg, .part = CLI_RINGS_HOVER_ANSWERS);
    memor.set(&(MemoriaCfg){.dst = slots, .val = 0, .bytes = CLI_PANELS_HOVER_RING_BYTES});
    EMBED_CALL(embed_ticket_ring_begin, EmbedTicketRingBeginCfg, &s_hover_ring, slots, CLI_PANELS_HOVER_RING_SHIFT);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_hover_ticket, .value = 0u);
    s_hover_taken = 0u;
    s_hover_ring_positioned = 1;
}

// the max lines above the pointer's a parameter or a local is sought in, and the lines read at once
#define CLI_PANELS_LOCAL_LINES 512ULL
#define CLI_PANELS_LOCAL_CHUNK 64ULL

// A signature ask: the view's key and the file's tree path, the cursor's place, and whether the ask wrote a signature
// into the panels' store
typedef struct
{
    const char *key;
    const char *path;
    CliPosition cursor;
    int found;
} PanelsSignatureAsk;

typedef struct PanelsServeJob PanelsServeJob;

typedef struct
{
    const char *key;
    const char *text;
    unsigned long long length;
    const char *lowered;
    const char *prefix;
    const char *prefix_lowered;
    const PanelsItem *item;
    const CarcerMovableEntry *list;
    const char *type;
    const char *detail;
    const char *insert;
    unsigned long long insert_length;
    VerbaLinea *out;
    CliServerItems *server;
    unsigned long long line;
    unsigned long long from;
    int on_server;
    unsigned int at;
    const CliSignaturePanel *own;
    const CliSignaturePanel *panel;
    PanelsHoverAsk *hover_ask;
    unsigned long long size;
    const char *word;
    const char *path;
    PanelsListHead *head;
    unsigned long long max;
    unsigned int *length_out;
    const PanelsServeJob *job;
} PanelsCtx;

// the column, in UTF-16 units, where `word` first stands in `size` bytes of `text`, 0 where it does not
EMBED_INLINE unsigned long long panels_word_col(const PanelsCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->word, .cap = CLI_PANELS_WORD});
    for (unsigned long long at = 0ULL; (length != 0ULL) && (at + length <= args->size); at += 1ULL)
    {
        if (memor.cmp(&(MemoriaCfg){.src = args->text + at, .other = args->word, .bytes = (embed_word)length}) == 0)
        {
            return EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = args->text, .size = at);
        }
    }
    return 0ULL;
}

// the first declaration found kept as the one Go to Definition goes to: its file, its line and the word's column on
// the line `text`, `size` bytes
EMBED_INLINE void panels_defined(const PanelsCtx *args)
{
    if (args->hover_ask->defined_found)
    {
        return;
    }
    VerbaLinea defined = {args->hover_ask->defined, sizeof(args->hover_ask->defined), 0ULL};
    mmgr_line_put(&defined, args->path);
    args->hover_ask->defined_found = mmgr_line_finish(&defined) != 0ULL;
    args->hover_ask->defined_line = args->line;
    args->hover_ask->defined_col = EMBED_CALL(panels_word_col, PanelsCtx, .text = args->text, .size = args->size,
                                              .word = args->hover_ask->word_text);
}

// the key of the view displayed, else NULL
EMBED_INLINE const char *panels_viewed_key(const PanelsCtx *args)
{
    (void)args;
    const int displayed = cli_view_displayed();
    // a view's place is not negative past the test; the call copies the key into the panels' own buffer
    static char s_viewed_key[CLI_NAME_MAX_LEN];
    return ((displayed >= 0) && cli_view_key(&(CliViewAtCfg){.at = (unsigned int)displayed, .key_out = s_viewed_key}))
               ? s_viewed_key
               : NULL;
}

// a byte a word holds
EMBED_INLINE int panels_word_byte(const PanelsCtx *args)
{
    const unsigned char value = (unsigned char)args->text[0];
    return ((value >= 'a') && (value <= 'z')) || ((value >= 'A') && (value <= 'Z')) ||
           ((value >= '0') && (value <= '9')) || (value == '_') || (value == '$') || (value >= 0x80u);
}

// the hover gone, and its timer to go with it
EMBED_INLINE void panels_hover_hide(const PanelsCtx *args)
{
    (void)args;
    s_panels.hover_displayed = 0;
    s_panels.hover_length = 0ULL;
    s_panels.hover_hide_ms = 0ULL;
    s_panels.hover_take_ms = 0ULL;
}

// the list closed, and nothing forced open; its cell is for the next
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
    s_panels.signature_displayed = 0;
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
}

// whether the pointer is on the hover's word, a unit either side
EMBED_INLINE int panels_pointer_on_word(const PanelsCtx *args)
{
    (void)args;
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    unsigned long long line = 0ULL;
    unsigned long long col = 0ULL;
    return s_panels.pointer_in_text && s_panels.hover_displayed && (key != NULL) &&
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
    if (!s_panels.hover_displayed)
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

// a declaration of the hover's word written: its own line `text` as code, then its `type` and place, `path` and `line`
EMBED_INLINE void panels_declaration_write(const PanelsCtx *args)
{
    if (args->text[0] != '\0')
    {
        mmgr_line_put(args->out, "```\n");
        mmgr_line_put(args->out, args->text);
        mmgr_line_put(args->out, "\n```\n\n");
    }
    mmgr_line_put(args->out, args->type);
    mmgr_line_put(args->out, " `");
    mmgr_line_put(args->out, args->path);
    mmgr_line_ch(args->out, ':');
    mmgr_line_decimal(args->out, args->line + 1ULL, 1u);
    mmgr_line_ch(args->out, '`');
}

// The hover's word as a parameter or a local of the function the pointer is in, off the runs the walk saved. The call
// reads the lines above the pointer's upward, to the line the grammar marks identifier.declaration.function. It reads
// that line's own parameters too. The topmost run named identifier.parameter or identifier.local with the word as its
// text is the declaration. The walk gives every later use of a declared name the same name.
//
// The call writes the declaration's line as the tree writes its lines, with its type and place. 1, or 0 where it finds
// none
EMBED_INLINE int panels_hover_local_run(const PanelsCtx *args)
{
    PanelsHoverAsk *const ask = args->hover_ask;
    if (EMBED_CALL(cli_document_names.count, CliDocumentNamesCfg, .key = ask->key) == 0u)
    {
        return 0;
    }
    const unsigned int parameter =
        EMBED_CALL(cli_document_names.place, CliDocumentNamesCfg, .key = ask->key, .text = "identifier.parameter");
    const unsigned int local =
        EMBED_CALL(cli_document_names.place, CliDocumentNamesCfg, .key = ask->key, .text = "identifier.local");
    const unsigned int head = EMBED_CALL(cli_document_names.place, CliDocumentNamesCfg, .key = ask->key,
                                         .text = "identifier.declaration.function");
    const unsigned long long low = (ask->line > CLI_PANELS_LOCAL_LINES) ? (ask->line - CLI_PANELS_LOCAL_LINES) : 0ULL;
    unsigned long long end = ask->line + 1ULL;
    unsigned long long found_line = 0ULL;
    const char *type = NULL;
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
        if (!EMBED_CALL(cli_document_runs.runs_walk, CliDocumentRunsCfg, .key = ask->key, .number = number,
                        .count = count, .mirrored = 0))
        {
            break;
        }
        for (unsigned long long at = count; (at > 0ULL) && !ended; at -= 1ULL)
        {
            const unsigned long long line = first + at - 1ULL;
            const unsigned int runs = EMBED_CALL(cli_document_line_run_table.count, CliDocumentLineRunCfg,
                                                 .key = ask->key, .line = line, .mirrored = 0);
            unsigned long long units_count = 0ULL;
            (void)EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = ask->key, .line = line,
                             .count = &units_count);
            for (unsigned int run = 0u; run < runs; run += 1u)
            {
                unsigned int name = 0u;
                unsigned int bounds[2] = {0u, 0u};
                (void)EMBED_CALL(cli_document_line_run_table.read, CliDocumentLineRunCfg, .key = ask->key, .line = line,
                                 .mirrored = 0, .run = run, .column = &bounds[0], .name = &name);
                ended = ended || ((name == head) && (line < ask->line));
                const int is_parameter = name == parameter;
                if (!is_parameter && (name != local))
                {
                    continue;
                }
                if ((run + 1u) < runs)
                {
                    (void)EMBED_CALL(cli_document_line_run_table.read, CliDocumentLineRunCfg, .key = ask->key,
                                     .line = line, .mirrored = 0, .run = run + 1u, .column = &bounds[1]);
                }
                const unsigned long long from = bounds[0];
                const unsigned long long to = ((run + 1u) < runs) ? bounds[1] : units_count;
                // a run longer than the word's bytes is not the word, as each unit is a byte of it at the least
                if ((to > units_count) || (from >= to) || ((to - from) >= CLI_PANELS_WORD))
                {
                    continue;
                }
                // the run's units read into this call's own array, from the run's first unit on
                unsigned short units[CLI_PANELS_WORD];
                unsigned long long whole = 0ULL;
                (void)EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = ask->key, .line = line,
                                 .count = &whole, .dst = units, .capacity = to - from, .from = from);
                char text[CLI_PANELS_WORD];
                VerbaLinea out = {text, sizeof(text), 0ULL};
                EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &out, .units = units,
                           .count = to - from);
                if ((mmgr_line_finish(&out) != 0ULL) &&
                    cellul.eq(&(CatenaFinitaCfg){.src = text, .other = ask->word_text, .cap = sizeof(text)}))
                {
                    type = is_parameter ? "parameter" : "local";
                    found_line = line;
                }
            }
        }
        end = first;
    }
    if (type == NULL)
    {
        return 0;
    }
    char line_text[CLI_PANELS_LINE_MAX];
    VerbaLinea read = {line_text, sizeof(line_text), 0ULL};
    if (!EMBED_CALL(cli_document_key.lines_read, CliDocumentKeyCfg, .key = ask->key, .first = found_line,
                    .last = found_line, .state = NULL, .capacity = 0ULL, .out = &read))
    {
        return 0;
    }
    const unsigned long long size = read.at;
    const unsigned long long start = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                .text = line_text, .at = 0ULL, .size = size, .set = " \t");
    EMBED_CALL(panels_defined, PanelsCtx, .hover_ask = ask, .path = ask->path, .line = found_line, .text = line_text,
               .size = size);
    // the Markdown written into the asker's line, where it gives one; a line it does not hold goes back to as it was
    VerbaLinea *const out = ask->out;
    const size_t begun = (out != NULL) ? out->at : 0u;
    if (out != NULL)
    {
        mmgr_line_put(out, "```\n");
        mmgr_line_put_n(out, line_text + start, size - start);
        mmgr_line_put(out, "\n```\n\n");
        EMBED_CALL(panels_declaration_write, PanelsCtx, .text = "", .type = type, .path = ask->path, .line = found_line,
                   .out = out);
    }
    if ((out != NULL) && !mmgr_line_ok(out))
    {
        out->at = begun;
        return 0;
    }
    ask->length = (out != NULL) ? (out->at - begun) : 0ULL;
    return 1;
}

// A hover's read of the tree's declarations: the ask, the hovered file's language, and whether the tree knows it. Then
// the line the Markdown goes after, and how many parts it wrote
typedef struct
{
    PanelsHoverAsk *ask;
    char wanted[CLI_NAME_MAX_LEN];
    int known;
    VerbaLinea *out;
    unsigned int parts;
} PanelsHoverFound;

// one declaration the tree finds, where its name is the word and its file opens in the hovered file's language. The
// call keeps its line as the definition and writes it into the asker's line as Markdown
static void panels_hover_found(const CliEachFoundCfg *args)
{
    PanelsHoverFound *const found = (PanelsHoverFound *)args->context;
    PanelsHoverAsk *const ask = found->ask;
    if (found->parts >= CLI_PANELS_HOVER_DECLARATIONS)
    {
        return;
    }
    char id[CLI_NAME_MAX_LEN];
    const int same =
        !found->known || (EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                     .path = args->path, .id_out = id) &&
                          cellul.eq(&(CatenaFinitaCfg){.src = id, .other = found->wanted, .cap = sizeof(id)}));
    if (!same || (args->name_length >= CLI_PANELS_WORD) ||
        !cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = ask->word_text, .cap = CLI_PANELS_WORD}))
    {
        return;
    }
    // the declaration's own line cut to the hover's max
    char text[CLI_PANELS_LINE_MAX];
    const unsigned long long text_length =
        (args->text_length < (sizeof(text) - 1ULL)) ? args->text_length : (sizeof(text) - 1ULL);
    memor.cpy(&(MemoriaCfg){.dst = text, .src = args->text, .bytes = text_length});
    text[text_length] = '\0';
    EMBED_CALL(panels_defined, PanelsCtx, .hover_ask = ask, .path = args->path, .line = args->line, .text = text,
               .size = text_length);
    if (found->out != NULL)
    {
        mmgr_line_put(found->out, (found->parts > 0u) ? CLI_PANELS_HOVER_BETWEEN : "");
        EMBED_CALL(panels_declaration_write, PanelsCtx, .text = text, .type = args->type, .path = args->path,
                   .line = args->line, .out = found->out);
    }
    found->parts += 1u;
}

// the tree's declarations of the word as Markdown in a cell sized to them. Only files that open in the hovered file's
// language plugin give declarations
static void panels_hover_tree_run(void *context)
{
    PanelsHoverAsk *const ask = (PanelsHoverAsk *)context;
    if (EMBED_CALL(panels_hover_local_run, PanelsCtx, .hover_ask = ask))
    {
        return;
    }
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = ask->root);
    PanelsHoverFound found = {ask, {0}, 0, ask->out, 0u};
    found.known = EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg, .path = ask->path,
                             .id_out = found.wanted);
    // the Markdown written into the asker's line, where it gives one
    VerbaLinea *const out = ask->out;
    const size_t begun = (out != NULL) ? out->at : 0u;
    ask->reading =
        EMBED_CALL(cli_symbols_query_max.tree_declarations_each, CliSymbolsQueryMaxCfg, .query = ask->word_text,
                   .max = CLI_PANELS_DECLARATIONS, .each = panels_hover_found, .context = &found);
    const unsigned int parts = found.parts;
    if (out == NULL)
    {
        return;
    }
    if ((parts == 0u) || !mmgr_line_ok(out))
    {
        out->at = begun;
    }
    ask->length = out->at - begun;
}

// A worker reads the hover's declarations from the tree. The answer goes into a movable cell of its own. The worker
// pushes the cell's entry on the answers' ring under a ticket of its own. A cell without a free ring entry waits for
// one. The view thread takes it as it next steps.
static void panels_hover_job(const CliServersJobCfg *args)
{
    if (args->size < sizeof(PanelsHoverJob))
    {
        return;
    }
    // the call reads the job's bytes where the jobs ring hands them, for as long as it runs
    const PanelsHoverJob *const job = (const PanelsHoverJob *)args->job;
    // the Markdown goes into this call's own array, no larger than the hover the view thread shows
    char text[CLI_PANELS_HOVER_MAX];
    VerbaLinea written = mmgr_line_over(text, sizeof(text));
    PanelsHoverAsk asked = {job->ask, job->word, s_panels.root, job->line, job->from, job->to, &written,
                            0ULL,     0,         job->key,      ""};
    VerbaLinea path = {asked.path, sizeof(asked.path), 0ULL};
    mmgr_line_put(&path, job->path);
    (void)mmgr_line_finish(&path);
    panels_hover_tree_run(&asked);
    const PanelsHoverLanded head = {job->ask, job->line, job->from, job->to, asked.length, asked.reading};
    const CarcerMovableEntry *const landed =
        g_cli_guard->movable_buf_alloc(sizeof(head) + asked.length, g_cli_servers_owner);
    if (landed != NULL)
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = landed, .src = &head, .bytes = sizeof(head)});
        if (asked.length != 0ULL)
        {
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = landed, .offset = sizeof(head), .src = text, .bytes = asked.length});
        }
    }
    if (landed == NULL)
    {
        return;
    }
    const embed_u32 ticket = EMBED_ATOMIC_ADD_32(&s_hover_ticket, 1u);
    const embed_u64 stamp = EMBED_CALL(embed_ticket_stamp, EmbedTicketStampCfg, cli_ticket_epoch(), ticket);
    // Explicit cast carries the cell's address as the pass's entry
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)landed;
    embed_word seen = 0u;
    // a ring with no free entry has answers the view thread did not take yet: this answer goes back, and the next ask
    // reads anew
    if (!cli_ticket_room(&s_hover_ring, ticket) ||
        !EMBED_CALL(embed_ticket_ring_pass_grant, EmbedTicketRingPassGrantCfg, &s_hover_turnstiles, address,
                    &s_hover_ring, stamp, &seen))
    {
        (void)g_cli_guard->movable_buf_release(landed, g_cli_servers_owner);
        return;
    }
    // Explicit cast carries the entry, read only, as the ring's job
    (void)EMBED_CALL(embed_ticket_ring_pass_push, EmbedTicketRingPassPushCfg, &s_hover_turnstiles, address,
                     &s_hover_ring, stamp, (void *)(embed_word)landed, &seen);
}

// On the view thread: the call pops every answer on the ring in the order of its ticket. An answer to the ask that
// still runs is the hover. The call copies its Markdown into the hover's own bytes and cuts it to them. The call drops
// an answer to an ask the pointer left. For a word the tree's walk did not reach yet, it asks again after the rest.
//
// It does that while the pointer stays on the word. Each cell goes back, and its ring entry comes free.
EMBED_INLINE void panels_hover_answers_taken(const PanelsCtx *args)
{
    (void)args;
    panels_hover_ring_positioned();
    // Explicit cast carries the ring's address as the pass's entry
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)&s_hover_ring;
    embed_word stamp = 0u;
    while (cli_ticket_pushed(&s_hover_ring, s_hover_taken, &stamp))
    {
        void *job = NULL;
        embed_u64 seen = 0u;
        if (!EMBED_CALL(embed_ticket_ring_pass_pop, EmbedTicketRingPassPopCfg, &s_hover_turnstiles, address,
                        &s_hover_ring, stamp, &job, &seen))
        {
            break;
        }
        const CarcerMovableEntry *const landed = job;
        PanelsHoverLanded head;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = landed, .dst = &head, .bytes = sizeof(head)});
        if (head.ask == s_panels.hover_ask)
        {
            // the hide also stops the takes: the answer asked for is here
            EMBED_CALL(panels_hover_hide, PanelsCtx, .at = 0u);
            const unsigned long long retained =
                (head.length < CLI_PANELS_HOVER_MAX) ? head.length : CLI_PANELS_HOVER_MAX - 1ULL;
            if (retained != 0ULL)
            {
                (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                    .cell = landed, .offset = sizeof(head), .dst = s_panels.hover_text, .bytes = retained});
                s_panels.hover_text[retained] = '\0';
                s_panels.hover_length = retained;
                s_panels.hover_displayed = 1;
            }
            s_panels.hover_line = head.line;
            s_panels.hover_from = head.from;
            s_panels.hover_to = head.to;
            if ((retained == 0ULL) && head.reading)
            {
                s_panels.hover_due_ms = cli_clock_milliseconds() + CLI_PANELS_HOVER_REST_MS;
            }
        }
        (void)g_cli_guard->movable_buf_release(landed, g_cli_servers_owner);
        (void)EMBED_CALL(embed_ticket_ring_drain_release, EmbedTicketRingDrainReleaseCfg, &s_hover_turnstiles,
                         &s_hover_ring, stamp, &seen);
        s_hover_taken += 1u;
    }
}

int cli_references_definition(const CliReferencesPlaceCfg *args)
{
    if ((args->root == NULL) || (args->name == NULL) || (args->name[0] == '\0'))
    {
        return 0;
    }
    PanelsHoverAsk asked = {0ULL, args->name, args->root, args->line, 0ULL, 0ULL, NULL, 0ULL, 0, args->key, ""};
    VerbaLinea path = {asked.path, sizeof(asked.path), 0ULL};
    mmgr_line_put(&path, args->path);
    (void)mmgr_line_finish(&path);
    panels_hover_tree_run(&asked);
    // the tree's first reading started by the ask: waited for, no longer than CLI_PANELS_DEFINITION_WAIT_MS, and the
    // declarations asked for again once it read them
    if (!asked.defined_found && asked.reading)
    {
        CliWake pause = CLI_WAKE_INIT;
        const unsigned long long last_ms = cli_clock_milliseconds() + CLI_PANELS_DEFINITION_WAIT_MS;
        while (cli_tree_unwalked() && (cli_clock_milliseconds() < last_ms))
        {
            (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 20ULL);
        }
        asked.reading = 0;
        panels_hover_tree_run(&asked);
    }
    if (!asked.defined_found)
    {
        return 0;
    }
    VerbaLinea out = {args->path_out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&out, asked.defined);
    (void)mmgr_line_finish(&out);
    *args->line_out = asked.defined_line;
    *args->col_out = asked.defined_col;
    return 1;
}

// The hover asked for at the pointer's cell. A pointer at rest on the word displayed does not ask again. A place with
// no word lets the hover displayed go, unless the pointer is on it. The tree's read of the word goes to a worker, and
// the view thread goes on.
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
    PanelsHoverJob job = {s_panels.hover_ask, "", "", "", line, from, to};
    VerbaLinea word = {job.word, sizeof(job.word), 0ULL};
    mmgr_line_put(&word, word_text);
    (void)mmgr_line_finish(&word);
    VerbaLinea viewed = {job.key, sizeof(job.key), 0ULL};
    mmgr_line_put(&viewed, key);
    (void)mmgr_line_finish(&viewed);
    if (!EMBED_CALL(cli_view_path_type.path, CliViewPathTypeCfg, .key = key, .path_out = job.path))
    {
        job.path[0] = '\0';
    }
    // The view thread comes back on its own deadline to take the answer. The worker pushes it on the ring and wakes
    // nothing
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (EMBED_CALL(cli_servers_run_internal.later, CliServersRunCtx, .run = panels_hover_job, .job = &job,
                   .size = sizeof(job)))
    {
        s_panels.hover_asked_ms = now_ms;
        s_panels.hover_take_ms = now_ms + CLI_PANELS_HOVER_TAKE_MS;
    }
}

// the item's score for the prefix. 5 where its label starts with it, and 4 where it starts with it in any case. 3 where
// it has it inside in any case. 2 where it has its characters in order in any case. 1 for no prefix, 0 for no answer.
// The caller reads the item's label into `text` and its lowered label into `lowered`
EMBED_INLINE unsigned int panels_item_score(const PanelsCtx *args)
{
    const PanelsItem *const item = args->item;
    const char *const label = args->text;
    const char *const lowered = args->lowered;
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
    for (unsigned long long character = 0ULL; character < want; character += 1ULL)
    {
        first[0] = args->prefix_lowered[character];
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

// the list's cell with the items in rank, which the order reads as it compares two
static const CarcerMovableEntry *s_ranking;

// two items in rank order: the higher score first, then the shorter label, then the labels' bytes. The order reads each
// item out of the cell at its place
static int panels_item_order(const void *one, const void *other)
{
    PanelsItem first;
    PanelsItem second;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_ranking,
        .offset = offsetof(PanelsCompletion, item) + ((unsigned long long)*(const unsigned int *)one * sizeof(first)),
        .dst = &first,
        .bytes = sizeof(first)});
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_ranking,
                            .offset = offsetof(PanelsCompletion, item) +
                                      ((unsigned long long)*(const unsigned int *)other * sizeof(second)),
                            .dst = &second,
                            .bytes = sizeof(second)});
    if (first.score != second.score)
    {
        return (first.score > second.score) ? -1 : 1;
    }
    if (first.label_length != second.label_length)
    {
        return (first.label_length < second.label_length) ? -1 : 1;
    }
    const embed_iword order = g_cli_guard->movable_buf_compare(
        &(CarcerMovableCfg){.cell = s_ranking,
                            .other_cell = s_ranking,
                            .offset = offsetof(PanelsCompletion, text) + first.label,
                            .other_offset = offsetof(PanelsCompletion, text) + second.label,
                            .bytes = first.label_length});
    if (order == 0)
    {
        return 0;
    }
    return (order < 0) ? -1 : 1;
}

// The items the prefix answers ranked in the list's cell `list`. The row chosen stays on its item where the list still
// displays that item. The call reads each item, its label and its lowered label out of the cell, and writes its score
// back. It sorts the ranked places in its own array and writes them back whole
EMBED_INLINE void panels_completion_rank(const PanelsCtx *args)
{
    const CarcerMovableEntry *const list = args->list;
    PanelsListHead head;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = list, .dst = &head, .bytes = sizeof(head)});
    unsigned int ranked[CLI_PANELS_ITEMS_MAX];
    unsigned int saved = head.items;
    if (head.count > 0u)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = list,
            .offset = offsetof(PanelsCompletion, ranked) + ((unsigned long long)head.chosen * sizeof(saved)),
            .dst = &saved,
            .bytes = sizeof(saved)});
    }
    head.count = 0u;
    for (unsigned int at = 0u; at < head.items; at += 1u)
    {
        const unsigned long long item_at =
            offsetof(PanelsCompletion, item) + ((unsigned long long)at * sizeof(PanelsItem));
        PanelsItem item;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = list, .offset = item_at, .dst = &item, .bytes = sizeof(item)});
        char label[CLI_PANELS_LABEL_MAX];
        char lowered[CLI_PANELS_LABEL_MAX];
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = list,
                                                                .offset = offsetof(PanelsCompletion, text) + item.label,
                                                                .dst = label,
                                                                .bytes = item.label_length});
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = list,
                                .offset = offsetof(PanelsCompletion, text) + item.lowered,
                                .dst = lowered,
                                .bytes = item.label_length});
        item.score = EMBED_CALL(panels_item_score, PanelsCtx, .item = &item, .text = label, .lowered = lowered,
                                .length = head.prefix_length, .prefix = head.prefix, .prefix_lowered = head.lowered);
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = list,
                                                                 .offset = item_at + offsetof(PanelsItem, score),
                                                                 .src = &item.score,
                                                                 .bytes = sizeof(item.score)});
        if (item.score > 0u)
        {
            ranked[head.count] = at;
            head.count += 1u;
        }
    }
    s_ranking = list;
    EMBED_CALL(cli_sort, CliSortCfg, .base = ranked, .count = head.count, .size = sizeof(*ranked),
               .order = panels_item_order);
    s_ranking = NULL;
    head.chosen = 0u;
    for (unsigned int at = 0u; at < head.count; at += 1u)
    {
        if (ranked[at] == saved)
        {
            head.chosen = at;
        }
    }
    if (head.count != 0u)
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = list,
                                                                 .offset = offsetof(PanelsCompletion, ranked),
                                                                 .src = ranked,
                                                                 .bytes = head.count * sizeof(*ranked)});
    }
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = list, .src = &head, .bytes = sizeof(head)});
}

// the first row displayed moved as far as it must to show the row chosen
EMBED_INLINE void panels_completion_scrolled(const PanelsCtx *args)
{
    if (args->head->chosen < args->head->first)
    {
        args->head->first = args->head->chosen;
    }
    else if (args->head->chosen >= (args->head->first + CLI_COMPLETION_ROWS))
    {
        args->head->first = args->head->chosen - CLI_COMPLETION_ROWS + 1u;
    }
    const unsigned int max = (args->head->count > CLI_COMPLETION_ROWS) ? (args->head->count - CLI_COMPLETION_ROWS) : 0u;
    args->head->first = (args->head->first > max) ? max : args->head->first;
}

// the placeholder of a tab stop, from `from` in `length` bytes of `text`, written to `out` up to the brace that closes
// the stop. Each stop inside it becomes its own placeholder, and an escaped byte goes as itself. The call gives the
// place past that brace, or the text's end
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

// the tab stop whose `$` is just before `from` in `length` bytes of `text`, written to `out` as the text it inserts.
// `$n` and `${n}` give nothing, `${n:text}` gives its placeholder, and `${n|a,b|}` gives its first choice. The call
// gives the place past it
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

// `length` bytes of a snippet `text` written as the text it inserts. A tab stop with a placeholder, `${n:text}`, gives
// its placeholder, and so do the stops inside it. A stop with choices, `${n|a,b|}`, gives its first choice. A bare
// stop,
// `$n` or `${n}`, gives nothing, and an escaped byte gives itself
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

// `length` bytes of `text` copied into the list's text after its bytes in use, cut to `max` with their end. The call
// gives the copy's place in the text, and its bytes into `length_out`. CLI_PANELS_TEXT_MAX where the text has no room
static unsigned int panels_text_put(const PanelsCtx *args)
{
    PanelsListHead *const head = args->head;
    const unsigned long long retained = (args->length < args->max) ? args->length : args->max - 1ULL;
    if (head->text_used + retained + 1ULL > CLI_PANELS_TEXT_MAX)
    {
        *args->length_out = 0u;
        return CLI_PANELS_TEXT_MAX;
    }
    // the text in use is below CLI_PANELS_TEXT_MAX, inside an unsigned int, as is a text cut to its max
    const unsigned int at = (unsigned int)head->text_used;
    const char end = '\0';
    if (retained != 0ULL)
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = args->list, .offset = offsetof(PanelsCompletion, text) + at, .src = args->text, .bytes = retained});
    }
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = args->list, .offset = offsetof(PanelsCompletion, text) + at + retained, .src = &end, .bytes = 1u});
    head->text_used += retained + 1ULL;
    *args->length_out = (unsigned int)retained;
    return at;
}

// an item put after the list's items. Its label of `length` bytes at `text`, its type, its detail and its insert of
// `insert_length` bytes go into the list's text. The call lowers the label once. 1, or 0 where the items or the text
// are full, and then the text stays as it was
EMBED_INLINE int panels_item_put(const PanelsCtx *args)
{
    const CarcerMovableEntry *const list = args->list;
    PanelsListHead *const head = args->head;
    const unsigned long long used = head->text_used;
    if (head->items == CLI_PANELS_ITEMS_MAX)
    {
        return 0;
    }
    // the label lowered in this call's own array before it goes into the cell
    const unsigned long long lowered_length =
        (args->length < CLI_PANELS_LABEL_MAX) ? args->length : (CLI_PANELS_LABEL_MAX - 1ULL);
    char lowered[CLI_PANELS_LABEL_MAX];
    if (lowered_length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = lowered, .src = args->text, .bytes = lowered_length});
    }
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .size = lowered_length, .text_out = lowered);
    PanelsItem item = {0};
    unsigned int lowered_put = 0u;
    item.label = EMBED_CALL(panels_text_put, PanelsCtx, .list = list, .head = head, .text = args->text,
                            .length = args->length, .max = CLI_PANELS_LABEL_MAX, .length_out = &item.label_length);
    item.lowered = EMBED_CALL(panels_text_put, PanelsCtx, .list = list, .head = head, .text = lowered,
                              .length = lowered_length, .max = CLI_PANELS_LABEL_MAX, .length_out = &lowered_put);
    item.type = EMBED_CALL(panels_text_put, PanelsCtx, .list = list, .head = head, .text = args->type,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = args->type, .cap = CLI_POOL_BYTES}),
                           .max = CLI_PANELS_TYPE_MAX, .length_out = &item.type_length);
    item.detail = EMBED_CALL(panels_text_put, PanelsCtx, .list = list, .head = head, .text = args->detail,
                             .length = cellul.len(&(CatenaFinitaCfg){.src = args->detail, .cap = CLI_POOL_BYTES}),
                             .max = CLI_PANELS_DETAIL_MAX, .length_out = &item.detail_length);
    item.insert =
        EMBED_CALL(panels_text_put, PanelsCtx, .list = list, .head = head, .text = args->insert,
                   .length = args->insert_length, .max = CLI_PANELS_INSERT_MAX, .length_out = &item.insert_length);
    if ((item.label == CLI_PANELS_TEXT_MAX) || (item.lowered == CLI_PANELS_TEXT_MAX) ||
        (item.type == CLI_PANELS_TEXT_MAX) || (item.detail == CLI_PANELS_TEXT_MAX) ||
        (item.insert == CLI_PANELS_TEXT_MAX))
    {
        head->text_used = used;
        return 0;
    }
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = list,
                                                             .offset = offsetof(PanelsCompletion, item) +
                                                                       ((unsigned long long)head->items * sizeof(item)),
                                                             .src = &item,
                                                             .bytes = sizeof(item)});
    head->items += 1u;
    return 1;
}

// The entries of the table of items in a list, a power of two above twice the items at max. Each entry is an item's
// place plus one, 0 for an empty entry
#define PANELS_SEEN_ENTRIES 4096u

// The words put into a completion list: the list's cell and its head in the caller's record. Then whether the cell has
// room for one more, and the table of the items in the list by the hash of their labels
typedef struct
{
    const CarcerMovableEntry *list;
    PanelsListHead *head;
    int room;
    unsigned int seen[PANELS_SEEN_ENTRIES];
} PanelsWordsPut;

_Static_assert(PANELS_SEEN_ENTRIES >= (2u * CLI_PANELS_ITEMS_MAX), "the table of items holds twice the items at max");

// the FNV-1a hash of `length` bytes of `label`
static unsigned int panels_label_hash(const char *label, unsigned long long length)
{
    unsigned int hash = 2166136261u;
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        hash = (hash ^ (unsigned char)label[at]) * 16777619u;
    }
    return hash;
}

// The entry of the table for `length` bytes of `label`: the entry that holds the item of that label, else the empty
// entry where it goes. Each entry compares its item's label through the list's cell
static unsigned int panels_seen_entry(const PanelsWordsPut *put, const char *label, unsigned long long length)
{
    unsigned int entry = panels_label_hash(label, length) & (PANELS_SEEN_ENTRIES - 1u);
    while (put->seen[entry] != 0u)
    {
        PanelsItem given;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = put->list,
            .offset = offsetof(PanelsCompletion, item) + ((unsigned long long)(put->seen[entry] - 1u) * sizeof(given)),
            .dst = &given,
            .bytes = sizeof(given)});
        if ((given.label_length == length) && (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){
                                                   .cell = put->list,
                                                   .offset = offsetof(PanelsCompletion, text) + given.label,
                                                   .src = label,
                                                   .bytes = length}) == 0))
        {
            return entry;
        }
        entry = (entry + 1u) & (PANELS_SEEN_ENTRIES - 1u);
    }
    return entry;
}

// one word of the file put after the list's items, where the cell has room and the server gave no item of that label
static void panels_word_put(const CliEachTextLengthCfg *args)
{
    PanelsWordsPut *const put = (PanelsWordsPut *)args->context;
    const char *const label = args->text;
    const unsigned long long label_length = args->length;
    const unsigned long long kept =
        (label_length < CLI_PANELS_LABEL_MAX) ? label_length : (CLI_PANELS_LABEL_MAX - 1ULL);
    // the word typed itself adds nothing to the list
    const int typed = (kept == put->head->prefix_length) &&
                      (memor.cmp(&(MemoriaCfg){.src = label, .other = put->head->prefix, .bytes = kept}) == 0);
    if (!put->room || (kept == 0ULL) || typed)
    {
        return;
    }
    // a word in the list already, from the server, the tree or the edited line, goes in once
    const unsigned int entry = panels_seen_entry(put, label, kept);
    if (put->seen[entry] != 0u)
    {
        return;
    }
    put->room = EMBED_CALL(panels_item_put, PanelsCtx, .list = put->list, .head = put->head, .text = label,
                           .length = label_length, .type = "word", .detail = "", .insert = label,
                           .insert_length = label_length);
    put->seen[entry] = put->room ? put->head->items : 0u;
}

// The items the list holds put in the table of items: each label read out of the list's cell into this call's own
// array
static void panels_seen_fill(PanelsWordsPut *put)
{
    for (unsigned int at = 0u; at < put->head->items; at += 1u)
    {
        PanelsItem given;
        char label[CLI_PANELS_LABEL_MAX];
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = put->list,
                                .offset = offsetof(PanelsCompletion, item) + ((unsigned long long)at * sizeof(given)),
                                .dst = &given,
                                .bytes = sizeof(given)});
        const unsigned long long length =
            (given.label_length < CLI_PANELS_LABEL_MAX) ? given.label_length : (CLI_PANELS_LABEL_MAX - 1ULL);
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = put->list,
                                .offset = offsetof(PanelsCompletion, text) + given.label,
                                .dst = label,
                                .bytes = length});
        const unsigned int entry = panels_seen_entry(put, label, length);
        put->seen[entry] = (put->seen[entry] == 0u) ? (at + 1u) : put->seen[entry];
    }
}

// The list put in the list's cell `list` from the server's items and the words of the view `key`. A word the server
// gave goes in once. The call lowers each label once, flattens each snippet once, and copies every text into the cell.
// The items end where the items or the text are full. The call keeps the head in its own record and writes it into the
// cell last
EMBED_INLINE void panels_completion_positioned(const PanelsCtx *args)
{
    const CarcerMovableEntry *const list = args->list;
    PanelsListHead head_record;
    PanelsListHead *const head = &head_record;
    const unsigned int from_server = (args->server != NULL) ? args->server->count : 0u;
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
        // a snippet flattened to what it inserts, in a buffer of the insert's max; one past it inserts its label
        char flat_text[CLI_PANELS_INSERT_MAX];
        VerbaLinea flat = {flat_text, sizeof(flat_text), 0ULL};
        if (one->snippet)
        {
            EMBED_CALL(panels_snippet_write, PanelsCtx, .text = insert, .length = inserted, .out = &flat);
        }
        const int flattened = one->snippet && mmgr_line_ok(&flat);
        room = EMBED_CALL(panels_item_put, PanelsCtx, .list = list, .head = head, .text = one->label,
                          .length = label_length, .type = one->type, .detail = one->detail,
                          .insert = flattened ? flat_text : (one->snippet ? one->label : insert),
                          .insert_length = flattened ? flat.at : (one->snippet ? label_length : inserted));
    }
    // The file's own words: first the names of the file's branch in the tree, as its walk or its save joined them. Then
    // the words of the line under edit. No read of the text goes past that line
    PanelsWordsPut put = {list, head, room, {0u}};
    panels_seen_fill(&put);
    char path[CLI_PATH_MAX_LEN];
    if ((args->key != NULL) &&
        EMBED_CALL(cli_view_path_type.path, CliViewPathTypeCfg, .key = args->key, .path_out = path) &&
        (path[0] != '\0'))
    {
        (void)EMBED_CALL(cli_symbols_path_each.names_each, CliSymbolsPathEachCfg, .path = path, .each = panels_word_put,
                         .context = &put);
    }
    if (args->key != NULL)
    {
        cli_view_words_each(&(CliViewWordsCfg){.key = args->key, .each = panels_word_put, .context = &put});
    }
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = list, .src = head, .bytes = sizeof(*head)});
}

// The built list's cell made the cell displayed, in place of the last, the row chosen saved on its item. The list
// closes where nothing answers. It closes where the one answer is the typed word and no key forced the list. It also
// closes where the prefix is empty and no list was open or forced open. Else the two cells change places, and the built
// list is the open list
EMBED_INLINE void panels_completion_are(const PanelsCtx *args)
{
    (void)args;
    const CarcerMovableEntry *const built = s_panels.completion_built;
    // the list that was: its first row and its row chosen's label, read out of its cell
    PanelsListHead previous = {0};
    const int was = s_panels.completion_open && (s_panels.completion != NULL);
    if (was)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_panels.completion, .offset = 0ULL, .dst = &previous, .bytes = sizeof(previous)});
    }
    char was_label[CLI_PANELS_LABEL_MAX] = "";
    unsigned int was_label_length = 0u;
    if (was && (previous.count > 0u))
    {
        PanelsItem chosen;
        {
            unsigned int ranked_at = 0u;
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = s_panels.completion,
                                    .offset = offsetof(PanelsCompletion, ranked) +
                                              ((unsigned long long)previous.chosen * sizeof(ranked_at)),
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
    EMBED_CALL(panels_completion_rank, PanelsCtx, .list = built);
    PanelsListHead head;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = built, .dst = &head, .bytes = sizeof(head)});
    if (was && (previous.count > 0u))
    {
        for (unsigned int at = 0u; at < head.count; at += 1u)
        {
            unsigned int ranked_at = 0u;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = built,
                .offset = offsetof(PanelsCompletion, ranked) + ((unsigned long long)at * sizeof(ranked_at)),
                .dst = &ranked_at,
                .bytes = sizeof(ranked_at)});
            PanelsItem item;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = built,
                .offset = offsetof(PanelsCompletion, item) + ((unsigned long long)ranked_at * sizeof(item)),
                .dst = &item,
                .bytes = sizeof(item)});
            if ((item.label_length == was_label_length) && (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){
                                                                .cell = built,
                                                                .offset = offsetof(PanelsCompletion, text) + item.label,
                                                                .src = was_label,
                                                                .bytes = item.label_length}) == 0))
            {
                head.chosen = at;
                break;
            }
        }
        head.first = previous.first;
    }
    int alone = 0;
    if (head.count == 1u)
    {
        unsigned int ranked_at = 0u;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = built,
                                                                .offset = offsetof(PanelsCompletion, ranked),
                                                                .dst = &ranked_at,
                                                                .bytes = sizeof(ranked_at)});
        PanelsItem only;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = built,
            .offset = offsetof(PanelsCompletion, item) + ((unsigned long long)ranked_at * sizeof(only)),
            .dst = &only,
            .bytes = sizeof(only)});
        alone = (only.label_length == head.prefix_length) &&
                (g_cli_guard->movable_buf_compare(
                     &(CarcerMovableCfg){.cell = built,
                                         .offset = offsetof(PanelsCompletion, text) + only.label,
                                         .src = head.prefix,
                                         .bytes = head.prefix_length}) == 0);
    }
    const int empty = (head.prefix_length == 0ULL) && !s_panels.completion_forced && !was;
    if ((head.count == 0u) || (alone && !s_panels.completion_forced) || empty)
    {
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
        return;
    }
    EMBED_CALL(panels_completion_scrolled, PanelsCtx, .head = &head);
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = built, .src = &head, .bytes = sizeof(head)});
    s_panels.completion_built = s_panels.completion;
    s_panels.completion = built;
    s_panels.completion_open = 1;
}

// the completions asked for the word before the cursor: the file's words, positioned in the built list's cell
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
    if ((s_panels.completion == NULL) || (s_panels.completion_built == NULL))
    {
        return;
    }
    EMBED_CALL(panels_completion_positioned, PanelsCtx, .list = s_panels.completion_built, .server = NULL, .key = key,
               .line = cursor.line, .from = from, .prefix = prefix,
               .length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = sizeof(prefix)}));
    EMBED_CALL(panels_completion_are, PanelsCtx, .at = 0u);
}

// The list, where it is open, follows the frame's cursor. Where the word before the cursor changed, the call filters
// the list again in its own cell by that word. The list closes where the cursor left the line. It also closes where
// that word no longer starts at the prefix's start
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
    memor.cpy(&(MemoriaCfg){.dst = head.prefix, .src = prefix, .bytes = length + 1ULL});
    memor.cpy(&(MemoriaCfg){.dst = head.lowered, .src = prefix, .bytes = length + 1ULL});
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .size = length, .text_out = head.lowered);
    head.prefix_length = length;
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_panels.completion, .src = &head, .bytes = sizeof(head)});
    EMBED_CALL(panels_completion_rank, PanelsCtx, .list = s_panels.completion);
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_panels.completion, .dst = &head, .bytes = sizeof(head)});
    if (head.count == 0u)
    {
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
        return;
    }
    EMBED_CALL(panels_completion_scrolled, PanelsCtx, .head = &head);
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_panels.completion, .src = &head, .bytes = sizeof(head)});
    s_panels.completion_open = 1;
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

void cli_servers_panels_accept(const CliServersPanelsCfg *args)
{
    EMBED_CALL(panels_completion_accept, PanelsCtx, .at = args->at);
}

void cli_servers_panels_wheel(const CliServersPanelsCfg *args)
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
    const unsigned int max = head.count - CLI_COMPLETION_ROWS;
    if (args->rows < 0)
    {
        // the args->rows turned up, a count far inside what an unsigned int represents
        const unsigned int up = (unsigned int)(-args->rows);
        head.first = (head.first > up) ? (head.first - up) : 0u;
    }
    else
    {
        // the args->rows turned are not negative
        head.first += (unsigned int)args->rows;
        head.first = (head.first > max) ? max : head.first;
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
    EMBED_CALL(panels_completion_scrolled, PanelsCtx, .head = &head);
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = s_panels.completion, .offset = 0ULL, .src = &head, .bytes = sizeof(PanelsListHead)});
    return 1;
}

int cli_servers_panels_key(const CliServersPanelsCfg *args)
{
    if (EMBED_CALL(panels_completion_key, PanelsCtx, .text = args->name))
    {
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = 64ULL}) &&
        (s_panels.signature_displayed || s_panels.hover_displayed))
    {
        cli_servers_panels_hide();
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Mod+Space", .cap = 64ULL}))
    {
        s_panels.completion_forced = 1;
        EMBED_CALL(panels_completion_ask, PanelsCtx, .at = 0u);
        return 1;
    }
    EMBED_CALL(panels_hover_hide, PanelsCtx, .at = 0u);
    // a key that takes characters out saves the list open on the prefix left; any other key closes it
    if (s_panels.completion_open)
    {
        const int edits = cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Backspace", .cap = 64ULL}) ||
                          cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Delete", .cap = 64ULL});
        s_panels.completion_wanted = edits;
        if (!edits)
        {
            EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
        }
    }
    return 0;
}

void cli_servers_panels_text(const CliServersPanelsCfg *args)
{
    s_panels.changed = 1;
    EMBED_CALL(panels_hover_hide, PanelsCtx, .at = 0u);
    if ((args->length > 0ULL) && EMBED_CALL(panels_word_byte, PanelsCtx, .text = args->text + args->length - 1ULL))
    {
        s_panels.completion_wanted = 1;
    }
    else
    {
        s_panels.completion_wanted = 0;
        EMBED_CALL(panels_completion_close, PanelsCtx, .at = 0u);
    }
    if (EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->text, .at = 0ULL,
                   .size = args->length, .set = "(,") < args->length)
    {
        s_panels.signature_due_ms = cli_clock_milliseconds() + CLI_PANELS_SIGNATURE_WAIT_MS;
    }
}

static void panels_signature_tree_run(void *context);
static void panels_signature_tree_done(void *context);

// A signature's lines as the document gives them: the ask they answer, the state the first line enters in, and the
// cursor's line among them
typedef struct
{
    PanelsSignatureAsk *ask;
    char state[CLI_PANELS_STATE];
    unsigned long long line;
} PanelsSignatureLines;

// the tree's signature of the call read off the lines the document gives
static void panels_signature_lines_read(const CliEachTextLengthCfg *args)
{
    PanelsSignatureLines *const lines = (PanelsSignatureLines *)args->context;
    PanelsSignatureAsk *const ask = lines->ask;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    ask->found =
        EMBED_CALL(cli_symbols_path_text_size_out.signature_read, CliSymbolsPathTextSizeOutCfg, .path = ask->path,
                   .text = args->text, .size = args->length, .state = (lines->state[0] != '\0') ? lines->state : "root",
                   .line = lines->line, .col = ask->cursor.col, .err_msg = &ignored, .store = &s_panels.signature);
}

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

// the text's lines from CLI_PANELS_SIGNATURE_LINES above the cursor's to the cursor's. For a paged text, the lines the
// page read. The read starts in the first line's state, and gives the tree's signature of the call
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
    PanelsSignatureLines lines = {ask, {0}, line - top};
    (void)EMBED_CALL(cli_document_key.lines_each, CliDocumentKeyCfg, .key = ask->key, .first = top, .last = line,
                     .state = lines.state, .capacity = sizeof(lines.state), .each = panels_signature_lines_read,
                     .context = &lines);
}

// the tree's answer is, where the ask wrote one into the store
static void panels_signature_tree_done(void *context)
{
    PanelsSignatureAsk *const ask = (PanelsSignatureAsk *)context;
    EMBED_CALL(panels_signature_hide, PanelsCtx, .at = 0u);
    s_panels.signature_displayed = ask->found;
    ask->found = 0;
    s_panels.signature_asking = 0;
}

// What a serve job does: open the file in its server with its text, send its changed text, or send where its view rests
#define PANELS_SERVE_OPEN 0u
#define PANELS_SERVE_CHANGE 1u
#define PANELS_SERVE_VIEW 2u

// A serve job: what it does; the file's whole path and its language; the view's key, whose text the job reads. The
// view's first and last lines and the lines that open the regions around its first line, outermost first; and the
// problems' sink
struct PanelsServeJob
{
    unsigned int does;
    char full[CLI_PATH_MAX_LEN];
    char language[CLI_NAME_MAX_LEN];
    char key[CLI_NAME_MAX_LEN];
    unsigned int first;
    unsigned int last;
    unsigned int openers;
    unsigned int opener[CLI_PANELS_OPENERS];
    CliServersSink sink;
};

// the view's text the document gives sent to the job's server: opened in it, or sent as its changed text
static void panels_serve_text(const CliEachTextLengthCfg *args)
{
    const PanelsServeJob *const job = (const PanelsServeJob *)args->context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    if (job->does == PANELS_SERVE_OPEN)
    {
        int served = 0;
        (void)EMBED_CALL(cli_servers_root.open, CliServersRootCfg, .root = s_panels.root, .path = job->full,
                         .language = job->language, .text = args->text, .size = args->length, .sink = &job->sink,
                         .served = &served, .err_msg = &ignored);
        return;
    }
    (void)EMBED_CALL(cli_servers_root.change, CliServersRootCfg, .path = job->full, .text = args->text,
                     .size = args->length, .err_msg = &ignored);
}

// One serve job on a thread of the servers, lined up by the view thread, which goes on at once. For an open or a
// change, the document reads the view's text on this thread and gives it to panels_serve_text. The server's open and
// change cut a slice, and the slice cut walks the file and its includes
static void panels_serve_job(const CliServersJobCfg *args)
{
    const PanelsServeJob *const job = (const PanelsServeJob *)args->job;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    if (job->does == PANELS_SERVE_VIEW)
    {
        char output_text[16];
        VerbaLinea output = {output_text, sizeof(output_text), 0ULL};
        (void)EMBED_CALL(cli_servers_path_out_err_msg.view, CliServersPathOutErrMsgCfg, .path = job->full,
                         .first = job->first, .last = job->last, .opener = job->opener, .openers = job->openers,
                         .out = &output, .err_msg = &ignored);
        return;
    }
    (void)EMBED_CALL(cli_document_key.text_each, CliDocumentKeyCfg, .key = job->key, .each = panels_serve_text,
                     .context = (void *)(embed_word)job);
}

// the serve job `job` passed to a thread of the servers: 1, or 0 where the jobs ring is full
static int panels_serve_later(const PanelsCtx *args)
{
    return EMBED_CALL(cli_servers_run_internal.later, CliServersRunCtx, .run = panels_serve_job, .job = args->job,
                      .size = sizeof(*args->job));
}

// the view under `key` opened in its language's server. The open goes in line for a thread of the servers with the
// view's text. The call marks the view served, and sends its view at the next step. That needs a view on a file of the
// tree, in a language the plugins know
static void panels_open_send(const PanelsCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    if ((s_panels.root == NULL) ||
        !EMBED_CALL(cli_view_path_type.path, CliViewPathTypeCfg, .key = args->key, .path_out = path) ||
        (path[0] == '\0'))
    {
        return;
    }
    PanelsServeJob job = {.does = PANELS_SERVE_OPEN};
    const int known = EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg, .path = path,
                                 .id_out = job.language);
    if (!known)
    {
        return;
    }
    VerbaLinea keyed = {job.key, sizeof(job.key), 0ULL};
    mmgr_line_put(&keyed, args->key);
    if (mmgr_line_finish(&keyed) == 0ULL)
    {
        return;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_panels.root, .relative = path, .joined = job.full);
    job.sink = cli_servers_problems_sink(&(CliServersProblemsCfg){.root = s_panels.root});
    const int lined = EMBED_CALL(panels_serve_later, PanelsCtx, .job = &job);
    if (lined && EMBED_CALL(cli_view_full.served_set, CliViewFullCfg, .key = args->key, .full = job.full))
    {
        s_panels.view_sent = 0;
        s_panels.view_due_ms = cli_clock_milliseconds();
    }
}

// the view under `key`, about to close, lets go of its server. The server closes where its file has no view it serves
static void panels_closing(const char *key)
{
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_view_full_type.serve, CliViewFullTypeCfg, .key = key, .full_out = full))
    {
        return;
    }
    // the view lets go of its server first. Then a view of the same file that the server still serves saves the
    // server's copy
    (void)EMBED_CALL(cli_view_full.served_set, CliViewFullCfg, .key = key, .full = "");
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_view_full.serving, CliViewFullCfg, .full = full))
    {
        (void)EMBED_CALL(cli_servers_path_out_err_msg_from_to.close, CliServersPathOutErrMsgFromToCfg, .path = full,
                         .err_msg = &ignored);
    }
}

// the view's text sent to the server on a thread of the servers. That needs a view the server serves, with a text that
// changed since the server last took it
static void panels_sync_post(const PanelsCtx *args)
{
    (void)args;
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    PanelsServeJob job = {.does = PANELS_SERVE_CHANGE};
    if ((key == NULL) || !EMBED_CALL(cli_view_full_type.serve, CliViewFullTypeCfg, .key = key, .full_out = job.full))
    {
        return;
    }
    if (!EMBED_CALL(cli_view_table.changed_clear, CliViewCfg, .key = key))
    {
        return;
    }
    s_panels.changed = 0;
    VerbaLinea keyed = {job.key, sizeof(job.key), 0ULL};
    mmgr_line_put(&keyed, key);
    if (mmgr_line_finish(&keyed) != 0ULL)
    {
        (void)EMBED_CALL(panels_serve_later, PanelsCtx, .job = &job);
    }
}

// the place where the view of the tab displayed rests, sent to the server on a thread of the servers. A view the server
// does not serve yet opens in its server first
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
        // a view opens in its server once, the first time it rests
        if (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = s_panels.open_key, .cap = CLI_NAME_MAX_LEN}))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = s_panels.open_key,
                .src = key,
                .bytes = (embed_word)cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_NAME_MAX_LEN - 1u}) + 1u});
            EMBED_CALL(panels_open_send, PanelsCtx, .key = key);
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
    if (EMBED_CALL(panels_serve_later, PanelsCtx, .job = &job))
    {
        s_panels.view_sent = 1;
        s_panels.view_rows = rows;
        VerbaLinea sent = {s_panels.sent_key, sizeof(s_panels.sent_key), 0ULL};
        mmgr_line_put(&sent, key);
        (void)mmgr_line_finish(&sent);
    }
}

// a frame of a view put in place. For the view displayed, the panels follow its cursor, its top line, its lines and
// its rows. Where the cursor or the view moved, the panels ask the server again
static void panels_drawn(const CliViewDrawn *args)
{
    const char *const key = EMBED_CALL(panels_viewed_key, PanelsCtx, .at = 0u);
    if ((key == NULL) || !cellul.eq(&(CatenaFinitaCfg){.src = key, .other = args->key, .cap = CLI_NAME_MAX_LEN}))
    {
        return;
    }
    s_panels.rows = args->rows;
    s_panels.cursor = args->cursor;
    // the server has where this view rests: the view sent is this one, at this top and these rows
    const int rested =
        s_panels.view_sent && (s_panels.view_top == args->top) && (s_panels.view_rows == args->rows) &&
        cellul.eq(&(CatenaFinitaCfg){.src = s_panels.sent_key, .other = key, .cap = sizeof(s_panels.sent_key)});
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
    if (s_panels.signature_displayed && !s_panels.signature_asking && moved)
    {
        EMBED_CALL(panels_signature_ask, PanelsCtx, .at = 0u);
    }
}

unsigned long long cli_servers_panels_due_ms(void)
{
    const unsigned long long dues[] = {s_panels.hover_due_ms, s_panels.hover_take_ms, s_panels.hover_hide_ms,
                                       s_panels.signature_due_ms, s_panels.view_due_ms};
    unsigned long long earliest = 0ULL;
    for (unsigned int at = 0u; at < (sizeof(dues) / sizeof(dues[0])); at += 1u)
    {
        earliest = ((dues[at] != 0ULL) && ((earliest == 0ULL) || (dues[at] < earliest))) ? dues[at] : earliest;
    }
    return earliest;
}

// What the panels show, as the step compares it before and after: the hover and its place, the list open, and the
// signature. A step that changes none of it owes no frame.
typedef struct
{
    int hover_displayed;
    unsigned long long hover_length;
    unsigned long long hover_line;
    unsigned long long hover_from;
    unsigned long long hover_to;
    int completion_open;
    int signature_displayed;
} PanelsShown;

// what the panels show now
EMBED_INLINE PanelsShown panels_shown(const PanelsCtx *args)
{
    (void)args;
    return (PanelsShown){s_panels.hover_displayed,    s_panels.hover_length, s_panels.hover_line,
                         s_panels.hover_from,         s_panels.hover_to,     s_panels.completion_open,
                         s_panels.signature_displayed};
}

int cli_servers_panels_step(void)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    const PanelsShown before = EMBED_CALL(panels_shown, PanelsCtx, .at = 0u);
    int asked = 0;
    EMBED_CALL(panels_hover_answers_taken, PanelsCtx, .at = 0u);
    // The view thread takes an answer still to come at the next deadline, until CLI_PANELS_HOVER_ANSWER_MS after the
    // ask
    if ((s_panels.hover_take_ms != 0ULL) && (now_ms >= s_panels.hover_take_ms))
    {
        s_panels.hover_take_ms = ((now_ms - s_panels.hover_asked_ms) < CLI_PANELS_HOVER_ANSWER_MS)
                                     ? now_ms + CLI_PANELS_HOVER_TAKE_MS
                                     : 0ULL;
    }
    if ((s_panels.hover_hide_ms != 0ULL) && (now_ms >= s_panels.hover_hide_ms))
    {
        EMBED_CALL(panels_hover_hide, PanelsCtx, .at = 0u);
    }
    if ((s_panels.hover_due_ms != 0ULL) && (now_ms >= s_panels.hover_due_ms))
    {
        s_panels.hover_due_ms = 0ULL;
        s_panels.hover_ask += 1ULL;
        EMBED_CALL(panels_hover_ask, PanelsCtx, .at = 0u);
    }
    if ((s_panels.signature_due_ms != 0ULL) && (now_ms >= s_panels.signature_due_ms))
    {
        s_panels.signature_due_ms = 0ULL;
        EMBED_CALL(panels_signature_ask, PanelsCtx, .at = 0u);
        // a signature asked for anew can change what it shows while it stays displayed
        asked = s_panels.signature_displayed;
    }
    if ((s_panels.view_due_ms != 0ULL) && (now_ms >= s_panels.view_due_ms))
    {
        EMBED_CALL(panels_view_send, PanelsCtx, .at = 0u);
    }
    const PanelsShown after = EMBED_CALL(panels_shown, PanelsCtx, .at = 0u);
    return asked || (memor.cmp(&(MemoriaCfg){.src = &before, .other = &after, .bytes = sizeof(before)}) != 0);
}

// the views' moments the panels take: each frame put in place, and each view about to close. A view opens in its
// server as it first rests, not as it opens
static const CliViewWatcher s_panels_watcher = {panels_drawn, NULL, panels_closing};

void cli_servers_panels_begin(const CliServersPanelsCfg *args)
{
    s_panels.root = args->root;
    panels_hover_ring_positioned();
    // the two list cells, the one displayed and the one built, each taken once at its max
    if (s_panels.completion == NULL)
    {
        s_panels.completion = g_cli_guard->movable_buf_alloc(sizeof(PanelsCompletion), g_cli_servers_owner);
    }
    if (s_panels.completion_built == NULL)
    {
        s_panels.completion_built = g_cli_guard->movable_buf_alloc(sizeof(PanelsCompletion), g_cli_servers_owner);
    }
    cli_view_watch(&(CliViewAtCfg){.watcher = &s_panels_watcher});
}

void cli_servers_panels_end(void)
{
    cli_view_watch(&(CliViewAtCfg){.watcher = NULL});
    cli_servers_panels_hide();
    // every answer pushed and not taken goes back with the panels
    EMBED_CALL(panels_hover_answers_taken, PanelsCtx, .at = 0u);
    (void)g_cli_guard->movable_buf_release(s_panels.completion, g_cli_servers_owner);
    s_panels.completion = NULL;
    (void)g_cli_guard->movable_buf_release(s_panels.completion_built, g_cli_servers_owner);
    s_panels.completion_built = NULL;
    s_panels.root = NULL;
}

int cli_servers_panels_hover(const CliServersPanelsCfg *args)
{
    if (!s_panels.hover_displayed)
    {
        return 0;
    }
    *args->hover_out = (CliHoverPanel){s_panels.hover_text, s_panels.hover_length, s_panels.hover_line,
                                       s_panels.hover_from, s_panels.hover_to};
    return 1;
}

int cli_servers_panels_completion(const CliServersPanelsCfg *args)
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
    *args->panel = (CliCompletionPanel){head.line, head.from, s_prefix, head.count, head.first, rows, head.chosen};
    return 1;
}

int cli_servers_panels_completion_row(const CliServersPanelsCfg *args)
{
    // the texts of the row `args->row` copied out of the cell into the rows' own bytes, one set for each row displayed.
    // The caller reads them before the list changes
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
    if ((args->row >= CLI_COMPLETION_ROWS) || !opened || ((head.first + args->row) >= head.count))
    {
        return 0;
    }
    PanelsItem item;
    {
        unsigned int ranked_at = 0u;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_panels.completion,
                                .offset = offsetof(PanelsCompletion, ranked) +
                                          ((unsigned long long)(head.first + args->row) * sizeof(ranked_at)),
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
                                                                .dst = s_label[args->row],
                                                                .bytes = item.label_length});
    }
    s_label[args->row][item.label_length] = '\0';
    if (item.type_length != 0u)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_panels.completion,
                                                                .offset = offsetof(PanelsCompletion, text) + item.type,
                                                                .dst = s_type[args->row],
                                                                .bytes = item.type_length});
    }
    s_type[args->row][item.type_length] = '\0';
    if (item.detail_length != 0u)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_panels.completion,
                                .offset = offsetof(PanelsCompletion, text) + item.detail,
                                .dst = s_detail[args->row],
                                .bytes = item.detail_length});
    }
    s_detail[args->row][item.detail_length] = '\0';
    // the prefix marks a label it starts, case aside; a prefix is far inside what an unsigned int represents
    *args->out =
        (CliCompletionRow){s_label[args->row],
                           item.label_length,
                           s_type[args->row],
                           s_detail[args->row],
                           item.detail_length,
                           ((head.prefix_length > 0ULL) && (item.score >= 4u)) ? (unsigned int)head.prefix_length : 0u};
    return 1;
}

const CliSignaturePanel *cli_servers_panels_signature(void)
{
    return s_panels.signature_displayed ? &s_panels.signature.panel : NULL;
}
