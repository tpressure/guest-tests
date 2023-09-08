#!/usr/bin/env bash

# @describe Guest Test Repo Formatter
# This script applies the following formatting tools onto the repo:
# - clang-format
# - cmake-format
#
# Changes are applied in-place except if the `--check` parameter is provided.
#
# @flag    -c --check

eval "$(argc --argc-eval "$0" "$@")"
JUST_CHECK=${argc_check=0} # "0" or "1"

set -euo pipefail
IFS=$'\n\t'

# Ensure that this script is always executed from the root of the project.
DIR=$(dirname "$(realpath "$0")")
cd "$DIR/.." || exit

function fn_format_sources() {
    echo "Formatting Sources (C, CPP)"

    FOLDERS=(lib tests) # don't reformat contrib folder

    EXTENSIONS=(c h cpp hpp) # lds/S explicitly not listed; formatting errors
    EXTENSIONS_ARG_STR=()
    for EXTENSION in "${EXTENSIONS[@]}"; do
        EXTENSIONS_ARG_STR+=(--extension "$EXTENSION")
    done

    for FOLDER in "${FOLDERS[@]}"; do
        echo "  Processing folder: $FOLDER"
        if [ "$JUST_CHECK" = "1" ]; then
            fd --type file "${EXTENSIONS_ARG_STR[@]}" . "$FOLDER" | xargs -I {} clang-format --Werror --dry-run {}
        else
            fd --type file "${EXTENSIONS_ARG_STR[@]}" . "$FOLDER" | xargs -I {} clang-format --Werror --i {}
        fi
    done
}

function fn_format_cmake() {
    echo "Formatting CMake files"

    if [ "$JUST_CHECK" = "1" ]; then
        fd ".*\.cmake|CMakeLists.txt" | xargs -I {} cmake-format --check {}
    else
        fd ".*\.cmake|CMakeLists.txt" | xargs -I {} cmake-format --in-place {}
    fi
}
function fn_main() {
    fn_format_cmake
    fn_format_sources
}

fn_main
