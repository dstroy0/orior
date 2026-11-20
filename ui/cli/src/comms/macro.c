// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macro.c: the door macros (macro.h). The macros stand in one movable cell of CLI_DOOR_MACROS records, taken on the
// first recording. A record holds its name, its steps' offsets into its bytes, and the bytes: each step is its menu,
// its command and its words, each ended by a 0. A send keeps a cursor, the macro, the step and the round, and sends
// one step through the sessions at a time
#include "macro.h"
#include "../command_line/commands.h"
#include "sessions.h"

// A macro: whether its record is in use, its name, the steps and where each starts in the bytes, the bytes in use,
// and the bytes
typedef struct
{
    int used;
    char name[CLI_DOOR_MACRO_NAME];
    unsigned int steps;
    unsigned int start[CLI_DOOR_MACRO_STEPS];
    unsigned int length;
    char bytes[CLI_DOOR_MACRO_BYTES];
} DoorMacro;

// The macros: the cell of records; the recording, the record it fills and whether it is on; the send, the record,
// the step and the round it is at, the rounds asked, the sender's name and how many steps ran; and the answer that
// waits, `pending` while one does, its text and whether every host ran every step
typedef struct
{
    const CarcerMovableEntry *cell;
    int recording;
    unsigned int recorded;
    int sending;
    unsigned int sent;
    unsigned int step;
    unsigned long long round;
    unsigned long long rounds;
    char sender[CLI_DOOR_MACRO_NAME];
    unsigned long long ran;
    int pending;
    int done;
    char text[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea line;
} DoorMacroCtx;

typedef struct
{
    unsigned int at;
    DoorMacro *macro;
    const DoorMacro *macro_read;
    const char *name;
    const char *command;
    char *word_out;
    unsigned int *at_out;
    int done;
    int make;
} DoorMacroOpCtx;

static DoorMacroCtx s_macros;

// the record at `at` read into `macro`, and `macro_read` written at `at`
EMBED_INLINE void macro_read(const DoorMacroOpCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_macros.cell,
                                                            .offset = (embed_word)args->at * sizeof(DoorMacro),
                                                            .dst = args->macro,
                                                            .bytes = sizeof(DoorMacro)});
}

EMBED_INLINE void macro_write(const DoorMacroOpCtx *args)
{
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_macros.cell,
                                                             .offset = (embed_word)args->at * sizeof(DoorMacro),
                                                             .src = args->macro_read,
                                                             .bytes = sizeof(DoorMacro)});
}

// the cell of records taken where none is, each record free: 1, or 0 where no cell is free
EMBED_INLINE int macros_cell_ready(void)
{
    if (s_macros.cell != NULL)
    {
        return 1;
    }
    s_macros.cell =
        g_cli_guard->movable_buf_alloc((unsigned long long)CLI_DOOR_MACROS * sizeof(DoorMacro), g_cli_comms_owner);
    if (s_macros.cell == NULL)
    {
        return 0;
    }
    static DoorMacro s_empty;
    s_empty.used = 0;
    for (unsigned int at = 0u; at < CLI_DOOR_MACROS; at += 1u)
    {
        EMBED_CALL(macro_write, DoorMacroOpCtx, .at = at, .macro_read = &s_empty);
    }
    return 1;
}

// the record named `name`, into `macro` with its place in `at`: 1, or 0 where none is; with `make`, the first free
// record taken for the name where none has it
EMBED_INLINE int macro_named(const DoorMacroOpCtx *args)
{
    if (!macros_cell_ready())
    {
        return 0;
    }
    unsigned int free_at = CLI_DOOR_MACROS;
    for (unsigned int at = 0u; at < CLI_DOOR_MACROS; at += 1u)
    {
        EMBED_CALL(macro_read, DoorMacroOpCtx, .at = at, .macro = args->macro);
        if (args->macro->used &&
            cellul.eq(&(CatenaFinitaCfg){.src = args->macro->name, .other = args->name, .cap = CLI_DOOR_MACRO_NAME}))
        {
            *args->at_out = at;
            return 1;
        }
        if (!args->macro->used && (free_at == CLI_DOOR_MACROS))
        {
            free_at = at;
        }
    }
    if (!args->make || (free_at == CLI_DOOR_MACROS))
    {
        return 0;
    }
    EMBED_CALL(macro_read, DoorMacroOpCtx, .at = free_at, .macro = args->macro);
    *args->at_out = free_at;
    return 1;
}

// the menu word of the menu whose item has the command `command`, into `word_out`: 1, or 0 where no menu has it
EMBED_INLINE int macro_menu_word(const DoorMacroOpCtx *args)
{
    const unsigned int menus = cli_menus();
    for (unsigned int menu = 0u; menu < menus; menu += 1u)
    {
        CliItem item;
        if (cli_menu_command(&(CliCommandsMenuCfg){.menu = menu, .word_text = args->command, .item = &item}))
        {
            EMBED_CALL(cli_commands_menu_word.menu_word, CliCommandsMenuWordCfg, .menu = menu,
                       .word_out = args->word_out);
            return 1;
        }
    }
    return 0;
}

// the answer that waits begun: its text started over, `done` as given
EMBED_INLINE void macro_answer_begin(const DoorMacroOpCtx *args)
{
    s_macros.line = (VerbaLinea){s_macros.text, sizeof(s_macros.text), 0ULL};
    s_macros.pending = 1;
    s_macros.done = args->done;
}

int cli_door_macro_record(const CliDoorMacroCfg *args)
{
    if (s_macros.recording)
    {
        mmgr_line_put(args->err_msg, "a macro records already");
        return 0;
    }
    if ((args->name == NULL) || (args->name[0] == '\0'))
    {
        mmgr_line_put(args->err_msg, "a name is needed");
        return 0;
    }
    DoorMacro macro;
    unsigned int at = 0u;
    if (!EMBED_CALL(macro_named, DoorMacroOpCtx, .name = args->name, .macro = &macro, .at_out = &at, .make = 1))
    {
        mmgr_line_put(args->err_msg, "no record is free for another macro");
        return 0;
    }
    VerbaLinea name = {macro.name, sizeof(macro.name), 0ULL};
    mmgr_line_put(&name, args->name);
    if (mmgr_line_finish(&name) == 0ULL)
    {
        mmgr_line_put(args->err_msg, "the name does not fit");
        return 0;
    }
    macro.used = 1;
    macro.steps = 0u;
    macro.length = 0u;
    EMBED_CALL(macro_write, DoorMacroOpCtx, .at = at, .macro_read = &macro);
    s_macros.recording = 1;
    s_macros.recorded = at;
    return 1;
}

int cli_door_macro_stop(const CliDoorMacroCfg *args)
{
    if (!s_macros.recording)
    {
        mmgr_line_put(args->err_msg, "no macro records");
        return 0;
    }
    s_macros.recording = 0;
    DoorMacro macro;
    EMBED_CALL(macro_read, DoorMacroOpCtx, .at = s_macros.recorded, .macro = &macro);
    mmgr_line_put(args->out, macro.name);
    mmgr_line_put(args->out, ": ");
    mmgr_line_decimal(args->out, macro.steps, 1u);
    mmgr_line_put(args->out, (macro.steps == 1u) ? " step" : " steps");
    return 1;
}

int cli_door_macro_recording(void)
{
    return s_macros.recording;
}

int cli_door_macro_recorded(const CliDoorMacroCfg *args)
{
    if (!s_macros.recording || (args->command == NULL))
    {
        return 0;
    }
    char menu[CLI_LABEL_MAX_LEN];
    if (!EMBED_CALL(macro_menu_word, DoorMacroOpCtx, .command = args->command, .word_out = menu))
    {
        return 0;
    }
    DoorMacro macro;
    EMBED_CALL(macro_read, DoorMacroOpCtx, .at = s_macros.recorded, .macro = &macro);
    if (macro.steps >= CLI_DOOR_MACRO_STEPS)
    {
        return 0;
    }
    VerbaLinea step = {macro.bytes + macro.length, sizeof(macro.bytes) - macro.length, 0ULL};
    mmgr_line_put(&step, menu);
    mmgr_line_ch(&step, '\0');
    mmgr_line_put(&step, args->command);
    mmgr_line_ch(&step, '\0');
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(&step, args->request[at]);
        mmgr_line_ch(&step, '\0');
    }
    if (!mmgr_line_ok(&step))
    {
        return 0;
    }
    macro.start[macro.steps] = macro.length;
    macro.steps += 1u;
    // the step's bytes end at the last 0 written, which the line counts
    macro.length += (unsigned int)step.at;
    EMBED_CALL(macro_write, DoorMacroOpCtx, .at = s_macros.recorded, .macro_read = &macro);
    return 1;
}

void cli_door_macro_list(const CliDoorMacroCfg *args)
{
    EMBED_CALL(macro_answer_begin, DoorMacroOpCtx, .done = 1);
    VerbaLinea *const out = (args->out != NULL) ? args->out : &s_macros.line;
    unsigned int listed = 0u;
    if (s_macros.cell != NULL)
    {
        DoorMacro macro;
        for (unsigned int at = 0u; at < CLI_DOOR_MACROS; at += 1u)
        {
            EMBED_CALL(macro_read, DoorMacroOpCtx, .at = at, .macro = &macro);
            if (!macro.used)
            {
                continue;
            }
            if (listed != 0u)
            {
                mmgr_line_ch(out, '\n');
            }
            mmgr_line_put(out, macro.name);
            mmgr_line_put(out, ": ");
            mmgr_line_decimal(out, macro.steps, 1u);
            mmgr_line_put(out, (macro.steps == 1u) ? " step" : " steps");
            listed += 1u;
        }
    }
    if (listed == 0u)
    {
        mmgr_line_put(out, "no macros");
    }
}

int cli_door_macro_forget(const CliDoorMacroCfg *args)
{
    DoorMacro macro;
    unsigned int at = 0u;
    if ((args->name == NULL) ||
        !EMBED_CALL(macro_named, DoorMacroOpCtx, .name = args->name, .macro = &macro, .at_out = &at, .make = 0))
    {
        mmgr_line_put(args->err_msg, "no macro is named ");
        mmgr_line_put(args->err_msg, (args->name != NULL) ? args->name : "");
        return 0;
    }
    if (s_macros.recording && (s_macros.recorded == at))
    {
        s_macros.recording = 0;
    }
    macro.used = 0;
    EMBED_CALL(macro_write, DoorMacroOpCtx, .at = at, .macro_read = &macro);
    return 1;
}

// The step the cursor is at sent to every host through the sessions: the step's fields read off the macro's bytes,
// `--as` and the sender put before a pointer step's words where it has none. 1, or 0 with why given where the
// sessions took no request
EMBED_INLINE int macro_step_sent(const DoorMacroOpCtx *args)
{
    DoorMacro macro;
    EMBED_CALL(macro_read, DoorMacroOpCtx, .at = s_macros.sent, .macro = &macro);
    const char *field[CLI_WORDS_MAX + 4u];
    unsigned int fields = 0u;
    unsigned int at = macro.start[s_macros.step];
    const unsigned int end = (s_macros.step + 1u < macro.steps) ? macro.start[s_macros.step + 1u] : macro.length;
    while ((at < end) && (fields < CLI_WORDS_MAX + 2u))
    {
        field[fields] = macro.bytes + at;
        fields += 1u;
        at += (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = macro.bytes + at, .cap = end - at}) + 1u;
    }
    (void)args;
    if (fields < 2u)
    {
        mmgr_line_put(&s_macros.line, "\na step has no command");
        s_macros.done = 0;
        return 0;
    }
    const char *words[CLI_WORDS_MAX + 2u];
    unsigned int count = 0u;
    const int pointer_step = cellul.eq(&(CatenaFinitaCfg){.src = field[1], .other = "pointer", .cap = CLI_POOL_BYTES});
    const int named =
        (fields > 2u) && cellul.eq(&(CatenaFinitaCfg){.src = field[2], .other = "--as", .cap = CLI_POOL_BYTES});
    if (pointer_step && !named)
    {
        words[count] = "--as";
        words[count + 1u] = s_macros.sender;
        count += 2u;
    }
    for (unsigned int field_at = 2u; field_at < fields; field_at += 1u)
    {
        words[count] = field[field_at];
        count += 1u;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_sessions.send, CliSessionsCfg, .menu = field[0], .command = field[1], .request = words,
                    .count = count, .err_msg = &err_msg))
    {
        (void)mmgr_line_finish(&err_msg);
        mmgr_line_put(&s_macros.line, "\nstep ");
        mmgr_line_decimal(&s_macros.line, s_macros.step + 1u, 1u);
        mmgr_line_put(&s_macros.line, " not sent: ");
        mmgr_line_put(&s_macros.line, why);
        s_macros.done = 0;
        return 0;
    }
    return 1;
}

int cli_door_macro_send(const CliDoorMacroCfg *args)
{
    if (s_macros.sending || s_macros.pending)
    {
        mmgr_line_put(args->err_msg, "a macro is sent already");
        return 0;
    }
    DoorMacro macro;
    unsigned int at = 0u;
    if ((args->name == NULL) ||
        !EMBED_CALL(macro_named, DoorMacroOpCtx, .name = args->name, .macro = &macro, .at_out = &at, .make = 0))
    {
        mmgr_line_put(args->err_msg, "no macro is named ");
        mmgr_line_put(args->err_msg, (args->name != NULL) ? args->name : "");
        return 0;
    }
    if (macro.steps == 0u)
    {
        mmgr_line_put(args->err_msg, "the macro has no steps");
        return 0;
    }
    if (s_macros.recording && (s_macros.recorded == at))
    {
        mmgr_line_put(args->err_msg, "the macro still records");
        return 0;
    }
    VerbaLinea sender = {s_macros.sender, sizeof(s_macros.sender), 0ULL};
    if (args->sender != NULL)
    {
        mmgr_line_put(&sender, args->sender);
    }
    else if (!cli_machine_name(&sender))
    {
        mmgr_line_put(&sender, "sender");
    }
    (void)mmgr_line_finish(&sender);
    EMBED_CALL(macro_answer_begin, DoorMacroOpCtx, .done = 1);
    s_macros.sent = at;
    s_macros.step = 0u;
    s_macros.round = 0ULL;
    s_macros.rounds = (args->times == 0ULL) ? 1ULL : args->times;
    s_macros.ran = 0ULL;
    s_macros.sending = 1;
    if (!EMBED_CALL(macro_step_sent, DoorMacroOpCtx, .macro = &macro))
    {
        s_macros.sending = 0;
        s_macros.pending = 0;
        mmgr_line_put(args->err_msg, "the sessions took no request");
        return 0;
    }
    return 1;
}

int cli_door_macro_pending(void)
{
    return s_macros.pending;
}

int cli_door_macro_step(const CliDoorMacroCfg *args)
{
    if (!s_macros.pending)
    {
        return 0;
    }
    if (s_macros.sending)
    {
        char text[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea line = {text, sizeof(text), 0ULL};
        int done = 0;
        if (!EMBED_CALL(cli_sessions.step, CliSessionsCfg, .out = &line, .done = &done))
        {
            return 0;
        }
        (void)mmgr_line_finish(&line);
        s_macros.ran += 1ULL;
        if (!done)
        {
            // the hosts' lines of a step that was refused somewhere, after the step's number
            mmgr_line_put(&s_macros.line, "\nstep ");
            mmgr_line_decimal(&s_macros.line, s_macros.step + 1u, 1u);
            mmgr_line_put(&s_macros.line, ": ");
            mmgr_line_put(&s_macros.line, text);
            s_macros.done = 0;
        }
        DoorMacro macro;
        EMBED_CALL(macro_read, DoorMacroOpCtx, .at = s_macros.sent, .macro = &macro);
        s_macros.step += 1u;
        if (s_macros.step >= macro.steps)
        {
            s_macros.step = 0u;
            s_macros.round += 1ULL;
        }
        if ((s_macros.round < s_macros.rounds) && EMBED_CALL(macro_step_sent, DoorMacroOpCtx, .macro = &macro))
        {
            return 0;
        }
        s_macros.sending = 0;
        // the count goes before the lines gathered: the text is moved up by the count's length
        char head[64];
        VerbaLinea count = {head, sizeof(head), 0ULL};
        mmgr_line_decimal(&count, s_macros.ran, 1u);
        mmgr_line_put(&count, (s_macros.ran == 1ULL) ? " step on every host" : " steps on every host");
        (void)mmgr_line_finish(&count);
        char gathered[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea rest = {gathered, sizeof(gathered), 0ULL};
        mmgr_line_put_n(&rest, s_macros.text, s_macros.line.at);
        (void)mmgr_line_finish(&rest);
        s_macros.line = (VerbaLinea){s_macros.text, sizeof(s_macros.text), 0ULL};
        mmgr_line_put(&s_macros.line, head);
        mmgr_line_put(&s_macros.line, gathered);
    }
    (void)mmgr_line_finish(&s_macros.line);
    mmgr_line_put(args->out, s_macros.text);
    *args->done = s_macros.done;
    s_macros.pending = 0;
    return 1;
}

void cli_door_macro_answered(const CliDoorMacroCfg *args)
{
    EMBED_CALL(macro_answer_begin, DoorMacroOpCtx, .done = args->ok);
    mmgr_line_put_n(&s_macros.line, args->out->out, args->out->at);
}

void cli_door_macro_close(void)
{
    if (s_macros.cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_macros.cell, g_cli_comms_owner);
    }
    s_macros.cell = NULL;
    s_macros.recording = 0;
    s_macros.sending = 0;
    s_macros.pending = 0;
}
