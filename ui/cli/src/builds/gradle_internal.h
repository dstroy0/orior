// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gradle_internal.h: Gradle's scripts, in Groovy or Kotlin, as their reader reads them: each byte code, a string's or
// a comment's; their blocks read by their braces, each the name before its brace; each `dependencies` block's lines by
// the configuration each names and the dependency it gives; the configurations a script declares; and the catalogs of
// the build's `gradle` folder (gradle.c, gradle_build.c, gradle_check.c, gradle_references.c, gradle_complete.c)
#ifndef GRADLE_INTERNAL_H
#define GRADLE_INTERNAL_H
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "builds_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliGradleTextCtx CliGradleTextCtx;
typedef struct CliGradleTextSizeCtx CliGradleTextSizeCtx;
typedef struct CliGradleLinesCtx CliGradleLinesCtx;
typedef struct CliGradleSizeCtx CliGradleSizeCtx;
typedef struct CliGradleRootCtx CliGradleRootCtx;
typedef struct CliGradleTextSizeClassOfCtx CliGradleTextSizeClassOfCtx;
typedef struct CliGradleCtx CliGradleCtx;
typedef struct CliGradleFolderCtx CliGradleFolderCtx;
typedef struct CliGradleSizeWordCtx CliGradleSizeWordCtx;

// what a byte of a script is
#define CLI_GRADLE_CODE 0u
#define CLI_GRADLE_STRING 1u
#define CLI_GRADLE_COMMENT 2u

// the max bytes of a script read, the max blocks and stray braces of it, the max configurations it declares, and
// no block
#define CLI_GRADLE_TEXT (1u << 24u)
#define CLI_GRADLE_BLOCKS 65536u
#define CLI_GRADLE_DECLARED 4096u
#define CLI_GRADLE_NONE 0xffffffffull

// A block of a script: the name before its brace, and where it opens and closes, CLI_GRADLE_NONE where it does not
typedef struct
{
    unsigned long long name;
    unsigned long long name_size;
    unsigned long long open;
    unsigned long long close;
} CliGradleBlock;

// A script as read, under the reader's lock: its text, each byte's class, its blocks in the order they open, the
// braces that close none, and the configurations it declares, each a text of the script. The classes, the blocks and
// the braces are cells counted to the reader, taken at the sizes the script gives as it is read and given back by
// cli_gradle_release
typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned char *class_of;
    CliGradleBlock *block;
    unsigned int blocks;
    unsigned long long *stray;
    unsigned int strays;
    const char *declared[CLI_GRADLE_DECLARED];
    unsigned long long declared_size[CLI_GRADLE_DECLARED];
    unsigned int declares;
} CliGradleScript;

extern CliGradleScript g_cli_gradle;

// the configurations Gradle's own plugins and the common ones of others declare, what a `dependencies` block calls
// beside them, and the ends of the configurations a source set or a build variant declares (gradle.c)
extern const char *const g_cli_gradle_configurations[30];
extern const char *const g_cli_gradle_helpers[12];
extern const char *const g_cli_gradle_suffixes[8];

// each byte of the first `size` bytes of `text` classed; the script read: its classes, its blocks and its stray
// braces, each a cell at the size the script gives, 1, or 0, nothing allocated, where it is larger than CLI_GRADLE_TEXT
// or the cellblock cannot meet its cells; and the script read given back, its cells with it (gradle.c)
void cli_gradle_classes(const CliGradleTextSizeClassOfCtx *args);
int cli_gradle_read(const CliGradleTextSizeCtx *args);
void cli_gradle_release(void);

// the words a block of `size` bytes of `name` takes, into `count`
const char *const *cli_gradle_block_words(const CliGradleSizeCtx *args);

// the innermost block that contains byte `at`, CLI_GRADLE_BLOCKS where none does
unsigned int cli_gradle_block_at(const CliGradleTextCtx *args);

// whether a block's name is `name`
int cli_gradle_block_by_name(const CliGradleCtx *args);

// each statement the block at `block` contains itself, not those of the blocks inside it, before `end`: its first word,
// where the word starts, and what follows it on its line, passed to `each`
typedef void (*CliGradleStatement)(const CliGradleStatementCfg *args);
void cli_gradle_statements(const CliGradleLinesCtx *args);

// the configurations the script declares, by name in a `configurations` block, by `create`, `register` or
// `maybeCreate`, and by Kotlin's `by configurations.creating`, stored in the script read
void cli_gradle_declare(const CliGradleLinesCtx *args);

// whether `size` bytes of `word` are a configuration Gradle or the script declares
int cli_gradle_known_configuration(const CliGradleSizeWordCtx *args);

// the folder of the Gradle build the file at `path` belongs to: the nearest above it, in the tree, that contains a
// settings script or a wrapper, a catalog's build being the one above its `gradle` folder; the file's own folder where
// none does; into `out`, sized for CLI_PATH_MAX_LEN
void cli_gradle_build_root(const CliGradleRootCtx *args);

// the max catalogs of a build's `gradle` folder
#define CLI_GRADLE_CATALOGS 64u

// The catalogs of a build: each one's accessor, its name before `.versions.toml`, and its path, sorted
typedef struct
{
    char name[CLI_GRADLE_CATALOGS][CLI_NAME_MAX_LEN];
    char path[CLI_GRADLE_CATALOGS][CLI_PATH_MAX_LEN];
    unsigned int count;
} CliGradleCatalogs;

// the catalogs of the build at `folder` into `catalogs`
void cli_gradle_catalogs(const CliGradleFolderCtx *args);

// the findings of the script at `path` of the tree at `root`, `size` bytes of its text (gradle_check.c)
void cli_gradle_check(const CliGradleTextSizeCtx *args);

// what completes the word before byte `at` of the script at `path`: a catalog's aliases after its accessor, and at the
// head of a statement, what the block it is in takes (gradle_complete.c)
void cli_gradle_complete(const CliGradleTextCtx *args);

struct CliGradleTextCtx
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
    const char *root;
    const char *path;
    CliCompletions *items;
};

struct CliGradleTextSizeCtx
{
    const char *text;
    unsigned long long size;
    const CliBuildLines *lines;
    const char *root;
    const char *path;
    CliBuildFindings *found;
};

struct CliGradleLinesCtx
{
    const CliBuildLines *lines;
    unsigned int block_u32;
    unsigned long long end;
    CliGradleStatement each;
    void *context;
};

struct CliGradleSizeCtx
{
    unsigned long long size;
    const char *name;
    unsigned int *count;
};

struct CliGradleRootCtx
{
    const char *root;
    const char *path;
    char *out;
};

struct CliGradleTextSizeClassOfCtx
{
    const char *text;
    unsigned long long size;
    unsigned char *class_of;
};

struct CliGradleCtx
{
    const char *name;
    const CliGradleBlock *block;
};

struct CliGradleFolderCtx
{
    const char *folder;
    CliGradleCatalogs *catalogs;
};

struct CliGradleSizeWordCtx
{
    unsigned long long size;
    const char *word_text;
};

typedef struct
{
    void (*classes)(const CliGradleTextSizeClassOfCtx *args);
} CliGradleTextSizeClassOfNs;
EMBED_TABLE_LAYOUT(CliGradleTextSizeClassOfNs, classes);
EMBED_TABLE_STORAGE CliGradleTextSizeClassOfNs cli_gradle_text_size_class_of_internal EMBED_UNUSED = {
    .classes = cli_gradle_classes,
};

typedef struct
{
    int (*read)(const CliGradleTextSizeCtx *args);
    void (*check)(const CliGradleTextSizeCtx *args);
} CliGradleTextSizeNs;
EMBED_TABLE_LAYOUT(CliGradleTextSizeNs, read, check);
EMBED_TABLE_STORAGE CliGradleTextSizeNs cli_gradle_text_size_internal EMBED_UNUSED = {
    .read = cli_gradle_read,
    .check = cli_gradle_check,
};

typedef struct
{
    const char *const *(*block_words)(const CliGradleSizeCtx *args);
} CliGradleSizeNs;
EMBED_TABLE_LAYOUT(CliGradleSizeNs, block_words);
EMBED_TABLE_STORAGE CliGradleSizeNs cli_gradle_size_internal EMBED_UNUSED = {
    .block_words = cli_gradle_block_words,
};

typedef struct
{
    unsigned int (*block_at)(const CliGradleTextCtx *args);
    void (*complete)(const CliGradleTextCtx *args);
} CliGradleTextNs;
EMBED_TABLE_LAYOUT(CliGradleTextNs, block_at, complete);
EMBED_TABLE_STORAGE CliGradleTextNs cli_gradle_text_internal EMBED_UNUSED = {
    .block_at = cli_gradle_block_at,
    .complete = cli_gradle_complete,
};

typedef struct
{
    int (*block_by_name)(const CliGradleCtx *args);
} CliGradleNs;
EMBED_TABLE_LAYOUT(CliGradleNs, block_by_name);
EMBED_TABLE_STORAGE CliGradleNs cli_gradle_internal EMBED_UNUSED = {
    .block_by_name = cli_gradle_block_by_name,
};

typedef struct
{
    void (*statements)(const CliGradleLinesCtx *args);
    void (*declare)(const CliGradleLinesCtx *args);
} CliGradleLinesNs;
EMBED_TABLE_LAYOUT(CliGradleLinesNs, statements, declare);
EMBED_TABLE_STORAGE CliGradleLinesNs cli_gradle_lines_internal EMBED_UNUSED = {
    .statements = cli_gradle_statements,
    .declare = cli_gradle_declare,
};

typedef struct
{
    int (*known_configuration)(const CliGradleSizeWordCtx *args);
} CliGradleSizeWordNs;
EMBED_TABLE_LAYOUT(CliGradleSizeWordNs, known_configuration);
EMBED_TABLE_STORAGE CliGradleSizeWordNs cli_gradle_size_word_internal EMBED_UNUSED = {
    .known_configuration = cli_gradle_known_configuration,
};

typedef struct
{
    void (*build_root)(const CliGradleRootCtx *args);
} CliGradleRootNs;
EMBED_TABLE_LAYOUT(CliGradleRootNs, build_root);
EMBED_TABLE_STORAGE CliGradleRootNs cli_gradle_root_internal EMBED_UNUSED = {
    .build_root = cli_gradle_build_root,
};

typedef struct
{
    void (*catalogs)(const CliGradleFolderCtx *args);
} CliGradleFolderNs;
EMBED_TABLE_LAYOUT(CliGradleFolderNs, catalogs);
EMBED_TABLE_STORAGE CliGradleFolderNs cli_gradle_folder_internal EMBED_UNUSED = {
    .catalogs = cli_gradle_catalogs,
};

#endif
