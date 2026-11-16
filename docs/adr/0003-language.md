# 0003 — Implementation language

Status: accepted

## Context

Direct3D interfaces are COM vtables, which C++ classes model directly. Metal is an Objective-C API; Metal 4 types appear first in the Objective-C headers.

## Decision

- C++20 for all code.
- Objective-C++ (`.mm`, ARC enabled) only inside the Metal backend, behind a C++ interface. No Objective-C types appear in backend headers.
- PE-side code is plain C++ with no Apple framework dependencies.

## Consequences

- The backend uses every Metal API on SDK release day without waiting for wrapper updates.
- Front ends and shader compiler are testable without Objective-C.
