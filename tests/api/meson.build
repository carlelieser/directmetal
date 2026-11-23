unix_library = get_option('unix_library')
if unix_library == ''
    unix_library = meson.project_source_root() / 'build' / 'unix' / 'src' / 'unix' / 'directmetal.so'
endif

api_call_log_dir = meson.current_build_dir() / 'call-logs'
api_arch = host_machine.cpu_family() == 'x86_64' ? 'win64' : 'win32'

d3d11_create_device = executable(
    'd3d11_create_device',
    'd3d11_create_device.cpp',
    include_directories: bridge_include,
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
