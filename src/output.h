#ifndef OUTPUT_H
#define OUTPUT_H

#include "cluster.h"
#include "includes.h"

/* Write cluster files using each cluster's path field. */
int output_write_clusters(const pch_cluster_list *list, const include_index *ix);

/* Write map as src:clusterK.h lines (sorted by source path). */
int output_write_map(const pch_cluster_list *list, const char *map_path);

/* Update sources: rewrite existing pch include in place, or prepend if absent.
 * prefix_base is the basename of -o PREFIX (e.g. "cluster"). */
int output_write_sources(const pch_cluster_list *list, const char *prefix_base, const char *ext);

#endif /* OUTPUT_H */
