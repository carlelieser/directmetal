# API tests (suite 'api'). PE builds run the programs under Wine; macOS builds run them natively
# through the in-process bridge. Exit tests (docs/plans/phase-1.md#exit-test) and the validation
# canary go through run_api_test.py, which enables Metal validation in assert mode, fails on any
# abnormal exit and compares the output with the Windows reference.
api_runner = files('run_api_test.py')
exit_reference = files('../reference/d3d11_clear_readback/reference.json')

# Exit tests skip when this is false, for bring-up of a new front end.
is_exit_test_wired = true
exit_test_pending = 'D3D11 front end not wired into the exit test'
# The exit program reaches 8 D3D11 entry points and all three IUnknown methods.
exit_status_requirements = ['--require', 'd3d11:tested>=8', '--require', 'common:tested>=3']

# Native runs, in every macOS build (unix, unix-arm64, sanitize). The exit program links the D3D11
# front end and the native bridge directly.
if not is_windows
    subdir('tests')
    native_harness_dep = declare_dependency(
        sources: files('harness/output.cpp', 'harness/options.cpp', 'harness/process_native.cpp'),
        include_directories: include_directories('harness/include'),
    )
    d3d11_clear_readback = executable(
        'd3d11_clear_readback',
        'programs/d3d11_clear_readback.cpp',
        dependencies: [native_harness_dep, d3d11_frontend_dep],
    )
    native_call_logs = meson.current_build_dir() / 'call-logs' / 'd3d11_clear_readback'
    native_exit_args = ['--mode', 'native', '--reference', exit_reference]
    native_exit_args += ['--program', d3d11_clear_readback, '--call-log-dir', native_call_logs]
    if not is_exit_test_wired
        native_exit_args += ['--skip', exit_test_pending]
    endif
    test(
        'd3d11_clear_readback_native',
        python,
        args: ['-I', api_runner] + native_exit_args,
        suite: 'api',
        is_parallel: false,
        timeout: 180,
    )
    bridge_ring_roundtrip = executable(
        'bridge_ring_roundtrip',
        'programs/bridge_ring_roundtrip.cpp',
        dependencies: [bridge_client_dep, bridge_transport_dep],
    )
    test(
        'bridge_ring_roundtrip_native',
        python,
        args: ['-I', api_runner, '--mode', 'native', '--program', bridge_ring_roundtrip],
        suite: 'api',
        is_parallel: false,
        timeout: 180,
    )
    if is_exit_test_wired
        test(
            'd3d11_exit_status_gate',
            python,
            args: [
                '-I', files('../../tools/api/report_status.py'),
                '--call-log-dir', native_call_logs,
                '--output', meson.current_build_dir() / 'exit-status',
            ] + exit_status_requirements,
            suite: 'api',
            priority: -1,
            is_parallel: false,
            timeout: 60,
        )
    endif
    subdir_done()
endif

# tools/build.sh builds the unix side here before the PE targets.
unix_build_dir = meson.project_source_root() / 'build' / 'unix'
unix_library = get_option('unix_library')
if unix_library == ''
    unix_library = unix_build_dir / 'src' / 'unix' / 'directmetal.so'
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
        '--expect', 'directmetal: call D3D11CreateDeviceAndSwapChain',
    ],
    suite: 'api',
    timeout: 180,
)

# Without directmetal.so, Wine fails to load the unix library with STATUS_DLL_NOT_FOUND. The PE side
# must fail cleanly: D3D11CreateDevice returns DXGI_ERROR_UNSUPPORTED after logging the failed
# initialize, and the stub still returns E_NOTIMPL and reports that status.
test(
    'd3d11_create_device_without_unix_library',
    python,
    args: d3d11_create_device_args + [
        '--program-arg', 'no-unix',
        '--call-log-dir', api_call_log_dir / 'd3d11_create_device_without_unix_library',
        '--work-dir', meson.current_build_dir() / 'd3d11_create_device_without_unix_library',
        '--expect', 'directmetal: warn bridge: initialize failed with status 0xc0000135',
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

test(
    'bridge_ring_roundtrip_wine',
    python,
    args: [
        '-I', api_runner,
        '--mode', 'wine',
        '--arch', api_arch,
        '--program', bridge_ring_roundtrip,
        '--builtin', directmetal_builtin_dll,
        '--unix-library', unix_library,
        '--work-dir', meson.current_build_dir() / 'bridge_ring_roundtrip_wine',
    ],
    suite: 'api',
    is_parallel: false,
    timeout: 180,
)

# The validation canary's unix library stands in for directmetal.so. Its first unix call trips
# Metal validation, which Wine reports to the PE side as status 0xc0000005, so the program fails.
# The runner passes only on that failure with the debug layer's message on stderr, which proves
# its validation settings reach Metal in a Wine process.
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
        '--expect-abort-stderr', 'failed assertion `Copy From Buffer Validation',
    ],
    suite: 'api',
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

# Phase 1 exit gate (docs/ROADMAP.md) on the exit test's call logs.
if is_exit_test_wired
    test(
        'd3d11_exit_status_gate',
        python,
        args: [
            '-I', files('../../tools/api/report_status.py'),
            '--call-log-dir', api_call_log_dir / 'd3d11_clear_readback',
            '--output', meson.current_build_dir() / 'exit-status',
        ] + exit_status_requirements,
        suite: 'api',
        priority: -1,
        is_parallel: false,
        timeout: 60,
    )
endif
