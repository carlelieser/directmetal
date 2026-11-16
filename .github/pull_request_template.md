## Summary

<!-- What changed and why. Link the issue it resolves. -->

## Checklist

- [ ] Follows [the coding standards](https://github.com/carlelieser/directmetal/blob/main/docs/CODING_STANDARDS.md); formatted with `clang-format` / `ruff format`.
- [ ] Contains no code copied or translated from LGPL/GPL projects (Wine, vkd3d, vkd3d-proton, DXMT).
- [ ] Tests added at the lowest tier that exercises the change; bug fixes include a regression test that fails without the fix.
- [ ] GPU tests pass with `MTL_DEBUG_LAYER=1` and `MTL_SHADER_VALIDATION=1`.
- [ ] One logical change per commit; every commit builds and passes tests.
