// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_pane_internal.h: the Database pane's state its four sources share, and the entries each lends the others: the
// connections with their labels, the schema's nodes over one text, the rows shown as places among the nodes, the
// sentence row, the form, the ask's owner and the run on its thread (db_pane.c, db_pane_tree.c, db_pane_act.c,
// db_form.c)
#ifndef DB_PANE_INTERNAL_H
#define DB_PANE_INTERNAL_H
#include "../text/typed.h"
#include "db_grid.h"
#include "db_pane.h"
#include "embed_atomic.h"

typedef struct CliDbPaneCtx CliDbPaneCtx;

// the form's fields: its name, host, port, user, database, file and the machine ssh goes through; its rows' places:
// a field, the type's choice, the matters box, the guard's choice, the buttons and the refusal; and the types and the
// guards a choice steps through
#define CLI_DB_FORM_NAME 0u
#define CLI_DB_FORM_HOST 1u
#define CLI_DB_FORM_PORT 2u
#define CLI_DB_FORM_USER 3u
#define CLI_DB_FORM_DATABASE 4u
#define CLI_DB_FORM_FILE 5u
#define CLI_DB_FORM_SSH 6u
#define CLI_DB_FORM_FIELDS 7u
#define CLI_DB_FORM_TYPE 7u
#define CLI_DB_FORM_MATTERS 8u
#define CLI_DB_FORM_GUARD 9u
#define CLI_DB_FORM_BUTTONS 10u
#define CLI_DB_FORM_REFUSAL 11u
#define CLI_DB_FORM_ROWS 12u
#define CLI_DB_FORM_TYPES 3u
#define CLI_DB_FORM_GUARDS 2u

// A node of the schema: its label's, its type's and its detail's places in the text with their bytes, its path's
// names' places, how many, its parent's place, CLI_DB_PANE_NONE at the top, its children's first place and how many,
// its depth, and whether it is a leaf, open, read and a failed level's sentence
typedef struct
{
    unsigned long long label;
    unsigned long long label_length;
    unsigned long long type;
    unsigned long long type_length;
    unsigned long long detail;
    unsigned long long detail_length;
    unsigned long long path[CLI_DB_PANE_PATH];
    unsigned int path_count;
    unsigned int parent;
    unsigned int first;
    unsigned int children;
    unsigned int depth;
    unsigned char leaf;
    unsigned char open;
    unsigned char read;
    unsigned char failed;
} CliDbPaneNode;

// The form: its fields, each a typed text in a movable cell taken once at its most, the type and the guard chosen,
// whether the connection matters, the field that has the keys, CLI_DB_PANE_NONE for none, whether it edits a connection
// and the name it had, whether the last save was refused and its refusal, its rows' places and how many, and whether
// it is open
typedef struct
{
    CliTyped field[CLI_DB_FORM_FIELDS];
    unsigned int type;
    unsigned int guard;
    int matters;
    unsigned int focused;
    int editing;
    char was[CLI_NAME_MAX_LEN];
    int refused;
    char refusal[CLI_ERR_MSG_MAX_LEN];
    unsigned long long refusal_length;
    unsigned int row_place[CLI_DB_FORM_ROWS];
    unsigned int rows;
    int open;
} CliDbPaneForm;

// The run on its thread: the thread, the connection it runs on, the statement in a cell sized to it, whether its ask
// was answered, its answer's cell and line, its failure, whether it ran, whether it stands, and whether its thread
// ended, set on that thread
typedef struct
{
    CliThread thread;
    CliDbOpen *open;
    unsigned int connection;
    char *statement;
    unsigned long long size;
    int answered;
    char *answer;
    VerbaLinea out;
    CliDbFailure failure;
    int ran;
    int standing;
    embed_atomic_u32 ended;
} CliDbPaneRunning;

// A connection's schema tree as the pane last showed it: its nodes and their text, each in a movable cell sized to
// them, and the rows shown, places among the nodes in a movable cell of their own. The chosen connection's stands in
// the pane itself; each other connection's is kept here while another is chosen, and is the tree the pane shows again
// when the connection is chosen again, its nodes open as they were left, as every database client shows a connection
// switched back to
typedef struct
{
    const CarcerMovableEntry *node;
    unsigned int nodes;
    const CarcerMovableEntry *text;
    unsigned long long text_used;
    const CarcerMovableEntry *shown;
    unsigned int shown_count;
    unsigned int top_count;
} CliDbPaneTree;

// the most bytes a connection's label takes with its end: its name, its type in parentheses and the open mark
#define CLI_DB_PANE_LABEL (CLI_NAME_MAX_LEN + 32u)

// the most bytes of a transaction's state with its end
#define CLI_DB_PANE_TRANSACTION 16u

// The pane: the connections, the open one of each, NULL where it is not, the transaction's state of each and the tree
// kept of each while another is chosen, in one movable cell sized to them, the connections from its start, the open
// ones from `open_at`, the transactions from `transaction_at` and the kept trees from `kept_at`, each read by copy
// and written in place; their labels in a movable cell sized to them, each
// label's place first and the labels' texts after, and the label last read out of it, the one chosen, CLI_DB_PANE_NONE
// for none, and its name; the schema's nodes and their text, each in a movable cell sized to them; the rows shown,
// places among the nodes in a movable cell; whether a sentence row stands, the sentence and whether it is a failure;
// the form; whether the ask stands
// for the pane, the password retried and the statement the ask confirms; the node the menu was built for; the run; and
// what wakes the view's thread
typedef struct
{
    const CarcerMovableEntry *connections_cell;
    unsigned long long open_at;
    unsigned long long transaction_at;
    unsigned long long kept_at;
    unsigned int connections;
    const CarcerMovableEntry *labels;
    char label[CLI_DB_PANE_LABEL];
    unsigned int chosen;
    char chosen_name[CLI_NAME_MAX_LEN];
    const CarcerMovableEntry *node;
    unsigned int nodes;
    const CarcerMovableEntry *text;
    unsigned long long text_used;
    const CarcerMovableEntry *shown;
    unsigned int shown_count;
    unsigned int top_count;
    int noted;
    char note[CLI_ERR_MSG_MAX_LEN];
    unsigned long long note_length;
    int note_failed;
    CliDbPaneForm form;
    int asking;
    unsigned int menu_node;
    char *confirming;
    unsigned long long confirming_size;
    CliDbPaneRunning run;
    const char *root;
    void (*woken)(void);
} CliDbPaneState;

extern CliDbPaneState g_cli_db_pane_state;

// the places in the connections cell of the connection `at_`, of its open one, of its transaction and of its kept tree
#define CLI_DB_PANE_CONNECTION_AT(at_) ((unsigned long long)(at_) * sizeof(CliDbConnection))
#define CLI_DB_PANE_OPEN_AT(at_) (g_cli_db_pane_state.open_at + ((unsigned long long)(at_) * sizeof(CliDbOpen *)))
#define CLI_DB_PANE_TRANSACTION_AT(at_)                                                                                \
    (g_cli_db_pane_state.transaction_at + ((unsigned long long)(at_) * CLI_DB_PANE_TRANSACTION))
#define CLI_DB_PANE_KEPT_AT(at_) (g_cli_db_pane_state.kept_at + ((unsigned long long)(at_) * sizeof(CliDbPaneTree)))

// the labels of the connections laid again in a movable cell sized to them, each its name, its type and the open mark;
// the connection `at`'s label read into the pane's label, cut to CLI_DB_PANE_LABEL bytes: its bytes, 0 for none; the
// sentence row set to `text`, a failure's where `failed`, none where `text` is NULL; the schema let go; the level
// under the node `at`, or the top where it is CLI_DB_PANE_NONE, read from the chosen connection's schema link and
// stored after the nodes, a failure stored as one failed node: 1, or 0 where the cells cannot meet it; the rows shown
// laid again from the open nodes; the form's rows laid again for its type; and the chosen connection opened, its
// password `text` where it is not NULL: 1, or 0 with the ask for its password opened where the server refused, else
// the sentence row its failure
void cli_db_pane_labels_laid(const CliDbPaneCtx *args);
unsigned long long cli_db_pane_label_read(const CliDbPaneCtx *args);
void cli_db_pane_note_set(const CliDbPaneCtx *args);
void cli_db_pane_schema_released(const CliDbPaneCtx *args);
// the chosen connection's tree moved into its kept place, the pane's left empty; and the connection `at`'s kept tree
// moved into the pane: 1 where one was kept, 0 where the pane is left as it stands
void cli_db_pane_tree_kept(const CliDbPaneCtx *args);
int cli_db_pane_tree_restored(const CliDbPaneCtx *args);
int cli_db_pane_level_read(const CliDbPaneCtx *args);
void cli_db_pane_shown_laid(const CliDbPaneCtx *args);
void cli_db_pane_form_laid(const CliDbPaneCtx *args);
int cli_db_pane_form_row(const CliDbPaneCtx *args);
int cli_db_pane_connected(const CliDbPaneCtx *args);
// the form opened, on the chosen connection where `editing`, new where not; its row `at` pressed on its button
// `choice` for the tree at `root`: 1 where a field took the keys; and closed, its cells released
void cli_db_pane_form_open(const CliDbPaneCtx *args);
int cli_db_pane_form_press(const CliDbPaneCtx *args);
void cli_db_pane_form_close(const CliDbPaneCtx *args);
struct CliDbPaneCtx
{
    const char *text;
    int failed;
    unsigned int at;
    unsigned int choice;
    const char *root;
    int editing;
    CliDbPaneRow *row;
};

typedef struct
{
    void (*labels_laid)(const CliDbPaneCtx *args);
    unsigned long long (*label_read)(const CliDbPaneCtx *args);
    void (*note_set)(const CliDbPaneCtx *args);
    void (*schema_released)(const CliDbPaneCtx *args);
    void (*tree_kept)(const CliDbPaneCtx *args);
    int (*tree_restored)(const CliDbPaneCtx *args);
    int (*level_read)(const CliDbPaneCtx *args);
    void (*shown_laid)(const CliDbPaneCtx *args);
    void (*form_laid)(const CliDbPaneCtx *args);
    int (*form_row)(const CliDbPaneCtx *args);
    int (*connected)(const CliDbPaneCtx *args);
    void (*form_open)(const CliDbPaneCtx *args);
    int (*form_press)(const CliDbPaneCtx *args);
    void (*form_close)(const CliDbPaneCtx *args);
} CliDbPaneInternalNs;
EMBED_TABLE_LAYOUT(CliDbPaneInternalNs, labels_laid, label_read, note_set, schema_released, tree_kept, tree_restored,
                   level_read, shown_laid, form_laid, form_row, connected, form_open, form_press, form_close);
EMBED_TABLE_STORAGE CliDbPaneInternalNs cli_db_pane_internal EMBED_UNUSED = {
    .labels_laid = cli_db_pane_labels_laid,
    .label_read = cli_db_pane_label_read,
    .note_set = cli_db_pane_note_set,
    .schema_released = cli_db_pane_schema_released,
    .tree_kept = cli_db_pane_tree_kept,
    .tree_restored = cli_db_pane_tree_restored,
    .level_read = cli_db_pane_level_read,
    .shown_laid = cli_db_pane_shown_laid,
    .form_laid = cli_db_pane_form_laid,
    .form_row = cli_db_pane_form_row,
    .connected = cli_db_pane_connected,
    .form_open = cli_db_pane_form_open,
    .form_press = cli_db_pane_form_press,
    .form_close = cli_db_pane_form_close,
};

#endif
