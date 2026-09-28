#ifndef COMPILER_H
#define COMPILER_H

#include "cflags.h"

typedef enum { LANG_NONE = 0, LANG_C = 1, LANG_CXX = 2 } lang_t;

/* Query gcc/clang-compatible compiler for builtin macros into cf->defines.
 * Returns 0 on success, -1 on failure. */
int compiler_load_builtins(compile_flags *cf, const char *cc, lang_t lang);

#endif /* COMPILER_H */
