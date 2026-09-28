#ifndef CLUSTER_H
#define CLUSTER_H

#include "includes.h"

#include <glib.h>

typedef struct {
    char **headers; /* keys */
    size_t n_headers;
    char **sources;
    size_t n_sources;
    char *path;     /* output path, e.g. ./cluster1.h */
    char *filename; /* include / map name, e.g. cluster1.h */
} pch_cluster;

typedef struct {
    pch_cluster *items;
    size_t count;
} pch_cluster_list;

void pch_cluster_list_clear(pch_cluster_list *list);

/* Build clusters. Prefix default "./cluster"; ext ".h" or ".hpp".
 * Clusters are numbered K=1.. by descending header count (major). */
int cluster_build(pch_cluster_list *out, include_index *ix, int min_corr, int use_dag,
                  const char *prefix, const char *ext);

#endif /* CLUSTER_H */
