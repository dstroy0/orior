// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_registry.c: off Windows there is no registry: the PATH is the one orior started with, and a folder added to the
// reader's PATH goes into their shell's profile instead
#include "../command_line/cli_internal.h"

#if !defined(_WIN32)
int cli_registry_machine_path(char *out, unsigned long long room)
{
    (void)out;
    (void)room;
    return 0;
}

int cli_registry_user_value(const char *name, char *out, unsigned long long room)
{
    (void)name;
    (void)out;
    (void)room;
    return 0;
}

int cli_registry_set_user_value(const char *name, const char *value, CliLine *said)
{
    (void)value;
    cli_line_text(said, name);
    cli_line_text(said, " is kept in the registry, which only Windows has");
    return 0;
}

void cli_registry_announce(void)
{
}

void cli_registry_expand(const char *text, char *out, unsigned long long room)
{
    memor.cpy(&(MemoriaCfg){
        .dst = out, .src = text, .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1u});
}
#endif
