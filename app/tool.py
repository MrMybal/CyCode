"""Compatibility entry point; all automation runs in the native CyMCPSDK host."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_sdk import NativeSDKError as CyToolError, NativeRuntime as Runtime, LocalClient, create_runtime
from native_client import NativeClient, native_parameters
from sdk_cli import main

if __name__ == '__main__':
    raise SystemExit(main())
