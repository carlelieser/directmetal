# API tests (suite 'api'). PE builds run the programs under Wine; macOS builds run them natively
# through the in-process bridge. Exit tests (docs/plans/phase-1.md#exit-test) and the validation
# canary go through run_api_test.py, which enables Metal validation in assert mode, fails on any
# abnormal exit and compares the output with the Windows reference.
fs = import('fs')
api_runner = files('run_api_test.py')
exit_reference = files('../reference/d3d11_clear_readback/reference.json')

# Phase 1 wave 4 sets this once the D3D11 front end (package I) is wired into the exit program.
# Until then the exit tests are registered but report a skip.
is_exit_test_wired = false
exit_test_pending = 'D3D11 front end not wired into the exit test yet (Phase 1 wave 4)'

# Native runs, in every macOS build (unix, unix-arm64, sanitize). The exit test runs once
# is_exit_test_wired is set and this block enters programs/ first, which then builds
# d3d11_clear_readback for macOS against the D3D11 front end and the native bridge (needs
# has_native_win32_headers from package E and the front end from package I). macOS builds reach
# this file only once tests/meson.build enters tests/api instead of tests/api/tests.
if not is_windows
    subdir('tests')
    native_exit_args = ['--mode', 'native', '--reference', exit_reference]
    if not is_exit_test_wired
        native_exit_args += ['--program', 'd3d11_clear_readback', '--skip', exit_test_pending]
    elif not is_variable('d3d11_clear_readback')
        native_exit_args += [
            '--program', 'd3d11_clear_readback',
            '--skip', 'd3d11_clear_readback does not build natively yet',
        ]
    else
        native_exit_args += ['--program', d3d11_clear_readback]
    endif
    test(
        'd3d11_clear_readback_native',
        python,
        args: ['-I', api_runner] + native_exit_args,
        suite: 'api',
        is_parallel: false,
        timeout: 180,
    )
    subdir_done()
endif

unix_library = get_option('unix_library')
if unix_library == ''
    unix_library = meson.project_source_root() / 'build' / 'unix' / 'src' / 'unix' / 'directmetal.so'
endif

api_call_log_dir = meson.current_build_dir() / 'call-logs'
api_arch = host_machine.cpu_family() == 'x86_64' ? 'win64' : 'win32'

d3d11_create_device = executable(
    'd3d11_create_device',
    'd3d11_create_device.cpp',
    dependencies: bridge_protocol_dep,
    link_args: '-static',
)

static_library(
    'markers_header_check',
    'markers_header_check.cpp',
    dependencies: frontend_markers_dep,
)

d3d11_create_device_args = [
    files('run_under_wine.py'),
    '--arch', api_arch,
    '--program', d3d11_create_device,
    '--dll', d3d11_dll,
    '--builtin', directmetal_builtin_dll,
]

test(
    'd3d11_create_device',
    python,
    args: d3d11_create_device_args + [
        '--unix-library', unix_library,
        '--program-arg', 'unix',
        '--call-log-dir', api_call_log_dir / 'd3d11_create_device',
        '--work-dir', meson.current_build_dir() / 'd3d11_create_device',
        '--expect', 'directmetal: call D3D11CreateDevice',
        '--expect', 'directmetal: call D3D11CreateDeviceAndSwapChain',
    ],
    suite: 'api',
    timeout: 180,
)

# Without directmetal.so, Wine fails to load the unix library with STATUS_DLL_NOT_FOUND; the stubs
# must still return E_NOTIMPL and report that status on the PE side.
test(
    'd3d11_create_device_without_unix_library',
    python,
    args: d3d11_create_device_args + [
        '--program-arg', 'no-unix',
        '--call-log-dir', api_call_log_dir / 'd3d11_create_device_without_unix_library',
        '--work-dir', meson.current_build_dir() / 'd3d11_create_device_without_unix_library',
        '--expect', 'directmetal: stub D3D11CreateDevice not logged: bridge call failed with status 0xc0000135',
        '--expect', 'directmetal: stub D3D11CreateDeviceAndSwapChain not logged: bridge call failed with status 0xc0000135',
    ],
    suite: 'api',
    timeout: 180,
)

# Phase 0 exit gate (docs/ROADMAP.md) on the logs of the test that reaches the unix side. Runs
# alone after the Wine tests have written their logs.
test(
    'd3d11_status_gate',
    python,
    args: [
        '-I', files('../../tools/api/report_status.py'),
        '--call-log-dir', api_call_log_dir / 'd3d11_create_device',
        '--output', meson.current_build_dir() / 'api-status',
        '--require', 'd3d11:stub>=1',
        '--require', 'd3d11:tested>=1',
    ],
    suite: 'api',
    priority: -1,
    is_parallel: false,
    timeout: 60,
)

subdir('programs')

# The validation canary's unix library stands in for directmetal.so. Its first unix call trips
# Metal validation, which Wine reports to the PE side as status 0xc0000005, so the program fails;
# this proves the runner's validation settings reach Metal in a Wine process.
unix_build_dir = fs.parent(fs.parent(fs.parent(unix_library)))
validation_canary_unixlib = unix_build_dir / 'tests' / 'gpu' / 'validation_canary_unixlib.so'
wine_runner_args = [
    '-I', api_runner,
    '--mode', 'wine',
    '--arch', api_arch,
    '--dll', d3d11_dll,
    '--builtin', directmetal_builtin_dll,
]

test(
    'validation_canary_wine',
    python,
    args: wine_runner_args + [
        '--program', d3d11_create_device,
        '--program-arg', 'unix',
        '--unix-library', validation_canary_unixlib,
        '--work-dir', meson.current_build_dir() / 'validation_canary_wine',
    ],
    suite: 'api',
    should_fail: true,
    is_parallel: false,
    timeout: 180,
)

wine_exit_args = [
    '--program', d3d11_clear_readback,
    '--reference', exit_reference,
    '--unix-library', unix_library,
    '--call-log-dir', api_call_log_dir / 'd3d11_clear_readback',
    '--work-dir', meson.current_build_dir() / 'd3d11_clear_readback',
]
if not is_exit_test_wired
    wine_exit_args += ['--skip', exit_test_pending]
endif

test(
    'd3d11_clear_readback_wine',
    python,
    args: wine_runner_args + wine_exit_args,
    suite: 'api',
    is_parallel: false,
    timeout: 180,
)
