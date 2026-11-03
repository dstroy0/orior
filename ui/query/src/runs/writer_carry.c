// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// writer_machine_transfer.c: the run's records read: the latest of a name in the run while this process is a member,
// from the .qry after; every record of a name joined; the file at a path as the run stores it; and the latest record of
// names carried from a finished .qry into this run
#include "../../../cli/src/text/text.h"
#include "../query_internal.h"

// a record read out of a finished .qry, in a cell of its size retained until the next read of one
static unsigned char *s_read;

typedef struct
{
    const char *name;
    unsigned int length;
    const char *text;
    const QryRecord *record;
    const char *const *names;
    unsigned int count;
} WriterCarryCtx;
// 1 where the `length` letters at `name` are the text `text`
EMBED_INLINE int query_name_is(const WriterCarryCtx *args)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}) == args->length) &&
           (memor.cmp(&(MemoriaCfg){.src = args->name, .other = args->text, .bytes = args->length}) == 0);
}

int query_writer_latest(const QueryCountNameCtx *args)
{
    const unsigned char **bytes = args->bytes_u8s;
    unsigned long long *size = args->size_pointer;
    if (query_count_name_internal.writer_is_member(args))
    {
        return qry_member_blob(args->name, bytes, size, NULL);
    }
    unsigned long long mapped_size = 0ULL;
    const unsigned char *const mapped = qry_member_file_mapped(args->writer->qry, &mapped_size);
    if (mapped == NULL)
    {
        return 0;
    }
    unsigned long long at = 0ULL;
    const QryRecord *record = NULL;
    const QryRecord *latest = NULL;
    while (qry_record_read(mapped, mapped_size, &at, &record))
    {
        latest = EMBED_CALL(query_name_is, WriterCarryCtx, .name = QRY_RECORD_NAME(record),
                            .length = record->name_length, .text = args->name)
                     ? record
                     : latest;
    }
    g_cli_cells->persistent_buf_release(s_read, &g_query_cells);
    // one byte past the blob, so an empty blob is a cell too
    s_read = (latest != NULL) ? g_cli_cells->persistent_buf_alloc(latest->blob_length + 1ULL, &g_query_cells) : NULL;
    if (s_read != NULL)
    {
        // a record fits half a ring, whose bytes a size_t counts
        memor.cpy(&(MemoriaCfg){.dst = s_read, .src = QRY_RECORD_BLOB(latest), .bytes = (size_t)latest->blob_length});
        *bytes = s_read;
        *size = latest->blob_length;
    }
    qry_member_file_unmapped(mapped, mapped_size);
    return s_read != NULL;
}

// Every record of `name` joined into `joined`, in the order the writer wrote them: in the run while this process is a
// member, from the .qry after. 1, or 0 where none was there or they did not fit
int query_writer_read(const QueryCountNameCtx *args)
{
    if (query_count_name_internal.writer_is_member(args))
    {
        const unsigned long long capacity = args->joined->capacity - args->joined->at;
        const unsigned long long gathered =
            qry_member_gathered(args->name, (unsigned char *)(args->joined->out + args->joined->at), capacity);
        if ((gathered == QRY_MEMBER_UNREAD) || (gathered == 0ULL))
        {
            return 0;
        }
        args->joined->at += gathered;
        return 1;
    }
    unsigned long long mapped_size = 0ULL;
    const unsigned char *const mapped = qry_member_file_mapped(args->writer->qry, &mapped_size);
    if (mapped == NULL)
    {
        return 0;
    }
    unsigned long long at = 0ULL;
    const QryRecord *record = NULL;
    int found = 0;
    while (qry_record_read(mapped, mapped_size, &at, &record))
    {
        if (EMBED_CALL(query_name_is, WriterCarryCtx, .name = QRY_RECORD_NAME(record), .length = record->name_length,
                       .text = args->name))
        {
            mmgr_line_put_n(args->joined, (const char *)QRY_RECORD_BLOB(record), record->blob_length);
            found = 1;
        }
    }
    qry_member_file_unmapped(mapped, mapped_size);
    return found && mmgr_line_ok(args->joined);
}

// the bytes every record of `name` takes, in the run this process is a member of
EMBED_INLINE unsigned long long query_member_name_size(const WriterCarryCtx *args)
{
    unsigned long long at = 0ULL;
    unsigned long long size = 0ULL;
    const QryRecord *record = NULL;
    while (qry_member_read(&at, &record))
    {
        size += EMBED_CALL(query_name_is, WriterCarryCtx, .name = QRY_RECORD_NAME(record),
                           .length = record->name_length, .text = args->name)
                    ? record->blob_length
                    : 0ULL;
    }
    return size;
}

char *query_writer_read_alloc(const QueryCountNameCtx *args)
{
    unsigned long long *size = args->size_pointer;
    if (query_count_name_internal.writer_is_member(args))
    {
        for (;;)
        {
            const unsigned long long named = EMBED_CALL(query_member_name_size, WriterCarryCtx, .name = args->name);
            char *const joined =
                (named > 0ULL) ? g_cli_cells->persistent_buf_alloc(named + 1ULL, &g_query_cells) : NULL;
            if (joined == NULL)
            {
                return NULL;
            }
            const unsigned long long gathered = qry_member_gathered(args->name, (unsigned char *)joined, named);
            if (gathered != QRY_MEMBER_UNREAD)
            {
                joined[gathered] = '\0';
                *size = gathered;
                return joined;
            }
            // records of the name came between the count and the gathering, so they are counted again
            g_cli_cells->persistent_buf_release(joined, &g_query_cells);
        }
    }
    unsigned long long mapped_size = 0ULL;
    const unsigned char *const mapped = qry_member_file_mapped(args->writer->qry, &mapped_size);
    if (mapped == NULL)
    {
        return NULL;
    }
    unsigned long long at = 0ULL;
    unsigned long long named = 0ULL;
    const QryRecord *record = NULL;
    while (qry_record_read(mapped, mapped_size, &at, &record))
    {
        named += EMBED_CALL(query_name_is, WriterCarryCtx, .name = QRY_RECORD_NAME(record),
                            .length = record->name_length, .text = args->name)
                     ? record->blob_length
                     : 0ULL;
    }
    char *const joined = (named > 0ULL) ? g_cli_cells->persistent_buf_alloc(named + 1ULL, &g_query_cells) : NULL;
    unsigned long long put = 0ULL;
    at = 0ULL;
    while ((joined != NULL) && qry_record_read(mapped, mapped_size, &at, &record))
    {
        if (EMBED_CALL(query_name_is, WriterCarryCtx, .name = QRY_RECORD_NAME(record), .length = record->name_length,
                       .text = args->name))
        {
            // a record fits half a ring, whose bytes a size_t counts
            memor.cpy(&(MemoriaCfg){
                .dst = joined + put, .src = QRY_RECORD_BLOB(record), .bytes = (size_t)record->blob_length});
            put += record->blob_length;
        }
    }
    qry_member_file_unmapped(mapped, mapped_size);
    if (joined != NULL)
    {
        joined[put] = '\0';
        *size = put;
    }
    return joined;
}

// The file at `path` as the run stores it: its latest record, or the file where the run stores none, at `bytes`
int query_writer_current(const QueryCountNameCtx *args)
{
    return query_count_name_internal.writer_is_member(args) &&
           qry_member_file_blob(args->path, args->bytes_u8s, args->size_pointer);
}

// whether `record` is of a name carried: one of the `count` of `names`, or any where none are named
EMBED_INLINE int query_machine_transfer_want(const WriterCarryCtx *args)
{
    int required = (args->count == 0u);
    for (unsigned int each = 0u; !required && (each < args->count); each += 1u)
    {
        required = EMBED_CALL(query_name_is, WriterCarryCtx, .name = QRY_RECORD_NAME(args->record),
                              .length = args->record->name_length, .text = args->names[each]);
    }
    return required;
}

int query_writer_machine_transfer(const QueryCountNameCtx *args)
{
    if (!query_count_name_internal.writer_is_member(args))
    {
        mmgr_line_put(args->err_msg, "carry: this process is a member of no run");
        return 0;
    }
    unsigned long long mapped_size = 0ULL;
    const unsigned char *const mapped = qry_member_file_mapped(args->from, &mapped_size);
    if (mapped == NULL)
    {
        mmgr_line_put(args->err_msg, args->from);
        mmgr_line_put(args->err_msg, " could not be read whole");
        return 0;
    }
    // the records carried counted, then the latest record of each name carried, where it stands in the mapped .qry, in
    // a cell of one place for each
    unsigned long long at = 0ULL;
    const QryRecord *record = NULL;
    unsigned int records = 0u;
    while (qry_record_read(mapped, mapped_size, &at, &record))
    {
        records += EMBED_CALL(query_machine_transfer_want, WriterCarryCtx, .record = record, .names = args->names,
                              .count = args->count)
                       ? 1u
                       : 0u;
    }
    const QryRecord **const latest =
        (records > 0u)
            ? g_cli_cells->persistent_buf_alloc((unsigned long long)records * sizeof(QryRecord *), &g_query_cells)
            : NULL;
    unsigned int latests = 0u;
    at = 0ULL;
    while ((latest != NULL) && qry_record_read(mapped, mapped_size, &at, &record))
    {
        if (!EMBED_CALL(query_machine_transfer_want, WriterCarryCtx, .record = record, .names = args->names,
                        .count = args->count))
        {
            continue;
        }
        const char *const name = QRY_RECORD_NAME(record);
        unsigned int found_at = 0u;
        while ((found_at < latests) &&
               ((latest[found_at]->name_length != record->name_length) ||
                (memor.cmp(&(MemoriaCfg){
                     .src = QRY_RECORD_NAME(latest[found_at]), .other = name, .bytes = record->name_length}) != 0)))
        {
            found_at += 1u;
        }
        latests += (found_at == latests) ? 1u : 0u;
        latest[found_at] = record;
    }
    const int whole = (records == 0u) || ((latest != NULL) && (at == mapped_size));
    unsigned int carried = 0u;
    for (unsigned int each = 0u; whole && (each < latests); each += 1u)
    {
        char name[QRY_NAME_MAX_LEN + 1u];
        memor.cpy(&(MemoriaCfg){.dst = name, .src = QRY_RECORD_NAME(latest[each]), .bytes = latest[each]->name_length});
        name[latest[each]->name_length] = '\0';
        carried +=
            (qry_member_append(name, QRY_RECORD_BLOB(latest[each]), latest[each]->blob_length, QRY_ORDINARY) != 0ULL)
                ? 1u
                : 0u;
    }
    g_cli_cells->persistent_buf_release((void *)latest, &g_query_cells);
    qry_member_settled();
    qry_member_file_unmapped(mapped, mapped_size);
    if (!whole)
    {
        mmgr_line_put(args->err_msg, args->from);
        mmgr_line_put(args->err_msg, " could not be read whole");
        return 0;
    }
    mmgr_line_decimal(args->err_msg, carried, 0u);
    mmgr_line_put(args->err_msg, " records carried from ");
    mmgr_line_put(args->err_msg, args->from);
    return 1;
}

// The names listed in `list_entries`, one a line, carried from the finished .qry at `from`: what was carried reported
int query_writer_machine_transfer_list(const QueryFromWriterCtx *args)
{
    // the list copied into a cell of its size, each name trimmed of the spaces about it and ended by a 0 in place, and
    // a place for each line in a cell of their count
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = args->list_entries, .cap = CLI_CELLS_POOL});
    char *const names = g_cli_cells->persistent_buf_alloc(size + 1u, &g_query_cells);
    unsigned int lines = 1u;
    for (size_t at = 0u; at < size; at += 1u)
    {
        lines += (args->list_entries[at] == '\n') ? 1u : 0u;
    }
    const char **const named =
        (names != NULL) ? g_cli_cells->persistent_buf_alloc((unsigned long long)lines * sizeof(char *), &g_query_cells)
                        : NULL;
    if (named == NULL)
    {
        g_cli_cells->persistent_buf_release(names, &g_query_cells);
        mmgr_line_put(args->err_msg, "the names carried could not be stored");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = names, .src = args->list_entries, .bytes = size + 1u});
    unsigned int count = 0u;
    char *line = names;
    while (*line != '\0')
    {
        const size_t length = EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = line, .set = "\r\n");
        size_t first = 0u;
        first = (size_t)EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = line, .at = first,
                                   .size = length, .set = " \t");
        size_t last = length;
        last = (size_t)(first + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                           .text = line + first, .at = last - first, .set = " \t"));
        const size_t gap = EMBED_CALL(cli_text_set_table.over, CliTextSetCfg, .text = line + length, .set = "\r\n");
        if ((last > first) && ((last - first) <= QRY_NAME_MAX_LEN))
        {
            line[last] = '\0';
            named[count] = line + first;
            count += 1u;
        }
        line += length + gap;
    }
    const int carried = (count == 0u) || EMBED_CALL(query_count_name_internal.writer_machine_transfer,
                                                    QueryCountNameCtx, .writer = args->writer, .from = args->from,
                                                    .names = named, .count = count, .err_msg = args->err_msg);
    if (count == 0u)
    {
        mmgr_line_put(args->err_msg, "no records carried from ");
        mmgr_line_put(args->err_msg, args->from);
    }
    g_cli_cells->persistent_buf_release((void *)named, &g_query_cells);
    g_cli_cells->persistent_buf_release(names, &g_query_cells);
    return carried;
}
