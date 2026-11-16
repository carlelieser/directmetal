pe_dll_main = static_library('pe_dll_main', 'dll_main.cpp')

# Static runtime linking keeps the DLLs free of MinGW runtime DLL imports.
pe_link_args = ['-static']
if host_machine.cpu_family() == 'x86'
    # Export stdcall functions under their undecorated names, as Windows DLLs do.
    pe_link_args += '-Wl,--kill-at'
endif

foreach dll : ['d3d9', 'd3d10core', 'd3d10', 'd3d12', 'dxgi']
    shared_library(
        dll,
        link_whole: pe_dll_main,
        name_prefix: '',
        vs_module_defs: dll / dll + '.def',
        link_args: pe_link_args,
    )
endforeach
