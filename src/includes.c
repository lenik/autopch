/*
 * Copyright (C) 2026 Lenik <autopch@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "includes.h"
#include "util.h"

#include <bas/base/str.h>
#include <bas/log/deflog.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { ST_ACTIVE = 1, ST_SKIP = 0, ST_BOTH = 2 } branch_state;

typedef struct {
    branch_state *stack;
    size_t depth;
    size_t cap;
} cond_stack;

static void cond_init(cond_stack *cs) {
    cs->stack = NULL;
    cs->depth = 0;
    cs->cap = 0;
}

static void cond_clear(cond_stack *cs) {
    free(cs->stack);
    cs->stack = NULL;
    cs->depth = 0;
    cs->cap = 0;
}

static int cond_push(cond_stack *cs, branch_state st) {
    if (cs->depth + 1 > cs->cap) {
        size_t ncap = cs->cap ? cs->cap * 2 : 8;
        branch_state *ns = realloc(cs->stack, ncap * sizeof(*ns));
        if (!ns) {
            return -1;
        }
        cs->stack = ns;
        cs->cap = ncap;
    }
    cs->stack[cs->depth++] = st;
    return 0;
}

static branch_state cond_effective(const cond_stack *cs) {
    branch_state eff = ST_ACTIVE;
    for (size_t i = 0; i < cs->depth; i++) {
        if (cs->stack[i] == ST_SKIP) {
            return ST_SKIP;
        }
        if (cs->stack[i] == ST_BOTH) {
            eff = ST_BOTH;
        }
    }
    return eff;
}

static void header_info_free(gpointer p) {
    header_info *h = p;
    if (!h) {
        return;
    }
    free(h->key);
    free(h->spell);
    free(h);
}

static void free_str_set(gpointer p) {
    if (p) {
        g_hash_table_destroy(p);
    }
}

void include_index_init(include_index *ix) {
    ix->headers = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, header_info_free);
    ix->src_headers = g_hash_table_new_full(g_str_hash, g_str_equal, free, free_str_set);
    ix->dag_edges = g_hash_table_new_full(g_str_hash, g_str_equal, free, NULL);
    ix->header_freq = g_hash_table_new_full(g_str_hash, g_str_equal, free, NULL);
}

void include_index_clear(include_index *ix) {
    if (ix->headers) {
        g_hash_table_destroy(ix->headers);
        ix->headers = NULL;
    }
    if (ix->src_headers) {
        g_hash_table_destroy(ix->src_headers);
        ix->src_headers = NULL;
    }
    if (ix->dag_edges) {
        g_hash_table_destroy(ix->dag_edges);
        ix->dag_edges = NULL;
    }
    if (ix->header_freq) {
        g_hash_table_destroy(ix->header_freq);
        ix->header_freq = NULL;
    }
}

static int macro_defined(const compile_flags *cf, const char *name) {
    return g_hash_table_contains(cf->defines, name);
}

/* Evaluate simple #if defined(NAME) / !defined(NAME) / defined NAME.
 * Returns 1 true, 0 false, -1 unknown. */
static int eval_simple_if(const compile_flags *cf, const char *expr) {
    char *buf = xstrdup(expr);
    if (!buf) {
        return -1;
    }
    char *p = trim(buf);
    int neg = 0;
    if (*p == '!') {
        neg = 1;
        p++;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
    }
    if (strncmp(p, "defined", 7) == 0 && (p[7] == '(' || isspace((unsigned char)p[7]) || !p[7])) {
        p += 7;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '(') {
            p++;
        }
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        char *name = p;
        while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
            p++;
        }
        *p = '\0';
        int def = macro_defined(cf, name);
        free(buf);
        return neg ? !def : def;
    }
    /* bare identifier: treat as defined(name) */
    if (isalpha((unsigned char)*p) || *p == '_') {
        char *name = p;
        while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
            p++;
        }
        if (*p == '\0' || isspace((unsigned char)*p)) {
            *p = '\0';
            int def = macro_defined(cf, name);
            free(buf);
            return neg ? !def : def;
        }
    }
    free(buf);
    return -1;
}

static char *make_spell(const char *name, int angle) {
    size_t n = strlen(name);
    char *s = malloc(n + 3);
    if (!s) {
        return NULL;
    }
    if (angle) {
        s[0] = '<';
        memcpy(s + 1, name, n);
        s[n + 1] = '>';
        s[n + 2] = '\0';
    } else {
        s[0] = '"';
        memcpy(s + 1, name, n);
        s[n + 1] = '"';
        s[n + 2] = '\0';
    }
    return s;
}

static char *resolve_include(const char *name, int angle, const char *current_file,
                             const compile_flags *cf) {
    if (!angle) {
        char *dir = path_dirname(current_file);
        char *cand = path_join(dir, name);
        free(dir);
        if (cand && path_is_file(cand)) {
            char *rp = path_realpath_or_dup(cand);
            free(cand);
            return rp;
        }
        free(cand);
    }
    for (guint i = 0; i < cf->include_dirs->len; i++) {
        const char *inc = g_ptr_array_index(cf->include_dirs, i);
        char *cand = path_join(inc, name);
        if (cand && path_is_file(cand)) {
            char *rp = path_realpath_or_dup(cand);
            free(cand);
            return rp;
        }
        free(cand);
    }
    return NULL;
}

static const header_info *ensure_header(include_index *ix, const char *key, const char *spell,
                                        int angle) {
    header_info *h = g_hash_table_lookup(ix->headers, key);
    if (h) {
        return h;
    }
    h = calloc(1, sizeof(*h));
    if (!h) {
        return NULL;
    }
    h->key = xstrdup(key);
    h->spell = xstrdup(spell);
    h->angle = angle;
    if (!h->key || !h->spell) {
        header_info_free(h);
        return NULL;
    }
    g_hash_table_insert(ix->headers, h->key, h);
    return h;
}

static void bump_freq(include_index *ix, const char *key) {
    gpointer owned = NULL;
    gpointer v = NULL;
    if (g_hash_table_lookup_extended(ix->header_freq, key, &owned, &v)) {
        g_hash_table_steal(ix->header_freq, owned);
        g_hash_table_insert(ix->header_freq, owned, (gpointer)((intptr_t)v + 1));
    } else {
        g_hash_table_insert(ix->header_freq, xstrdup(key), (gpointer)(intptr_t)1);
    }
}

static void add_dag_edge(include_index *ix, const char *from, const char *to) {
    if (!from || !to || strcmp(from, to) == 0) {
        return;
    }
    char *pair = NULL;
    if (asprintf(&pair, "%s\x1f%s", from, to) < 0) {
        return;
    }
    if (!g_hash_table_contains(ix->dag_edges, pair)) {
        g_hash_table_insert(ix->dag_edges, pair, GINT_TO_POINTER(1));
    } else {
        free(pair);
    }
}

typedef struct parse_ctx {
    include_index *ix;
    const compile_flags *cf;
    int recursive;
    int all_includes;      /* if 0, skip #include "..." */
    GHashTable *visited;   /* files already fully parsed for recursion */
    GHashTable *collect;   /* header keys for current source */
    const char *from_key;  /* including header key, or NULL for source */
} parse_ctx;

static int parse_file(parse_ctx *pc, const char *path);

static int handle_include(parse_ctx *pc, const char *current_file, const char *name, int angle) {
    char *spell = make_spell(name, angle);
    if (!spell) {
        return -1;
    }
    char *resolved = resolve_include(name, angle, current_file, pc->cf);
    const char *key = resolved ? resolved : spell;

    const header_info *hi = ensure_header(pc->ix, key, spell, angle);
    if (!hi) {
        free(spell);
        free(resolved);
        return -1;
    }
    /* Prefer resolved key stored in hi */
    key = hi->key;
    free(spell);
    free(resolved);

    g_hash_table_insert(pc->collect, (gpointer)key, GINT_TO_POINTER(1));

    if (pc->from_key) {
        add_dag_edge(pc->ix, pc->from_key, key);
    }

    if (pc->recursive && path_is_file(key)) {
        const char *prev = pc->from_key;
        pc->from_key = key;
        int rc = parse_file(pc, key);
        pc->from_key = prev;
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

static char *strip_line_comment(char *line) {
    int in_str = 0;
    char q = 0;
    for (char *p = line; *p; p++) {
        if (!in_str && p[0] == '/' && p[1] == '/') {
            *p = '\0';
            break;
        }
        if (!in_str && (*p == '"' || *p == '\'')) {
            in_str = 1;
            q = *p;
        } else if (in_str && *p == q && p[-1] != '\\') {
            in_str = 0;
        }
    }
    return line;
}

static int parse_file(parse_ctx *pc, const char *path) {
    if (g_hash_table_contains(pc->visited, path)) {
        return 0;
    }
    g_hash_table_insert(pc->visited, xstrdup(path), GINT_TO_POINTER(1));

    FILE *fp = fopen(path, "r");
    if (!fp) {
        logdebug_fmt("cannot open %s", path);
        return 0;
    }

    cond_stack cs;
    cond_init(&cs);
    char *line = NULL;
    size_t cap = 0;
    int rc = 0;

    while (str_getline(&line, &cap, fp) > 0) {
        chomp(line);
        char *p = line;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p != '#') {
            continue;
        }
        p++;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }

        if (strncmp(p, "ifdef", 5) == 0 && !isalnum((unsigned char)p[5])) {
            p += 5;
            while (*p && isspace((unsigned char)*p)) {
                p++;
            }
            char *name = p;
            while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
                p++;
            }
            *p = '\0';
            branch_state st = macro_defined(pc->cf, name) ? ST_ACTIVE : ST_SKIP;
            if (cond_push(&cs, st) != 0) {
                rc = -1;
                break;
            }
            continue;
        }
        if (strncmp(p, "ifndef", 6) == 0 && !isalnum((unsigned char)p[6])) {
            p += 6;
            while (*p && isspace((unsigned char)*p)) {
                p++;
            }
            char *name = p;
            while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
                p++;
            }
            *p = '\0';
            branch_state st = macro_defined(pc->cf, name) ? ST_SKIP : ST_ACTIVE;
            if (cond_push(&cs, st) != 0) {
                rc = -1;
                break;
            }
            continue;
        }
        if (strncmp(p, "if", 2) == 0 && !isalnum((unsigned char)p[2])) {
            p += 2;
            while (*p && isspace((unsigned char)*p)) {
                p++;
            }
            strip_line_comment(p);
            int ev = eval_simple_if(pc->cf, p);
            branch_state st = (ev < 0) ? ST_BOTH : (ev ? ST_ACTIVE : ST_SKIP);
            if (cond_push(&cs, st) != 0) {
                rc = -1;
                break;
            }
            continue;
        }
        if (strncmp(p, "elif", 4) == 0 && !isalnum((unsigned char)p[4])) {
            if (cs.depth == 0) {
                continue;
            }
            branch_state cur = cs.stack[cs.depth - 1];
            if (cur == ST_BOTH) {
                continue;
            }
            if (cur == ST_ACTIVE) {
                cs.stack[cs.depth - 1] = ST_SKIP;
                continue;
            }
            /* previous was SKIP: evaluate */
            p += 4;
            while (*p && isspace((unsigned char)*p)) {
                p++;
            }
            strip_line_comment(p);
            int ev = eval_simple_if(pc->cf, p);
            cs.stack[cs.depth - 1] = (ev < 0) ? ST_BOTH : (ev ? ST_ACTIVE : ST_SKIP);
            continue;
        }
        if (strncmp(p, "else", 4) == 0 && !isalnum((unsigned char)p[4])) {
            if (cs.depth == 0) {
                continue;
            }
            branch_state cur = cs.stack[cs.depth - 1];
            if (cur == ST_BOTH) {
                continue;
            }
            cs.stack[cs.depth - 1] = (cur == ST_ACTIVE) ? ST_SKIP : ST_ACTIVE;
            continue;
        }
        if (strncmp(p, "endif", 5) == 0 && !isalnum((unsigned char)p[5])) {
            if (cs.depth > 0) {
                cs.depth--;
            }
            continue;
        }

        branch_state eff = cond_effective(&cs);
        if (eff == ST_SKIP) {
            continue;
        }

        if (strncmp(p, "include", 7) == 0 && !isalnum((unsigned char)p[7])) {
            p += 7;
            while (*p && isspace((unsigned char)*p)) {
                p++;
            }
            strip_line_comment(p);
            char delim = *p;
            int angle = 0;
            if (delim == '<') {
                angle = 1;
            } else if (delim == '"') {
                angle = 0;
            } else {
                continue;
            }
            /* Local includes are ignored unless -a/--all */
            if (!angle && !pc->all_includes) {
                continue;
            }
            p++;
            char *end = strchr(p, angle ? '>' : '"');
            if (!end) {
                continue;
            }
            *end = '\0';
            if (handle_include(pc, path, p, angle) != 0) {
                rc = -1;
                break;
            }
        }
    }

    free(line);
    cond_clear(&cs);
    fclose(fp);
    return rc;
}

int includes_analyze_source(include_index *ix, const char *source_path, const compile_flags *cf,
                            int recursive_include, int all_includes) {
    GHashTable *collect = g_hash_table_new(g_str_hash, g_str_equal);
    GHashTable *visited = g_hash_table_new_full(g_str_hash, g_str_equal, free, NULL);

    parse_ctx pc = {
        .ix = ix,
        .cf = cf,
        .recursive = recursive_include,
        .all_includes = all_includes,
        .visited = visited,
        .collect = collect,
        .from_key = NULL,
    };

    int rc = parse_file(&pc, source_path);
    g_hash_table_destroy(visited);

    if (rc != 0) {
        g_hash_table_destroy(collect);
        return rc;
    }

    /* Record per-source set and bump frequencies once per source */
    GHashTable *owned = g_hash_table_new(g_str_hash, g_str_equal);
    GHashTableIter it;
    gpointer k, v;
    g_hash_table_iter_init(&it, collect);
    while (g_hash_table_iter_next(&it, &k, &v)) {
        g_hash_table_insert(owned, k, GINT_TO_POINTER(1));
        bump_freq(ix, (const char *)k);
    }
    g_hash_table_destroy(collect);

    g_hash_table_insert(ix->src_headers, xstrdup(source_path), owned);
    return 0;
}
