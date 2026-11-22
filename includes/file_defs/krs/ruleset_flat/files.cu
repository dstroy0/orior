// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files.cu: the files read: a file's bytes from the run or the disk, a file appended, the paths of a part, a ruleset's
// text, and what the relations and the maps reported
#include "../../../parsers/tree_folder.h"
#include "ruleset_flat_internal.h"

// the folder of the files of a part: src/cu/transpiler/lstar/protocol/table in the tree of the build of this file. The
// machine file of the part, its .kdm and its .ksc are there
static int ruleset_part_folder(char *folder, size_t bytes)
{
    return tree_folder_from(__FILE__, 4u, "src/cu/transpiler/lstar/protocol/table", folder, bytes);
}

int ruleset_bytes_read(const char *path, PlatformText *text)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    if (!qry_member_file_blob(path, &bytes, &size))
    {
        return 0;
    }
    const unsigned long long held = platform_text_size(text);
    const unsigned long long room = (held < RULESET_FILE_MAX) ? (RULESET_FILE_MAX - held) : 0ULL;
    // a ruleset that the reader accepts is below RULESET_FILE_MAX characters, which a size_t counts. The code cuts one
    // past it there, and the caller refuses it
    (void)mmgr_line_put_n(&text->line, (const char *)bytes, (size_t)((size < room) ? size : room));
    return 1;
}

// A text begun for a ruleset's files, which holds RULESET_FILE_MAX characters and one more, for the caller to refuse
void ruleset_text_begun(PlatformText *text)
{
    platform_text_begun(text, (unsigned long long)RULESET_FILE_MAX + 2ULL, platform_place_g_program_owner, 0);
}

// The lines of the .kdm of a part, or of a .ksc that a ruleset reads, onto `lines`. That is the text of each of its
// blocks of the language of the ruleset, `__krs__(` to `)__krs__` (klang.h), each ended by a newline.
// The arrangements, the pipes, the questions and the comments belong to the file, and are outside each such block
static void ruleset_lines_read(const char *kdm, size_t length, PlatformText *lines)
{
    KlangBlock block;
    size_t from = 0u;
    char last = '\n';
    while (klang_block_find(kdm, length, from, "krs", &block))
    {
        const size_t block_length = block.text_end - block.text;
        (void)mmgr_line_put_n(&lines->line, &kdm[block.text], block_length);
        last = (block_length != 0u) ? kdm[block.text_end - 1u] : last;
        if (last != '\n')
        {
            mmgr_line_ch(&lines->line, '\n');
            last = '\n';
        }
        from = block.closed;
    }
}

// The part that the ruleset at `path` names on its line `part <part>`, into `part`, `bytes` of it. Empty where it
// names none or does not open
static void ruleset_part_named(const char *path, char *part, size_t bytes)
{
    part[0] = '\0';
    PlatformText text;
    ruleset_text_begun(&text);
    if (ruleset_bytes_read(path, &text) == 0)
    {
        platform_text_ended(&text);
        return;
    }
    const char *const bytes_read = platform_text_bytes(&text);
    const size_t size = (size_t)platform_text_size(&text);
    size_t at = 0u;
    while (at < size)
    {
        const char *const newline = mmgr_text_char(&bytes_read[at], '\n', size - at);
        // a line ends at or past its start
        const size_t end = (newline != NULL) ? (size_t)(newline - bytes_read) : size;
        if (((end - at) >= 5u) && (mmgr_bytes_compare(&bytes_read[at], "part ", 5u) == 0))
        {
            size_t stop = at + 5u;
            while ((stop < end) && (bytes_read[stop] != ' ') && (bytes_read[stop] != '\t') &&
                   (bytes_read[stop] != '\r'))
            {
                stop += 1u;
            }
            VerbaLinea named = mmgr_line_over(part, bytes);
            (void)mmgr_line_put_n(&named, &bytes_read[at + 5u], stop - (at + 5u));
            (void)mmgr_line_finish(&named);
            break;
        }
        at = end + 1u;
    }
    platform_text_ended(&text);
}

unsigned int ruleset_paths(const RulesetSchema *schema, const char *path,
                           char paths[RULESET_PATHS_MAX][PLATFORM_PATH_BYTES])
{
    (void)schema;
    unsigned int count = 0u;
    (void)mmgr_text_copy(paths[count], path, PLATFORM_PATH_BYTES);
    count += 1u;
    char named[PLATFORM_PATH_BYTES];
    ruleset_part_named(path, named, sizeof(named));
    char folder[PLATFORM_PATH_BYTES];
    if ((named[0] != '\0') && (ruleset_part_folder(folder, sizeof(folder)) != 0))
    {
        const char *const suffixes[2] = {".krs", ".kdm"};
        for (unsigned int at = 0u; at < 2u; at += 1u)
        {
            VerbaLinea part = mmgr_line_over(paths[count], PLATFORM_PATH_BYTES);
            mmgr_line_put(&part, folder);
            mmgr_line_ch(&part, '/');
            mmgr_line_put(&part, named);
            mmgr_line_put(&part, suffixes[at]);
            (void)mmgr_line_finish(&part);
            count += 1u;
        }
    }
    // the folds the .ksc beside the file contains as forms, where it has one, as the run stores it
    const char *const dot = mmgr_text_char_last(path, '.', PLATFORM_TEXT_TO_TERMINATOR);
    const size_t stem = (dot != NULL) ? (size_t)(dot - path) : mmgr_text_length(path, PLATFORM_TEXT_TO_TERMINATOR);
    VerbaLinea ksc = mmgr_line_over(paths[count], PLATFORM_PATH_BYTES);
    (void)mmgr_line_put_n(&ksc, path, stem);
    mmgr_line_put(&ksc, ".ksc");
    (void)mmgr_line_finish(&ksc);
    PlatformText ksc_text;
    ruleset_text_begun(&ksc_text);
    if (ruleset_bytes_read(paths[count], &ksc_text))
    {
        count += 1u;
    }
    platform_text_ended(&ksc_text);
    return count;
}

int ruleset_text(const RulesetSchema *schema, const char *path, PlatformText *text, VerbaLinea *error)
{
    char paths[RULESET_PATHS_MAX][PLATFORM_PATH_BYTES];
    const unsigned int count = ruleset_paths(schema, path, paths);
    if (ruleset_bytes_read(paths[0], text) == 0)
    {
        mmgr_line_put(error, "it could not be opened");
        return 0;
    }
    for (unsigned int at = 1u; at < count; at += 1u)
    {
        PlatformText read;
        ruleset_text_begun(&read);
        if (ruleset_bytes_read(paths[at], &read) == 0)
        {
            platform_text_ended(&read);
            mmgr_line_put(error, "the part's ");
            mmgr_line_put(error, paths[at]);
            mmgr_line_put(error, " could not be opened");
            return 0;
        }
        const unsigned long long size = platform_text_size(text);
        if ((size != 0ULL) && (platform_text_bytes(text)[size - 1ULL] != '\n'))
        {
            mmgr_line_ch(&text->line, '\n');
        }
        // the .krs of a part opens with krs 1, as each ruleset does, and the own file of the ruleset gave that line. Of
        // a .kdm and a .ksc, the code reads only the ruleset lines
        const char *const read_bytes = platform_text_bytes(&read);
        const size_t read_size = (size_t)platform_text_size(&read);
        const size_t path_length = mmgr_text_length(paths[at], PLATFORM_PATH_BYTES);
        const int krs = (path_length >= 4u) && (mmgr_bytes_compare(&paths[at][path_length - 4u], ".krs", 4u) == 0);
        if (krs == 0)
        {
            ruleset_lines_read(read_bytes, read_size, text);
        }
        else
        {
            const char *const opening = mmgr_text_char(read_bytes, '\n', read_size);
            if (opening != NULL)
            {
                // the newline lies inside the read, at or past its start
                const size_t after = (size_t)(opening - read_bytes) + 1u;
                (void)mmgr_line_put_n(&text->line, &read_bytes[after], read_size - after);
            }
        }
        platform_text_ended(&read);
    }
    if ((platform_text_size(text) >= RULESET_FILE_MAX) || (mmgr_line_ok(&text->line) == 0))
    {
        mmgr_line_put(error, "it is more than the reader supports");
        return 0;
    }
    return 1;
}

const char *ruleset_relation_text(unsigned int type)
{
    static const char *const s_text[RULESET_CORE_RELATION_TYPES] = {
        "is given again after",
        "has the text of",
        "is under another name",
        "is, but for its modifiers and one word of its name,",
        "opens literal operands of",
        "writes a name no file gives",
        "gives another count of arguments than",
        "reaches itself through its line",
        "never writes its parameter",
        "writes a place its head does not take, at character",
        "has nothing after its equals",
        "uses the numeric name of the register of",
        "is not named by the schema",
        "is named by the schema and given by no file",
        "takes another count of parameters than the schema gives",
        "is the arrangement of",
        "counts other than the questions written, which are",
        "answers with a class its file does not name",
    };
    return (type < RULESET_CORE_RELATION_TYPES) ? s_text[type] : "";
}

const char *ruleset_map_text(unsigned int type)
{
    static const char *const s_text[RULESET_CORE_MAP_TYPES] = {
        "is one text in the first set and two in the second with",
        "is two texts in the first set and one in the second with",
        "is given as another type in the second set:",
        "is given by no file of the second set",
    };
    return (type < RULESET_CORE_MAP_TYPES) ? s_text[type] : "";
}
