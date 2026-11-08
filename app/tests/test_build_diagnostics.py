"""Data-only build reporting checks: no compiler, native fixture or SDK process."""
from pathlib import Path
import runpy
import tempfile
import unittest

api = runpy.run_path(str(Path(__file__).resolve().parents[1]/'scripts/build.py'))


class BuildDiagnosticsTests(unittest.TestCase):
    def test_clang_error_and_candidates_are_visible(self):
        output = api['build_failure_excerpt']('''[1/2] Compile UnrealProject.cpp
FAILED: UnrealProject.cpp.o
/usr/bin/clang++ -c UnrealProject.cpp
UnrealProject.cpp:81:16: error: reference to 'environment' is ambiguous
    auto value = environment("HOME");
                 ^
note: candidate function is here
native_file.h:10:1: note: candidate declaration
ninja: build stopped: subcommand failed.
''')
        self.assertIn("reference to 'environment' is ambiguous", output)
        self.assertIn('candidate function', output)

    def test_cmake_generator_reason_is_visible(self):
        output = api['build_failure_excerpt']('''CMake Error at CMakeLists.txt:2 (project):
  Generator

    Visual Studio 17 2022

  could not find any instance of Visual Studio.
''')
        self.assertIn('could not find any instance of Visual Studio', output)

    def test_msvc_error_retains_source_context(self):
        output = api['build_failure_excerpt']('''Compile item
sample.cpp(8,2): error C2143: syntax error: missing ';'
next source line
    function declaration
''')
        self.assertIn('error C2143', output)
        self.assertIn('function declaration', output)

    def test_output_is_bounded_for_many_errors_and_huge_lines(self):
        output = api['build_failure_excerpt'](('file.cpp:1: error: '+('x'*500)+'\n')*1000,
                                             max_lines=12,max_chars=1200)
        self.assertLessEqual(len(output),1200)
        self.assertLessEqual(len(output.splitlines()),12)
        self.assertIn('full build log',output)

    def test_unknown_failure_keeps_the_end_of_the_log(self):
        output = api['build_failure_excerpt']('\n'.join(f'line {i}' for i in range(100)))
        self.assertNotIn('line 0\n',output)
        self.assertIn('line 99',output)
        self.assertEqual(len(output.splitlines()),25)

    def test_large_log_reads_only_a_bounded_tail(self):
        workspace=Path(__file__).resolve().parents[2]/'runtime/test-workspaces/build-reporting'
        workspace.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='text-only-',dir=workspace) as directory:
            path=Path(directory)/'build.log'
            self.assertTrue(path.resolve().is_relative_to(workspace.resolve()))
            path.write_bytes(b'old output\n'*10000+b'last.cpp:4: error: expected expression\n')
            text=api['build_log_tail'](path,maximum=2048)
            self.assertLessEqual(len(text),2100)
            self.assertIn('earlier build output omitted',text)
            self.assertIn('expected expression',api['build_failure_excerpt'](text))


if __name__ == '__main__':
    unittest.main()
