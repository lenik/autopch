/*
 * Copyright (C) 2026 Lenik <autopch@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cflags.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

void compile_flags_init(compile_flags *cf) {
    cf->include_dirs = g_ptr_array_new_with_free_func(free);
    cf->defines = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
}

void compile_flags_clear(compile_flags *cf) {
    if (cf->include_dirs) {
        g_ptr_array_free(cf->include_dirs, TRUE);
        cf->include_dirs = NULL;
    }
    if (cf->defines) {
        g_hash_table_destroy(cf->defines);
        cf->defines = NULL;
    }
}

void compile_flags_add_include(compile_flags *cf, const char *dir) {
    if (!cf || !dir || !*dir) {
        return;
    }
    g_ptr_array_add(cf->include_dirs, xstrdup(dir));
}

void compile_flags_add_define(compile_flags *cf, const char *spec) {
    if (!cf || !spec || !*spec) {
        return;
    }
    const char *eq = strchr(spec, '=');
    char *name;
    char *val;
    if (eq) {
        name = xstrndup(spec, (size_t)(eq - spec));
        val = xstrdup(eq + 1);
    } else {
        name = xstrdup(spec);
        val = xstrdup("");
    }
    if (!name || !val) {
        free(name);
        free(val);
        return;
    }
    g_hash_table_insert(cf->defines, name, val);
}

int compile_flags_parse_cflags(compile_flags *cf, const char *cflags) {
    if (!cflags || !*cflags) {
        return 0;
    }
    int argc = 0;
    char **argv = split_shell_words(cflags, &argc);
    if (!argv) {
        return -1;
    }
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (strncmp(a, "-I", 2) == 0) {
            if (a[2]) {
                compile_flags_add_include(cf, a + 2);
            } else if (i + 1 < argc) {
                compile_flags_add_include(cf, argv[++i]);
            }
        } else if (strcmp(a, "-isystem") == 0) {
            if (i + 1 < argc) {
                compile_flags_add_include(cf, argv[++i]);
            }
        } else if (strncmp(a, "-D", 2) == 0) {
            if (a[2]) {
                compile_flags_add_define(cf, a + 2);
            } else if (i + 1 < argc) {
                compile_flags_add_define(cf, argv[++i]);
            }
        }
    }
    free_argv(argv);
    return 0;
}
