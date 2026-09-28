#ifndef SCAN_H
#define SCAN_H

#include "compiler.h"

#include <glib.h>

/* Append matching source paths into out (GPtrArray of char*).
 * recursive: walk directories; otherwise skip directory args with a warning. */
int scan_collect_sources(GPtrArray *out, char **paths, int npaths, lang_t lang, int recursive);

int scan_is_source_name(const char *name, lang_t lang);

#endif /* SCAN_H */
