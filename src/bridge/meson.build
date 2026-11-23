bridge_protocol_dep = declare_dependency(
    include_directories: bridge_include,
    dependencies: backend_types_dep,
)

# Private to the bridge: rules shared by the PE and native transports.
bridge_transport_include = include_directories('transport')

subdir('client')

# bridge_transport_dep provides directmetal_bridge_call, directmetal_bridge_allocate_shared and
# directmetal_bridge_free_shared: directmetal.dll's import library in PE builds, the native
# in-process bridge (src/bridge/native, built after src/unix) in macOS builds.
if is_windows
    subdir('pe')
    bridge_transport_dep = directmetal_bridge_dep
endif
