// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tables_read.c: the instruction set of the vendor, read for the command line. The program names each encoding given by
// its class, with the bank, byte offset and bytes read of its constant operand, and the verdict of rule 13 against a
// bank size. Or the program counts the spans of the constant address fields that every class of the set uses
//
//     orior_tables_read <instructions.txt> class [--bank <size>] <low> <high>...
//     orior_tables_read <instructions.txt> spans
//
// `class` prints one line for each encoding:
// `<low> <high> <class> [bank <n> offset 0x<bytes> reads <bytes> <verdict>]`. The verdict is `gated` where rule 13
// blocks the read against the bank of `--bank` bytes, and `passes` where it does not. It is `unknown` where the caller
// gives no size, and the program then checks against the bank alone (cubin_safe/constant.c).
//
// `spans` prints each distinct pair of the span of a bank field and the span of an offset field. With it goes the count
// of classes that put their constant operand so: `bank <first>..<last> offset <first>..<last> <count>`. Exit 0, 2 where
// the words are not as above, and 1 where the program could not read the instruction set
#include "../../../../../../includes/platform/platform.h"
#include "../../../../engine/engine_note.h"
#include "../cubin_safe.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "read.h"

// the file at `path` whole, or NULL
static unsigned char *whole(const char *path, size_t *size)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return NULL;
    }
    unsigned char *bytes = NULL;
    size_t length = 0u;
    size_t room = 0u;
    for (;;)
    {
        if (length == room)
        {
            room = (room == 0u) ? (1u << 20u) : (room * 2u);
            unsigned char *const grown =
                (unsigned char *)platform_cell_grown(bytes, length, room, platform_place_g_program_owner, 0);
            if (grown == NULL)
            {
                platform_cell_released(bytes, platform_place_g_program_owner);
                platform_file_close(file);
                return NULL;
            }
            bytes = grown;
        }
        const size_t got = platform_file_read_items(bytes + length, 1u, room - length, file);
        if (got == 0u)
        {
            break;
        }
        length += got;
    }
    platform_file_close(file);
    *size = length;
    return bytes;
}

static int spans_printed(const TablesSet *set)
{
    unsigned int bank_first[256];
    unsigned int bank_last[256];
    unsigned int offset_first[256];
    unsigned int offset_last[256];
    unsigned int counted[256];
    unsigned int spans = 0u;
    for (unsigned int at = 0u; at < set->count; at += 1u)
    {
        const TablesClass *const class_ = &set->class_[at];
        const TablesField *const bank =
            (class_->bank[0] != '\0') ? tables_field_by_operand(class_, class_->bank) : NULL;
        const TablesField *const offset =
            (class_->offset[0] != '\0') ? tables_field_by_operand(class_, class_->offset) : NULL;
        if ((bank == NULL) || (offset == NULL))
        {
            continue;
        }
        // the lowest bit of every run and the highest, the fields' spans
        unsigned int span[4] = {bank->run[0].first, bank->run[0].last, offset->run[0].first, offset->run[0].last};
        for (unsigned int run = 1u; run < bank->runs; run += 1u)
        {
            span[0] = (bank->run[run].first < span[0]) ? bank->run[run].first : span[0];
            span[1] = (bank->run[run].last > span[1]) ? bank->run[run].last : span[1];
        }
        for (unsigned int run = 1u; run < offset->runs; run += 1u)
        {
            span[2] = (offset->run[run].first < span[2]) ? offset->run[run].first : span[2];
            span[3] = (offset->run[run].last > span[3]) ? offset->run[run].last : span[3];
        }
        unsigned int found = spans;
        for (unsigned int each = 0u; (each < spans) && (found == spans); each += 1u)
        {
            found = ((bank_first[each] == span[0]) && (bank_last[each] == span[1]) && (offset_first[each] == span[2]) &&
                     (offset_last[each] == span[3]))
                        ? each
                        : found;
        }
        if ((found == spans) && (spans < 256u))
        {
            bank_first[spans] = span[0];
            bank_last[spans] = span[1];
            offset_first[spans] = span[2];
            offset_last[spans] = span[3];
            counted[spans] = 0u;
            spans += 1u;
        }
        if (found < 256u)
        {
            counted[found] += 1u;
        }
    }
    for (unsigned int each = 0u; each < spans; each += 1u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "bank ");
        mmgr_line_decimal(output_line, bank_first[each], 1u);
        mmgr_line_put(output_line, "..");
        mmgr_line_decimal(output_line, bank_last[each], 1u);
        mmgr_line_put(output_line, " offset ");
        mmgr_line_decimal(output_line, offset_first[each], 1u);
        mmgr_line_put(output_line, "..");
        mmgr_line_decimal(output_line, offset_last[each], 1u);
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal(output_line, counted[each], 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}

int program_main(int count, char **words)
{
    if ((count < 3) || ((!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[2], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                     .other = "class", .ci = EMBED_FALSE)) &&
                        (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[2], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                     .other = "spans", .ci = EMBED_FALSE))))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "orior_tables_read <instructions.txt> class [--bank <size>] <low> "
                                             "<high>...\norior_tables_read <instructions.txt> spans");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    size_t size = 0u;
    unsigned char *const text = whole(words[1], &size);
    TablesSet set = {NULL, 0u, 0u};
    if ((text == NULL) || !tables_set_read(&set, text, size) || (set.count == 0u))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "orior_tables_read: ");
            mmgr_line_put(&engine_note.line, words[1]);
            mmgr_line_put(&engine_note.line, " contains no instruction set");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        platform_cell_released(text, platform_place_g_program_owner);
        return 1;
    }
    platform_cell_released(text, platform_place_g_program_owner);
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[2], .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "spans",
                   .ci = EMBED_FALSE))
    {
        const int code = spans_printed(&set);
        tables_set_freed(&set);
        return code;
    }
    int given = 3;
    unsigned long long bank_size = 0ULL;
    if (((given + 1) < count) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[given],
                                             .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "--bank", .ci = EMBED_FALSE)))
    {
        bank_size = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[given + 1], .end = NULL, .base = 0);
        given += 2;
    }
    for (; (given + 1) < count; given += 2)
    {
        const unsigned long long low =
            EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[given], .end = NULL, .base = 16);
        const unsigned long long high =
            EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[given + 1], .end = NULL, .base = 16);
        char line[256];
        const int classed = tables_class_described(&set, low, high, line, sizeof(line));
        const TablesClass *const class_ = classed ? tables_class_of(&set, low, high) : NULL;
        const int constant = (class_ != NULL) && (class_->bank[0] != '\0');
        const char *verdict = "";
        if (constant)
        {
            verdict = (bank_size == 0ULL)                                      ? " unknown"
                      : cubin_constant_out_of_bank(&set, low, high, bank_size) ? " gated"
                                                                               : " passes";
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, line);
            mmgr_line_put(output_line, verdict);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    tables_set_freed(&set);
    return 0;
}
