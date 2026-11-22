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
