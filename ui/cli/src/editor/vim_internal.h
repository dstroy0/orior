// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vim_internal.h: what Vim's files share: the state, the views with Vim's keys as rows of one cell in the order of
// their keys, every text a part in a movable cell of its own bytes, the command parsed from the keys typed, where a
// motion goes and the span an object or a selection takes, and the steps each file lends the others (vim.c,
// vim_parse.c, vim_motions.c, vim_objects.c, vim_commands.c, vim_visual.c)
#ifndef VIM_INTERNAL_H
#define VIM_INTERNAL_H

#include "../command_line/pool.h"
#include "view.h"
#include "vim.h"

typedef struct CliVimCtx CliVimCtx;

// the registers: " first, then 0, then a to z
#define CLI_VIM_REGISTERS 28u
#define CLI_VIM_REGISTER_UNNAMED 0u
#define CLI_VIM_REGISTER_YANKED 1u
#define CLI_VIM_REGISTER_LETTERS 2u
// a view's texts among the parts, after the registers' and in the order of the views: its key, the keys typed of the
// command so far, the label, the prompt's text, the last search's text, the last change's keys and the input typed in
// insert mode after them, and those of the change being taken
#define CLI_VIM_PART_KEY 0u
#define CLI_VIM_PART_TYPED 1u
#define CLI_VIM_PART_LABEL 2u
#define CLI_VIM_PART_PROMPT 3u
#define CLI_VIM_PART_SEARCH 4u
#define CLI_VIM_PART_LAST_KEYS 5u
#define CLI_VIM_PART_LAST_INSERT 6u
#define CLI_VIM_PART_TAKING_KEYS 7u
#define CLI_VIM_PART_TAKING_INSERT 8u
#define CLI_VIM_VIEW_PARTS 9u
// a key typed in insert mode among the input a change takes stands as its name between these two bytes, which no
// text typed carries
#define CLI_VIM_KEY_OPEN '\x01'
#define CLI_VIM_KEY_CLOSE '\x02'
// the keys no letter carries, as the control bytes their chords type: Escape, Ctrl+R, Ctrl+D and Ctrl+U; and gg among
// the motions, past every letter of the basic plane
#define CLI_VIM_ESCAPE 0x1bu
#define CLI_VIM_REDO 0x12u
#define CLI_VIM_DOWN 0x04u
#define CLI_VIM_UP 0x15u
#define CLI_VIM_TOP 0x10000u
// the letter a place past a line's end reads as
#define CLI_VIM_LINE_END 0x0au
// the most bytes the keys of one command take, past which they are dropped
#define CLI_VIM_TYPED_MOST 64ULL

// A text: its movable cell, sized to its bytes, NULL where it is empty; and its bytes
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned long long length;
} CliVimPart;

// A view with Vim's keys: whether they are on; its mode; the column moves up and down go back to, where one is set;
// visual mode's anchor and head; whether the prompt is open and searches back; whether a search stands, back, as a
// whole word; the last f F t T with its letter, 0 for none; whether a change is being taken for `.` and whether `.`
// is replaying one
typedef struct
{
    int on;
    unsigned int mode;
    int goal_set;
    unsigned long long goal;
    CliPosition visual_anchor;
    CliPosition visual_head;
    int prompt_open;
    int prompt_back;
    int searched;
    int searched_back;
    int searched_word;
    unsigned int found_how;
    unsigned int found_letter;
    int taking;
    int replaying;
} CliVimView;

// the state's mosts, each the size of one movable cell taken once as Vim begins: the views with Vim's keys, and the
// parts, the registers' and every view's; and the bytes the shape's label and prompt take with their ends
#define CLI_VIM_VIEWS 64u
#define CLI_VIM_PARTS (CLI_VIM_REGISTERS + (CLI_VIM_VIEWS * CLI_VIM_VIEW_PARTS))
#define CLI_VIM_LABEL_MAX 96u
#define CLI_VIM_PROMPT_MAX 256u

// The module's state: the parts' cell and how many parts stand, the registers' then each view's, each part's text a
// cell of its own; the views' cell, in the order of their keys, and how many; each register's whether it
// takes whole lines; the setting; the label's and the prompt's bytes the shape points into; and the shape the window
// reads. A view or a part is read whole into a copy and a field written back alone; a text is copied out
typedef struct
{
    const CarcerMovableEntry *part;
    unsigned int parts;
    const CarcerMovableEntry *view;
    unsigned int views;
    unsigned char register_lines[CLI_VIM_REGISTERS];
    int on;
    char label_text[CLI_VIM_LABEL_MAX];
    char prompt_text[CLI_VIM_PROMPT_MAX];
    CliVimShape shape;
} CliVimState;

extern CliVimState g_cli_vim_state;

// A command parsed from the keys typed: whether it is none, waits for more keys or is whole; its register's letter, 0
// for none; whether a count was typed and the count; its command or operator's letter, 0 for a motion alone; its
// motion's letter, CLI_VIM_TOP for gg; the letter after f F t T or r; the text object's i or a and the letter after
// it; whether a doubled operator takes whole lines; and whether it acts on visual mode's selection
#define CLI_VIM_PARSE_NONE 0u
#define CLI_VIM_PARSE_WAIT 1u
#define CLI_VIM_PARSE_WHOLE 2u
typedef struct
{
    unsigned int status;
    unsigned int register_name;
    int counted;
    unsigned long long count;
    unsigned int name;
    unsigned int motion;
    unsigned int letter;
    unsigned int object_type;
    unsigned int object_of;
    int lines;
    int chosen;
} CliVimCommand;

// Where a motion goes: whether it goes anywhere, the place, whether it takes the letter it ends on, whether it takes
// whole lines, whether the column moves go back to stands, and where an inclusive motion's span ends where it ends
// past the letter
typedef struct
{
    int found;
    CliPosition to;
    int inclusive;
    int linewise;
    int goal_stays;
    int has_end;
    CliPosition end;
} CliVimFound;

// A span of the text: whether one stands, its start, the place after it, and whether it takes whole lines
typedef struct
{
    int found;
    CliPosition start;
    CliPosition end;
    int lines;
} CliVimSpan;

struct CliVimCtx
{
    unsigned int at;
    const char *key;
    unsigned int part;
    int on;
    const char *required;
    char *bytes_out;
    const char *text;
    unsigned long long length;
    unsigned long long *length_out;
    unsigned long long line;
    unsigned long long count;
    int counted;
    CliPosition place;
    CliPosition from;
    CliPosition to;
    CliPosition *place_out;
    unsigned int letter;
    unsigned int how;
    unsigned int name;
    unsigned int mode;
    unsigned int register_name;
    int big;
    int back;
    int lines;
    int yanked;
    int goal_stays;
    int word_text;
    const CliVimCommand *command;
    CliVimCommand *command_out;
    CliVimFound *found;
    CliVimSpan *span;
    const CliVimSpan *span_read;
    VerbaLinea *err_msg;
    const unsigned short *units;
};

// the texts (vim.c): the part `part` written anew with `length` bytes of `text` in a cell of its own, its old cell
// given back: 1, or 0 where the cells cannot meet the text, the part as it was; the bytes of the part `part`; the
// part's bytes copied out into `bytes_out` of `length` bytes with their end: how many were copied; the part's bytes
// copied out whole with their end into a cell of the temporary tier, their count into `length_out`, which the caller
// gives back: the cell, or NULL where the cells cannot meet it; the view `at`'s part `part`'s place among the parts;
// the view `at`'s mode set to `mode`, the label following; the label written anew from the mode and the keys typed; and
// the code point `letter` written as UTF-8 into `bytes_out`, four bytes at the most: how many
int cli_vim_part_put(const CliVimCtx *args);
unsigned long long cli_vim_part_length(const CliVimCtx *args);
unsigned long long cli_vim_part_copy(const CliVimCtx *args);
char *cli_vim_part_alloc(const CliVimCtx *args);
unsigned int cli_vim_part_of(const CliVimCtx *args);
void cli_vim_mode_set(const CliVimCtx *args);
void cli_vim_label_settled(const CliVimCtx *args);
unsigned long long cli_vim_letter_written(const CliVimCtx *args);
// the keys typed `length` bytes of `text`, read as a command, into `command_out` (vim_parse.c)
void cli_vim_parse(const CliVimCtx *args);
// the text read through the view `key` (vim_motions.c): how many lines; the units of the line `line`, their count into
// `length_out`; the letter at `place`, CLI_VIM_LINE_END past its line's end; the place a letter on from `place`,
// across a line's end, and a letter back: 1 with it into `place_out`, or 0 at the text's end or start; a letter's type,
// 0 for a blank or a line's end, 1 for a word's letter and 2 for any other, with `big` 1 for every letter but a blank;
// a line's first letter that is not blank, its last letter where all are; its last letter; the place Vim's cursor
// stands at, the selection's head in visual mode; the cursor put at `place`, the line and the column cut to the text,
// on a letter outside insert mode, the column moves go back to let go unless `goal_stays`; where the motion `name` with
// `letter`, counted `count` times where `counted`, goes from `from`, into `found`; and the cursor or visual mode's head
// moved by it
unsigned long long cli_vim_lines(const CliVimCtx *args);
const unsigned short *cli_vim_line(const CliVimCtx *args);
void cli_vim_line_end(const CliVimCtx *args);
unsigned int cli_vim_letter_at(const CliVimCtx *args);
int cli_vim_forward(const CliVimCtx *args);
int cli_vim_backward(const CliVimCtx *args);
unsigned int cli_vim_type(const CliVimCtx *args);
unsigned long long cli_vim_first_letter(const CliVimCtx *args);
unsigned long long cli_vim_last_col(const CliVimCtx *args);
CliPosition cli_vim_head(const CliVimCtx *args);
void cli_vim_put(const CliVimCtx *args);
void cli_vim_motion(const CliVimCtx *args);
void cli_vim_move_by(const CliVimCtx *args);
// the span the text object `how`, i or a, with the letter `letter` takes about `from`, `count` levels out for a
// bracket, into `span` (vim_objects.c)
void cli_vim_object(const CliVimCtx *args);
// the operators and commands (vim_commands.c): the operator `name` applied to the span `span_read` with the register
// `register_name`; a command with an operator applied to its motion, its object or its lines; the text `length` bytes
// of `text` stored in the register `register_name`, " and with `yanked` 0 taking it too, a capital adding to its
// letter's; insert mode entered at `place`; insert mode left, the cursor a letter back; the case of each letter from
// `from` to `to` turned; `count` lines joined from `line`; a command run; and the search for `length` bytes of
// `text`, back where `back`, as a whole word where `word_text`, the cursor put where it finds
void cli_vim_apply(const CliVimCtx *args);
void cli_vim_operate(const CliVimCtx *args);
void cli_vim_store(const CliVimCtx *args);
void cli_vim_insert_at(const CliVimCtx *args);
void cli_vim_insert_left(const CliVimCtx *args);
void cli_vim_case_turned(const CliVimCtx *args);
void cli_vim_join(const CliVimCtx *args);
void cli_vim_command_run(const CliVimCtx *args);
void cli_vim_search(const CliVimCtx *args);
// visual mode (vim_visual.c): the span the selection takes into `span`, whole lines in visual line mode, and from one
// end's letter to the other's, both taken, in visual mode; the view's selection set to it; visual mode left, the
// cursor at `place`; and a command acting on the selection run
void cli_vim_visual_span(const CliVimCtx *args);
void cli_vim_chosen_drawn(const CliVimCtx *args);
void cli_vim_visual_left(const CliVimCtx *args);
void cli_vim_chosen_run(const CliVimCtx *args);

// the state's steps and the reading of the keys typed (vim.c, vim_parse.c)
typedef struct
{
    int (*part_put)(const CliVimCtx *args);
    unsigned long long (*part_length)(const CliVimCtx *args);
    unsigned long long (*part_copy)(const CliVimCtx *args);
    char *(*part_alloc)(const CliVimCtx *args);
    unsigned int (*part_of)(const CliVimCtx *args);
    void (*mode_set)(const CliVimCtx *args);
    void (*label_settled)(const CliVimCtx *args);
    unsigned long long (*letter_written)(const CliVimCtx *args);
    void (*parse)(const CliVimCtx *args);
} CliVimNsInternal;
EMBED_TABLE_LAYOUT(CliVimNsInternal, part_put, part_length, part_copy, part_alloc, part_of, mode_set, label_settled,
                   letter_written, parse);
EMBED_TABLE_STORAGE CliVimNsInternal cli_vim_internal EMBED_UNUSED = {
    .part_put = cli_vim_part_put,
    .part_length = cli_vim_part_length,
    .part_copy = cli_vim_part_copy,
    .part_alloc = cli_vim_part_alloc,
    .part_of = cli_vim_part_of,
    .mode_set = cli_vim_mode_set,
    .label_settled = cli_vim_label_settled,
    .letter_written = cli_vim_letter_written,
    .parse = cli_vim_parse,
};

// the text as Vim reads it, the motions and the objects (vim_motions.c, vim_objects.c)
typedef struct
{
    unsigned long long (*lines)(const CliVimCtx *args);
    const unsigned short *(*line)(const CliVimCtx *args);
    void (*line_end)(const CliVimCtx *args);
    unsigned int (*letter_at)(const CliVimCtx *args);
    int (*forward)(const CliVimCtx *args);
    int (*backward)(const CliVimCtx *args);
    unsigned int (*type)(const CliVimCtx *args);
    unsigned long long (*first_letter)(const CliVimCtx *args);
    unsigned long long (*last_col)(const CliVimCtx *args);
    CliPosition (*head)(const CliVimCtx *args);
    void (*put)(const CliVimCtx *args);
    void (*motion)(const CliVimCtx *args);
    void (*move_by)(const CliVimCtx *args);
    void (*object)(const CliVimCtx *args);
} CliVimMotionsNsInternal;
EMBED_TABLE_LAYOUT(CliVimMotionsNsInternal, lines, line, line_end, letter_at, forward, backward, type, first_letter,
                   last_col, head, put, motion, move_by, object);
EMBED_TABLE_STORAGE CliVimMotionsNsInternal cli_vim_motions_internal EMBED_UNUSED = {
    .lines = cli_vim_lines,
    .line = cli_vim_line,
    .line_end = cli_vim_line_end,
    .letter_at = cli_vim_letter_at,
    .forward = cli_vim_forward,
    .backward = cli_vim_backward,
    .type = cli_vim_type,
    .first_letter = cli_vim_first_letter,
    .last_col = cli_vim_last_col,
    .head = cli_vim_head,
    .put = cli_vim_put,
    .motion = cli_vim_motion,
    .move_by = cli_vim_move_by,
    .object = cli_vim_object,
};

// the operators, the commands and visual mode (vim_commands.c, vim_visual.c)
typedef struct
{
    void (*apply)(const CliVimCtx *args);
    void (*operate)(const CliVimCtx *args);
    void (*store)(const CliVimCtx *args);
    void (*insert_at)(const CliVimCtx *args);
    void (*insert_left)(const CliVimCtx *args);
    void (*case_turned)(const CliVimCtx *args);
    void (*join)(const CliVimCtx *args);
    void (*command_run)(const CliVimCtx *args);
    void (*search)(const CliVimCtx *args);
    void (*visual_span)(const CliVimCtx *args);
    void (*chosen_drawn)(const CliVimCtx *args);
    void (*visual_left)(const CliVimCtx *args);
    void (*chosen_run)(const CliVimCtx *args);
} CliVimCommandsNsInternal;
EMBED_TABLE_LAYOUT(CliVimCommandsNsInternal, apply, operate, store, insert_at, insert_left, case_turned, join,
                   command_run, search, visual_span, chosen_drawn, visual_left, chosen_run);
EMBED_TABLE_STORAGE CliVimCommandsNsInternal cli_vim_commands_internal EMBED_UNUSED = {
    .apply = cli_vim_apply,
    .operate = cli_vim_operate,
    .store = cli_vim_store,
    .insert_at = cli_vim_insert_at,
    .insert_left = cli_vim_insert_left,
    .case_turned = cli_vim_case_turned,
    .join = cli_vim_join,
    .command_run = cli_vim_command_run,
    .search = cli_vim_search,
    .visual_span = cli_vim_visual_span,
    .chosen_drawn = cli_vim_chosen_drawn,
    .visual_left = cli_vim_visual_left,
    .chosen_run = cli_vim_chosen_run,
};

#endif
