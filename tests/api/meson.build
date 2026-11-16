unix_library = get_option('unix_library')
if unix_library == ''
    unix_library = meson.project_source_root() / 'build' / 'unix' / 'src' / 'unix' / 'directmetal.so'
endif

api_arch = host_machine.cpu_family() == 'x86_64' ? 'win64' : 'win32'

d3d11_create_device = executable(
    'd3d11_create_device',
    'd3d11_create_device.cpp',
    link_args: '-static',
)

test(
    'd3d11_create_device',
    python,
    args: [
        files('run_under_wine.py'),
        '--arch', api_arch,
        '--program', d3d11_create_device,
        '--dll', d3d11_dll,
        '--builtin', directmetal_builtin_dll,
        '--unix-library', unix_library,
        '--work-dir', meson.current_build_dir() / 'd3d11_create_device',
        '--expect', 'directmetal: call D3D11CreateDevice',
        '--expect', 'directmetal: call D3D11CreateDeviceAndSwapChain',
    ],
    suite: 'api',
    timeout: 180,
)
