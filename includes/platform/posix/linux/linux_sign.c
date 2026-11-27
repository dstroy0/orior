// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// linux_sign.c: the programs a build makes on Linux, which nothing signs: no signing step, and nothing that a signature
// needs
#include "../../../../ui/cli/src/programs/executables_internal.h"

#if defined(__linux__)

int cli_executables_signs(void)
{
    return 0;
}

int cli_executables_sign_steps(const CliExecutablesErrMsgCfg *args)
{
    (void)args;
    return 0;
}

void cli_executables_signature_print(const CliExecutablesCfg *args)
{
    (void)args;
}
#endif
