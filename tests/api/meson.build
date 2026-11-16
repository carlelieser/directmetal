unix_library = get_option('unix_library')
if unix_library == ''
    unix_library = meson.project_source_root() / 'build' / 'unix' / 'src' / 'unix' / 'directmetal.so'
endif

api_arch = host_machine.cpu_family() == 'x86_64' ? 'win64' : 'win32'

d3d11_create_device = executable(
    'd3d11_create_device',
    'd3d11_create_device.cpp',
    include_directories: bridge_include,
    link_args: '-static',
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
        '--work-dir', meson.current_build_dir() / 'd3d11_create_device_without_unix_library',
        '--expect', 'directmetal: stub D3D11CreateDevice not logged: bridge call failed with status 0xc0000135',
        '--expect', 'directmetal: stub D3D11CreateDeviceAndSwapChain not logged: bridge call failed with status 0xc0000135',
    ],
    suite: 'api',
    timeout: 180,
)
