# Wine loads only the marked copy as builtin; the unmarked image exists for linking.
directmetal_builtin_dll = custom_target(
    'directmetal_builtin',
    input: directmetal_dll,
    output: 'directmetal.dll',
    command: [python, '-I', mark_builtin_script, '@INPUT@', '@OUTPUT@'],
    build_by_default: true,
)
