/*
 * Copyright (C) 2026 Lenik <autopch@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "compiler.h"
#include "util.h"

#include <bas/base/str.h>
#include <bas/log/deflog.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int looks_like_msvc(const char *cc) {
    const char *base = strrchr(cc, '/');
    base = base ? base + 1 : cc;
    return strcasecmp(base, "cl") == 0 || strcasecmp(base, "cl.exe") == 0 ||
           strcasecmp(base, "msvc") == 0;
}

int compiler_load_builtins(compile_flags *cf, const char *cc, lang_t lang) {
    if (!cf || !cc || !*cc) {
        return -1;
    }
    if (looks_like_msvc(cc)) {
        logwarn_fmt("MSVC-style compiler '%s' builtin dump is not supported yet", cc);
        return -1;
    }

    const char *xlang = (lang == LANG_CXX) ? "c++" : "c";
    char *cmd = NULL;
    if (asprintf(&cmd, "%s -dM -E -x %s /dev/null 2>/dev/null", cc, xlang) < 0) {
        return -1;
    }

    FILE *fp = popen(cmd, "r");
    free(cmd);
    if (!fp) {
        logwarn_fmt("failed to run compiler '%s' for builtins", cc);
        return -1;
    }

    char *line = NULL;
    size_t cap = 0;
    while (str_getline(&line, &cap, fp) > 0) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        chomp(p);
        /* #define NAME rest */
        if (strncmp(p, "#define ", 8) != 0) {
            continue;
        }
        p += 8;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        char *name = p;
        while (*p && !isspace((unsigned char)*p)) {
            p++;
        }
        char *val;
        if (*p) {
            *p++ = '\0';
            while (*p && isspace((unsigned char)*p)) {
                p++;
            }
            val = p;
        } else {
            val = "";
        }
        /* Do not override user -D */
        if (!g_hash_table_contains(cf->defines, name)) {
            g_hash_table_insert(cf->defines, xstrdup(name), xstrdup(val));
        }
    }
    free(line);
    int st = pclose(fp);
    if (st != 0) {
        logwarn_fmt("compiler '%s' exited with status %d while dumping macros", cc, st);
        return -1;
    }
    return 0;
}
