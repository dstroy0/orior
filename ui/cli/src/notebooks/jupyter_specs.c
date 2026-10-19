// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_specs.c: every kernel the machine has, each name once, in Jupyter's order: the specs in the folders Jupyter
// keeps its data in, then a kernel for each Python orior finds that has no spec of its own
#include "../toolchains/toolchains.h"
#include "jupyter_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// the most kernels listed and entries of a kernels folder, the longest name of an entry, and the most nodes and
// bytes of a spec read
#define CLI_KERNEL_LISTED 256u
#define CLI_KERNEL_ENTRIES 1024u
#define CLI_KERNEL_ENTRY 256u
#define CLI_KERNEL_SPEC_NODES 4096u
#define CLI_KERNEL_SPEC_TEXT (1u << 18u)

// What a listing of the specs reads into: the folders and Pythons, the names and first words of the specs listed, a
// kernels folder's entries, a spec read and the spec made
typedef struct
{
    char folders[CLI_KERNEL_FOLDERS][CLI_PATH_LONGEST];
    char pythons[CLI_KERNEL_PYTHONS][CLI_PATH_LONGEST];
    char names[CLI_KERNEL_LISTED][CLI_KERNEL_ENTRY];
    char programs[CLI_KERNEL_LISTED][CLI_PATH_LONGEST];
    unsigned int listed;
    char entries[CLI_KERNEL_ENTRIES][CLI_KERNEL_ENTRY];
    unsigned int entry_count;
    CliJsonNode nodes[CLI_KERNEL_SPEC_NODES];
    CfgJsonToken tokens[CLI_KERNEL_SPEC_NODES];
    char text[CLI_KERNEL_SPEC_TEXT];
    CliJson json;
    CliKernelSpec spec;
} CliKernelListing;

static CliKernelListing s_listing;
static CliLock s_listing_lock = CLI_LOCK_INIT;

// what is done with each spec listed: 1 to go on, 0 to stop
typedef int (*CliKernelEach)(void *context, const CliKernelSpec *spec);

const char *cli_kernel_text(const CliKernelSpec *spec, unsigned int at)
{
    return (at == CLI_KERNEL_NONE) ? "" : spec->text + at;
}

// `length` bytes put in the spec's texts: their place, CLI_KERNEL_NONE where they do not fit
static unsigned int jupyter_put(CliKernelSpec *spec, const char *text, unsigned long long length)
{
    if ((spec->used + length + 1ull > CLI_KERNEL_TEXT) ||
        (memor.chr(&(MemoriaCfg){.src = text, .val = '\0', .bytes = length}) != NULL))
    {
        spec->full = 1;
        return CLI_KERNEL_NONE;
    }
    const unsigned int at = spec->used;
    memor.cpy(&(MemoriaCfg){.dst = spec->text + at, .src = text, .bytes = length});
    spec->text[at + length] = '\0';
    spec->used += (unsigned int)length + 1u;
    return at;
}

// an entry of a kernels folder kept
static void jupyter_entry(void *context, const char *name, int is_folder)
{
    (void)context;
    (void)is_folder;
    if ((s_listing.entry_count < CLI_KERNEL_ENTRIES) &&
        (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) < CLI_KERNEL_ENTRY))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_listing.entries[s_listing.entry_count],
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
        s_listing.entry_count += 1u;
    }
}

static int jupyter_entry_order(const void *left, const void *right)
{
    return cli_text_order((const char *)left, (const char *)right);
}

// a string member of a spec read into the spec: 1, or 0 where it is no string, or absent with no `fallback`
static int jupyter_member(unsigned int object, const char *name, const char *fallback, unsigned int *at,
                          CliKernelSpec *spec)
{
    const unsigned int member = cli_json_member(&s_listing.json, object, name);
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(&s_listing.json, member, &length);
    if ((member == CLI_JSON_NONE) && (fallback != NULL))
    {
        *at = jupyter_put(spec, fallback, cellul.len(&(CatenaFinitaCfg){.src = fallback, .cap = CLI_CELLS_POOL}));
        return *at != CLI_KERNEL_NONE;
    }
    if (text == NULL)
    {
        return 0;
    }
    *at = jupyter_put(spec, text, length);
    return *at != CLI_KERNEL_NONE;
}

// the spec a kernel.json says, its name `name` and its folder `folder`, read into the listing's spec: 1, or 0 where it
// is not one, as serde reads a Spec
static int jupyter_spec_read(unsigned int root, const char *name, const char *folder)
{
    CliKernelSpec *const spec = &s_listing.spec;
    memor.set(&(MemoriaCfg){.dst = spec, .val = 0, .bytes = sizeof(*spec)});
    spec->name = jupyter_put(spec, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    if (!cli_json_is(&s_listing.json, root, CFG_JSON_OBJECT) ||
        !jupyter_member(root, "display_name", NULL, &spec->display_name, spec) ||
        !jupyter_member(root, "language", NULL, &spec->language, spec) ||
        !jupyter_member(root, "interrupt_mode", "", &spec->interrupt_mode, spec) ||
        !jupyter_member(root, "folder", "", &spec->folder, spec))
    {
        return 0;
    }
    const unsigned int argv = cli_json_member(&s_listing.json, root, "argv");
    if (!cli_json_is(&s_listing.json, argv, CFG_JSON_ARRAY))
    {
        return 0;
    }
    for (unsigned int index = 0u; cli_json_item(&s_listing.json, argv, index) != CLI_JSON_NONE; index += 1u)
    {
        unsigned long long length = 0ull;
        const char *const word = cli_json_string(&s_listing.json, cli_json_item(&s_listing.json, argv, index), &length);
        if ((word == NULL) || (spec->argv_count == CLI_KERNEL_WORDS))
        {
            return 0;
        }
        spec->argv[spec->argv_count] = jupyter_put(spec, word, length);
        spec->argv_count += 1u;
    }
    const unsigned int env = cli_json_member(&s_listing.json, root, "env");
    if ((env != CLI_JSON_NONE) && !cli_json_is(&s_listing.json, env, CFG_JSON_OBJECT))
    {
        return 0;
    }
    for (unsigned int member = (env != CLI_JSON_NONE) ? s_listing.json.node[env].first : CLI_JSON_NONE;
         member != CLI_JSON_NONE; member = s_listing.json.node[member].next)
    {
        unsigned long long length = 0ull;
        const char *const value = cli_json_string(&s_listing.json, member, &length);
        if ((value == NULL) || (spec->env_count == CLI_KERNEL_SETTINGS))
        {
            return 0;
        }
        const char *const key = s_listing.json.text + s_listing.json.node[member].key;
        spec->env_name[spec->env_count] =
            jupyter_put(spec, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
        spec->env_value[spec->env_count] = jupyter_put(spec, value, length);
        spec->env_count += 1u;
    }
    const unsigned int install = cli_json_member(&s_listing.json, root, "install");
    spec->install = CLI_KERNEL_NONE;
    if ((install != CLI_JSON_NONE) && !cli_json_is(&s_listing.json, install, CFG_JSON_NULL) &&
        !jupyter_member(root, "install", NULL, &spec->install, spec))
    {
        return 0;
    }
    spec->folder = jupyter_put(spec, folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
    return !spec->full;
}

// the spec listed: its name and first word kept, and handed on: 1 to go on, 0 to stop
static int jupyter_listed(CliKernelEach each, void *context)
{
    if (s_listing.listed < CLI_KERNEL_LISTED)
    {
        CliLine name = {s_listing.names[s_listing.listed], CLI_KERNEL_ENTRY, 0ull};
        cli_line_text(&name, cli_kernel_text(&s_listing.spec, s_listing.spec.name));
        cli_line_closed(&name);
        CliLine program = {s_listing.programs[s_listing.listed], CLI_PATH_LONGEST, 0ull};
        cli_line_text(&program,
                      (s_listing.spec.argv_count > 0u) ? cli_kernel_text(&s_listing.spec, s_listing.spec.argv[0]) : "");
        cli_line_closed(&program);
        s_listing.listed += 1u;
    }
    return each(context, &s_listing.spec);
}

// the specs of one folder of Jupyter's data, in the order of their folders' names, each handed on: 1 to go on, 0 to
// stop
static int jupyter_folder_specs(const char *data, CliKernelEach each, void *context)
{
    char kernels[CLI_PATH_LONGEST];
    cli_full(data, "kernels", kernels);
    s_listing.entry_count = 0u;
    (void)query_folder_each(kernels, jupyter_entry, NULL);
    qsort(s_listing.entries, s_listing.entry_count, CLI_KERNEL_ENTRY, jupyter_entry_order);
    for (unsigned int entry = 0u; entry < s_listing.entry_count; entry += 1u)
    {
        const char *const name = s_listing.entries[entry];
        int seen = 0;
        for (unsigned int one = 0u; (one < s_listing.listed) && !seen; one += 1u)
        {
            seen = (cellul.eq(&(CatenaFinitaCfg){.src = s_listing.names[one], .other = name, .cap = CLI_CELLS_POOL}));
        }
        char folder[CLI_PATH_LONGEST];
        char file[CLI_PATH_LONGEST];
        cli_full(kernels, name, folder);
        cli_full(folder, "kernel.json", file);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = seen ? NULL : qry_member_file_mapped(file, &size);
        if (bytes == NULL)
        {
            continue;
        }
        cli_json_begun(&s_listing.json, s_listing.nodes, CLI_KERNEL_SPEC_NODES, s_listing.text, CLI_KERNEL_SPEC_TEXT);
        char why[CLI_SAID_LONGEST];
        CliLine unread = {why, sizeof(why), 0ull};
        // a spec is read as a text, which is UTF-8 whole
        const unsigned int root = cli_utf8_whole((const char *)bytes, size)
                                      ? cli_json_read(&s_listing.json, (const char *)bytes, size, s_listing.tokens,
                                                      CLI_KERNEL_SPEC_NODES, &unread)
                                      : CLI_JSON_NONE;
        const int read = (root != CLI_JSON_NONE) && jupyter_spec_read(root, name, folder);
        qry_member_file_unmapped(bytes, size);
        if (read && !jupyter_listed(each, context))
        {
            return 0;
        }
    }
    return 1;
}

// a kernel for each Python found that no spec listed runs, each handed on
static void jupyter_python_specs(unsigned int pythons, int has_environment, CliKernelEach each, void *context)
{
    for (unsigned int index = 0u; index < pythons; index += 1u)
    {
        const char *const python = s_listing.pythons[index];
        CliKernelSpec *const spec = &s_listing.spec;
        memor.set(&(MemoriaCfg){.dst = spec, .val = 0, .bytes = sizeof(*spec)});
        char name[32];
        CliLine name_line = {name, sizeof(name), 0ull};
        if ((index == 0u) && has_environment)
        {
            cli_line_text(&name_line, "python3-tree");
        }
        else
        {
            cli_line_text(&name_line, "python3-");
            cli_line_decimal(&name_line, index + 1u, 1u);
        }
        cli_line_closed(&name_line);
        char shown[CLI_PATH_LONGEST + 16u];
        CliLine shown_line = {shown, sizeof(shown), 0ull};
        cli_line_text(&shown_line, "Python (");
        cli_line_text(&shown_line, python);
        cli_line_text(&shown_line, ")");
        cli_line_closed(&shown_line);
        static const char *const words[4] = {"-m", "ipykernel_launcher", "-f", "{connection_file}"};
        spec->name = jupyter_put(spec, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        spec->display_name =
            jupyter_put(spec, shown, cellul.len(&(CatenaFinitaCfg){.src = shown, .cap = CLI_CELLS_POOL}));
        spec->language = jupyter_put(spec, "python", 6u);
        spec->argv[0] = jupyter_put(spec, python, cellul.len(&(CatenaFinitaCfg){.src = python, .cap = CLI_CELLS_POOL}));
        for (unsigned int word = 0u; word < 4u; word += 1u)
        {
            spec->argv[word + 1u] = jupyter_put(
                spec, words[word], cellul.len(&(CatenaFinitaCfg){.src = words[word], .cap = CLI_CELLS_POOL}));
        }
        spec->argv_count = 5u;
        spec->interrupt_mode = jupyter_put(spec, "signal", 6u);
        spec->folder = jupyter_put(spec, "", 0ull);
        spec->install = CLI_KERNEL_NONE;
        if (!cli_kernel_holds_ipykernel(python))
        {
            char install[CLI_PATH_LONGEST + 48u];
            CliLine line = {install, sizeof(install), 0ull};
            cli_line_character(&line, '"');
            cli_line_text(&line, python);
            cli_line_text(&line, "\" -m pip install ipykernel");
            cli_line_closed(&line);
            spec->install =
                jupyter_put(spec, install, cellul.len(&(CatenaFinitaCfg){.src = install, .cap = CLI_CELLS_POOL}));
        }
        int runs = 0;
        for (unsigned int one = 0u; (one < s_listing.listed) && !runs; one += 1u)
        {
            runs =
                (cellul.eq(&(CatenaFinitaCfg){.src = s_listing.programs[one], .other = python, .cap = CLI_CELLS_POOL}));
        }
        if (!runs && !jupyter_listed(each, context))
        {
            return;
        }
    }
}

// every spec the machine has, each handed to `each` in Jupyter's order, under the listing's lock
static void jupyter_specs_each(CliKernelEach each, void *context)
{
    cli_lock_take(&s_listing_lock);
    s_listing.listed = 0u;
    int has_environment = 0;
    const unsigned int pythons = cli_kernel_pythons(s_listing.pythons, &has_environment);
    const unsigned int folders =
        cli_kernel_data_folders((const char (*)[CLI_PATH_LONGEST])s_listing.pythons, pythons, s_listing.folders);
    int going = 1;
    for (unsigned int folder = 0u; going && (folder < folders); folder += 1u)
    {
        going = jupyter_folder_specs(s_listing.folders[folder], each, context);
    }
    if (going)
    {
        jupyter_python_specs(pythons, has_environment, each, context);
    }
    cli_lock_give(&s_listing_lock);
}

// a spec written as serde writes one, a comma before it where it is not the first
static int jupyter_spec_written(void *context, const CliKernelSpec *spec)
{
    CliLine *const out = (CliLine *)context;
    const char last = (out->at > 0ull) ? out->out[out->at - 1ull] : '[';
    cli_line_text(out, (last == '[') ? "{\"name\":" : ",{\"name\":");
    const char *const texts[3] = {cli_kernel_text(spec, spec->name), cli_kernel_text(spec, spec->display_name),
                                  cli_kernel_text(spec, spec->language)};
    cli_json_quoted(out, texts[0], cellul.len(&(CatenaFinitaCfg){.src = texts[0], .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"display_name\":");
    cli_json_quoted(out, texts[1], cellul.len(&(CatenaFinitaCfg){.src = texts[1], .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"language\":");
    cli_json_quoted(out, texts[2], cellul.len(&(CatenaFinitaCfg){.src = texts[2], .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"argv\":[");
    for (unsigned int word = 0u; word < spec->argv_count; word += 1u)
    {
        const char *const text = cli_kernel_text(spec, spec->argv[word]);
        cli_line_text(out, (word > 0u) ? "," : "");
        cli_json_quoted(out, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
    }
    const char *const mode = cli_kernel_text(spec, spec->interrupt_mode);
    cli_line_text(out, "],\"interrupt_mode\":");
    cli_json_quoted(out, mode, cellul.len(&(CatenaFinitaCfg){.src = mode, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"env\":{");
    unsigned int order[CLI_KERNEL_SETTINGS];
    for (unsigned int at = 0u; at < spec->env_count; at += 1u)
    {
        // the names in their bytes' order, as a BTreeMap keeps them
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(cli_kernel_text(spec, spec->env_name[order[place - 1u]]),
                                               cli_kernel_text(spec, spec->env_name[at])) > 0))
        {
            order[place] = order[place - 1u];
            place -= 1u;
        }
        order[place] = at;
    }
    for (unsigned int at = 0u; at < spec->env_count; at += 1u)
    {
        const char *const name = cli_kernel_text(spec, spec->env_name[order[at]]);
        const char *const value = cli_kernel_text(spec, spec->env_value[order[at]]);
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_json_quoted(out, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        cli_line_character(out, ':');
        cli_json_quoted(out, value, cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}));
    }
    const char *const folder = cli_kernel_text(spec, spec->folder);
    cli_line_text(out, "},\"folder\":");
    cli_json_quoted(out, folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"install\":");
    if (spec->install == CLI_KERNEL_NONE)
    {
        cli_line_text(out, "null");
    }
    else
    {
        const char *const install = cli_kernel_text(spec, spec->install);
        cli_json_quoted(out, install, cellul.len(&(CatenaFinitaCfg){.src = install, .cap = CLI_CELLS_POOL}));
    }
    cli_line_character(out, '}');
    return 1;
}

void cli_jupyter_specs_written(CliLine *out)
{
    cli_line_character(out, '[');
    jupyter_specs_each(jupyter_spec_written, out);
    cli_line_character(out, ']');
}

// The spec sought by name, and where it is put
typedef struct
{
    const char *name;
    CliKernelSpec *spec;
    int found;
} CliKernelSought;

static int jupyter_spec_sought(void *context, const CliKernelSpec *spec)
{
    CliKernelSought *const sought = (CliKernelSought *)context;
    if (!cellul.eq(
            &(CatenaFinitaCfg){.src = cli_kernel_text(spec, spec->name), .other = sought->name, .cap = CLI_CELLS_POOL}))
    {
        return 1;
    }
    memor.cpy(&(MemoriaCfg){.dst = sought->spec, .src = spec, .bytes = sizeof(*spec)});
    sought->found = 1;
    return 0;
}

int cli_jupyter_spec_named(const char *name, CliKernelSpec *spec)
{
    CliKernelSought sought = {name, spec, 0};
    jupyter_specs_each(jupyter_spec_sought, &sought);
    return sought.found;
}
