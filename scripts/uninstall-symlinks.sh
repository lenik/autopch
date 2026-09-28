#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Generated/maintained for meson run_target; invoked as:
#   bash scripts/<name>.sh <SOURCE_ROOT> <BUILD_ROOT>
set -euo pipefail
SOURCE_ROOT="${1:-${MESON_SOURCE_ROOT:-.}}"
BUILD_ROOT="${2:-${MESON_BUILD_ROOT:-.}}"
for p in \
    "@1@/autopch" \
    "@3@/man1/autopch.1" \
    "@2@/bash-completion/completions/autopch"
do
    if [ -L "$p" ]; then
        sudo rm -f "$p"
        printf "Removed %s\n" "$p"
    else
        printf "Skipped (not a symlink): %s\n" "$p"
    fi
done
