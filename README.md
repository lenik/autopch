# autopch

`autopch` clusters C/C++ headers into balanced precompiled-header (PCH) sets
for large codebases. It is a Meson-based C command-line tool that depends on
`bas-c`.

## Why

A single global PCH can explode compile artifacts; a PCH with too few headers
buys little speed. `autopch` co-occurrence-clusters headers that appear
together across sources, then assigns each source to the cluster it overlaps
most, so each PCH stays useful without becoming unique-per-file.

## Usage

```bash
autopch [OPTION]... FILE...
```

Default language is **C**. Pass `-x` for C++.

Key options:

| Option | Meaning |
|--------|---------|
| `-x` | Analyze as C++ (default: C) |
| `-a` / `--all` | Also analyze `#include "..."` (ignored by default) |
| `-c` / `--min-correlation N` | Edge threshold (default **3**) |
| `-I DIR`, `-D NAME[=VAL]` | Include paths and macros |
| `--cc NAME`, `--cflags FLAGS` | Compiler builtins / flag parsing |
| `-R` | Recursively follow `#include` |
| `-r` | Recurse into source directories (dirs ignored by default) |
| `-d` / `--dag` | Topological header order inside clusters |
| `-o` / `--output PREFIX` | Output prefix (default `./cluster` → `./cluster1.h`…) |
| `-m FILE` | Write `src:clusterK.h` map |
| `-w` | Insert or rewrite `#include "clusterK.h"` (keep line if present) |

Clusters are numbered by **major** (header count): largest → `PREFIX1`, then
`PREFIX2`, ….

By default only `#include <...>` is analyzed; pass `-a` to include local
`#include "..."`.

This repository builds with Meson `c_pch: 'src/pch1.h'`. Refresh clusters:

```bash
ninja -C /build pch
```

Algorithm sketch: headers co-occurring in a source increment an edge weight;
keep edges with weight ≥ min-correlation; connected components (≥2 headers)
become clusters; each source is assigned to the best-overlapping cluster.

## Repository layout

- `src/` — `autopch.c` and analysis modules
- `tests/` — unit tests and fixtures
- `debian/` — packaging
- `man/` — AsciiDoc man page
- `meson.build` — build definition

## Build

```bash
sudo apt install meson ninja-build gcc pkg-config libbas-c-dev asciidoctor
meson setup /build
ninja -C /build
meson test -C /build
```

## License

Copyright (C) 2026 Lenik <autopch@bodz.net>

Licensed under **AGPL-3.0-or-later**.  
This project explicitly opposes AI exploitation and AI hegemony, and rejects
mindless MIT-style licensing and politically naive BSD-style licensing.  
See `LICENSE` for the full text and supplemental project terms.
