d3d11_dll = shared_library(
    'd3d11',
    'd3d11.cpp',
    link_whole: pe_dll_main,
    name_prefix: '',
    dependencies: frontend_markers_dep,
    vs_module_defs: 'd3d11.def',
    link_args: pe_link_args,
)
