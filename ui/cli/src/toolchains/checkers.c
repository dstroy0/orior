// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// checkers.c: the checkers the manifest knows, found and run over a file or a tree, and their findings written by file
#include "../command_line/cells.h"
#include "../machine/trust.h"
#include "../text/json_tree.h"
#include "checkers_internal.h"
#include "toolchains.h"

// The room a check reads into: the manifest, in a cell of its own, the toolchain found, the checker's words, the PATH
// it runs with, and its findings
typedef struct
{
    CliJson json;
    CliFound found;
    char words[CLI_WORDS_TEXT];
    char path[CLI_PATH_TEXT];
    CliFindings findings;
} CliCheckRoom;

static CliCheckRoom *s_room;
static CliLock s_lock = CLI_LOCK_INIT;

// the lock taken and the room a cell under it: 1, or 0 with the lock given back where no cell is left
static int checkers_held(void)
{
    cli_lock_take(&s_lock);
    s_room = cli_cell_taken(sizeof(*s_room), "a checker's room");
    if (s_room == NULL)
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    cli_json_begun(&s_room->json, NULL, 0u, NULL, 0ull);
    return 1;
}

// the room and its manifest given back, and the lock with it
static void checkers_released(void)
{
    cli_json_given(&s_room->json);
    cli_cell_given(s_room);
    s_room = NULL;
    cli_lock_give(&s_lock);
}

// the manifest read into the room, the one read before given back: its array of toolchains
static unsigned int cli_manifest(void)
{
    cli_json_given(&s_room->json);
    return cli_toolchains_manifest(&s_room->json, 0u, 0ull);
}

// a member's string, `otherwise` where it is none
static const char *cli_string_or(unsigned int node, const char *key, const char *otherwise)
{
    const char *const text = cli_json_string(&s_room->json, cli_json_member(&s_room->json, node, key), NULL);
    return (text != NULL) ? text : otherwise;
}

// the toolchain of the checker `id` in the manifest read: the tool, or CLI_JSON_NONE where it has no checker
static unsigned int cli_checker_of(unsigned int manifest, const char *id)
{
    for (unsigned int at = 0u; cli_json_item(&s_room->json, manifest, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int tool = cli_json_item(&s_room->json, manifest, at);
        if ((cli_json_member(&s_room->json, tool, "checker") != CLI_JSON_NONE) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = cli_string_or(tool, "id", ""), .other = id, .cap = CLI_CELLS_POOL})))
        {
            return tool;
        }
    }
    return CLI_JSON_NONE;
}

// whether a text is `lower`, ASCII's case aside
static int cli_same_lower(const char *text, const char *lower)
{
    for (; (text[0] != '\0') && (lower[0] != '\0'); text += 1, lower += 1)
    {
        const char letter = ((text[0] >= 'A') && (text[0] <= 'Z')) ? (char)(text[0] + 32) : text[0];
        if (letter != lower[0])
        {
            return 0;
        }
    }
    return (text[0] == '\0') && (lower[0] == '\0');
}

int cli_checkers_named(const char *name, char *id)
{
    char wanted[CLI_NAME_LONGEST];
    const char *from = name;
    while ((from[0] == ' ') || ((from[0] >= '\t') && (from[0] <= '\r')))
    {
        from += 1;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = wanted, .src = from, .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_CELLS_POOL});
    while ((length > 0ull) &&
           ((wanted[length - 1ull] == ' ') || ((wanted[length - 1ull] >= '\t') && (wanted[length - 1ull] <= '\r'))))
    {
        length -= 1ull;
    }
    wanted[length] = '\0';
    for (char *at = wanted; at[0] != '\0'; at += 1)
    {
        at[0] = ((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0];
    }
    if (!checkers_held())
    {
        return 0;
    }
    const unsigned int manifest = cli_manifest();
    int found = 0;
    for (unsigned int at = 0u; !found && (cli_json_item(&s_room->json, manifest, at) != CLI_JSON_NONE); at += 1u)
    {
        const unsigned int tool = cli_json_item(&s_room->json, manifest, at);
        const char *const tool_id = cli_string_or(tool, "id", "");
        found = (cli_json_member(&s_room->json, tool, "checker") != CLI_JSON_NONE) &&
                (cli_same_lower(tool_id, wanted) || cli_same_lower(cli_string_or(tool, "name", ""), wanted));
        if (found)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = id,
                              .src = tool_id,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = tool_id, .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
    checkers_released();
    return found;
}

int cli_checkers_reads_tree(const char *id)
{
    if (!checkers_held())
    {
        return 0;
    }
    const unsigned int tool = cli_checker_of(cli_manifest(), id);
    const unsigned int spec = cli_json_member(&s_room->json, tool, "checker");
    const int tree = cli_json_is(&s_room->json, cli_json_member(&s_room->json, spec, "tree"), CFG_JSON_ARRAY);
    checkers_released();
    return tree;
}

int cli_checkers_reads_input(const char *id)
{
    if (!checkers_held())
    {
        return 0;
    }
    const unsigned int tool = cli_checker_of(cli_manifest(), id);
    const unsigned int spec = cli_json_member(&s_room->json, tool, "checker");
    const int input = cli_json_is(&s_room->json, cli_json_member(&s_room->json, spec, "stdin"), CFG_JSON_TRUE);
    checkers_released();
    return input;
}

int cli_checkers_checks(const char *id, const char *language)
{
    if (!checkers_held())
    {
        return 0;
    }
    const unsigned int tool = cli_checker_of(cli_manifest(), id);
    const unsigned int languages =
        cli_json_member(&s_room->json, cli_json_member(&s_room->json, tool, "checker"), "languages");
    int checks = 0;
    for (unsigned int at = 0u; !checks && (cli_json_item(&s_room->json, languages, at) != CLI_JSON_NONE); at += 1u)
    {
        const char *const one = cli_json_string(&s_room->json, cli_json_item(&s_room->json, languages, at), NULL);
        checks = (one != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = one, .other = language, .cap = CLI_CELLS_POOL}));
    }
    checkers_released();
    return checks;
}

void cli_checkers_languages(CliLine *out)
{
    if (!checkers_held())
    {
        cli_line_text(out, "{}");
        return;
    }
    const unsigned int manifest = cli_manifest();
    cli_line_character(out, '{');
    unsigned int written = 0u;
    for (unsigned int at = 0u; cli_json_item(&s_room->json, manifest, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int tool = cli_json_item(&s_room->json, manifest, at);
        const unsigned int spec = cli_json_member(&s_room->json, tool, "checker");
        if (spec == CLI_JSON_NONE)
        {
            continue;
        }
        const char *const id = cli_string_or(tool, "id", "");
        cli_line_text(out, (written != 0u) ? "," : "");
        cli_json_quoted(out, id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ":[");
        const unsigned int languages = cli_json_member(&s_room->json, spec, "languages");
        unsigned int listed = 0u;
        for (unsigned int one = 0u; cli_json_item(&s_room->json, languages, one) != CLI_JSON_NONE; one += 1u)
        {
            unsigned long long length = 0ull;
            const char *const language =
                cli_json_string(&s_room->json, cli_json_item(&s_room->json, languages, one), &length);
            if (language != NULL)
            {
                cli_line_text(out, (listed != 0u) ? "," : "");
                cli_json_quoted(out, language, length);
                listed += 1u;
            }
        }
        cli_line_character(out, ']');
        written += 1u;
    }
    cli_line_character(out, '}');
    checkers_released();
}

// whether `size` bytes hold no more than whitespace, and the last line of them that is not blank written into `said`
static int cli_blank_text(const char *text, unsigned long long size)
{
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        if ((text[at] != ' ') && ((text[at] < '\t') || (text[at] > '\r')))
        {
            return 0;
        }
    }
    return 1;
}

static void cli_last_line(const char *text, unsigned long long size, CliLine *said)
{
    unsigned long long end = size;
    while ((end > 0ull) && ((text[end - 1ull] == ' ') || ((text[end - 1ull] >= '\t') && (text[end - 1ull] <= '\r'))))
    {
        end -= 1ull;
    }
    unsigned long long start = end;
    while ((start > 0ull) && (text[start - 1ull] != '\n'))
    {
        start -= 1ull;
    }
    while ((start < end) && ((text[start] == ' ') || ((text[start] >= '\t') && (text[start] <= '\r'))))
    {
        start += 1ull;
    }
    cli_lossy(said, (const unsigned char *)text + start, end - start);
}

// the checker `tool` run with the words of its spec's `args_key`, `{file}` written as `file`, from the tree `root`,
// `length` bytes of `input` on its input where it is given, and its findings read into the room: 1, or 0 with why said
static int cli_checked(unsigned int tool, const char *args_key, const char *file, const char *root, const char *input,
                       unsigned long long length, CliLine *said)
{
    CliJson *const json = &s_room->json;
    const char *const name = cli_string_or(tool, "name", "");
    const unsigned int spec = cli_json_member(json, tool, "checker");
    const char *const program_name = cli_string_or(spec, "program", "");
    const char *const reads = cli_string_or(spec, "reads", "");
    if (cli_trust_held_back(root))
    {
        cli_line_text(said, name);
        cli_line_text(said, " does not run in a tree whose own code is held back");
        return 0;
    }
    unsigned int folders = 0u;
    char (*const path)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&folders);
    cli_toolchains_find(json, tool, (const char (*)[CLI_PATH_LONGEST])path, folders, &s_room->found);
    cli_cell_given(path);
    char program[CLI_PATH_LONGEST];
    const char *const names[1] = {program_name};
    if ((s_room->found.folder[0] == '\0') || !cli_toolchains_program_in(s_room->found.folder, names, 1u, program))
    {
        cli_line_text(said, name);
        cli_line_text(said, " is not found: File, Toolchains opens its install page or takes its folder");
        return 0;
    }
    const char *words[CLI_WORDS_MOST + 2u] = {program};
    unsigned int count = 1u;
    CliLine filled = {s_room->words, sizeof(s_room->words), 0ull};
    const unsigned int args = cli_json_member(json, spec, args_key);
    for (unsigned int at = 0u; (count <= CLI_WORDS_MOST) && (cli_json_item(json, args, at) != CLI_JSON_NONE); at += 1u)
    {
        const char *const word = cli_json_string(json, cli_json_item(json, args, at), NULL);
        if (word == NULL)
        {
            continue;
        }
        const unsigned long long start = filled.at;
        for (const char *letter = word; letter[0] != '\0';)
        {
            const int named =
                (file != NULL) &&
                (cellul.starts(&(CatenaFinitaCfg){.src = letter, .other = "{file}", .cap = CLI_CELLS_POOL}));
            cli_line_text(&filled, named ? file : "");
            if (!named)
            {
                cli_line_character(&filled, letter[0]);
            }
            letter += named ? 6 : 1;
        }
        cli_line_character(&filled, '\0');
        words[count] = s_room->words + start;
        count += cli_line_fits(&filled) ? 1u : 0u;
    }
    words[count] = NULL;
    cli_toolchains_run_path(s_room->path, sizeof(s_room->path));
    const CliSetting settings[1] = {{"PATH", s_room->path}};
    CliRan *const ran = cli_ran_taken();
    char why[CLI_NAME_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    if (!cli_ran_fed(words, root, settings, 1u, input, length, ran, &unstarted))
    {
        cli_ran_given(ran);
        cli_line_text(said, program);
        cli_line_text(said, ": ");
        cli_line_counted(said, why, unstarted.at);
        return 0;
    }
    if (cli_blank_text(ran->out, ran->out_size) && !cli_blank_text(ran->err, ran->err_size) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = reads, .other = "lines", .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(said, name);
        cli_line_text(said, ": ");
        cli_last_line(ran->err, ran->err_size, said);
        cli_ran_given(ran);
        return 0;
    }
    cli_checkers_read(name, reads, ran->out, ran->out_size, ran->err, ran->err_size, root, file, &s_room->findings);
    cli_ran_given(ran);
    return 1;
}

// the findings written by file, as `[[path, [diagnostic]]]`
static void cli_findings_written(CliLine *out)
{
    const CliFindings *const findings = &s_room->findings;
    cli_line_character(out, '[');
    for (unsigned int file = 0u; file < findings->files; file += 1u)
    {
        cli_line_text(out, (file != 0u) ? ",[" : "[");
        cli_json_quoted(out, findings->path[file],
                        cellul.len(&(CatenaFinitaCfg){.src = findings->path[file], .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",[");
        unsigned int written = 0u;
        for (unsigned int at = 0u; at < findings->findings; at += 1u)
        {
            if (findings->file_of[at] == file)
            {
                cli_line_text(out, (written != 0u) ? "," : "");
                cli_line_counted(out, findings->text + findings->start[at], findings->length[at]);
                written += 1u;
            }
        }
        cli_line_text(out, "]]");
    }
    cli_line_character(out, ']');
}

int cli_checkers_check_file(const char *id, const char *root, const char *path, const char *text,
                            unsigned long long length, CliLine *out, CliLine *said)
{
    if (!checkers_held())
    {
        cli_line_text(said, "a check could not be taken");
        return 0;
    }
    const unsigned int tool = cli_checker_of(cli_manifest(), id);
    if (tool == CLI_JSON_NONE)
    {
        cli_line_text(said, id);
        cli_line_text(said, " is not in the manifest");
        checkers_released();
        return 0;
    }
    const unsigned int spec = cli_json_member(&s_room->json, tool, "checker");
    const int stdin_read = cli_json_is(&s_room->json, cli_json_member(&s_room->json, spec, "stdin"), CFG_JSON_TRUE);
    unsigned long long size = 0ull;
    const unsigned char *const mapped = (stdin_read && (text == NULL)) ? cli_file_held(path, &size) : NULL;
    const char *const input =
        stdin_read ? ((text != NULL) ? text : ((mapped != NULL) ? (const char *)mapped : "")) : NULL;
    const unsigned long long input_length = (text != NULL) ? length : size;
    const int checked = cli_checked(tool, "args", path, root, input, input_length, said);
    if (mapped != NULL)
    {
        cli_file_let_go(mapped, size);
    }
    if (checked)
    {
        cli_checkers_file(&s_room->findings, path, 1);
        cli_findings_written(out);
    }
    checkers_released();
    return checked;
}

int cli_checkers_check_tree(const char *id, const char *root, CliLine *out, CliLine *said)
{
    if (!checkers_held())
    {
        cli_line_text(said, "a check could not be taken");
        return 0;
    }
    const unsigned int tool = cli_checker_of(cli_manifest(), id);
    const unsigned int spec = cli_json_member(&s_room->json, tool, "checker");
    if (!cli_json_is(&s_room->json, cli_json_member(&s_room->json, spec, "tree"), CFG_JSON_ARRAY))
    {
        cli_line_text(out, "[]");
        checkers_released();
        return 1;
    }
    const int checked = cli_checked(tool, "tree", NULL, root, NULL, 0ull, said);
    if (checked)
    {
        cli_findings_written(out);
    }
    checkers_released();
    return checked;
}
