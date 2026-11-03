// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_model.c: the model: what every input means, and the parts published. An input changes the model and marks
// the parts it moved dirty; once no input waits each dirty part is read into a snapshot and published, so a burst of
// inputs publishes once. The walk's colors, the lanes' answers and the model's timers come through here the same way,
// on the controller's thread
#include "controller_internal.h"
#include "controller_work.h"

#include "../command_line/cells.h"
#include "../editor/view.h"
#include "../machine/report.h"
#include "../machine/state.h"
#include "../text/text.h"

// the milliseconds between readings of what orior holds in RAM; orior's repository, a press on the name opening it; the
// settings orior's folder saves; and the opacities and zoom of the scheme
#define CLI_MODEL_MEMORY_MS 2000ULL
#define CLI_MODEL_REPOSITORY "https://github.com/dstroy0/orior"
#define CLI_MODEL_REDUCE_KEY "reduce-motion"
#define CLI_MODEL_ZOOM_KEY "zoom"
#define CLI_MODEL_BAR_KEY "opacity.bar"
#define CLI_MODEL_EDITOR_KEY "opacity.editor"
#define CLI_MODEL_COLOR_KEY "color."
#define CLI_MODEL_WORDMARK_KEY "wordmark.session"
#define CLI_MODEL_BAR_PERCENT 30u
#define CLI_MODEL_EDITOR_PERCENT 80u
#define CLI_MODEL_ZOOM_PERCENT 100u
// the most bytes of a log in; and two starts of a session, the seconds their boot times may read apart
#define CLI_MACHINE_LOGON_MAX_LEN 128u
#define CLI_MODEL_SESSION_SECONDS 60ULL

CliModel g_cli_model;

// the tools, the top group then the foot group
const CliTool g_cli_tools[] = {
    {"explorer", "folder", "Explorer", "Ctrl+Shift+E", CLI_TOOL_TOP, 0, 1},
    {"search", "search", "Search", "Ctrl+Shift+F", CLI_TOOL_TOP, 0, 1},
    {"structure", "structure", "Structure", "", CLI_TOOL_TOP, 0, 1},
    {"commit", "commit", "Commit", "", CLI_TOOL_TOP, 0, 1},
    {"more", "more", "More Tool Windows", "", CLI_TOOL_TOP, 1, 0},
    {"run", "run", "Run", "Ctrl+Shift+D", CLI_TOOL_FOOTER, 0, 0},
    {"debug", "debug", "Debug", "Alt+5", CLI_TOOL_FOOTER, 0, 0},
    {"terminal", "terminal", "Terminal", "Ctrl+`", CLI_TOOL_FOOTER, 0, 0},
    {"profile", "profile", "Profile", "", CLI_TOOL_FOOTER, 0, 0},
    {"containers", "containers", "Containers", "", CLI_TOOL_FOOTER, 0, 0},
    {"database", "database", "Database", "", CLI_TOOL_FOOTER, 0, 0},
    {"comms", "comms", "Comms", "", CLI_TOOL_FOOTER, 0, 0},
    {"problems", "problems", "Problems", "", CLI_TOOL_FOOTER, 0, 1},
    {"tests", "tests", "Tests", "", CLI_TOOL_FOOTER, 0, 1},
    {"git", "git", "Git", "", CLI_TOOL_FOOTER, 0, 1},
    {"runs", "runs", "Runs", "", CLI_TOOL_FOOTER, 0, 1},
};
const unsigned int g_cli_tools_count = sizeof(g_cli_tools) / sizeof(g_cli_tools[0]);

// whether the walk colored a line since it was last looked at, set on the walk's threads; the colors saved in orior's
// folder, a cell the settings' snapshot copies; whether the pointer drags in the editor; and whether a reading of
// memory is under way
static embed_atomic_u32 s_colored;
static CliSettingColor *s_color;
static unsigned int s_colors;
static int s_dragging;
static int s_memory_reading;

typedef struct
{
    const CliInput *input;
    const char *bytes;
    const char *key;
    const char *value;
    unsigned int pointer;
    unsigned int part;
    const CliStateEntries *entries;
    unsigned int *percent;
} ControllerModelCtx;

// the walk has colored lines: the frame and the map are read again as nothing else waits, from any of the walk's
// threads
static void model_colored(void)
{
    EMBED_ATOMIC_STORE(&s_colored, 1u);
    cli_controller_wake();
}

void cli_controller_snapshot_release(unsigned int part, void *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }
    if (part == CLI_SNAPSHOT_FRAME)
    {
        cli_view_frame_release((CliViewFrame *)snapshot);
    }
    else if (part == CLI_SNAPSHOT_MAP)
    {
        cli_view_map_release((CliDocumentMap *)snapshot);
    }
    else if (part == CLI_SNAPSHOT_TERMINAL)
    {
        cli_model_terminal_release((CliTerminalPanel *)snapshot);
    }
    else
    {
        g_cli_cells->persistent_buf_release(snapshot, &g_cli_controller_cells);
    }
}

void cli_model_editing(const CliModelCtx *args)
{
    const int on = args->on && (g_cli_model.active >= 0);
    if (g_cli_model.editing != on)
    {
        g_cli_model.editing = on;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS);
    }
}

void cli_model_view_input(const CliModelCtx *args)
{
    const char *const key = EMBED_CALL(cli_model.key, CliModelCtx, .at = 0u);
    if ((key == NULL) || !g_cli_model.tab[g_cli_model.active]->viewed)
    {
        return;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key,
                     .input = (const CliViewInput *)args->input, .err_msg = &err_msg);
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_FRAME);
    if (args->on)
    {
        // the input may have edited the text, which the map's lines then no longer show, and the server handed the change
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_MAP);
        EMBED_CALL(cli_model.server_changed, CliModelCtx, .at = 0u);
    }
}

void cli_model_tool_toggle(const CliModelCtx *args)
{
    if ((args->at >= g_cli_tools_count) || g_cli_tools[args->at].menu)
    {
        return;
    }
    // the terminal's panel stands under both views beside whatever tool is shown
    if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_tools[args->at].name, .other = "terminal", .cap = 16ULL}))
    {
        (void)EMBED_CALL(cli_model_terminal.command, CliModelCtx, .path = "toggle");
        return;
    }
    // the tools are fewer than an int counts
    g_cli_model.tool = (g_cli_model.tool == (int)args->at) ? -1 : (int)args->at;
    EMBED_CALL(cli_model.editing, CliModelCtx, .on = 0);
    EMBED_CALL(cli_model.panels_hide, CliModelCtx, .at = 0u);
    EMBED_CALL(cli_model.explorer_show, CliModelCtx, .at = 0u);
    EMBED_CALL(cli_model.session_save, CliModelCtx, .at = 0u);
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS) | CLI_MODEL_DIRTY(CLI_SNAPSHOT_EXPLORER) |
                         CLI_MODEL_DIRTY(CLI_SNAPSHOT_PROBLEMS);
}

// a press, a drag or a letting go in the editor's text or its gutter handed to the view
EMBED_INLINE void model_view_pointer(const ControllerModelCtx *args)
{
    const CliInput *const input = args->input;
    const CliViewInput pointed = {.kind = CLI_VIEW_POINTER,
                                  .pointer = args->pointer,
                                  .row = input->row,
                                  .column = input->column,
                                  .clicks = input->clicks,
                                  .shift = (input->mods & CLI_INPUT_MOD_SHIFT) != 0u,
                                  .gutter = input->hit == CLI_HIT_GUTTER};
    EMBED_CALL(cli_model.view_input, CliModelCtx, .input = &pointed, .on = 0);
}

// the view's top moved to `row`
EMBED_INLINE void model_view_scroll(const ControllerModelCtx *args)
{
    const CliViewInput scrolled = {.kind = CLI_VIEW_SCROLL, .top = args->input->row};
    EMBED_CALL(cli_model.view_input, CliModelCtx, .input = &scrolled, .on = 0);
}

// a press, on what the window's layout found under it
EMBED_INLINE void model_press(const ControllerModelCtx *args)
{
    const CliInput *const input = args->input;
    EMBED_CALL(cli_model_commands.menu_input, CliModelCtx, .input = input);
    switch (input->hit)
    {
    case CLI_HIT_MENU_TITLE:
    case CLI_HIT_MENU_ROW:
    case CLI_HIT_SUGGEST_LIST:
    case CLI_HIT_HOVER_BOX:
    case CLI_HIT_SIGNATURE_BOX:
        return;
    case CLI_HIT_SUGGEST_ROW:
        EMBED_CALL(cli_model_panels.completion_accept, CliModelCtx, .at = input->index);
        return;
    case CLI_HIT_STATUS_NOTICE:
    case CLI_HIT_STATUS_SILENCE:
    case CLI_HIT_STATUS_FOCUS:
        EMBED_CALL(cli_model_notices.press, CliModelCtx, .at = input->hit);
        return;
    default:
        break;
    }
    EMBED_CALL(cli_model.panels_hide, CliModelCtx, .at = 0u);
    // a press on the terminal is the terminal's; one elsewhere takes the keys from it
    EMBED_CALL(cli_model_terminal.press, CliModelCtx, .input = input);
    if ((input->hit >= CLI_HIT_TERMINAL) && (input->hit <= CLI_HIT_TERMINAL_PANEL))
    {
        return;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    switch (input->hit)
    {
    case CLI_HIT_WORDMARK:
        (void)EMBED_CALL(cli_report_err_msg.open_page, CliReportErrMsgCfg, .url = CLI_MODEL_REPOSITORY,
                         .err_msg = &err_msg);
        break;
    case CLI_HIT_TOOL:
        EMBED_CALL(cli_model.tool_toggle, CliModelCtx, .at = input->index);
        break;
    case CLI_HIT_PROBLEMS_CHECK:
        EMBED_CALL(cli_model.editing, CliModelCtx, .on = 0);
        EMBED_CALL(cli_model_problems.check, CliModelCtx, .at = 0u);
        break;
    case CLI_HIT_PROBLEM_ROW:
        EMBED_CALL(cli_model.editing, CliModelCtx, .on = 0);
        EMBED_CALL(cli_model_problems.press, CliModelCtx, .at = input->index);
        break;
    case CLI_HIT_EXPLORER_ROW:
        EMBED_CALL(cli_model.editing, CliModelCtx, .on = 0);
        EMBED_CALL(cli_model_explorer.press, CliModelCtx, .at = input->index);
        break;
    case CLI_HIT_TAB:
        EMBED_CALL(cli_model.tab_show, CliModelCtx, .at = input->index);
        break;
    case CLI_HIT_TAB_CLOSE:
        EMBED_CALL(cli_model.tab_close, CliModelCtx, .at = input->index);
        break;
    case CLI_HIT_MAP:
        EMBED_CALL(model_view_scroll, ControllerModelCtx, .input = input);
        break;
    case CLI_HIT_EDITOR:
    case CLI_HIT_GUTTER:
        s_dragging = 1;
        EMBED_CALL(cli_model.editing, CliModelCtx, .on = 1);
        EMBED_CALL(model_view_pointer, ControllerModelCtx, .input = input, .pointer = CLI_VIEW_POINTER_DOWN);
        break;
    default:
        break;
    }
}

// the wheel: over the completion's list its rows, over the editor or the map the text, the window's own rows' offsets
// its own
EMBED_INLINE void model_wheel(const ControllerModelCtx *args)
{
    const CliInput *const input = args->input;
    if (input->hit == CLI_HIT_SUGGEST_LIST)
    {
        EMBED_CALL(cli_model_panels.completion_wheel, CliModelCtx, .on = input->rows_moved);
        return;
    }
    if (input->hit == CLI_HIT_TERMINAL)
    {
        EMBED_CALL(cli_model_terminal.wheel, CliModelCtx, .input = input);
        return;
    }
    const char *const key = EMBED_CALL(cli_model.key, CliModelCtx, .at = 0u);
    if (key == NULL)
    {
        return;
    }
    EMBED_CALL(cli_model.panels_hide, CliModelCtx, .at = 0u);
    const unsigned long long top = cli_view_top(key);
    // a wheel's rows are far inside what a long long represents, and a top above the first line stops at it
    unsigned long long moved;
    if (input->rows_moved >= 0)
    {
        moved = top + (unsigned long long)input->rows_moved;
    }
    else if (top > (unsigned long long)(-(long long)input->rows_moved))
    {
        moved = top - (unsigned long long)(-(long long)input->rows_moved);
    }
    else
    {
        moved = 0ULL;
    }
    const CliViewInput scrolled = {.kind = CLI_VIEW_SCROLL, .top = moved};
    EMBED_CALL(cli_model.view_input, CliModelCtx, .input = &scrolled, .on = 0);
}

// the percent the text `value` writes into `percent`, digits alone and no more than three, where it does
EMBED_INLINE void model_percent_read(const ControllerModelCtx *args)
{
    if (args->value == NULL)
    {
        return;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = 8ULL});
    unsigned long long end = 0ULL;
    const unsigned long long number = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = args->value,
                                                 .at = 0ULL, .size = size, .end = &end);
    if ((size != 0ULL) && (size <= 3ULL) && (end == size))
    {
        // no more than three digits, far inside what an unsigned int represents
        *args->percent = (unsigned int)number;
    }
}

// the color `key` saved as `value`, in place of the one saved under the name; cells grown as a name comes
EMBED_INLINE void model_color_set(const ControllerModelCtx *args)
{
    const unsigned long long name_size = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL}) + 1ULL;
    const unsigned long long value_size =
        cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = CLI_CELLS_POOL}) + 1ULL;
    char *const text = g_cli_cells->persistent_buf_alloc(name_size + value_size, &g_cli_controller_cells);
    if (text == NULL)
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = text, .src = args->key, .bytes = name_size});
    memor.cpy(&(MemoriaCfg){.dst = text + name_size, .src = args->value, .bytes = value_size});
    for (unsigned int at = 0u; at < s_colors; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_color[at].name, .other = args->key, .cap = CLI_CELLS_POOL}))
        {
            g_cli_cells->persistent_buf_release(s_color[at].name, &g_cli_controller_cells);
            s_color[at] = (CliSettingColor){text, text + name_size};
            return;
        }
    }
    CliSettingColor *const grown =
        g_cli_cells->persistent_buf_alloc((s_colors + 1ULL) * sizeof(*grown), &g_cli_controller_cells);
    if (grown == NULL)
    {
        g_cli_cells->persistent_buf_release(text, &g_cli_controller_cells);
        return;
    }
    if (s_color != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = s_color, .bytes = s_colors * sizeof(*grown)});
    }
    grown[s_colors] = (CliSettingColor){text, text + name_size};
    g_cli_cells->persistent_buf_release(s_color, &g_cli_controller_cells);
    s_color = grown;
    s_colors += 1u;
}

// a setting of `key` taken: the model's value moved and it saved in orior's folder
EMBED_INLINE void model_setting(const ControllerModelCtx *args)
{
    const char *const value =
        args->key + cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL}) + 1ULL;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = CLI_MODEL_ZOOM_KEY, .cap = 32ULL}))
    {
        EMBED_CALL(model_percent_read, ControllerModelCtx, .value = value, .percent = &g_cli_model.zoom_percent);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = CLI_MODEL_BAR_KEY, .cap = 32ULL}))
    {
        EMBED_CALL(model_percent_read, ControllerModelCtx, .value = value, .percent = &g_cli_model.bar_percent);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = CLI_MODEL_EDITOR_KEY, .cap = 32ULL}))
    {
        EMBED_CALL(model_percent_read, ControllerModelCtx, .value = value, .percent = &g_cli_model.editor_percent);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = CLI_MODEL_REDUCE_KEY, .cap = 32ULL}))
    {
        g_cli_model.motion_saved = cellul.eq(&(CatenaFinitaCfg){.src = value, .other = "true", .cap = 8ULL});
    }
    else if (cellul.starts(&(CatenaFinitaCfg){.src = args->key, .other = CLI_MODEL_COLOR_KEY, .cap = 32ULL}))
    {
        EMBED_CALL(model_color_set, ControllerModelCtx, .key = args->key + sizeof(CLI_MODEL_COLOR_KEY) - 1u,
                   .value = value);
    }
    EMBED_CALL(cli_model.value_save, CliModelCtx, .path = args->key, .value = value);
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_SETTINGS);
}

void cli_controller_model_input(const CliInput *input, const char *bytes)
{
    // the notices know when the reader commanded and when the work rests
    if ((input->kind == CLI_INPUT_KEY) || (input->kind == CLI_INPUT_TEXT) || (input->kind == CLI_INPUT_POINTER_DOWN))
    {
        EMBED_CALL(cli_model_notices.input, CliModelCtx, .input = input,
                   .path = (input->kind == CLI_INPUT_KEY) ? bytes : NULL);
    }
    switch (input->kind)
    {
    case CLI_INPUT_KEY:
        EMBED_CALL(cli_model_commands.key_take, CliModelCtx, .path = bytes, .on = input->hit == CLI_HIT_WORDMARK);
        break;
    case CLI_INPUT_TEXT: {
        if (EMBED_CALL(cli_model_terminal.text, CliModelCtx, .path = bytes, .length = input->length))
        {
            break;
        }
        const CliViewInput typed = {.kind = CLI_VIEW_TEXT, .text = bytes, .length = input->length};
        EMBED_CALL(cli_model.editing, CliModelCtx, .on = 1);
        EMBED_CALL(cli_model_panels.text, CliModelCtx, .path = bytes, .length = input->length);
        EMBED_CALL(cli_model.view_input, CliModelCtx, .input = &typed, .on = 1);
        break;
    }
    case CLI_INPUT_POINTER_DOWN:
        EMBED_CALL(model_press, ControllerModelCtx, .input = input);
        break;
    case CLI_INPUT_POINTER_MOVED:
        EMBED_CALL(cli_model_commands.menu_input, CliModelCtx, .input = input);
        if (EMBED_CALL(cli_model_terminal.pointer, CliModelCtx, .input = input))
        {
            break;
        }
        if (s_dragging)
        {
            EMBED_CALL(model_view_pointer, ControllerModelCtx, .input = input, .pointer = CLI_VIEW_POINTER_MOVED);
        }
        else
        {
            EMBED_CALL(cli_model_panels.pointer, CliModelCtx, .input = input);
        }
        break;
    case CLI_INPUT_POINTER_UP:
        (void)EMBED_CALL(cli_model_terminal.pointer, CliModelCtx, .input = input);
        if (s_dragging)
        {
            s_dragging = 0;
            EMBED_CALL(model_view_pointer, ControllerModelCtx, .input = input, .pointer = CLI_VIEW_POINTER_UP);
        }
        break;
    case CLI_INPUT_WHEEL:
        EMBED_CALL(model_wheel, ControllerModelCtx, .input = input);
        break;
    case CLI_INPUT_SIZE: {
        const int sized = (g_cli_model.rows != input->rows) || (g_cli_model.columns != input->columns);
        g_cli_model.rows = input->rows;
        g_cli_model.columns = input->columns;
        cli_model_menu_bar_size(&(CliModelCtx){.at = input->index, .on = (int)input->level});
        if (sized)
        {
            const CliViewInput size = {.kind = CLI_VIEW_SIZE, .rows = input->rows, .columns = input->columns};
            EMBED_CALL(cli_model.view_input, CliModelCtx, .input = &size, .on = 0);
        }
        break;
    }
    case CLI_INPUT_DROP:
        EMBED_CALL(cli_model.file_open, CliModelCtx, .path = bytes);
        break;
    case CLI_INPUT_CLOSE:
        g_cli_model.closing = 1;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS);
        break;
    case CLI_INPUT_SETTING:
        EMBED_CALL(model_setting, ControllerModelCtx, .key = bytes);
        break;
    case CLI_INPUT_COMMAND:
        EMBED_CALL(cli_model_commands.command_run, CliModelCtx, .path = bytes);
        break;
    case CLI_INPUT_PREFERENCES_KEY:
        EMBED_CALL(cli_model_commands.preferences_key, CliModelCtx, .path = bytes);
        break;
    case CLI_INPUT_MAP_SPAN:
        if (!g_cli_model.map_span_known || (g_cli_model.map_first != input->row) || (g_cli_model.map_last != input->column))
        {
            g_cli_model.map_span_known = 1;
            g_cli_model.map_first = input->row;
            g_cli_model.map_last = input->column;
            g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_MAP);
        }
        break;
    case CLI_INPUT_TERMINAL_SIZE:
    case CLI_INPUT_TERMINAL_HEIGHT:
        EMBED_CALL(cli_model_terminal.size, CliModelCtx, .input = input);
        break;
    case CLI_INPUT_MENU:
    case CLI_INPUT_MENU_AWAY:
        EMBED_CALL(cli_model_commands.menu_open, CliModelCtx, .input = input);
        break;
    case CLI_INPUT_MOTION_SYSTEM:
        // Reduce Motion's mark reads the system's wish as well as the saved setting
        g_cli_model.motion_system = input->index != 0u;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_MENUS);
        break;
    default:
        break;
    }
}

// the tabs' snapshot: its rows and their paths and names in one cell
static void *model_tabs_alloc(void)
{
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; at < g_cli_model.tabs; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = g_cli_model.tab[at]->path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    }
    CliTabs *const tabs = g_cli_cells->persistent_buf_alloc(
        sizeof(*tabs) + (g_cli_model.tabs * sizeof(CliTabRow)) + bytes, &g_cli_controller_cells);
    if (tabs == NULL)
    {
        return NULL;
    }
    tabs->count = g_cli_model.tabs;
    tabs->active = g_cli_model.active;
    tabs->row = (CliTabRow *)(void *)(tabs + 1);
    char *text = (char *)(void *)(tabs->row + g_cli_model.tabs);
    for (unsigned int at = 0u; at < g_cli_model.tabs; at += 1u)
    {
        const char *const path = g_cli_model.tab[at]->path;
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
        memor.cpy(&(MemoriaCfg){.dst = text, .src = path, .bytes = size + 1ULL});
        const unsigned long long name_at = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                      .text = text, .at = size, .set = "/\\");
        tabs->row[at] =
            (CliTabRow){text, text + name_at,
                         EMBED_CALL(cli_view_table.modified, CliViewCfg, .key = g_cli_model.tab[at]->key),
                         g_cli_model.tab[at]->closing};
        text += size + 1ULL;
    }
    return tabs;
}

// the window's standing in one cell
static void *model_status_alloc(void)
{
    const char *const title = (g_cli_model.title != NULL) ? g_cli_model.title : "";
    const char *const path = (g_cli_model.active >= 0) ? g_cli_model.tab[g_cli_model.active]->path : "";
    const char *const notice = (g_cli_model.notice != NULL) ? g_cli_model.notice : "";
    const unsigned long long title_size = cellul.len(&(CatenaFinitaCfg){.src = title, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long path_size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long notice_size = cellul.len(&(CatenaFinitaCfg){.src = notice, .cap = CLI_CELLS_POOL}) + 1ULL;
    CliStatus *const status = g_cli_cells->persistent_buf_alloc(sizeof(*status) + title_size + path_size + notice_size,
                                                                &g_cli_controller_cells);
    if (status == NULL)
    {
        return NULL;
    }
    char *const text = (char *)(void *)(status + 1);
    memor.cpy(&(MemoriaCfg){.dst = text, .src = title, .bytes = title_size});
    memor.cpy(&(MemoriaCfg){.dst = text + title_size, .src = path, .bytes = path_size});
    memor.cpy(&(MemoriaCfg){.dst = text + title_size + path_size, .src = notice, .bytes = notice_size});
    const char *const key = EMBED_CALL(cli_model.key, CliModelCtx, .at = 0u);
    *status = (CliStatus){text,
                          text + title_size,
                          (g_cli_model.active >= 0) && g_cli_model.tab[g_cli_model.active]->viewed,
                          (key != NULL) && EMBED_CALL(cli_document.crlf, CliDocumentCfg, .key = key),
                          g_cli_model.editing,
                          g_cli_model.tool,
                          g_cli_model.problems_marked,
                          (g_cli_model.busy != 0u) || g_cli_model.checking,
                          g_cli_model.closing,
                          g_cli_model.preferences_displayed,
                          g_cli_model.memory_bytes,
                          g_cli_model.wordmark,
                          g_cli_model.tree_read,
                          g_cli_model.shown,
                          text + title_size + path_size,
                          g_cli_model.notice_failed,
                          g_cli_model.focus,
                          g_cli_model.focus_waiting};
    return status;
}

// the settings in one cell
static void *model_settings_alloc(void)
{
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; at < s_colors; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = s_color[at].name, .cap = CLI_CELLS_POOL}) +
                 cellul.len(&(CatenaFinitaCfg){.src = s_color[at].value, .cap = CLI_CELLS_POOL}) + 2ULL;
    }
    CliSettings *const settings = g_cli_cells->persistent_buf_alloc(
        sizeof(*settings) + (s_colors * sizeof(CliSettingColor)) + bytes, &g_cli_controller_cells);
    if (settings == NULL)
    {
        return NULL;
    }
    settings->zoom_percent = g_cli_model.zoom_percent;
    settings->bar_percent = g_cli_model.bar_percent;
    settings->editor_percent = g_cli_model.editor_percent;
    settings->motion_saved = g_cli_model.motion_saved;
    settings->colors = s_colors;
    settings->color = (CliSettingColor *)(void *)(settings + 1);
    char *text = (char *)(void *)(settings->color + s_colors);
    for (unsigned int at = 0u; at < s_colors; at += 1u)
    {
        const unsigned long long name =
            cellul.len(&(CatenaFinitaCfg){.src = s_color[at].name, .cap = CLI_CELLS_POOL}) + 1ULL;
        const unsigned long long value =
            cellul.len(&(CatenaFinitaCfg){.src = s_color[at].value, .cap = CLI_CELLS_POOL}) + 1ULL;
        memor.cpy(&(MemoriaCfg){.dst = text, .src = s_color[at].name, .bytes = name});
        memor.cpy(&(MemoriaCfg){.dst = text + name, .src = s_color[at].value, .bytes = value});
        settings->color[at] = (CliSettingColor){text, text + name};
        text += name + value;
    }
    return settings;
}

// the snapshot of `part`, NULL where it shows nothing or the cells cannot meet it
EMBED_INLINE void *model_part_alloc(const ControllerModelCtx *args)
{
    const char *const key = EMBED_CALL(cli_model.key, CliModelCtx, .at = 0u);
    const int viewed = (key != NULL) && g_cli_model.tab[g_cli_model.active]->viewed;
    switch (args->part)
    {
    case CLI_SNAPSHOT_FRAME: {
        void *const frame = viewed ? cli_view_frame_alloc(key) : NULL;
        EMBED_CALL(cli_model_panels.frame, CliModelCtx, .input = frame);
        return frame;
    }
    case CLI_SNAPSHOT_MAP:
        return (viewed && g_cli_model.map_span_known)
                   ? cli_view_map_alloc(key, g_cli_model.map_first, g_cli_model.map_last)
                   : NULL;
    case CLI_SNAPSHOT_HOVER:
        return EMBED_CALL(cli_model_panels.hover_alloc, CliModelCtx, .at = 0u);
    case CLI_SNAPSHOT_COMPLETIONS:
        return EMBED_CALL(cli_model_panels.completions_alloc, CliModelCtx, .at = 0u);
    case CLI_SNAPSHOT_SIGNATURE:
        return EMBED_CALL(cli_model_panels.signature_alloc, CliModelCtx, .at = 0u);
    case CLI_SNAPSHOT_MENUS:
        return EMBED_CALL(cli_model_commands.menus_alloc, CliModelCtx, .at = 0u);
    case CLI_SNAPSHOT_TABS:
        return model_tabs_alloc();
    case CLI_SNAPSHOT_EXPLORER:
        return EMBED_CALL(cli_model_explorer.rows_alloc, CliModelCtx, .at = 0u);
    case CLI_SNAPSHOT_PROBLEMS:
        return EMBED_CALL(cli_model_problems.rows_alloc, CliModelCtx, .at = 0u);
    case CLI_SNAPSHOT_STATUS:
        return model_status_alloc();
    case CLI_SNAPSHOT_SETTINGS:
        return model_settings_alloc();
    case CLI_SNAPSHOT_NAMES:
        return (key != NULL) ? EMBED_CALL(cli_document_names.names_alloc, CliDocumentNamesCfg, .key = key,
                                          .owner = &g_cli_controller_cells)
                             : NULL;
    case CLI_SNAPSHOT_TERMINAL:
        return EMBED_CALL(cli_model_terminal.alloc, CliModelCtx, .at = 0u);
    default:
        return NULL;
    }
}

// what runs on the files' lane: what orior holds in RAM read
static void model_memory_run(void *context)
{
    CliProcessMemory measured;
    const int read =
        EMBED_CALL(cli_pid_internal.process_memory, CliPidCtx, .pid = cli_process_own(), .memory = &measured);
    *(unsigned long long *)context = read ? measured.working : 0ULL;
}

static void model_memory_done(void *context)
{
    g_cli_model.memory_bytes = *(unsigned long long *)context;
    g_cli_cells->persistent_buf_release(context, &g_cli_controller_cells);
    s_memory_reading = 0;
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS);
}

int cli_controller_model_step(void)
{
    int did = cli_controller_work_step();
    did = cli_document_released_free() || did;
    const unsigned long long now_ms = cli_clock_milliseconds();
    if ((EMBED_ATOMIC_LOAD(&s_colored) != 0u) && (g_cli_model.active >= 0))
    {
        EMBED_ATOMIC_STORE(&s_colored, 0u);
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_FRAME) | CLI_MODEL_DIRTY(CLI_SNAPSHOT_MAP) |
                             CLI_MODEL_DIRTY(CLI_SNAPSHOT_NAMES);
    }
    did = EMBED_CALL(cli_model_commands.menu_step, CliModelCtx, .at = 0u) || did;
    did = EMBED_CALL(cli_model_panels.step, CliModelCtx, .at = 0u) || did;
    did = EMBED_CALL(cli_model_problems.step, CliModelCtx, .at = 0u) || did;
    did = EMBED_CALL(cli_model_door.step, CliModelCtx, .at = 0u) || did;
    did = EMBED_CALL(cli_model_watch.step, CliModelCtx, .at = 0u) || did;
    did = EMBED_CALL(cli_model_terminal.step, CliModelCtx, .at = 0u) || did;
    did = EMBED_CALL(cli_model_notices.step, CliModelCtx, .at = 0u) || did;
    if (!s_memory_reading && (now_ms >= g_cli_model.memory_due_ms))
    {
        unsigned long long *const reading =
            g_cli_cells->persistent_buf_alloc(sizeof(*reading), &g_cli_controller_cells);
        g_cli_model.memory_due_ms = now_ms + CLI_MODEL_MEMORY_MS;
        s_memory_reading = (reading != NULL) &&
                           EMBED_CALL(cli_controller_work.post, CliControllerWorkCfg, .lane_at = CLI_WORK_FILES,
                                      .run = model_memory_run, .done = model_memory_done, .context = reading);
        if ((reading != NULL) && !s_memory_reading)
        {
            g_cli_cells->persistent_buf_release(reading, &g_cli_controller_cells);
        }
    }
    const unsigned int dirty = g_cli_model.dirty;
    g_cli_model.dirty = 0u;
    for (unsigned int part = 0u; part < CLI_SNAPSHOT_PARTS; part += 1u)
    {
        if ((dirty & CLI_MODEL_DIRTY(part)) != 0u)
        {
            cli_controller_publish(part, EMBED_CALL(model_part_alloc, ControllerModelCtx, .part = part));
            did = 1;
        }
    }
    return did;
}

unsigned long long cli_controller_model_due_ms(void)
{
    const unsigned long long dues[] = {EMBED_CALL(cli_model_commands.menu_due_ms, CliModelCtx, .at = 0u),
                                       EMBED_CALL(cli_model_panels.due_ms, CliModelCtx, .at = 0u),
                                       EMBED_CALL(cli_model_terminal.next_ms, CliModelCtx, .at = 0u),
                                       EMBED_CALL(cli_model_notices.due_ms, CliModelCtx, .at = 0u),
                                       g_cli_model.memory_due_ms};
    unsigned long long earliest = 0ULL;
    for (unsigned int at = 0u; at < (sizeof(dues) / sizeof(dues[0])); at += 1u)
    {
        if ((dues[at] != 0ULL) && ((earliest == 0ULL) || (dues[at] < earliest)))
        {
            earliest = dues[at];
        }
    }
    if (earliest == 0ULL)
    {
        return 0ULL;
    }
    const unsigned long long now_ms = cli_clock_milliseconds();
    return (earliest > now_ms) ? (earliest - now_ms) : 1ULL;
}

// what the name does in this session: it stands on the first start in it, and goes home on any start after; the session
// saved is `seconds logon`, the boot time reading a few seconds either way from one start to the next
EMBED_INLINE void model_wordmark_read(const ControllerModelCtx *args)
{
    char logon[CLI_MACHINE_LOGON_MAX_LEN];
    EMBED_CALL(cli_capacity_logon_internal.logon_by_name, CliCapacityLogonCtx, .logon = logon,
               .capacity_bytes = sizeof(logon));
    const unsigned long long booted = cli_booted_seconds();
    const char *const saved = cli_state_entry_value(args->entries, CLI_MODEL_WORDMARK_KEY);
    int same = 0;
    if (saved != NULL)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = saved, .cap = CLI_CELLS_POOL});
        unsigned long long end = 0ULL;
        const unsigned long long then =
            EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = saved, .at = 0ULL, .size = size, .end = &end);
        const unsigned long long apart = (then > booted) ? (then - booted) : (booted - then);
        same = (end < size) && (saved[end] == ' ') && (apart < CLI_MODEL_SESSION_SECONDS) &&
               cellul.eq(&(CatenaFinitaCfg){.src = saved + end + 1ULL, .other = logon, .cap = sizeof(logon)});
    }
    g_cli_model.wordmark = same ? CLI_WORDMARK_HOME : CLI_WORDMARK_STAND;
    char text[CLI_MACHINE_LOGON_MAX_LEN + 32u];
    VerbaLinea now = {text, sizeof(text), 0ULL};
    mmgr_line_decimal(&now, booted, 1u);
    mmgr_line_ch(&now, ' ');
    mmgr_line_put(&now, logon);
    if (mmgr_line_finish(&now) != 0ULL)
    {
        EMBED_CALL(cli_model.value_save, CliModelCtx, .path = CLI_MODEL_WORDMARK_KEY, .value = text);
    }
}

// the tree open here and its name, the last part of its path, each in a cell of its own
EMBED_INLINE void model_root_read(const ControllerModelCtx *args)
{
    (void)args;
    const char *const root = cli_controller_root();
    if (root == NULL)
    {
        return;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_PATH_MAX_LEN});
    g_cli_model.root = g_cli_cells->persistent_buf_alloc(size + 1ULL, &g_cli_controller_cells);
    if (g_cli_model.root != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = g_cli_model.root, .src = root, .bytes = size + 1ULL});
        // a root's trailing slash is none of its name
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                                  .text = root, .at = size, .set = "/\\");
        const unsigned long long from = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                   .text = root, .at = end, .set = "/\\");
        g_cli_model.title = g_cli_cells->persistent_buf_alloc(end - from + 1ULL, &g_cli_controller_cells);
        if (g_cli_model.title != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = g_cli_model.title, .src = root + from, .bytes = end - from});
            g_cli_model.title[end - from] = '\0';
        }
    }
}

void cli_controller_model_begin(void)
{
    memor.set(&(MemoriaCfg){.dst = &g_cli_model, .val = 0, .bytes = sizeof(g_cli_model)});
    g_cli_model.active = -1;
    g_cli_model.last_active = -1;
    g_cli_model.tool = -1;
    g_cli_model.zoom_percent = CLI_MODEL_ZOOM_PERCENT;
    g_cli_model.bar_percent = CLI_MODEL_BAR_PERCENT;
    g_cli_model.editor_percent = CLI_MODEL_EDITOR_PERCENT;
    (void)cli_controller_work_begin();
    EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = model_colored, .threaded = 1);
    EMBED_CALL(model_root_read, ControllerModelCtx, .part = 0u);
    (void)EMBED_CALL(cli_model_commands.catalog_read, CliModelCtx, .at = 0u);
    EMBED_CALL(cli_model_problems.begin, CliModelCtx, .at = 0u);
    char folder[CLI_PATH_MAX_LEN];
    CliStateEntries *const entries =
        EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder)
            ? cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = folder, .owner = &g_cli_controller_cells})
            : NULL;
    const char *const reduce = cli_state_entry_value(entries, CLI_MODEL_REDUCE_KEY);
    g_cli_model.motion_saved =
        (reduce != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = reduce, .other = "true", .cap = 8ULL});
    EMBED_CALL(model_percent_read, ControllerModelCtx, .value = cli_state_entry_value(entries, CLI_MODEL_ZOOM_KEY),
               .percent = &g_cli_model.zoom_percent);
    EMBED_CALL(model_percent_read, ControllerModelCtx, .value = cli_state_entry_value(entries, CLI_MODEL_BAR_KEY),
               .percent = &g_cli_model.bar_percent);
    EMBED_CALL(model_percent_read, ControllerModelCtx, .value = cli_state_entry_value(entries, CLI_MODEL_EDITOR_KEY),
               .percent = &g_cli_model.editor_percent);
    for (unsigned int at = 0u; (entries != NULL) && (at < entries->count); at += 1u)
    {
        if (cellul.starts(
                &(CatenaFinitaCfg){.src = entries->entry[at].key, .other = CLI_MODEL_COLOR_KEY, .cap = 32ULL}))
        {
            EMBED_CALL(model_color_set, ControllerModelCtx,
                       .key = entries->entry[at].key + sizeof(CLI_MODEL_COLOR_KEY) - 1u,
                       .value = entries->entry[at].value);
        }
    }
    EMBED_CALL(model_wordmark_read, ControllerModelCtx, .entries = entries);
    EMBED_CALL(cli_model_notices.restore, CliModelCtx, .input = entries);
    EMBED_CALL(cli_model_terminal.restore, CliModelCtx, .input = entries);
    if (g_cli_model.root != NULL)
    {
        EMBED_CALL(cli_model.session_restore, CliModelCtx, .input = entries);
    }
    g_cli_model.restored = 1;
    g_cli_model.tree_read = 1;
    g_cli_cells->persistent_buf_release(entries, &g_cli_controller_cells);
    EMBED_CALL(cli_model_door.launch_run, CliModelCtx, .at = 0u);
    g_cli_model.dirty = (1u << CLI_SNAPSHOT_PARTS) - 1u;
}

void cli_controller_model_end(void)
{
    EMBED_CALL(cli_model.session_save, CliModelCtx, .at = 0u);
    EMBED_CALL(cli_model_terminal.end, CliModelCtx, .at = 0u);
    EMBED_CALL(cli_model_watch.end, CliModelCtx, .at = 0u);
    cli_controller_work_end();
    EMBED_CALL(cli_model_problems.end, CliModelCtx, .at = 0u);
    EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = NULL, .threaded = 0);
    (void)cli_document_released_free();
}
