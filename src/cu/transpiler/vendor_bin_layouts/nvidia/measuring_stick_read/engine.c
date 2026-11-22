// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// engine.c: the engine: nvcc's listing read and its operations, the lane operations, the cells, the engine's writing
// read, the ruleset's operations, the engine of a kernel, the orders, and the counts written
#include "stick_internal.h"
#include "../../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "verba_scribo/verba_scribo.h"

// each kernel's operations from cuobjdump's listing at `path`, NOP left out
void stick_nvcc_read(const char *path)
{
    stick_read(path);
    const unsigned int nop = stick_named("NOP", 3u);
    StickKernel *kernel = NULL;
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        const char *const text = stick_spaced(s_file.line[line]);
        static const char s_function[] = "Function : ";
        if (mmgr_text_order(text, s_function, (size_t)(sizeof(s_function) - 1u)) == 0)
        {
            const char *const name = text + (sizeof(s_function) - 1u);
            const size_t length = strcspn(name, s_space);
            if (length != 0u)
            {
                unsigned int at = 0u;
                while ((at < s_kernels) && !((mmgr_text_order(s_kernel[at].name, name, (size_t)(length)) == 0) &&
                                             (s_kernel[at].name[length] == '\0')))
                {
                    at += 1u;
                }
                if (at == s_kernels)
                {
                    s_kernel = (StickKernel *)stick_allocated(
                        platform_cell_grown(s_kernel, s_kernels * sizeof(StickKernel),
                                            (s_kernels + 1u) * sizeof(StickKernel), platform_place_g_program_owner, 0));
                    mmgr_bytes_set(&s_kernel[at], 0, sizeof(StickKernel));
                    s_kernel[at].name = stick_copy(name, length);
                    s_kernels += 1u;
                }
                s_kernel[at].operations.count = 0u;
                kernel = &s_kernel[at];
                continue;
            }
        }
        unsigned int operation = 0u;
        if (stick_operation_of(s_file.line[line], &operation) && (operation != nop) && (kernel != NULL))
        {
            stick_number_add(&kernel->operations, operation);
        }
    }
}

// the operations of the listing nvdisasm wrote at `path`, in order, NOP left out, stored after `operations`
static void stick_listing_operations(const char *path, StickNumbers *operations)
{
    stick_read(path);
    const unsigned int nop = stick_named("NOP", 3u);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        unsigned int operation = 0u;
        if (stick_operation_of(s_file.line[line], &operation) && (operation != nop))
        {
            stick_number_add(operations, operation);
        }
    }
}

// the operations of the lane's text at `path`, in order, each instruction's first word, its guard and its `;` cut,
// NOP left out: the count of them, and the number of each at `operations`
static void stick_lane_operations(const char *path, StickNumbers *operations)
{
    stick_read(path);
    const unsigned int nop = stick_named("NOP", 3u);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        // the line, the space at either end cut
        char *const text = s_file.line[line] + strspn(s_file.line[line], s_space);
        size_t length = mmgr_text_length(text, PLATFORM_TEXT_TO_TERMINATOR);
        while ((length != 0u) && (mmgr_text_char(s_space, text[length - 1u], PLATFORM_TEXT_TO_TERMINATOR) != NULL))
        {
            length -= 1u;
        }
        text[length] = '\0';
        if ((length == 0u) || (mmgr_text_starts(text, "//", PLATFORM_TEXT_TO_TERMINATOR)) || (text[0] == '.') ||
            (text[length - 1u] == ':'))
        {
            continue;
        }
        const char *const word = text + stick_guard(text);
        size_t characters = strcspn(word, s_space);
        while ((characters != 0u) && (word[characters - 1u] == ';'))
        {
            characters -= 1u;
        }
        const unsigned int operation = stick_named(word, characters);
        if (operation != nop)
        {
            stick_number_add(operations, operation);
        }
    }
}

// the tab-apart cells of `line`, at max `max` of them, the last containing the rest of the line: their count, each
// cell a copy at `cell`
unsigned int stick_cells(const char *line, char **cell, unsigned int max)
{
    unsigned int count = 0u;
    const char *at = line;
    while (count < max)
    {
        const size_t length =
            ((count + 1u) == max) ? mmgr_text_length(at, PLATFORM_TEXT_TO_TERMINATOR) : strcspn(at, "\t");
        cell[count] = stick_copy(at, length);
        count += 1u;
        if (at[length] != '\t')
        {
            break;
        }
        at += length + 1u;
    }
    return count;
}

// each kernel's line of the engine's run at `table`, and for each it answers its operations as nvdisasm reads them
// back from `directory` and how many of them agree with the lane's text, position by position
void stick_engine_read(const char *table, const char *directory)
{
    stick_read(table);
    KfileLines lines = s_file;
    s_file = (KfileLines){NULL, 0u, 0u};
    for (unsigned int line = 0u; line < lines.lines; line += 1u)
    {
        char *cell[5] = {NULL, NULL, NULL, NULL, NULL};
        const unsigned int cells = stick_cells(lines.line[line], cell, 5u);
        if ((cells == 5u) && (!mmgr_text_equal(cell[0], "number", PLATFORM_TEXT_TO_TERMINATOR)))
        {
            char name[256];
            {
                VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
                mmgr_line_put(&buffer_line, s_prefix);
                mmgr_line_put(&buffer_line, cell[0]);
                (void)mmgr_line_finish(&buffer_line);
            }
            unsigned int at = 0u;
            while ((at < s_engines) && (!mmgr_text_equal(s_engine[at].name, name, PLATFORM_TEXT_TO_TERMINATOR)))
            {
                at += 1u;
            }
            if (at == s_engines)
            {
                s_engine = (StickEngine *)stick_allocated(platform_cell_grown(s_engine, s_engines * sizeof(StickEngine),
                                                                              (s_engines + 1u) * sizeof(StickEngine),
                                                                              platform_place_g_program_owner, 0));
                mmgr_bytes_set(&s_engine[at], 0, sizeof(StickEngine));
                s_engine[at].name = stick_copy(name, mmgr_text_length(name, PLATFORM_TEXT_TO_TERMINATOR));
                s_engines += 1u;
            }
            StickEngine *const entry = &s_engine[at];
            platform_cell_released(entry->answer, platform_place_g_program_owner);
            platform_cell_released(entry->note, platform_place_g_program_owner);
            entry->answer = stick_copy(cell[1], mmgr_text_length(cell[1], PLATFORM_TEXT_TO_TERMINATOR));
            entry->steps = (unsigned int)strtoul(cell[2], NULL, 10);
            entry->note = stick_copy(cell[4], mmgr_text_length(cell[4], PLATFORM_TEXT_TO_TERMINATOR));
            entry->operations.count = 0u;
            entry->agree = 0u;
            entry->text = 0u;
            if (mmgr_text_equal(cell[1], "answered", PLATFORM_TEXT_TO_TERMINATOR))
            {
                char path[1024];
                {
                    VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
                    mmgr_line_put(&buffer_line, directory);
                    mmgr_line_put(&buffer_line, "/");
                    mmgr_line_put(&buffer_line, cell[0]);
                    mmgr_line_put(&buffer_line, ".dis");
                    (void)mmgr_line_finish(&buffer_line);
                }
                stick_listing_operations(path, &entry->operations);
                StickNumbers text = {NULL, 0u, 0u};
                {
                    VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
                    mmgr_line_put(&buffer_line, directory);
                    mmgr_line_put(&buffer_line, "/");
                    mmgr_line_put(&buffer_line, cell[0]);
                    mmgr_line_put(&buffer_line, ".sass");
                    (void)mmgr_line_finish(&buffer_line);
                }
                stick_lane_operations(path, &text);
                for (unsigned int one = 0u; (one < text.count) && (one < entry->operations.count); one += 1u)
                {
                    entry->agree += (text.number[one] == entry->operations.number[one]) ? 1u : 0u;
                }
                entry->text = text.count;
                platform_cell_released(text.number, platform_place_g_program_owner);
            }
        }
        for (unsigned int one = 0u; one < cells; one += 1u)
        {
            platform_cell_released(cell[one], platform_place_g_program_owner);
        }
    }
    kfile_lines_free(&lines);
}

// the operations the SASS ruleset's forms write, each instruction of each form's text, its guard cut, marked in
// `buffer`, from sass.krs at `path`, then sm_86.krs and the blocks of the ruleset's language of sm_86.kdm beside it,
// then those of sass.ksc beside it
void stick_ruleset_operations(const char *path, unsigned char **buffer, unsigned int *room)
{
    // the folder sass.krs is in, to its last separator, and its stem, to the dot of its extension
    const char *slash = NULL;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        slash = ((*at == '/') || (*at == '\\')) ? at : slash;
    }
    const size_t folder = (slash != NULL) ? (size_t)(slash - path) : 0u;
    const char *const dot = strrchr(path, '.');
    const size_t stem = ((dot != NULL) && ((slash == NULL) || (dot > slash)))
                            ? (size_t)(dot - path)
                            : mmgr_text_length(path, PLATFORM_TEXT_TO_TERMINATOR);
    char files[4][1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(files[0], sizeof(files[0]));
        mmgr_line_put(&buffer_line, path);
        (void)mmgr_line_finish(&buffer_line);
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(files[1], sizeof(files[1]));
        mmgr_line_put_max(&buffer_line, path, (size_t)((int)folder));
        mmgr_line_put(&buffer_line, (slash != NULL) ? "/" : "");
        mmgr_line_put(&buffer_line, "sm_86.krs");
        (void)mmgr_line_finish(&buffer_line);
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(files[2], sizeof(files[2]));
        mmgr_line_put_max(&buffer_line, path, (size_t)((int)folder));
        mmgr_line_put(&buffer_line, (slash != NULL) ? "/" : "");
        mmgr_line_put(&buffer_line, "sm_86.kdm");
        (void)mmgr_line_finish(&buffer_line);
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(files[3], sizeof(files[3]));
        mmgr_line_put_max(&buffer_line, path, (size_t)((int)stem));
        mmgr_line_put(&buffer_line, ".ksc");
        (void)mmgr_line_finish(&buffer_line);
    }
    for (unsigned int file = 0u; file < 4u; file += 1u)
    {
        stick_read(files[file]);
        // a .krs is the ruleset's language whole; a .kdm and a .ksc contain it in its blocks (klang.h)
        unsigned int from = 0u;
        unsigned int first = 0u;
        unsigned int past = s_file.lines;
        const int whole = (file < 2u);
        while (whole || kfile_lines_block_lines(&s_file, "krs", from, &first, &past))
        {
            for (unsigned int line = first; line < past; line += 1u)
            {
                const char *const text = s_file.line[line];
                const char *const equals = mmgr_text_find(text, " = ", PLATFORM_TEXT_TO_TERMINATOR);
                if ((!mmgr_text_starts(text, "form ", PLATFORM_TEXT_TO_TERMINATOR)) || (equals == NULL))
                {
                    continue;
                }
                // each instruction of the form's text, apart by a written newline, a written tab a space
                const char *instruction = equals + 3;
                for (;;)
                {
                    const char *const newline = mmgr_text_find(instruction, "\\n", PLATFORM_TEXT_TO_TERMINATOR);
                    const size_t length = (newline != NULL)
                                              ? (size_t)(newline - instruction)
                                              : mmgr_text_length(instruction, PLATFORM_TEXT_TO_TERMINATOR);
                    char *const cut = stick_copy(instruction, length);
                    for (char *tab = mmgr_text_find(cut, "\\t", PLATFORM_TEXT_TO_TERMINATOR); tab != NULL;
                         tab = mmgr_text_find(cut, "\\t", PLATFORM_TEXT_TO_TERMINATOR))
                    {
                        tab[0] = ' ';
                        mmgr_bytes_move(&tab[1], &tab[2], mmgr_text_length(&tab[2], PLATFORM_TEXT_TO_TERMINATOR) + 1u);
                    }
                    // the instruction, the space at either end cut, then its guard
                    char *const start = cut + strspn(cut, s_space);
                    size_t size = mmgr_text_length(start, PLATFORM_TEXT_TO_TERMINATOR);
                    while ((size != 0u) &&
                           (mmgr_text_char(s_space, start[size - 1u], PLATFORM_TEXT_TO_TERMINATOR) != NULL))
                    {
                        size -= 1u;
                    }
                    start[size] = '\0';
                    const char *const word = start + stick_form_guard(start);
                    const size_t words = mmgr_text_length(word, PLATFORM_TEXT_TO_TERMINATOR);
                    if ((words != 0u) && (!mmgr_text_starts(word, "//", PLATFORM_TEXT_TO_TERMINATOR)) &&
                        (word[0] != '.') && (word[words - 1u] != ':'))
                    {
                        size_t characters = strcspn(word, s_space);
                        while ((characters != 0u) && (word[characters - 1u] == ';'))
                        {
                            characters -= 1u;
                        }
                        const unsigned int operation = stick_named(word, characters);
                        if (operation >= *room)
                        {
                            const unsigned int grown = (operation + 1u) * 2u;
                            *buffer = (unsigned char *)stick_allocated(
                                platform_cell_grown(*buffer, *room, grown, platform_place_g_program_owner, 0));
                            mmgr_bytes_set(&(*buffer)[*room], 0, grown - *room);
                            *room = grown;
                        }
                        (*buffer)[operation] = 1u;
                    }
                    platform_cell_released(cut, platform_place_g_program_owner);
                    if (newline == NULL)
                    {
                        break;
                    }
                    instruction = newline + 2;
                }
            }
            if (whole)
            {
                break;
            }
            from = (past > from) ? past : (from + 1u);
        }
    }
}

// the kernel named `name`, or NULL where nvcc's listing contains none
const StickEngine *stick_engine_of(const char *name)
{
    for (unsigned int at = 0u; at < s_engines; at += 1u)
    {
        if (mmgr_text_equal(s_engine[at].name, name, PLATFORM_TEXT_TO_TERMINATOR))
        {
            return &s_engine[at];
        }
    }
    return NULL;
}

// <0, 0 or >0 as the kernel at `left` is named before, with or after the one at `right`
int stick_kernel_order(const void *left, const void *right)
{
    return mmgr_text_order(((const StickKernel *)left)->name, ((const StickKernel *)right)->name,
                           PLATFORM_TEXT_TO_TERMINATOR);
}

// <0, 0 or >0 as the name of the operation at `left` is before, with or after the one at `right`
int stick_name_order(const void *left, const void *right)
{
    return mmgr_text_order(s_name[*(const unsigned int *)left], s_name[*(const unsigned int *)right],
                           PLATFORM_TEXT_TO_TERMINATOR);
}

// `text` with every `|` written `\|`, written into `record`, which has room for `room`, at `at`: the offset past it,
// `room` where it did not fit
size_t stick_escaped(char *record, size_t room, size_t at, const char *text)
{
    size_t written = at;
    for (const char *character = text; *character != '\0'; character += 1)
    {
        written = verba_textus.put_n(&(VerbaTextusCfg){.out = record,
                                                       .cap = room,
                                                       .at = written,
                                                       .text = (*character == '|') ? "\\|" : character,
                                                       .text_len = (*character == '|') ? 2u : 1u});
    }
    return written;
}

// the count of each operation of `operations` into `count`, one for each name of the table
void stick_counted(const StickNumbers *operations, unsigned int *count)
{
    mmgr_bytes_set(count, 0, s_names * sizeof(unsigned int));
    for (unsigned int at = 0u; at < operations->count; at += 1u)
    {
        count[operations->number[at]] += 1u;
    }
}

// each operation `count` contains more of than `less`, `<name> <more>`, apart by ", ", in the order of their names,
// written into `record`, which has room for `room`, at `at`; `less` NULL where none is taken away: the offset past
// them, `room` where they did not fit
size_t stick_counts_written(char *record, size_t room, size_t at, const unsigned int *order, const unsigned int *count,
                            const unsigned int *less)
{
    size_t written = at;
    int first = 1;
    for (unsigned int place = 0u; place < s_names; place += 1u)
    {
        const unsigned int operation = order[place];
        const unsigned int taken = (less != NULL) ? less[operation] : 0u;
        if (count[operation] > taken)
        {
            written = verba_textus.put(
                &(VerbaTextusCfg){.out = record, .cap = room, .at = written, .text = first ? "" : ", "});
            written = verba_textus.put(
                &(VerbaTextusCfg){.out = record, .cap = room, .at = written, .text = s_name[operation]});
            written = verba_littera.ch(&(VerbaLitteraCfg){.out = record, .cap = room, .at = written, .ch = ' '});
            written = verba_numerus.u64(
                &(VerbaNumerusCfg){.out = record, .cap = room, .at = written, .val = count[operation] - taken});
            first = 0;
        }
    }
    return written;
}
