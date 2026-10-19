#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# run.sh [--host] circuit.qasm: builds qasm_bitstring and the tessera daemon into build/<stamp>_qasm and runs it.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP="$(cd "$HERE/../.." && pwd)"
if [ "$#" -eq 0 ]; then
    echo "  usage: run.sh [--host] circuit.qasm"
    exit 2
fi
source "$HERE/build.sh"
qasm_build qasm || exit 1
qasm_link qasm_bitstring "$QASM_CU/qasm_bitstring.cu" || exit 1

"$BINARY" "$@"
STATUS=$?
echo "  qasm_bitstring exit $STATUS"
exit "$STATUS"
