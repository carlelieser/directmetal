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

if host_machine.system() == 'darwin'
    macos_min_version = '-mmacosx-version-min=14.0'
    add_project_arguments(macos_min_version, language: 'cpp')
    add_project_link_arguments(macos_min_version, language: 'cpp')
    subdir('src/unix')
else
    error('unsupported host system: ' + host_machine.system())
endif
