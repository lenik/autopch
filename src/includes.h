#ifndef INCLUDES_H
#define INCLUDES_H

#include "cflags.h"
#include "compiler.h"

#include <glib.h>

typedef struct {
    char *key;   /* canonical id: resolved path or "name"/<name> */
    char *spell; /* printable #include argument with quotes */
    int angle;   /* 1 if <> */
} header_info;

typedef struct {
    GHashTable *headers;      /* key -> header_info* (owned) */
    GHashTable *src_headers;  /* source path -> GHashTable set of header keys */
    GHashTable *dag_edges;    /* "from\0to" or pair string from->to (include dependency) */
    GHashTable *header_freq;  /* key -> intptr_t count across sources */
} include_index;

void include_index_init(include_index *ix);
void include_index_clear(include_index *ix);

/* Analyze one source file; updates index.
 * recursive_include: follow #include into headers.
 * all_includes: if 0, ignore #include "..." (local); only process <...>. */
int includes_analyze_source(include_index *ix, const char *source_path, const compile_flags *cf,
                            int recursive_include, int all_includes);

#endif /* INCLUDES_H */
