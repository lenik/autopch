/*
 * Copyright (C) 2026 Lenik <autopch@bodz.net>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cflags.h"
#include "cluster.h"
#include "util.h"
#include "includes.h"
#include "output.h"
#include "scan.h"

#include <bas/log/deflog.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

define_logger();

static int failures;

static void expect_true(const char *name, int cond) {
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", name);
        failures++;
    }
}

static void expect_eq_int(const char *name, int got, int want) {
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %d want %d\n", name, got, want);
        failures++;
    }
}

static void expect_eq_size(const char *name, size_t got, size_t want) {
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %zu want %zu\n", name, got, want);
        failures++;
    }
}

static void expect_eq_str(const char *name, const char *got, const char *want) {
    if (!got || strcmp(got, want) != 0) {
        fprintf(stderr, "FAIL %s: got %s want %s\n", name, got ? got : "(null)", want);
        failures++;
    }
}

static void test_cflags_parse(void) {
    compile_flags cf;
    compile_flags_init(&cf);
    expect_eq_int("parse cflags",
                  compile_flags_parse_cflags(&cf, "-I/inc -DFOO=1 -isystem /sys -DBAR"), 0);
    expect_eq_int("includes count", (int)cf.include_dirs->len, 2);
    expect_true("has FOO", g_hash_table_contains(cf.defines, "FOO"));
    expect_true("has BAR", g_hash_table_contains(cf.defines, "BAR"));
    expect_true("FOO val", strcmp(g_hash_table_lookup(cf.defines, "FOO"), "1") == 0);
    compile_flags_clear(&cf);
}

static int cluster_has_header_named(const pch_cluster *c, const include_index *ix,
                                    const char *needle) {
    for (size_t i = 0; i < c->n_headers; i++) {
        const header_info *hi = g_hash_table_lookup(ix->headers, c->headers[i]);
        if (hi && hi->spell && strstr(hi->spell, needle)) {
            return 1;
        }
        if (strstr(c->headers[i], needle)) {
            return 1;
        }
    }
    return 0;
}

static void test_correlation_clusters(void) {
    compile_flags cf;
    compile_flags_init(&cf);
    compile_flags_add_include(&cf, "fixtures/inc");

    char *paths[] = {
        "fixtures/src_a/s1.c", "fixtures/src_a/s2.c", "fixtures/src_a/s3.c",
        "fixtures/src_b/t1.c", "fixtures/src_b/t2.c", "fixtures/src_b/t3.c",
    };
    GPtrArray *sources = g_ptr_array_new();
    for (size_t i = 0; i < 6; i++) {
        g_ptr_array_add(sources, paths[i]);
    }

    include_index ix;
    include_index_init(&ix);
    for (guint i = 0; i < sources->len; i++) {
        expect_eq_int("analyze", includes_analyze_source(&ix, sources->pdata[i], &cf, 0, 1), 0);
    }

    char tmpl[] = "/tmp/autopch-test-XXXXXX";
    char *dir = mkdtemp(tmpl);
    expect_true("mkdtemp", dir != NULL);
    char *prefix = NULL;
    if (dir) {
        asprintf(&prefix, "%s/cluster", dir);
    }

    pch_cluster_list list;
    expect_eq_int("cluster_build", cluster_build(&list, &ix, 3, 0, prefix ? prefix : "./cluster", ".h"),
                  0);
    expect_eq_size("two clusters", list.count, 2);

    int saw_common = 0;
    int saw_other = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (cluster_has_header_named(&list.items[i], &ix, "common_a") &&
            cluster_has_header_named(&list.items[i], &ix, "common_b")) {
            saw_common = 1;
            expect_eq_size("common sources", list.items[i].n_sources, 3);
        }
        if (cluster_has_header_named(&list.items[i], &ix, "other_x") &&
            cluster_has_header_named(&list.items[i], &ix, "other_y")) {
            saw_other = 1;
            expect_eq_size("other sources", list.items[i].n_sources, 3);
        }
        /* both majors are 2; filenames must be cluster1/2 under prefix */
        expect_true("has filename", list.items[i].filename != NULL);
        expect_true("has path", list.items[i].path != NULL);
    }
    expect_true("common cluster", saw_common);
    expect_true("other cluster", saw_other);

    if (dir && prefix) {
        char map_path[256];
        snprintf(map_path, sizeof map_path, "%s/map.txt", dir);
        expect_eq_int("write clusters", output_write_clusters(&list, &ix), 0);
        expect_eq_int("write map", output_write_map(&list, map_path), 0);
        expect_true("cluster1 exists", path_is_file(list.items[0].path));
        FILE *fp = fopen(map_path, "r");
        expect_true("map open", fp != NULL);
        if (fp) {
            char buf[1024] = {0};
            size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
            fclose(fp);
            expect_true("map has src:cluster", n > 0 && strchr(buf, ':') && strstr(buf, "cluster"));
            expect_true("map not old format", !strstr(buf, "cluster1.h:"));
        }
    }

    free(prefix);
    pch_cluster_list_clear(&list);
    include_index_clear(&ix);
    g_ptr_array_free(sources, TRUE);
    compile_flags_clear(&cf);
}

static void test_major_ordering(void) {
    /* Build two clusters with different sizes via crafted include_index is hard;
     * instead verify cmp through cluster_build when one component has more headers.
     * Use fixtures: add a third header to src_a only across 3 files. */
    compile_flags cf;
    compile_flags_init(&cf);
    compile_flags_add_include(&cf, "fixtures/inc");

    /* Create temp sources: group A has 3 headers co-occurring, group B has 2 */
    char tmpl[] = "/tmp/autopch-major-XXXXXX";
    char *dir = mkdtemp(tmpl);
    expect_true("major mkdtemp", dir != NULL);
    if (!dir) {
        compile_flags_clear(&cf);
        return;
    }
    char *incdir = NULL;
    asprintf(&incdir, "%s/inc", dir);
    mkdir(incdir, 0755);
    const char *hdrs_a[] = {"a1.h", "a2.h", "a3.h"};
    const char *hdrs_b[] = {"b1.h", "b2.h"};
    for (int i = 0; i < 3; i++) {
        char *p = NULL;
        asprintf(&p, "%s/%s", incdir, hdrs_a[i]);
        FILE *fp = fopen(p, "w");
        fprintf(fp, "#define A%d 1\n", i);
        fclose(fp);
        free(p);
    }
    for (int i = 0; i < 2; i++) {
        char *p = NULL;
        asprintf(&p, "%s/%s", incdir, hdrs_b[i]);
        FILE *fp = fopen(p, "w");
        fprintf(fp, "#define B%d 1\n", i);
        fclose(fp);
        free(p);
    }
    compile_flags_add_include(&cf, incdir);

    char *srcs[6];
    for (int i = 0; i < 3; i++) {
        asprintf(&srcs[i], "%s/a%d.c", dir, i);
        FILE *fp = fopen(srcs[i], "w");
        fputs("#include \"a1.h\"\n#include \"a2.h\"\n#include \"a3.h\"\n", fp);
        fclose(fp);
    }
    for (int i = 0; i < 3; i++) {
        asprintf(&srcs[3 + i], "%s/b%d.c", dir, i);
        FILE *fp = fopen(srcs[3 + i], "w");
        fputs("#include \"b1.h\"\n#include \"b2.h\"\n", fp);
        fclose(fp);
    }

    include_index ix;
    include_index_init(&ix);
    for (int i = 0; i < 6; i++) {
        includes_analyze_source(&ix, srcs[i], &cf, 0, 1);
    }

    char *prefix = NULL;
    asprintf(&prefix, "%s/cluster", dir);
    pch_cluster_list list;
    expect_eq_int("major build", cluster_build(&list, &ix, 3, 0, prefix, ".h"), 0);
    expect_eq_size("major two", list.count, 2);
    if (list.count == 2) {
        expect_eq_size("K1 is largest", list.items[0].n_headers, 3);
        expect_eq_size("K2 is smaller", list.items[1].n_headers, 2);
        expect_eq_str("K1 name", list.items[0].filename, "cluster1.h");
        /* filename strips path prefix dir/ */
        expect_true("K1 path ends cluster1.h", list.items[0].path && strstr(list.items[0].path, "cluster1.h"));
    }

    for (int i = 0; i < 6; i++) {
        free(srcs[i]);
    }
    free(prefix);
    free(incdir);
    pch_cluster_list_clear(&list);
    include_index_clear(&ix);
    compile_flags_clear(&cf);
}

static void test_dag_order(void) {
    compile_flags cf;
    compile_flags_init(&cf);
    compile_flags_add_include(&cf, "fixtures/dag");

    const char *paths[] = {"fixtures/dag/d1.c", "fixtures/dag/d2.c", "fixtures/dag/d3.c"};
    include_index ix;
    include_index_init(&ix);
    for (size_t i = 0; i < 3; i++) {
        expect_eq_int("dag analyze", includes_analyze_source(&ix, paths[i], &cf, 1, 1), 0);
    }

    pch_cluster_list list;
    expect_eq_int("dag cluster", cluster_build(&list, &ix, 3, 1, "./cluster", ".h"), 0);
    expect_true("dag has cluster", list.count >= 1);
    if (list.count >= 1) {
        const pch_cluster *c = &list.items[0];
        expect_true("dag has 2 headers", c->n_headers >= 2);
        int base_i = -1;
        int mid_i = -1;
        for (size_t i = 0; i < c->n_headers; i++) {
            if (strstr(c->headers[i], "base.h")) {
                base_i = (int)i;
            }
            if (strstr(c->headers[i], "mid.h")) {
                mid_i = (int)i;
            }
        }
        expect_true("found base and mid", base_i >= 0 && mid_i >= 0);
        expect_true("base before mid", base_i < mid_i);
    }

    pch_cluster_list_clear(&list);
    include_index_clear(&ix);
    compile_flags_clear(&cf);
}

static void test_write_rewrite(void) {
    char tmpl[] = "/tmp/autopch-write-XXXXXX";
    char *dir = mkdtemp(tmpl);
    expect_true("write mkdtemp", dir != NULL);
    if (!dir) {
        return;
    }
    char src_path[256];
    snprintf(src_path, sizeof src_path, "%s/x.c", dir);
    FILE *fp = fopen(src_path, "w");
    expect_true("create src", fp != NULL);
    if (!fp) {
        return;
    }
    fputs("/* keep */\n#include \"cluster9.h\"\nint x;\n", fp);
    fclose(fp);

    pch_cluster_list list = {0};
    list.count = 1;
    list.items = calloc(1, sizeof(pch_cluster));
    list.items[0].filename = xstrdup("cluster1.h");
    list.items[0].path = xstrdup("./cluster1.h");
    list.items[0].n_sources = 1;
    list.items[0].sources = calloc(1, sizeof(char *));
    list.items[0].sources[0] = xstrdup(src_path);

    expect_eq_int("rewrite", output_write_sources(&list, "cluster", ".h"), 0);

    fp = fopen(src_path, "r");
    expect_true("reopen", fp != NULL);
    if (fp) {
        char buf[256] = {0};
        fread(buf, 1, sizeof(buf) - 1, fp);
        fclose(fp);
        /* comment stays first; include line rewritten in place */
        expect_true("keeps comment first", strncmp(buf, "/* keep */", 10) == 0);
        expect_true("rewrote to cluster1", strstr(buf, "#include \"cluster1.h\"") != NULL);
        expect_true("removed cluster9", strstr(buf, "cluster9.h") == NULL);
        expect_true("keeps body", strstr(buf, "int x;") != NULL);
    }

    /* second write is idempotent and keeps position */
    expect_eq_int("rewrite twice", output_write_sources(&list, "cluster", ".h"), 0);
    fp = fopen(src_path, "r");
    if (fp) {
        char buf[256] = {0};
        fread(buf, 1, sizeof(buf) - 1, fp);
        fclose(fp);
        expect_true("still comment first", strncmp(buf, "/* keep */", 10) == 0);
        int count = 0;
        const char *p = buf;
        while ((p = strstr(p, "#include \"cluster1.h\"")) != NULL) {
            count++;
            p++;
        }
        expect_eq_int("include once", count, 1);
    }

    /* prepend when absent */
    snprintf(src_path, sizeof src_path, "%s/y.c", dir);
    fp = fopen(src_path, "w");
    fputs("int y;\n", fp);
    fclose(fp);
    free(list.items[0].sources[0]);
    list.items[0].sources[0] = xstrdup(src_path);
    expect_eq_int("prepend", output_write_sources(&list, "cluster", ".h"), 0);
    fp = fopen(src_path, "r");
    if (fp) {
        char buf[256] = {0};
        fread(buf, 1, sizeof(buf) - 1, fp);
        fclose(fp);
        expect_true("prepended", strncmp(buf, "#include \"cluster1.h\"", 21) == 0);
    }

    pch_cluster_list_clear(&list);
}

static void test_scan_recursive(void) {
    GPtrArray *out = g_ptr_array_new_with_free_func(free);
    char *args[] = {"fixtures/src_a"};
    expect_eq_int("scan r", scan_collect_sources(out, args, 1, LANG_C, 1), 0);
    expect_eq_size("three sources", out->len, 3);
    g_ptr_array_free(out, TRUE);

    out = g_ptr_array_new_with_free_func(free);
    expect_eq_int("scan no-r", scan_collect_sources(out, args, 1, LANG_C, 0), 0);
    expect_eq_size("ignore dir", out->len, 0);
    g_ptr_array_free(out, TRUE);
}

static void test_ignore_quoted_without_all(void) {
    compile_flags cf;
    compile_flags_init(&cf);
    compile_flags_add_include(&cf, "fixtures/inc");

    include_index ix;
    include_index_init(&ix);
    expect_eq_int("analyze no-all",
                  includes_analyze_source(&ix, "fixtures/src_a/s1.c", &cf, 0, 0), 0);
    GHashTable *set = g_hash_table_lookup(ix.src_headers, "fixtures/src_a/s1.c");
    expect_true("has src set", set != NULL);
    expect_eq_size("no quoted headers", set ? g_hash_table_size(set) : 0, 0);
    include_index_clear(&ix);

    include_index_init(&ix);
    expect_eq_int("analyze all",
                  includes_analyze_source(&ix, "fixtures/src_a/s1.c", &cf, 0, 1), 0);
    set = g_hash_table_lookup(ix.src_headers, "fixtures/src_a/s1.c");
    expect_true("has src set all", set != NULL);
    expect_eq_size("quoted counted", set ? g_hash_table_size(set) : 0, 2);
    include_index_clear(&ix);
    compile_flags_clear(&cf);
}

int main(void) {
    test_cflags_parse();
    test_scan_recursive();
    test_ignore_quoted_without_all();
    test_correlation_clusters();
    test_major_ordering();
    test_dag_order();
    test_write_rewrite();
    return failures == 0 ? 0 : 1;
}
