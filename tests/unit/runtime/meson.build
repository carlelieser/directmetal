# Unit tests for src/runtime (suite 'unit': no GPU required).
runtime_tests = executable(
    'runtime_tests',
    'config_test.cpp',
    'hud_test.cpp',
    'log_test.cpp',
    'log_file_test.cpp',
    'log_spec_test.cpp',
    dependencies: [runtime_dep, gtest_dep],
)

test('runtime', runtime_tests, protocol: 'gtest', suite: 'unit', timeout: 30)

# process_config resolves once per process, so each test runs in its own process. They share the
# executable's directory for directmetal.conf, so they do not run in parallel.
process_config_tests = executable(
    'runtime_process_tests',
    'process_config_test.cpp',
    dependencies: [runtime_dep, gtest_dep],
)

foreach name : [
    'AppliesTheSectionOfTheRunningExecutable',
    'SectionsOfOtherExecutablesDoNotApply',
    'EnvironmentOverridesBeatTheFile',
    'ReadsTheFileNextToTheExecutable',
    'ExplicitFileReplacesTheDefaultFile',
    'AbsentDefaultFileIsSilent',
    'MissingExplicitFileWarns',
    'UnreadableExplicitFileWarns',
    'ResolvesOnceAcrossThreads',
]
    test(
        'runtime_process_' + name,
        process_config_tests,
        args: ['--gtest_filter=ProcessConfigTest.' + name],
        env: {'DIRECTMETAL_TEST_EXECUTABLE_DIRECTORY': meson.current_build_dir()},
        protocol: 'gtest',
        suite: 'unit',
        timeout: 30,
        is_parallel: false,
    )
endforeach
