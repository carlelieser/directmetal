ntdll_dep = meson.get_compiler('cpp').find_library('ntdll')

directmetal_dll = shared_library(
    'directmetal',
    'bridge.cpp',
    name_prefix: '',
    include_directories: bridge_transport_include,
    dependencies: [bridge_protocol_dep, ntdll_dep],
    vs_module_defs: 'directmetal.def',
    link_args: pe_link_args,
)

directmetal_bridge_dep = declare_dependency(
    link_with: directmetal_dll,
    dependencies: bridge_protocol_dep,
)

subdir('builtin')
