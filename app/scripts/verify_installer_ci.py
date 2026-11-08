"""Install/repair/uninstall an owned Windows runner fixture; never a local app."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
UNINSTALL_KEY = r'Software\Microsoft\Windows\CurrentVersion\Uninstall\{308D75A7-6921-47C0-A6A4-B21769A5487E}_is1'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'win32' or os.environ.get('GITHUB_ACTIONS') != 'true':
        parser.error('Installer acceptance is restricted to an explicit Windows GitHub Actions job.')
    import winreg
    def registration():
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, UNINSTALL_KEY, 0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
                return {name: winreg.QueryValueEx(key, name)[0] for name in ('DisplayVersion', 'InstallLocation')}
        except FileNotFoundError:
            return None
    assert registration() is None, 'Preserve an existing registered CyCode installation'
    setup = args.setup.resolve(strict=True)
    assert setup.is_relative_to(ROOT / 'runtime/releases')
    version = json.loads((ROOT/'app/CyTool.json').read_text('utf-8'))['version']
    assert setup.name == 'CyCode-' + version + '-windows-x64-setup.exe'
    reports = ROOT / 'runtime/reports/installer-ci'
    reports.mkdir(parents=True, exist_ok=True)
    parent = ROOT / 'runtime/test-installations'
    parent.mkdir(parents=True, exist_ok=True)
    # Do not erase a failed fixture automatically. Its installer log/contents are evidence.
    destination = Path(tempfile.mkdtemp(prefix='CyCode-', dir=parent)).resolve()
    sequence = 0
    def install(language='english', expected=True):
        nonlocal sequence
        sequence += 1
        result = subprocess.run([str(setup), '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-',
            '/DIR=' + str(destination), '/LANG=' + language,
            '/LOG=' + str(reports / ('setup-' + str(sequence) + '.log'))], timeout=180)
        assert (result.returncode == 0) == expected, ('Setup result', result.returncode, expected)
    def verify():
        manifest = json.loads((destination/'manifest.json').read_text('utf-8'))
        assert manifest['version'] == version
        for item in manifest['files']:
            path = destination / item['path']
            assert path.resolve(strict=True).is_relative_to(destination) and not path.is_symlink()
            data = path.read_bytes()
            assert len(data) == item['size'] and hashlib.sha256(data).hexdigest() == item['sha256'], item['path']
        assert (destination/'installed.flag').read_bytes() == b'CyCode per-user installation\n'
        assert (destination/'msvcp140.dll').is_file() and (destination/'vcruntime140.dll').is_file()
        assert (destination/'app/Documentation/Documentation.html').is_file()
        entry = registration()
        assert entry['DisplayVersion'] == version and Path(entry['InstallLocation']).resolve() == destination
        return hashlib.sha256((destination/'manifest.json').read_bytes()).hexdigest()
    install()
    baseline = verify()
    user = destination/'data/settings/installer-ci.txt'
    user.parent.mkdir(parents=True, exist_ok=True)
    user.write_bytes(b'Owned runner settings: preserve across repair and uninstall.\n')
    retained = user.read_bytes()
    # Hold the same shared ownership file as a running application.
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
        ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    lease = kernel.CreateFileW(str(destination/'data/updates/instances.lock'), 0x80000000, 1, None, 3, 0x80, None)
    assert lease not in (None, ctypes.c_void_p(-1).value), ctypes.get_last_error()
    try:
        install(expected=False)
    finally:
        assert kernel.CloseHandle(lease)
    assert verify() == baseline and user.read_bytes() == retained
    pending = destination/'data/updates/pending.json'
    assert not pending.exists()
    pending.write_bytes(b'{}')
    install(expected=False)
    assert pending.read_bytes() == b'{}' and verify() == baseline
    pending.unlink()
    install('french')
    assert verify() == baseline and user.read_bytes() == retained
    subprocess.run([str(destination/'tools/python/python.exe'),'-B','-X','utf8',
        str(destination/'app/scripts/verify_mcp_runtime_ci.py'),'--installation',str(destination),
        '--fixture-root',str(parent),'--reports',str(ROOT/'runtime/reports/mcp-ci')],check=True,timeout=180)
    assert verify() == baseline and user.read_bytes() == retained
    uninstallers = list(destination.glob('unins*.exe'))
    assert len(uninstallers) == 1
    result = subprocess.run([str(uninstallers[0]), '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART',
        '/LOG=' + str(reports/'uninstall.log')], timeout=180)
    assert result.returncode == 0 and registration() is None
    assert not (destination/'CyCode.exe').exists() and not (destination/'installed.flag').exists()
    assert user.read_bytes() == retained
    (reports/'result.json').write_text(json.dumps({'version': version, 'initialInstall': True,
        'ownedPayloadVerified': True, 'busyInstallationRefused': True, 'pendingRecoveryRefused': True,
        'frenchRepair': True, 'installedMcpRuntime': True, 'uninstall': True, 'userDataPreserved': True}, indent=2) + '\n', 'utf-8')
    print('Windows runner install, payload verification, guarded repair and uninstall passed. Application/update execution is separate.')

if __name__ == '__main__':
    main()
