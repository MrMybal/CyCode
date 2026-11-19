"""Deprecated command name; prepares only Manager stdlib, never a Python MCP SDK."""
import sys
from prepare_manager_runtime import main
if __name__ == '__main__':
    arguments = sys.argv[1:]
    for flag in ('--core-source', '--core-wheel'):
        if flag in arguments:
            i = arguments.index(flag)
            if i + 1 >= len(arguments): raise SystemExit(flag + ' needs a value')
            del arguments[i:i+2]
            print(flag + ' is obsolete: CyMCPSDK is compiled natively; the old Python payload is ignored.', file=sys.stderr)
    main(arguments)
