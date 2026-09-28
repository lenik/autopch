/*
 * Copyright (C) 2026 Lenik <autopch@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "scan.h"
#include "util.h"

#include <bas/base/str.h>
#include <bas/log/deflog.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int scan_is_source_name(const char *name, lang_t lang) {
    const char *dot = strrchr(name, '.');
    if (!dot || !dot[1]) {
        return 0;
    }
    if (lang == LANG_C) {
        return strcmp(dot, ".c") == 0;
    }
    /* C++: .c .cc .cpp .cxx .C */
    if (strcmp(dot, ".c") == 0 || strcmp(dot, ".cc") == 0 || strcmp(dot, ".cpp") == 0 ||
        strcmp(dot, ".cxx") == 0 || strcmp(dot, ".C") == 0) {
        return 1;
    }
    return 0;
}

static int scan_dir(GPtrArray *out, const char *dir, lang_t lang) {
    DIR *d = opendir(dir);
    if (!d) {
        logwarn_fmt("cannot open directory %s", dir);
        return -1;
    }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        char *child = path_join(dir, ent->d_name);
        if (!child) {
            closedir(d);
            return -1;
        }
        if (path_is_dir(child)) {
            if (scan_dir(out, child, lang) != 0) {
                free(child);
                closedir(d);
                return -1;
            }
            free(child);
        } else if (path_is_file(child) && scan_is_source_name(ent->d_name, lang)) {
            g_ptr_array_add(out, child);
        } else {
            free(child);
        }
    }
    closedir(d);
    return 0;
}

int scan_collect_sources(GPtrArray *out, char **paths, int npaths, lang_t lang, int recursive) {
    for (int i = 0; i < npaths; i++) {
        const char *p = paths[i];
        if (path_is_dir(p)) {
            if (!recursive) {
                logwarn_fmt("ignoring directory %s (pass -r to recurse)", p);
                continue;
            }
            if (scan_dir(out, p, lang) != 0) {
                return -1;
            }
        } else if (path_is_file(p)) {
            g_ptr_array_add(out, xstrdup(p));
        } else {
            logwarn_fmt("path not found: %s", p);
        }
    }
    return 0;
}
