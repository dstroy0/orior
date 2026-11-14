// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_jobs.c: the driver's jobs, the runners, the page builders, the stage scripts and their pipelines, the test
// suites, and the whole catalog read in the order the engine's own steps run
#include "../text/text.h"
#include "catalog_internal.h"
#include "pool.h"

// A reading's lists, a cell taken with the lock and given back with it: the runs, parts and paths listed, the
// subjects, the folders within them and their files, and the folders of each depth collected, each list in a cell of
// its own sized to what it contains
typedef struct
{
    CliPaths runs;
    CliEntries parts;
    CliPaths paths;
    CliEntries subjects;
    CliEntries inner;
    CliEntries files;
    CliEntries dirs[17];
} CliCatalogJobsWorkState;

static CliCatalogJobsWorkState *s_jobs;

typedef struct
{
    CliCatalog *catalog;
    const char *key;
    char (*list)[CLI_PATH_MAX_LEN];
    unsigned int count;
    int required;
    const char *root;
    const char *path;
    unsigned int *seen;
    const char *name;
    const char *under;
    unsigned int depth;
} CatalogJobsCtx;
// the arguments every run of the driver ends with
EMBED_INLINE void cli_set(const CatalogJobsCtx *args)
{
    CliCatalog *catalog = args->catalog;
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_VALUE, .a = "set",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_WORDS, .a = "samples",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
}

// a choice param of the runs listed
EMBED_INLINE void cli_choice(const CatalogJobsCtx *args)
{
    CliCatalog *catalog = args->catalog;
    const char *key = args->key;
    int required = args->required;
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = key, .type = "choice",
               .required = required);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = catalog, .choice = args->list[at]);
    }
}

// The cell tracker's driver as ingest, as a run of its parts, and as a render of what it found
EMBED_INLINE void cli_driver(const CatalogJobsCtx *args)
{
    const char *root = args->root;
    CliCatalog *catalog = args->catalog;
    char source[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char tracker[CLI_PATH_MAX_LEN];
    char base[CLI_PATH_MAX_LEN];
    char cfg[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = root,
               .name = "examples/cell_tracking/src/track_driver/track_driver.cu", .path_out = source);
    char *const text = EMBED_CALL(cli_catalog_path_out_internal.script_alloc, CliCatalogPathOutCtx, .path = source);
    if (text == NULL)
    {
        return;
    }
    (void)EMBED_CALL(cli_catalog_names_internal.names_capacity, CliCatalogNamesCtx, .names = &s_jobs->parts,
                     .most = EMBED_CALL(cli_catalog_capacity_internal.driver_parts, CliCatalogCapacityCtx,
                                        .source = text, .parts = NULL, .capacity = 0u));
    const unsigned int parts =
        (s_jobs->parts.name != NULL)
            ? EMBED_CALL(cli_catalog_capacity_internal.driver_parts, CliCatalogCapacityCtx, .source = text,
                         .parts = s_jobs->parts.name, .capacity = s_jobs->parts.capacity)
            : 0u;
    g_cli_guard->persistent_buf_release(text, g_cli_catalog_owner);
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = source, .relative_out = file);
    char *const reserved = EMBED_CALL(cli_catalog_path_internal.about_alloc, CliCatalogPathCtx, .path = source);
    const char *const about = (reserved != NULL) ? reserved : "";
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = root,
               .name = "examples/cell_tracking", .path_out = tracker);
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = tracker, .name = "base.cfg",
               .path_out = base);
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = tracker, .name = "cfg",
               .path_out = cfg);
    unsigned int runs = 0u;
    if (EMBED_CALL(cli_catalog_max_internal.paths_capacity, CliCatalogMaxCtx, .paths = &s_jobs->runs, .most = 1u))
    {
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = base,
                   .relative_out = s_jobs->runs.path[0]);
        runs = EMBED_CALL(cli_catalog_filter_internal.paths, CliCatalogFilterCtx, .root = root, .dir = cfg,
                          .filter = cli_catalog_is_cfg, .list = &s_jobs->runs, .at = 1u);
    }

    EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = catalog, .id = "ingest/track_driver",
               .group = "ingest", .title = "track_driver --ingest", .file = file, .about = about, .opens = "");
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "source", .type = "dir",
               .required = 1);
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "axes", .type = "word",
               .required = 0);
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "channel", .type = "word",
               .required = 0);
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "set", .type = "save-dir",
               .required = 1);
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "samples", .type = "words",
               .required = 0);
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = catalog,
               .program = CLI_PROGRAM_BUILT, .text = "track_driver", .tool_program = NULL, .system = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_LIT, .a = "--ingest",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_FLAG, .a = "--source",
               .b = "source", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_FLAG, .a = "--axes",
               .b = "axes", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_FLAG, .a = "--channel",
               .b = "channel", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_set, CatalogJobsCtx, .catalog = catalog);

    EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = catalog, .id = "run/track_driver",
               .group = "run", .title = "track_driver --run", .file = file, .about = about, .opens = "");
    EMBED_CALL(cli_choice, CatalogJobsCtx, .catalog = catalog, .key = "cfg", .list = s_jobs->runs.path, .count = runs,
               .required = 0);
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "cfg-out", .type = "save",
               .required = 0);
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "run", .type = "many",
               .required = 0);
    for (unsigned int at = 0u; at < parts; at += 1u)
    {
        EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = catalog,
                   .choice = s_jobs->parts.name[at]);
    }
    EMBED_CALL(cli_catalog_choice_table.words, CliCatalogChoiceCfg, .catalog = catalog);
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "set", .type = "dir",
               .required = 0);
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "samples", .type = "words",
               .required = 0);
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = catalog,
               .program = CLI_PROGRAM_BUILT, .text = "track_driver", .tool_program = NULL, .system = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_FLAG, .a = "--cfg",
               .b = "cfg", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_FLAG, .a = "--cfg-out",
               .b = "cfg-out", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_EACH, .a = "--run",
               .b = "run", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_WORDS, .a = "arguments",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_set, CatalogJobsCtx, .catalog = catalog);

    EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = catalog, .id = "render/track_driver",
               .group = "render", .title = "track_driver --vis", .file = file, .about = about, .opens = "");
    EMBED_CALL(cli_choice, CatalogJobsCtx, .catalog = catalog, .key = "cfg", .list = s_jobs->runs.path, .count = runs,
               .required = 0);
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "vis", .type = "save",
               .required = 0);
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "export", .type = "save",
               .required = 0);
    EMBED_CALL(cli_catalog_choice_table.words, CliCatalogChoiceCfg, .catalog = catalog);
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "set", .type = "dir",
               .required = 0);
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "samples", .type = "words",
               .required = 0);
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = catalog,
               .program = CLI_PROGRAM_BUILT, .text = "track_driver", .tool_program = NULL, .system = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_FLAG, .a = "--cfg",
               .b = "cfg", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_FLAG, .a = "--vis",
               .b = "vis", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_FLAG, .a = "--export",
               .b = "export", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_WORDS, .a = "arguments",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_set, CatalogJobsCtx, .catalog = catalog);
    g_cli_guard->persistent_buf_release(reserved, g_cli_catalog_owner);
}

// The other runners: the Navier-Stokes programs on their cfgs and the qasm circuits
EMBED_INLINE void cli_runners(const CatalogJobsCtx *args)
{
    const char *root = args->root;
    CliCatalog *catalog = args->catalog;
    char navier[CLI_PATH_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = root,
               .name = "examples/navier_stokes/run.sh", .path_out = navier);
    char *const text = EMBED_CALL(cli_catalog_path_out_internal.script_alloc, CliCatalogPathOutCtx, .path = navier);
    if (text != NULL)
    {
        (void)EMBED_CALL(cli_catalog_names_internal.names_capacity, CliCatalogNamesCtx, .names = &s_jobs->parts,
                         .most = EMBED_CALL(cli_catalog_text_variable_internal.case_names, CliCatalogTextVariableCtx,
                                            .text_read = text, .variable = "PROGRAM", .names_callback = NULL,
                                            .capacity = 0u));
        const unsigned int programs =
            (s_jobs->parts.name != NULL)
                ? EMBED_CALL(cli_catalog_text_variable_internal.case_names, CliCatalogTextVariableCtx,
                             .text_read = text, .variable = "PROGRAM", .names_callback = s_jobs->parts.name,
                             .capacity = s_jobs->parts.capacity)
                : 0u;
        g_cli_guard->persistent_buf_release(text, g_cli_catalog_owner);
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = root,
                   .name = "examples/navier_stokes/cfg", .path_out = folder);
        const unsigned int configs =
            EMBED_CALL(cli_catalog_filter_internal.paths, CliCatalogFilterCtx, .root = root, .dir = folder,
                       .filter = cli_catalog_is_cfg, .list = &s_jobs->runs, .at = 0u);
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = navier,
                   .relative_out = script);
        EMBED_CALL(cli_catalog_path_internal.script_job, CliCatalogPathCtx, .catalog = catalog, .root = root,
                   .group = "run", .path = navier, .title = NULL, .opens = "");
        EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "program",
                   .type = "choice", .required = 1);
        for (unsigned int at = 0u; at < programs; at += 1u)
        {
            EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = catalog,
                       .choice = s_jobs->parts.name[at]);
        }
        EMBED_CALL(cli_choice, CatalogJobsCtx, .catalog = catalog, .key = "cfg", .list = s_jobs->runs.path,
                   .count = configs, .required = 1);
        EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = catalog,
                   .program = CLI_PROGRAM_BASH, .text = script, .tool_program = NULL, .system = 0);
        EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_VALUE,
                   .a = "program", .b = NULL, .c = NULL, .d = NULL, .separated = 0);
        EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_VALUE, .a = "cfg",
                   .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    }
    char qasm[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = root,
               .name = "examples/qasm/run.sh", .path_out = qasm);
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = qasm))
    {
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = root,
                   .name = "examples/qasm/test", .path_out = folder);
        const unsigned int circuits =
            EMBED_CALL(cli_catalog_filter_internal.paths, CliCatalogFilterCtx, .root = root, .dir = folder,
                       .filter = cli_catalog_is_qasm, .list = &s_jobs->runs, .at = 0u);
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = qasm, .relative_out = script);
        EMBED_CALL(cli_catalog_path_internal.script_job, CliCatalogPathCtx, .catalog = catalog, .root = root,
                   .group = "run", .path = qasm, .title = NULL, .opens = "");
        EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "circuit",
                   .type = "choice-or-file", .required = 1);
        for (unsigned int at = 0u; at < circuits; at += 1u)
        {
            EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = catalog,
                       .choice = s_jobs->runs.path[at]);
        }
        EMBED_CALL(cli_catalog_choice_table.words, CliCatalogChoiceCfg, .catalog = catalog);
        EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = catalog,
                   .program = CLI_PROGRAM_BASH, .text = script, .tool_program = NULL, .system = 0);
        EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_WORDS,
                   .a = "arguments", .b = NULL, .c = NULL, .d = NULL, .separated = 0);
        EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_VALUE,
                   .a = "circuit", .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    }
}

// one page builder's job, where its path is not one seen before
EMBED_INLINE void cli_view(const CatalogJobsCtx *args)
{
    const char *root = args->root;
    CliCatalog *catalog = args->catalog;
    const char *path = args->path;
    unsigned int *seen = args->seen;
    for (unsigned int at = 0u; at < *seen; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_jobs->paths.path[at], .other = path, .cap = CLI_POOL_BYTES}))
        {
            return;
        }
    }
    if (*seen < s_jobs->paths.capacity)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_jobs->paths.path[*seen],
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}) + 1u});
        *seen += 1u;
    }
    char file[CLI_PATH_MAX_LEN];
    char stem[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = path, .relative_out = file);
    EMBED_CALL(cli_catalog_path_out_internal.stem, CliCatalogPathOutCtx, .path = path, .out = stem);
    const char *title = stem;
    while (cellul.starts(&(CatenaFinitaCfg){.src = title, .other = "build_", .cap = CLI_POOL_BYTES}))
    {
        title += 6;
    }
    EMBED_CALL(cli_catalog_path_internal.script_job, CliCatalogPathCtx, .catalog = catalog, .root = root,
               .group = "view", .path = path, .title = title, .opens = "views");
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = catalog, .key = "input", .type = "file",
               .required = 0);
    EMBED_CALL(cli_catalog_choice_table.words, CliCatalogChoiceCfg, .catalog = catalog);
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = catalog,
               .program = CLI_PROGRAM_PYTHON, .text = file, .tool_program = NULL, .system = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_VALUE, .a = "input",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_WORDS, .a = "arguments",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
}

// Every page builder: the viewers, and the builders beside the examples that write a page. A builder that takes a file
// as its first word gets one, and every one takes free words after it
EMBED_INLINE void cli_views(const CatalogJobsCtx *args)
{
    char examples[CLI_PATH_MAX_LEN];
    unsigned int positions = 0u;
    unsigned int seen = 0u;
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = args->root, .name = "examples",
               .path_out = examples);
    EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = examples, .folders = 1,
               .filter = NULL, .entries = &s_jobs->subjects);
    (void)EMBED_CALL(cli_catalog_max_internal.paths_capacity, CliCatalogMaxCtx, .paths = &s_jobs->runs,
                     .most = 1u + (2u * s_jobs->subjects.count));
    if (s_jobs->runs.capacity != 0u)
    {
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = args->root,
                   .name = "examples/00_blob_viz_tools", .path_out = s_jobs->runs.path[positions]);
        positions += 1u;
    }
    for (unsigned int at = 0u; (at < s_jobs->subjects.count) && ((positions + 2u) <= s_jobs->runs.capacity); at += 1u)
    {
        char subject[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = examples,
                   .name = s_jobs->subjects.name[at], .path_out = subject);
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = subject, .name = "maint",
                   .path_out = s_jobs->runs.path[positions]);
        memor.cpy(&(MemoriaCfg){.dst = s_jobs->runs.path[positions + 1u],
                                .src = subject,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = subject, .cap = CLI_POOL_BYTES}) + 1u});
        positions += 2u;
    }
    for (unsigned int at = 0u; at < positions; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = s_jobs->runs.path[at],
                   .folders = 0, .filter = cli_catalog_is_builder, .entries = &s_jobs->files);
        (void)EMBED_CALL(cli_catalog_max_internal.paths_capacity, CliCatalogMaxCtx, .paths = &s_jobs->paths,
                         .most = seen + s_jobs->files.count);
        for (unsigned int each = 0u; each < s_jobs->files.count; each += 1u)
        {
            EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = s_jobs->runs.path[at],
                       .name = s_jobs->files.name[each], .path_out = path);
            EMBED_CALL(cli_view, CatalogJobsCtx, .root = args->root, .catalog = args->catalog, .path = path,
                       .seen = &seen);
        }
        EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = s_jobs->runs.path[at],
                   .folders = 0, .filter = cli_catalog_is_figure, .entries = &s_jobs->files);
        (void)EMBED_CALL(cli_catalog_max_internal.paths_capacity, CliCatalogMaxCtx, .paths = &s_jobs->paths,
                         .most = seen + s_jobs->files.count);
        for (unsigned int each = 0u; each < s_jobs->files.count; each += 1u)
        {
            EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = s_jobs->runs.path[at],
                       .name = s_jobs->files.name[each], .path_out = path);
            EMBED_CALL(cli_view, CatalogJobsCtx, .root = args->root, .catalog = args->catalog, .path = path,
                       .seen = &seen);
        }
    }
}

// whether a folder is a stage's: its name two or more letters, a digit and then `_`
EMBED_INLINE int cli_is_stage(const CatalogJobsCtx *args)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) > 2u) && (args->name[0] >= '0') &&
           (args->name[0] <= '9') && (args->name[1] == '_');
}

// the job of the stage script `name` in the stage folder `path` of the subject folder `under`, its path listed after
// the `seen` listed before it
EMBED_INLINE void cli_stage_script(const CatalogJobsCtx *args)
{
    const char *root = args->root;
    CliCatalog *catalog = args->catalog;
    unsigned int *seen = args->seen;
    char script[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char title[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = args->path, .name = args->name,
               .path_out = script);
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = script, .relative_out = file);
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->under, .path = script,
               .relative_out = title);
    if (*seen < s_jobs->paths.capacity)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_jobs->paths.path[*seen],
                                .src = file,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_POOL_BYTES}) + 1u});
        *seen += 1u;
    }
    EMBED_CALL(cli_catalog_path_internal.script_job, CliCatalogPathCtx, .catalog = catalog, .root = root,
               .group = "stage", .path = script, .title = title, .opens = "");
    EMBED_CALL(cli_catalog_choice_table.words, CliCatalogChoiceCfg, .catalog = catalog);
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = catalog,
               .program = CLI_PROGRAM_PYTHON, .text = file, .tool_program = NULL, .system = 0);
    EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_WORDS, .a = "arguments",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
}

// Each subject's stage scripts, one job a script, and one pipeline a subject that runs its stages in order and stops
// at the first that fails
EMBED_INLINE void cli_stages(const CatalogJobsCtx *args)
{
    const char *root = args->root;
    CliCatalog *catalog = args->catalog;
    char examples[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = root, .name = "examples",
               .path_out = examples);
    EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = examples, .folders = 1,
               .filter = NULL, .entries = &s_jobs->subjects);
    for (unsigned int at = 0u; at < s_jobs->subjects.count; at += 1u)
    {
        char subject[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = examples,
                   .name = s_jobs->subjects.name[at], .path_out = subject);
        EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = subject, .folders = 1,
                   .filter = NULL, .entries = &s_jobs->inner);
        unsigned int every = 0u;
        for (unsigned int stage = 0u; stage < s_jobs->inner.count; stage += 1u)
        {
            if (!EMBED_CALL(cli_is_stage, CatalogJobsCtx, .name = s_jobs->inner.name[stage]))
            {
                continue;
            }
            char folder[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = subject,
                       .name = s_jobs->inner.name[stage], .path_out = folder);
            EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = folder, .folders = 0,
                       .filter = cli_catalog_is_py, .entries = &s_jobs->files);
            (void)EMBED_CALL(cli_catalog_max_internal.paths_capacity, CliCatalogMaxCtx, .paths = &s_jobs->paths,
                             .most = every + s_jobs->files.count);
            for (unsigned int each = 0u; each < s_jobs->files.count; each += 1u)
            {
                EMBED_CALL(cli_stage_script, CatalogJobsCtx, .root = root, .catalog = catalog, .path = folder,
                           .under = subject, .name = s_jobs->files.name[each], .seen = &every);
            }
        }
        if (every == 0u)
        {
            continue;
        }
        char name[CLI_NAME_MAX_LEN];
        char id[CLI_PATH_MAX_LEN];
        char file[CLI_PATH_MAX_LEN];
        char readme[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_catalog_path_out_internal.stem, CliCatalogPathOutCtx, .path = subject, .out = name);
        VerbaLinea line = {id, sizeof(id), 0ULL};
        mmgr_line_put(&line, "pipeline/");
        mmgr_line_put(&line, name);
        mmgr_line_finish(&line);
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = subject,
                   .relative_out = file);
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = subject,
                   .name = "README.md", .path_out = readme);
        char *const about =
            EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = readme)
                ? EMBED_CALL(cli_catalog_path_out_internal.markdown_alloc, CliCatalogPathOutCtx, .path = readme)
                : NULL;
        EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = catalog, .id = id, .group = "pipeline",
                   .title = name, .file = file, .about = (about != NULL) ? about : "", .opens = "");
        g_cli_guard->persistent_buf_release(about, g_cli_catalog_owner);
        for (unsigned int each = 0u; each < every; each += 1u)
        {
            EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = catalog,
                       .program = CLI_PROGRAM_PYTHON, .text = s_jobs->paths.path[each], .tool_program = NULL,
                       .system = 0);
            EMBED_CALL(cli_catalog_type.arg, CliCatalogTypeCfg, .catalog = catalog, .type_u32 = CLI_ARG_WORDS,
                       .a = "arguments", .b = NULL, .c = NULL, .d = NULL, .separated = 0);
        }
    }
}

// every file named `name` under `under`, here and in its folders depth first, put into the list from `count`: the
// count after
static unsigned int cli_collect(const CatalogJobsCtx *args)
{
    const char *name = args->name;
    unsigned int count = args->count;
    char here[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = args->under, .name = name,
               .path_out = here);
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = here) &&
        EMBED_CALL(cli_catalog_max_internal.paths_capacity, CliCatalogMaxCtx, .paths = &s_jobs->paths,
                   .most = count + 1u))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_jobs->paths.path[count],
                                .src = here,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = here, .cap = CLI_POOL_BYTES}) + 1u});
        count += 1u;
    }
    if (args->depth >= 16u)
    {
        return count;
    }
    // the folders read before going down, so a deeper read takes another work state
    EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = args->under, .folders = 1,
               .filter = NULL, .entries = &s_jobs->dirs[args->depth]);
    for (unsigned int at = 0u; at < s_jobs->dirs[args->depth].count; at += 1u)
    {
        char dir[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = args->under,
                   .name = s_jobs->dirs[args->depth].name[at], .path_out = dir);
        count = EMBED_CALL(cli_collect, CatalogJobsCtx, .under = dir, .name = name, .count = count,
                           .depth = args->depth + 1u);
    }
    g_cli_guard->persistent_buf_release((&s_jobs->dirs[args->depth])->name, g_cli_catalog_owner);
    (&s_jobs->dirs[args->depth])->name = NULL;
    (&s_jobs->dirs[args->depth])->count = 0u;
    (&s_jobs->dirs[args->depth])->capacity = 0u;
    return count;
}

// The test suites: every run.sh under utils/test and under an example's test folder
EMBED_INLINE void cli_tests(const CatalogJobsCtx *args)
{
    const char *root = args->root;
    CliCatalog *catalog = args->catalog;
    char under[CLI_PATH_MAX_LEN];
    char examples[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = root, .name = "utils/test",
               .path_out = under);
    unsigned int count =
        EMBED_CALL(cli_collect, CatalogJobsCtx, .under = under, .name = "run.sh", .count = 0u, .depth = 0u);
    EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = root, .name = "examples",
               .path_out = examples);
    EMBED_CALL(cli_catalog_folder_internal.entries, CliCatalogFolderCtx, .folder = examples, .folders = 1,
               .filter = NULL, .entries = &s_jobs->subjects);
    for (unsigned int at = 0u; at < s_jobs->subjects.count; at += 1u)
    {
        char subject[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = examples,
                   .name = s_jobs->subjects.name[at], .path_out = subject);
        EMBED_CALL(cli_catalog_folder_name_internal.join, CliCatalogFolderNameCtx, .folder = subject, .name = "test",
                   .path_out = under);
        count = EMBED_CALL(cli_collect, CatalogJobsCtx, .under = under, .name = "run.sh", .count = count, .depth = 0u);
    }
    EMBED_CALL(cli_text_paths.paths_sort, CliTextPathsCfg, .paths = s_jobs->paths.path, .count = count);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_catalog_path_internal.bash_alone, CliCatalogPathCtx, .catalog = catalog, .root = root,
                   .group = "test", .path = s_jobs->paths.path[at]);
    }
}

void cli_catalog_read(const CliCatalogRootCfg *args)
{
    s_jobs = g_cli_guard->temporary_buf_alloc(sizeof(*s_jobs), g_cli_catalog_owner);
    if (s_jobs == NULL)
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = s_jobs, .val = 0, .bytes = sizeof(*s_jobs)});
    EMBED_CALL(cli_catalog_path_internal.builds, CliCatalogPathCtx, .root = args->root, .catalog = args->catalog);
    cli_catalog_root.executables_jobs(args);
    cli_catalog_root.deploy_jobs(args);
    cli_catalog_root.query_jobs(args);
    EMBED_CALL(cli_driver, CatalogJobsCtx, .root = args->root, .catalog = args->catalog);
    EMBED_CALL(cli_runners, CatalogJobsCtx, .root = args->root, .catalog = args->catalog);
    EMBED_CALL(cli_catalog_path_internal.sims, CliCatalogPathCtx, .root = args->root, .catalog = args->catalog);
    EMBED_CALL(cli_views, CatalogJobsCtx, .root = args->root, .catalog = args->catalog);
    EMBED_CALL(cli_stages, CatalogJobsCtx, .root = args->root, .catalog = args->catalog);
    EMBED_CALL(cli_tests, CatalogJobsCtx, .root = args->root, .catalog = args->catalog);
    g_cli_guard->persistent_buf_release(s_jobs->runs.path, g_cli_catalog_owner);
    s_jobs->runs.path = NULL;
    s_jobs->runs.capacity = 0u;
    g_cli_guard->persistent_buf_release(s_jobs->parts.name, g_cli_catalog_owner);
    s_jobs->parts.name = NULL;
    s_jobs->parts.count = 0u;
    s_jobs->parts.capacity = 0u;
    g_cli_guard->persistent_buf_release(s_jobs->paths.path, g_cli_catalog_owner);
    s_jobs->paths.path = NULL;
    s_jobs->paths.capacity = 0u;
    g_cli_guard->persistent_buf_release(s_jobs->subjects.name, g_cli_catalog_owner);
    s_jobs->subjects.name = NULL;
    s_jobs->subjects.count = 0u;
    s_jobs->subjects.capacity = 0u;
    g_cli_guard->persistent_buf_release(s_jobs->inner.name, g_cli_catalog_owner);
    s_jobs->inner.name = NULL;
    s_jobs->inner.count = 0u;
    s_jobs->inner.capacity = 0u;
    g_cli_guard->persistent_buf_release(s_jobs->files.name, g_cli_catalog_owner);
    s_jobs->files.name = NULL;
    s_jobs->files.count = 0u;
    s_jobs->files.capacity = 0u;
    for (unsigned int depth = 0u; depth < 17u; depth += 1u)
    {
        g_cli_guard->persistent_buf_release((&s_jobs->dirs[depth])->name, g_cli_catalog_owner);
        (&s_jobs->dirs[depth])->name = NULL;
        (&s_jobs->dirs[depth])->count = 0u;
        (&s_jobs->dirs[depth])->capacity = 0u;
    }
    g_cli_guard->persistent_buf_release(s_jobs, g_cli_catalog_owner);
    s_jobs = NULL;
}
