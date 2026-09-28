/*
 * Copyright (C) 2026 Lenik <autopch@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char *xstrdup(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t n = strlen(s);
    char *p = malloc(n + 1);
    if (!p) {
        return NULL;
    }
    memcpy(p, s, n + 1);
    return p;
}

char *xstrndup(const char *s, size_t n) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s);
    if (n > len) {
        n = len;
    }
    char *p = malloc(n + 1);
    if (!p) {
        return NULL;
    }
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

char *path_join(const char *dir, const char *name) {
    if (!dir || !*dir) {
        return xstrdup(name);
    }
    if (!name) {
        return xstrdup(dir);
    }
    size_t dl = strlen(dir);
    size_t nl = strlen(name);
    int need_slash = !(dl > 0 && dir[dl - 1] == '/');
    char *out = malloc(dl + need_slash + nl + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, dir, dl);
    size_t o = dl;
    if (need_slash) {
        out[o++] = '/';
    }
    memcpy(out + o, name, nl + 1);
    return out;
}

char *path_dirname(const char *path) {
    if (!path || !*path) {
        return xstrdup(".");
    }
    const char *slash = strrchr(path, '/');
    if (!slash) {
        return xstrdup(".");
    }
    if (slash == path) {
        return xstrdup("/");
    }
    return xstrndup(path, (size_t)(slash - path));
}

char *path_basename(const char *path) {
    if (!path || !*path) {
        return xstrdup("");
    }
    const char *slash = strrchr(path, '/');
    return xstrdup(slash ? slash + 1 : path);
}

int path_is_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

int path_is_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISREG(st.st_mode);
}

char *path_realpath_or_dup(const char *path) {
    char buf[PATH_MAX];
    if (realpath(path, buf)) {
        return xstrdup(buf);
    }
    return xstrdup(path);
}

int path_ensure_parent(const char *path) {
    char *dir = path_dirname(path);
    if (!dir) {
        return -1;
    }
    if (strcmp(dir, ".") == 0 || strcmp(dir, "/") == 0) {
        free(dir);
        return 0;
    }
    if (path_is_dir(dir)) {
        free(dir);
        return 0;
    }
    /* mkdir parents one level at a time */
    size_t len = strlen(dir);
    for (size_t i = 1; i <= len; i++) {
        if (dir[i] != '/' && dir[i] != '\0') {
            continue;
        }
        char saved = dir[i];
        dir[i] = '\0';
        if (!path_is_dir(dir)) {
            if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
                dir[i] = saved;
                free(dir);
                return -1;
            }
        }
        dir[i] = saved;
    }
    free(dir);
    return 0;
}

char **split_shell_words(const char *s, int *argc_out) {
    size_t cap = 8;
    size_t n = 0;
    char **argv = calloc(cap, sizeof(char *));
    if (!argv) {
        return NULL;
    }

    const char *p = s;
    while (p && *p) {
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (!*p) {
            break;
        }

        char *tok = NULL;
        size_t tcap = 32;
        size_t tlen = 0;
        tok = malloc(tcap);
        if (!tok) {
            free_argv(argv);
            return NULL;
        }

        char quote = 0;
        while (*p) {
            if (!quote && isspace((unsigned char)*p)) {
                break;
            }
            if (!quote && (*p == '"' || *p == '\'')) {
                quote = *p++;
                continue;
            }
            if (quote && *p == quote) {
                quote = 0;
                p++;
                continue;
            }
            if (*p == '\\' && p[1] && (!quote || quote == '"')) {
                p++;
            }
            if (tlen + 1 >= tcap) {
                tcap *= 2;
                char *nt = realloc(tok, tcap);
                if (!nt) {
                    free(tok);
                    free_argv(argv);
                    return NULL;
                }
                tok = nt;
            }
            tok[tlen++] = *p++;
        }
        tok[tlen] = '\0';

        if (n + 1 >= cap) {
            cap *= 2;
            char **na = realloc(argv, cap * sizeof(char *));
            if (!na) {
                free(tok);
                free_argv(argv);
                return NULL;
            }
            argv = na;
            for (size_t i = n + 1; i < cap; i++) {
                argv[i] = NULL;
            }
        }
        argv[n++] = tok;
    }

    argv[n] = NULL;
    if (argc_out) {
        *argc_out = (int)n;
    }
    return argv;
}

void free_argv(char **argv) {
    if (!argv) {
        return;
    }
    for (size_t i = 0; argv[i]; i++) {
        free(argv[i]);
    }
    free(argv);
}
