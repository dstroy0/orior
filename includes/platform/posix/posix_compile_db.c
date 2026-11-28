// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_compile_db.c: Visual Studio's environment on Linux and macOS, where Visual Studio does not run
#include "../../../ui/cli/src/builds/compile_db_internal.h"

#if !defined(_WIN32)

const CliVsEnvironment *cli_compile_db_vs_environment(void)
{
    return NULL;
}
#endif
