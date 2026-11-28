// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scripts.h: the script plugins. Each one's code runs in a Node process of its own, the host, which the window starts
// with Node's permission model as it starts a language server. An ask starts the host where it does not run, and the
// answer comes back. A quiet stores the state that the plugin keeps and stops the host. The next start hands that state
// back. The overseer holds each host to the resources of its plugin.json (scripts.c, host.js)
//
// plugin.json's `resources` gives `memoryMiB` of working set, `cpuPercent` of one processor and `askSeconds` for one
// ask. Each is optional, and CLI_SCRIPTS_MEMORY_MIB, CLI_SCRIPTS_CPU_PERCENT and CLI_SCRIPTS_ASK_SECONDS stand in for
// one that is absent
#ifndef SCRIPTS_H
#define SCRIPTS_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct CliScriptsCfg CliScriptsCfg;

// the most hosts that run at once and the resources of a plugin that names none. Then how long a host stays quiet
// before it stops, in milliseconds
#define CLI_SCRIPTS_HOSTS 8u
#define CLI_SCRIPTS_MEMORY_MIB 512ULL
#define CLI_SCRIPTS_CPU_PERCENT 100u
#define CLI_SCRIPTS_ASK_SECONDS 30ULL
#define CLI_SCRIPTS_QUIET_MS 30000ULL

// the values of an ask's `source`: a command that the plugin registered, an event that it listens to, and a message of
// its server
#define CLI_SCRIPTS_COMMAND 0u
#define CLI_SCRIPTS_EVENT 1u
#define CLI_SCRIPTS_SERVER 2u

// The rights of a plugin.json, in the order of the store. The plugin reads the tree's files, writes them, reaches
// hosts on the network, and keeps secrets in the system's keychain. It also asks orior to start programs. Then the
// reader's answer for one right. The window did not ask yet, the reader refused the right, or the reader allowed it
#define CLI_SCRIPTS_READS 0u
#define CLI_SCRIPTS_WRITES 1u
#define CLI_SCRIPTS_REACHES 2u
#define CLI_SCRIPTS_SECRETS 3u
#define CLI_SCRIPTS_STARTS 4u
#define CLI_SCRIPTS_RIGHTS 5u
#define CLI_SCRIPTS_UNASKED 0u
#define CLI_SCRIPTS_REFUSED 1u
#define CLI_SCRIPTS_ALLOWED 2u

    typedef struct CliScriptsRightsCfg CliScriptsRightsCfg;

    // whether the list of the right `right` in the plugin.json of `id` names `name`. For reads and writes, `name` is a
    // path from the tree's top with `/` between its names. For reaches, `name` is a host. For secrets, `name` is a key,
    // and an item `*` names every key (scripts_rights.c)
    int cli_scripts_rights_named(const CliScriptsRightsCfg *args);
    // the reader's answer for the right `right` of the plugin `id`, from the store in plugin_state/<id>.rights.json
    unsigned int cli_scripts_rights_answer(const CliScriptsRightsCfg *args);
    // the answer `answer` for the right `right` of the plugin `id` put in the store: 1, or 0 with why given
    int cli_scripts_rights_answer_set(const CliScriptsRightsCfg *args);

    struct CliScriptsRightsCfg
    {
        const char *id;
        unsigned int right;
        const char *name;
        unsigned int answer;
        VerbaLinea *err_msg;
    };

    typedef struct
    {
        int (*named)(const CliScriptsRightsCfg *args);
        unsigned int (*answer)(const CliScriptsRightsCfg *args);
        int (*answer_set)(const CliScriptsRightsCfg *args);
    } CliScriptsRightsNs;
    EMBED_TABLE_LAYOUT(CliScriptsRightsNs, named, answer, answer_set);
    EMBED_TABLE_STORAGE CliScriptsRightsNs cli_scripts_rights EMBED_UNUSED = {
        .named = cli_scripts_rights_named,
        .answer = cli_scripts_rights_answer,
        .answer_set = cli_scripts_rights_answer_set,
    };

    typedef struct CliScriptsTreeCfg CliScriptsTreeCfg;
    typedef struct CliScriptsQuestionCfg CliScriptsQuestionCfg;

    // the tree that a plugin's files calls read and write in, a copy of `root` kept, none where `root` is NULL
    // (scripts_calls.c)
    void cli_scripts_set_tree(const CliScriptsTreeCfg *args);
    // A call that needs a right that the reader did not answer yet waits, and the window asks the reader in a dialog.
    // The question of the first such call goes into `title` and `body`, once for each question: 1, or 0 where no
    // question waits or one shows now. Then the reader's answer `answer` to the question that shows. An allow or a
    // refusal goes into the store, and each call of that right gets its answer. CLI_SCRIPTS_UNASKED refuses those
    // calls and stores nothing
    int cli_scripts_question(const CliScriptsQuestionCfg *args);
    void cli_scripts_question_answered(const CliScriptsQuestionCfg *args);

    struct CliScriptsTreeCfg
    {
        const char *root;
    };

    struct CliScriptsQuestionCfg
    {
        VerbaLinea *title;
        VerbaLinea *body;
        unsigned int answer;
    };

    typedef struct
    {
        void (*set_tree)(const CliScriptsTreeCfg *args);
    } CliScriptsTreeNs;
    EMBED_TABLE_LAYOUT(CliScriptsTreeNs, set_tree);
    EMBED_TABLE_STORAGE CliScriptsTreeNs cli_scripts_tree EMBED_UNUSED = {
        .set_tree = cli_scripts_set_tree,
    };

    typedef struct
    {
        int (*question)(const CliScriptsQuestionCfg *args);
        void (*answered)(const CliScriptsQuestionCfg *args);
    } CliScriptsQuestionNs;
    EMBED_TABLE_LAYOUT(CliScriptsQuestionNs, question, answered);
    EMBED_TABLE_STORAGE CliScriptsQuestionNs cli_scripts_question_table EMBED_UNUSED = {
        .question = cli_scripts_question,
        .answered = cli_scripts_question_answered,
    };

    // the bytes of host.js, built into the program by bundle/plugins_bundle.c, and their count
    extern const unsigned char g_cli_scripts_host[];
    extern const unsigned long long g_cli_scripts_host_size;

    // The ask `name` of the `source` given to the plugin `id`, with `length` bytes of JSON `value`, "null" where it
    // has none. The host starts first where it does not run. The answer's JSON goes into `answer`. 1, or 0 with why
    // given
    int cli_scripts_ask(const CliScriptsCfg *args);
    // the host of the plugin `id` stopped where it runs, and the state that the plugin keeps stored first
    void cli_scripts_stop(const CliScriptsCfg *args);
    // whether the host of the plugin `id` runs
    int cli_scripts_running(const CliScriptsCfg *args);
    // each host quiet for CLI_SCRIPTS_QUIET_MS at `now_ms` stopped, its state stored first
    void cli_scripts_quiet(const CliScriptsCfg *args);
    // every host stopped as the program ends, each state stored first
    void cli_scripts_end(void);

    struct CliScriptsCfg
    {
        const char *id;
        unsigned int source;
        const char *name;
        const char *value;
        unsigned long long length;
        VerbaLinea *answer;
        VerbaLinea *err_msg;
        unsigned long long now_ms;
    };

    typedef struct
    {
        int (*ask)(const CliScriptsCfg *args);
        void (*stop)(const CliScriptsCfg *args);
        int (*running)(const CliScriptsCfg *args);
        void (*quiet)(const CliScriptsCfg *args);
    } CliScriptsNs;
    EMBED_TABLE_LAYOUT(CliScriptsNs, ask, stop, running, quiet);
    EMBED_TABLE_STORAGE CliScriptsNs cli_scripts EMBED_UNUSED = {
        .ask = cli_scripts_ask,
        .stop = cli_scripts_stop,
        .running = cli_scripts_running,
        .quiet = cli_scripts_quiet,
    };

#ifdef __cplusplus
}
#endif

#endif
