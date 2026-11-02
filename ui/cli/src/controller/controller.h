// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller.h: the window's controller. The window is a view: it writes input and draws what is published, and it asks
// nothing, waits for nothing, decides nothing and allocates nothing. The controller's thread owns the model, reads the
// input, runs every command whoever pressed what, and publishes each part the window draws as a snapshot in the CLI's
// cells, immutable once published. Nothing the CLI hands out comes back: a part's ring moves on as the window moves to
// a newer snapshot, and the controller releases each cell the ring's position shows it has passed. Work that blocks
// runs on workers, whose results come back to the controller and never to the window. Each queue is an anularis ring
// with one writer and one reader (controller.c)
#ifndef CONTROLLER_H
#define CONTROLLER_H
#include "../command_line/cli_internal.h"
#include "controller_parts.h"
#include "embed_atomic.h"
#include "embed_dispatch_layout.h"
#include "memoria_anularis/memoria_anularis.h"

// what an input is: a key by its name, text typed, the pointer pressed, moved or let go on what it hit, the wheel, the
// editor's size in rows and columns of its text, a file dropped on it, the system's close, a setting changed, its key
// and its value in the bytes with a 0 between, the lines the minimap stands for, from `row` to `column`, a command
// named in the bytes, and a key pressed in Preferences' window, taken only where it is that command's own
#define CLI_INPUT_KEY 0u
#define CLI_INPUT_TEXT 1u
#define CLI_INPUT_POINTER_DOWN 2u
#define CLI_INPUT_POINTER_MOVED 3u
#define CLI_INPUT_POINTER_UP 4u
#define CLI_INPUT_WHEEL 5u
#define CLI_INPUT_SIZE 6u
#define CLI_INPUT_DROP 7u
#define CLI_INPUT_CLOSE 8u
#define CLI_INPUT_SETTING 9u
#define CLI_INPUT_MAP_SPAN 10u
#define CLI_INPUT_COMMAND 11u
#define CLI_INPUT_PREFERENCES_KEY 12u

// what the pointer hit, as the window's layout places it; a key sent while the name's pane stands names the wordmark,
// no view taking it
#define CLI_HIT_NONE 0u
#define CLI_HIT_MENU_TITLE 1u
#define CLI_HIT_MENU_ROW 2u
#define CLI_HIT_TAB 3u
#define CLI_HIT_TAB_CLOSE 4u
#define CLI_HIT_TOOL 5u
#define CLI_HIT_EXPLORER_ROW 6u
#define CLI_HIT_PROBLEM_ROW 7u
#define CLI_HIT_PROBLEMS_CHECK 8u
#define CLI_HIT_SUGGEST_ROW 9u
#define CLI_HIT_MAP 10u
#define CLI_HIT_EDITOR 11u
#define CLI_HIT_GUTTER 12u
#define CLI_HIT_WORDMARK 13u
#define CLI_HIT_HOVER_BOX 14u
#define CLI_HIT_SIGNATURE_BOX 15u
#define CLI_HIT_SUGGEST_LIST 16u

// the modifier keys down on an input, as bits
#define CLI_INPUT_MOD_SHIFT 1u
#define CLI_INPUT_MOD_CONTROL 2u
#define CLI_INPUT_MOD_ALT 4u

// One input: its kind; what the pointer hit, with the level and the row of what it hit; the cell it stands on in the
// editor; how many presses in a row and the modifier keys; the wheel's rows; the window's rows and columns; and how
// many bytes of a name, text or path follow it on the input's bytes
typedef struct
{
    unsigned int kind;
    unsigned int hit;
    unsigned int level;
    unsigned int index;
    unsigned long long row;
    unsigned long long column;
    unsigned int clicks;
    unsigned int mods;
    int rows_moved;
    unsigned int rows;
    unsigned int columns;
    unsigned int length;
} CliInput;

// the parts the controller publishes
#define CLI_SNAPSHOT_FRAME 0u
#define CLI_SNAPSHOT_MAP 1u
#define CLI_SNAPSHOT_HOVER 2u
#define CLI_SNAPSHOT_COMPLETIONS 3u
#define CLI_SNAPSHOT_SIGNATURE 4u
#define CLI_SNAPSHOT_MENUS 5u
#define CLI_SNAPSHOT_TABS 6u
#define CLI_SNAPSHOT_EXPLORER 7u
#define CLI_SNAPSHOT_PROBLEMS 8u
#define CLI_SNAPSHOT_STATUS 9u
#define CLI_SNAPSHOT_SETTINGS 10u
#define CLI_SNAPSHOT_NAMES 11u
#define CLI_SNAPSHOT_PARTS 12u

// What wakes the view's thread, from any thread
typedef void (*CliControllerWoken)(void *context);

// the controller begun, its rings in cells of its own, its thread started and the view's thread woken through `woken`:
// 1, or 0 where the cells cannot meet the rings or the thread could not be started; and ended, its thread let end and
// every cell released
int cli_controller_begin(CliControllerWoken woken, void *context);
void cli_controller_end(void);
// from the view's thread alone: one input written with its `length` bytes, dropped where the rings cannot hold it; and
// the newest snapshot of `part` published, NULL where none has been, which stands until a later call for the part finds
// a newer
void cli_controller_input(const CliInput *input, const char *bytes);
const void *cli_controller_latest(unsigned int part);

// A place on the screen in the system's own pixels, and whether the window filled its monitor there
typedef struct
{
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    int maximized;
} CliWindowPosition;

// A monitor's area, in the system's own pixels
typedef struct
{
    int x;
    int y;
    unsigned int width;
    unsigned int height;
} CliWindowArea;

// the least size the window is dragged to, and a place saved is read back at, in pixels at a scale of one
#define CLI_POSITION_MIN_WIDTH 960u
#define CLI_POSITION_MIN_HEIGHT 600u

// the command line run on the words the program was started with, as the system saves them: the code to exit with, 1 in
// `window` where the words open the window instead (controller_tree.c)
int cli_controller_command_line(int *window);
// the tree the window opens on found and readied: orior_cli run on the machine where it is in a Flatpak's sandbox,
// every process it starts contained under it, the door opened for the command line and the tree watched; and every
// server, session and process of the window's ended, as it ends (controller_tree.c)
void cli_controller_tree_begin(void);
void cli_controller_tree_end(void);
// the place saved for the tree open, fitted to the monitors `areas`, `count` of them, the system's main one first, into
// `place`: 1, or 0 where none is saved, it is below the least size or there is no monitor; and `place` saved for the
// tree open, beside the places of the other trees, nothing saved where no tree is open (controller_place.c)
int cli_controller_place_saved(const CliWindowArea *areas, unsigned int count, CliWindowPosition *place);
void cli_controller_place_store(const CliWindowPosition *place);

#endif
