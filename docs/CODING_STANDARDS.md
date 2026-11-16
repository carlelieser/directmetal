# Coding standards

These rules apply to every change. Architecture decisions live in the [ADRs](adr/README.md) and are linked, not restated, here. A rule changes only through a pull request that edits this file.

## Languages and boundaries

- C++20, with Objective-C++ confined to the Metal backend ([ADR 0003](adr/0003-language.md)).
- Wine dependencies and the PE↔unix boundary follow [ADR 0002](adr/0002-delivery-model.md) and [ADR 0008](adr/0008-wine-bridge-module.md).
- Python 3.12 for tooling under `tools/`.

## License

- Never copy or translate LGPL/GPL code ([ADR 0001](adr/0001-license.md)).
- Permissively licensed code may be incorporated or adapted under ADR 0001, keeping its notices. Adapted code lives with first-party sources, e.g. DXIL reader code derived from DirectXShaderCompiler in `src/` ([ADR 0006](adr/0006-shader-pipeline.md)), and keeps its original copyright header. Vendored, unmodified copies live under `third_party/`.

## Formatting

- First-party sources are formatted with `clang-format` (C++ and Objective-C++, root `.clang-format`) and `ruff format` (Python, `ruff.toml`). No other formatter is used.
- Vendored code under `third_party/` and generated files such as `docs/api/` are exempt.
- Python also passes `ruff check`.
- Formatting changes go in their own commit, separate from behavior changes.

## Shape

- Functions: at most 40 lines, 3 parameters and 3 levels of nesting. A named domain type counts as one parameter; a struct that only bundles arguments counts as its fields.
- COM methods whose signature is fixed by the Direct3D or DXGI headers are exempt from the parameter limit, and only from that limit. Their bodies validate arguments and delegate.
- Files: at most 300 lines. Split by responsibility, not by line count.
- Conditions: at most 2 boolean operators. Name the sub-expressions.
- Data belongs in data structures. Format tables, capability tables and state mappings are `constexpr` arrays or maps, not `switch` statements spread across functions.

## Naming

- Types: `PascalCase`. Functions, variables and namespaces: `snake_case`. Macros: `DIRECTMETAL_UPPER_CASE`, and only where a macro is unavoidable.
- COM interfaces, their methods and Windows types keep their SDK names (`ID3D11Device::CreateBuffer`, `DXGI_FORMAT`). Metal types keep their Apple names.
- No single-letter names except loop indices. No type suffixes (`format_enum`, `desc_struct`).
- Booleans start with `is`, `has` or `can`.
- No module, file or namespace named `utils`, `helpers`, `common` or `misc`.
- Use the vocabulary of the API being implemented: Direct3D terms in front ends, Metal terms in the backend.
- Match the vocabulary already used in the codebase.

## Error handling

- `HRESULT` is the typed failure for COM methods and for internal functions that feed them. Return the code Windows returns for the same input; the Windows reference ([ADR 0007](adr/0007-testing-strategy.md)) decides when documentation is silent.
- No exception crosses a COM method or the bridge boundary. COM methods and bridge entry points are `noexcept`; internal code that can throw (allocation, standard library) is caught at that boundary and converted (`std::bad_alloc` → `E_OUTOFMEMORY`).
- Unimplemented methods return `E_NOTIMPL` and log the interface and method once. They never return `S_OK`.
- No empty or log-only `catch`, and no `catch` that returns a default hiding the failure. Catch only what this layer can convert or handle.
- Log and error messages name the operation and its identifying input (interface, method, format, size).
- Python tooling raises exceptions that name the operation and input, and exits non-zero on failure.

## Organization

- Dependencies point one way: API front ends → shared translation services → Metal backend. Only the bridge (`directmetal.dll` / `directmetal.so`) touches Wine. No circular dependencies between directories.
- Public headers live under `include/directmetal/` in each component. Everything else is private to its component.
- Shared libraries build with hidden symbol visibility. Exports are marked explicitly (`DIRECTMETAL_UNIX_API`) or listed in the DLL's `.def` file.

## Testing

Tiers, gates, Metal validation and tolerances follow [ADR 0007](adr/0007-testing-strategy.md). In addition:

- A bug fix's regression test fails without the fix.
- Before claiming a test works, break the implementation and confirm the test fails.

## Comments

- Comments explain why, only when it is not obvious from the code.
- Keep them short. Link a specification section, issue or URL instead of explaining at length.
- Cite the documented or observed Windows behavior a workaround reproduces, never another project's source.
