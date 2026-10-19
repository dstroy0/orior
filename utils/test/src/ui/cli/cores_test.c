// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cores_test.c: the cores' check: at least one core is kept, and at least one is worked on
#include "../../../../../ui/cli/src/machine/cores.h"
#include "cli_check.h"

// `count` less `less`, one at the least
static unsigned int cores_left(unsigned int count, unsigned int less)
{
    return (count > less + 1u) ? count - less : 1u;
}

int main(void)
{
    const unsigned int machine = cli_cores_machine();
    cli_cores_keep(0u);
    cli_check(cli_cores_kept() == 1u, "none asked for keeps one");
    cli_check(cli_cores_for_work() == cores_left(machine, 1u), "keeping one leaves the rest to work on");
    cli_cores_keep(machine + 4u);
    cli_check(cli_cores_for_work() == 1u, "keeping more than the machine has leaves one to work on");
    cli_cores_keep(2u);
    cli_check(cli_cores_for_work() == cores_left(machine, 2u), "keeping two leaves the rest to work on");
    cli_cores_keep(CLI_CORES_KEPT_LEAST);
    return cli_checked();
}
