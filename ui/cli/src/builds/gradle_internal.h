// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gradle_internal.h: Gradle's scripts, in Groovy or Kotlin, as their reader reads them: each byte code, a string's or
// a comment's; their blocks read by their braces, each the name before its brace; each `dependencies` block's lines by
// the configuration each names and the dependency it gives; the configurations a script declares; and the catalogs of
// the build's `gradle` folder (gradle.c, gradle_build.c, gradle_check.c, gradle_references.c, gradle_complete.c)
#ifndef GRADLE_INTERNAL_H
#define GRADLE_INTERNAL_H

#include "../text/text.h"
#include "builds_internal.h"

// what a byte of a script is
#define CLI_GRADLE_CODE 0u
#define CLI_GRADLE_STRING 1u
#define CLI_GRADLE_COMMENT 2u

// the most bytes of a script read, the most blocks and stray braces of it, the most configurations it declares, and
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
// cli_gradle_released
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
extern CliLock g_cli_gradle_lock;

// what the scripts read hold in the cellblock (gradle.c)
extern CliCellsTally g_cli_gradle_cells;

// the configurations Gradle's own plugins and the common ones of others declare, what a `dependencies` block calls
// beside them, and the ends of the configurations a source set or a build variant declares (gradle.c)
extern const char *const g_cli_gradle_configurations[30];
extern const char *const g_cli_gradle_helpers[12];
extern const char *const g_cli_gradle_suffixes[8];

// each byte of the first `size` bytes of `text` classed; the script read: its classes, its blocks and its stray
// braces, each a cell at the size the script gives, 1, or 0, nothing held, where it is larger than CLI_GRADLE_TEXT or
// the cellblock cannot meet its cells; and the script read given back, its cells with it (gradle.c)
void cli_gradle_classes(const char *text, unsigned long long size, unsigned char *class_of);
int cli_gradle_read(const char *text, unsigned long long size);
void cli_gradle_released(void);

// the words a block of `size` bytes of `name` takes, into `count`
const char *const *cli_gradle_block_words(const char *name, unsigned long long size, unsigned int *count);

// the innermost block that holds byte `at`, CLI_GRADLE_BLOCKS where none does
unsigned int cli_gradle_block_at(unsigned long long at);

// whether a block's name is `name`
int cli_gradle_block_named(const CliGradleBlock *block, const char *name);

// each statement the block at `block` holds itself, not those of the blocks inside it, before `end`: its first word,
// where the word starts, and what follows it on its line, handed to `each`
typedef void (*CliGradleStatement)(void *context, unsigned long long word, unsigned long long word_size,
                                   unsigned long long rest, unsigned long long rest_end);
void cli_gradle_statements(const CliBuildLines *lines, unsigned int block, unsigned long long end,
                           CliGradleStatement each, void *context);

// the configurations the script declares, by name in a `configurations` block, by `create`, `register` or
// `maybeCreate`, and by Kotlin's `by configurations.creating`, kept in the script read
void cli_gradle_declared(const CliBuildLines *lines);

// whether `size` bytes of `word` are a configuration Gradle or the script declares
int cli_gradle_known_configuration(const char *word, unsigned long long size);

// the folder of the Gradle build the file at `path` belongs to: the nearest above it, in the tree, that holds a
// settings script or a wrapper, a catalog's build being the one above its `gradle` folder; the file's own folder where
// none does; into `out`, which holds CLI_PATH_LONGEST
void cli_gradle_build_root(const char *root, const char *path, char *out);

// the most catalogs of a build's `gradle` folder
#define CLI_GRADLE_CATALOGS 64u

// The catalogs of a build: each one's accessor, its name before `.versions.toml`, and its path, sorted
typedef struct
{
    char name[CLI_GRADLE_CATALOGS][CLI_NAME_LONGEST];
    char path[CLI_GRADLE_CATALOGS][CLI_PATH_LONGEST];
    unsigned int count;
} CliGradleCatalogs;

// the catalogs of the build at `folder` into `catalogs`
void cli_gradle_catalogs(const char *folder, CliGradleCatalogs *catalogs);

// the findings of the script at `path` of the tree at `root`, `size` bytes of its text (gradle_check.c)
void cli_gradle_check(const char *root, const char *path, const char *text, unsigned long long size,
                      const CliBuildLines *lines, CliBuildFindings *found);

// what completes the word before byte `at` of the script at `path`: a catalog's aliases after its accessor, and at the
// head of a statement, what the block it stands in takes (gradle_complete.c)
void cli_gradle_complete(const char *root, const char *path, const char *text, unsigned long long size,
                         unsigned long long at, CliCompletions *items);

#endif
