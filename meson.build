project(
    'autopch',
    'c',
    version: run_command(
        'sh',
        '-c', '''
        v=$(
            command -v zfr >/dev/null 2>&1 && zfr version 2>/dev/null || true
        )
        if [ -z "$v" ] && [ -f VERSION ]; then
            v=$(head -n1 VERSION | tr -d '\r')
            v=${v#v}
        fi
        if [ -z "$v" ]; then
            v="0.0.0" # FIXED TO 0.0.0, DO NOT MODIFY
        fi
        printf '%s' "$v"
        ''',
        check: false,
    ).stdout().strip(),
    license: 'AGPL-3.0-or-later',
    default_options: [
        'c_std=c99',
        'warning_level=2',
    ],
)

project_author = 'Lenik'
project_email = 'autopch@bodz.net'
project_year = 2026

asciidoctor = find_program('asciidoctor', required: true)
fs = import('fs')

prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
datadir = prefix / get_option('datadir')
mandir = prefix / get_option('mandir')
localedir = prefix / get_option('localedir')
pkgdocdir = datadir / 'doc' / meson.project_name()

config_h = configuration_data()
config_h.set_quoted('PROJECT_VERSION', meson.project_version())
config_h.set_quoted('PROJECT_AUTHOR', project_author)
config_h.set_quoted('PROJECT_EMAIL', project_email)
config_h.set('PROJECT_YEAR', project_year)
config_h.set_quoted('LOCALEDIR', localedir)

configure_file(
    output: 'config.h',
    configuration: config_h,
)

bas_c_dep = dependency('bas-c', required: true)
bas_args = [
    '-DLOGGER_NAME=' + meson.project_name(),
    '-DTEXT_DOMAIN=' + meson.project_name(),
    '-D_GNU_SOURCE',
]

inc = include_directories('src', '.')

common_sources = [
    'src/util.c',
    'src/cflags.c',
    'src/compiler.c',
    'src/scan.c',
    'src/includes.c',
    'src/cluster.c',
    'src/output.c',
]

app_sources = [
    'src/autopch.c',
] + common_sources

cc = meson.get_compiler('c')

# Flags passed to `ninja pch` so analysis matches this build.
pch_cflag_list = []
pch_cflag_list += run_command(
    find_program('pkg-config'),
    '--cflags',
    'bas-c',
    check: true,
).stdout().strip().split()
pch_cflag_list += bas_args
pch_cflag_list += [
    '-I' + (meson.project_source_root() / 'src'),
    '-I' + meson.project_build_root(),
]
pch_cflags = ' '.join(pch_cflag_list)

autopch_exe = executable(
    'autopch',
    app_sources,
    include_directories: inc,
    dependencies: [bas_c_dep],
    c_args: bas_args,
    c_pch: 'src/pch1.h',
    install: true,
    install_dir: bindir,
)

# Regenerate src/pchK.h from project sources (angle-bracket includes only).
run_target(
    'pch',
    command: [
        autopch_exe,
        '--cc', cc.cmd_array()[0],
        '--cflags', pch_cflags,
        '-o', meson.project_source_root() / 'src' / 'pch',
        '-r',
        meson.project_source_root() / 'src',
    ],
)

install_data(
    'autopch.bash',
    install_dir: datadir / 'bash-completion' / 'completions',
    rename: 'autopch',
)

install_data(
    [
        'LICENSE',
        'README.md',
        'README-zh_CN.md',
    ],
    install_dir: pkgdocdir,
)

custom_target(
    'autopch-man',
    input: 'man/autopch.adoc',
    output: 'autopch.1',
    command: [
        asciidoctor,
        '-b', 'manpage',
        '-a', 'project-version=' + meson.project_version(),
        '-a', 'project-year=@0@'.format(project_year),
        '-a', 'project-author=' + project_author,
        '-a', 'project-email=' + project_email,
        '-o', '@OUTPUT@',
        '@INPUT@',
    ],
    build_by_default: true,
    install: true,
    install_dir: mandir / 'man1',
)

subdir('tests')

run_target(
    'look',
    command: [
        find_program('zfr'),
        'build',
        '--look',
        '-C', meson.project_source_root(),
        '-b', meson.project_build_root(),
    ],
)

run_target(
    'install-symlinks',
    command: [
        'bash',
        meson.project_source_root() / 'scripts' / 'install-symlinks.sh',
        meson.project_source_root(),
        meson.project_build_root(),
    ],
)

run_target(
    'uninstall-symlinks',
    command: [
        'bash',
        meson.project_source_root() / 'scripts' / 'uninstall-symlinks.sh',
        meson.project_source_root(),
        meson.project_build_root(),
    ],
)
