// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cores_test.c: the cores' check: at least one core is reserved, and at least one is worked on
#include "../../../../../ui/cli/src/machine/cores.h"
#include "cli_check.h"

// `count` less `less`, one at the least
static unsigned int cores_left(unsigned int count, unsigned int less)
{
    return (count > less + 1u) ? count - less : 1u;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    const unsigned int machine = cli_cores_machine();
    cli_cores_reserve(0u);
    cli_check(cli_cores_reserved() == 1u, "none requested reserves one");
    cli_check(cli_cores_for_work() == cores_left(machine, 1u), "reserving one leaves the rest to work on");
    cli_cores_reserve(machine + 4u);
    cli_check(cli_cores_for_work() == 1u, "reserving more than the machine has leaves one to work on");
    cli_cores_reserve(2u);
    cli_check(cli_cores_for_work() == cores_left(machine, 2u), "reserving two leaves the rest to work on");
    cli_cores_reserve(CLI_CORES_RESERVED_MIN);
    return cli_checked();
}
