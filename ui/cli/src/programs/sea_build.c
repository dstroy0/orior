// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sea_build.c: a script built into a single executable of Node's, by `node --build-sea` from Node 25.5 and by Node's
// documented steps before it; node's copy written in pieces from its mapping, so node is never held whole
#include "sea.h"

// the most bytes of a script read, and the most modules outside Node told
#define CLI_SEA_SCRIPT (1u << 22u)

#define CLI_SEA_WANTED 64u

// a line said through `say`
static void cli_say(QuerySink say, const char *text)
{
    say.said(say.context, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// `path` with its file's extension, where it has one, put as `extension` into `out`, which holds CLI_PATH_LONGEST
static void cli_with_extension(const char *path, const char *extension, char *out)
{
    const char *name = path;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    const char *dot = NULL;
    for (const char *at = name + 1; (name[0] != '\0') && (at[0] != '\0'); at += 1)
    {
        dot = (at[0] == '.') ? at : dot;
    }
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    cli_line_counted(&line, path,
                     (dot != NULL) ? (unsigned long long)(dot - path)
                                   : cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    cli_line_character(&line, '.');
    cli_line_text(&line, extension);
    cli_line_closed(&line);
}

// `path` with `/` between its parts, written as a JSON string after `line`
static void cli_slashed(CliLine *line, const char *path)
{
    char slashed[CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = slashed, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = slashed; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    char quoted[CLI_PATH_LONGEST * 2u];
    CliLine json = {quoted, sizeof(quoted), 0ull};
    cli_line_character(&json, '"');
    for (const char *at = slashed; at[0] != '\0'; at += 1)
    {
        if ((at[0] == '"') || (at[0] == '\\'))
        {
            cli_line_character(&json, '\\');
        }
        cli_line_character(&json, at[0]);
    }
    cli_line_character(&json, '"');
    cli_line_closed(&json);
    cli_line_text(line, quoted);
}

// node run with `args`, `count` of them, each line it writes said as it ends: 1, or 0 with why said where it fails
static int cli_node_run(const char *node, const char *const *args, unsigned int count, QuerySink say, CliLine *said)
{
    char shown[CLI_PATH_LONGEST * 2u];
    CliLine line = {shown, sizeof(shown), 0ull};
    cli_line_text(&line, "$ node");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_character(&line, ' ');
        cli_line_text(&line, args[at]);
    }
    cli_line_closed(&line);
    cli_say(say, shown);
    const char *words[8] = {node, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    for (unsigned int at = 0u; (at < count) && (at < 6u); at += 1u)
    {
        words[at + 1u] = args[at];
    }
    CliRan *const ran = cli_ran_taken();
    char why[CLI_PATH_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    if (!cli_ran(words, NULL, NULL, 0u, ran, &unstarted))
    {
        cli_ran_given(ran);
        cli_line_text(said, "node: ");
        cli_line_counted(said, why, unstarted.at);
        return 0;
    }
    for (unsigned int stream = 0u; stream < 2u; stream += 1u)
    {
        const char *text = (stream == 0u) ? ran->out : ran->err;
        while (text[0] != '\0')
        {
            const char *stop = text;
            while ((stop[0] != '\0') && (stop[0] != '\n'))
            {
                stop += 1;
            }
            const unsigned long long length =
                (unsigned long long)(stop - text) - (((stop > text) && (stop[-1] == '\r')) ? 1ull : 0ull);
            say.said(say.context, text, length);
            text = (stop[0] != '\0') ? (stop + 1) : stop;
        }
    }
    const long long code = ran->code;
    cli_ran_given(ran);
    if (code != 0ll)
    {
        cli_line_text(said, "node ");
        cli_line_text(said, (count != 0u) ? args[0] : "");
        cli_line_text(said, " ended with exit code: ");
        if (code < 0ll)
        {
            cli_line_character(said, '-');
        }
        // the code's size, its sign written apart
        cli_line_decimal(said, (unsigned long long)((code < 0ll) ? -code : code), 1u);
        return 0;
    }
    return 1;
}

// the version node says it is, its three numbers into `major` and `minor`: 1, or 0 with why said
static int cli_version(const char *node, unsigned long long *major, unsigned long long *minor, CliLine *said)
{
    const char *const words[3] = {node, "--version", NULL};
    CliRan *const ran = cli_ran_taken();
    char why[CLI_PATH_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    if (!cli_ran(words, NULL, NULL, 0u, ran, &unstarted))
    {
        cli_ran_given(ran);
        cli_line_text(said, node);
        cli_line_text(said, ": ");
        cli_line_counted(said, why, unstarted.at);
        return 0;
    }
    const char *text = ran->out;
    const char *end = ran->out + ran->out_size;
    while ((text < end) && ((text[0] == ' ') || ((text[0] >= '\t') && (text[0] <= '\r'))))
    {
        text += 1;
    }
    while ((end > text) && ((end[-1] == ' ') || ((end[-1] >= '\t') && (end[-1] <= '\r'))))
    {
        end -= 1;
    }
    const char *const trimmed = text;
    while ((text < end) && (text[0] == 'v'))
    {
        text += 1;
    }
    // the parts between the dots that are numbers
    unsigned long long numbers[3] = {0ull, 0ull, 0ull};
    unsigned int read = 0u;
    while ((text < end) && (read < 3u))
    {
        const char *stop = text;
        while ((stop < end) && (stop[0] != '.'))
        {
            stop += 1;
        }
        int number = stop > text;
        unsigned long long value = 0ull;
        for (const char *digit = text; number && (digit < stop); digit += 1)
        {
            number = (digit[0] >= '0') && (digit[0] <= '9');
            value = (value * 10ull) + (unsigned long long)(digit[0] - '0');
        }
        if (number)
        {
            numbers[read] = value;
            read += 1u;
        }
        text = (stop < end) ? (stop + 1) : stop;
    }
    if (read < 3u)
    {
        cli_line_text(said, node);
        cli_line_text(said, " says no version: ");
        cli_line_counted(said, trimmed, (unsigned long long)(end - trimmed));
        cli_ran_given(ran);
        return 0;
    }
    cli_ran_given(ran);
    *major = numbers[0];
    *minor = numbers[1];
    return 1;
}

// `path` could not be read or written, said
static int cli_failed(const char *path, const char *how, CliLine *said)
{
    cli_line_text(said, path);
    cli_line_text(said, how);
    return 0;
}

#if !defined(_WIN32)
// the copy of node with the blob added, as Linux takes it: a note in a segment of its own, the fuse turned
static int cli_linux_copy(const unsigned char *program, unsigned long long size, const unsigned char *data,
                          unsigned long long data_size, const char *output, QuerySink say, CliLine *said)
{
    static CliSeaNote s_note;
    unsigned long long fuse = 0ull;
    if (!cli_sea_note(program, size, data_size, &s_note, said) || !cli_sea_fuse_at(program, size, &fuse, said))
    {
        return 0;
    }
    char told[CLI_NAME_LONGEST];
    CliLine line = {told, sizeof(told), 0ull};
    cli_line_text(&line, "added ");
    cli_line_decimal(&line, data_size, 1u);
    cli_line_text(&line, " bytes as the note NODE_SEA_BLOB");
    cli_line_closed(&line);
    cli_say(say, told);
    // the program's head with its header table's place and count set to the new ones
    unsigned char head[64];
    memor.cpy(&(MemoriaCfg){.dst = head, .src = program, .bytes = 64u});
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        // each byte of the table's place, its low eight bits
        head[0x20u + byte] = (unsigned char)(s_note.region >> (8u * byte));
    }
    const unsigned long long count = s_note.table_size / 56ull;
    // the count of headers, below 65536 as CLI_SEA_HEADERS bounds it, its two bytes
    head[0x38] = (unsigned char)count;
    head[0x39] = (unsigned char)(count >> 8u);
    static const unsigned char s_turned = '1';
    const CliFilePiece pieces[9] = {{head, 64ull},
                                    {program + 64, fuse - 64ull},
                                    {&s_turned, 1ull},
                                    {program + fuse + 1, size - fuse - 1ull},
                                    {NULL, s_note.region - size},
                                    {s_note.table, s_note.table_size},
                                    {NULL, s_note.note_at - s_note.region - s_note.table_size},
                                    {s_note.head, s_note.head_size},
                                    {data, data_size}};
    static const unsigned char s_pad[4] = {0u, 0u, 0u, 0u};
    const CliFilePiece padding = {s_pad, (4ull - (data_size % 4ull)) % 4ull};
    CliFilePiece all[10];
    memor.cpy(&(MemoriaCfg){.dst = all, .src = pieces, .bytes = sizeof(pieces)});
    all[9] = padding;
    if ((fuse < 64ull) || !cli_file_pieces_written(output, all, 10u))
    {
        return cli_failed(output, ": it could not be written", said);
    }
    cli_file_runnable(output);
    return 1;
}
#else
// the copy of node with the blob added, as Windows takes it: node's signature taken off, the blob added as a resource,
// and the fuse turned, the copy written beside the output and moved over it
static int cli_windows_copy(const unsigned char *program, unsigned long long size, const unsigned char *data,
                            unsigned long long data_size, const char *output, QuerySink say, CliLine *said)
{
    unsigned long long entry = 0ull;
    unsigned long long length = size;
    const int cut = cli_sea_signature(program, size, &entry, &length, said);
    if (cut < 0)
    {
        return 0;
    }
    if (cut > 0)
    {
        cli_say(say, "took off node's signature");
    }
    const CliFilePiece unsigned_pieces[3] = {{program, (cut > 0) ? entry : length},
                                             {NULL, (cut > 0) ? 8ull : 0ull},
                                             {program + entry + 8ull, (cut > 0) ? (length - entry - 8ull) : 0ull}};
    if (!cli_file_pieces_written(output, unsigned_pieces, 3u))
    {
        return cli_failed(output, ": it could not be written", said);
    }
    if (!cli_resource_added(output, "NODE_SEA_BLOB", data, data_size, said))
    {
        return 0;
    }
    char told[CLI_NAME_LONGEST];
    CliLine line = {told, sizeof(told), 0ull};
    cli_line_text(&line, "added ");
    cli_line_decimal(&line, data_size, 1u);
    cli_line_text(&line, " bytes as the resource NODE_SEA_BLOB");
    cli_line_closed(&line);
    cli_say(say, told);
    unsigned long long made_size = 0ull;
    const unsigned char *const made = qry_member_file_mapped(output, &made_size);
    if (made == NULL)
    {
        return cli_failed(output, ": it could not be read", said);
    }
    unsigned long long fuse = 0ull;
    char fused[CLI_PATH_LONGEST];
    CliLine fused_line = {fused, sizeof(fused), 0ull};
    cli_line_text(&fused_line, output);
    cli_line_text(&fused_line, ".fused");
    static const unsigned char s_turned = '1';
    int done = (cli_line_closed(&fused_line) != 0ull) && cli_sea_fuse_at(made, made_size, &fuse, said);
    if (done)
    {
        const CliFilePiece pieces[3] = {{made, fuse}, {&s_turned, 1ull}, {made + fuse + 1, made_size - fuse - 1ull}};
        done = cli_file_pieces_written(fused, pieces, 3u);
        if (!done)
        {
            cli_failed(fused, ": it could not be written", said);
        }
    }
    qry_member_file_unmapped(made, made_size);
    if (done && !query_file_renamed(fused, output))
    {
        return cli_failed(output, ": it could not be replaced", said);
    }
    return done;
}
#endif

int cli_sea_build(const char *node, const char *main, const char *output, int module, QuerySink say, char *made,
                  CliLine *said)
{
    static char s_script[CLI_SEA_SCRIPT];
    static char s_wanted[CLI_SEA_WANTED][CLI_NAME_LONGEST];
    static char s_config_text[CLI_PATH_LONGEST * 4u];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    const unsigned long long script_size = qry_member_file_read(main, (unsigned char *)s_script, sizeof(s_script) - 1u);
    if (script_size == QRY_MEMBER_UNREAD)
    {
        cli_lock_give(&s_lock);
        return cli_failed(main, ": it could not be read", said);
    }
    s_script[script_size] = '\0';
    const unsigned int wanted = cli_sea_outside(s_script, s_wanted, CLI_SEA_WANTED);
    if (wanted != 0u)
    {
        cli_line_text(said, main);
        cli_line_text(said, " asks for ");
        for (unsigned int at = 0u; at < wanted; at += 1u)
        {
            cli_line_text(said, (at != 0u) ? ", " : "");
            cli_line_text(said, s_wanted[at]);
        }
        cli_line_text(said, ", which a single executable of Node's does not hold: it loads Node's own modules alone; "
                            "Bun or Deno bundles them in");
        cli_lock_give(&s_lock);
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = made,
                            .src = output,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = output, .cap = CLI_CELLS_POOL}) + 1u});
#if defined(_WIN32)
    const char *name = output;
    for (const char *at = output; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    if ((name[0] != '\0') &&
        (cellul.chr(&(CatenaFinitaCfg){.src = name + 1, .cap = CLI_CELLS_POOL, .byte = '.'}) == NULL))
    {
        cli_with_extension(output, "exe", made);
    }
#endif
    char folder[CLI_PATH_LONGEST];
    if (cli_path_parent(made, folder) && !query_folder_made(folder))
    {
        cli_lock_give(&s_lock);
        return cli_failed(folder, ": it could not be made", said);
    }
    unsigned long long major = 0ull;
    unsigned long long minor = 0ull;
    if (!cli_version(node, &major, &minor, said))
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    const char *const format = module ? "module" : "commonjs";
    char config[CLI_PATH_LONGEST];
    char config_slashed[CLI_PATH_LONGEST];
    cli_with_extension(made, "sea.json", config);
    memor.cpy(&(MemoriaCfg){.dst = config_slashed,
                            .src = config,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = config, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = config_slashed; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    const int builds_itself = (major > 25ull) || ((major == 25ull) && (minor >= 5ull));
    if (!builds_itself && (major < 20ull))
    {
        cli_line_text(said, "Node ");
        cli_line_decimal(said, major, 1u);
        cli_line_character(said, '.');
        cli_line_decimal(said, minor, 1u);
        cli_line_text(said, " makes no single executable: Node 20 is the first that does");
        cli_lock_give(&s_lock);
        return 0;
    }
#if defined(__APPLE__)
    if (!builds_itself)
    {
        cli_line_text(said, "on macOS, Node ");
        cli_line_decimal(said, major, 1u);
        cli_line_character(said, '.');
        cli_line_decimal(said, minor, 1u);
        cli_line_text(said, " needs postject to add the blob; Node 25.5 builds it itself, with --build-sea");
        cli_lock_give(&s_lock);
        return 0;
    }
#endif
    char blob[CLI_PATH_LONGEST];
    cli_with_extension(made, "blob", blob);
    CliLine text = {s_config_text, sizeof(s_config_text), 0ull};
    cli_line_text(&text, "{\n  \"main\": ");
    cli_slashed(&text, main);
    cli_line_text(&text, ",\n  \"output\": ");
    cli_slashed(&text, builds_itself ? made : blob);
    if (builds_itself || module)
    {
        cli_line_text(&text, ",\n  \"mainFormat\": \"");
        cli_line_text(&text, format);
        cli_line_character(&text, '"');
    }
    cli_line_text(&text, ",\n  \"disableExperimentalSEAWarning\": true\n}\n");
    if (!cli_line_fits(&text) || !query_file_written(config, s_config_text, text.at))
    {
        cli_lock_give(&s_lock);
        return cli_failed(config, ": it could not be written", said);
    }
    const char *const args[2] = {builds_itself ? "--build-sea" : "--experimental-sea-config", config_slashed};
    if (!cli_node_run(node, args, 2u, say, said))
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    if (builds_itself)
    {
        cli_lock_give(&s_lock);
        return 1;
    }
    unsigned long long data_size = 0ull;
    const unsigned char *const data = qry_member_file_mapped(blob, &data_size);
    if (data == NULL)
    {
        cli_lock_give(&s_lock);
        return cli_failed(blob, ": it could not be read", said);
    }
    char told[CLI_PATH_LONGEST * 2u];
    CliLine copy = {told, sizeof(told), 0ull};
    cli_line_text(&copy, "copy ");
    cli_line_text(&copy, node);
    cli_line_text(&copy, " to ");
    cli_line_text(&copy, made);
    cli_line_closed(&copy);
    cli_say(say, told);
    unsigned long long size = 0ull;
    const unsigned char *const program = qry_member_file_mapped(node, &size);
    int done = program != NULL;
    if (!done)
    {
        cli_failed(node, ": it could not be read", said);
    }
#if defined(_WIN32)
    done = done && cli_windows_copy(program, size, data, data_size, made, say, said);
#else
    done = done && cli_linux_copy(program, size, data, data_size, made, say, said);
#endif
    if (program != NULL)
    {
        qry_member_file_unmapped(program, size);
    }
    qry_member_file_unmapped(data, data_size);
    if (done)
    {
        cli_say(say, "turned the fuse to 1");
    }
    cli_lock_give(&s_lock);
    return done;
}
