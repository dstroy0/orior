// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_unity.h: what impero's Unity suites share: the memory pool started once, a clean folder for a test, and JSON parsed
// for a test to read (cli_unity.c)
#ifndef CLI_UNITY_H
#define CLI_UNITY_H

#include "ui/cli/src/builds/builds.h"
#include "ui/cli/src/command_line/cli_internal.h"
#include "ui/cli/src/text/json_tree.h"
#include "unity.h"

// the size of a NUL-terminated string, no longer than the pool
#define CLI_UNITY_LENGTH(text_) cellul.len(&(CatenaFinitaCfg){.src = (text_), .cap = CLI_POOL_BYTES})
// `size_` bytes at `text_` are exactly the string `requested_`
#define CLI_TEST_ASSERT_TEXT(requested_, text_, size_)                                                                          \
    do                                                                                                                       \
    {                                                                                                                        \
        TEST_ASSERT_EQUAL_UINT64(CLI_UNITY_LENGTH(requested_), (size_));                                                        \
        if ((size_) > 0ull)                                                                                                  \
        {                                                                                                                    \
            TEST_ASSERT_EQUAL_MEMORY((requested_), (text_), (size_));                                                           \
        }                                                                                                                    \
    } while (0)
// the string `text_` contains the string `part_`
#define CLI_TEST_ASSERT_CONTAINS(part_, text_)                                                                               \
    TEST_ASSERT_NOT_NULL(cellul.find(&(CatenaFinitaCfg){.src = (text_), .cap = CLI_POOL_BYTES, .other = (part_), .other_cap = CLI_POOL_BYTES}))
// the string `text_` does not contain the string `part_`
#define CLI_TEST_ASSERT_NOT_CONTAINS(part_, text_)                                                                           \
    TEST_ASSERT_NULL(cellul.find(&(CatenaFinitaCfg){.src = (text_), .cap = CLI_POOL_BYTES, .other = (part_), .other_cap = CLI_POOL_BYTES}))

// the memory pool started, once for the process: every suite's setUp calls it
void cli_unity_begun(void);
// the folder `name` under the working directory, deleted and created empty; its full path into `full`, CLI_PATH_MAX_LEN bytes
void cli_unity_folder(const char *name, char *full);
// the file `name` under the folder `root` written with the string `text`
void cli_unity_file(const char *root, const char *name, const char *text);
// git run in `root` with `count` words, at max 6, the test failed unless it exits with 0
void cli_unity_git(const char *root, const char *const *words, unsigned int count);
// `size` bytes of JSON parsed into a tree shared by the suite, replacing the last one: the tree, its root in `*root`,
// CLI_JSON_NONE where the text is not JSON
const CliJson *cli_unity_parsed(const char *text, unsigned long long size, unsigned int *root);
// that tree's memory released, for a test that needs no cell allocated
void cli_unity_parsed_released(void);
// a string member of a node of that tree, the empty string where it has none
const char *cli_unity_member_text(unsigned int node, const char *name);
// the first item of the findings array `found` of that tree with the code `code` and a message containing `part`, either NULL for
// any: its node, CLI_JSON_NONE where none has them
unsigned int cli_unity_finding(unsigned int found, const char *code, const char *part);
// whether the completions contain the label `label`
int cli_unity_completes(const CliCompletions *items, const char *label);
// what completes the build file at `path` of the tree at `root` at the end of its text `text`
const CliCompletions *cli_unity_build_completed(const char *root, const char *path, const char *text);
// the address of the server `name`: 127.0.0.1 and `port`, or the variable ORIOR_<name>_ADDRESS read as host:port where it is set;
// `host` CLI_NAME_MAX_LEN bytes
void cli_unity_server(const char *name, unsigned int port, char *host, unsigned int *at);
#endif
