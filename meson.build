project(
    'pchgreat',
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
)

project_author = 'Lenik'
project_email = 'pchgreat@bodz.net'
project_year = 2026

py = import('python').find_installation('python3')
fs = import('fs')
asciidoctor = find_program('asciidoctor', required: true)

prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
datadir = prefix / get_option('datadir')
localedir = prefix / get_option('localedir')
mandir = prefix / get_option('mandir')
pkgdocdir = datadir / 'doc' / meson.project_name()

subdir('po')

pchgreat_exe = custom_target(
    'pchgreat',
    input: 'src/pchgreat.py',
    output: 'pchgreat',
    command: [
        py,
        '-c',
        'import os, shutil, sys; shutil.copyfile(sys.argv[1], sys.argv[2]); os.chmod(sys.argv[2], 0o755)',
        '@INPUT@',
        '@OUTPUT@',
    ],
    build_by_default: true,
    install: true,
    install_dir: bindir,
)

commons_mod = custom_target(
    'commons',
    input: 'src/commons.py',
    output: 'commons.py',
    command: ['cp', '@INPUT@', '@OUTPUT@'],
    build_by_default: true,
    install: true,
    install_dir: bindir,
)

install_data(
    'pchgreat.bash',
    install_dir: datadir / 'bash-completion' / 'completions',
    rename: 'pchgreat',
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
    'pchgreat-man',
    input: 'man/pchgreat.adoc',
    output: 'pchgreat.1',
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

test(
    'python-unit',
    py,
    args: ['-m', 'unittest', 'discover', '-s', (meson.project_source_root() / 'tests'), '-p', 'test_*.py'],
    env: [
        'PYTHONPATH=' + (meson.project_source_root() / 'src'),
        'PCHGREAT_LOCALEDIR=' + (meson.project_build_root() / 'po'),
    ],
)

run_target(
    'posync',
    command: [
        find_program('zfr'),
        'translate',
        '--sync',
        '-C', meson.project_source_root(),
    ],
)

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
        '-euc', '''
            prefix="@0@"
            bindir="@1@"
            datadir="@2@"
            mandir="@3@"
            mkdir -p "$bindir" "$datadir/bash-completion/completions" "$mandir/man1"
            sudo ln -sfn "@BUILD_ROOT@/pchgreat" "$bindir/pchgreat"
            sudo ln -sfn "@BUILD_ROOT@/pchgreat.1" "$mandir/man1/pchgreat.1"
            sudo ln -sfn "@SOURCE_ROOT@/pchgreat.bash" "$datadir/bash-completion/completions/pchgreat"
            printf "Symlinks installed under %s\n" "$prefix"
        '''.format(get_option('prefix'), bindir, datadir, mandir),
    ],
)

run_target(
    'uninstall-symlinks',
    command: [
        'bash',
        '-euc', '''
            prefix="@0@"
            bindir="@1@"
            datadir="@2@"
            mandir="@3@"
            for p in "$bindir/pchgreat" "$mandir/man1/pchgreat.1" "$datadir/bash-completion/completions/pchgreat"
            do
                if [ -L "$p" ]; then
                    sudo rm -f "$p"
                    printf "Removed %s\n" "$p"
                else
                    printf "Skipped (not a symlink): %s\n" "$p"
                fi
            done
        '''.format(get_option('prefix'), bindir, datadir, mandir),
    ],
)
