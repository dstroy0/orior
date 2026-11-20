# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Runs the miner, and starts it again one second after each exit. run_miner.ps1 starts this script where restart
# is true in miner.json. The first argument is the binary, and the rest go to the binary.

$exe = $args[0]
$rest = @($args | Select-Object -Skip 1)
while ($true) {
    & $exe @rest
    Start-Sleep -Seconds 1
}
