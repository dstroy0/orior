// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// read.c: what is read: the text passed to the run, the records and the numbers and fields of a row, an oracle parsed
// and emptied, a code named and its family given, the blobs of a run retained where they are, and the oracle read
// from the run
#include "../../../../../../../includes/parsers/line_read.h"
#include "../../../../../../../includes/parsers/who.h"
#include "../../../../../../platform/platform.h"
#include "../../table_text.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "kdecode_internal.h"
#include "memoria_operor/memoria_operor.h"
#include "verba_scribo/verba_scribo.h"

KqrRecord s_record;
KrepFile s_file;

static char s_displayed_bytes[QRY_WRITER_RING_BYTES / 2ULL];
VerbaLinea s_displayed = {s_displayed_bytes, sizeof(s_displayed_bytes), 0ULL};

// what s_displayed contains passed on and emptied: to the run as KDECODE_DISPLAYED where this process is a member of
// one, and where it is a member of none, written to the standard error where `failure` is 1 and to the standard output
// where 0
static void kdecode_displayed_given(int failure)
{
    if (mmgr_line_ok(&s_displayed) && (s_displayed.at != 0ULL))
    {
        if (qry_member_run() != NULL)
        {
            qry_member_append(KDECODE_DISPLAYED, s_displayed.out, s_displayed.at, QRY_ORDINARY);
        }
        else
        {
            // a failure goes to the error stream, and the rows go to the line of the output stream
            if (failure != 0)
            {
                platform_terminal_write(1, s_displayed.out, (unsigned long long)s_displayed.at);
            }
            else
            {
                (void)platform_line_out_put(s_displayed.out, (unsigned long long)s_displayed.at);
            }
        }
    }
    s_displayed.at = 0ULL;
}

void kdecode_displayed_passed(int whole)
{
    if (!whole && (s_displayed.at < (s_displayed.capacity / 2ULL)))
    {
        return;
    }
    kdecode_displayed_given(0);
}

void kdecode_failure_passed(void)
{
    kdecode_displayed_given(1);
}

// the records of `file` from `from`, `count` of them or every one past it where `count` is 0, a line each: raw, its
// code and its numbers in hex, where `raw`; and where not, each who named and every other record with its code, its
// numbers and the texts it names by number after it
void kdecode_records(const KrepFile *file, int raw, unsigned int from, unsigned int count)
{
    const unsigned int past =
        (from >= file->records)
            ? from
            : (((count == 0u) || (count > (file->records - from))) ? file->records : (from + count));
    for (unsigned int at = from; at < past; at += 1u)
    {
        const KrepRecord *const record = &file->record[at];
        if (!raw && (record->code == KREP_RECORD_WHO) && (record->count == 2u))
        {
            char code[WHO_CODE_CHARACTERS + 1u];
            char named[KDECODE_NAMED];
            const Who who = who_of_words(record->word);
            who_code(who, code, sizeof(code));
            mmgr_line_put(&s_displayed, "who ");
            mmgr_line_put(&s_displayed, code);
            mmgr_line_ch(&s_displayed, ' ');
            mmgr_line_put(&s_displayed, who_named(who, named, sizeof(named)) ? named : "");
            mmgr_line_ch(&s_displayed, '\n');
            kdecode_displayed_passed(0);
            continue;
        }
        mmgr_line_hex(&s_displayed, record->code, 0u);
        for (unsigned int place = 0u; place < record->count; place += 1u)
        {
            mmgr_line_ch(&s_displayed, ' ');
            mmgr_line_hex(&s_displayed, record->word[place], 0u);
        }
        for (unsigned int place = 0u; !raw && (place < record->count); place += 1u)
        {
            unsigned long long length = 0ULL;
            const char *const text = krep_text(file, record->word[place], &length);
            if ((record->code != KREP_RECORD_WHO) && (text != NULL) && (record->word[place] != 0ULL))
            {
                mmgr_line_put(&s_displayed, " [");
                mmgr_line_hex(&s_displayed, record->word[place], 0u);
                mmgr_line_ch(&s_displayed, ' ');
                mmgr_line_put_n(&s_displayed, text, length);
                mmgr_line_ch(&s_displayed, ']');
            }
        }
        mmgr_line_ch(&s_displayed, '\n');
        kdecode_displayed_passed(0);
    }
}

// the number the word `word` writes into `value`: 1, or 0 where it is not one written plain in decimal
int kdecode_number(const char *word, unsigned long long *value)
{
    const char *at = word;
    return line_number(&at, 0, value) && (*at == '\0');
}

// `field` of the line `line`, its fields apart by tabs, copied into `word`, which has room for `room`, cut to it: 1, or
// 0 where the line has no such field
static int kdecode_field(const char *line, unsigned int field, char *word, size_t room)
{
    const char *at = line;
    for (unsigned int passed = 0u; passed < field; passed += 1u)
    {
        at = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\t');
        if (at == NULL)
        {
            return 0;
        }
        at += 1;
    }
    const size_t length =
        EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "\t\r\n");
    VerbaLinea copied = {word, room, 0ULL};
    mmgr_line_put_max(&copied, at, (length < room) ? length : (room - 1u));
    mmgr_line_finish(&copied);
    return 1;
}

// the oracle `bytes` contain, `size` of them, read into `oracle`: its codes, and where `codes_only` is 0 its row's type
// and its columns too
static void kdecode_oracle_parsed(const unsigned char *bytes, unsigned long long size, KdecodeOracle *oracle,
                                  int codes_only)
{
    static char s_line[KDECODE_LINE];
    unsigned long long at = 0ULL;
    while (line_read(bytes, size, &at, s_line, sizeof(s_line)))
    {
        char where[KDECODE_WORD];
        char type[KDECODE_WORD];
        char code[KDECODE_WORD];
        char form[KDECODE_WORD];
        if (!kdecode_field(s_line, 0u, where, sizeof(where)) || !kdecode_field(s_line, 2u, type, sizeof(type)) ||
            !kdecode_field(s_line, 3u, code, sizeof(code)) || !kdecode_field(s_line, 4u, form, sizeof(form)))
        {
            continue;
        }
        if (!codes_only &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = where, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "row",
                        .ci = EMBED_FALSE)) &&
            (oracle->type[0] == '\0'))
        {
            (void)verba_finis.finish(
                &(VerbaFinisCfg){.out = oracle->type,
                                 .cap = sizeof(oracle->type),
                                 .at = verba_textus.put_clip(&(VerbaTextusCfg){
                                     .out = oracle->type, .cap = sizeof(oracle->type), .at = 0u, .text = type})});
        }
        if (!codes_only && (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = where, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                       .other = "column ", .ci = EMBED_FALSE)))
        {
            const char *place_at = where + 7;
            unsigned long long place = 0ULL;
            line_number(&place_at, 0, &place);
            if (place < TABLE_ROW_COLUMNS)
            {
                // a column's place is below TABLE_ROW_COLUMNS, which an unsigned int represents
                KdecodeColumn *const column = &oracle->column[place];
                (void)verba_finis.finish(
                    &(VerbaFinisCfg){.out = column->name,
                                     .cap = sizeof(column->name),
                                     .at = verba_textus.put_clip(&(VerbaTextusCfg){
                                         .out = column->name, .cap = sizeof(column->name), .at = 0u, .text = type})});
                (void)verba_finis.finish(
                    &(VerbaFinisCfg){.out = column->form,
                                     .cap = sizeof(column->form),
                                     .at = verba_textus.put_clip(&(VerbaTextusCfg){
                                         .out = column->form, .cap = sizeof(column->form), .at = 0u, .text = form})});
                const char *const gloss = EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = s_line,
                                                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\t');
                const char *const named = (gloss != NULL)
                                              ? EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = gloss,
                                                           .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".oracle.tsv",
                                                           .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE)
                                              : NULL;
                const char *start = named;
                while ((start != NULL) && (start > gloss) &&
                       (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALNUM, .byte = (unsigned char)start[-1]) ||
                        (start[-1] == '_')))
                {
                    start -= 1;
                }
                if ((named != NULL) && (start < named))
                {
                    // the oracle's name, its stem and .oracle.tsv, is far shorter than its room
                    VerbaLinea codes_from = {column->codes_from, sizeof(column->codes_from), 0ULL};
                    mmgr_line_put_max(&codes_from, start,
                                      (unsigned long long)(named - start) + (sizeof(".oracle.tsv") - 1u));
                    mmgr_line_finish(&codes_from);
                }
                oracle->columns =
                    ((unsigned int)place >= oracle->columns) ? ((unsigned int)place + 1u) : oracle->columns;
            }
        }
        if ((EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = code, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "0x",
                        .ci = EMBED_FALSE)) &&
            (oracle->codes < KDECODE_CODES_MAX))
        {
            // a family numbered past its name, `record 1`, is the family its name gives
            KdecodeCode *const one = &oracle->code[oracle->codes];
            VerbaLinea family = {one->family, sizeof(one->family), 0ULL};
            mmgr_line_put_max(&family, where,
                              EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = where,
                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " "));
            mmgr_line_finish(&family);
            (void)verba_finis.finish(
                &(VerbaFinisCfg){.out = one->name,
                                 .cap = sizeof(one->name),
                                 .at = verba_textus.put_clip(&(VerbaTextusCfg){
                                     .out = one->name, .cap = sizeof(one->name), .at = 0u, .text = type})});
            const char *value_at = code;
            one->value = 0ULL;
            line_number(&value_at, 1, &one->value);
            oracle->codes += 1u;
        }
    }
}

void kdecode_oracle_free(KdecodeOracle *oracle)
{
    oracle->type[0] = '\0';
    EMBED_CALL(memor.set, MemoriaCfg, .dst = oracle->column, .bytes = sizeof(oracle->column), .val = 0);
    oracle->columns = 0u;
    oracle->codes = 0u;
}

// the name of the code `value` of the family `family` in `oracle`, or NULL where it gives none
const char *kdecode_code_named(const KdecodeOracle *oracle, const char *family, unsigned long long value)
{
    for (unsigned int at = 0u; at < oracle->codes; at += 1u)
    {
        if ((oracle->code[at].value == value) &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = oracle->code[at].family, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = family, .ci = EMBED_FALSE)))
        {
            return oracle->code[at].name;
        }
    }
    return NULL;
}

// 1 where `oracle` gives a code of the family `family`
int kdecode_family_given(const KdecodeOracle *oracle, const char *family)
{
    for (unsigned int at = 0u; at < oracle->codes; at += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = oracle->code[at].family, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = family, .ci = EMBED_FALSE))
        {
            return 1;
        }
    }
    return 0;
}

// the word at `place` of the `size` bytes at `bytes`, the low byte first, its bytes past `size` taken as zeros
static unsigned long long kdecode_word(const unsigned char *bytes, unsigned long long size, unsigned long long place)
{
    unsigned long long word = 0ULL;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        const unsigned long long byte = (place * 8ULL) + at;
        word |= (byte < size) ? ((unsigned long long)bytes[byte] << (8u * at)) : 0ULL;
    }
    return word;
}

// a text `length` bytes at `bytes` of the file `file` stored under `number`, where it is
static void kdecode_text_stored(KdecodeRun *run, unsigned long long file, unsigned long long number,
                                const unsigned char *bytes, unsigned long long length)
{
    if (run->texts >= KDECODE_RECORDS_MAX)
    {
        run->failed = 1;
        return;
    }
    run->text[run->texts] = (KdecodeText){file, number, bytes, length};
    run->texts += 1u;
}

// 1 where the `name_length` characters at `name` are the text `text`
static int kdecode_name_is(const char *name, unsigned int name_length, const char *text)
{
    return (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR) == name_length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = name, .other = text, .bytes = name_length) == 0);
}

// an oracle the run contains stored, `size` bytes at `bytes` under its name, in place of an earlier blob of the name:
// the blobs come in the order of their sequences, and the latest is retained
static void kdecode_content_stored(KdecodeRun *run, const char *name, unsigned int name_length,
                                   const unsigned char *bytes, unsigned long long size)
{
    size_t place = 0u;
    while ((place < run->content_count) && ((run->contents[place].name_length != name_length) ||
                                            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = run->contents[place].name,
                                                        .other = name, .bytes = name_length) != 0)))
    {
        place += 1u;
    }
    if ((place == run->content_count) && (run->content_count >= KDECODE_CONTENTS_MAX))
    {
        run->failed = 1;
        return;
    }
    run->contents[place] = (KdecodeContent){name, name_length, bytes, size};
    run->content_count += (place == run->content_count) ? 1u : 0u;
}

// one blob of the run, passed by kdecode_table in the order of the sequences: its sequence and name recorded; an oracle
// and a text of the run's table of texts recorded; and where it is of the table requested, a k-file's head and text
// recorded and a row of the range recorded, each where it is in the mapped run
void kdecode_blob(void *context, const char *name, unsigned int name_length, unsigned long long sequence,
                  const unsigned char *bytes, unsigned long long size)
{
    KdecodeRun *const run = (KdecodeRun *)context;
    if (run->failed)
    {
        return;
    }
    if (run->blobs >= KDECODE_RECORDS_MAX)
    {
        run->failed = 1;
        return;
    }
    run->blob[run->blobs] = (KdecodeBlob){sequence, name, name_length};
    run->blobs += 1u;
    const unsigned int suffix = (unsigned int)(sizeof(".oracle.tsv") - 1u);
    if ((name_length > suffix) && (EMBED_CALL(memor.cmp, MemoriaCfg, .src = name + name_length - suffix,
                                              .other = ".oracle.tsv", .bytes = suffix) == 0))
    {
        kdecode_content_stored(run, name, name_length, bytes, size);
    }
    const unsigned long long words = (size + 7ULL) / 8ULL;
    if (kdecode_name_is(name, name_length, TABLE_TEXTS) && (words >= 4ULL) &&
        (kdecode_word(bytes, size, 3ULL) <= (size - 32ULL)))
    {
        kdecode_text_stored(run, 0ULL, kdecode_word(bytes, size, 2ULL), bytes + 32u, kdecode_word(bytes, size, 3ULL));
    }
    if (!kdecode_name_is(name, name_length, run->table))
    {
        return;
    }
    const unsigned long long change = (words >= 4ULL) ? kdecode_word(bytes, size, 3ULL) : ~0ULL;
    if ((change == KREP_ROW_HEAD) && (words >= 5ULL) && (run->files < KDECODE_FILES_MAX))
    {
        KdecodeFile *const file = &run->file[run->files];
        file->key = sequence;
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            // the type's four bytes are the low first in its word
            file->type[at] = (char)((kdecode_word(bytes, size, 4ULL) >> (8u * at)) & 0xffULL);
        }
        file->type[4] = '\0';
        run->files += 1u;
    }
    if ((change == KREP_ROW_TEXT) && (words >= 6ULL) && (kdecode_word(bytes, size, 5ULL) <= (size - 48ULL)))
    {
        kdecode_text_stored(run, kdecode_word(bytes, size, 0ULL), kdecode_word(bytes, size, 4ULL), bytes + 48u,
                            kdecode_word(bytes, size, 5ULL));
    }
    const int ranged =
        (sequence >= run->from) && (sequence <= run->to) && ((run->max == 0ULL) || (run->rows < run->max));
    if (!ranged)
    {
        return;
    }
    if (run->rows >= KDECODE_RECORDS_MAX)
    {
        run->failed = 1;
        return;
    }
    // a row's words are its blob's, which the mapped run contains and a size_t counts
    run->row[run->rows] = (KdecodeRow){sequence, bytes, (size_t)words};
    run->rows += 1u;
}

// the oracle at `path` from the top of the tree read into `oracle` as kdecode_oracle_parsed reads it: the one the run
// read stores, the latest blob of its name, or where it stores none, the tree's, read from the top of the tree where
// orior_kdecode is. 1, or 0 where neither could be read
int kdecode_oracle_read(const KdecodeRun *run, const char *path, KdecodeOracle *oracle, int codes_only)
{
    for (size_t at = 0u; at < run->content_count; at += 1u)
    {
        if (kdecode_name_is(run->contents[at].name, run->contents[at].name_length, path))
        {
            kdecode_oracle_parsed(run->contents[at].bytes, run->contents[at].size, oracle, codes_only);
            return 1;
        }
    }
    // an oracle is far below a mebibyte of lines
    static unsigned char s_oracle[1ULL << 22u];
    const unsigned long long size = qry_member_file_read(path, s_oracle, sizeof(s_oracle));
    if (size == QRY_MEMBER_UNREAD)
    {
        return 0;
    }
    kdecode_oracle_parsed(s_oracle, size, oracle, codes_only);
    return 1;
}
