"""CLI safety contracts: no GUI, installer, native fixture or SDK is launched."""
import contextlib
import io
import os
from pathlib import Path
import runpy
import sys
import unittest
from unittest.mock import patch

APP = Path(__file__).resolve().parents[1]

class RunnerLaunchGuards(unittest.TestCase):
    def test_runner_checks_refuse_local_execution_before_spawning(self):
        cases = [
            ('verify_graphical_ci.py', ['--executable', 'not-used']),
            ('verify_installer_ci.py', ['--setup', 'not-used.exe']),
            ('verify_shortcuts.py', ['--ci', '--executable', 'not-used']),
        ]
        for name, arguments in cases:
            with self.subTest(script=name):
                api = runpy.run_path(str(APP / 'scripts' / name))
                with patch.dict(os.environ, {'GITHUB_ACTIONS': 'false'}), \
                     patch.object(sys, 'argv', [name, *arguments]), \
                     patch('subprocess.run') as execute, \
                     patch('subprocess.Popen') as spawn, \
                     contextlib.redirect_stderr(io.StringIO()):
                    with self.assertRaises(SystemExit) as result:
                        api['main']()
                    self.assertEqual(result.exception.code, 2)
                    execute.assert_not_called()
                    spawn.assert_not_called()

if __name__ == '__main__':
    unittest.main()
