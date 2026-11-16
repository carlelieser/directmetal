pe_dll_main = files('dll_main.cpp')

# Static runtime linking keeps the DLLs free of MinGW runtime DLL imports.
pe_link_args = ['-static']

foreach dll : ['d3d9', 'd3d10core', 'd3d10', 'd3d11', 'd3d12', 'dxgi']
    subdir(dll)
endforeach
