"""Existing Python command entry, forwarded to the production native SDK CLI."""
import os
from pathlib import Path
import subprocess
import sys
from core_host import find_core
from native_sdk import create_runtime, NativeSDKError


def main(argv=None):
    arguments = list(sys.argv[1:] if argv is None else argv)
    selected = None
    if '--core-executable' in arguments:
        index = arguments.index('--core-executable')
        selected = arguments[index + 1]
        del arguments[index:index + 2]
    arguments = ['--workspace' if value == '--core-workspace' else value for value in arguments]
    if '--native-descriptor' not in arguments and os.environ.get('CYCODE_BRIDGE'):
        arguments[:0] = ['--native-descriptor', os.environ['CYCODE_BRIDGE']]
    for flag in ('--desktop-service', '--core-service'):
        if flag in arguments:
            arguments.remove(flag); arguments.append('serve')
    data = None
    if '--data-dir' in arguments: data = Path(arguments[arguments.index('--data-dir') + 1])
    if data is not None:
        from scripts.migrate_legacy_jobs import migrate
        migrate(data)
    environment = dict(os.environ)
    # Retain the old helper's asynchronous MCP surface unless explicitly upgraded.
    environment.setdefault('CYCODE_MCP_PROFILE', 'code')
    command = [str(find_core(selected)), '--sdk', '--legacy-cli', *arguments]
    return subprocess.run(command, env=environment, check=False).returncode


if __name__ == '__main__':
    raise SystemExit(main())
