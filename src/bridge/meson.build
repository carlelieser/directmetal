bridge_protocol_dep = declare_dependency(
    include_directories: bridge_include,
    dependencies: backend_types_dep,
)

subdir('client')

# bridge_transport_dep provides directmetal_bridge_call: directmetal.dll's import library in PE
# builds, the native in-process bridge in macOS builds.
if is_windows
    subdir('pe')
    bridge_transport_dep = directmetal_bridge_dep
else
    subdir('native')
endif
