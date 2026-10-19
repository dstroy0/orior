// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_found.c: the Rust tests of a tree: each package's targets as Cargo finds them, and each target's modules
// followed from its first file, `mod name;` by `mod name;`, for the functions marked as tests
#include "../command_line/cells.h"
#include "../editor/files.h"
#include "cargo_tests_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// the most files one package's search follows
#define CLI_CARGO_SEEN 8192u

// What the search of one package keeps: the tree, where the tests go, the manifest, package and target, and the files
// followed
typedef struct
{
    const char *root;
    CliCargoTests *tests;
    const char *manifest;
    const char *package;
    CliCargoTarget target;
    unsigned long long seen[CLI_CARGO_SEEN];
    unsigned int seen_count;
} CliCargoWalk;

// A search's room, a cell taken with the search's lock and given back with it: the texts of the files seen, and the
// walk
typedef struct
{
    char seen_text[1u << 20u];
    CliCargoWalk walk;
} CliCargoFoundRoom;

static CliCargoFoundRoom *s_found;
static unsigned long long s_seen_used;

int cli_cargo_is_rust(const char *id)
{
    const char *const path =
        cellul.find(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL, .other = "::", .other_cap = CLI_CELLS_POOL});
    const size_t length =
        (path != NULL) ? (size_t)(path - id) : cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL});
    return (length >= 3u) && (memor.cmp(&(MemoriaCfg){.src = id + length - 3u, .other = ".rs", .bytes = 3u}) == 0);
}

// a text put in the tests' texts: where it is, "" where it does not fit
static const char *found_put(CliCargoTests *tests, const char *text, unsigned long long length)
{
    if (tests->used + length + 1ull > CLI_CARGO_TEXT)
    {
        tests->full = 1;
        return "";
    }
    char *const at = tests->text + tests->used;
    memor.cpy(&(MemoriaCfg){.dst = at, .src = text, .bytes = length});
    at[length] = '\0';
    tests->used += length + 1ull;
    return at;
}

// whether a file of the package was followed already: 1, else it is kept as followed and 0
static int found_seen(CliCargoWalk *walk, const char *file)
{
    for (unsigned int at = 0u; at < walk->seen_count; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = s_found->seen_text + walk->seen[at], .other = file, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u;
    if ((walk->seen_count < CLI_CARGO_SEEN) && (s_seen_used + length <= sizeof(s_found->seen_text)))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_found->seen_text + s_seen_used, .src = file, .bytes = length});
        walk->seen[walk->seen_count] = s_seen_used;
        walk->seen_count += 1u;
        s_seen_used += length;
    }
    return 0;
}

// a test the search found put with its names
static void found_test(CliCargoWalk *walk, const char *file, const char *path, const CliRustHeld *held)
{
    const char *const inline_names = g_cli_cargo_scan.names + held->inline_names;
    const char *const name = g_cli_cargo_scan.names + held->name;
    CliCargoTests *const tests = walk->tests;
    if (tests->count == CLI_CARGO_FOUND)
    {
        tests->full = 1;
        return;
    }
    char inside[4096];
    char full[4096];
    char id[CLI_PATH_LONGEST + 4096u];
    cli_cargo_joined(inline_names, name, "::", inside, sizeof(inside));
    cli_cargo_joined(path, inside, "::", full, sizeof(full));
    cli_cargo_joined(file, inside, "::", id, sizeof(id));
    CliCargoFound *const found = &tests->found[tests->count];
    found->test.id = found_put(tests, id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
    found->test.file = found_put(tests, file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
    found->test.class_name =
        (inline_names[0] != '\0')
            ? found_put(tests, inline_names, cellul.len(&(CatenaFinitaCfg){.src = inline_names, .cap = CLI_CELLS_POOL}))
            : NULL;
    found->test.name = found_put(tests, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    found->test.line = held->line;
    found->manifest = walk->manifest;
    found->package = walk->package;
    found->target = walk->target;
    found->full = found_put(tests, full, cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}));
    tests->count += 1u;
}

// the modules of `file`, a path in the tree whose own module path is `path`, followed: each test it holds found, and
// each module it names with `mod name;` followed in turn. A file that is a target's first, or a mod.rs, holds its
// modules' files beside it; any other holds them in the folder of its own name
static void found_walk(CliCargoWalk *walk, const char *file, const char *path, int owns_folder)
{
    if (found_seen(walk, file))
    {
        return;
    }
    char full[CLI_PATH_LONGEST];
    cli_full(walk->root, file, full);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(full, &size);
    if (bytes == NULL)
    {
        return;
    }
    const unsigned int first = g_cli_cargo_scan.held_count;
    const unsigned long long names = g_cli_cargo_scan.name_used;
    unsigned int count = 0u;
    if (cli_utf8_whole((const char *)bytes, size))
    {
        cli_cargo_tokens((const char *)bytes, size);
        count = cli_cargo_scan((const char *)bytes);
    }
    qry_member_file_unmapped(bytes, size);
    for (unsigned int at = first; at < first + count; at += 1u)
    {
        if (g_cli_cargo_scan.held[at].test)
        {
            found_test(walk, file, path, &g_cli_cargo_scan.held[at]);
        }
    }
    char base[CLI_PATH_LONGEST];
    const char *const slash =
        cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/');
    const size_t folder_size = (slash != NULL) ? (size_t)(slash - file) : 0u;
    size_t stem_size =
        cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) - ((slash != NULL) ? folder_size + 1u : 0u);
    const char *const stem = (slash != NULL) ? slash + 1 : file;
    while ((stem_size >= 3u) &&
           (memor.cmp(&(MemoriaCfg){.src = stem + stem_size - 3u, .other = ".rs", .bytes = 3u}) == 0))
    {
        stem_size -= 3u;
    }
    CliLine base_line = {base, sizeof(base), 0ull};
    cli_line_counted(&base_line, file, folder_size);
    if (!owns_folder)
    {
        cli_line_text(&base_line, (folder_size > 0u) ? "/" : "");
        cli_line_counted(&base_line, stem, stem_size);
    }
    cli_line_closed(&base_line);
    for (unsigned int at = first; at < first + count; at += 1u)
    {
        const CliRustHeld held = g_cli_cargo_scan.held[at];
        if (held.test)
        {
            continue;
        }
        const char *const inline_names = g_cli_cargo_scan.names + held.inline_names;
        const char *const name = g_cli_cargo_scan.names + held.name;
        char dir[CLI_PATH_LONGEST];
        char tail[CLI_PATH_LONGEST];
        char whole[4096];
        char inner[4096];
        CliLine dir_line = {dir, sizeof(dir), 0ull};
        cli_line_text(&dir_line, base);
        for (const char *one = inline_names; *one != '\0';)
        {
            const char *const next = cellul.find(
                &(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL, .other = "::", .other_cap = CLI_CELLS_POOL});
            const size_t length = (next != NULL) ? (size_t)(next - one)
                                                 : cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL});
            cli_line_text(&dir_line, (dir_line.at > 0ull) ? "/" : "");
            cli_line_counted(&dir_line, one, length);
            one += length + ((next != NULL) ? 2u : 0u);
        }
        cli_line_closed(&dir_line);
        cli_cargo_joined(inline_names, name, "::", inner, sizeof(inner));
        cli_cargo_joined(path, inner, "::", whole, sizeof(whole));
        char flat[CLI_PATH_LONGEST];
        char nested[CLI_PATH_LONGEST];
        CliLine tail_line = {tail, sizeof(tail), 0ull};
        cli_line_text(&tail_line, name);
        cli_line_text(&tail_line, ".rs");
        cli_line_closed(&tail_line);
        cli_cargo_joined(dir, tail, "/", flat, sizeof(flat));
        tail_line.at = 0ull;
        cli_line_text(&tail_line, name);
        cli_line_text(&tail_line, "/mod.rs");
        cli_line_closed(&tail_line);
        cli_cargo_joined(dir, tail, "/", nested, sizeof(nested));
        char flat_full[CLI_PATH_LONGEST];
        char nested_full[CLI_PATH_LONGEST];
        cli_full(walk->root, flat, flat_full);
        cli_full(walk->root, nested, nested_full);
        if (query_is_file(flat_full))
        {
            found_walk(walk, flat, whole, 0);
        }
        else if (query_is_file(nested_full))
        {
            found_walk(walk, nested, whole, 1);
        }
    }
    g_cli_cargo_scan.held_count = first;
    g_cli_cargo_scan.name_used = names;
}

static CliLock s_found_lock = CLI_LOCK_INIT;

void cli_cargo_found(const char *root, CliCargoTests *tests)
{
    static const char *const s_not_searched[4] = {"target", "node_modules", ".git", "build"};
    cli_lock_take(&s_found_lock);
    s_found = cli_cell_taken(sizeof(*s_found), "a search for Rust's tests");
    if (s_found == NULL)
    {
        cli_lock_give(&s_found_lock);
        return;
    }
    tests->count = 0u;
    tests->used = 0ull;
    tests->full = 0;
    g_cli_cargo_scan.held_count = 0u;
    g_cli_cargo_scan.name_used = 0ull;
    const CliFiles *const files = cli_files_held(root);
    for (unsigned int index = 0u; (files != NULL) && (index < files->count); index += 1u)
    {
        const char *const manifest = cli_files_at(files, index);
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = manifest, .cap = CLI_CELLS_POOL});
        if (!((length == 10u) &&
              (cellul.eq(&(CatenaFinitaCfg){.src = manifest, .other = "Cargo.toml", .cap = CLI_CELLS_POOL}))) &&
            !((length > 10u) && (cellul.eq(&(CatenaFinitaCfg){
                                    .src = manifest + length - 11u, .other = "/Cargo.toml", .cap = CLI_CELLS_POOL}))))
        {
            continue;
        }
        int searched = 1;
        for (const char *part = manifest; searched && (*part != '\0');)
        {
            const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL, .byte = '/'});
            const size_t size = (slash != NULL) ? (size_t)(slash - part)
                                                : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL});
            for (unsigned int skipped = 0u; skipped < 4u; skipped += 1u)
            {
                searched =
                    searched &&
                    !((cellul.len(&(CatenaFinitaCfg){.src = s_not_searched[skipped], .cap = CLI_CELLS_POOL}) == size) &&
                      (memor.cmp(&(MemoriaCfg){.src = part, .other = s_not_searched[skipped], .bytes = size}) == 0));
            }
            part += size + ((slash != NULL) ? 1u : 0u);
        }
        char manifest_full[CLI_PATH_LONGEST];
        cli_full(root, manifest, manifest_full);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = searched ? qry_member_file_mapped(manifest_full, &size) : NULL;
        if (bytes == NULL)
        {
            continue;
        }
        char package[CLI_NAME_LONGEST];
        char folder[CLI_PATH_LONGEST];
        CliLine folder_line = {folder, sizeof(folder), 0ull};
        size_t folder_size = length - 10u;
        while ((folder_size > 0u) && (manifest[folder_size - 1u] == '/'))
        {
            folder_size -= 1u;
        }
        cli_line_counted(&folder_line, manifest, folder_size);
        cli_line_closed(&folder_line);
        const int read =
            cli_utf8_whole((const char *)bytes, size) && cli_cargo_package_name((const char *)bytes, size, package);
        char package_folder[CLI_PATH_LONGEST];
        cli_full(root, folder, package_folder);
        unsigned int target_count = 0u;
        const CliCargoFirst *const firsts =
            read ? cli_cargo_targets(package_folder, (const char *)bytes, size, package, &target_count) : NULL;
        qry_member_file_unmapped(bytes, size);
        if (!read)
        {
            continue;
        }
        s_found->walk.root = root;
        s_found->walk.tests = tests;
        s_found->walk.manifest = found_put(tests, manifest, length);
        s_found->walk.package =
            found_put(tests, package, cellul.len(&(CatenaFinitaCfg){.src = package, .cap = CLI_CELLS_POOL}));
        s_found->walk.seen_count = 0u;
        s_seen_used = 0ull;
        for (unsigned int target = 0u; target < target_count; target += 1u)
        {
            const CliCargoFirst *const first = &firsts[target];
            char file[CLI_PATH_LONGEST];
            cli_cargo_joined(folder, first->path, "/", file, sizeof(file));
            s_found->walk.target = (CliCargoTarget){
                first->kind,
                (first->kind == CLI_CARGO_LIB)
                    ? NULL
                    : found_put(tests, first->name,
                                cellul.len(&(CatenaFinitaCfg){.src = first->name, .cap = CLI_CELLS_POOL}))};
            found_walk(&s_found->walk, file, "", 1);
        }
    }
    cli_files_let(files);
    cli_cell_given(s_found);
    s_found = NULL;
    cli_lock_give(&s_found_lock);
}
