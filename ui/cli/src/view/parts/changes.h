// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// changes.h: the Commit tool's side pane table, the Changes: a row each file that differs from the last commit, read
// where the git module lists them (code/git.h), a box lit while the next commit takes the file, the file's mark, its
// name in its state's color, its folder quieter after it and its state's letter at the right; a row pressed on its box
// leaves the file out or takes it again, pressed elsewhere opens the file. Over the rows the box for the commit's
// message, which takes the keys, Commit beside it, and git's refusal under them. The changed file's row is drawn once
// here for every pane that lists files with their states (changes.c)
#ifndef GUI_PARTS_CHANGES_H
#define GUI_PARTS_CHANGES_H
#include "pane.h"

typedef struct GuiChangesFileCfg GuiChangesFileCfg;

// the sizes in the page's pixels: the gap before the state letter, the state letter's text size, the taken box's side
// and its corners' radius, the gap after it, the Commit button's padding, the refusal's row and its text size
#define GUI_CHANGES_GAP 8
#define GUI_CHANGES_STATE_SIZE 11u
#define GUI_CHANGES_BOX 11
#define GUI_CHANGES_BOX_RADIUS 2
#define GUI_CHANGES_BOX_GAP 6
#define GUI_CHANGES_BUTTON_PAD 10
#define GUI_CHANGES_REPORT 20
#define GUI_CHANGES_REPORT_SIZE 12u

// the color of a `state` letter: a changed file in the third mark's, one added or not yet tracked in the second's, one
// gone in the first's, and any other in the text's quieter color; and a changed file drawn in the pane's row from x:
// its state letter at the row's right padding in the state's color, bold in the code face, and before it the file
// through the explorer's file drawing, its name in `color` and its folder, `folder_length` bytes of `path`, quieter,
// both cut before the letter and to `clip`: the x after the name and folder
GuiColor gui_changes_state_color(const GuiChangesFileCfg *args);
float gui_changes_file_draw(const GuiChangesFileCfg *args);
struct GuiChangesFileCfg
{
    const GuiPaneCfg *pane;
    int x;
    const char *path;
    const char *name;
    unsigned long long name_length;
    unsigned long long folder_length;
    char state;
    GuiColor color;
    GuiRect clip;
};

typedef struct
{
    GuiColor (*state_color)(const GuiChangesFileCfg *args);
    float (*file_draw)(const GuiChangesFileCfg *args);
} GuiChangesFileNs;
EMBED_TABLE_LAYOUT(GuiChangesFileNs, state_color, file_draw);
EMBED_TABLE_STORAGE GuiChangesFileNs gui_changes_file EMBED_UNUSED = {
    .state_color = gui_changes_state_color,
    .file_draw = gui_changes_file_draw,
};

// the Commit tool's pane table
extern const GuiPaneTable gui_changes_table;

#endif
