// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_pane.h: the Database tool's left column as the window reads it. The connections the tree's databases.json lists,
// the one chosen and the ones open; under the chosen one, when it is open, its schema as rows, a node a row, each
// level read from the connection's schema link as its node first opens and standing until it is read again; the
// connection form in place of the schema while it is open; and the one ask in place of either while the pane asks.
// Over the rows the head: the connection the pane works on and the bar of tools with their labels and whether each can
// act. A statement runs on a thread of its own, its answer handed to the view's thread through a wake and adopted by
// the grid as the step collects it (db_pane.c, db_pane_tree.c, db_pane_act.c, db_form.c)
#ifndef DB_PANE_H
#define DB_PANE_H
#include "../command_line/commands.h"
#include "../command_line/prompt.h"
#include "db.h"
#include "embed_dispatch_layout.h"

typedef struct CliDbPaneCfg CliDbPaneCfg;
typedef struct CliDbPaneHeadCfg CliDbPaneHeadCfg;
typedef struct CliDbPaneTreeCfg CliDbPaneTreeCfg;
typedef struct CliDbPaneInputCfg CliDbPaneInputCfg;

// no place; the most names of a node's path; the rows of a page a statement's first answer and each later read
// carry; and the most bytes of one statement's answer
#define CLI_DB_PANE_NONE 0xFFFFFFFFu
#define CLI_DB_PANE_PATH 3u
#define CLI_DB_PANE_PAGE 200ULL
#define CLI_DB_PANE_ANSWER (1ULL << 22u)

// the rows' kinds: a sentence, a node of the schema, a field of the form, a choice of the form stepped by a press, a
// box of the form turned by a press, the form's buttons, and a row of the ask
#define CLI_DB_PANE_ROW_NOTE 0u
#define CLI_DB_PANE_ROW_NODE 1u
#define CLI_DB_PANE_ROW_FIELD 2u
#define CLI_DB_PANE_ROW_CHOICE 3u
#define CLI_DB_PANE_ROW_BOX 4u
#define CLI_DB_PANE_ROW_BUTTONS 5u
#define CLI_DB_PANE_ROW_PROMPT 6u

// the head's rows; and its actions, 0 standing for none: the connection the pane works on, stepped on to the next,
// then the bar's tools from the left, Connect or Disconnect, New, Edit, Read Again, Stop Reading, Stop, Commit and
// Roll Back
#define CLI_DB_PANE_HEAD_ROWS 2u
#define CLI_DB_PANE_PICK 1u
#define CLI_DB_PANE_CONNECT 2u
#define CLI_DB_PANE_NEW 3u
#define CLI_DB_PANE_EDIT 4u
#define CLI_DB_PANE_READ 5u
#define CLI_DB_PANE_STOP_READING 6u
#define CLI_DB_PANE_STOP 7u
#define CLI_DB_PANE_COMMIT 8u
#define CLI_DB_PANE_ROLLBACK 9u
#define CLI_DB_PANE_FIRST_TOOL CLI_DB_PANE_CONNECT
#define CLI_DB_PANE_LAST_TOOL CLI_DB_PANE_ROLLBACK

// A row of the pane: its kind and its depth in the schema; its text and its bytes, a node's label, a sentence, or a
// form row's label; its detail and its bytes, a node's kind or column type, a field's text, NULL for a password's, or
// a choice's word; whether a node opens and whether it is open, whether a sentence or a level is a failure, whether a
// field has the keys, whether a box is on, how many letters a password's field carries, and an ask's row or the form's
// buttons
typedef struct
{
    unsigned int kind;
    unsigned int depth;
    const char *text;
    unsigned long long text_length;
    const char *detail;
    unsigned long long detail_length;
    int opens;
    int open;
    int failed;
    int focused;
    int on;
    int secret;
    unsigned long long letters;
    CliPromptRow prompt;
} CliDbPaneRow;

// the pane begun, `woken` waking the view's thread as a run on its own thread ends; the connections of the tree at
// `root` read, each with whether it is open, the one chosen standing where the tree still lists it; the pane's title,
// the connection it works on with its open mark, with its bytes into `length_out`; how many rows the pane has, a
// number that changes as its data does; the row `at` into `row`: 1, or 0 past the last; and the step at `now_ms`: a
// run its thread ended adopted into the grid, its transaction's state read, its ask opened where it confirms first: 1
// where anything changed
void cli_db_pane_begin(const CliDbPaneCfg *args);
void cli_db_pane_read(const CliDbPaneCfg *args);
const char *cli_db_pane_title(const CliDbPaneCfg *args);
unsigned int cli_db_pane_rows(const CliDbPaneCfg *args);
int cli_db_pane_row(const CliDbPaneCfg *args);
int cli_db_pane_step(const CliDbPaneCfg *args);
// a statement of `size` bytes of `statement` run on the chosen connection on a thread of its own, connecting first
// where it is not open, `answered` where its ask was answered already: 1 where it started, 0 where no connection is
// open or a run stands; and whether a run stands
int cli_db_pane_run(const CliDbPaneCfg *args);
int cli_db_pane_running(const CliDbPaneCfg *args);
struct CliDbPaneCfg
{
    void (*woken)(void);
    const char *root;
    unsigned int at;
    CliDbPaneRow *row;
    unsigned long long *length_out;
    unsigned long long now_ms;
    const char *statement;
    unsigned long long size;
    int answered;
};

typedef struct
{
    void (*begin)(const CliDbPaneCfg *args);
    void (*read)(const CliDbPaneCfg *args);
    const char *(*title)(const CliDbPaneCfg *args);
    unsigned int (*rows)(const CliDbPaneCfg *args);
    int (*row)(const CliDbPaneCfg *args);
    int (*step)(const CliDbPaneCfg *args);
    int (*run)(const CliDbPaneCfg *args);
    int (*running)(const CliDbPaneCfg *args);
} CliDbPaneNs;
EMBED_TABLE_LAYOUT(CliDbPaneNs, begin, read, title, rows, row, step, run, running);
EMBED_TABLE_STORAGE CliDbPaneNs cli_db_pane EMBED_UNUSED = {
    .begin = cli_db_pane_begin,
    .read = cli_db_pane_read,
    .title = cli_db_pane_title,
    .rows = cli_db_pane_rows,
    .row = cli_db_pane_row,
    .step = cli_db_pane_step,
    .run = cli_db_pane_run,
    .running = cli_db_pane_running,
};

// how many rows the head has; the label of the head's action `which` with its bytes into `length_out` and whether it
// can act into `enabled_out`, Connect while the connection chosen is not open and Disconnect while it is, Edit while
// one is chosen, Commit and Roll Back while its transaction is open or failed, Stop while a statement runs, Stop
// Reading while a level is read; and the action `which` taken for the tree at `root`: the next connection chosen, the
// chosen one connected, with the ask for its password where its server refuses the one it has, or disconnected, the
// form opened new or on the chosen one, the schema read again, a run stopped, the transaction committed or rolled back
unsigned int cli_db_pane_head_rows(const CliDbPaneHeadCfg *args);
const char *cli_db_pane_head_tool(const CliDbPaneHeadCfg *args);
void cli_db_pane_action(const CliDbPaneHeadCfg *args);
struct CliDbPaneHeadCfg
{
    unsigned int which;
    unsigned long long *length_out;
    int *enabled_out;
    const char *root;
};

typedef struct
{
    unsigned int (*head_rows)(const CliDbPaneHeadCfg *args);
    const char *(*head_tool)(const CliDbPaneHeadCfg *args);
    void (*action)(const CliDbPaneHeadCfg *args);
} CliDbPaneHeadNs;
EMBED_TABLE_LAYOUT(CliDbPaneHeadNs, head_rows, head_tool, action);
EMBED_TABLE_STORAGE CliDbPaneHeadNs cli_db_pane_head EMBED_UNUSED = {
    .head_rows = cli_db_pane_head_rows,
    .head_tool = cli_db_pane_head_tool,
    .action = cli_db_pane_action,
};

// the row `at` pressed, on its button `choice` where it has buttons, for the tree at `root`: a node opened, its level
// read as it first opens, or closed; a form's field given the keys, its choice stepped, its box turned, its button
// pressed; an ask's button answered: 1 where the pane takes the keys; the menu of the row `at`, or of the pane where
// `at` is CLI_DB_PANE_NONE, built as the menus' own rows: a node's Copy Name, Show Its First Rows for a table, a view,
// a materialized view or a foreign table, and Read Again, and the pane's own the connections to choose among: 1 where
// it has any; and the command `name` with its `count` `words` run for the tree at `root`: db-choose, db-copy-name,
// db-first-rows and db-read-again: 1 where it is the pane's
int cli_db_pane_press(const CliDbPaneTreeCfg *args);
int cli_db_pane_menu(const CliDbPaneTreeCfg *args);
int cli_db_pane_command(const CliDbPaneTreeCfg *args);
struct CliDbPaneTreeCfg
{
    unsigned int at;
    unsigned int choice;
    const char *root;
    const char *name;
    const char *const *words;
    unsigned int count;
};

typedef struct
{
    int (*press)(const CliDbPaneTreeCfg *args);
    int (*menu)(const CliDbPaneTreeCfg *args);
    int (*command)(const CliDbPaneTreeCfg *args);
} CliDbPaneTreeNs;
EMBED_TABLE_LAYOUT(CliDbPaneTreeNs, press, menu, command);
EMBED_TABLE_STORAGE CliDbPaneTreeNs cli_db_pane_tree EMBED_UNUSED = {
    .press = cli_db_pane_press,
    .menu = cli_db_pane_menu,
    .command = cli_db_pane_command,
};

// `size` bytes of `text` typed, a UTF-8 run, into the ask while the pane asks, else into the form's field that has
// the keys; and the key `name` pressed, the ask's keys while it asks, else the form's: Backspace erases the field's
// last letter, Tab hands the keys to the next field, Enter saves the form, Escape closes it: 1 where it was taken
void cli_db_pane_type(const CliDbPaneInputCfg *args);
int cli_db_pane_key(const CliDbPaneInputCfg *args);
struct CliDbPaneInputCfg
{
    const char *text;
    unsigned long long size;
    const char *name;
    const char *root;
};

typedef struct
{
    void (*type)(const CliDbPaneInputCfg *args);
    int (*key)(const CliDbPaneInputCfg *args);
} CliDbPaneInputNs;
EMBED_TABLE_LAYOUT(CliDbPaneInputNs, type, key);
EMBED_TABLE_STORAGE CliDbPaneInputNs cli_db_pane_input EMBED_UNUSED = {
    .type = cli_db_pane_type,
    .key = cli_db_pane_key,
};

#endif
