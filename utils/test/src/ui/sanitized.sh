# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# sanitized.sh: sourced by cli_test.sh and gui_test.sh. ORIOR_SANITIZE=address or thread compiles and links each check
# under that sanitizer against the library ui/cli/build.sh builds under it, in a run folder of that sanitizer's own.
# A report does not halt its check: each is written to sanitizer.<pid> in the run folder, its summary lines said, and
# a check with a report exits 1. MMgr's cells are carved from one region, and a sanitizer sees that region as one
# allocation: a write past a cell inside it is not a report.
SANITIZE="${ORIOR_SANITIZE:-}"
SANITIZE_WORDS=()
SANITIZE_SUFFIX=""
if [ -n "$SANITIZE" ]; then
    SANITIZE_SUFFIX="-$SANITIZE"
    SANITIZE_WORDS=("-fsanitize=$SANITIZE" "-fsanitize-recover=$SANITIZE" -fno-omit-frame-pointer)
    # the programs the library's build runs, from the top of the tree, report under build/; the checks report in their run
    # folder from sanitized_checks_begin on
    ASAN_OPTIONS_GIVEN="${ASAN_OPTIONS:-}"
    TSAN_OPTIONS_GIVEN="${TSAN_OPTIONS:-}"
    export ASAN_OPTIONS="halt_on_error=0:log_path=build/sanitizer_build${ASAN_OPTIONS_GIVEN:+:$ASAN_OPTIONS_GIVEN}"
    export TSAN_OPTIONS="halt_on_error=0:log_path=build/sanitizer_build${TSAN_OPTIONS_GIVEN:+:$TSAN_OPTIONS_GIVEN}"
    # on Windows a check names the C runtime's DLL, as the library does, and the address sanitizer's runtime is a
    # library beside the compiler's, found through the PATH
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            SANITIZE_WORDS+=(-fms-runtime-lib=dll -D_DLL)
            RUNTIME="$(dirname "$("$CC" -print-file-name=clang_rt.asan_dynamic-x86_64.dll)")"
            export PATH="$RUNTIME:$PATH" ;;
    esac
fi

# the library built, every report from here on written where each check runs
sanitized_checks_begin() {
    [ -n "$SANITIZE" ] || return 0
    export ASAN_OPTIONS="halt_on_error=0:log_path=sanitizer${ASAN_OPTIONS_GIVEN:+:$ASAN_OPTIONS_GIVEN}"
    export TSAN_OPTIONS="halt_on_error=0:log_path=sanitizer${TSAN_OPTIONS_GIVEN:+:$TSAN_OPTIONS_GIVEN}"
}

# the exit of the check `name` that just ran in `run`, 1 where the sanitizer wrote a report: the report kept as
# <name>.sanitizer.<pid> and its summary lines said
sanitized_exit() {
    local name="$1" run="$2" code="$3" report
    for report in "$run"/sanitizer.*; do
        [ -f "$report" ] || continue
        grep -h "^SUMMARY:" "$report" | sort | uniq -c | sed 's/^/  sanitizer /'
        mv -f "$report" "$run/$name.$(basename "$report")"
        code=1
    done
    return "$code"
}
