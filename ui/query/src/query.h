// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query.h: the query: the run's lifecycle and every loop of the protocol, each a command of `orior protocol`. The
// writer, the carrier, the cross and the teacher are C programs the query builds when their sources are newer and
// runs as processes, the run a database they share; the query holds which program runs after which and with what
// words. Every program it needs comes by role through QueryPrograms, which the command line gives over its
// toolchains.
#ifndef QUERY_H
#define QUERY_H

#include "../../cli/src/command_line/file_writing.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// the longest path the query names, and the longest line it says
#define QUERY_PATH_LONGEST 1024u

#define QUERY_SAID_LONGEST 4096u

    // A program by what it is for
    typedef enum
    {
        // the C compiler the tree's C programs are built with: cc or gcc, or clang
        QUERY_ROLE_C,
        // the C++ compiler the code generator's .cu files are compiled with as C++
        QUERY_ROLE_CXX,
        // nvcc, the vendor's compiler, and nvdisasm and cuobjdump beside it
        QUERY_ROLE_NVCC,
        QUERY_ROLE_NVDISASM,
        QUERY_ROLE_CUOBJDUMP,
        // cl, the host compiler nvcc calls on Windows
        QUERY_ROLE_CL,
        // the tree's Python
        QUERY_ROLE_PYTHON,
        // nvidia-smi, which says whether the part is free
        QUERY_ROLE_NVIDIA_SMI,
        // docker, which runs the harness's images; curl, which fetches the vendor's archives; tar, which unpacks them
        QUERY_ROLE_DOCKER,
        QUERY_ROLE_CURL,
        QUERY_ROLE_TAR,
        QUERY_ROLE_COUNT
    } QueryRole;

    // Where the query's programs are found: the program of a role into `path`, which holds `room`, 1, or 0 with why
    // it is not found said into `said` in words that say what to do; and the PATH every program the query starts runs
    // with
    typedef struct
    {
        int (*program)(void *context, QueryRole role, char *path, size_t room, CliLine *said);
        const char *(*run_path)(void *context);
        void *context;
    } QueryPrograms;

    // what a line said goes to: `length` bytes of `text`, one line without its end
    typedef struct
    {
        void (*said)(void *context, const char *text, unsigned long long length);
        void *context;
    } QuerySink;

    // Whether a run's files are left as it leaves them: every one, none, or each asked
    typedef enum
    {
        QUERY_LEAVE_ALL,
        QUERY_LEAVE_NONE,
        QUERY_LEAVE_ASK
    } QueryLeave;

    // what is answered of one file: this one left or not, or every one from here on
    typedef enum
    {
        QUERY_ANSWER_YES,
        QUERY_ANSWER_NO,
        QUERY_ANSWER_ALL,
        QUERY_ANSWER_NONE
    } QueryAnswer;

    // whoever answers for a file: whether the file at `path` is left as the run leaves it, its difference shown
    typedef struct
    {
        QueryAnswer (*asked)(void *context, const char *path);
        void *context;
    } QueryAsker;

    // One input a command takes: a choice among `choices`, a word, or free words
    typedef struct
    {
        const char *key;
        const char *kind;
        const char *const *choices;
        unsigned int choice_count;
        int required;
    } QueryParam;

    // How a command's words are made from the values given, as the catalog makes a step's arguments: written as it
    // is (`first`); the value of the param `first` where one is given; the flag `first` and then the value of the
    // param `second` where one is given; the flag `first` before each value chosen of the param `second`; the words of
    // the param `first`, split as a shell splits them; or the word `third` where the param `first`'s value is `second`
    typedef enum
    {
        QUERY_WORD_LITERAL,
        QUERY_WORD_VALUE,
        QUERY_WORD_FLAG,
        QUERY_WORD_EACH,
        QUERY_WORD_WORDS,
        QUERY_WORD_WHEN
    } QueryWordKind;

    typedef struct
    {
        QueryWordKind kind;
        const char *first;
        const char *second;
        const char *third;
    } QueryWord;

    // One command of `orior protocol`
    typedef struct
    {
        const char *id;
        const char *title;
        const char *about;
        const QueryParam *params;
        unsigned int param_count;
        const QueryWord *words;
        unsigned int word_count;
    } QueryCommand;

    // the commands, each with its params and the words that run it, the languages and the parts the bridge's folder
    // under `root` holds whole offered as its choices: their count, the list at `commands`
    unsigned int query_commands(const char *root, const QueryCommand **commands);

    // the role as a person reads it in a message
    const char *query_role_name(QueryRole role);

    // the programs as the PATH alone gives them
    QueryPrograms query_on_path(void);

    // the person at the terminal
    QueryAsker query_terminal(void);

    // The words `words`, `count` of them, run under the tree at `root`, every program found through `programs`, each
    // line said through `sink`: the code to exit with, or -1 with why the words could not be run said through `sink`
    int query_run(const char *root, const QueryPrograms *programs, const char *const *words, unsigned int count,
                  QuerySink sink);

#ifdef __cplusplus
}
#endif

#endif
