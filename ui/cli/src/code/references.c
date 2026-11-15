// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// references.c: the references to the name at a place and its rename, each the tree's answer (symbols_usages.c), the
// references in the order of their files, lines and columns, each file's text the editor's where it holds one. The
// references found go into one table in one movable cell: a record a file, a record a reference, then the paths and
// the lines' texts. A rename edits each file's view, from the file's last reference to its first, so each earlier
// place stays where the tree read it
#include "symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../editor/view.h"
#include "../servers/servers.h"
#include "../text/text.h"
#include "references.h"

// the max bytes the paths, the texts and the records each grow to as the tree answers
#define CLI_REFERENCES_CAPACITY (8ULL << 20u)

// One file of the table: where its path starts among the table's bytes, the path's bytes, the place of its name in
// the path, its first reference among the references, how many it has, and whether its rows are closed
typedef struct
{
    unsigned long long path;
    unsigned long long path_length;
    unsigned long long name;
    unsigned int first;
    unsigned int count;
    int closed;
} ReferencesFile;

// One reference of the table: where its line's text starts among the table's bytes and the text's bytes, its file, its
// line from 0, and its span in UTF-16 units
typedef struct
{
    unsigned long long text;
    unsigned long long length;
    unsigned int file;
    unsigned int line;
    unsigned int from;
    unsigned int to;
} Reference;

// The table: its cell, NULL for none, how many files and references it holds, where its bytes start in the cell, how many
// rows it lists, the name searched with its bytes, and the report with its bytes
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned int files;
    unsigned int references;
    unsigned long long bytes_at;
    unsigned int rows;
    char name[CLI_NAME_MAX_LEN];
    unsigned long long name_length;
    char report[CLI_ERR_MSG_MAX_LEN];
    unsigned long long report_length;
} References;

static References s_references;

// The references as the tree passes them: the files' records, the references' records and the bytes, each a line that grows,
// the path of the file the last reference was in, and whether every write fit
typedef struct
{
    CliGrownLine files;
    CliGrownLine ones;
    CliGrownLine bytes;
    char last_path[CLI_PATH_MAX_LEN];
    unsigned int count;
    int whole;
} ReferencesRead;

typedef struct
{
    const char *path;
    unsigned int line;
    unsigned int col;
    ReferencesRead *read;
    const ReferencesFile *file;
    unsigned long long at;
    const char *text;
    unsigned long long length;
    unsigned int file_at;
    unsigned int reference;
    VerbaLinea *err_msg;
    const char *root;
    unsigned int rows;
    unsigned int columns;
    const char *name;
    unsigned long long name_length;
    unsigned int *files_out;
    unsigned int *inside_out;
    char *out;
    unsigned long long capacity;
} ReferencesCtx;

// the place in the cell of the file `at_`'s record and of the reference `at_`'s record
#define REFERENCES_FILE_AT(at_) ((unsigned long long)(at_) * sizeof(ReferencesFile))
#define REFERENCES_ONE_AT(at_)                                                                                     \
    (REFERENCES_FILE_AT(s_references.files) + ((unsigned long long)(at_) * sizeof(Reference)))

// the text the editor holds of the tree's file at `path`, in a cell the tree gives back as the code's: NULL where it
// holds none
static char *references_open_text(const CliOpenTextCfg *args)
{
    unsigned long long size = 0ULL;
    char *const held = EMBED_CALL(cli_servers_path_size_type.text_alloc, CliServersPathSizeTypeCfg, .path = args->path,
                                  .size_pointer = &size);
    if (held == NULL)
    {
        return NULL;
    }
    char *const copy = g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_code_owner);
    if (copy != NULL)
    {
        // a text the editor holds fits a embed_word
        memor.cpy(&(MemoriaCfg){.dst = copy, .src = held, .bytes = (embed_word)size});
        copy[size] = '\0';
        *args->size = size;
    }
    g_cli_guard->persistent_buf_release(held, g_cli_servers_owner);
    return copy;
}

// one reference the tree passes: a file record where its path is not the last file's, its line's text and its record
static void references_each(const CliUsageCfg *args)
{
    ReferencesRead *const read = (ReferencesRead *)args->context;
    if (!read->whole)
    {
        return;
    }
    const unsigned long long path_length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN});
    const int same = (read->count != 0u) && cellul.eq(&(CatenaFinitaCfg){.src = read->last_path,
                                                                            .cap = CLI_PATH_MAX_LEN,
                                                                            .other = args->path,
                                                                            .other_cap = CLI_PATH_MAX_LEN});
    unsigned int files = (unsigned int)(cli_grown_size(&read->files) / sizeof(ReferencesFile));
    if (!same)
    {
        const ReferencesFile file = {cli_grown_size(&read->bytes), path_length,
                                        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                   .text = args->path, .at = path_length, .set = "/"),
                                        read->count, 0u, 0};
        (void)mmgr_line_put_n(&read->bytes.line, args->path, path_length);
        mmgr_line_ch(&read->bytes.line, '\0');
        (void)mmgr_line_put_n(&read->files.line, (const char *)&file, sizeof(file));
        memor.cpy(&(MemoriaCfg){.dst = read->last_path, .src = args->path, .bytes = (embed_word)(path_length + 1ULL)});
        files += 1u;
    }
    // the last file's count raised by one, the record read and written back
    ReferencesFile last;
    memor.cpy(&(MemoriaCfg){.dst = &last,
                            .src = read->files.cell + ((files - 1u) * sizeof(last)),
                            .bytes = sizeof(last)});
    last.count += 1u;
    memor.cpy(&(MemoriaCfg){.dst = read->files.cell + ((files - 1u) * sizeof(last)), .src = &last, .bytes = sizeof(last)});
    const Reference one = {cli_grown_size(&read->bytes), args->length, files - 1u, args->line, args->from, args->to};
    (void)mmgr_line_put_n(&read->bytes.line, args->text, args->length);
    mmgr_line_ch(&read->bytes.line, '\0');
    (void)mmgr_line_put_n(&read->ones.line, (const char *)&one, sizeof(one));
    read->count += 1u;
    read->whole = mmgr_line_ok(&read->bytes.line) && mmgr_line_ok(&read->files.line) && mmgr_line_ok(&read->ones.line);
}

// the rows counted: a row a file, and a row a reference under a file not closed
EMBED_INLINE void references_layout(const ReferencesCtx *args)
{
    (void)args;
    unsigned int rows = 0u;
    for (unsigned int at = 0u; at < s_references.files; at += 1u)
    {
        ReferencesFile file;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_references.cell, .offset = REFERENCES_FILE_AT(at), .dst = &file, .bytes = sizeof(file)});
        rows += 1u + (file.closed ? 0u : file.count);
    }
    s_references.rows = rows;
}

// the report written: the references and files counted with the name, or that none were found
EMBED_INLINE void references_report(const ReferencesCtx *args)
{
    (void)args;
    VerbaLinea line = {s_references.report, sizeof(s_references.report), 0ULL};
    if (s_references.references == 0u)
    {
        mmgr_line_put(&line, "No references");
    }
    else
    {
        mmgr_line_decimal(&line, s_references.references, 1u);
        mmgr_line_put(&line, (s_references.references == 1u) ? " reference of " : " references of ");
        (void)mmgr_line_put_n(&line, s_references.name, s_references.name_length);
        mmgr_line_put(&line, " in ");
        mmgr_line_decimal(&line, s_references.files, 1u);
        mmgr_line_put(&line, (s_references.files == 1u) ? " file" : " files");
    }
    s_references.report_length = mmgr_line_finish(&line);
}

// the name searched: the bytes of the first reference's span in its line
EMBED_INLINE void references_name_read(const ReferencesCtx *args)
{
    s_references.name_length = 0ULL;
    s_references.name[0] = '\0';
    if (s_references.references == 0u)
    {
        return;
    }
    const ReferencesRead *const read = args->read;
    Reference one;
    memor.cpy(&(MemoriaCfg){.dst = &one, .src = read->ones.cell, .bytes = sizeof(one)});
    const char *const text = read->bytes.cell + one.text;
    const unsigned long long from =
        EMBED_CALL(cli_text_at_size.unit_byte, CliTextAtSizeCfg, .text = text, .size = one.length, .at = one.from);
    const unsigned long long to =
        EMBED_CALL(cli_text_at_size.unit_byte, CliTextAtSizeCfg, .text = text, .size = one.length, .at = one.to);
    const unsigned long long length = ((to > from) && (to - from < sizeof(s_references.name))) ? to - from : 0ULL;
    memor.cpy(&(MemoriaCfg){.dst = s_references.name, .src = text + from, .bytes = (embed_word)length});
    s_references.name[length] = '\0';
    s_references.name_length = length;
}

// the references of the name at the place read from the tree into the table, the table before released: 1, 0 where the
// tree knows no name there, or -1 with why given
EMBED_INLINE int references_collect(const ReferencesCtx *args)
{
    cli_references_end();
    ReferencesRead read;
    memor.set(&(MemoriaCfg){.dst = &read, .val = 0, .bytes = sizeof(read)});
    cli_grown_begin(&read.files, CLI_REFERENCES_CAPACITY, g_cli_servers_owner);
    cli_grown_begin(&read.ones, CLI_REFERENCES_CAPACITY, g_cli_servers_owner);
    cli_grown_begin(&read.bytes, CLI_REFERENCES_CAPACITY, g_cli_servers_owner);
    read.whole = (read.files.cell != NULL) && (read.ones.cell != NULL) && (read.bytes.cell != NULL);
    int found = 0;
    if (!read.whole)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the references");
        found = -1;
    }
    else
    {
        found = EMBED_CALL(cli_symbols_path_context.usages, CliSymbolsPathContextCfg, .path = args->path,
                           .line = args->line, .col = args->col, .across = 1, .open_text = references_open_text,
                           .each_usage = references_each, .context = &read, .err_msg = args->err_msg);
    }
    if ((found == 1) && !read.whole)
    {
        mmgr_line_put(args->err_msg, "the references are more than the cells hold");
        found = -1;
    }
    if (found == 1)
    {
        const unsigned long long files_bytes = cli_grown_size(&read.files);
        const unsigned long long ones_bytes = cli_grown_size(&read.ones);
        const unsigned long long bytes = cli_grown_size(&read.bytes);
        s_references.cell = g_cli_guard->movable_buf_alloc(files_bytes + ones_bytes + bytes, g_cli_servers_owner);
        if (s_references.cell == NULL)
        {
            mmgr_line_put(args->err_msg, "the cells cannot hold the references");
            found = -1;
        }
        else
        {
            s_references.files = (unsigned int)(files_bytes / sizeof(ReferencesFile));
            s_references.references = read.count;
            s_references.bytes_at = files_bytes + ones_bytes;
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = s_references.cell, .offset = 0ULL, .src = read.files.cell, .bytes = (embed_word)files_bytes});
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = s_references.cell, .offset = files_bytes, .src = read.ones.cell, .bytes = (embed_word)ones_bytes});
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_references.cell,
                                                                     .offset = s_references.bytes_at,
                                                                     .src = read.bytes.cell,
                                                                     .bytes = (embed_word)bytes});
            EMBED_CALL(references_name_read, ReferencesCtx, .read = &read);
            EMBED_CALL(references_layout, ReferencesCtx, .at = 0ULL);
        }
    }
    cli_grown_end(&read.files);
    cli_grown_end(&read.ones);
    cli_grown_end(&read.bytes);
    EMBED_CALL(references_report, ReferencesCtx, .at = 0ULL);
    return found;
}

int cli_references_find(const CliReferencesPlaceCfg *args)
{
    return EMBED_CALL(references_collect, ReferencesCtx, .path = args->path, .line = args->line, .col = args->col,
                      .err_msg = args->err_msg);
}

// the file record `file_at` read out of the cell
// the reference record `reference` read out of the cell
// `length` bytes of the cell's bytes at `at` copied into `out`, cut to `capacity` less its end: the bytes copied
// whether `name` is a name: a letter or an underline first, letters, digits and underlines after
EMBED_INLINE int references_is_name(const ReferencesCtx *args)
{
    if (args->name_length == 0ULL)
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < args->name_length; at += 1ULL)
    {
        const char byte = args->name[at];
        const int letter = ((byte >= 'a') && (byte <= 'z')) || ((byte >= 'A') && (byte <= 'Z')) || (byte == '_');
        const int digit = (byte >= '0') && (byte <= '9');
        if (!letter && !(digit && (at != 0ULL)))
        {
            return 0;
        }
    }
    return 1;
}

// the view open on the tree's file at `path`: its place, or the views' count where none is
EMBED_INLINE unsigned int references_view_of(const ReferencesCtx *args)
{
    const unsigned int count = cli_view_count();
    char path[CLI_PATH_MAX_LEN];
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cli_view_path_at(at, path) && cellul.eq(&(CatenaFinitaCfg){.src = path,
                                                                        .cap = CLI_PATH_MAX_LEN,
                                                                        .other = args->path,
                                                                        .other_cap = CLI_PATH_MAX_LEN}))
        {
            return at;
        }
    }
    return count;
}

// the references of the file `file_at` written over with the name, from the last to the first, in the file's view, a file no
// view holds opened as one first: how many were written, or -1 with why given
EMBED_INLINE int references_file_rename(const ReferencesCtx *args)
{
    ReferencesFile file;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_references.cell, .offset = REFERENCES_FILE_AT(args->file_at), .dst = &file, .bytes = sizeof(file)});
    char path[CLI_PATH_MAX_LEN];
    // the bytes at `file.path`, cut to the room, and a zero after them
    const unsigned long long path_copied = (file.path_length < sizeof(path) - 1ULL) ? file.path_length : sizeof(path) - 1ULL;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_references.cell, .offset = s_references.bytes_at + file.path, .dst = path, .bytes = (embed_word)path_copied});
    path[path_copied] = '\0';
    unsigned int at = EMBED_CALL(references_view_of, ReferencesCtx, .path = path);
    const int opened_here = at == cli_view_count();
    if (opened_here)
    {
        const int opened =
            cli_view_file_open(args->root, path, args->rows, args->columns, NULL, args->err_msg);
        if (opened < 0)
        {
            return -1;
        }
        at = (unsigned int)opened;
    }
    char key[CLI_NAME_MAX_LEN];
    if (!cli_view_key(at, key))
    {
        mmgr_line_put(args->err_msg, "the view of ");
        mmgr_line_put(args->err_msg, path);
        mmgr_line_put(args->err_msg, " is gone");
        return -1;
    }
    int written = 0;
    for (unsigned int back = file.count; (written >= 0) && (back > 0u); back -= 1u)
    {
        Reference one;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_references.cell, .offset = REFERENCES_ONE_AT((file.first + back - 1u)), .dst = &one, .bytes = sizeof(one)});
        const CliViewInput replace = {.type = CLI_VIEW_REPLACE,
                                      .text = args->name,
                                      .length = args->name_length,
                                      .from = {one.line, one.from},
                                      .to = {one.line, one.to}};
        written = EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &replace,
                             .err_msg = args->err_msg)
                      ? written + 1
                      : -1;
    }
    // a file no tab held is written and its view closed again, as VS Code saves the files a refactoring changes
    if (opened_here)
    {
        char full[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = path, .joined = full);
        if ((written >= 0) &&
            !EMBED_CALL(cli_view_save_table.save, CliViewSaveCfg, .key = key, .full = full, .err_msg = args->err_msg))
        {
            written = -1;
        }
        cli_view_file_close(at);
    }
    return written;
}

int cli_references_rename(const CliReferencesPlaceCfg *args)
{
    const unsigned long long name_length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_NAME_MAX_LEN});
    if (!EMBED_CALL(references_is_name, ReferencesCtx, .name = args->name, .name_length = name_length))
    {
        mmgr_line_put(args->err_msg, "the new name is no name");
        return 0;
    }
    const int found = EMBED_CALL(references_collect, ReferencesCtx, .path = args->path, .line = args->line,
                                 .col = args->col, .err_msg = args->err_msg);
    if (found != 1)
    {
        return found;
    }
    const int displayed = cli_view_displayed();
    int written = 0;
    for (unsigned int at = 0u; (written >= 0) && (at < s_references.files); at += 1u)
    {
        const int file_written = EMBED_CALL(references_file_rename, ReferencesCtx, .file_at = at, .root = args->root,
                                            .rows = args->rows, .columns = args->columns, .name = args->name,
                                            .name_length = name_length, .err_msg = args->err_msg);
        written = (file_written < 0) ? -1 : written + file_written;
    }
    *args->files_out = s_references.files;
    // the view displayed before is displayed again, the files opened for the rename behind it
    if ((displayed >= 0) && (cli_view_displayed() != displayed))
    {
        cli_view_show((unsigned int)displayed);
    }
    cli_references_end();
    return written;
}

unsigned int cli_references_rows(void)
{
    return (s_references.cell != NULL) ? s_references.rows : 0u;
}

// the file whose rows the row `at` is in and the row's place from the file's own: 1 with them, or 0 past the rows
EMBED_INLINE int references_row_find(const ReferencesCtx *args)
{
    unsigned int row = 0u;
    for (unsigned int file_at = 0u; file_at < s_references.files; file_at += 1u)
    {
        ReferencesFile file;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_references.cell, .offset = REFERENCES_FILE_AT(file_at), .dst = &file, .bytes = sizeof(file)});
        const unsigned int rows = 1u + (file.closed ? 0u : file.count);
        if (args->at < (unsigned long long)row + rows)
        {
            *args->files_out = file_at;
            *args->inside_out = (unsigned int)(args->at - row);
            return 1;
        }
        row += rows;
    }
    return 0;
}

int cli_references_row(unsigned int at, CliReferenceRow *row)
{
    unsigned int file_at = 0u;
    unsigned int inside = 0u;
    if ((s_references.cell == NULL) ||
        !EMBED_CALL(references_row_find, ReferencesCtx, .at = at, .files_out = &file_at, .inside_out = &inside))
    {
        return 0;
    }
    ReferencesFile file;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_references.cell, .offset = REFERENCES_FILE_AT(file_at), .dst = &file, .bytes = sizeof(file)});
    memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
    // the bytes at `file.path`, cut to the room, and a zero after them
    const unsigned long long path_copied = (file.path_length < sizeof(row->path_copied) - 1ULL) ? file.path_length : sizeof(row->path_copied) - 1ULL;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_references.cell, .offset = s_references.bytes_at + file.path, .dst = row->path_copied, .bytes = (embed_word)path_copied});
    row->path_copied[path_copied] = '\0';
    row->path = row->path_copied;
    if (inside == 0u)
    {
        row->type = CLI_REFERENCES_ROW_FILE;
        row->name = row->path_copied + file.name;
        row->name_length = file.path_length - file.name;
        row->folder_length = file.name;
        row->count = file.count;
        row->open = !file.closed;
        return 1;
    }
    Reference one;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_references.cell, .offset = REFERENCES_ONE_AT((file.first + inside - 1u)), .dst = &one, .bytes = sizeof(one)});
    row->type = CLI_REFERENCES_ROW_PLACE;
    row->line = one.line;
    row->col = one.from;
    row->to = one.to;
    // the bytes at `one.text`, cut to the room, and a zero after them
    const unsigned long long text_copied = (one.length < sizeof(row->text_copied) - 1ULL) ? one.length : sizeof(row->text_copied) - 1ULL;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_references.cell, .offset = s_references.bytes_at + one.text, .dst = row->text_copied, .bytes = (embed_word)text_copied});
    row->text_copied[text_copied] = '\0';
    row->text_length = text_copied;
    row->text = row->text_copied;
    row->match_at = EMBED_CALL(cli_text_at_size.unit_byte, CliTextAtSizeCfg, .text = row->text,
                               .size = row->text_length, .at = one.from);
    const unsigned long long match_end = EMBED_CALL(cli_text_at_size.unit_byte, CliTextAtSizeCfg, .text = row->text,
                                                    .size = row->text_length, .at = one.to);
    row->match_length = (match_end > row->match_at) ? match_end - row->match_at : 0ULL;
    return 1;
}

int cli_references_press(unsigned int at, CliReferenceRow *row)
{
    if (!cli_references_row(at, row))
    {
        return 0;
    }
    if (row->type == CLI_REFERENCES_ROW_PLACE)
    {
        return 1;
    }
    unsigned int file_at = 0u;
    unsigned int inside = 0u;
    (void)EMBED_CALL(references_row_find, ReferencesCtx, .at = at, .files_out = &file_at, .inside_out = &inside);
    ReferencesFile file;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_references.cell, .offset = REFERENCES_FILE_AT(file_at), .dst = &file, .bytes = sizeof(file)});
    file.closed = !file.closed;
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = s_references.cell, .offset = REFERENCES_FILE_AT(file_at), .src = &file, .bytes = sizeof(file)});
    EMBED_CALL(references_layout, ReferencesCtx, .at = 0ULL);
    return 0;
}

const char *cli_references_report(unsigned long long *length_out)
{
    *length_out = s_references.report_length;
    return s_references.report;
}

void cli_references_end(void)
{
    if (s_references.cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_references.cell, g_cli_servers_owner);
    }
    memor.set(&(MemoriaCfg){.dst = &s_references, .val = 0, .bytes = sizeof(s_references)});
}
