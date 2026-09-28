test_inc = include_directories('../src', '..')
test_deps = [dependency('bas-c')]
test_args = ['-DLOGGER_NAME=autopch', '-DTEXT_DOMAIN=autopch', '-D_GNU_SOURCE']

common_src = [
    '../src/util.c',
    '../src/cflags.c',
    '../src/compiler.c',
    '../src/scan.c',
    '../src/includes.c',
    '../src/cluster.c',
    '../src/output.c',
]

autopch_test = executable(
    'autopch_test',
    ['autopch_test.c'] + common_src,
    include_directories: test_inc,
    dependencies: test_deps,
    c_args: test_args,
    install: false,
)

test(
    'autopch',
    autopch_test,
    workdir: meson.current_source_dir(),
)
