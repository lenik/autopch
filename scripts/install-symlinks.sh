#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Generated/maintained for meson run_target; invoked as:
#   bash scripts/<name>.sh <SOURCE_ROOT> <BUILD_ROOT>
set -euo pipefail
SOURCE_ROOT="${1:-${MESON_SOURCE_ROOT:-.}}"
BUILD_ROOT="${2:-${MESON_BUILD_ROOT:-.}}"
prefix="@0@"
bindir="@1@"
datadir="@2@"
mandir="@3@"
mkdir -p "$bindir" "$datadir/bash-completion/completions" "$mandir/man1"
sudo ln -sfn "${BUILD_ROOT}/autopch" "$bindir/autopch"
sudo ln -sfn "${BUILD_ROOT}/autopch.1" "$mandir/man1/autopch.1"
sudo ln -sfn "${SOURCE_ROOT}/autopch.bash" "$datadir/bash-completion/completions/autopch"
printf "Symlinks installed under %s\n" "$prefix"
