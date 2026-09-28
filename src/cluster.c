/*
 * Copyright (C) 2026 Lenik <autopch@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cluster.h"
#include "util.h"

#include <bas/log/deflog.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void pch_cluster_list_clear(pch_cluster_list *list) {
    if (!list) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        pch_cluster *c = &list->items[i];
        for (size_t j = 0; j < c->n_headers; j++) {
            free(c->headers[j]);
        }
        free(c->headers);
        for (size_t j = 0; j < c->n_sources; j++) {
            free(c->sources[j]);
        }
        free(c->sources);
        free(c->path);
        free(c->filename);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

static char *pair_key(const char *a, const char *b) {
    /* canonical unordered pair: lexicographically smaller first */
    const char *x = a;
    const char *y = b;
    if (strcmp(a, b) > 0) {
        x = b;
        y = a;
    }
    char *k = NULL;
    if (asprintf(&k, "%s\x1f%s", x, y) < 0) {
        return NULL;
    }
    return k;
}

static GHashTable *build_correlation(include_index *ix) {
    GHashTable *corr = g_hash_table_new_full(g_str_hash, g_str_equal, free, NULL);
    GHashTableIter sit;
    gpointer sk, sv;
    g_hash_table_iter_init(&sit, ix->src_headers);
    while (g_hash_table_iter_next(&sit, &sk, &sv)) {
        GHashTable *set = sv;
        GPtrArray *keys = g_ptr_array_new();
        GHashTableIter hit;
        gpointer hk, hv;
        g_hash_table_iter_init(&hit, set);
        while (g_hash_table_iter_next(&hit, &hk, &hv)) {
            g_ptr_array_add(keys, hk);
        }
        for (guint i = 0; i < keys->len; i++) {
            for (guint j = i + 1; j < keys->len; j++) {
                char *pk = pair_key(keys->pdata[i], keys->pdata[j]);
                if (!pk) {
                    g_ptr_array_free(keys, TRUE);
                    g_hash_table_destroy(corr);
                    return NULL;
                }
                gpointer ok = NULL;
                gpointer ov = NULL;
                if (g_hash_table_lookup_extended(corr, pk, &ok, &ov)) {
                    g_hash_table_steal(corr, ok);
                    g_hash_table_insert(corr, ok, (gpointer)((intptr_t)ov + 1));
                    free(pk);
                } else {
                    g_hash_table_insert(corr, pk, (gpointer)(intptr_t)1);
                }
            }
        }
        g_ptr_array_free(keys, TRUE);
    }
    return corr;
}

static include_index *g_sort_ix;

static int cmp_freq_name_qsort(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    intptr_t fa = (intptr_t)g_hash_table_lookup(g_sort_ix->header_freq, sa);
    intptr_t fb = (intptr_t)g_hash_table_lookup(g_sort_ix->header_freq, sb);
    if (fa != fb) {
        return (fb > fa) - (fb < fa);
    }
    return strcmp(sa, sb);
}

typedef struct {
    char **nodes;
    size_t n;
    GHashTable *index; /* key -> size_t idx */
    GPtrArray **adj;   /* adjacency lists of size_t indices */
} graph_t;

static void graph_free(graph_t *g) {
    if (!g) {
        return;
    }
    for (size_t i = 0; i < g->n; i++) {
        free(g->nodes[i]);
        if (g->adj) {
            g_ptr_array_free(g->adj[i], TRUE);
        }
    }
    free(g->nodes);
    free(g->adj);
    if (g->index) {
        g_hash_table_destroy(g->index);
    }
    free(g);
}

static graph_t *graph_from_corr(GHashTable *corr, int min_corr, include_index *ix) {
    GHashTable *node_set = g_hash_table_new_full(g_str_hash, g_str_equal, free, NULL);
    GHashTableIter it;
    gpointer k, v;
    g_hash_table_iter_init(&it, corr);
    while (g_hash_table_iter_next(&it, &k, &v)) {
        if ((intptr_t)v < min_corr) {
            continue;
        }
        char *pair = xstrdup((char *)k);
        if (!pair) {
            continue;
        }
        char *sep = strchr(pair, '\x1f');
        if (!sep) {
            free(pair);
            continue;
        }
        *sep = '\0';
        if (!g_hash_table_contains(node_set, pair)) {
            g_hash_table_insert(node_set, xstrdup(pair), GINT_TO_POINTER(1));
        }
        if (!g_hash_table_contains(node_set, sep + 1)) {
            g_hash_table_insert(node_set, xstrdup(sep + 1), GINT_TO_POINTER(1));
        }
        free(pair);
    }

    graph_t *g = calloc(1, sizeof(*g));
    if (!g) {
        g_hash_table_destroy(node_set);
        return NULL;
    }
    g->n = g_hash_table_size(node_set);
    if (g->n == 0) {
        g_hash_table_destroy(node_set);
        return g;
    }
    g->nodes = calloc(g->n, sizeof(char *));
    g->adj = calloc(g->n, sizeof(GPtrArray *));
    g->index = g_hash_table_new(g_str_hash, g_str_equal);
    if (!g->nodes || !g->adj || !g->index) {
        graph_free(g);
        g_hash_table_destroy(node_set);
        return NULL;
    }

    size_t idx = 0;
    g_hash_table_iter_init(&it, node_set);
    while (g_hash_table_iter_next(&it, &k, &v)) {
        g->nodes[idx] = xstrdup((char *)k);
        g->adj[idx] = g_ptr_array_new();
        /* store idx+1 so index 0 is not confused with a missing lookup */
        g_hash_table_insert(g->index, g->nodes[idx], GUINT_TO_POINTER(idx + 1));
        idx++;
    }
    g_hash_table_destroy(node_set);

    g_hash_table_iter_init(&it, corr);
    while (g_hash_table_iter_next(&it, &k, &v)) {
        if ((intptr_t)v < min_corr) {
            continue;
        }
        char *pair = xstrdup((char *)k);
        char *sep = strchr(pair, '\x1f');
        if (!sep) {
            free(pair);
            continue;
        }
        *sep = '\0';
        gpointer ia = g_hash_table_lookup(g->index, pair);
        gpointer ib = g_hash_table_lookup(g->index, sep + 1);
        if (ia && ib) {
            size_t a = GPOINTER_TO_UINT(ia) - 1;
            size_t b = GPOINTER_TO_UINT(ib) - 1;
            g_ptr_array_add(g->adj[a], (gpointer)(uintptr_t)b);
            g_ptr_array_add(g->adj[b], (gpointer)(uintptr_t)a);
        }
        free(pair);
    }
    (void)ix;
    return g;
}

static int cmp_strptr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Kahn topological sort within component; falls back to frequency order on cycle. */
static void order_headers(char **headers, size_t n, include_index *ix, int use_dag) {
    if (n <= 1) {
        return;
    }
    g_sort_ix = ix;
    if (!use_dag) {
        qsort(headers, n, sizeof(char *), cmp_freq_name_qsort);
        return;
    }

    GHashTable *idx = g_hash_table_new(g_str_hash, g_str_equal);
    for (size_t i = 0; i < n; i++) {
        g_hash_table_insert(idx, headers[i], (gpointer)(uintptr_t)i);
    }

    int *indeg = calloc(n, sizeof(int));
    GPtrArray **children = calloc(n, sizeof(GPtrArray *));
    if (!indeg || !children) {
        free(indeg);
        free(children);
        g_hash_table_destroy(idx);
        qsort(headers, n, sizeof(char *), cmp_freq_name_qsort);
        return;
    }
    for (size_t i = 0; i < n; i++) {
        children[i] = g_ptr_array_new();
    }

    GHashTableIter it;
    gpointer k, v;
    g_hash_table_iter_init(&it, ix->dag_edges);
    while (g_hash_table_iter_next(&it, &k, &v)) {
        char *pair = xstrdup((char *)k);
        char *sep = strchr(pair, '\x1f');
        if (!sep) {
            free(pair);
            continue;
        }
        *sep = '\0';
        const char *from = pair;
        const char *to = sep + 1;
        /* from includes to => to should appear before from */
        if (!g_hash_table_contains(idx, from) || !g_hash_table_contains(idx, to)) {
            free(pair);
            continue;
        }
        size_t fi = (uintptr_t)g_hash_table_lookup(idx, from);
        size_t ti = (uintptr_t)g_hash_table_lookup(idx, to);
        g_ptr_array_add(children[ti], (gpointer)(uintptr_t)fi);
        indeg[fi]++;
        free(pair);
    }

    char **ordered = calloc(n, sizeof(char *));
    size_t on = 0;
    GPtrArray *queue = g_ptr_array_new();
    for (size_t i = 0; i < n; i++) {
        if (indeg[i] == 0) {
            g_ptr_array_add(queue, (gpointer)(uintptr_t)i);
        }
    }
    while (queue->len > 0) {
        guint best = 0;
        for (guint i = 1; i < queue->len; i++) {
            size_t a = (uintptr_t)queue->pdata[best];
            size_t b = (uintptr_t)queue->pdata[i];
            if (cmp_freq_name_qsort(&headers[b], &headers[a]) < 0) {
                best = i;
            }
        }
        size_t u = (uintptr_t)queue->pdata[best];
        g_ptr_array_remove_index(queue, best);
        ordered[on++] = headers[u];
        for (guint i = 0; i < children[u]->len; i++) {
            size_t w = (uintptr_t)children[u]->pdata[i];
            if (--indeg[w] == 0) {
                g_ptr_array_add(queue, (gpointer)(uintptr_t)w);
            }
        }
    }
    g_ptr_array_free(queue, TRUE);
    for (size_t i = 0; i < n; i++) {
        g_ptr_array_free(children[i], TRUE);
    }
    free(children);
    free(indeg);
    g_hash_table_destroy(idx);

    if (on != n) {
        free(ordered);
        qsort(headers, n, sizeof(char *), cmp_freq_name_qsort);
        return;
    }
    memcpy(headers, ordered, n * sizeof(char *));
    free(ordered);
}

static size_t overlap_score(GHashTable *src_set, char **headers, size_t n) {
    size_t score = 0;
    for (size_t i = 0; i < n; i++) {
        if (g_hash_table_contains(src_set, headers[i])) {
            score++;
        }
    }
    return score;
}

static void free_comp(gpointer p) {
    g_ptr_array_free(p, TRUE);
}

static int cmp_cluster_major(const void *a, const void *b) {
    const pch_cluster *ca = a;
    const pch_cluster *cb = b;
    if (ca->n_headers != cb->n_headers) {
        return (cb->n_headers > ca->n_headers) - (cb->n_headers < ca->n_headers);
    }
    if (ca->n_sources != cb->n_sources) {
        return (cb->n_sources > ca->n_sources) - (cb->n_sources < ca->n_sources);
    }
    const char *ha = (ca->n_headers && ca->headers[0]) ? ca->headers[0] : "";
    const char *hb = (cb->n_headers && cb->headers[0]) ? cb->headers[0] : "";
    return strcmp(ha, hb);
}

int cluster_build(pch_cluster_list *out, include_index *ix, int min_corr, int use_dag,
                  const char *prefix, const char *ext) {
    memset(out, 0, sizeof(*out));
    if (!prefix || !*prefix) {
        prefix = "./cluster";
    }
    if (!ext) {
        ext = ".h";
    }
    GHashTable *corr = build_correlation(ix);
    if (!corr) {
        return -1;
    }
    graph_t *g = graph_from_corr(corr, min_corr, ix);
    g_hash_table_destroy(corr);
    if (!g) {
        return -1;
    }
    if (g->n == 0) {
        graph_free(g);
        loginfo("no header pairs met the minimum correlation; no clusters");
        return 0;
    }

    char *seen = calloc(g->n, 1);
    GPtrArray *components = g_ptr_array_new_with_free_func(free_comp);
    for (size_t s = 0; s < g->n; s++) {
        if (seen[s]) {
            continue;
        }
        GPtrArray *comp = g_ptr_array_new();
        GPtrArray *stack = g_ptr_array_new();
        g_ptr_array_add(stack, (gpointer)(uintptr_t)s);
        seen[s] = 1;
        while (stack->len > 0) {
            size_t u = (uintptr_t)stack->pdata[stack->len - 1];
            g_ptr_array_remove_index(stack, stack->len - 1);
            g_ptr_array_add(comp, g->nodes[u]);
            for (guint i = 0; i < g->adj[u]->len; i++) {
                size_t w = (uintptr_t)g->adj[u]->pdata[i];
                if (!seen[w]) {
                    seen[w] = 1;
                    g_ptr_array_add(stack, (gpointer)(uintptr_t)w);
                }
            }
        }
        g_ptr_array_free(stack, TRUE);
        if (comp->len >= 2) {
            g_ptr_array_add(components, comp);
        } else {
            g_ptr_array_free(comp, TRUE);
        }
    }
    free(seen);

    out->count = components->len;
    out->items = calloc(out->count ? out->count : 1, sizeof(pch_cluster));
    if (!out->items) {
        g_ptr_array_free(components, TRUE);
        graph_free(g);
        return -1;
    }

    for (guint ci = 0; ci < components->len; ci++) {
        GPtrArray *comp = components->pdata[ci];
        pch_cluster *c = &out->items[ci];
        c->n_headers = comp->len;
        c->headers = calloc(c->n_headers, sizeof(char *));
        for (guint i = 0; i < comp->len; i++) {
            c->headers[i] = xstrdup((char *)comp->pdata[i]);
        }
        order_headers(c->headers, c->n_headers, ix, use_dag);
    }
    g_ptr_array_free(components, TRUE);
    graph_free(g);

    /* Assign sources before numbering so major sort can use n_sources as tiebreak */
    GHashTableIter sit;
    gpointer sk, sv;
    g_hash_table_iter_init(&sit, ix->src_headers);
    while (g_hash_table_iter_next(&sit, &sk, &sv)) {
        const char *src = sk;
        GHashTable *set = sv;
        size_t best_score = 0;
        size_t best_i = 0;
        int found = 0;
        for (size_t i = 0; i < out->count; i++) {
            size_t sc = overlap_score(set, out->items[i].headers, out->items[i].n_headers);
            if (sc == 0) {
                continue;
            }
            if (!found || sc > best_score ||
                (sc == best_score && out->items[i].n_headers > out->items[best_i].n_headers)) {
                best_score = sc;
                best_i = i;
                found = 1;
            }
        }
        if (!found) {
            continue;
        }
        pch_cluster *c = &out->items[best_i];
        char **ns = realloc(c->sources, (c->n_sources + 1) * sizeof(char *));
        if (!ns) {
            return -1;
        }
        c->sources = ns;
        c->sources[c->n_sources++] = xstrdup(src);
    }

    for (size_t i = 0; i < out->count; i++) {
        if (out->items[i].n_sources > 1) {
            qsort(out->items[i].sources, out->items[i].n_sources, sizeof(char *), cmp_strptr);
        }
    }

    /* Number by descending major (= header count) */
    if (out->count > 1) {
        qsort(out->items, out->count, sizeof(pch_cluster), cmp_cluster_major);
    }
    char *prefix_base = path_basename(prefix);
    if (!prefix_base || !*prefix_base) {
        free(prefix_base);
        prefix_base = xstrdup("cluster");
    }
    for (size_t i = 0; i < out->count; i++) {
        pch_cluster *c = &out->items[i];
        unsigned k = (unsigned)(i + 1);
        if (asprintf(&c->path, "%s%u%s", prefix, k, ext) < 0) {
            c->path = NULL;
        }
        if (asprintf(&c->filename, "%s%u%s", prefix_base, k, ext) < 0) {
            c->filename = NULL;
        }
    }
    free(prefix_base);

    loginfo_fmt("built %zu cluster(s)", out->count);
    return 0;
}
