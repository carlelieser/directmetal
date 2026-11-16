# Coding standards

These rules apply to every change. Architecture decisions live in the [ADRs](adr/README.md) and are referenced, not restated, here. A rule changes only through a pull request that edits this file.

## Languages

- C++20 everywhere ([ADR 0003](adr/0003-language.md)).
- Objective-C++ (`.mm`, ARC) only under `src/backend`, behind a C++ interface. Backend headers contain no Objective-C types and no Apple framework includes.
- PE-side code (`src/pe`) is plain C++ with no Apple framework dependencies. API front ends and the Metal backend have no Wine dependency; only the platform bridge does ([ADR 0002](adr/0002-delivery-model.md)).
- Python 3.12 for tooling under `tools/`.

## License

DirectMetal is MIT ([ADR 0001](adr/0001-license.md)).

- Never copy or translate code from Wine, vkd3d, vkd3d-proton, DXMT or any other LGPL/GPL project, including by porting it line by line to another language or structure. Study their behavior and tests; implement from documentation, tests and observation.
- Permissively licensed code (MIT, zlib, BSD, NCSA, Apache-2.0) may be incorporated only under `third_party/` with its license notice intact.

## Formatting

- `clang-format` formats C++ and Objective-C++ using the root `.clang-format`. `ruff format` formats Python using `ruff.toml`. No other formatter is used and no file is exempt.
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

## Error handling

- `HRESULT` is the typed failure for COM methods and for internal functions that feed them. Return the code the Windows runtime returns for the same input; ADR 0007's Windows reference decides when documentation is silent.
- No exception crosses a COM method or the PE↔unix bridge. COM methods and bridge entry points are `noexcept`; internal code that can throw (allocation, standard library) is caught at that boundary and converted (`std::bad_alloc` → `E_OUTOFMEMORY`).
- Unimplemented methods return `E_NOTIMPL` and log the interface and method once. They never return `S_OK`.
- No empty or log-only `catch`, and no `catch` that returns a default hiding the failure. Catch only what this layer can convert or handle.
- Log and error messages name the operation and its identifying input (interface, method, format, size).
- Python tooling raises exceptions that name the operation and input, and exits non-zero on failure.

## Organization

- Layers depend only on layers above them, in the order given in the [roadmap](ROADMAP.md#architecture): platform bridge, Metal backend, shared translation services, API front ends. No circular dependencies between directories.
- Public headers live under `include/directmetal/` in each component. Everything else is private to its component.
- Shared libraries build with hidden symbol visibility. Exports are marked explicitly (`DIRECTMETAL_UNIX_API`) or listed in the DLL's `.def` file.

## Testing

Test tiers, gates and infrastructure are defined in [ADR 0007](adr/0007-testing-strategy.md). In addition:

- Tests assert observable behavior through public interfaces: COM methods, the backend's C++ interface, the shader compiler's input and output. Never internals.
- Every bug fix ships with a regression test at the lowest tier that reproduces it, and that test fails without the fix.
- Before claiming a test works, break the implementation and confirm the test fails.
- GPU tests run with `MTL_DEBUG_LAYER=1` and `MTL_SHADER_VALIDATION=1`. A validation error is a failure.
- Image tolerances are tightened, never loosened without review.

## Comments

- Comments explain why, only when it is not obvious from the code.
- Keep them short. Link a specification section, issue or URL instead of explaining at length.
- Cite the documented or observed Windows behavior a workaround reproduces, never another project's source.
