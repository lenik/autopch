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
#include "compiler.h"
#include "config.h"
#include "includes.h"
#include "output.h"
#include "scan.h"

#include <bas/locale/i18n.h>
#include <bas/log/deflog.h>

#include <getopt.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bas/proc/env.h>


define_logger();

enum {
    OPT_VERSION = 256,
    OPT_CC,
    OPT_CFLAGS,
    OPT_DEFINE,
    OPT_OUTPUT,
};

static void usage(FILE *out) {
    fputs(_("Usage: autopch [OPTION]... FILE...\n"
            "Analyze C/C++ sources and emit PCH header clusters by co-occurrence.\n"),
          out);
    fputs("\n", out);
    fputs("  -x                  ", out);
    fputs(_("analyze as C++ (default: C)\n"), out);
    fputs("  -c, --min-correlation N  ", out);
    fputs(_("minimum co-occurrence for an edge (default 3)\n"), out);
    fputs("  -I DIR              ", out);
    fputs(_("add include search path\n"), out);
    fputs("  -D, --define NAME[=VAL]  ", out);
    fputs(_("define a preprocessor macro\n"), out);
    fputs("      --cc NAME       ", out);
    fputs(_("compiler used to load builtin macros\n"), out);
    fputs("      --cflags FLAGS  ", out);
    fputs(_("parse flags and merge -I/-D/-isystem\n"), out);
    fputs("  -R, --recursive-include  ", out);
    fputs(_("recursively follow #include\n"), out);
    fputs("  -r, --recursive     ", out);
    fputs(_("recurse into directories for sources\n"), out);
    fputs("  -d, --dag           ", out);
    fputs(_("order headers by include dependency DAG\n"), out);
    fputs("  -a, --all           ", out);
    fputs(_("also analyze local #include \"...\" (ignored by default)\n"), out);
    fputs("  -o, --output PREFIX ", out);
    fputs(_("output path prefix (default ./cluster)\n"), out);
    fputs("  -m, --map FILE      ", out);
    fputs(_("write source-to-cluster map\n"), out);
    fputs("  -w, --write         ", out);
    fputs(_("insert/rewrite #include \"PREFIXK.ext\" in sources\n"), out);
    fputs("  -v, --verbose       ", out);
    fputs(_("repeat for more verbose loggings\n"), out);
    fputs("  -q, --quiet         ", out);
    fputs(_("show less logging messages\n"), out);
    fputs("  -h, --help          ", out);
    fputs(_("display this help and exit\n"), out);
    fputs("      --version       ", out);
    fputs(_("output version information and exit\n"), out);
    fputs("\n", out);
    fprintf(out, _("Report bugs to: <%s>\n"), PROJECT_EMAIL);
}

int main(int argc, char **argv) {
    const char *exe = self_exe();
    init_i18n(LOCALEDIR);
    (void)exe;

    lang_t lang = LANG_C;
    int min_corr = 3;
    int recursive_include = 0;
    int recursive_dirs = 0;
    int use_dag = 0;
    int all_includes = 0;
    int do_write = 0;
    const char *prefix = "./cluster";
    const char *map_path = NULL;
    const char *cc = NULL;
    const char *cflags = NULL;

    compile_flags cf;
    compile_flags_init(&cf);

    static const struct option long_opts[] = {
        {"min-correlation", required_argument, NULL, 'c'},
        {"define", required_argument, NULL, OPT_DEFINE},
        {"cc", required_argument, NULL, OPT_CC},
        {"cflags", required_argument, NULL, OPT_CFLAGS},
        {"recursive-include", no_argument, NULL, 'R'},
        {"recursive", no_argument, NULL, 'r'},
        {"dag", no_argument, NULL, 'd'},
        {"all", no_argument, NULL, 'a'},
        {"output", required_argument, NULL, 'o'},
        {"map", required_argument, NULL, 'm'},
        {"write", no_argument, NULL, 'w'},
        {"verbose", no_argument, NULL, 'v'},
        {"quiet", no_argument, NULL, 'q'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, OPT_VERSION},
        {NULL, 0, NULL, 0},
    };

    for (;;) {
        int c = getopt_long(argc, argv, "c:I:D:Rrdo:m:wvqhxa", long_opts, NULL);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 'x':
            lang = LANG_CXX;
            break;
        case 'a':
            all_includes = 1;
            break;
        case 'c':
            min_corr = atoi(optarg);
            if (min_corr < 1) {
                fprintf(stderr, "%s: --min-correlation must be >= 1\n", argv[0]);
                compile_flags_clear(&cf);
                return 1;
            }
            break;
        case 'I':
            compile_flags_add_include(&cf, optarg);
            break;
        case 'D':
        case OPT_DEFINE:
            compile_flags_add_define(&cf, optarg);
            break;
        case OPT_CC:
            cc = optarg;
            break;
        case OPT_CFLAGS:
            cflags = optarg;
            break;
        case 'R':
            recursive_include = 1;
            break;
        case 'r':
            recursive_dirs = 1;
            break;
        case 'd':
            use_dag = 1;
            break;
        case 'o':
            prefix = optarg;
            break;
        case 'm':
            map_path = optarg;
            break;
        case 'w':
            do_write = 1;
            break;
        case 'v':
            log_more();
            break;
        case 'q':
            log_less();
            break;
        case 'h':
            usage(stdout);
            compile_flags_clear(&cf);
            return 0;
        case OPT_VERSION:
            printf("autopch %s\n", PROJECT_VERSION);
            printf(_("Copyright (C) %d %s\n"), PROJECT_YEAR, PROJECT_AUTHOR);
            fputs(_("License AGPL-3.0-or-later: <https://www.gnu.org/licenses/agpl-3.0.html>\n"),
                  stdout);
            fputs(_("This is free software: you are free to change and redistribute it.\n"),
                  stdout);
            fputs(_("This project opposes AI exploitation and AI hegemony.\n"), stdout);
            fputs(_("This project rejects mindless MIT-style licensing and politically naive "
                    "BSD-style licensing.\n"),
                  stdout);
            fputs(_("There is NO WARRANTY, to the extent permitted by law.\n"), stdout);
            compile_flags_clear(&cf);
            return 0;
        default:
            usage(stderr);
            compile_flags_clear(&cf);
            return 1;
        }
    }

    if (cflags) {
        if (compile_flags_parse_cflags(&cf, cflags) != 0) {
            fprintf(stderr, "%s: failed to parse --cflags\n", argv[0]);
            compile_flags_clear(&cf);
            return 1;
        }
    }
    if (cc) {
        if (compiler_load_builtins(&cf, cc, lang) != 0) {
            logwarn("continuing without compiler builtins");
        }
    }

    argc -= optind;
    argv += optind;
    if (argc < 1) {
        fprintf(stderr, "autopch: no input files\n");
        usage(stderr);
        compile_flags_clear(&cf);
        return 1;
    }

    GPtrArray *sources = g_ptr_array_new_with_free_func(free);
    if (scan_collect_sources(sources, argv, argc, lang, recursive_dirs) != 0) {
        g_ptr_array_free(sources, TRUE);
        compile_flags_clear(&cf);
        return 1;
    }
    if (sources->len == 0) {
        fprintf(stderr, "autopch: no source files found\n");
        g_ptr_array_free(sources, TRUE);
        compile_flags_clear(&cf);
        return 1;
    }
    loginfo_fmt("analyzing %u source file(s)", sources->len);

    include_index ix;
    include_index_init(&ix);
    for (guint i = 0; i < sources->len; i++) {
        const char *src = g_ptr_array_index(sources, i);
        if (includes_analyze_source(&ix, src, &cf, recursive_include, all_includes) != 0) {
            logerror_fmt("failed analyzing %s", src);
            include_index_clear(&ix);
            g_ptr_array_free(sources, TRUE);
            compile_flags_clear(&cf);
            return 1;
        }
    }

    const char *ext = (lang == LANG_CXX) ? ".hpp" : ".h";
    pch_cluster_list clusters;
    if (cluster_build(&clusters, &ix, min_corr, use_dag, prefix, ext) != 0) {
        include_index_clear(&ix);
        g_ptr_array_free(sources, TRUE);
        compile_flags_clear(&cf);
        return 1;
    }

    char *prefix_base = path_basename(prefix);
    int rc = 0;
    if (output_write_clusters(&clusters, &ix) != 0) {
        rc = 1;
    }
    if (rc == 0 && output_write_map(&clusters, map_path) != 0) {
        rc = 1;
    }
    if (rc == 0 && do_write) {
        if (output_write_sources(&clusters, prefix_base, ext) != 0) {
            rc = 1;
        }
    }

    free(prefix_base);
    pch_cluster_list_clear(&clusters);
    include_index_clear(&ix);
    g_ptr_array_free(sources, TRUE);
    compile_flags_clear(&cf);
    return rc;
}
