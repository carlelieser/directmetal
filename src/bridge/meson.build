bridge_protocol_dep = declare_dependency(
    include_directories: bridge_include,
    dependencies: backend_types_dep,
)

subdir('client')

# bridge_transport_dep provides directmetal_bridge_call: directmetal.dll's import library in PE
# builds, the native in-process bridge (src/bridge/native, built after src/unix) in macOS builds.
if is_windows
    subdir('pe')
    bridge_transport_dep = directmetal_bridge_dep
endif
