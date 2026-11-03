// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_jobs.c: the facts of this machine a build job is made from, the C compiler and linker a build for another
// machine needs, and the params of the job that builds each program the tree's scripts make
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "executables_internal.h"

// the target choice that builds for the machine the build runs on, and the cgo choice that leaves it to Go
static const char s_this_machine[] = "this machine";

static const char s_as_go_chooses[] = "as Go chooses";

// the targets a Rust binary can be built for, by Rust's own names: the musl ones link as a whole file that requires
// no glibc
static const char *const s_rust_targets[8] = {
    s_this_machine,           "x86_64-unknown-linux-musl", "aarch64-unknown-linux-musl", "x86_64-unknown-linux-gnu",
    "x86_64-pc-windows-msvc", "aarch64-pc-windows-msvc",   "x86_64-apple-darwin",        "aarch64-apple-darwin"};

void cli_program_file(const CliExecutablesNameOutFolderCtx *args)
{
    VerbaLinea line = {args->out, CLI_NAME_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->name);
#if defined(_WIN32)
    mmgr_line_put(&line, ".exe");
#endif
    mmgr_line_finish(&line);
}

void cli_folder_get(const CliExecutablesOutCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->script, .out = folder))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = folder,
                          .src = args->root,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}) + 1u});
    }
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = folder,
               .relative_out = args->out);
    if (args->out[0] == '\0')
    {
        memor.cpy(&(MemoriaCfg){.dst = args->out,
                                .src = ".",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
    }
}

void cli_build_folder(const CliExecutablesOutCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_executables_out_internal.folder_get, CliExecutablesOutCtx, .root = args->root,
               .script = args->script, .out = folder);
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, "build/");
    mmgr_line_put(&line, args->tool);
    mmgr_line_ch(&line, '/');
    if (!cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = ".", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(&line, folder);
        mmgr_line_ch(&line, '/');
    }
    mmgr_line_finish(&line);
}

void cli_go_host(const CliExecutablesSystemCtx *args)
{
    const char **system = args->system;
    const char **processor = args->processor;
#if defined(_WIN32)
    *system = "windows";
#elif defined(__APPLE__)
    *system = "darwin";
#else
    *system = "linux";
#endif
#if defined(__x86_64__) || defined(_M_X64)
    *processor = "amd64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    *processor = "arm64";
#elif defined(__i386__) || defined(_M_IX86)
    *processor = "x86";
#elif defined(__arm__) || defined(_M_ARM)
    *processor = "arm";
#else
    *processor = "riscv64";
#endif
}

const char *cli_first(const CliExecutablesKeyCtx *args)
{
    const char *const first =
        EMBED_CALL(cli_catalog_key.values_first, CliCatalogKeyCfg, .values = args->values, .key = args->key);
    return ((first != NULL) && (first[0] != '\0')) ? first : args->fallback;
}

typedef struct
{
    const char *system;
    const char *processor;
    char *out;
    const char *zig;
    const char *language;
    const char *target;
    unsigned long long capacity;
    CliCatalog *catalog;
    const char *key;
    const char *const *choices;
    unsigned int count;
    const CliExecutable *exe;
} ExecutablesJobsCtx;
// Zig's name for a system and processor, as `zig cc -target` takes it, from Go's names or Rust's target, into `out`,
// sized for CLI_NAME_MAX_LEN: 1, or 0 where Zig has none
EMBED_INLINE int cli_zig_target(const ExecutablesJobsCtx *args)
{
    const char *const zig_processor =
        ((cellul.eq(&(CatenaFinitaCfg){.src = args->processor, .other = "amd64", .cap = CLI_CELLS_POOL})) ||
         (cellul.eq(&(CatenaFinitaCfg){.src = args->processor, .other = "x86_64", .cap = CLI_CELLS_POOL})))
            ? "x86_64"
        : ((cellul.eq(&(CatenaFinitaCfg){.src = args->processor, .other = "arm64", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = args->processor, .other = "aarch64", .cap = CLI_CELLS_POOL})))
            ? "aarch64"
            : NULL;
    const char *const zig_system =
        (cellul.eq(&(CatenaFinitaCfg){.src = args->system, .other = "linux", .cap = CLI_CELLS_POOL})) ? "linux-musl"
        : (cellul.eq(&(CatenaFinitaCfg){.src = args->system, .other = "windows", .cap = CLI_CELLS_POOL}))
            ? "windows-gnu"
        : ((cellul.eq(&(CatenaFinitaCfg){.src = args->system, .other = "darwin", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = args->system, .other = "macos", .cap = CLI_CELLS_POOL})))
            ? "macos"
            : NULL;
    if ((zig_processor == NULL) || (zig_system == NULL))
    {
        return 0;
    }
    VerbaLinea line = {args->out, CLI_NAME_MAX_LEN, 0ULL};
    mmgr_line_put(&line, zig_processor);
    mmgr_line_ch(&line, '-');
    mmgr_line_put(&line, zig_system);
    return mmgr_line_finish(&line) != 0ULL;
}

// `zig cc` or `zig c++` for a Zig target, as a variable of the environment stores it, into `out`
EMBED_INLINE void cli_zig_command(const ExecutablesJobsCtx *args)
{
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    mmgr_line_put(&line, args->zig);
    mmgr_line_ch(&line, ' ');
    mmgr_line_put(&line, args->language);
    mmgr_line_put(&line, " -target ");
    mmgr_line_put(&line, args->target);
    mmgr_line_finish(&line);
}

void cli_executables_cross_env(const CliExecutablesKindCfg *args)
{
    const CliValues *values = args->values;
    char zig[CLI_PATH_MAX_LEN + 2u];
    char found[CLI_PATH_MAX_LEN];
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int has_zig = EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "zig", .name = "zig",
                                   .path = found, .err_msg = &err_msg);
    VerbaLinea quoted = {zig, sizeof(zig), 0ULL};
    const int spaced = cellul.chr(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL, .byte = ' '}) != NULL;
    mmgr_line_put(&quoted, (has_zig && spaced) ? "\"" : "");
    mmgr_line_put(&quoted, has_zig ? found : "");
    mmgr_line_put(&quoted, (has_zig && spaced) ? "\"" : "");
    mmgr_line_finish(&quoted);
    char target[CLI_NAME_MAX_LEN];
    char command[CLI_PATH_MAX_LEN + CLI_NAME_MAX_LEN];
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->kind, .other = "go", .cap = CLI_CELLS_POOL}))
    {
        const char *host_system = NULL;
        const char *host_processor = NULL;
        EMBED_CALL(cli_executables_system_internal.go_host, CliExecutablesSystemCtx, .system = &host_system,
                   .processor = &host_processor);
        const char *const system = EMBED_CALL(cli_executables_key_internal.first, CliExecutablesKeyCtx,
                                              .values = values, .key = "GOOS", .fallback = host_system);
        const char *const processor = EMBED_CALL(cli_executables_key_internal.first, CliExecutablesKeyCtx,
                                                 .values = values, .key = "GOARCH", .fallback = host_processor);
        const int other =
            (!cellul.eq(&(CatenaFinitaCfg){.src = system, .other = host_system, .cap = CLI_CELLS_POOL})) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = processor, .other = host_processor, .cap = CLI_CELLS_POOL}));
        if ((cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_executables_key_internal.first, CliExecutablesKeyCtx,
                                                            .values = values, .key = "CGO_ENABLED",
                                                            .fallback = s_as_go_chooses),
                                          .other = "1",
                                          .cap = CLI_CELLS_POOL})) &&
            other && has_zig &&
            EMBED_CALL(cli_zig_target, ExecutablesJobsCtx, .system = system, .processor = processor, .out = target))
        {
            EMBED_CALL(cli_zig_command, ExecutablesJobsCtx, .zig = zig, .language = "cc", .target = target,
                       .out = command, .capacity = sizeof(command));
            EMBED_CALL(args->each, CliEachNameValueCfg, .context = args->context, .name = "CC", .value = command);
            EMBED_CALL(cli_zig_command, ExecutablesJobsCtx, .zig = zig, .language = "c++", .target = target,
                       .out = command, .capacity = sizeof(command));
            EMBED_CALL(args->each, CliEachNameValueCfg, .context = args->context, .name = "CXX", .value = command);
        }
        return;
    }
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->kind, .other = "rust", .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    const char *const rust = EMBED_CALL(cli_executables_key_internal.first, CliExecutablesKeyCtx, .values = values,
                                        .key = "target", .fallback = s_this_machine);
    if (cellul.eq(&(CatenaFinitaCfg){.src = rust, .other = s_this_machine, .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    // the target's variables' names: the target in upper case, `_` for each `-`
    char upper[CLI_NAME_MAX_LEN];
    char lower[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){
        .dst = upper, .src = rust, .bytes = cellul.len(&(CatenaFinitaCfg){.src = rust, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){
        .dst = lower, .src = rust, .bytes = cellul.len(&(CatenaFinitaCfg){.src = rust, .cap = CLI_CELLS_POOL}) + 1u});
    const unsigned long long upper_size = cellul.len(&(CatenaFinitaCfg){.src = upper, .cap = CLI_CELLS_POOL});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = upper, .size = upper_size, .from = '-',
               .to = '_');
    EMBED_CALL(cli_text_size_from.raise, CliTextSizeFromCfg, .text_out = upper, .size = upper_size);
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = lower, .size = upper_size, .from = '-',
               .to = '_');
    char linker[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea named = {linker, sizeof(linker), 0ULL};
    mmgr_line_put(&named, "CARGO_TARGET_");
    mmgr_line_put(&named, upper);
    mmgr_line_put(&named, "_LINKER");
    mmgr_line_finish(&named);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = rust, .cap = CLI_CELLS_POOL});
    char set[CLI_PATH_MAX_LEN];
#if defined(__linux__)
    const int on_linux = 1;
#else
    const int on_linux = 0;
#endif
    if ((length >= 11ULL) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = rust + length - 11ULL, .other = "-linux-musl", .cap = CLI_CELLS_POOL})) &&
        !on_linux &&
        !EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = linker, .value_out = set,
                    .capacity = sizeof(set)))
    {
        EMBED_CALL(args->each, CliEachNameValueCfg, .context = args->context, .name = linker, .value = "rust-lld");
    }
    char processor[CLI_NAME_MAX_LEN];
    const char *const dash = cellul.chr(&(CatenaFinitaCfg){.src = rust, .cap = CLI_CELLS_POOL, .byte = '-'});
    VerbaLinea part = {processor, sizeof(processor), 0ULL};
    mmgr_line_put_n(&part, rust, (dash != NULL) ? (unsigned long long)(dash - rust) : length);
    mmgr_line_finish(&part);
    const char *const system =
        (cellul.find(&(CatenaFinitaCfg){
             .src = rust, .cap = CLI_CELLS_POOL, .other = "linux", .other_cap = CLI_CELLS_POOL}) != NULL)
            ? "linux"
        : (cellul.find(&(CatenaFinitaCfg){
               .src = rust, .cap = CLI_CELLS_POOL, .other = "windows-gnu", .other_cap = CLI_CELLS_POOL}) != NULL)
            ? "windows"
        : (cellul.find(&(CatenaFinitaCfg){
               .src = rust, .cap = CLI_CELLS_POOL, .other = "apple", .other_cap = CLI_CELLS_POOL}) != NULL)
            ? "darwin"
            : "";
    if (has_zig &&
        EMBED_CALL(cli_zig_target, ExecutablesJobsCtx, .system = system, .processor = processor, .out = target))
    {
        char compiler[CLI_NAME_MAX_LEN * 2u];
        VerbaLinea name = {compiler, sizeof(compiler), 0ULL};
        mmgr_line_put(&name, "CC_");
        mmgr_line_put(&name, lower);
        mmgr_line_finish(&name);
        EMBED_CALL(cli_zig_command, ExecutablesJobsCtx, .zig = zig, .language = "cc", .target = target, .out = command,
                   .capacity = sizeof(command));
        EMBED_CALL(args->each, CliEachNameValueCfg, .context = args->context, .name = compiler, .value = command);
    }
}

// a choice param put on the last job, its first choice its default
EMBED_INLINE void cli_choice(const ExecutablesJobsCtx *args)
{
    CliCatalog *catalog = args->catalog;
    const char *key = args->key;
    EMBED_CALL(cli_catalog_key_kind.param, CliCatalogKeyKindCfg, .catalog = catalog, .key = key, .kind = "choice",
               .required = 1);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = catalog,
                   .choice = args->choices[at]);
    }
}

// the params a kind of build takes before those every build takes
EMBED_INLINE void cli_kind_params(const ExecutablesJobsCtx *args)
{
    static const char *const s_profiles[2] = {"dev", "release"};
    static const char *const s_configs[4] = {"Debug", "Release", "RelWithDebInfo", "MinSizeRel"};
    static const char *const s_buildtypes[4] = {"debug", "debugoptimized", "release", "minsize"};
    static const char *const s_cgo[3] = {s_as_go_chooses, "0", "1"};
    static const char *const s_dotnet_configs[2] = {"Debug", "Release"};
    static const char *const s_outputs[2] = {"files", "one file"};
    static const char *const s_optimize[4] = {"Debug", "ReleaseSafe", "ReleaseFast", "ReleaseSmall"};
    if (args->exe->kind == CLI_KIND_CARGO)
    {
        EMBED_CALL(cli_choice, ExecutablesJobsCtx, .catalog = args->catalog, .key = "profile", .choices = s_profiles,
                   .count = 2u);
        EMBED_CALL(cli_choice, ExecutablesJobsCtx, .catalog = args->catalog, .key = "target", .choices = s_rust_targets,
                   .count = 8u);
    }
    else if (args->exe->kind == CLI_KIND_CMAKE)
    {
        EMBED_CALL(cli_choice, ExecutablesJobsCtx, .catalog = args->catalog, .key = "config", .choices = s_configs,
                   .count = 4u);
    }
    else if (args->exe->kind == CLI_KIND_MESON)
    {
        EMBED_CALL(cli_choice, ExecutablesJobsCtx, .catalog = args->catalog, .key = "buildtype",
                   .choices = s_buildtypes, .count = 4u);
    }
    else if (args->exe->kind == CLI_KIND_GO)
    {
        const char *system = NULL;
        const char *processor = NULL;
        EMBED_CALL(cli_executables_system_internal.go_host, CliExecutablesSystemCtx, .system = &system,
                   .processor = &processor);
        // each choice with this machine's first
        const char *const systems_all[3] = {"windows", "linux", "darwin"};
        const char *const processors_all[2] = {"amd64", "arm64"};
        const char *systems[4] = {system, NULL, NULL, NULL};
        const char *processors[3] = {processor, NULL, NULL};
        unsigned int system_count = 1u;
        unsigned int processor_count = 1u;
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            if (!cellul.eq(&(CatenaFinitaCfg){.src = systems_all[at], .other = system, .cap = CLI_CELLS_POOL}))
            {
                systems[system_count] = systems_all[at];
                system_count += 1u;
            }
        }
        for (unsigned int at = 0u; at < 2u; at += 1u)
        {
            if (!cellul.eq(&(CatenaFinitaCfg){.src = processors_all[at], .other = processor, .cap = CLI_CELLS_POOL}))
            {
                processors[processor_count] = processors_all[at];
                processor_count += 1u;
            }
        }
        EMBED_CALL(cli_choice, ExecutablesJobsCtx, .catalog = args->catalog, .key = "GOOS", .choices = systems,
                   .count = system_count);
        EMBED_CALL(cli_choice, ExecutablesJobsCtx, .catalog = args->catalog, .key = "GOARCH", .choices = processors,
                   .count = processor_count);
        EMBED_CALL(cli_choice, ExecutablesJobsCtx, .catalog = args->catalog, .key = "CGO_ENABLED", .choices = s_cgo,
                   .count = 3u);
    }
    else if (args->exe->kind == CLI_KIND_DOTNET)
    {
        EMBED_CALL(cli_choice, ExecutablesJobsCtx, .catalog = args->catalog, .key = "configuration",
                   .choices = s_dotnet_configs, .count = 2u);
        EMBED_CALL(cli_choice, ExecutablesJobsCtx, .catalog = args->catalog, .key = "output", .choices = s_outputs,
                   .count = 2u);
    }
    else if ((args->exe->kind == CLI_KIND_ZIG) && args->exe->optimize)
    {
        EMBED_CALL(cli_choice, ExecutablesJobsCtx, .catalog = args->catalog, .key = "optimize", .choices = s_optimize,
                   .count = 4u);
    }
    else if ((args->exe->kind == CLI_KIND_NPM) && (args->exe->withs != 0u))
    {
        EMBED_CALL(cli_choice, ExecutablesJobsCtx, .catalog = args->catalog, .key = "with", .choices = args->exe->with,
                   .count = args->exe->withs);
    }
}

void cli_executables_jobs(const CliCatalogRootCfg *args)
{
    const char *root = args->root;
    CliCatalog *catalog = args->catalog;
    CliExecutables found = {NULL, 0u, 0u, NULL};
    EMBED_CALL(cli_executables_found_internal.read, CliExecutablesFoundCtx, .root = root, .found = &found);
    for (unsigned int index = 0u; index < found.count; index += 1u)
    {
        const CliExecutable *const exe = &found.item[found.order[index]];
        unsigned int named = 0u;
        for (unsigned int other = 0u; other < found.count; other += 1u)
        {
            named += (cellul.eq(
                         &(CatenaFinitaCfg){.src = found.item[other].name, .other = exe->name, .cap = CLI_CELLS_POOL}))
                         ? 1u
                         : 0u;
        }
        char title[CLI_PATH_MAX_LEN];
        VerbaLinea line = {title, sizeof(title), 0ULL};
        if (named > 1u)
        {
            char folder[CLI_PATH_MAX_LEN];
            const char *base = "";
            if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = exe->script,
                           .out = folder))
            {
                base = folder;
                const unsigned long long base_end = EMBED_CALL(
                    cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = folder,
                    .at = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), .set = "/\\");
                base = (base_end != 0ULL) ? (folder + base_end) : base;
            }
            mmgr_line_put(&line, base);
            mmgr_line_ch(&line, '/');
        }
        mmgr_line_put(&line, exe->name);
        mmgr_line_finish(&line);
        if (EMBED_CALL(cli_executables_root_internal.job_begin, CliExecutablesRootCtx, .root = root, .exe = exe,
                       .title = title, .catalog = catalog))
        {
            EMBED_CALL(cli_kind_params, ExecutablesJobsCtx, .exe = exe, .catalog = catalog);
            static const char *const s_then[3] = {"build", "run", "run as the system runs it"};
            EMBED_CALL(cli_choice, ExecutablesJobsCtx, .catalog = catalog, .key = "then", .choices = s_then,
                       .count = 3u);
            EMBED_CALL(cli_catalog_choice_table.trace, CliCatalogChoiceCfg, .catalog = catalog);
            EMBED_CALL(cli_catalog_key_kind.param, CliCatalogKeyKindCfg, .catalog = catalog, .key = "folder",
                       .kind = "dir", .required = 0);
            EMBED_CALL(cli_executables_root_internal.steps, CliExecutablesRootCtx, .root = root, .exe = exe,
                       .catalog = catalog);
        }
    }
    EMBED_CALL(cli_executables_found_internal.free, CliExecutablesFoundCtx, .found = &found);
}
