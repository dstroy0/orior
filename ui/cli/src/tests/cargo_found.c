// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_found.c: the Rust tests of a tree: each package's targets as Cargo finds them, and each target's modules
// followed from its first file, `mod name;` by `mod name;`, for the functions marked as tests
#include "../command_line/pool.h"
#include "../editor/files.h"
#include "cargo_tests_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// What the search of one package records: the tree, where the tests go, the manifest, package and target, and the files
// followed, where each starts in their texts, both in cells at their most
typedef struct
{
    const char *root;
    CliCargoTests *tests;
    const char *manifest;
    const char *package;
    CliCargoTarget target;
    unsigned long long *seen;
    unsigned long long seen_count;
    unsigned long long seen_capacity;
    char *seen_text;
    unsigned long long seen_used;
    unsigned long long seen_text_capacity;
} CliCargoWalk;

int cli_cargo_is_rust(const CliCargoTestsIdCfg *args)
{
    const char *const path = cellul.find(
        &(CatenaFinitaCfg){.src = args->id, .cap = CLI_POOL_BYTES, .other = "::", .other_cap = CLI_POOL_BYTES});
    const size_t length = (path != NULL) ? (size_t)(path - args->id)
                                         : cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_POOL_BYTES});
    return (length >= 3u) &&
           (memor.cmp(&(MemoriaCfg){.src = args->id + length - 3u, .other = ".rs", .bytes = 3u}) == 0);
}

typedef struct
{
    CliCargoTests *tests;
    const char *text;
    unsigned long long length;
    CliCargoWalk *walk;
    const char *file;
    const char *path;
    const CliRustItem *item;
    int owner_folder;
} CargoFoundCtx;
// a text put in the tests' texts, in the newest piece where it has capacity, else in a new piece put before it, so a
// text put stays where it is: where it is, "" where it does not fit or no piece could be taken
EMBED_INLINE const char *found_append(const CargoFoundCtx *args)
{
    if (args->tests->used + args->length + 1ULL > CLI_CARGO_TEXT)
    {
        args->tests->full = 1;
        return "";
    }
    CliCargoPiece *piece = args->tests->pieces;
    if ((piece == NULL) || ((piece->used + args->length + 1ULL) > piece->capacity))
    {
        const unsigned long long capacity =
            ((args->length + 1ULL) > CLI_CARGO_PIECE) ? (args->length + 1ULL) : CLI_CARGO_PIECE;
        piece = g_cli_guard->persistent_buf_alloc(sizeof(*piece) + capacity, g_cli_cargo_owner);
        if (piece == NULL)
        {
            args->tests->full = 1;
            return "";
        }
        *piece = (CliCargoPiece){args->tests->pieces, 0ULL, capacity};
        args->tests->pieces = piece;
    }
    char *const at = piece->text + piece->used;
    memor.cpy(&(MemoriaCfg){.dst = at, .src = args->text, .bytes = args->length});
    at[args->length] = '\0';
    piece->used += args->length + 1ULL;
    args->tests->used += args->length + 1ULL;
    return at;
}

// whether a file of the package was followed already: 1, else it is recorded as followed and 0
EMBED_INLINE int found_visit(const CargoFoundCtx *args)
{
    for (unsigned long long at = 0ULL; at < args->walk->seen_count; at += 1ULL)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = args->walk->seen_text + args->walk->seen[at], .other = args->file, .cap = CLI_POOL_BYTES}))
        {
            return 1;
        }
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_POOL_BYTES}) + 1u;
    if ((args->walk->seen_count < args->walk->seen_capacity) &&
        (args->walk->seen_used + length <= args->walk->seen_text_capacity))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->walk->seen_text + args->walk->seen_used, .src = args->file, .bytes = length});
        args->walk->seen[args->walk->seen_count] = args->walk->seen_used;
        args->walk->seen_count += 1ULL;
        args->walk->seen_used += length;
    }
    return 0;
}

// a test the search found put with its names
EMBED_INLINE void found_test(const CargoFoundCtx *args)
{
    const char *file = args->file;
    const char *const inline_names = g_cli_cargo_scan.names + args->item->inline_names;
    const char *const name = g_cli_cargo_scan.names + args->item->name;
    CliCargoTests *const tests = args->walk->tests;
    if (tests->count >= tests->capacity)
    {
        tests->full = 1;
        return;
    }
    char inside[4096];
    char full[4096];
    char id[CLI_PATH_MAX_LEN + 4096u];
    EMBED_CALL(cli_cargo_tests_a_internal.cargo_join, CliCargoTestsACtx, .a = inline_names, .b = name,
               .between = "::", .out = inside, .capacity = sizeof(inside));
    EMBED_CALL(cli_cargo_tests_a_internal.cargo_join, CliCargoTestsACtx, .a = args->path, .b = inside,
               .between = "::", .out = full, .capacity = sizeof(full));
    EMBED_CALL(cli_cargo_tests_a_internal.cargo_join, CliCargoTestsACtx, .a = file, .b = inside,
               .between = "::", .out = id, .capacity = sizeof(id));
    CliCargoFound *const found = &tests->found[tests->count];
    found->test.id = EMBED_CALL(found_append, CargoFoundCtx, .tests = tests, .text = id,
                                .length = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_POOL_BYTES}));
    found->test.file = EMBED_CALL(found_append, CargoFoundCtx, .tests = tests, .text = file,
                                  .length = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_POOL_BYTES}));
    found->test.class_name =
        (inline_names[0] != '\0')
            ? EMBED_CALL(found_append, CargoFoundCtx, .tests = tests, .text = inline_names,
                         .length = cellul.len(&(CatenaFinitaCfg){.src = inline_names, .cap = CLI_POOL_BYTES}))
            : NULL;
    found->test.name = EMBED_CALL(found_append, CargoFoundCtx, .tests = tests, .text = name,
                                  .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}));
    found->test.line = args->item->line;
    found->manifest = args->walk->manifest;
    found->package = args->walk->package;
    found->target = args->walk->target;
    found->full = EMBED_CALL(found_append, CargoFoundCtx, .tests = tests, .text = full,
                             .length = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_POOL_BYTES}));
    tests->count += 1u;
}

// the modules of `file`, a path in the tree whose own module path is `path`, followed: each test it contains found, and
// each module it names with `mod name;` followed in turn. A file that is a target's first, or a mod.rs, has its
// modules' files beside it; any other stores them in the folder of its own name
static void found_walk(const CargoFoundCtx *args)
{
    const char *path = args->path;
    if (found_visit(args))
    {
        return;
    }
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->walk->root, .relative = args->file, .joined = full);
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = full,
                                                  .size_pointer = &size, .owner = g_cli_cargo_owner);
    if (bytes == NULL)
    {
        return;
    }
    const unsigned int first = g_cli_cargo_scan.item_count;
    const unsigned long long names = g_cli_cargo_scan.name_used;
    unsigned int count = 0u;
    if (EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = (const char *)bytes, .size = size))
    {
        EMBED_CALL(cli_cargo_tests_internal.cargo_tokens, CliCargoTestsCtx, .text = (const char *)bytes, .size = size);
        count = EMBED_CALL(cli_cargo_tests_internal.cargo_scan, CliCargoTestsCtx, .text = (const char *)bytes);
    }
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_cargo_owner));
    for (unsigned int at = first; at < first + count; at += 1u)
    {
        if (g_cli_cargo_scan.items[at].test)
        {
            EMBED_CALL(found_test, CargoFoundCtx, .walk = args->walk, .file = args->file, .path = path,
                       .item = &g_cli_cargo_scan.items[at]);
        }
    }
    char base[CLI_PATH_MAX_LEN];
    const char *const slash = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->file,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_POOL_BYTES}), .byte = '/');
    const size_t folder_size = (slash != NULL) ? (size_t)(slash - args->file) : 0u;
    size_t stem_size = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_POOL_BYTES}) -
                       ((slash != NULL) ? folder_size + 1u : 0u);
    const char *const stem = (slash != NULL) ? slash + 1 : args->file;
    while ((stem_size >= 3u) &&
           (memor.cmp(&(MemoriaCfg){.src = stem + stem_size - 3u, .other = ".rs", .bytes = 3u}) == 0))
    {
        stem_size -= 3u;
    }
    VerbaLinea base_line = {base, sizeof(base), 0ULL};
    mmgr_line_put_n(&base_line, args->file, folder_size);
    if (!args->owner_folder)
    {
        mmgr_line_put(&base_line, (folder_size > 0u) ? "/" : "");
        mmgr_line_put_n(&base_line, stem, stem_size);
    }
    mmgr_line_finish(&base_line);
    for (unsigned int at = first; at < first + count; at += 1u)
    {
        const CliRustItem item = g_cli_cargo_scan.items[at];
        if (item.test)
        {
            continue;
        }
        const char *const inline_names = g_cli_cargo_scan.names + item.inline_names;
        const char *const name = g_cli_cargo_scan.names + item.name;
        char dir[CLI_PATH_MAX_LEN];
        char tail[CLI_PATH_MAX_LEN];
        char whole[4096];
        char inner[4096];
        VerbaLinea dir_line = {dir, sizeof(dir), 0ULL};
        mmgr_line_put(&dir_line, base);
        for (const char *one = inline_names; *one != '\0';)
        {
            const char *const next = cellul.find(
                &(CatenaFinitaCfg){.src = one, .cap = CLI_POOL_BYTES, .other = "::", .other_cap = CLI_POOL_BYTES});
            const size_t length = (next != NULL) ? (size_t)(next - one)
                                                 : cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_POOL_BYTES});
            mmgr_line_put(&dir_line, (dir_line.at > 0ULL) ? "/" : "");
            mmgr_line_put_n(&dir_line, one, length);
            one += length + ((next != NULL) ? 2u : 0u);
        }
        mmgr_line_finish(&dir_line);
        EMBED_CALL(cli_cargo_tests_a_internal.cargo_join, CliCargoTestsACtx, .a = inline_names, .b = name,
                   .between = "::", .out = inner, .capacity = sizeof(inner));
        EMBED_CALL(cli_cargo_tests_a_internal.cargo_join, CliCargoTestsACtx, .a = path, .b = inner,
                   .between = "::", .out = whole, .capacity = sizeof(whole));
        char flat[CLI_PATH_MAX_LEN];
        char nested[CLI_PATH_MAX_LEN];
        VerbaLinea tail_line = {tail, sizeof(tail), 0ULL};
        mmgr_line_put(&tail_line, name);
        mmgr_line_put(&tail_line, ".rs");
        mmgr_line_finish(&tail_line);
        EMBED_CALL(cli_cargo_tests_a_internal.cargo_join, CliCargoTestsACtx, .a = dir, .b = tail, .between = "/",
                   .out = flat, .capacity = sizeof(flat));
        tail_line.at = 0ULL;
        mmgr_line_put(&tail_line, name);
        mmgr_line_put(&tail_line, "/mod.rs");
        mmgr_line_finish(&tail_line);
        EMBED_CALL(cli_cargo_tests_a_internal.cargo_join, CliCargoTestsACtx, .a = dir, .b = tail, .between = "/",
                   .out = nested, .capacity = sizeof(nested));
        char flat_full[CLI_PATH_MAX_LEN];
        char nested_full[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->walk->root, .relative = flat, .joined = flat_full);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->walk->root, .relative = nested,
                   .joined = nested_full);
        if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = flat_full))
        {
            EMBED_CALL(found_walk, CargoFoundCtx, .walk = args->walk, .file = flat, .path = whole, .owner_folder = 0);
        }
        else if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = nested_full))
        {
            EMBED_CALL(found_walk, CargoFoundCtx, .walk = args->walk, .file = nested, .path = whole, .owner_folder = 1);
        }
    }
    g_cli_cargo_scan.item_count = first;
    g_cli_cargo_scan.name_used = names;
}

void cli_cargo_find(const CliCargoTestsCfg *args)
{
    static const char *const s_not_searched[4] = {"target", "node_modules", ".git", "build"};
    CliCargoWalk walk = {args->root, args->tests, NULL, NULL, {0u, NULL}, NULL, 0ULL, 0ULL, NULL, 0ULL, 0ULL};
    CliCargoPiece *piece = args->tests->pieces;
    while (piece != NULL)
    {
        CliCargoPiece *const next = piece->next;
        g_cli_guard->persistent_buf_release(piece, g_cli_cargo_owner);
        piece = next;
    }
    g_cli_guard->persistent_buf_release(args->tests->found, g_cli_cargo_owner);
    *args->tests = (CliCargoTests){NULL, 0u, 0u, NULL, 0ULL, 0};
    g_cli_guard->persistent_buf_release(g_cli_cargo_scan.tokens, g_cli_cargo_owner);
    g_cli_guard->persistent_buf_release(g_cli_cargo_scan.items, g_cli_cargo_owner);
    memor.set(&(MemoriaCfg){.dst = &g_cli_cargo_scan, .val = 0, .bytes = sizeof(g_cli_cargo_scan)});
    args->tests->found = g_cli_guard->persistent_buf_alloc(
        (unsigned long long)CLI_CARGO_FOUND * sizeof(*args->tests->found), g_cli_cargo_owner);
    args->tests->capacity = (args->tests->found != NULL) ? CLI_CARGO_FOUND : 0u;
    const unsigned long long seen_bytes = (unsigned long long)CLI_CARGO_FOUND * sizeof(*walk.seen);
    char *const seen_cell = g_cli_guard->temporary_buf_alloc(seen_bytes + CLI_CARGO_TEXT, g_cli_cargo_owner);
    if (seen_cell != NULL)
    {
        walk.seen = (unsigned long long *)(void *)seen_cell;
        walk.seen_capacity = CLI_CARGO_FOUND;
        walk.seen_text = seen_cell + seen_bytes;
        walk.seen_text_capacity = CLI_CARGO_TEXT;
    }
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = args->root);
    for (unsigned int index = 0u; (files != NULL) && (index < files->count); index += 1u)
    {
        const char *const manifest = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = index);
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = manifest, .cap = CLI_POOL_BYTES});
        if (!((length == 10u) &&
              (cellul.eq(&(CatenaFinitaCfg){.src = manifest, .other = "Cargo.toml", .cap = CLI_POOL_BYTES}))) &&
            !((length > 10u) && (cellul.eq(&(CatenaFinitaCfg){
                                    .src = manifest + length - 11u, .other = "/Cargo.toml", .cap = CLI_POOL_BYTES}))))
        {
            continue;
        }
        int searched = 1;
        for (const char *part = manifest; searched && (*part != '\0');)
        {
            const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_POOL_BYTES, .byte = '/'});
            const size_t size = (slash != NULL) ? (size_t)(slash - part)
                                                : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_POOL_BYTES});
            for (unsigned int skipped = 0u; skipped < 4u; skipped += 1u)
            {
                searched =
                    searched &&
                    !((cellul.len(&(CatenaFinitaCfg){.src = s_not_searched[skipped], .cap = CLI_POOL_BYTES}) == size) &&
                      (memor.cmp(&(MemoriaCfg){.src = part, .other = s_not_searched[skipped], .bytes = size}) == 0));
            }
            part += size + ((slash != NULL) ? 1u : 0u);
        }
        char manifest_full[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = manifest,
                   .joined = manifest_full);
        unsigned long long size = 0ULL;
        const unsigned char *const bytes =
            searched ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = manifest_full,
                                  .size_pointer = &size, .owner = g_cli_cargo_owner)
                     : NULL;
        if (bytes == NULL)
        {
            continue;
        }
        char package[CLI_NAME_MAX_LEN];
        char folder[CLI_PATH_MAX_LEN];
        VerbaLinea folder_line = {folder, sizeof(folder), 0ULL};
        size_t folder_size = length - 10u;
        folder_size = (size_t)EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = manifest,
                                         .at = folder_size, .set = "/");
        mmgr_line_put_n(&folder_line, manifest, folder_size);
        mmgr_line_finish(&folder_line);
        const int read =
            EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = (const char *)bytes, .size = size) &&
            EMBED_CALL(cli_cargo_tests_internal.cargo_package_name, CliCargoTestsCtx, .text = (const char *)bytes,
                       .size = size, .name = package);
        char package_folder[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = folder,
                   .joined = package_folder);
        unsigned int target_count = 0u;
        const CliCargoFirst *const firsts =
            read ? EMBED_CALL(cli_cargo_tests_text_size_internal.cargo_targets, CliCargoTestsTextSizeCtx,
                              .folder = package_folder, .text = (const char *)bytes, .size = size, .package = package,
                              .count = &target_count)
                 : NULL;
        g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_cargo_owner));
        if (!read)
        {
            continue;
        }
        walk.manifest =
            EMBED_CALL(found_append, CargoFoundCtx, .tests = args->tests, .text = manifest, .length = length);
        walk.package = EMBED_CALL(found_append, CargoFoundCtx, .tests = args->tests, .text = package,
                                  .length = cellul.len(&(CatenaFinitaCfg){.src = package, .cap = CLI_POOL_BYTES}));
        walk.seen_count = 0ULL;
        walk.seen_used = 0ULL;
        for (unsigned int target = 0u; target < target_count; target += 1u)
        {
            const CliCargoFirst *const first = &firsts[target];
            char file[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_cargo_tests_a_internal.cargo_join, CliCargoTestsACtx, .a = folder, .b = first->path,
                       .between = "/", .out = file, .capacity = sizeof(file));
            walk.target = (CliCargoTarget){
                first->kind,
                (first->kind == CLI_CARGO_LIB)
                    ? NULL
                    : EMBED_CALL(found_append, CargoFoundCtx, .tests = args->tests, .text = first->name,
                                 .length = cellul.len(&(CatenaFinitaCfg){.src = first->name, .cap = CLI_POOL_BYTES}))};
            EMBED_CALL(found_walk, CargoFoundCtx, .walk = &walk, .file = file, .path = "", .owner_folder = 1);
        }
    }
    cli_cargo_firsts_free();
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
    g_cli_guard->persistent_buf_release(seen_cell, g_cli_cargo_owner);
    g_cli_guard->persistent_buf_release(g_cli_cargo_scan.tokens, g_cli_cargo_owner);
    g_cli_guard->persistent_buf_release(g_cli_cargo_scan.items, g_cli_cargo_owner);
    memor.set(&(MemoriaCfg){.dst = &g_cli_cargo_scan, .val = 0, .bytes = sizeof(g_cli_cargo_scan)});
}
