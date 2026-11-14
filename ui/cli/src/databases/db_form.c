// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_form.c: the connection form as the Database pane's rows. Each field stands in a cell sized to its letters; the
// rows are places laid again as the type changes, the server's fields standing for PostgreSQL and MySQL and the file
// for SQLite, the guard's choice while the connection matters, and the refusal of the last save under the buttons. A
// save writes the connection into databases.json, a rename through the name it had, and a remove takes it out; the
// pane's text and keys go to the field that has the keys, or to the ask while the pane asks
#include "db_pane_internal.h"

// the types a connection is of, as databases.json stores them and as the choice reads them; the guards the same; the
// fields' labels; and the most bytes of a field
static const char *const s_types[CLI_DB_FORM_TYPES] = {"postgres", "mysql", "sqlite"};
static const char *const s_type_words[CLI_DB_FORM_TYPES] = {"PostgreSQL", "MySQL or MariaDB", "SQLite"};
static const char *const s_guards[CLI_DB_FORM_GUARDS] = {"confirm", "read-only"};
static const char *const s_guard_words[CLI_DB_FORM_GUARDS] = {"asks before each statement that writes",
                                                              "opens read-only"};
static const char *const s_field_labels[CLI_DB_FORM_FIELDS] = {"Name",     "Host", "Port",       "User",
                                                               "Database", "File", "Through ssh"};
static const char *const s_add_choices[1] = {"Add"};
static const char *const s_save_choices[2] = {"Save", "Remove"};
#define FORM_FIELD_MAX 1024ULL

typedef struct
{
    unsigned int at;
    const char *text;
    unsigned long long size;
    char *dst;
    const char *root;
} DbFormCtx;

void cli_db_pane_form_laid(const CliDbPaneCtx *args)
{
    (void)args;
    CliDbPaneForm *const form = &g_cli_db_pane_state.form;
    const int lite = form->type == 2u;
    unsigned int rows = 0u;
    form->row_place[rows] = CLI_DB_FORM_NAME;
    rows += 1u;
    form->row_place[rows] = CLI_DB_FORM_TYPE;
    rows += 1u;
    if (lite)
    {
        form->row_place[rows] = CLI_DB_FORM_FILE;
        rows += 1u;
    }
    else
    {
        static const unsigned int s_served[5] = {CLI_DB_FORM_HOST, CLI_DB_FORM_PORT, CLI_DB_FORM_USER,
                                                 CLI_DB_FORM_DATABASE, CLI_DB_FORM_SSH};
        for (unsigned int at = 0u; at < 5u; at += 1u)
        {
            form->row_place[rows] = s_served[at];
            rows += 1u;
        }
    }
    form->row_place[rows] = CLI_DB_FORM_MATTERS;
    rows += 1u;
    if (form->matters)
    {
        form->row_place[rows] = CLI_DB_FORM_GUARD;
        rows += 1u;
    }
    form->row_place[rows] = CLI_DB_FORM_BUTTONS;
    rows += 1u;
    if (form->refused)
    {
        form->row_place[rows] = CLI_DB_FORM_REFUSAL;
        rows += 1u;
    }
    form->rows = rows;
}

// the field `at` begun with `size` bytes of `text`
EMBED_INLINE void field_set(const DbFormCtx *args)
{
    CliDbPaneForm *const form = &g_cli_db_pane_state.form;
    EMBED_CALL(cli_typed.release, CliTypedCfg, .typed = &form->field[args->at], .owner = g_cli_db_pane_owner);
    if (args->size != 0ULL)
    {
        EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &form->field[args->at], .text = args->text, .size = args->size,
                   .most = FORM_FIELD_MAX, .owner = g_cli_db_pane_owner);
    }
}

void cli_db_pane_form_close(const CliDbPaneCtx *args)
{
    (void)args;
    CliDbPaneForm *const form = &g_cli_db_pane_state.form;
    for (unsigned int at = 0u; at < CLI_DB_FORM_FIELDS; at += 1u)
    {
        EMBED_CALL(cli_typed.release, CliTypedCfg, .typed = &form->field[at], .owner = g_cli_db_pane_owner);
    }
    memor.set(&(MemoriaCfg){.dst = form, .val = 0, .bytes = sizeof(*form)});
    form->focused = CLI_DB_PANE_NONE;
}

void cli_db_pane_form_open(const CliDbPaneCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    CliDbPaneForm *const form = &pane->form;
    EMBED_CALL(cli_db_pane_internal.form_close, CliDbPaneCtx, .at = 0u);
    form->open = 1;
    form->focused = CLI_DB_FORM_NAME;
    const int editing = args->editing && (pane->chosen != CLI_DB_PANE_NONE);
    form->editing = editing;
    if (editing)
    {
        // the chosen connection read out of the connections cell
        CliDbConnection connection_read;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = pane->connections_cell,
                                                                .offset = CLI_DB_PANE_CONNECTION_AT(pane->chosen),
                                                                .dst = &connection_read,
                                                                .bytes = sizeof(connection_read)});
        const CliDbConnection *const connection = &connection_read;
        (void)cellul.copy(&(CatenaFinitaCfg){.dst = form->was, .src = connection->name, .cap = CLI_NAME_MAX_LEN});
        const char *const texts[CLI_DB_FORM_FIELDS] = {connection->name, connection->host,     "",
                                                       connection->user, connection->database, connection->file,
                                                       connection->ssh};
        for (unsigned int at = 0u; at < CLI_DB_FORM_FIELDS; at += 1u)
        {
            EMBED_CALL(field_set, DbFormCtx, .at = at, .text = texts[at],
                       .size = cellul.len(&(CatenaFinitaCfg){.src = texts[at], .cap = CLI_PATH_MAX_LEN}));
        }
        if (connection->port != 0u)
        {
            char digits[12];
            VerbaLinea line = {digits, sizeof(digits), 0ULL};
            mmgr_line_decimal(&line, connection->port, 1u);
            EMBED_CALL(field_set, DbFormCtx, .at = CLI_DB_FORM_PORT, .text = digits, .size = line.at);
        }
        for (unsigned int at = 0u; at < CLI_DB_FORM_TYPES; at += 1u)
        {
            form->type = cellul.eq(&(CatenaFinitaCfg){.src = connection->type, .other = s_types[at], .cap = 16ULL})
                             ? at
                             : form->type;
        }
        form->matters = connection->matters;
        form->guard =
            cellul.eq(&(CatenaFinitaCfg){.src = connection->guard, .other = "read-only", .cap = 16ULL}) ? 1u : 0u;
    }
    EMBED_CALL(cli_db_pane_internal.form_laid, CliDbPaneCtx, .at = 0u);
}

int cli_db_pane_form_row(const CliDbPaneCtx *args)
{
    const CliDbPaneForm *const form = &g_cli_db_pane_state.form;
    if (args->at >= form->rows)
    {
        return 0;
    }
    CliDbPaneRow *const row = args->row;
    const unsigned int place = form->row_place[args->at];
    if (place < CLI_DB_FORM_FIELDS)
    {
        row->type = CLI_DB_PANE_ROW_FIELD;
        row->text = s_field_labels[place];
        row->text_length = cellul.len(&(CatenaFinitaCfg){.src = row->text, .cap = 32ULL});
        // the field's text copied out of the typed text's cell into the form's own buffer, which the caller reads
        // before the next row
        static char s_detail[FORM_FIELD_MAX];
        row->detail_length = EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed_read = &form->field[place], .dst = s_detail,
                                        .capacity = sizeof(s_detail));
        row->detail = s_detail;
        row->focused = form->focused == place;
        return 1;
    }
    switch (place)
    {
    case CLI_DB_FORM_TYPE:
        row->type = CLI_DB_PANE_ROW_CHOICE;
        row->text = "Kind";
        row->detail = s_type_words[form->type];
        break;
    case CLI_DB_FORM_MATTERS:
        row->type = CLI_DB_PANE_ROW_BOX;
        row->text = "It matters: its writes are guarded";
        row->on = form->matters;
        row->detail = "";
        break;
    case CLI_DB_FORM_GUARD:
        row->type = CLI_DB_PANE_ROW_CHOICE;
        row->text = "Guarded, it";
        row->detail = s_guard_words[form->guard];
        break;
    case CLI_DB_FORM_BUTTONS: {
        row->type = CLI_DB_PANE_ROW_BUTTONS;
        row->text = "";
        row->detail = "";
        const char *const *const choices = form->editing ? s_save_choices : s_add_choices;
        row->prompt.type = CLI_PROMPT_ROW_BUTTONS;
        row->prompt.choices = form->editing ? 2u : 1u;
        for (unsigned int at = 0u; at < row->prompt.choices; at += 1u)
        {
            row->prompt.choice[at] = choices[at];
            row->prompt.choice_length[at] = cellul.len(&(CatenaFinitaCfg){.src = choices[at], .cap = 16ULL});
        }
        return 1;
    }
    default:
        row->type = CLI_DB_PANE_ROW_NOTE;
        row->text = form->refusal;
        row->text_length = form->refusal_length;
        row->failed = 1;
        return 1;
    }
    row->text_length = cellul.len(&(CatenaFinitaCfg){.src = row->text, .cap = 64ULL});
    row->detail_length = cellul.len(&(CatenaFinitaCfg){.src = row->detail, .cap = 64ULL});
    return 1;
}

// the refusal of the last save set to `size` bytes of `text`, cut to the form's refusal bytes, none where `text` is
// NULL
EMBED_INLINE void refusal_set(const DbFormCtx *args)
{
    CliDbPaneForm *const form = &g_cli_db_pane_state.form;
    const unsigned long long size =
        (args->size < sizeof(form->refusal) - 1ULL) ? args->size : sizeof(form->refusal) - 1ULL;
    form->refused = args->text != NULL;
    if (form->refused)
    {
        memor.cpy(&(MemoriaCfg){.dst = form->refusal, .src = args->text, .bytes = size});
    }
    form->refusal[size] = '\0';
    form->refusal_length = form->refused ? size : 0ULL;
    EMBED_CALL(cli_db_pane_internal.form_laid, CliDbPaneCtx, .at = 0u);
}

// the field `at`'s text, with its end, copied into `dst` sized for `size` bytes
EMBED_INLINE void field_copied(const DbFormCtx *args)
{
    (void)EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed = &g_cli_db_pane_state.form.field[args->at], .dst = args->dst,
                     .capacity = args->size);
}

// the form saved into databases.json of the tree at `root`, the connection it edits renamed where its name changed,
// the pane's connections read again with the one saved chosen, and the form closed; a refusal under the buttons
EMBED_INLINE void form_saved(const DbFormCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    CliDbPaneForm *const form = &pane->form;
    CliDbConnection connection;
    memor.set(&(MemoriaCfg){.dst = &connection, .val = 0, .bytes = sizeof(connection)});
    EMBED_CALL(field_copied, DbFormCtx, .at = CLI_DB_FORM_NAME, .dst = connection.name, .size = CLI_NAME_MAX_LEN);
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = connection.type, .src = s_types[form->type], .cap = 16ULL});
    const int lite = form->type == 2u;
    if (lite)
    {
        EMBED_CALL(field_copied, DbFormCtx, .at = CLI_DB_FORM_FILE, .dst = connection.file, .size = CLI_PATH_MAX_LEN);
    }
    else
    {
        EMBED_CALL(field_copied, DbFormCtx, .at = CLI_DB_FORM_HOST, .dst = connection.host, .size = CLI_NAME_MAX_LEN);
        EMBED_CALL(field_copied, DbFormCtx, .at = CLI_DB_FORM_USER, .dst = connection.user, .size = CLI_NAME_MAX_LEN);
        EMBED_CALL(field_copied, DbFormCtx, .at = CLI_DB_FORM_DATABASE, .dst = connection.database,
                   .size = CLI_NAME_MAX_LEN);
        EMBED_CALL(field_copied, DbFormCtx, .at = CLI_DB_FORM_SSH, .dst = connection.ssh, .size = CLI_NAME_MAX_LEN);
        char port[FORM_FIELD_MAX];
        const unsigned long long port_length =
            EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed = &form->field[CLI_DB_FORM_PORT], .dst = port,
                       .capacity = sizeof(port));
        unsigned int value = 0u;
        for (unsigned long long at = 0ULL; at < port_length; at += 1ULL)
        {
            const char digit = port[at];
            // a digit's value is 0 to 9, and a port past five digits is no port
            value = ((digit >= '0') && (digit <= '9') && (value < 100000u))
                        ? (value * 10u) + (unsigned int)(digit - '0')
                        : value;
        }
        connection.port = value;
    }
    connection.matters = form->matters;
    (void)cellul.copy(
        &(CatenaFinitaCfg){.dst = connection.guard, .src = form->matters ? s_guards[form->guard] : "", .cap = 16ULL});
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_db_connection.save, CliDbConnectionCfg, .connection = &connection, .root = args->root,
                    .was = form->editing ? form->was : NULL, .err_msg = &err_msg))
    {
        EMBED_CALL(refusal_set, DbFormCtx, .text = why, .size = mmgr_line_finish(&err_msg));
        return;
    }
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = pane->chosen_name, .src = connection.name, .cap = CLI_NAME_MAX_LEN});
    EMBED_CALL(cli_db_pane_internal.form_close, CliDbPaneCtx, .at = 0u);
    EMBED_CALL(cli_db_pane.read, CliDbPaneCfg, .root = args->root);
    const char *const words[1] = {connection.name};
    (void)EMBED_CALL(cli_db_pane_tree.command, CliDbPaneTreeCfg, .name = "db-choose", .words = words, .count = 1u,
                     .root = args->root);
}

// the connection the form edits taken out of databases.json of the tree at `root`, closed where it is open, and the
// pane's connections read again; a refusal under the buttons
EMBED_INLINE void form_removed(const DbFormCtx *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    CliDbPaneForm *const form = &pane->form;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliDbConnection removed;
    if (!EMBED_CALL(cli_db_root_name.remove, CliDbRootNameCfg, .root = args->root, .name = form->was,
                    .connection_out = &removed, .err_msg = &err_msg))
    {
        EMBED_CALL(refusal_set, DbFormCtx, .text = why, .size = mmgr_line_finish(&err_msg));
        return;
    }
    pane->chosen_name[0] = '\0';
    EMBED_CALL(cli_db_pane_internal.form_close, CliDbPaneCtx, .at = 0u);
    EMBED_CALL(cli_db_pane.read, CliDbPaneCfg, .root = args->root);
}

int cli_db_pane_form_press(const CliDbPaneCtx *args)
{
    CliDbPaneForm *const form = &g_cli_db_pane_state.form;
    if (args->at >= form->rows)
    {
        return 0;
    }
    const unsigned int place = form->row_place[args->at];
    if (place < CLI_DB_FORM_FIELDS)
    {
        form->focused = place;
        return 1;
    }
    switch (place)
    {
    case CLI_DB_FORM_TYPE:
        form->type = (form->type + 1u) % CLI_DB_FORM_TYPES;
        break;
    case CLI_DB_FORM_MATTERS:
        form->matters = !form->matters;
        break;
    case CLI_DB_FORM_GUARD:
        form->guard = (form->guard + 1u) % CLI_DB_FORM_GUARDS;
        break;
    case CLI_DB_FORM_BUTTONS:
        if (args->choice == 0u)
        {
            EMBED_CALL(form_saved, DbFormCtx, .root = args->root);
        }
        else if (form->editing && (args->choice == 1u))
        {
            EMBED_CALL(form_removed, DbFormCtx, .root = args->root);
        }
        return 0;
    default:
        return 0;
    }
    form->focused = (form->focused < CLI_DB_FORM_FIELDS) ? form->focused : CLI_DB_PANE_NONE;
    EMBED_CALL(cli_db_pane_internal.form_laid, CliDbPaneCtx, .at = 0u);
    return 0;
}

void cli_db_pane_type(const CliDbPaneInputCfg *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    if (pane->asking && EMBED_CALL(cli_prompt.asked, CliPromptCfg, .context = pane))
    {
        EMBED_CALL(cli_prompt_input.type, CliPromptInputCfg, .text = args->text, .size = args->size);
        return;
    }
    CliDbPaneForm *const form = &pane->form;
    if (!form->open || (form->focused >= CLI_DB_FORM_FIELDS))
    {
        return;
    }
    EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &form->field[form->focused], .text = args->text, .size = args->size,
               .most = FORM_FIELD_MAX, .owner = g_cli_db_pane_owner);
}

int cli_db_pane_key(const CliDbPaneInputCfg *args)
{
    CliDbPaneState *const pane = &g_cli_db_pane_state;
    if (pane->asking && EMBED_CALL(cli_prompt.asked, CliPromptCfg, .context = pane))
    {
        return EMBED_CALL(cli_prompt_input.key, CliPromptInputCfg, .name = args->name);
    }
    CliDbPaneForm *const form = &pane->form;
    if (!form->open)
    {
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = 16ULL}))
    {
        EMBED_CALL(cli_db_pane_internal.form_close, CliDbPaneCtx, .at = 0u);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Enter", .cap = 16ULL}))
    {
        EMBED_CALL(form_saved, DbFormCtx, .root = args->root);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Tab", .cap = 16ULL}))
    {
        // the keys handed to the next field among the rows, the first after the last
        unsigned int at = 0u;
        while ((at < form->rows) && (form->row_place[at] != form->focused))
        {
            at += 1u;
        }
        for (unsigned int step = 1u; step <= form->rows; step += 1u)
        {
            const unsigned int place = form->row_place[(at + step) % form->rows];
            if (place < CLI_DB_FORM_FIELDS)
            {
                form->focused = place;
                break;
            }
        }
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Backspace", .cap = 16ULL}) &&
        (form->focused < CLI_DB_FORM_FIELDS))
    {
        EMBED_CALL(cli_typed.erase, CliTypedCfg, .typed = &form->field[form->focused], .owner = g_cli_db_pane_owner);
        return 1;
    }
    return 0;
}
