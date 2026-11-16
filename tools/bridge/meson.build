mark_builtin_script = files('mark_builtin.py')

if host_machine.system() == 'darwin'
    test(
        'mark_builtin',
        python,
        args: ['-I', '-m', 'unittest', 'discover', '-s', meson.current_source_dir() / 'tests'],
        suite: 'tools',
        timeout: 30,
    )
endif
