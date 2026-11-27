# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# build_needs.sh: build.sh sources this file, with TOP, CC, AR and lld set. The function build_needs_met finds the
# programs, headers and submodules that the build needs. When the machine does not have one, it gives the developer
# a list and the command that installs each part. Then the build stops before it compiles a file.
# utils/test/window_host_probe.sh finds the package manager and the X11 headers. utils/test/window_packages.tsv gives
# the package for each program and header. utils/test/window_managers.tsv gives the install command of each manager.
# harness_build.py shows each line that starts with NEEDS_MARK. Thus the harness shows the full list.
NEEDS_MARK="  needs:"
NEEDS_PACKAGES="$TOP/utils/test/window_packages.tsv"
NEEDS_MANAGERS="$TOP/utils/test/window_managers.tsv"

# The cell of the table "$1" in the row that starts with "$2", in the column that the head line names "$3". A cell
# of "-" gives an empty line.
needs_cell() {
    awk -F'\t' -v key="$2" -v field="$3" '
        NR == 1 { for (i = 1; i <= NF; i++) if ($i == field) column = i; next }
        $1 == key && column && $column != "-" { print $column }' "$1"
}

# A zero exit when the program "$1" is on PATH or in the folder of the compiler. Clang finds lld-link in its own folder.
needs_found() {
    command -v "$1" > /dev/null 2>&1 && return 0
    local folder
    folder="$(dirname "$(command -v "$CC" 2> /dev/null || echo .)")"
    [[ -x "$folder/$1" ]] || [[ -x "$folder/$1.exe" ]]
}

# The check. "$1" is 1 for a build of the window, and "$2" is the target. A non-zero exit tells that the machine does
# not have a part.
build_needs_met() {
    local window="$1" target="$2"
    local names=() reasons=() submodules=0
    if [[ ! -f "$TOP/includes/mmgr/CMakeLists.txt" ]] || [[ ! -d "$TOP/includes/embedded_types/include" ]]; then
        submodules=1
    fi
    if ! needs_found "$CC"; then
        names+=("$(basename "$CC")"); reasons+=("the compiler of each source")
    fi
    if ! needs_found cmake; then
        names+=(cmake); reasons+=("the build of MMgr for the target")
    fi
    if ! needs_found ninja; then
        names+=(ninja); reasons+=("the build tool that CMake runs")
    fi
    if ! needs_found "$AR"; then
        names+=(ar); reasons+=("the archive of the objects that the bundler links")
    fi
    case "$target" in
        *windows*)
            if ! needs_found lld-link; then
                names+=(lld-link); reasons+=("the linker of a Windows target")
            fi
            if [[ "$window" = 1 ]] && ! needs_found llvm-rc; then
                names+=(llvm-rc); reasons+=("the compiler of the icon of the window")
            fi ;;
        *) ;;
    esac
    local probed manager
    probed="$(bash "$TOP/utils/test/window_host_probe.sh")"
    manager="$(printf '%s\n' "$probed" | awk -F'\t' '$1 == "manager" { print $2 }')"
    if [[ "$window" = 1 ]] && [[ "$(uname -s)" = Linux ]]; then
        local header
        while IFS= read -r header; do
            [[ -n "$header" ]] || continue
            names+=("$header"); reasons+=("the X11 body of the window")
        done < <(printf '%s\n' "$probed" | awk -F'\t' '$1 == "header" && $3 == "no" { print $2 }')
    fi

    # Without lld, clang links thin LTO through the system linker and the LTO plugin of clang. Not every system has
    # the plugin. Thus the check gives a note, and the build continues.
    if [[ "$(uname -s)" = Linux ]] && [[ "$CC" != *gcc* ]] && [[ -z "${lld:-}" ]]; then
        echo "$NEEDS_MARK This machine does not have ld.lld. The link uses the system linker and the LTO plugin of clang."
        echo "$NEEDS_MARK If the link fails, install lld: $(needs_line "$manager" lld)"
    fi

    if [[ "${#names[@]}" -eq 0 ]] && [[ "$submodules" -eq 0 ]]; then
        return 0
    fi
    echo "$NEEDS_MARK The build cannot start. This machine does not have these parts:"
    local i
    for i in "${!names[@]}"; do
        echo "$NEEDS_MARK   ${names[$i]}: ${reasons[$i]}"
    done
    if [[ "$submodules" -eq 1 ]]; then
        echo "$NEEDS_MARK   includes/mmgr and includes/embedded_types: the submodules that hold MMgr"
    fi
    echo "$NEEDS_MARK To get them, run these commands:"
    if [[ "$submodules" -eq 1 ]]; then
        echo "$NEEDS_MARK   git -C \"$TOP\" submodule update --init --recursive"
    fi
    if [[ "${#names[@]}" -gt 0 ]]; then
        needs_line "$manager" "${names[@]}" | while IFS= read -r line; do
            echo "$NEEDS_MARK   $line"
        done
    fi
    local note
    note="$(needs_cell "$NEEDS_MANAGERS" "$manager" note)"
    [[ -z "$note" ]] || echo "$NEEDS_MARK $note"
    echo "$NEEDS_MARK Then run the build again."
    return 1
}

# The install command of the manager "$1" for the programs and headers after it, one command on each line. A name
# without a package from that manager goes on a line of its own, for the developer to install.
needs_line() {
    local manager="$1"
    shift
    local install each packages=() others=() name package
    install="$(needs_cell "$NEEDS_MANAGERS" "$manager" install)"
    each="$(needs_cell "$NEEDS_MANAGERS" "$manager" each)"
    for name in "$@"; do
        package="$(needs_cell "$NEEDS_PACKAGES" "$name" "$manager")"
        if [[ -z "$install" ]] || [[ -z "$package" ]]; then
            others+=("$name")
        elif [[ " ${packages[*]:-} " != *" $package "* ]]; then
            packages+=("$package")
        fi
    done
    if [[ "${#packages[@]}" -gt 0 ]]; then
        if [[ "$each" = yes ]]; then
            for package in "${packages[@]}"; do
                echo "${install//%s/$package}"
            done
        else
            echo "${install//%s/${packages[*]}}"
        fi
    fi
    if [[ "${#others[@]}" -gt 0 ]]; then
        echo "install these parts yourself: ${others[*]}"
    fi
}
