"""Optional CyCodeCore update companion; does not run or replace the Core itself."""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes as W
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading
import time
import webbrowser

HOME = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(HOME / 'app'))
import core_updates as updates

TEXT = {
    'en': {
        'title': 'CyCodeUpdater', 'check': 'Check for updates', 'install': 'Download and install',
        'release': 'GitHub release', 'close': 'Close', 'checking': 'Checking GitHub releases...',
        'current': 'CyCodeCore {current} is up to date.',
        'available': 'CyCodeCore {latest} is available. Installed: {current}.',
        'ready': 'Installed CyCodeCore: {current}. Updates are checked when this application starts.',
        'download': 'Downloading and verifying the installer...',
        'confirm': 'Download and install CyCodeCore {latest}?\n\nClose existing Core/MCP clients before continuing. The installer opens after SHA-256 verification. Installation requires no administrator account.',
        'offered': 'CyCodeCore {latest} is available (installed: {current}).\n\nOpen CyCodeUpdater to review and install it?',
        'error': 'The update could not be completed.\n\n{error}',
        'unsupported': 'The updater window is currently available on Windows. Use --check for a JSON result.',
    },
    'fr': {
        'title': 'CyCodeUpdater', 'check': 'Rechercher les mises à jour', 'install': 'Télécharger et installer',
        'release': 'Version GitHub', 'close': 'Fermer', 'checking': 'Recherche des versions GitHub...',
        'current': 'CyCodeCore {current} est à jour.',
        'available': 'CyCodeCore {latest} est disponible. Version installée : {current}.',
        'ready': 'CyCodeCore installé : {current}. Les mises à jour sont vérifiées au démarrage de cette application.',
        'download': 'Téléchargement et vérification de l’installateur...',
        'confirm': 'Télécharger et installer CyCodeCore {latest} ?\n\nFermez les clients Core/MCP ouverts avant de continuer. L’installateur s’ouvre après vérification SHA-256. Aucun compte administrateur n’est nécessaire.',
        'offered': 'CyCodeCore {latest} est disponible (version installée : {current}).\n\nOuvrir CyCodeUpdater pour consulter et installer cette version ?',
        'error': 'La mise à jour n’a pas pu être terminée.\n\n{error}',
        'unsupported': 'La fenêtre de mise à jour est disponible sur Windows. Utilisez --check pour un résultat JSON.',
    },
}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--home', type=Path, default=HOME)
    parser.add_argument('--cache', type=Path)
    parser.add_argument('--language', choices=('en', 'fr'), default='en')
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--check', action='store_true', help='Check now and print JSON, without opening the interface')
    modes.add_argument('--smoke-ui', action='store_true', help='Create and close the updater controls without showing a window or checking the network')
    modes.add_argument('--background', action='store_true', help='Daily check; offer the interface only when newer Core is available')
    args = parser.parse_args(argv)
    text = TEXT[args.language]
    cache = args.cache or updates.cache_directory()
    if args.check:
        print(json.dumps(updates.check(args.home, cache=cache, force=True), ensure_ascii=False))
        return 0
    if os.name != 'nt':
        raise updates.UpdateError(text['unsupported'])
    user = ctypes.WinDLL('user32', use_last_error=True)
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    gdi = ctypes.WinDLL('gdi32', use_last_error=True)
    user.MessageBoxW.argtypes = [W.HWND, W.LPCWSTR, W.LPCWSTR, W.UINT]
    user.MessageBoxW.restype = ctypes.c_int
    if args.background:
        try:
            result = updates.check(args.home, cache=cache)
            if result['status'] != 'available':
                return 0
            offered = cache / 'offered.json'
            previous = json.loads(offered.read_text('utf-8')) if offered.is_file() and offered.stat().st_size < 4096 else {}
            if (previous.get('version') == result['release']['version'] and
                    0 <= time.time() - previous.get('at', 0) < updates.CHECK_INTERVAL_SECONDS):
                return 0
            updates.atomic_json(offered, {'version': result['release']['version'], 'at': time.time()})
            prompt = text['offered'].format(latest=result['release']['version'], current=result['installedVersion'])
            if user.MessageBoxW(None, prompt, text['title'], 0x24) != 6:
                return 0
        except Exception:
            # Optional login checks must not interrupt startup or leak credentials.
            return 0
    manifest = updates.installation(args.home)
    try:
        user.SetProcessDPIAware()
    except AttributeError:
        pass
    scale = max(1.0, float(user.GetDpiForSystem()) / 96.0)
    pixels = lambda value: round(value * scale)
    CALLBACK = ctypes.WINFUNCTYPE(ctypes.c_ssize_t, W.HWND, W.UINT, W.WPARAM, W.LPARAM)

    class WindowClass(ctypes.Structure):
        _fields_ = [('style', W.UINT), ('procedure', CALLBACK), ('class_extra', ctypes.c_int),
                    ('window_extra', ctypes.c_int), ('instance', W.HINSTANCE), ('icon', W.HICON),
                    ('cursor', W.HANDLE), ('background', W.HBRUSH), ('menu', W.LPCWSTR), ('name', W.LPCWSTR)]

    user.DefWindowProcW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
    user.DefWindowProcW.restype = ctypes.c_ssize_t
    user.CreateWindowExW.argtypes = [W.DWORD, W.LPCWSTR, W.LPCWSTR, W.DWORD, ctypes.c_int, ctypes.c_int,
                                     ctypes.c_int, ctypes.c_int, W.HWND, W.HMENU, W.HINSTANCE, W.LPVOID]
    user.CreateWindowExW.restype = W.HWND
    user.RegisterClassW.argtypes = [ctypes.POINTER(WindowClass)]
    user.RegisterClassW.restype = W.ATOM
    kernel.GetModuleHandleW.argtypes = [W.LPCWSTR]
    kernel.GetModuleHandleW.restype = W.HMODULE
    user.SendMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
    user.SendMessageW.restype = ctypes.c_ssize_t
    user.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
    user.PostMessageW.restype = W.BOOL
    user.SetWindowTextW.argtypes = [W.HWND, W.LPCWSTR]
    user.EnableWindow.argtypes = [W.HWND, W.BOOL]
    user.DestroyWindow.argtypes = [W.HWND]
    user.ShowWindow.argtypes = [W.HWND, ctypes.c_int]
    user.GetMessageW.argtypes = [ctypes.POINTER(W.MSG), W.HWND, W.UINT, W.UINT]
    user.GetMessageW.restype = W.BOOL
    user.IsDialogMessageW.argtypes = [W.HWND, ctypes.POINTER(W.MSG)]
    user.TranslateMessage.argtypes = [ctypes.POINTER(W.MSG)]
    user.DispatchMessageW.argtypes = [ctypes.POINTER(W.MSG)]
    user.DispatchMessageW.restype = ctypes.c_ssize_t
    gdi.GetStockObject.argtypes = [ctypes.c_int]
    gdi.GetStockObject.restype = W.HGDIOBJ
    gdi.CreateFontW.argtypes = [ctypes.c_int] * 5 + [W.DWORD] * 8 + [W.LPCWSTR]
    gdi.CreateFontW.restype = W.HFONT
    owned_font = gdi.CreateFontW(-pixels(14), 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 5, 0, 'Segoe UI')
    font = owned_font or gdi.GetStockObject(17)
    instance = kernel.GetModuleHandleW(None)
    results = queue.Queue()
    selected = None
    handles = {}
    worker = None
    stop = threading.Event()
    busy = False
    WM_RESULT = 0x8001

    def label(value):
        user.SetWindowTextW(handles['status'], value)

    def enable():
        user.EnableWindow(handles['check'], not busy)
        user.EnableWindow(handles['install'], not busy and selected is not None)
        user.EnableWindow(handles['release'], not busy and selected is not None)

    def begin(hwnd, kind):
        nonlocal busy, worker
        if busy:
            return
        if kind == 'download':
            prompt = text['confirm'].format(latest=selected['version'])
            if user.MessageBoxW(hwnd, prompt, text['title'], 0x124) != 6:
                return
        busy = True
        label(text['checking' if kind == 'check' else 'download'])
        enable()
        def task():
            try:
                value = updates.check(args.home, cache=cache, force=True) if kind == 'check' else updates.download(
                    args.home, selected, cache=cache, cancelled=stop.is_set)
                results.put((kind, value, None))
            except Exception as error:
                results.put((kind, None, str(error)))
            user.PostMessageW(hwnd, WM_RESULT, 0, 0)
        worker = threading.Thread(target=task, name='CyCodeUpdater', daemon=True)
        worker.start()

    @CALLBACK
    def procedure(hwnd, message, wparam, lparam):
        nonlocal busy, selected
        if message == 1:  # WM_CREATE
            def control(name, title, style, x, y, width, height, identity=0):
                handle = user.CreateWindowExW(0, name, title, 0x50000000 | style,
                    pixels(x), pixels(y), pixels(width), pixels(height), hwnd, identity, instance, None)
                if not handle:
                    raise OSError('Cannot create updater control')
                user.SendMessageW(handle, 0x30, font, 1)
                return handle
            control('STATIC', 'CyCodeCore ' + manifest['version'], 0, 24, 22, 640, 28)
            handles['status'] = control('STATIC', text['ready'].format(current=manifest['version']), 0,
                                        24, 58, 640, 56)
            handles['notes'] = control('EDIT', '', 0x200000 | 0x800000 | 0x800 | 0x40 | 4, 24, 122, 640, 170)
            handles['check'] = control('BUTTON', text['check'], 0x10000, 24, 310, 200, 34, 100)
            handles['install'] = control('BUTTON', text['install'], 0x10000, 234, 310, 200, 34, 101)
            handles['release'] = control('BUTTON', text['release'], 0x10000, 444, 310, 132, 34, 102)
            control('BUTTON', text['close'], 0x10000, 586, 310, 78, 34, 103)
            enable()
            return 0
        if message == 0x111:  # WM_COMMAND
            identity = int(wparam) & 0xFFFF
            if identity == 100:
                begin(hwnd, 'check')
            elif identity == 101 and selected:
                begin(hwnd, 'download')
            elif identity == 102 and selected:
                webbrowser.open(selected['page'])
            elif identity == 103:
                user.PostMessageW(hwnd, 0x10, 0, 0)
            return 0
        if message == WM_RESULT:
            kind, value, error = results.get_nowait()
            busy = False
            if error:
                label(text['error'].format(error=error))
            elif kind == 'check':
                selected = value['release']
                label(text['available' if selected else 'current'].format(
                    current=value['installedVersion'], latest=selected['version'] if selected else ''))
                user.SetWindowTextW(handles['notes'], selected['notes'] if selected else '')
            else:
                # User confirmed this installer before download. No shell, silent
                # install, protection changes or termination of unrelated clients.
                subprocess.Popen([str(value), '/DIR=' + str(args.home)], close_fds=True)
                user.PostMessageW(hwnd, 0x10, 0, 0)
            enable()
            return 0
        if message == 0x10:  # WM_CLOSE
            stop.set()
            user.DestroyWindow(hwnd)
            return 0
        if message == 2:  # WM_DESTROY
            user.PostQuitMessage(0)
            return 0
        return user.DefWindowProcW(hwnd, message, wparam, lparam)

    descriptor = WindowClass(0, procedure, 0, 0, instance, None, None, 6, None, 'CyCodeUpdaterWindow')
    if not user.RegisterClassW(ctypes.byref(descriptor)):
        raise OSError('Cannot register updater window')
    hwnd = user.CreateWindowExW(0x10000, descriptor.name, text['title'], 0x00CA0000,
                               pixels(180), pixels(160), pixels(710), pixels(410), None, None, instance, None)
    if not hwnd:
        raise OSError('Cannot open updater window')
    if args.smoke_ui:
        assert {'status', 'notes', 'check', 'install', 'release'} <= handles.keys()
        user.PostMessageW(hwnd, 0x10, 0, 0)
    else:
        user.ShowWindow(hwnd, 1)
        begin(hwnd, 'check')
    message = W.MSG()
    try:
        while user.GetMessageW(ctypes.byref(message), None, 0, 0) > 0:
            if not user.IsDialogMessageW(hwnd, ctypes.byref(message)):
                user.TranslateMessage(ctypes.byref(message))
                user.DispatchMessageW(ctypes.byref(message))
    finally:
        stop.set()
        if worker:
            worker.join(timeout=20)
        if owned_font:
            gdi.DeleteObject.argtypes = [W.HGDIOBJ]
            gdi.DeleteObject(owned_font)
    if args.smoke_ui:
        print(json.dumps({'state': 'updater-controls-created-and-closed', 'networkUsed': False, 'coreStarted': False}))
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception as error:
        if sys.stderr:
            print(str(error), file=sys.stderr)
        elif os.name == 'nt':
            ctypes.windll.user32.MessageBoxW(None, str(error), 'CyCodeUpdater', 0x10)
        raise SystemExit(2)
