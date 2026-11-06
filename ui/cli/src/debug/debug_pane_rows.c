// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_pane_rows.c: the Debug pane's rows. Frames: the stopped thread's stack read once a stop, each frame its
// function, its file's name and line and its path, the path written from the tree's top where the file is in the tree.
// Variables: each watch evaluated in the chosen frame, then the chosen frame's scopes, the first open until closed and
// the rest closed until opened, the variables of each open one, and under each open variable its own, to
// CLI_DEBUG_DEPTH levels, read again whole as a frame is chosen or a row opens or closes; a row's key is its path of
// labels, and the open set stores the keys whose rows stand the reverse of how they first stand, sorted by their bytes
// and found by halving. Console: a ring of lines over one text cell, a piece of output with no line end running on
// into the last line of its kind, the oldest line going past CLI_DEBUG_CONSOLE_LINES and its bytes with the next
// compaction
#include "debug_internal.h"
#include "debug_pane_internal.h"

// the bytes of a scratch a session's answer is written into, the most bytes of a row's key, the cell the console's
// text is first taken at, and the notices of the views with no rows of their own
#define ROWS_ANSWER (1ULL << 20u)
#define ROWS_KEY 4096u
#define ROWS_CONSOLE_TEXT 4096ULL
#define ROWS_FRAMES_NONE "Frames show here when the program stops."
#define ROWS_FRAMES_RUNNING "Running."
#define ROWS_VARIABLES_NONE "Variables show here when the program stops."

typedef struct
{
    const char *root;
    const char *text;
    unsigned long long length;
    unsigned long long extra;
    long long reference;
    unsigned int depth;
    unsigned int at;
    CliDebugPaneStore *store;
    const CliJson *json;
    CliJson *tree;
    int framed;
    unsigned int node;
    const char *name;
    unsigned long long *length_out;
    long long *value;
    const CliDebugPaneEntry *entry;
    CliDebugPaneRow *row;
} DebugPaneRowsCtx;

// whether `length` bytes of `text` start a path outside any tree: a root, a drive or a share
EMBED_INLINE int rows_absolute(const DebugPaneRowsCtx *args)
{
    const char *const text = args->text;
    return (args->length != 0ULL) &&
           ((text[0] == '/') || (text[0] == '\\') ||
            ((args->length > 2ULL) && (text[1] == ':') && ((text[2] == '/') || (text[2] == '\\'))));
}

// a path the adapter wrote as the JSON string the pane reads it as: from the tree's top where it is under the tree
// `context` carries, else as it came
static void rows_path_written(const CliDebugPathWrittenCfg *args)
{
    const char *const root = (const char *)args->context;
    const unsigned long long path_length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN});
    const unsigned long long root_length =
        (root != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_PATH_MAX_LEN}) : 0ULL;
    int under = (root_length != 0ULL) && (path_length > root_length + 1ULL);
    for (unsigned long long at = 0ULL; under && (at < root_length); at += 1ULL)
    {
        const char path_byte = (args->path[at] == '\\') ? '/' : args->path[at];
        const char root_byte = (root[at] == '\\') ? '/' : root[at];
        under = path_byte == root_byte;
    }
    under = under && ((args->path[root_length] == '/') || (args->path[root_length] == '\\'));
    const unsigned long long from = under ? root_length + 1ULL : 0ULL;
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)(args->path + from), .length = path_length - from);
}

// the string member `name` of `node`, its bytes into `length_out`: NULL where it has none
EMBED_INLINE const char *rows_member(const DebugPaneRowsCtx *args)
{
    *args->length_out = 0ULL;
    return EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = args->json, .object = args->node, .name = args->name),
                      .length_pointer = args->length_out);
}

// the number member `name` of `node` into `value`: 1, or 0 where it has none
EMBED_INLINE int rows_number(const DebugPaneRowsCtx *args)
{
    return EMBED_CALL(cli_debug_json_node_internal.debug_sign, CliDebugJsonNodeCtx, .json = args->json,
                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = args->json, .object = args->node, .name = args->name),
                      .value = args->value);
}

// the order of `length` bytes of `text` against the key of the open set's entry `at`: below 0, 0 or above 0
EMBED_INLINE int rows_key_order(const DebugPaneRowsCtx *args)
{
    const CliDebugPaneStore *const flipped = &g_cli_debug_pane.flipped;
    const CliDebugPaneEntry *const entry = &flipped->entry[args->at];
    const char *const key = flipped->text + entry->text;
    const unsigned long long shorter = (args->length < entry->text_length) ? args->length : entry->text_length;
    for (unsigned long long at = 0ULL; at < shorter; at += 1ULL)
    {
        if (args->text[at] != key[at])
        {
            // a byte read as unsigned carries its eight bits, the order the keys sort by
            return ((unsigned char)args->text[at] < (unsigned char)key[at]) ? -1 : 1;
        }
    }
    if (args->length == entry->text_length)
    {
        return 0;
    }
    return (args->length < entry->text_length) ? -1 : 1;
}

// the first entry of the open set whose key is at least `length` bytes of `text`, by halving over the sorted keys
EMBED_INLINE unsigned int rows_key_place(const DebugPaneRowsCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = g_cli_debug_pane.flipped.count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (EMBED_CALL(rows_key_order, DebugPaneRowsCtx, .text = args->text, .length = args->length, .at = middle) > 0)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// whether the key of `length` bytes of `text` stands in the open set
EMBED_INLINE int rows_flipped(const DebugPaneRowsCtx *args)
{
    const unsigned int place = EMBED_CALL(rows_key_place, DebugPaneRowsCtx, .text = args->text, .length = args->length);
    return (place < g_cli_debug_pane.flipped.count) &&
           (EMBED_CALL(rows_key_order, DebugPaneRowsCtx, .text = args->text, .length = args->length, .at = place) == 0);
}

// the key of `length` bytes of `text` put into the open set where it is not there, else taken out, the set's cells
// taken anew to the keys left as one goes
EMBED_INLINE void rows_flip(const DebugPaneRowsCtx *args)
{
    CliDebugPaneStore *const flipped = &g_cli_debug_pane.flipped;
    const unsigned int place = EMBED_CALL(rows_key_place, DebugPaneRowsCtx, .text = args->text, .length = args->length);
    if ((place < flipped->count) &&
        (EMBED_CALL(rows_key_order, DebugPaneRowsCtx, .text = args->text, .length = args->length, .at = place) == 0))
    {
        CliDebugPaneStore left = {0};
        for (unsigned int at = 0u; at < flipped->count; at += 1u)
        {
            if (at == place)
            {
                continue;
            }
            CliDebugPaneEntry *const left_entry =
                EMBED_CALL(cli_debug_pane_internal.entry_put, CliDebugPaneCtx, .store = &left);
            if (left_entry == NULL)
            {
                break;
            }
            left_entry->text_length = flipped->entry[at].text_length;
            left_entry->text =
                EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = &left,
                           .text = flipped->text + flipped->entry[at].text, .length = flipped->entry[at].text_length);
        }
        EMBED_CALL(cli_debug_pane_internal.store_empty, CliDebugPaneCtx, .store = flipped);
        *flipped = left;
        return;
    }
    const unsigned long long text = EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = flipped,
                                               .text = args->text, .length = args->length);
    CliDebugPaneEntry *const entry =
        (text != CLI_DEBUG_PANE_NONE) ? EMBED_CALL(cli_debug_pane_internal.entry_put, CliDebugPaneCtx, .store = flipped)
                                      : NULL;
    if (entry == NULL)
    {
        return;
    }
    // the entry put last moved to its place among the sorted keys
    const unsigned int last = flipped->count - 1u;
    if (place < last)
    {
        memor.move_up(&(MemoriaCfg){.dst = &flipped->entry[place + 1u],
                                    .src = &flipped->entry[place],
                                    .bytes = (unsigned long long)(last - place) * sizeof(CliDebugPaneEntry)});
    }
    flipped->entry[place] = (CliDebugPaneEntry){0};
    flipped->entry[place].text = text;
    flipped->entry[place].text_length = args->length;
}

// a row's texts put into the store: `text` with `length` bytes as its first, its key after
EMBED_INLINE CliDebugPaneEntry *rows_entry(const DebugPaneRowsCtx *args)
{
    CliDebugPaneEntry *const entry =
        EMBED_CALL(cli_debug_pane_internal.entry_put, CliDebugPaneCtx, .store = args->store);
    if (entry == NULL)
    {
        return NULL;
    }
    entry->text = EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = args->store,
                             .text = args->text, .length = args->length);
    entry->text_length = (entry->text != CLI_DEBUG_PANE_NONE) ? args->length : 0ULL;
    entry->text = (entry->text != CLI_DEBUG_PANE_NONE) ? entry->text : 0ULL;
    return entry;
}

// the request's answer, `length` bytes of `text`, read into `tree` in a cell sized to it, the caller's to release:
// its root, CLI_JSON_NONE where it is not JSON
EMBED_INLINE unsigned int rows_answer_read(const DebugPaneRowsCtx *args)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    return EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg,
                      .json = args->tree, .text = args->text, .size = args->length, .err_msg = &err_msg,
                      .owner = g_cli_debug_pane_owner);
}

// the variables of `reference` put after the store's rows at `depth`, each key `length` bytes of `text`, a slash and
// its name, and under each open one its own, to CLI_DEBUG_DEPTH levels
EMBED_INLINE void rows_variables_under(const DebugPaneRowsCtx *args)
{
    char *const scratch = g_cli_guard->temporary_buf_alloc(ROWS_ANSWER, g_cli_debug_pane_owner);
    if (scratch == NULL)
    {
        return;
    }
    VerbaLinea out = {scratch, ROWS_ANSWER, 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int answered = g_cli_debug_pane.requests->variables(&(CliDebugOutCfg){
        .out = &out, .err_msg = &err_msg, .id = g_cli_debug_pane.session, .reference = args->reference});
    CliJson json = {0};
    const unsigned int root =
        (answered && mmgr_line_ok(&out))
            ? EMBED_CALL(rows_answer_read, DebugPaneRowsCtx, .tree = &json, .text = scratch, .length = out.at)
            : CLI_JSON_NONE;
    g_cli_guard->persistent_buf_release(scratch, g_cli_debug_pane_owner);
    for (unsigned int at = 0u; root != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = &json, .array = root, .index = at);
        if (one == CLI_JSON_NONE)
        {
            break;
        }
        unsigned long long name_length = 0ULL;
        unsigned long long value_length = 0ULL;
        unsigned long long kind_length = 0ULL;
        long long reference = 0LL;
        const char *const name = EMBED_CALL(rows_member, DebugPaneRowsCtx, .json = &json, .node = one, .name = "name",
                                            .length_out = &name_length);
        const char *const value = EMBED_CALL(rows_member, DebugPaneRowsCtx, .json = &json, .node = one, .name = "value",
                                             .length_out = &value_length);
        const char *const kind = EMBED_CALL(rows_member, DebugPaneRowsCtx, .json = &json, .node = one, .name = "kind",
                                            .length_out = &kind_length);
        (void)EMBED_CALL(rows_number, DebugPaneRowsCtx, .json = &json, .node = one, .name = "reference",
                         .value = &reference);
        char key[ROWS_KEY];
        VerbaLinea key_line = {key, sizeof(key), 0ULL};
        mmgr_line_put_n(&key_line, args->text, args->length);
        mmgr_line_ch(&key_line, '/');
        mmgr_line_put_n(&key_line, (name != NULL) ? name : "", name_length);
        const int keyed = mmgr_line_ok(&key_line);
        const int opens = reference != 0LL;
        const int open =
            opens && keyed && EMBED_CALL(rows_flipped, DebugPaneRowsCtx, .text = key, .length = key_line.at);
        CliDebugPaneEntry *const entry = EMBED_CALL(rows_entry, DebugPaneRowsCtx, .store = args->store,
                                                    .text = (name != NULL) ? name : "", .length = name_length);
        if (entry == NULL)
        {
            break;
        }
        entry->kind = CLI_DEBUG_ROW_VARIABLE;
        entry->depth = args->depth;
        entry->opens = opens ? 1u : 0u;
        entry->open = open ? 1u : 0u;
        entry->reference = reference;
        entry->second = EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = args->store,
                                   .text = (value != NULL) ? value : "", .length = value_length);
        entry->second_length = value_length;
        entry->third = EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = args->store,
                                  .text = (kind != NULL) ? kind : "", .length = kind_length);
        entry->third_length = kind_length;
        entry->key = EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = args->store, .text = key,
                                .length = keyed ? key_line.at : 0ULL);
        entry->key_length = keyed ? key_line.at : 0ULL;
        if (open && (args->depth < CLI_DEBUG_DEPTH))
        {
            EMBED_CALL(rows_variables_under, DebugPaneRowsCtx, .store = args->store, .reference = reference,
                       .depth = args->depth + 1u, .text = key, .length = key_line.at);
        }
    }
    g_cli_guard->persistent_buf_release(json.node, g_cli_debug_pane_owner);
}

// a watch's row put into the store, its expression, `length` bytes of `text` with its end after them, evaluated in
// the frame `reference` where one is `framed`: its value and type, or why it could not be evaluated as its failed
// value
EMBED_INLINE void rows_watch(const DebugPaneRowsCtx *args)
{
    CliDebugPaneEntry *const entry =
        EMBED_CALL(rows_entry, DebugPaneRowsCtx, .store = args->store, .text = args->text, .length = args->length);
    if (entry == NULL)
    {
        return;
    }
    entry->kind = CLI_DEBUG_ROW_WATCH;
    if (!args->framed)
    {
        return;
    }
    char *const scratch = g_cli_guard->temporary_buf_alloc(ROWS_ANSWER, g_cli_debug_pane_owner);
    if (scratch == NULL)
    {
        return;
    }
    VerbaLinea out = {scratch, ROWS_ANSWER, 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int evaluated = g_cli_debug_pane.requests->evaluate(&(CliDebugOutErrMsgCfg){.out = &out,
                                                                                      .err_msg = &err_msg,
                                                                                      .id = g_cli_debug_pane.session,
                                                                                      .frame = args->reference,
                                                                                      .expression = args->text,
                                                                                      .has_frame = 1,
                                                                                      .context_text = "watch"});
    CliJson json = {0};
    const unsigned int root =
        (evaluated && mmgr_line_ok(&out))
            ? EMBED_CALL(rows_answer_read, DebugPaneRowsCtx, .tree = &json, .text = scratch, .length = out.at)
            : CLI_JSON_NONE;
    unsigned long long value_length = 0ULL;
    unsigned long long kind_length = 0ULL;
    const char *const value = (root != CLI_JSON_NONE)
                                  ? EMBED_CALL(rows_member, DebugPaneRowsCtx, .json = &json, .node = root,
                                               .name = "value", .length_out = &value_length)
                                  : NULL;
    const char *const kind = (root != CLI_JSON_NONE)
                                 ? EMBED_CALL(rows_member, DebugPaneRowsCtx, .json = &json, .node = root,
                                              .name = "kind", .length_out = &kind_length)
                                 : NULL;
    const int failed = value == NULL;
    const unsigned long long refused = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL;
    entry->failed = failed ? 1u : 0u;
    entry->second = EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = args->store,
                               .text = failed ? why : value, .length = failed ? refused : value_length);
    entry->second_length = failed ? refused : value_length;
    entry->third = EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = args->store,
                              .text = (kind != NULL) ? kind : "", .length = kind_length);
    entry->third_length = kind_length;
    g_cli_guard->persistent_buf_release(json.node, g_cli_debug_pane_owner);
    g_cli_guard->persistent_buf_release(scratch, g_cli_debug_pane_owner);
}

// the chosen frame's scopes put into the store, each open one's variables under it
EMBED_INLINE void rows_scopes(const DebugPaneRowsCtx *args)
{
    char *const scratch = g_cli_guard->temporary_buf_alloc(ROWS_ANSWER, g_cli_debug_pane_owner);
    if (scratch == NULL)
    {
        return;
    }
    VerbaLinea out = {scratch, ROWS_ANSWER, 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int answered = g_cli_debug_pane.requests->scopes(&(CliDebugOutErrMsgCfg){
        .out = &out, .err_msg = &err_msg, .id = g_cli_debug_pane.session, .frame = args->reference, .has_frame = 1});
    CliJson json = {0};
    const unsigned int root =
        (answered && mmgr_line_ok(&out))
            ? EMBED_CALL(rows_answer_read, DebugPaneRowsCtx, .tree = &json, .text = scratch, .length = out.at)
            : CLI_JSON_NONE;
    g_cli_guard->persistent_buf_release(scratch, g_cli_debug_pane_owner);
    for (unsigned int at = 0u; root != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = &json, .array = root, .index = at);
        if (one == CLI_JSON_NONE)
        {
            break;
        }
        unsigned long long name_length = 0ULL;
        long long reference = 0LL;
        const char *const name = EMBED_CALL(rows_member, DebugPaneRowsCtx, .json = &json, .node = one, .name = "name",
                                            .length_out = &name_length);
        (void)EMBED_CALL(rows_number, DebugPaneRowsCtx, .json = &json, .node = one, .name = "reference",
                         .value = &reference);
        const int expensive = EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = &json,
                                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                            .json_read = &json, .object = one, .name = "expensive"));
        char key[ROWS_KEY];
        VerbaLinea key_line = {key, sizeof(key), 0ULL};
        mmgr_line_put(&key_line, "scope:");
        mmgr_line_put_n(&key_line, (name != NULL) ? name : "", name_length);
        const int keyed = mmgr_line_ok(&key_line);
        // the first scope, a frame's own variables, stands open until closed; the rest, and one the adapter reports
        // is slow to read, stand closed until opened
        const int first = (at == 0u) && !expensive;
        const int flipped = keyed && EMBED_CALL(rows_flipped, DebugPaneRowsCtx, .text = key, .length = key_line.at);
        const int opens = reference != 0LL;
        const int open = opens && (first != flipped);
        CliDebugPaneEntry *const entry = EMBED_CALL(rows_entry, DebugPaneRowsCtx, .store = args->store,
                                                    .text = (name != NULL) ? name : "", .length = name_length);
        if (entry == NULL)
        {
            break;
        }
        entry->kind = CLI_DEBUG_ROW_SCOPE;
        entry->opens = opens ? 1u : 0u;
        entry->open = open ? 1u : 0u;
        entry->reference = reference;
        entry->key = EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = args->store, .text = key,
                                .length = keyed ? key_line.at : 0ULL);
        entry->key_length = keyed ? key_line.at : 0ULL;
        if (open)
        {
            EMBED_CALL(rows_variables_under, DebugPaneRowsCtx, .store = args->store, .reference = reference,
                       .depth = 1u, .text = key, .length = key_line.at);
        }
    }
    g_cli_guard->persistent_buf_release(json.node, g_cli_debug_pane_owner);
}

void cli_debug_pane_variables_read(const CliDebugPaneCtx *args)
{
    (void)args;
    CliDebugPaneStore built = {0};
    const CliDebugPaneStore *const frames = &g_cli_debug_pane.frames;
    const int framed =
        g_cli_debug_pane.has_session && g_cli_debug_pane.paused && (g_cli_debug_pane.frame_at < frames->count);
    const long long frame = framed ? frames->entry[g_cli_debug_pane.frame_at].reference : 0LL;
    const CliDebugPaneStore *const watches = &g_cli_debug_pane.watches;
    for (unsigned int at = 0u; at < watches->count; at += 1u)
    {
        EMBED_CALL(rows_watch, DebugPaneRowsCtx, .store = &built, .text = watches->text + watches->entry[at].text,
                   .length = watches->entry[at].text_length, .reference = frame, .framed = framed);
    }
    if (framed)
    {
        EMBED_CALL(rows_scopes, DebugPaneRowsCtx, .store = &built, .reference = frame);
    }
    EMBED_CALL(cli_debug_pane_internal.store_empty, CliDebugPaneCtx, .store = &g_cli_debug_pane.variables);
    g_cli_debug_pane.variables = built;
}

void cli_debug_pane_frames_read(const CliDebugPaneCtx *args)
{
    EMBED_CALL(cli_debug_pane_internal.store_empty, CliDebugPaneCtx, .store = &g_cli_debug_pane.frames);
    char *const scratch = g_cli_guard->temporary_buf_alloc(ROWS_ANSWER, g_cli_debug_pane_owner);
    if (scratch == NULL)
    {
        return;
    }
    VerbaLinea out = {scratch, ROWS_ANSWER, 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliJson json = {0};
    // a stop that carries no thread stops the first the adapter lists
    if (!g_cli_debug_pane.has_thread &&
        g_cli_debug_pane.requests->threads(
            &(CliDebugOutErrMsgIdStepCfg){.out = &out, .err_msg = &err_msg, .id = g_cli_debug_pane.session}) &&
        mmgr_line_ok(&out))
    {
        const unsigned int threads =
            EMBED_CALL(rows_answer_read, DebugPaneRowsCtx, .tree = &json, .text = scratch, .length = out.at);
        long long thread = 0LL;
        g_cli_debug_pane.has_thread =
            (threads != CLI_JSON_NONE) &&
            EMBED_CALL(rows_number, DebugPaneRowsCtx, .json = &json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = &json, .array = threads, .index = 0u),
                       .name = "id", .value = &thread);
        g_cli_debug_pane.thread = thread;
        g_cli_guard->persistent_buf_release(json.node, g_cli_debug_pane_owner);
        json = (CliJson){0};
        out.at = 0ULL;
    }
    // the tree's root rides the request's context, which the path writer reads and never writes
    const int answered =
        g_cli_debug_pane.has_thread &&
        g_cli_debug_pane.requests->stack(&(CliDebugOutErrMsgIdThreadCfg){.out = &out,
                                                                         .err_msg = &err_msg,
                                                                         .id = g_cli_debug_pane.session,
                                                                         .thread = g_cli_debug_pane.thread,
                                                                         .named = rows_path_written,
                                                                         .context = (void *)args->root});
    const unsigned int root =
        (answered && mmgr_line_ok(&out))
            ? EMBED_CALL(rows_answer_read, DebugPaneRowsCtx, .tree = &json, .text = scratch, .length = out.at)
            : CLI_JSON_NONE;
    g_cli_guard->persistent_buf_release(scratch, g_cli_debug_pane_owner);
    CliDebugPaneStore *const frames = &g_cli_debug_pane.frames;
    for (unsigned int at = 0u; root != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = &json, .array = root, .index = at);
        if (one == CLI_JSON_NONE)
        {
            break;
        }
        unsigned long long name_length = 0ULL;
        unsigned long long path_length = 0ULL;
        long long id = 0LL;
        long long line = 0LL;
        const char *const name = EMBED_CALL(rows_member, DebugPaneRowsCtx, .json = &json, .node = one, .name = "name",
                                            .length_out = &name_length);
        const char *const path = EMBED_CALL(rows_member, DebugPaneRowsCtx, .json = &json, .node = one, .name = "path",
                                            .length_out = &path_length);
        (void)EMBED_CALL(rows_number, DebugPaneRowsCtx, .json = &json, .node = one, .name = "id", .value = &id);
        (void)EMBED_CALL(rows_number, DebugPaneRowsCtx, .json = &json, .node = one, .name = "line", .value = &line);
        CliDebugPaneEntry *const entry = EMBED_CALL(rows_entry, DebugPaneRowsCtx, .store = frames,
                                                    .text = (name != NULL) ? name : "", .length = name_length);
        if (entry == NULL)
        {
            break;
        }
        entry->kind = CLI_DEBUG_ROW_FRAME;
        entry->reference = id;
        // the request writes a line counted from 0 in its low 32 bits
        entry->line = (unsigned int)(line & 0xffffffffLL);
        if (path == NULL)
        {
            continue;
        }
        // the file's name, the last part of its path, and its line counted from 1
        unsigned long long base = 0ULL;
        for (unsigned long long each = 0ULL; each < path_length; each += 1ULL)
        {
            base = ((path[each] == '/') || (path[each] == '\\')) ? each + 1ULL : base;
        }
        char where[CLI_PATH_MAX_LEN + 16u];
        VerbaLinea where_line = {where, sizeof(where), 0ULL};
        mmgr_line_put_n(&where_line, path + base, path_length - base);
        mmgr_line_ch(&where_line, ':');
        mmgr_line_decimal(&where_line, (unsigned long long)entry->line + 1ULL, 1u);
        const unsigned long long where_length = mmgr_line_ok(&where_line) ? where_line.at : 0ULL;
        entry->second = EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = frames, .text = where,
                                   .length = where_length);
        entry->second_length = where_length;
        entry->third = EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = frames, .text = path,
                                  .length = path_length);
        entry->third_length = path_length;
    }
    g_cli_guard->persistent_buf_release(json.node, g_cli_debug_pane_owner);
}

void cli_debug_pane_ended(const CliDebugPaneCtx *args)
{
    EMBED_CALL(cli_debug_pane_internal.store_empty, CliDebugPaneCtx, .store = &g_cli_debug_pane.frames);
    g_cli_guard->persistent_buf_release(g_cli_debug_pane.session_texts, g_cli_debug_pane_owner);
    g_cli_debug_pane.session_texts = NULL;
    g_cli_debug_pane.has_session = 0;
    g_cli_debug_pane.paused = 0;
    g_cli_debug_pane.has_thread = 0;
    g_cli_debug_pane.step_back = 0;
    g_cli_debug_pane.frame_at = 0u;
    EMBED_CALL(cli_debug_pane_internal.status_set, CliDebugPaneCtx, .text = args->text, .length = args->length);
    EMBED_CALL(cli_debug_pane_internal.variables_read, CliDebugPaneCtx, .root = args->root);
}

// the console's text cell fit to carry `extra` bytes more: the live lines' texts copied in their order into a cell
// twice the bytes they and the extra take where the cell is outgrown, the old cell released: 1, or 0 where the cells
// cannot meet it
EMBED_INLINE int rows_console_fit(const DebugPaneRowsCtx *args)
{
    CliDebugPaneStore *const console = &g_cli_debug_pane.console;
    if (console->used + args->extra <= console->size)
    {
        return 1;
    }
    unsigned long long live = 0ULL;
    for (unsigned int at = 0u; at < console->count; at += 1u)
    {
        live += console->entry[(console->first + at) % console->capacity].text_length + 1ULL;
    }
    const unsigned long long wanted = (live + args->extra) * 2ULL;
    const unsigned long long size = (wanted > ROWS_CONSOLE_TEXT) ? wanted : ROWS_CONSOLE_TEXT;
    char *const grown = g_cli_guard->persistent_buf_alloc(size, g_cli_debug_pane_owner);
    if (grown == NULL)
    {
        return 0;
    }
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; at < console->count; at += 1u)
    {
        CliDebugPaneEntry *const line = &console->entry[(console->first + at) % console->capacity];
        memor.cpy(
            &(MemoriaCfg){.dst = grown + used, .src = console->text + line->text, .bytes = line->text_length + 1ULL});
        line->text = used;
        used += line->text_length + 1ULL;
    }
    g_cli_guard->persistent_buf_release(console->text, g_cli_debug_pane_owner);
    console->text = grown;
    console->size = size;
    console->used = used;
    return 1;
}

// a line put after the console's last, the ring grown to CLI_DEBUG_CONSOLE_LINES and past it the oldest line taken
// out: the line, NULL where the cells cannot meet it
EMBED_INLINE CliDebugPaneEntry *rows_console_line(const DebugPaneRowsCtx *args)
{
    (void)args;
    CliDebugPaneStore *const console = &g_cli_debug_pane.console;
    if ((console->count == console->capacity) && (console->capacity < CLI_DEBUG_CONSOLE_LINES))
    {
        const unsigned int doubled = (console->capacity != 0u) ? console->capacity * 2u : 64u;
        const unsigned int capacity = (doubled < CLI_DEBUG_CONSOLE_LINES) ? doubled : CLI_DEBUG_CONSOLE_LINES;
        CliDebugPaneEntry *const grown = g_cli_guard->persistent_buf_alloc(
            (unsigned long long)capacity * sizeof(CliDebugPaneEntry), g_cli_debug_pane_owner);
        if (grown == NULL)
        {
            return NULL;
        }
        for (unsigned int at = 0u; at < console->count; at += 1u)
        {
            grown[at] = console->entry[(console->first + at) % console->capacity];
        }
        g_cli_guard->persistent_buf_release(console->entry, g_cli_debug_pane_owner);
        console->entry = grown;
        console->capacity = capacity;
        console->first = 0u;
    }
    if (console->count < console->capacity)
    {
        CliDebugPaneEntry *const line = &console->entry[(console->first + console->count) % console->capacity];
        console->count += 1u;
        *line = (CliDebugPaneEntry){0};
        return line;
    }
    CliDebugPaneEntry *const line = &console->entry[console->first];
    console->first = (console->first + 1u) % console->capacity;
    *line = (CliDebugPaneEntry){0};
    return line;
}

void cli_debug_pane_line_write(const CliDebugPaneCtx *args)
{
    CliDebugPaneStore *const console = &g_cli_debug_pane.console;
    unsigned long long from = 0ULL;
    while (from < args->length)
    {
        unsigned long long end = from;
        while ((end < args->length) && (args->text[end] != '\n'))
        {
            end += 1ULL;
        }
        const int ended = end < args->length;
        const unsigned long long piece = end - from;
        CliDebugPaneEntry *const last =
            (console->count != 0u) ? &console->entry[(console->first + console->count - 1u) % console->capacity] : NULL;
        const int runs_on = (last != NULL) && (last->line_kind == args->kind) && !last->ended;
        if (runs_on)
        {
            // the last line's text stands at the end of the cell, its end the cell's last byte used
            if (!EMBED_CALL(rows_console_fit, DebugPaneRowsCtx, .extra = piece))
            {
                return;
            }
            CliDebugPaneEntry *const line = &console->entry[(console->first + console->count - 1u) % console->capacity];
            if (piece != 0ULL)
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = console->text + console->used - 1ULL, .src = args->text + from, .bytes = piece});
            }
            console->used += piece;
            console->text[console->used - 1ULL] = '\0';
            line->text_length += piece;
            line->ended = ended ? 1u : 0u;
        }
        else
        {
            if (!EMBED_CALL(rows_console_fit, DebugPaneRowsCtx, .extra = piece + 1ULL))
            {
                return;
            }
            CliDebugPaneEntry *const line = EMBED_CALL(rows_console_line, DebugPaneRowsCtx, .extra = 0ULL);
            if (line == NULL)
            {
                return;
            }
            line->kind = CLI_DEBUG_ROW_LINE;
            line->line_kind = args->kind;
            line->failed = (args->kind == CLI_DEBUG_LINE_STDERR) ? 1u : 0u;
            line->text = console->used;
            line->text_length = piece;
            line->ended = ended ? 1u : 0u;
            if (piece != 0ULL)
            {
                memor.cpy(
                    &(MemoriaCfg){.dst = console->text + console->used, .src = args->text + from, .bytes = piece});
            }
            console->text[console->used + piece] = '\0';
            console->used += piece + 1ULL;
        }
        from = end + (ended ? 1ULL : 0ULL);
    }
}

// the store a view's rows stand in
EMBED_INLINE CliDebugPaneStore *rows_store(const DebugPaneRowsCtx *args)
{
    switch (args->at)
    {
    case CLI_DEBUG_VIEW_FRAMES:
        return &g_cli_debug_pane.frames;
    case CLI_DEBUG_VIEW_VARIABLES:
        return &g_cli_debug_pane.variables;
    default:
        return &g_cli_debug_pane.console;
    }
}

unsigned int cli_debug_pane_rows(const CliDebugPaneViewCfg *args)
{
    const CliDebugPaneStore *const store = EMBED_CALL(rows_store, DebugPaneRowsCtx, .at = args->view);
    return ((store->count == 0u) && (args->view != CLI_DEBUG_VIEW_CONSOLE)) ? 1u : store->count;
}

// the entry of a store laid into the row: its texts pointed into the store's text cell
EMBED_INLINE void rows_laid(const DebugPaneRowsCtx *args)
{
    const CliDebugPaneEntry *const entry = args->entry;
    const char *const text = args->store->text;
    CliDebugPaneRow *const row = args->row;
    row->kind = entry->kind;
    row->depth = entry->depth;
    row->opens = entry->opens;
    row->open = entry->open;
    row->failed = entry->failed;
    row->line_kind = entry->line_kind;
    row->line = entry->line;
    row->text = text + entry->text;
    row->text_length = entry->text_length;
    row->second = (entry->second_length != 0ULL) ? text + entry->second : "";
    row->second_length = entry->second_length;
    row->third = (entry->third_length != 0ULL) ? text + entry->third : "";
    row->third_length = entry->third_length;
}

int cli_debug_pane_row(const CliDebugPaneViewCfg *args)
{
    CliDebugPaneStore *const store = EMBED_CALL(rows_store, DebugPaneRowsCtx, .at = args->view);
    CliDebugPaneRow *const row = args->row;
    *row = (CliDebugPaneRow){0};
    row->text = "";
    row->second = "";
    row->third = "";
    if ((store->count == 0u) && (args->view != CLI_DEBUG_VIEW_CONSOLE) && (args->at == 0u))
    {
        const char *const notice = (args->view == CLI_DEBUG_VIEW_VARIABLES) ? ROWS_VARIABLES_NONE
                                   : g_cli_debug_pane.has_session           ? ROWS_FRAMES_RUNNING
                                                                            : ROWS_FRAMES_NONE;
        row->kind = CLI_DEBUG_ROW_NOTICE;
        row->text = notice;
        row->text_length = cellul.len(&(CatenaFinitaCfg){.src = notice, .cap = 64ULL});
        return 1;
    }
    if (args->at >= store->count)
    {
        return 0;
    }
    const unsigned int place =
        (args->view == CLI_DEBUG_VIEW_CONSOLE) ? (store->first + args->at) % store->capacity : args->at;
    EMBED_CALL(rows_laid, DebugPaneRowsCtx, .store = store, .entry = &store->entry[place], .row = row);
    row->current = (args->view == CLI_DEBUG_VIEW_FRAMES) && (args->at == g_cli_debug_pane.frame_at);
    return 1;
}

int cli_debug_pane_toggle(const CliDebugPaneViewCfg *args)
{
    const CliDebugPaneStore *const variables = &g_cli_debug_pane.variables;
    if ((args->view != CLI_DEBUG_VIEW_VARIABLES) || (args->at >= variables->count))
    {
        return 0;
    }
    const CliDebugPaneEntry *const entry = &variables->entry[args->at];
    if (entry->kind == CLI_DEBUG_ROW_WATCH)
    {
        // the watches stand first, the row's place the watch's
        CliDebugPaneStore *const watches = &g_cli_debug_pane.watches;
        CliDebugPaneStore left = {0};
        for (unsigned int at = 0u; at < watches->count; at += 1u)
        {
            if (at == args->at)
            {
                continue;
            }
            (void)EMBED_CALL(rows_entry, DebugPaneRowsCtx, .store = &left,
                             .text = watches->text + watches->entry[at].text, .length = watches->entry[at].text_length);
        }
        EMBED_CALL(cli_debug_pane_internal.store_empty, CliDebugPaneCtx, .store = watches);
        *watches = left;
    }
    else if (entry->opens && (entry->key_length != 0ULL))
    {
        EMBED_CALL(rows_flip, DebugPaneRowsCtx, .text = variables->text + entry->key, .length = entry->key_length);
    }
    else
    {
        return 0;
    }
    EMBED_CALL(cli_debug_pane_internal.variables_read, CliDebugPaneCtx, .root = args->root);
    return 1;
}

int cli_debug_pane_press(const CliDebugPaneViewCfg *args)
{
    const CliDebugPaneStore *const frames = &g_cli_debug_pane.frames;
    if ((args->view != CLI_DEBUG_VIEW_FRAMES) || (args->at >= frames->count))
    {
        return 0;
    }
    g_cli_debug_pane.frame_at = args->at;
    EMBED_CALL(cli_debug_pane_internal.variables_read, CliDebugPaneCtx, .root = args->root);
    const CliDebugPaneEntry *const entry = &g_cli_debug_pane.frames.entry[args->at];
    const char *const path = g_cli_debug_pane.frames.text + entry->third;
    if ((entry->third_length == 0ULL) ||
        EMBED_CALL(rows_absolute, DebugPaneRowsCtx, .text = path, .length = entry->third_length))
    {
        return 0;
    }
    *args->open_out = (CliDebugPaneOpen){path, entry->line, 1};
    return 1;
}
