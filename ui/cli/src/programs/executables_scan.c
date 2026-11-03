// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_scan.c: what each kind of build script declares of the programs it makes, read from its text: CMake's and
// Meson's executables and projects, the calls of a command in a script's code, and the comments taken out of it
// (executables_scan_more.c reads the rest)
#include "../text/text.h"
#include "executables_internal.h"

char *cli_uncommented_alloc(const CliExecutablesTextCtx *args)
{
    // the code with its comments taken out is no longer than the text
    const unsigned long long capacity = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}) + 1ULL;
    char *const code = g_cli_cells->persistent_buf_alloc(capacity, &g_cli_executables_cells);
    if (code != NULL)
    {
        EMBED_CALL(cli_executables_text_internal.uncommented, CliExecutablesTextCtx, .text = args->text,
                   .quote = args->quote, .out = code, .capacity = capacity);
    }
    return code;
}

unsigned int cli_calls_alloc(const CliExecutablesCodeCommandCtx *args)
{
    CliCall **calls = args->calls_pointer;
    const unsigned int count =
        (args->code != NULL)
            ? EMBED_CALL(cli_executables_code_internal.calls, CliExecutablesCodeCtx, .code = args->code,
                         .command = args->command, .calls = NULL, .capacity_u32 = 0u)
            : 0u;
    *calls = (count != 0u) ? g_cli_cells->persistent_buf_alloc((unsigned long long)count * sizeof(CliCall),
                                                               &g_cli_executables_cells)
                           : NULL;
    return (*calls != NULL) ? EMBED_CALL(cli_executables_code_internal.calls, CliExecutablesCodeCtx, .code = args->code,
                                         .command = args->command, .calls = *calls, .capacity_u32 = count)
                            : 0u;
}

int cli_scan_white(const CliExecutablesCtx *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r'));
}

typedef struct
{
    char letter;
    const char *code;
    const char *command;
    unsigned long long length;
    unsigned long long open;
    const CliCall *call;
    unsigned int index;
    char *out;
} ExecutablesScanCtx;
// whether a letter is one of a word: a letter, a digit, `_`, or a byte of a letter past ASCII
EMBED_INLINE int cli_wordy(const ExecutablesScanCtx *args)
{
    const unsigned char byte = (unsigned char)args->letter;
    return ((byte >= 'a') && (byte <= 'z')) || ((byte >= 'A') && (byte <= 'Z')) || ((byte >= '0') && (byte <= '9')) ||
           (byte == '_') || (byte >= 0x80u);
}

void cli_scan_trim(const CliExecutablesTextLengthCtx *args)
{
    const char **text = args->text_pointer;
    unsigned long long *length = args->length_pointer;
    while ((*length > 0ULL) && EMBED_CALL(cli_executables_internal.scan_white, CliExecutablesCtx, .letter = (*text)[0]))
    {
        *text += 1;
        *length -= 1ULL;
    }
    while ((*length > 0ULL) &&
           EMBED_CALL(cli_executables_internal.scan_white, CliExecutablesCtx, .letter = (*text)[*length - 1ULL]))
    {
        *length -= 1ULL;
    }
}

void cli_scan_copy(const CliExecutablesLengthTextCtx *args)
{
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    mmgr_line_put_n(&line, args->text, args->length);
    mmgr_line_finish(&line);
}

void cli_names_append(const CliExecutablesNameNamesCtx *args)
{
    for (unsigned int at = 0u; at < args->names->count; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = args->names->name[at], .cap = CLI_CELLS_POOL}) == args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = args->names->name[at], .other = args->name, .bytes = args->length}) == 0))
        {
            return;
        }
    }
    if (args->names->count < CLI_EXECUTABLES_NAMES)
    {
        EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = args->name,
                   .length = args->length, .out = args->names->name[args->names->count], .capacity = CLI_NAME_MAX_LEN);
        EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = args->names->value[args->names->count],
                   .text = args->value, .otherwise = "");
        args->names->count += 1u;
    }
}

void cli_uncommented(const CliExecutablesTextCtx *args)
{
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    const char *at = args->text;
    while (at[0] != '\0')
    {
        int quoted = 0;
        const char *stop = at;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_CELLS_POOL}), .set = "\n");
        const char *const end = ((stop > at) && (stop[-1] == '\r')) ? (stop - 1) : stop;
        for (const char *letter = at; letter < end; letter += 1)
        {
            quoted = (letter[0] == args->quote) ? !quoted : quoted;
            if ((letter[0] == '#') && !quoted)
            {
                break;
            }
            mmgr_line_ch(&line, letter[0]);
        }
        mmgr_line_ch(&line, '\n');
        at = (stop[0] != '\0') ? (stop + 1) : stop;
    }
    mmgr_line_finish(&line);
}

// the length of the text inside the parentheses that open at `open`, to the one that closes them, or to the code's end
EMBED_INLINE unsigned long long cli_inside_length(const ExecutablesScanCtx *args)
{
    long long depth = 0LL;
    unsigned long long at = args->open;
    const unsigned long long size = at + cellul.len(&(CatenaFinitaCfg){.src = args->code + at, .cap = CLI_CELLS_POOL});
    for (at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->code, .at = at,
                         .size = size, .set = "()");
         at < size; at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->code,
                                    .at = at + 1ULL, .size = size, .set = "()"))
    {
        depth += (args->code[at] == '(') ? 1LL : -1LL;
        if ((args->code[at] == ')') && (depth == 0LL))
        {
            return at - args->open - 1ULL;
        }
    }
    return (at > args->open) ? (at - args->open - 1ULL) : 0ULL;
}

unsigned int cli_calls(const CliExecutablesCodeCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->command, .cap = CLI_CELLS_POOL});
    unsigned int found = 0u;
    unsigned long long from = 0ULL;
    while (args->code[from] != '\0')
    {
        // the command found from `from`, its case aside
        const char *const hit = cellul.find(&(CatenaFinitaCfg){.src = args->code + from,
                                                               .cap = CLI_CELLS_POOL,
                                                               .other = args->command,
                                                               .other_cap = (size_t)length + 1u,
                                                               .ci = 1});
        const unsigned long long at =
            (hit != NULL) ? (unsigned long long)(hit - args->code)
                          : (from + cellul.len(&(CatenaFinitaCfg){.src = args->code + from, .cap = CLI_CELLS_POOL}));
        if (args->code[at] == '\0')
        {
            break;
        }
        from = at + length;
        const int starts_word =
            (at == 0ULL) || !EMBED_CALL(cli_wordy, ExecutablesScanCtx, .letter = args->code[at - 1ULL]);
        const unsigned long long gap =
            EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->code + from, .at = 0ULL,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = args->code + from, .cap = CLI_CELLS_POOL}),
                       .set = CLI_TEXT_WHITE);
        if (!starts_word || (args->code[from + gap] != '('))
        {
            continue;
        }
        const unsigned long long size =
            EMBED_CALL(cli_inside_length, ExecutablesScanCtx, .code = args->code, .open = from + gap);
        if (args->calls == NULL)
        {
            found += 1u;
        }
        else if (found < args->capacity_u32)
        {
            args->calls[found] = (CliCall){args->code + from + gap + 1ULL, size};
            found += 1u;
        }
        from += gap + size;
    }
    return found;
}

// the `index`th word of a CMake call, its quotes taken off, into `out`, sized for CLI_NAME_MAX_LEN: 1, or 0 where it
// has no such word
EMBED_INLINE int cli_cmake_word(const ExecutablesScanCtx *args)
{
    char *out = args->out;
    const char *at = args->call->text;
    const char *const end = args->call->text + args->call->length;
    unsigned int word_text = 0u;
    while (at < end)
    {
        while ((at < end) && EMBED_CALL(cli_executables_internal.scan_white, CliExecutablesCtx, .letter = at[0]))
        {
            at += 1;
        }
        const char *stop = at;
        while ((stop < end) && !EMBED_CALL(cli_executables_internal.scan_white, CliExecutablesCtx, .letter = stop[0]))
        {
            stop += 1;
        }
        if (stop == at)
        {
            return 0;
        }
        if (word_text == args->index)
        {
            const char *from = at;
            from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                               .size = (unsigned long long)(stop - from), .set = "\"");
            const char *to = stop;
            to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                                   .at = (unsigned long long)(to - from), .set = "\"");
            EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = from,
                       .length = (unsigned long long)(to - from), .out = out, .capacity = CLI_NAME_MAX_LEN);
            return 1;
        }
        at = stop;
        word_text += 1u;
    }
    return 0;
}

void cli_cmake_targets(const CliExecutablesNamesTextCtx *args)
{
    args->names->count = 0u;
    char *const code = EMBED_CALL(cli_executables_text_internal.uncommented_alloc, CliExecutablesTextCtx,
                                  .text = args->text, .quote = '"');
    CliCall *calls = NULL;
    const unsigned int count =
        EMBED_CALL(cli_executables_code_command_internal.calls_alloc, CliExecutablesCodeCommandCtx, .code = code,
                   .command = "add_executable", .calls_pointer = &calls);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char first[CLI_NAME_MAX_LEN];
        char second[CLI_NAME_MAX_LEN];
        if (EMBED_CALL(cli_cmake_word, ExecutablesScanCtx, .call = &calls[at], .index = 1u, .out = second) &&
            ((cellul.eq(&(CatenaFinitaCfg){.src = second, .other = "IMPORTED", .cap = CLI_CELLS_POOL})) ||
             (cellul.eq(&(CatenaFinitaCfg){.src = second, .other = "ALIAS", .cap = CLI_CELLS_POOL}))))
        {
            continue;
        }
        if (!EMBED_CALL(cli_cmake_word, ExecutablesScanCtx, .call = &calls[at], .index = 0u, .out = first))
        {
            continue;
        }
        char name[CLI_NAME_MAX_LEN];
        VerbaLinea line = {name, sizeof(name), 0ULL};
        EMBED_CALL(cli_text_out_name.append_fill, CliTextOutNameCfg, .out = &line, .text = first,
                   .name = "${PROJECT_NAME}", .value = args->project);
        const unsigned long long length = mmgr_line_finish(&line);
        if ((length != 0ULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = name, .cap = CLI_CELLS_POOL, .other = "${", .other_cap = CLI_CELLS_POOL}) == NULL))
        {
            EMBED_CALL(cli_executables_name_names_internal.names_append, CliExecutablesNameNamesCtx,
                       .names = args->names, .name = name, .length = length, .value = NULL);
        }
    }
    g_cli_cells->persistent_buf_release(calls, &g_cli_executables_cells);
    g_cli_cells->persistent_buf_release(code, &g_cli_executables_cells);
}

int cli_cmake_project(const CliExecutablesTextOutCtx *args)
{
    char *const code = EMBED_CALL(cli_executables_text_internal.uncommented_alloc, CliExecutablesTextCtx,
                                  .text = args->text, .quote = '"');
    CliCall *calls = NULL;
    const unsigned int count =
        EMBED_CALL(cli_executables_code_command_internal.calls_alloc, CliExecutablesCodeCommandCtx, .code = code,
                   .command = "project", .calls_pointer = &calls);
    int found = 0;
    unsigned int at = 0u;
    while (!found && (at < count))
    {
        found = EMBED_CALL(cli_cmake_word, ExecutablesScanCtx, .call = &calls[at], .index = 0u, .out = args->out);
        at += 1u;
    }
    g_cli_cells->persistent_buf_release(calls, &g_cli_executables_cells);
    g_cli_cells->persistent_buf_release(code, &g_cli_executables_cells);
    return found;
}

// a Meson call's first argument, where it is a string written out, into `out`, sized for CLI_NAME_MAX_LEN: 1, or 0
EMBED_INLINE int cli_meson_first(const ExecutablesScanCtx *args)
{
    char *out = args->out;
    const char *first = args->call->text;
    unsigned long long length = 0ULL;
    length = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = first, .at = length,
                        .size = args->call->length, .set = ",");
    EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &first,
               .length_pointer = &length);
    if ((length < 3ULL) || (first[0] != '\'') || (first[length - 1ULL] != '\''))
    {
        return 0;
    }
    EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = first + 1,
               .length = length - 2ULL, .out = out, .capacity = CLI_NAME_MAX_LEN);
    return 1;
}

void cli_meson_targets(const CliExecutablesNamesCtx *args)
{
    args->names->count = 0u;
    char *const code = EMBED_CALL(cli_executables_text_internal.uncommented_alloc, CliExecutablesTextCtx,
                                  .text = args->text, .quote = '\'');
    CliCall *calls = NULL;
    const unsigned int count =
        EMBED_CALL(cli_executables_code_command_internal.calls_alloc, CliExecutablesCodeCommandCtx, .code = code,
                   .command = "executable", .calls_pointer = &calls);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char name[CLI_NAME_MAX_LEN];
        if (EMBED_CALL(cli_meson_first, ExecutablesScanCtx, .call = &calls[at], .out = name))
        {
            EMBED_CALL(cli_executables_name_names_internal.names_append, CliExecutablesNameNamesCtx,
                       .names = args->names, .name = name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .value = NULL);
        }
    }
    g_cli_cells->persistent_buf_release(calls, &g_cli_executables_cells);
    g_cli_cells->persistent_buf_release(code, &g_cli_executables_cells);
}

int cli_meson_project(const CliExecutablesTextOutCtx *args)
{
    char *const code = EMBED_CALL(cli_executables_text_internal.uncommented_alloc, CliExecutablesTextCtx,
                                  .text = args->text, .quote = '\'');
    CliCall *calls = NULL;
    const unsigned int count =
        EMBED_CALL(cli_executables_code_command_internal.calls_alloc, CliExecutablesCodeCommandCtx, .code = code,
                   .command = "project", .calls_pointer = &calls);
    int found = 0;
    unsigned int at = 0u;
    while (!found && (at < count))
    {
        found = EMBED_CALL(cli_meson_first, ExecutablesScanCtx, .call = &calls[at], .out = args->out);
        at += 1u;
    }
    g_cli_cells->persistent_buf_release(calls, &g_cli_executables_cells);
    g_cli_cells->persistent_buf_release(code, &g_cli_executables_cells);
    return found;
}
