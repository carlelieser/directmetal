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
        # DirectX-Headers always comes from the pinned wrap, never from the system.
        'force_fallback_for=DirectX-Headers',
    ],
)

is_windows = host_machine.system() == 'windows'
is_darwin = host_machine.system() == 'darwin'
if not (is_windows or is_darwin)
    error('unsupported host system: ' + host_machine.system())
endif

if is_darwin
    # Objective-C++ is confined to the Metal backend (ADR 0003).
    add_languages('objcpp', native: false)
    macos_min_version = '-mmacosx-version-min=14.0'
    add_global_arguments(macos_min_version, language: ['cpp', 'objcpp'])
    add_global_link_arguments(macos_min_version, language: ['cpp', 'objcpp'])
endif

python = find_program('python3', version: '>=3.11')
bridge_include = include_directories('src/bridge/include')
backend_include = include_directories('src/backend/include')
# The backend's plain C++ types, shared with the bridge protocol and the format tables.
backend_types_dep = declare_dependency(include_directories: backend_include)

subdir('tools/bridge')
if is_windows
    subdir('src/pe')
endif
subdir('src/runtime')
subdir('src/memory')
subdir('src/formats')
subdir('src/win32')
subdir('src/bridge')
if is_darwin
    subdir('src/backend')
    subdir('src/unix')
    subdir('src/bridge/native')
endif
subdir('src/frontends')
subdir('tests')
