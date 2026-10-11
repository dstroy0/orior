// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// measuring_stick_read.c: nvcc's listing of the measuring stick read kernel by kernel, the answer key what the engine
// builds for each function of the language is held against. A kernel's operations are its instructions' operations,
// the guard cut and NOP left out, each as many times as it is written. Every operation nvcc writes that no form of
// sass.krs writes is where sass.krs falls short, and the record counts the kernels each is in.
//
// Given the run of measuring_stick_engine.sh, each kernel it writes through cu.krs and sass.krs is held against nvcc's:
// the operations nvdisasm reads back from the kernel's encodings, against the operations of nvcc's listing. A kernel is
// at parity where the two are the same multiset. The read back is checked against the kernel's own text, instruction by
// instruction: the assembler and the disassembler agree on every operation the comparison counts.
//
// Each operation is held as a number, the place of its name in the run's table of names, and compared as one. Every
// file is read as the run holds it (ktext.h), and the record is handed to the run's writer, which writes it.
//
//     measuring_stick_read <nvcc listing> <manifest> <sass.krs> <record> [<engine.tsv> <engine directory>]
#include "../../../types/file_defs/ktext.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the letters Python's split takes as space
static const char s_space[] = " \t\n\r\v\f\x1c\x1d\x1e\x1f";

// the start of each kernel's name
static const char s_prefix[] = "measuring_stick_";

// a list of numbers, each an operation's place in the table of names
typedef struct
{
    unsigned int *number;
    unsigned int count;
    unsigned int room;
} StickNumbers;

// a kernel: its name, nvcc's operations, and where the manifest names it, its category and its function
typedef struct
{
    char *name;
    StickNumbers operations;
    const char *category;
    const char *function;
} StickKernel;

// a kernel's line of the engine's run: its answer, its steps, its note, and where it answers, its operations as
// nvdisasm reads them back, how many agree with the lane's text position by position, and the count of the text's
typedef struct
{
    char *name;
    char *answer;
    unsigned int steps;
    char *note;
    StickNumbers operations;
    unsigned int agree;
    unsigned int text;
} StickEngine;

// the table of names: each operation's name at its number, and a hash of the names to their numbers
static char **s_name;
static unsigned int s_names;
static unsigned int s_name_room;
#define STICK_HASH 65536u
static unsigned int s_hash[STICK_HASH];

static StickKernel *s_kernel;
static unsigned int s_kernels;
static StickEngine *s_engine;
static unsigned int s_engines;
static KtextFile s_file;

// `pointer`, or the run ended where the system gave no memory
static void *stick_held(void *pointer)
{
    if (pointer == NULL)
    {
        fputs("measuring_stick_read: the system gave no memory\n", stderr);
        exit(2);
    }
    return pointer;
}

// `length` letters at `text` copied into an allocation of their own, ended
static char *stick_copy(const char *text, size_t length)
{
    char *const copy = (char *)stick_held(malloc(length + 1u));
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

// `number` kept after every number `numbers` holds
static void stick_number_add(StickNumbers *numbers, unsigned int number)
{
    if (numbers->count == numbers->room)
    {
        numbers->room = (numbers->room == 0u) ? 64u : (numbers->room * 2u);
        numbers->number = (unsigned int *)stick_held(realloc(numbers->number, numbers->room * sizeof(unsigned int)));
    }
    numbers->number[numbers->count] = number;
    numbers->count += 1u;
}

// the number of the name `length` letters at `text`, the name put in the table where it holds none
static unsigned int stick_named(const char *text, size_t length)
{
    // FNV-1a over the name's letters
    unsigned int hash = 2166136261u;
    for (size_t at = 0u; at < length; at += 1u)
    {
        hash = (hash ^ (unsigned char)text[at]) * 16777619u;
    }
    unsigned int slot = hash & (STICK_HASH - 1u);
    while (s_hash[slot] != 0u)
    {
        const char *const held = s_name[s_hash[slot] - 1u];
        if ((strncmp(held, text, length) == 0) && (held[length] == '\0'))
        {
            return s_hash[slot] - 1u;
        }
        slot = (slot + 1u) & (STICK_HASH - 1u);
    }
    if (s_names == (STICK_HASH / 2u))
    {
        fputs("measuring_stick_read: more operations than the table holds\n", stderr);
        exit(2);
    }
    if (s_names == s_name_room)
    {
        s_name_room = (s_name_room == 0u) ? 256u : (s_name_room * 2u);
        s_name = (char **)stick_held(realloc(s_name, s_name_room * sizeof(char *)));
    }
    s_name[s_names] = stick_copy(text, length);
    s_names += 1u;
    s_hash[slot] = s_names;
    return s_names - 1u;
}

// the file at `path` read into s_file as the run holds it
static void stick_read(const char *path)
{
    if (!ktext_read(&s_file, path))
    {
        fputs("measuring_stick_read: the system gave no memory\n", stderr);
        exit(2);
    }
}

// the place past the space at `text`
static const char *stick_spaced(const char *text)
{
    return text + strspn(text, s_space);
}

// the length of a guard at `text`, `@!?U?P[T0-9]+` and the space after it, or 0 where none is there
static size_t stick_guard(const char *text)
{
    size_t at = 0u;
    if (text[at] != '@')
    {
        return 0u;
    }
    at += 1u;
    at += (text[at] == '!') ? 1u : 0u;
    at += (text[at] == 'U') ? 1u : 0u;
    if (text[at] != 'P')
    {
        return 0u;
    }
    at += 1u;
    const size_t marks = strspn(&text[at], "T0123456789");
    const size_t space = (marks != 0u) ? strspn(&text[at + marks], s_space) : 0u;
    return (space != 0u) ? (at + marks + space) : 0u;
}

// the length of a form's guard at `text`, `@!?{?[A-Za-z0-9_]+}?` and the space after it, or 0 where none is there
static size_t stick_form_guard(const char *text)
{
    size_t at = 0u;
    if (text[at] != '@')
    {
        return 0u;
    }
    at += 1u;
    at += (text[at] == '!') ? 1u : 0u;
    at += (text[at] == '{') ? 1u : 0u;
    const size_t name = strspn(&text[at], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
    if (name == 0u)
    {
        return 0u;
    }
    at += name;
    at += (text[at] == '}') ? 1u : 0u;
    const size_t space = strspn(&text[at], s_space);
    return (space != 0u) ? (at + space) : 0u;
}

// 1 with the number of the operation of a listing's instruction line `line`, its guard cut: `/*<hex>*/`, then the
// instruction up to its `;`, its first word the operation; 0 where the line holds none
static int stick_operation_of(const char *line, unsigned int *operation)
{
    const char *at = stick_spaced(line);
    if (strncmp(at, "/*", 2u) != 0)
    {
        return 0;
    }
    at += 2;
    const size_t digits = strspn(at, "0123456789abcdef");
    if ((digits < 4u) || (strncmp(&at[digits], "*/", 2u) != 0))
    {
        return 0;
    }
    at += digits + 2u;
    const size_t space = strspn(at, s_space);
    if (space == 0u)
    {
        return 0;
    }
    at += space;
    const char *const end = strchr(at, ';');
    if (end == NULL)
    {
        return 0;
    }
    // the instruction, the space before its `;` cut
    size_t length = (size_t)(end - at);
    while ((length != 0u) && (strchr(s_space, at[length - 1u]) != NULL))
    {
        length -= 1u;
    }
    char *const instruction = stick_copy(at, length);
    const char *const text = stick_spaced(instruction + stick_guard(instruction));
    const size_t word = strcspn(text, s_space);
    const int found = (word != 0u);
    if (found)
    {
        *operation = stick_named(text, word);
    }
    free(instruction);
    return found;
}

// each kernel's operations from cuobjdump's listing at `path`, NOP left out
static void stick_nvcc_read(const char *path)
{
    stick_read(path);
    const unsigned int nop = stick_named("NOP", 3u);
    StickKernel *kernel = NULL;
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        const char *const text = stick_spaced(s_file.line[line]);
        static const char s_function[] = "Function : ";
        if (strncmp(text, s_function, sizeof(s_function) - 1u) == 0)
        {
            const char *const name = text + (sizeof(s_function) - 1u);
            const size_t length = strcspn(name, s_space);
            if (length != 0u)
            {
                unsigned int at = 0u;
                while ((at < s_kernels) &&
                       !((strncmp(s_kernel[at].name, name, length) == 0) && (s_kernel[at].name[length] == '\0')))
                {
                    at += 1u;
                }
                if (at == s_kernels)
                {
                    s_kernel = (StickKernel *)stick_held(realloc(s_kernel, (s_kernels + 1u) * sizeof(StickKernel)));
                    memset(&s_kernel[at], 0, sizeof(StickKernel));
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

// the operations of the listing nvdisasm wrote at `path`, in order, NOP left out, kept after `operations`
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
        size_t length = strlen(text);
        while ((length != 0u) && (strchr(s_space, text[length - 1u]) != NULL))
        {
            length -= 1u;
        }
        text[length] = '\0';
        if ((length == 0u) || (strncmp(text, "//", 2u) == 0) || (text[0] == '.') || (text[length - 1u] == ':'))
        {
            continue;
        }
        const char *const word = text + stick_guard(text);
        size_t letters = strcspn(word, s_space);
        while ((letters != 0u) && (word[letters - 1u] == ';'))
        {
            letters -= 1u;
        }
        const unsigned int operation = stick_named(word, letters);
        if (operation != nop)
        {
            stick_number_add(operations, operation);
        }
    }
}

// the tab-apart cells of `line`, at most `most` of them, the last holding the rest of the line: their count, each cell
// a copy at `cell`
static unsigned int stick_cells(const char *line, char **cell, unsigned int most)
{
    unsigned int count = 0u;
    const char *at = line;
    while (count < most)
    {
        const size_t length = ((count + 1u) == most) ? strlen(at) : strcspn(at, "\t");
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
static void stick_engine_read(const char *table, const char *directory)
{
    stick_read(table);
    KtextFile lines = s_file;
    s_file = (KtextFile){NULL, 0u, 0u};
    for (unsigned int line = 0u; line < lines.lines; line += 1u)
    {
        char *cell[5] = {NULL, NULL, NULL, NULL, NULL};
        const unsigned int cells = stick_cells(lines.line[line], cell, 5u);
        if ((cells == 5u) && (strcmp(cell[0], "number") != 0))
        {
            char name[256];
            snprintf(name, sizeof(name), "%s%s", s_prefix, cell[0]);
            unsigned int at = 0u;
            while ((at < s_engines) && (strcmp(s_engine[at].name, name) != 0))
            {
                at += 1u;
            }
            if (at == s_engines)
            {
                s_engine = (StickEngine *)stick_held(realloc(s_engine, (s_engines + 1u) * sizeof(StickEngine)));
                memset(&s_engine[at], 0, sizeof(StickEngine));
                s_engine[at].name = stick_copy(name, strlen(name));
                s_engines += 1u;
            }
            StickEngine *const entry = &s_engine[at];
            free(entry->answer);
            free(entry->note);
            entry->answer = stick_copy(cell[1], strlen(cell[1]));
            entry->steps = (unsigned int)strtoul(cell[2], NULL, 10);
            entry->note = stick_copy(cell[4], strlen(cell[4]));
            entry->operations.count = 0u;
            entry->agree = 0u;
            entry->text = 0u;
            if (strcmp(cell[1], "answered") == 0)
            {
                char path[1024];
                snprintf(path, sizeof(path), "%s/%s.dis", directory, cell[0]);
                stick_listing_operations(path, &entry->operations);
                StickNumbers text = {NULL, 0u, 0u};
                snprintf(path, sizeof(path), "%s/%s.sass", directory, cell[0]);
                stick_lane_operations(path, &text);
                for (unsigned int one = 0u; (one < text.count) && (one < entry->operations.count); one += 1u)
                {
                    entry->agree += (text.number[one] == entry->operations.number[one]) ? 1u : 0u;
                }
                entry->text = text.count;
                free(text.number);
            }
        }
        for (unsigned int one = 0u; one < cells; one += 1u)
        {
            free(cell[one]);
        }
    }
    ktext_free(&lines);
}

// the operations the SASS ruleset's forms write, each instruction of each form's text, its guard cut, marked in
// `held`, from sass.krs at `path`, then sm_86.krs and the blocks of the ruleset's language of sm_86.kdm beside it,
// then those of sass.ksc beside it
static void stick_ruleset_operations(const char *path, unsigned char **held, unsigned int *room)
{
    // the folder sass.krs stands in, to its last separator, and its stem, to the dot of its extension
    const char *slash = NULL;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        slash = ((*at == '/') || (*at == '\\')) ? at : slash;
    }
    const size_t folder = (slash != NULL) ? (size_t)(slash - path) : 0u;
    const char *const dot = strrchr(path, '.');
    const size_t stem = ((dot != NULL) && ((slash == NULL) || (dot > slash))) ? (size_t)(dot - path) : strlen(path);
    char files[4][1024];
    snprintf(files[0], sizeof(files[0]), "%s", path);
    snprintf(files[1], sizeof(files[1]), "%.*s%ssm_86.krs", (int)folder, path, (slash != NULL) ? "/" : "");
    snprintf(files[2], sizeof(files[2]), "%.*s%ssm_86.kdm", (int)folder, path, (slash != NULL) ? "/" : "");
    snprintf(files[3], sizeof(files[3]), "%.*s.ksc", (int)stem, path);
    for (unsigned int file = 0u; file < 4u; file += 1u)
    {
        stick_read(files[file]);
        // a .krs is the ruleset's language whole; a .kdm and a .ksc hold it in its blocks (klang.h)
        unsigned int from = 0u;
        unsigned int first = 0u;
        unsigned int past = s_file.lines;
        const int whole = (file < 2u);
        while (whole || ktext_block_lines(&s_file, "krs", from, &first, &past))
        {
            for (unsigned int line = first; line < past; line += 1u)
            {
                const char *const text = s_file.line[line];
                const char *const equals = strstr(text, " = ");
                if ((strncmp(text, "form ", 5u) != 0) || (equals == NULL))
                {
                    continue;
                }
                // each instruction of the form's text, apart by a written newline, a written tab a space
                const char *instruction = equals + 3;
                for (;;)
                {
                    const char *const newline = strstr(instruction, "\\n");
                    const size_t length = (newline != NULL) ? (size_t)(newline - instruction) : strlen(instruction);
                    char *const cut = stick_copy(instruction, length);
                    for (char *tab = strstr(cut, "\\t"); tab != NULL; tab = strstr(cut, "\\t"))
                    {
                        tab[0] = ' ';
                        memmove(&tab[1], &tab[2], strlen(&tab[2]) + 1u);
                    }
                    // the instruction, the space at either end cut, then its guard
                    char *const start = cut + strspn(cut, s_space);
                    size_t size = strlen(start);
                    while ((size != 0u) && (strchr(s_space, start[size - 1u]) != NULL))
                    {
                        size -= 1u;
                    }
                    start[size] = '\0';
                    const char *const word = start + stick_form_guard(start);
                    const size_t words = strlen(word);
                    if ((words != 0u) && (strncmp(word, "//", 2u) != 0) && (word[0] != '.') &&
                        (word[words - 1u] != ':'))
                    {
                        size_t letters = strcspn(word, s_space);
                        while ((letters != 0u) && (word[letters - 1u] == ';'))
                        {
                            letters -= 1u;
                        }
                        const unsigned int operation = stick_named(word, letters);
                        if (operation >= *room)
                        {
                            const unsigned int grown = (operation + 1u) * 2u;
                            *held = (unsigned char *)stick_held(realloc(*held, grown));
                            memset(&(*held)[*room], 0, grown - *room);
                            *room = grown;
                        }
                        (*held)[operation] = 1u;
                    }
                    free(cut);
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

// the kernel named `name`, or NULL where nvcc's listing holds none
static const StickEngine *stick_engine_of(const char *name)
{
    for (unsigned int at = 0u; at < s_engines; at += 1u)
    {
        if (strcmp(s_engine[at].name, name) == 0)
        {
            return &s_engine[at];
        }
    }
    return NULL;
}

// <0, 0 or >0 as the kernel at `left` is named before, with or after the one at `right`
static int stick_kernel_order(const void *left, const void *right)
{
    return strcmp(((const StickKernel *)left)->name, ((const StickKernel *)right)->name);
}

// <0, 0 or >0 as the name of the operation at `left` stands before, with or after the one at `right`
static int stick_name_order(const void *left, const void *right)
{
    return strcmp(s_name[*(const unsigned int *)left], s_name[*(const unsigned int *)right]);
}

// `text` with every `|` written `\|`, added to `record`
static void stick_escaped(QryText *record, const char *text)
{
    for (const char *at = text; *at != '\0'; at += 1)
    {
        qry_text_added(record, (*at == '|') ? "\\|" : at, (*at == '|') ? 2u : 1u);
    }
}

// the count of each operation of `operations` into `count`, one for each name of the table
static void stick_counted(const StickNumbers *operations, unsigned int *count)
{
    memset(count, 0, s_names * sizeof(unsigned int));
    for (unsigned int at = 0u; at < operations->count; at += 1u)
    {
        count[operations->number[at]] += 1u;
    }
}

// each operation `count` holds more of than `less`, `<name> <more>`, apart by ", ", in the order of their names,
// added to `record`; `less` NULL where none is taken away
static void stick_counts_written(QryText *record, const unsigned int *order, const unsigned int *count,
                                 const unsigned int *less)
{
    int first = 1;
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const unsigned int operation = order[at];
        const unsigned int taken = (less != NULL) ? less[operation] : 0u;
        if (count[operation] > taken)
        {
            qry_text_printed(record, "%s%s %u", first ? "" : ", ", s_name[operation], count[operation] - taken);
            first = 0;
        }
    }
}

// a manifest line: a kernel's number, its category and its function
typedef struct
{
    char *name;
    char *category;
    char *function;
} StickManifest;

int main(int argc, char **argv)
{
    if ((argc != 5) && (argc != 7))
    {
        fputs("measuring_stick_read <nvcc listing> <manifest> <sass.krs> <record> [<engine.tsv> <engine directory>]\n",
              stderr);
        return 2;
    }
    if (argc == 7)
    {
        stick_engine_read(argv[5], argv[6]);
    }
    stick_nvcc_read(argv[1]);
    stick_read(argv[2]);
    StickManifest *const manifest = (StickManifest *)stick_held(calloc(s_file.lines + 1u, sizeof(StickManifest)));
    unsigned int manifests = 0u;
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        char *cell[3] = {NULL, NULL, NULL};
        const unsigned int cells = stick_cells(s_file.line[line], cell, 3u);
        if ((cells == 3u) && (strcmp(cell[0], "number") != 0))
        {
            char name[256];
            snprintf(name, sizeof(name), "%s%s", s_prefix, cell[0]);
            unsigned int at = 0u;
            while ((at < manifests) && (strcmp(manifest[at].name, name) != 0))
            {
                at += 1u;
            }
            if (at == manifests)
            {
                manifest[at].name = stick_copy(name, strlen(name));
                manifests += 1u;
            }
            free(manifest[at].category);
            free(manifest[at].function);
            manifest[at].category = cell[1];
            manifest[at].function = cell[2];
            cell[1] = NULL;
            cell[2] = NULL;
        }
        for (unsigned int one = 0u; one < cells; one += 1u)
        {
            free(cell[one]);
        }
    }
    unsigned char *held = NULL;
    unsigned int held_room = 0u;
    stick_ruleset_operations(argv[3], &held, &held_room);

    // the stick: every kernel of nvcc's listing the manifest names, in the order of their names
    StickKernel *const stick = (StickKernel *)stick_held(calloc(s_kernels + 1u, sizeof(StickKernel)));
    unsigned int sticks = 0u;
    for (unsigned int at = 0u; at < s_kernels; at += 1u)
    {
        for (unsigned int one = 0u; one < manifests; one += 1u)
        {
            if (strcmp(manifest[one].name, s_kernel[at].name) == 0)
            {
                stick[sticks] = s_kernel[at];
                stick[sticks].category = manifest[one].category;
                stick[sticks].function = manifest[one].function;
                sticks += 1u;
                break;
            }
        }
    }
    qsort(stick, sticks, sizeof(StickKernel), stick_kernel_order);
    // every name the table holds, in the order of the names, and each operation held where it is past the room
    unsigned int *const order = (unsigned int *)stick_held(calloc(s_names + 1u, sizeof(unsigned int)));
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        order[at] = at;
    }
    qsort(order, s_names, sizeof(unsigned int), stick_name_order);
    if (held_room < s_names)
    {
        held = (unsigned char *)stick_held(realloc(held, s_names + 1u));
        memset(&held[held_room], 0, (s_names + 1u) - held_room);
        held_room = s_names + 1u;
    }

    // by category, in the order each first stands; and each operation no form writes, the kernels it is in
    const char **const category = (const char **)stick_held(calloc(sticks + 1u, sizeof(char *)));
    unsigned int *const category_kernels = (unsigned int *)stick_held(calloc(sticks + 1u, sizeof(unsigned int)));
    unsigned int *const category_instructions = (unsigned int *)stick_held(calloc(sticks + 1u, sizeof(unsigned int)));
    unsigned int categories = 0u;
    unsigned int *const missing = (unsigned int *)stick_held(calloc(s_names + 1u, sizeof(unsigned int)));
    unsigned int *const missing_first = (unsigned int *)stick_held(calloc(s_names + 1u, sizeof(unsigned int)));
    unsigned char *const written = (unsigned char *)stick_held(calloc(s_names + 1u, 1u));
    unsigned int *const count = (unsigned int *)stick_held(calloc(s_names + 1u, sizeof(unsigned int)));
    unsigned int *const other = (unsigned int *)stick_held(calloc(s_names + 1u, sizeof(unsigned int)));
    unsigned int instructions = 0u;
    for (unsigned int at = 0u; at < sticks; at += 1u)
    {
        unsigned int one = 0u;
        while ((one < categories) && (strcmp(category[one], stick[at].category) != 0))
        {
            one += 1u;
        }
        categories += (one == categories) ? 1u : 0u;
        category[one] = stick[at].category;
        category_kernels[one] += 1u;
        category_instructions[one] += stick[at].operations.count;
        instructions += stick[at].operations.count;
        stick_counted(&stick[at].operations, count);
        for (unsigned int operation = 0u; operation < s_names; operation += 1u)
        {
            if (count[operation] == 0u)
            {
                continue;
            }
            written[operation] = 1u;
            if (!held[operation])
            {
                missing_first[operation] = (missing[operation] == 0u) ? at : missing_first[operation];
                missing[operation] += 1u;
            }
        }
    }
    unsigned int writings = 0u;
    unsigned int held_writings = 0u;
    for (unsigned int operation = 0u; operation < s_names; operation += 1u)
    {
        writings += written[operation];
        held_writings += (written[operation] && held[operation]) ? 1u : 0u;
    }

    const size_t prefix = sizeof(s_prefix) - 1u;
    QryText record = {NULL, 0ull, 0ull, 0};
    qry_text_printed(&record, "# The measuring stick: nvcc's listing, the answer key\n\n");
    qry_text_printed(&record, "Written by `measuring_stick.sh` whole on every run. Each kernel of the measuring stick "
                              "(measuring_stick.py), every function of the CUDA language in a kernel of its own, is "
                              "compiled by nvcc for sm_86 and read here. What the engine builds for each function is "
                              "held against it, a kernel at parity where the engine's code uses the same operations as "
                              "nvcc's, each as many times.\n\n");
    qry_text_printed(&record,
                     "- kernels: %u\n- instructions: %u\n- operations nvcc writes over the stick: %u, of which "
                     "sass.krs writes %u\n\n",
                     sticks, instructions, writings, held_writings);
    if (argc == 7)
    {
        unsigned int answered = 0u;
        unsigned int parity = 0u;
        unsigned int read_back = 0u;
        unsigned int agree = 0u;
        unsigned int text = 0u;
        for (unsigned int at = 0u; at < sticks; at += 1u)
        {
            const StickEngine *const entry = stick_engine_of(stick[at].name);
            if ((entry == NULL) || (strcmp(entry->answer, "answered") != 0))
            {
                continue;
            }
            answered += 1u;
            read_back += entry->operations.count;
            agree += entry->agree;
            text += entry->text;
            stick_counted(&entry->operations, count);
            stick_counted(&stick[at].operations, other);
            parity += (memcmp(count, other, s_names * sizeof(unsigned int)) == 0) ? 1u : 0u;
        }
        qry_text_printed(&record,
                         "- kernels the engine answers: %u, of which at parity with nvcc: %u; kernels that put a "
                         "question: %u\n- the engine's instructions: %u in its lanes' text, %u read back by nvdisasm, "
                         "%u of them the operation the text wrote\n\n",
                         answered, parity, sticks - answered, text, read_back, agree);
        qry_text_printed(&record,
                         "## The engine against nvcc\n\nEach kernel the engine answers: its record steps, "
                         "nvcc's instructions and the engine's as nvdisasm reads them back, and the operations "
                         "only one of the two writes, each by how many more times it writes it.\n\n"
                         "| kernel | function | steps | nvcc | engine | only nvcc | only the engine |\n"
                         "|---|---|---|---|---|---|---|\n");
        for (unsigned int at = 0u; at < sticks; at += 1u)
        {
            const StickEngine *const entry = stick_engine_of(stick[at].name);
            if ((entry == NULL) || (strcmp(entry->answer, "answered") != 0))
            {
                continue;
            }
            stick_counted(&entry->operations, count);
            stick_counted(&stick[at].operations, other);
            qry_text_printed(&record, "| %s | `", stick[at].name + prefix);
            stick_escaped(&record, stick[at].function);
            qry_text_printed(&record, "` | %u | %u | %u | ", entry->steps, stick[at].operations.count,
                             entry->operations.count);
            stick_counts_written(&record, order, other, count);
            qry_text_printed(&record, " | ");
            stick_counts_written(&record, order, count, other);
            qry_text_printed(&record, " |\n");
        }
        // the questions the engine puts: each note, the kernels that put it and the first of them, the most first
        char **const note = (char **)stick_held(calloc(sticks + 1u, sizeof(char *)));
        unsigned int *const note_kernels = (unsigned int *)stick_held(calloc(sticks + 1u, sizeof(unsigned int)));
        unsigned int *const note_first = (unsigned int *)stick_held(calloc(sticks + 1u, sizeof(unsigned int)));
        unsigned int notes = 0u;
        for (unsigned int at = 0u; at < sticks; at += 1u)
        {
            const StickEngine *const entry = stick_engine_of(stick[at].name);
            if ((entry == NULL) || (strcmp(entry->answer, "answered") == 0))
            {
                continue;
            }
            unsigned int one = 0u;
            while ((one < notes) && (strcmp(note[one], entry->note) != 0))
            {
                one += 1u;
            }
            note_first[one] = (one == notes) ? at : note_first[one];
            notes += (one == notes) ? 1u : 0u;
            note[one] = entry->note;
            note_kernels[one] += 1u;
        }
        qry_text_printed(&record, "\n## The questions the engine puts\n\nEach kernel the engine does not answer, by "
                                  "the question it puts.\n\n| question | kernels | first kernel |\n|---|---|---|\n");
        for (unsigned int written_notes = 0u; written_notes < notes; written_notes += 1u)
        {
            unsigned int best = notes;
            for (unsigned int one = 0u; one < notes; one += 1u)
            {
                if (note[one] == NULL)
                {
                    continue;
                }
                best = ((best == notes) || (note_kernels[one] > note_kernels[best]) ||
                        ((note_kernels[one] == note_kernels[best]) && (strcmp(note[one], note[best]) < 0)))
                           ? one
                           : best;
            }
            qry_text_printed(&record, "| ");
            stick_escaped(&record, note[best]);
            qry_text_printed(&record, " | %u | %s |\n", note_kernels[best], stick[note_first[best]].name + prefix);
            note[best] = NULL;
        }
        qry_text_printed(&record, "\n");
        free(note_first);
        free(note_kernels);
        free(note);
    }
    qry_text_printed(&record, "## By category\n\n| category | kernels | nvcc instructions |\n|---|---|---|\n");
    for (unsigned int at = 0u; at < categories; at += 1u)
    {
        qry_text_printed(&record, "| %s | %u | %u |\n", category[at], category_kernels[at], category_instructions[at]);
    }
    qry_text_printed(&record, "\n## Operations nvcc writes that no form of sass.krs writes\n\n"
                              "| operation | kernels it is in | first kernel | its function |\n|---|---|---|---|\n");
    for (;;)
    {
        unsigned int best = s_names;
        for (unsigned int at = 0u; at < s_names; at += 1u)
        {
            const unsigned int operation = order[at];
            best = ((missing[operation] != 0u) && ((best == s_names) || (missing[operation] > missing[best])))
                       ? operation
                       : best;
        }
        if (best == s_names)
        {
            break;
        }
        const StickKernel *const first = &stick[missing_first[best]];
        qry_text_printed(&record, "| `%s` | %u | %s | `", s_name[best], missing[best], first->name + prefix);
        stick_escaped(&record, first->function);
        qry_text_printed(&record, "` |\n");
        missing[best] = 0u;
    }
    qry_text_printed(&record, "\n## Every kernel\n\n| kernel | category | function | nvcc | its operations |\n"
                              "|---|---|---|---|---|\n");
    for (unsigned int at = 0u; at < sticks; at += 1u)
    {
        qry_text_printed(&record, "| %s | %s | `", stick[at].name + prefix, stick[at].category);
        stick_escaped(&record, stick[at].function);
        qry_text_printed(&record, "` | %u | ", stick[at].operations.count);
        stick_counted(&stick[at].operations, count);
        stick_counts_written(&record, order, count, NULL);
        qry_text_printed(&record, " |\n");
    }
    if (qry_text_filed(qry_run(), argv[4], &record) == 0ull)
    {
        fprintf(stderr, "measuring_stick_read: the record %s was not written\n", argv[4]);
        return 1;
    }
    printf("measuring stick: %u kernels, %u instructions in nvcc's listing, %u operations, %u of them written by "
           "sass.krs\n",
           sticks, instructions, writings, held_writings);
    return 0;
}
