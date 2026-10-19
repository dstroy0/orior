// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// writer_carry.c: the run's records read: the latest of a name in the run while this process is a member, from the
// .qry after; every record of a name joined; the file at a path as the run holds it; and the latest record of names
// carried from a finished .qry into this run
#include "../../../cli/src/text/text.h"
#include "../query_internal.h"

// a record read out of a finished .qry, in a cell of its size kept until the next read of one
static unsigned char *s_read;

// 1 where the `length` letters at `name` are the text `text`
static int query_name_is(const char *name, unsigned int length, const char *text)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) == length) &&
           (memor.cmp(&(MemoriaCfg){.src = name, .other = text, .bytes = length}) == 0);
}

int query_writer_latest(const QueryWriter *writer, const char *name, const unsigned char **bytes,
                        unsigned long long *size)
{
    if (query_writer_member_held(writer))
    {
        return qry_member_blob(name, bytes, size, NULL);
    }
    unsigned long long mapped_size = 0ull;
    const unsigned char *const mapped = qry_member_file_mapped(writer->qry, &mapped_size);
    if (mapped == NULL)
    {
        return 0;
    }
    unsigned long long at = 0ull;
    const QryRecord *record = NULL;
    const QryRecord *latest = NULL;
    while (qry_record_read(mapped, mapped_size, &at, &record))
    {
        latest = query_name_is(QRY_RECORD_NAME(record), record->name_length, name) ? record : latest;
    }
    cli_cell_given_by(&g_query_cells, s_read);
    // one byte past the blob, so an empty blob is a cell too
    s_read = (latest != NULL)
                 ? cli_cell_taken_by(&g_query_cells, latest->blob_length + 1ull, "a record read from a finished run")
                 : NULL;
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
int query_writer_read(const QueryWriter *writer, const char *name, CliLine *joined)
{
    if (query_writer_member_held(writer))
    {
        const unsigned long long room = joined->capacity - joined->at;
        const unsigned long long gathered =
            qry_member_gathered(name, (unsigned char *)(joined->out + joined->at), room);
        if ((gathered == QRY_MEMBER_UNREAD) || (gathered == 0ull))
        {
            return 0;
        }
        joined->at += gathered;
        return 1;
    }
    unsigned long long mapped_size = 0ull;
    const unsigned char *const mapped = qry_member_file_mapped(writer->qry, &mapped_size);
    if (mapped == NULL)
    {
        return 0;
    }
    unsigned long long at = 0ull;
    const QryRecord *record = NULL;
    int found = 0;
    while (qry_record_read(mapped, mapped_size, &at, &record))
    {
        if (query_name_is(QRY_RECORD_NAME(record), record->name_length, name))
        {
            cli_line_counted(joined, (const char *)QRY_RECORD_BLOB(record), record->blob_length);
            found = 1;
        }
    }
    qry_member_file_unmapped(mapped, mapped_size);
    return found && cli_line_fits(joined);
}

// the bytes every record of `name` takes, in the run this process is a member of
static unsigned long long query_member_named_size(const char *name)
{
    unsigned long long at = 0ull;
    unsigned long long size = 0ull;
    const QryRecord *record = NULL;
    while (qry_member_read(&at, &record))
    {
        size += query_name_is(QRY_RECORD_NAME(record), record->name_length, name) ? record->blob_length : 0ull;
    }
    return size;
}

char *query_writer_read_taken(const QueryWriter *writer, const char *name, unsigned long long *size)
{
    if (query_writer_member_held(writer))
    {
        for (;;)
        {
            const unsigned long long named = query_member_named_size(name);
            char *const joined =
                (named > 0ull) ? cli_cell_taken_by(&g_query_cells, named + 1ull, "a run's records joined") : NULL;
            if (joined == NULL)
            {
                return NULL;
            }
            const unsigned long long gathered = qry_member_gathered(name, (unsigned char *)joined, named);
            if (gathered != QRY_MEMBER_UNREAD)
            {
                joined[gathered] = '\0';
                *size = gathered;
                return joined;
            }
            // records of the name came between the count and the gathering, so they are counted again
            cli_cell_given_by(&g_query_cells, joined);
        }
    }
    unsigned long long mapped_size = 0ull;
    const unsigned char *const mapped = qry_member_file_mapped(writer->qry, &mapped_size);
    if (mapped == NULL)
    {
        return NULL;
    }
    unsigned long long at = 0ull;
    unsigned long long named = 0ull;
    const QryRecord *record = NULL;
    while (qry_record_read(mapped, mapped_size, &at, &record))
    {
        named += query_name_is(QRY_RECORD_NAME(record), record->name_length, name) ? record->blob_length : 0ull;
    }
    char *const joined =
        (named > 0ull) ? cli_cell_taken_by(&g_query_cells, named + 1ull, "a run's records joined") : NULL;
    unsigned long long put = 0ull;
    at = 0ull;
    while ((joined != NULL) && qry_record_read(mapped, mapped_size, &at, &record))
    {
        if (query_name_is(QRY_RECORD_NAME(record), record->name_length, name))
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

// The file at `path` as the run holds it: its latest record, or the file where the run holds none, at `bytes`
int query_writer_current(const QueryWriter *writer, const char *path, const unsigned char **bytes,
                         unsigned long long *size)
{
    return query_writer_member_held(writer) && qry_member_file_blob(path, bytes, size);
}

// whether `record` is of a name carried: one of the `count` of `names`, or any where none are named
static int query_carry_wanted(const QryRecord *record, const char *const *names, unsigned int count)
{
    int wanted = (count == 0u);
    for (unsigned int each = 0u; !wanted && (each < count); each += 1u)
    {
        wanted = query_name_is(QRY_RECORD_NAME(record), record->name_length, names[each]);
    }
    return wanted;
}

int query_writer_carry(const QueryWriter *writer, const char *from, const char *const *names, unsigned int count,
                       CliLine *said)
{
    if (!query_writer_member_held(writer))
    {
        cli_line_text(said, "carry: this process is a member of no run");
        return 0;
    }
    unsigned long long mapped_size = 0ull;
    const unsigned char *const mapped = qry_member_file_mapped(from, &mapped_size);
    if (mapped == NULL)
    {
        cli_line_text(said, from);
        cli_line_text(said, " could not be read whole");
        return 0;
    }
    // the records carried counted, then the latest record of each name carried, where it stands in the mapped .qry, in
    // a cell of one place for each
    unsigned long long at = 0ull;
    const QryRecord *record = NULL;
    unsigned int records = 0u;
    while (qry_record_read(mapped, mapped_size, &at, &record))
    {
        records += query_carry_wanted(record, names, count) ? 1u : 0u;
    }
    const QryRecord **const latest =
        (records > 0u) ? cli_cell_taken_by(&g_query_cells, (unsigned long long)records * sizeof(QryRecord *),
                                           "the records carried")
                       : NULL;
    unsigned int latests = 0u;
    at = 0ull;
    while ((latest != NULL) && qry_record_read(mapped, mapped_size, &at, &record))
    {
        if (!query_carry_wanted(record, names, count))
        {
            continue;
        }
        const char *const name = QRY_RECORD_NAME(record);
        unsigned int kept = 0u;
        while ((kept < latests) &&
               ((latest[kept]->name_length != record->name_length) ||
                (memor.cmp(&(MemoriaCfg){
                     .src = QRY_RECORD_NAME(latest[kept]), .other = name, .bytes = record->name_length}) != 0)))
        {
            kept += 1u;
        }
        latests += (kept == latests) ? 1u : 0u;
        latest[kept] = record;
    }
    const int whole = (records == 0u) || ((latest != NULL) && (at == mapped_size));
    unsigned int carried = 0u;
    for (unsigned int each = 0u; whole && (each < latests); each += 1u)
    {
        char name[QRY_NAME_LONGEST + 1u];
        memor.cpy(&(MemoriaCfg){.dst = name, .src = QRY_RECORD_NAME(latest[each]), .bytes = latest[each]->name_length});
        name[latest[each]->name_length] = '\0';
        carried +=
            (qry_member_append(name, QRY_RECORD_BLOB(latest[each]), latest[each]->blob_length, QRY_ORDINARY) != 0ull)
                ? 1u
                : 0u;
    }
    cli_cell_given_by(&g_query_cells, (void *)latest);
    qry_member_settled();
    qry_member_file_unmapped(mapped, mapped_size);
    if (!whole)
    {
        cli_line_text(said, from);
        cli_line_text(said, " could not be read whole");
        return 0;
    }
    cli_line_decimal(said, carried, 0u);
    cli_line_text(said, " records carried from ");
    cli_line_text(said, from);
    return 1;
}

// The names listed in `listed`, one a line, carried from the finished .qry at `from`: what was carried said
int query_writer_carry_listed(const QueryWriter *writer, const char *from, const char *listed, CliLine *said)
{
    // the list copied into a cell of its size, each name trimmed of the spaces about it and ended by a 0 in place, and
    // a place for each line in a cell of their count
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = listed, .cap = CLI_CELLS_POOL});
    char *const names = cli_cell_taken_by(&g_query_cells, size + 1u, "the names carried");
    unsigned int lines = 1u;
    for (size_t at = 0u; at < size; at += 1u)
    {
        lines += (listed[at] == '\n') ? 1u : 0u;
    }
    const char **const named =
        (names != NULL)
            ? cli_cell_taken_by(&g_query_cells, (unsigned long long)lines * sizeof(char *), "the names carried")
            : NULL;
    if (named == NULL)
    {
        cli_cell_given_by(&g_query_cells, names);
        cli_line_text(said, "the names carried could not be held");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = names, .src = listed, .bytes = size + 1u});
    unsigned int count = 0u;
    char *line = names;
    while (*line != '\0')
    {
        const size_t length = cli_text_upto(line, "\r\n");
        size_t first = 0u;
        while ((first < length) && ((line[first] == ' ') || (line[first] == '\t')))
        {
            first += 1u;
        }
        size_t last = length;
        while ((last > first) && ((line[last - 1u] == ' ') || (line[last - 1u] == '\t')))
        {
            last -= 1u;
        }
        const size_t gap = cli_text_over(line + length, "\r\n");
        if ((last > first) && ((last - first) <= QRY_NAME_LONGEST))
        {
            line[last] = '\0';
            named[count] = line + first;
            count += 1u;
        }
        line += length + gap;
    }
    const int carried = (count == 0u) || query_writer_carry(writer, from, named, count, said);
    if (count == 0u)
    {
        cli_line_text(said, "no records carried from ");
        cli_line_text(said, from);
    }
    cli_cell_given_by(&g_query_cells, (void *)named);
    cli_cell_given_by(&g_query_cells, names);
    return carried;
}
