// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate.c: a file validated with a tool plugin: the prover found, the file checked as a copy, and each barrier that
// stops the check lifted a step and the copy checked again, as far as each barrier's ceiling
#include "../command_line/cells.h"
#include "validate_internal.h"

#include "../text/text.h"

CliValidateRoom *g_cli_validate;
static CliLock s_validate_lock = CLI_LOCK_INIT;

int cli_validate_held(void)
{
    cli_lock_take(&s_validate_lock);
    g_cli_validate = cli_cell_taken(sizeof(*g_cli_validate), "a file validated");
    if (g_cli_validate == NULL)
    {
        cli_lock_give(&s_validate_lock);
        return 0;
    }
    cli_json_begun(&g_cli_validate->json, NULL, 0u, NULL, 0ull);
    cli_json_begun(&g_cli_validate->manifest, NULL, 0u, NULL, 0ull);
    g_cli_validate->output = NULL;
    g_cli_validate->output_size = 0ull;
    return 1;
}

void cli_validate_released(void)
{
    cli_json_given(&g_cli_validate->json);
    cli_json_given(&g_cli_validate->manifest);
    cli_cell_given(g_cli_validate->output);
    cli_cell_given(g_cli_validate);
    g_cli_validate = NULL;
    cli_lock_give(&s_validate_lock);
}

unsigned int cli_validate_key_of(unsigned int at)
{
    const CliTool *const tool = &g_cli_validate->tool;
    unsigned int key = 0u;
    while ((key < at) &&
           (!cellul.eq(&(CatenaFinitaCfg){
               .src = tool->barriers[key].name, .other = tool->barriers[at].name, .cap = CLI_CELLS_POOL})))
    {
        key += 1u;
    }
    return key;
}

unsigned int cli_validate_grown_keys(unsigned int *keys)
{
    const CliTool *const tool = &g_cli_validate->tool;
    unsigned int count = 0u;
    for (unsigned int key = 0u; key < tool->barrier_count; key += 1u)
    {
        if (!g_cli_validate->has_grown[key])
        {
            continue;
        }
        unsigned int place = count;
        while ((place > 0u) && (cli_text_order(tool->barriers[keys[place - 1u]].name, tool->barriers[key].name) > 0))
        {
            keys[place] = keys[place - 1u];
            place -= 1u;
        }
        keys[place] = key;
        count += 1u;
    }
    return count;
}

// the folder above `path`, nearest first, that holds one of the files the tool's run names as marking a project,
// into `project`, which holds CLI_PATH_LONGEST: 1, or 0 where none does
static int validate_project_of(const char *path, char *project)
{
    const CliJson *const json = &g_cli_validate->json;
    const unsigned int marks = g_cli_validate->tool.project;
    char here[CLI_PATH_LONGEST];
    char above[CLI_PATH_LONGEST];
    int has = cli_path_parent(path, here);
    while (has)
    {
        for (unsigned int index = 0u; cli_json_item(json, marks, index) != CLI_JSON_NONE; index += 1u)
        {
            char marked[CLI_PATH_LONGEST];
            cli_full(here, cli_json_string(json, cli_json_item(json, marks, index), NULL), marked);
            if (query_is_file(marked))
            {
                memor.cpy(&(MemoriaCfg){.dst = project, .src = here, .bytes = CLI_PATH_LONGEST});
                return 1;
            }
        }
        has = cli_path_parent(here, above);
        memor.cpy(&(MemoriaCfg){.dst = here, .src = above, .bytes = CLI_PATH_LONGEST});
    }
    return 0;
}

// a check said as `{"settings", "seconds", "errors", "barriers"}`, a comma before it where it is not the first
static void validate_check_said(const CliWords *words, unsigned int from, unsigned int settings, double seconds,
                                const int *hit, unsigned int hits)
{
    CliLine out = {g_cli_validate->checks + g_cli_validate->checks_size,
                   CLI_VALIDATE_CHECKS_TEXT - g_cli_validate->checks_size, 0ull};
    const CliProverMessages *const messages = &g_cli_validate->last;
    cli_line_text(&out, (g_cli_validate->checks_size > 0ull) ? ",{\"settings\":[" : "{\"settings\":[");
    for (unsigned int at = 0u; at < settings; at += 1u)
    {
        cli_line_text(&out, (at > 0u) ? "," : "");
        cli_json_quoted(&out, words->word[from + at],
                        cellul.len(&(CatenaFinitaCfg){.src = words->word[from + at], .cap = CLI_CELLS_POOL}));
    }
    unsigned long long errors = 0ull;
    for (unsigned int at = 0u; at < messages->count; at += 1u)
    {
        errors += (messages->message[at].severity == 1u) ? 1ull : 0ull;
    }
    cli_line_text(&out, "],\"seconds\":");
    cli_json_double(&out, seconds);
    cli_line_text(&out, ",\"errors\":");
    cli_line_decimal(&out, errors, 1u);
    cli_line_text(&out, ",\"barriers\":[");
    const CliTool *const tool = &g_cli_validate->tool;
    for (unsigned int at = 0u; at < hits; at += 1u)
    {
        // a name the one before it said is said once
        if ((at > 0u) && (cellul.eq(&(CatenaFinitaCfg){.src = tool->barriers[hit[at]].name,
                                                       .other = tool->barriers[hit[at - 1u]].name,
                                                       .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        cli_line_text(&out, (at > 0u) ? "," : "");
        cli_json_quoted(&out, tool->barriers[hit[at]].name,
                        cellul.len(&(CatenaFinitaCfg){.src = tool->barriers[hit[at]].name, .cap = CLI_CELLS_POOL}));
    }
    cli_line_text(&out, "]}");
    if (cli_line_fits(&out))
    {
        g_cli_validate->checks_size += out.at;
    }
}

// the limits that stop the check grown a step, each as far as its ceiling, its stops at the ceiling kept: 1 where
// any moved
static int validate_grown(const int *hit, unsigned int hits)
{
    const CliTool *const tool = &g_cli_validate->tool;
    int moved = 0;
    for (unsigned int at = 0u; at < hits; at += 1u)
    {
        const CliBarrier *const barrier = &tool->barriers[hit[at]];
        const unsigned int key = cli_validate_key_of((unsigned int)hit[at]);
        const unsigned long long now = g_cli_validate->has_grown[key] ? g_cli_validate->grown[key] : barrier->start;
        if (now >= barrier->ceiling)
        {
            g_cli_validate->stuck[key] = 1;
            g_cli_validate->stuck_at[key] = barrier->ceiling;
            continue;
        }
        const unsigned long long step = (barrier->grow > 2ull) ? barrier->grow : 2ull;
        // the step saturates where it would wrap, as Rust's saturating_mul does
        const unsigned long long times = (now > ~0ull / step) ? ~0ull : now * step;
        const unsigned long long next = (times < barrier->ceiling) ? times : barrier->ceiling;
        if (!g_cli_validate->has_grown[key] || (g_cli_validate->grown[key] != next))
        {
            g_cli_validate->grown[key] = next;
            g_cli_validate->has_grown[key] = 1;
            moved = 1;
        }
    }
    return moved;
}

// the options the grown limits set, by their names, each limit's barrier lifting by `-D<name>=...`, a later of one
// name standing for an earlier, into `names` and `values`: their count
static unsigned int validate_options(char (*names)[CLI_NAME_LONGEST], unsigned long long *values)
{
    const CliTool *const tool = &g_cli_validate->tool;
    unsigned int keys[CLI_VALIDATE_BARRIERS];
    const unsigned int grown = cli_validate_grown_keys(keys);
    unsigned int count = 0u;
    for (unsigned int key = 0u; key < grown; key += 1u)
    {
        const char *const lift = tool->barriers[keys[key]].lift;
        if (!cellul.starts(&(CatenaFinitaCfg){.src = lift, .other = "-D", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        char name[CLI_NAME_LONGEST];
        const char *const equal = cellul.chr(&(CatenaFinitaCfg){.src = lift + 2, .cap = CLI_CELLS_POOL, .byte = '='});
        const size_t length = (equal != NULL) ? (size_t)(equal - lift - 2)
                                              : cellul.len(&(CatenaFinitaCfg){.src = lift + 2, .cap = CLI_CELLS_POOL});
        if (length >= sizeof(name))
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = name, .src = lift + 2, .bytes = length});
        name[length] = '\0';
        unsigned int place = 0u;
        while ((place < count) && (cli_text_order(names[place], name) < 0))
        {
            place += 1u;
        }
        if ((place == count) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = names[place], .other = name, .cap = CLI_CELLS_POOL})))
        {
            memor.move_up(&(MemoriaCfg){
                .dst = names[place + 1u], .src = names[place], .bytes = (count - place) * sizeof(names[0])});
            memor.move_up(&(MemoriaCfg){
                .dst = &values[place + 1u], .src = &values[place], .bytes = (count - place) * sizeof(values[0])});
            memor.cpy(&(MemoriaCfg){.dst = names[place], .src = name, .bytes = length + 1u});
            count += 1u;
        }
        values[place] = g_cli_validate->grown[keys[key]];
    }
    return count;
}

// the checks run on a copy of the file at `copy`, each barrier that stops one grown a step and the copy checked again,
// until none stops it, none moves, one runs out of time, or CLI_VALIDATE_CHECKS have run: 1, or 0 with why said
static int validate_checks_run(const char *text, unsigned long long size, const char *program, const char *copy,
                               const char *project, CliLine *said)
{
    static CliWords s_words;
    static int s_hit[CLI_VALIDATE_MESSAGES];
    static char s_names[CLI_VALIDATE_BARRIERS][CLI_NAME_LONGEST];
    unsigned long long values[CLI_VALIDATE_BARRIERS];
    for (unsigned int check = 0u; check < CLI_VALIDATE_CHECKS; check += 1u)
    {
        const unsigned int options = validate_options(s_names, values);
        const char *names[CLI_VALIDATE_BARRIERS];
        for (unsigned int at = 0u; at < options; at += 1u)
        {
            names[at] = s_names[at];
        }
        CliLine own = {g_cli_validate->own, CLI_VALIDATE_OWN, 0ull};
        const unsigned long long copied = cli_validate_copy_of(text, size, names, values, options, &own);
        g_cli_validate->own_size = cli_line_fits(&own) ? own.at : 0ull;
        if ((copied > CLI_VALIDATE_COPY) || !query_file_written(copy, g_cli_validate->copy, copied))
        {
            cli_line_text(said, copy);
            cli_line_text(said, ": it could not be written");
            return 0;
        }
        unsigned int from = 0u;
        unsigned int settings = 0u;
        char dir[CLI_PATH_LONGEST];
        double seconds = 0.0;
        if (!cli_validate_command_for(program, copy, project, &s_words, &from, &settings, dir, said) ||
            !cli_validate_checked(&s_words, dir, &seconds, said))
        {
            return 0;
        }
        cli_validate_messages_of(g_cli_validate->output, g_cli_validate->output_size, &g_cli_validate->last);
        if (!g_cli_validate->ran)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = &g_cli_validate->first, .src = &g_cli_validate->last, .bytes = sizeof(g_cli_validate->first)});
            g_cli_validate->ran = 1;
        }
        unsigned int hits = 0u;
        for (unsigned int at = 0u; at < g_cli_validate->last.count; at += 1u)
        {
            const CliProverMessage *const message = &g_cli_validate->last.message[at];
            const int barrier =
                (message->severity == 1u) ? cli_validate_barrier_of(&g_cli_validate->last, message) : -1;
            if (barrier >= 0)
            {
                s_hit[hits] = barrier;
                hits += 1u;
            }
        }
        validate_check_said(&s_words, from, settings, seconds, s_hit, hits);
        if (g_cli_validate->late || (hits == 0u) || !validate_grown(s_hit, hits))
        {
            break;
        }
    }
    return 1;
}

// the validation with the file's text read, under the room's lock: 1, or 0 with why said
static int validate_text(const char *path, const char *text, unsigned long long size, CliLine *report, CliLine *said)
{
    const CliTool *const tool = &g_cli_validate->tool;
    CliJson *const json = &g_cli_validate->manifest;
    cli_json_given(json);
    const unsigned int manifest = cli_toolchains_manifest(json, 0u, 0ull);
    unsigned int chain = CLI_JSON_NONE;
    for (unsigned int index = 0u; (chain == CLI_JSON_NONE) && (cli_json_item(json, manifest, index) != CLI_JSON_NONE);
         index += 1u)
    {
        const unsigned int one = cli_json_item(json, manifest, index);
        const char *const id = cli_json_string(json, cli_json_member(json, one, "id"), NULL);
        chain = ((id != NULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = id, .other = tool->toolchain, .cap = CLI_CELLS_POOL})))
                    ? one
                    : CLI_JSON_NONE;
    }
    if (chain == CLI_JSON_NONE)
    {
        cli_line_text(said, "the toolchain manifest has no ");
        cli_line_text(said, tool->toolchain);
        return 0;
    }
    unsigned int count = 0u;
    char (*const folders)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&count);
    cli_toolchains_find(json, chain, (const char (*)[CLI_PATH_LONGEST])folders, count, &g_cli_validate->found);
    cli_cell_given(folders);
    if (!g_cli_validate->found.has_program)
    {
        const char *const name = cli_json_string(json, cli_json_member(json, chain, "name"), NULL);
        cli_line_text(said, (name != NULL) ? name : tool->toolchain);
        cli_line_text(said, " is not found: File, Toolchains opens its install page or takes its folder");
        return 0;
    }
    char project[CLI_PATH_LONGEST];
    const int has_project = validate_project_of(path, project);
    if (!cli_validate_code_of(text, size))
    {
        cli_line_text(said, path);
        cli_line_text(said, " is larger than orior holds");
        return 0;
    }
    cli_validate_words_of();
    g_cli_validate->theorem_count = 0u;
    if (tool->audited)
    {
        cli_validate_theorems_in();
    }
    unsigned long long lines = 0ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        lines += (text[at] == '\n') ? 1ull : 0ull;
    }
    g_cli_validate->lines = lines + (((size > 0ull) && (text[size - 1ull] != '\n')) ? 1ull : 0ull);
    char folder[CLI_PATH_LONGEST];
    char copy_name[CLI_PATH_LONGEST];
    char copy[CLI_PATH_LONGEST];
    const char *const named =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
#if defined(_WIN32)
    const char *const named_back =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '\\');
    const char *const base = ((named_back != NULL) && ((named == NULL) || (named_back > named))) ? named_back + 1
                             : (named != NULL)                                                   ? named + 1
                                                                                                 : path;
#else
    const char *const base = (named != NULL) ? named + 1 : path;
#endif
    const char *const dot =
        cli_text_last(base, cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}), '.');
    const size_t stem = ((dot != NULL) && (dot > base))
                            ? (size_t)(dot - base)
                            : cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL});
    CliLine copy_line = {copy_name, sizeof(copy_name), 0ull};
    cli_line_character(&copy_line, '.');
    cli_line_counted(&copy_line, base, stem);
    cli_line_text(&copy_line, ".validate.");
    cli_line_decimal(&copy_line, cli_process_own(), 1u);
    cli_line_text(&copy_line, ".lean");
    cli_line_closed(&copy_line);
    if (cli_path_parent(path, folder))
    {
        cli_full(folder, copy_name, copy);
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = copy, .src = copy_name, .bytes = sizeof(copy)});
    }
    const int ran =
        validate_checks_run(text, size, g_cli_validate->found.program, copy, has_project ? project : NULL, said);
    (void)query_removed(copy);
    if (!ran)
    {
        return 0;
    }
    cli_validate_reported(path, has_project ? project : NULL, report);
    return 1;
}

int cli_validated(const char *id, const char *path, CliLine *report, CliLine *said)
{
    if (!cli_validate_held())
    {
        cli_line_text(said, "the file could not be validated: the cells cannot meet it");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = g_cli_validate->has_grown, .val = 0, .bytes = sizeof(g_cli_validate->has_grown)});
    memor.set(&(MemoriaCfg){.dst = g_cli_validate->stuck, .val = 0, .bytes = sizeof(g_cli_validate->stuck)});
    memor.set(&(MemoriaCfg){.dst = g_cli_validate->lifted, .val = 0, .bytes = sizeof(g_cli_validate->lifted)});
    g_cli_validate->checks_size = 0ull;
    g_cli_validate->own_size = 0ull;
    g_cli_validate->finding_count = 0u;
    g_cli_validate->finding_used = 0ull;
    g_cli_validate->first.count = 0u;
    g_cli_validate->last.count = 0u;
    g_cli_validate->ran = 0;
    g_cli_validate->late = 0;
    int validated = 0;
    if (!cli_validate_tool_read(id))
    {
        cli_line_text(said, "no tool plugin is ");
        cli_line_text(said, id);
    }
    else
    {
        unsigned long long size = 0ull;
        const unsigned char *const bytes = qry_member_file_mapped(path, &size);
        if ((bytes == NULL) || !cli_utf8_whole((const char *)bytes, size))
        {
            cli_line_text(said, path);
            cli_line_text(said, ": it could not be read");
        }
        else
        {
            validated = validate_text(path, (const char *)bytes, size, report, said);
        }
        if (bytes != NULL)
        {
            qry_member_file_unmapped(bytes, size);
        }
    }
    cli_validate_released();
    return validated;
}
