#ifndef COMMONS_H
#define COMMONS_H

#include <stddef.h>

char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);
char *path_join(const char *dir, const char *name);
char *path_dirname(const char *path);
char *path_basename(const char *path);
int path_is_dir(const char *path);
int path_is_file(const char *path);
char *path_realpath_or_dup(const char *path);
/* Ensure parent directory of path exists (mkdir -p style, one level or full). */
int path_ensure_parent(const char *path);

/* Shell-like tokenization; returns newly allocated NULL-terminated argv.
 * Caller frees with g_strfreev or free_argv. */
char **split_shell_words(const char *s, int *argc_out);
void free_argv(char **argv);

#endif /* COMMONS_H */
