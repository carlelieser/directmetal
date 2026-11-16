project(
    'directmetal',
    'cpp',
    version: '0.1.0',
    license: 'MIT',
    meson_version: '>=1.3.0',
    default_options: [
        'cpp_std=c++20',
        'warning_level=3',
        'werror=true',
        'b_ndebug=if-release',
    ],
)

bridge_include = include_directories('src/bridge/include')
python = find_program('python3')

subdir('tools/bridge')

if host_machine.system() == 'windows'
    subdir('src/pe')
    subdir('src/bridge/pe')
    subdir('src/frontends')
elif host_machine.system() == 'darwin'
    macos_min_version = '-mmacosx-version-min=14.0'
    add_global_arguments(macos_min_version, language: ['cpp', 'objcpp'])
    add_global_link_arguments(macos_min_version, language: ['cpp', 'objcpp'])
    subdir('src/unix')
    subdir('tests/unit')
else
    error('unsupported host system: ' + host_machine.system())
endif
