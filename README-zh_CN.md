# autopch

`autopch` 面向大型 C/C++ 工程，把常用头文件聚类成若干组预编译头（PCH）。
它是基于 Meson 的 C 命令行工具，依赖 `bas-c`。

## 动机

只用一个全局 PCH，编译产物可能爆炸；PCH 里头文件太少，加速效果又差。
`autopch` 按「同现」给头文件建图并聚类，再把每个源文件分配到重叠最多的簇，
让每个 PCH 够用、又不至于大到失控。

## 用法

```bash
autopch [OPTION]... FILE...
```

默认按 **C** 分析；加 `-x` 则按 C++。

主要选项：

| 选项 | 含义 |
|------|------|
| `-x` | 按 C++ 分析（默认 C） |
| `-a` / `--all` | 同时分析本地 `#include "..."`（默认忽略） |
| `-c` / `--min-correlation N` | 边的最小相关指数（默认 **3**） |
| `-I DIR`、`-D NAME[=VAL]` | 头文件搜索路径与宏 |
| `--cc NAME`、`--cflags FLAGS` | 编译器内建宏 / 解析编译参数 |
| `-R` | 递归跟随 `#include` |
| `-r` | 递归扫描源文件目录（默认忽略目录参数） |
| `-d` / `--dag` | 簇内头文件按 include 依赖拓扑排序 |
| `-o` / `--output PREFIX` | 输出路径前缀（默认 `./cluster` → `./cluster1.h`…） |
| `-m FILE` | 写出 `src:clusterK.h` 映射 |
| `-w` | 插入或改写 `#include "clusterK.h"`（尽量保留原行位置） |

聚类编号按 **major**（头文件个数）降序：最大的为 `PREFIX1`，其次 `PREFIX2`…

默认只分析 `#include <...>`；需要本地 `#include "..."` 时加 `-a`。

本仓库用 Meson `c_pch: 'src/pch1.h'` 编译。更新聚类：

```bash
ninja -C /build pch
```

算法概要：同一源文件中出现的头文件互为相关，相关指数随共现源文件数增加；
保留相关指数 ≥ 阈值的边，连通分量（至少 2 个头）即聚类；源文件归入重叠最大的簇。

## 仓库结构

- `src/` — `autopch.c` 与分析模块
- `tests/` — 单元测试与夹具
- `debian/` — 打包元数据
- `man/` — AsciiDoc man 页
- `meson.build` — 构建定义

## 构建

```bash
sudo apt install meson ninja-build gcc pkg-config libbas-c-dev asciidoctor
meson setup /build
ninja -C /build
meson test -C /build
```

## 许可证

Copyright (C) 2026 Lenik <autopch@bodz.net>

采用 **AGPL-3.0-or-later** 许可。  
本项目明确反对 AI 剥削与 AI 霸权，反对无脑 MIT 式许可证和政治愚蠢的 BSD 式许可证。  
完整文本及项目补充条款见 `LICENSE`。
