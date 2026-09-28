#ifndef CFLAGS_H
#define CFLAGS_H

#include <glib.h>

typedef struct {
    GPtrArray *include_dirs; /* char* owned */
    GHashTable *defines;     /* name -> value ("" if bare -DNAME); owned */
} compile_flags;

void compile_flags_init(compile_flags *cf);
void compile_flags_clear(compile_flags *cf);
void compile_flags_add_include(compile_flags *cf, const char *dir);
void compile_flags_add_define(compile_flags *cf, const char *spec);
/* Parse a cflags string and merge -I / -isystem / -D into cf. Returns 0 on ok. */
int compile_flags_parse_cflags(compile_flags *cf, const char *cflags);

#endif /* CFLAGS_H */
