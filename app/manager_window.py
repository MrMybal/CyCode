"""Small native Windows manager; all file/update work belongs to shared services."""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes as W
import json
import os
from pathlib import Path
import queue
import struct
import subprocess
import sys
import threading
import time
import webbrowser
import zlib

import core_agents as agents
import core_updates as updates
from core_installation import current_payload, installation_root, manifest

TEXT = {
    'en': {
        'title': 'CyCodeCore Manager', 'agents': 'Agents and skills', 'updates': 'Updates',
        'apps': 'Select one or more applications', 'project': 'Project for the MCP connection',
        'browse': 'Browse...', 'scope': 'Configuration scope', 'project_scope': 'This project', 'user_scope': 'My user account',
        'rights': 'Core access', 'read': 'Read only', 'edit': 'Read, edit and build',
        'skill': 'Install the CyCode Core skill', 'mcp': 'Configure the local MCP connection',
        'preview': 'Preview changes', 'apply': 'Apply', 'connections': 'Managed connections',
        'refresh': 'Update managed connections', 'remove': 'Remove selected connection', 'reload': 'Refresh list',
        'hint': 'A skill provides instructions. MCP provides tools for the selected project. Reload your agent client after applying changes.',
        'ready': 'Choose applications, scope and the changes you want to make.', 'working': 'Preparing...',
        'review': 'Review these file locations before applying:\r\n', 'done': 'Changes applied. Reload the affected clients.',
        'empty': 'No managed connection found. Existing unowned settings are preserved.',
        'old_connections': '{count} connection(s) still target an older payload. Preview an update with Update managed connections.',
        'check': 'Check for updates', 'install': 'Download and install', 'release': 'GitHub release',
        'checking': 'Checking GitHub releases...', 'current': 'CyCodeCore {current} is up to date.',
        'available': 'CyCodeCore {latest} is available. Current selection: {current}.',
        'ready_update': 'The check runs once when this manager starts. Installation is always explicit.',
        'refresh_updates': 'Update owned MCP connections after installation',
        'confirm': 'Install CyCodeCore {latest}?\n\nThe verified Setup will install a separate payload. Running Core/MCP sessions are kept intact. Managed connections can be updated for their next reload.',
        'download': 'Downloading and verifying Setup...', 'installer': 'Complete the Setup window. Existing sessions remain active.',
        'installed': 'CyCodeCore {latest} was selected successfully. Reload MCP clients to use it.',
        'offered': 'CyCodeCore {latest} is available (current: {current}).\n\nOpen the manager?',
        'error': 'Action could not be completed:\r\n{error}', 'skipped': 'Preserved / skipped:',
        'busy_close': 'Wait for the current operation or cancel the Setup window.', 'cancel': 'Cancel download',
        'cancelled': 'Download cancellation requested.', 'choose_project': 'Choose an existing project directory',
        'unsupported': 'The manager window currently requires Windows. --check and --list do not open a window.',
    },
    'fr': {
        'title': 'CyCodeCore Manager', 'agents': 'Agents et skills', 'updates': 'Mises à jour',
        'apps': 'Sélectionnez une ou plusieurs applications', 'project': 'Projet de la connexion MCP',
        'browse': 'Parcourir...', 'scope': 'Portée de la configuration', 'project_scope': 'Ce projet', 'user_scope': 'Mon compte utilisateur',
        'rights': 'Accès au Core', 'read': 'Lecture seule', 'edit': 'Lecture, édition et compilation',
        'skill': 'Installer le skill CyCode Core', 'mcp': 'Configurer la connexion MCP locale',
        'preview': 'Voir les modifications', 'apply': 'Appliquer', 'connections': 'Connexions gérées',
        'refresh': 'Actualiser les connexions gérées', 'remove': 'Retirer la connexion sélectionnée', 'reload': 'Actualiser la liste',
        'hint': 'Le skill fournit les instructions. Le MCP fournit les outils pour le projet choisi. Rechargez le client IA après application.',
        'ready': 'Choisissez les applications, la portée et les modifications à effectuer.', 'working': 'Préparation...',
        'review': 'Vérifiez ces emplacements avant application :\r\n', 'done': 'Modifications appliquées. Rechargez les clients concernés.',
        'empty': 'Aucune connexion gérée trouvée. Les configurations non gérées sont conservées.',
        'old_connections': '{count} connexion(s) ciblent encore un ancien moteur. Utilisez Actualiser les connexions gérées pour préparer leur mise à jour.',
        'check': 'Vérifier les mises à jour', 'install': 'Télécharger et installer', 'release': 'Version GitHub',
        'checking': 'Recherche des versions GitHub...', 'current': 'CyCodeCore {current} est à jour.',
        'available': 'CyCodeCore {latest} est disponible. Sélection actuelle : {current}.',
        'ready_update': 'La vérification se lance à l’ouverture du gestionnaire. L’installation reste explicite.',
        'refresh_updates': 'Actualiser les connexions MCP gérées après installation',
        'confirm': 'Installer CyCodeCore {latest} ?\n\nLe Setup vérifié installera un moteur séparé. Les sessions Core/MCP actives sont conservées. Les connexions gérées peuvent être actualisées pour leur prochain rechargement.',
        'download': 'Téléchargement et vérification du Setup...', 'installer': 'Terminez l’assistant d’installation. Les sessions existantes restent actives.',
        'installed': 'CyCodeCore {latest} est sélectionné. Rechargez les clients MCP pour l’utiliser.',
        'offered': 'CyCodeCore {latest} est disponible (actuel : {current}).\n\nOuvrir le gestionnaire ?',
        'error': 'L’action n’a pas pu être terminée :\r\n{error}', 'skipped': 'Conservé / ignoré :',
        'busy_close': 'Patientez jusqu’à la fin de l’opération ou annulez l’assistant d’installation.', 'cancel': 'Annuler le téléchargement',
        'cancelled': 'Annulation du téléchargement demandée.', 'choose_project': 'Choisissez un dossier de projet existant',
        'unsupported': 'La fenêtre du gestionnaire nécessite Windows. --check et --list n’ouvrent aucune fenêtre.',
    },
}


def run(home, argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--home', type=Path, default=home)
    parser.add_argument('--installation-root', type=Path)
    parser.add_argument('--cache', type=Path)
    parser.add_argument('--state', type=Path)
    parser.add_argument('--language', choices=TEXT, default='en')
    parser.add_argument('--page', choices=('agents', 'updates'))
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--check', action='store_true')
    modes.add_argument('--list', action='store_true')
    modes.add_argument('--background', action='store_true')
    modes.add_argument('--smoke-ui', action='store_true')
    parser.add_argument('--snapshot', type=Path, help='Owned-window PNG capture for --smoke-ui only')
    parser.add_argument('--ui-scale', type=float, help='DPI validation override for --smoke-ui only')
    args = parser.parse_args(argv)
    if (args.snapshot or args.ui_scale is not None) and not args.smoke_ui:
        parser.error('Snapshot/scale validation requires --smoke-ui')
    context = agents.Manager(args.home, root=args.installation_root, state=args.state)
    cache = args.cache or updates.cache_directory() / agents.shared.digest(str(context.root).encode())[:20]
    preference = context.state / 'preferences.json'
    try:
        preferences = json.loads(agents.shared.bounded(preference)) if preference.exists() else {}
        if isinstance(preferences, dict) and preferences.get('format') == 'cycode-manager-preferences/1' and preferences.get('language') in TEXT:
            args.language = preferences['language']
    except (ValueError, OSError):
        pass
    text = TEXT[args.language]
    if args.check:
        print(json.dumps(updates.check(context.home, cache=cache, force=True), ensure_ascii=False))
        return 0
    if args.list:
        print(json.dumps({'clients': {key: profile.name for key, profile in agents.CLIENTS.items()},
                          'connections': context.receipts()}, ensure_ascii=False, indent=2))
        return 0
    if os.name != 'nt':
        raise RuntimeError(text['unsupported'])
    user = ctypes.WinDLL('user32', use_last_error=True)
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    gdi = ctypes.WinDLL('gdi32', use_last_error=True)
    user.MessageBoxW.argtypes = [W.HWND, W.LPCWSTR, W.LPCWSTR, W.UINT]
    user.MessageBoxW.restype = ctypes.c_int
    if args.background:
        try:
            found = updates.check(context.home, cache=cache)
            if found['status'] != 'available':
                return 0
            previous = updates.read_state(cache / 'offered')
            if previous.get('version') == found['release']['version'] and 0 <= time.time() - previous.get('at', 0) < updates.CHECK_INTERVAL_SECONDS:
                return 0
            updates.atomic_json(cache / 'offered/state.json', {'version': found['release']['version'], 'at': time.time()})
            if user.MessageBoxW(None, text['offered'].format(latest=found['release']['version'], current=found['installedVersion']), text['title'], 0x24) != 6:
                return 0
        except Exception:
            return 0
    user.SetProcessDPIAware()
    user.GetDpiForSystem.restype = W.UINT
    scale = args.ui_scale if args.ui_scale is not None else max(1.0, user.GetDpiForSystem() / 96.0)
    if not 0.75 <= scale <= 3:
        raise ValueError('Invalid UI scale')
    px = lambda n: round(n * scale)
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
    user.GetWindowTextLengthW.argtypes = [W.HWND]
    user.GetWindowTextLengthW.restype = ctypes.c_int
    user.GetWindowTextW.argtypes = [W.HWND, W.LPWSTR, ctypes.c_int]
    user.GetClientRect.argtypes = [W.HWND, ctypes.POINTER(W.RECT)]
    user.GetWindowRect.argtypes = [W.HWND, ctypes.POINTER(W.RECT)]
    user.GetFocus.restype = W.HWND
    user.MapWindowPoints.argtypes = [W.HWND, W.HWND, W.LPVOID, W.UINT]
    user.SetWindowPos.argtypes = [W.HWND, W.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, W.UINT]
    user.SetScrollRange.argtypes = [W.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, W.BOOL]
    user.SetScrollPos.argtypes = [W.HWND, ctypes.c_int, ctypes.c_int, W.BOOL]
    user.SetWindowTextW.argtypes = [W.HWND, W.LPCWSTR]
    user.EnableWindow.argtypes = [W.HWND, W.BOOL]
    user.ShowWindow.argtypes = [W.HWND, ctypes.c_int]
    user.UpdateWindow.argtypes = [W.HWND]
    user.LoadImageW.argtypes = [W.HINSTANCE, W.LPCWSTR, W.UINT, ctypes.c_int, ctypes.c_int, W.UINT]
    user.LoadImageW.restype = W.HANDLE
    user.DestroyIcon.argtypes = [W.HICON]
    user.GetKeyState.argtypes = [ctypes.c_int]
    user.GetKeyState.restype = ctypes.c_short
    user.DestroyWindow.argtypes = [W.HWND]
    user.GetMessageW.argtypes = [ctypes.POINTER(W.MSG), W.HWND, W.UINT, W.UINT]
    user.IsDialogMessageW.argtypes = [W.HWND, ctypes.POINTER(W.MSG)]
    user.TranslateMessage.argtypes = [ctypes.POINTER(W.MSG)]
    user.DispatchMessageW.argtypes = [ctypes.POINTER(W.MSG)]
    user.DispatchMessageW.restype = ctypes.c_ssize_t
    gdi.GetStockObject.argtypes = [ctypes.c_int]
    gdi.GetStockObject.restype = W.HGDIOBJ
    gdi.CreateFontW.argtypes = [ctypes.c_int] * 5 + [W.DWORD] * 8 + [W.LPCWSTR]
    gdi.CreateFontW.restype = W.HFONT
    font = gdi.CreateFontW(-px(14), 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 5, 0, 'Segoe UI')
    instance = kernel.GetModuleHandleW(None)
    controls, groups, identities = {}, {'agents': [], 'updates': []}, {}
    locations = {}
    scroll, scroll_max = 0, 0
    results, workers = queue.Queue(), {'agents': None, 'updates': None}
    kinds = {'agents': None, 'updates': None}
    stop = threading.Event()
    selected, plan, rows, active_page = None, None, [], args.page or ('updates' if args.background else 'agents')
    WM_RESULT, WM_PROGRESS = 0x8001, 0x8002

    def send_string(handle, message, value):
        buffer = ctypes.c_wchar_p(value)
        return user.SendMessageW(handle, message, 0, ctypes.cast(buffer, ctypes.c_void_p).value)

    def get(name):
        length = user.GetWindowTextLengthW(controls[name])
        if length > 8192:
            raise ValueError('Input path is too long')
        buffer = ctypes.create_unicode_buffer(length + 1)
        user.GetWindowTextW(controls[name], buffer, len(buffer))
        return buffer.value.strip()

    def set_text(name, value):
        user.SetWindowTextW(controls[name], str(value))

    def enabled():
        for name in ('preview', 'browse', 'apps', 'project', 'scope', 'rights', 'skill', 'mcp', 'refresh', 'remove', 'reload', 'connections'):
            user.EnableWindow(controls[name], workers['agents'] is None and kinds['updates'] != 'install')
        user.EnableWindow(controls['apply'], workers['agents'] is None and kinds['updates'] != 'install' and plan is not None)
        user.EnableWindow(controls['check'], workers['updates'] is None)
        user.EnableWindow(controls['install'], workers['updates'] is None and workers['agents'] is None and selected is not None)
        user.EnableWindow(controls['refresh_updates'], workers['updates'] is None)
        user.EnableWindow(controls['release'], selected is not None)
        user.EnableWindow(controls['cancel'], workers['updates'] is not None)

    def switch(page):
        nonlocal active_page
        active_page = page
        for group, handles in groups.items():
            for handle in handles:
                user.ShowWindow(handle, 5 if group == page else 0)

    def scroll_to(hwnd, value):
        nonlocal scroll
        scroll = max(0, min(scroll_max, value))
        for key, handle in controls.items():
            x, y = locations[key]
            user.SetWindowPos(handle, None, x, y - scroll, 0, 0, 0x15)
        user.SetScrollPos(hwnd, 1, scroll, True)

    def scroll_range(hwnd):
        nonlocal scroll_max
        rectangle = W.RECT()
        user.GetClientRect(hwnd, ctypes.byref(rectangle))
        scroll_max = max(0, px(690 if active_page == 'agents' else 635) - rectangle.bottom)
        user.SetScrollRange(hwnd, 1, 0, scroll_max, True)
        scroll_to(hwnd, scroll)

    def start(hwnd, group, kind, action):
        if workers[group] is not None:
            return
        def work():
            try:
                results.put((group, kind, action(), None))
            except Exception as error:
                results.put((group, kind, None, str(error)))
            user.PostMessageW(hwnd, WM_RESULT, 0, 0)
        workers[group] = threading.Thread(target=work, name='CyCodeManager-' + kind, daemon=False)
        kinds[group] = kind
        workers[group].start()
        set_text('status' if group == 'agents' else 'update_status', text['working' if group == 'agents' else 'checking' if kind == 'check' else 'download'])
        enabled()

    def inventory():
        nonlocal rows
        rows = context.receipts()
        user.SendMessageW(controls['connections'], 0x184, 0, 0)
        for row in rows:
            try:
                number = manifest(Path(row['home']))[0]['version']
            except (ValueError, OSError):
                number = '?'
            title = agents.CLIENTS[row['client']].name + ' | ' + row['workspace'] + ' | ' + number
            send_string(controls['connections'], 0x180, title)
        if not rows:
            set_text('status', text['empty'])
        else:
            outdated = sum(Path(row['home']) != context.home for row in rows)
            if outdated:
                set_text('status', text['old_connections'].format(count=outdated))

    def describe(prepared):
        details = prepared.preview()
        output = text['review'] + '\r\n'.join(details['files'])
        if details['skipped']:
            output += '\r\n\r\n' + text['skipped'] + '\r\n' + '\r\n'.join(details['skipped'])
        return output

    def progress(hwnd, value):
        # Only phase transitions are posted; streaming every block floods the UI.
        if value['phase'] == 'installer':
            results.put(('updates', 'progress', value, None))
            user.PostMessageW(hwnd, WM_RESULT, 0, 0)

    def choose_folder(hwnd):
        shell = ctypes.WinDLL('shell32', use_last_error=True)
        ole = ctypes.OleDLL('ole32')
        class Browse(ctypes.Structure):
            _fields_ = [('owner', W.HWND), ('root', W.LPVOID), ('display', W.LPWSTR), ('title', W.LPCWSTR),
                        ('flags', W.UINT), ('callback', W.LPVOID), ('parameter', W.LPARAM), ('image', ctypes.c_int)]
        shell.SHBrowseForFolderW.argtypes = [ctypes.POINTER(Browse)]
        shell.SHBrowseForFolderW.restype = W.LPVOID
        shell.SHGetPathFromIDListW.argtypes = [W.LPVOID, W.LPWSTR]
        ole.CoTaskMemFree.argtypes = [W.LPVOID]
        initialized = ole.CoInitialize(None)
        buffer = ctypes.create_unicode_buffer(32768)
        descriptor = Browse(hwnd, None, buffer, text['choose_project'], 0x241, None, 0, 0)
        item = shell.SHBrowseForFolderW(ctypes.byref(descriptor))
        try:
            if item and shell.SHGetPathFromIDListW(item, buffer):
                set_text('project', buffer.value)
        finally:
            if item:
                ole.CoTaskMemFree(item)
            if initialized in (0, 1):
                ole.CoUninitialize()

    @CALLBACK
    def procedure(hwnd, message, wparam, lparam):
        nonlocal plan, selected
        try:
            if message == 1:
                counter = 200
                def control(key, kind, label, x, y, width, height, *, style=0, group=None, identity=None):
                    nonlocal counter
                    counter += 1
                    identity = identity or counter
                    handle = user.CreateWindowExW(0x200 if kind in ('EDIT', 'LISTBOX') else 0, kind, label,
                        0x50000000 | style, px(x), px(y), px(width), px(height), hwnd, identity, instance, None)
                    if not handle:
                        raise OSError('Cannot create manager control: ' + key)
                    controls[key] = handle
                    locations[key] = (px(x), px(y))
                    identities[identity] = key
                    user.SendMessageW(handle, 0x30, font, 1)
                    if group:
                        groups[group].append(handle)
                    return handle
                current, _ = manifest(context.home)
                control('heading', 'STATIC', 'CyCodeCore ' + current['version'], 22, 16, 880, 28)
                control('agents_tab', 'BUTTON', text['agents'], 22, 52, 230, 32, style=0x10000)
                control('updates_tab', 'BUTTON', text['updates'], 262, 52, 230, 32, style=0x10000)
                control('language', 'BUTTON', 'English' if args.language == 'fr' else 'Français', 762, 52, 160, 32, style=0x10000)
                def label(key, title, x, y, width=650, height=24, group='agents'):
                    return control(key, 'STATIC', title, x, y, width, height, group=group)
                label('apps_label', text['apps'], 22, 95, 300)
                app_list = control('apps', 'LISTBOX', '', 22, 123, 310, 168, style=0x200000 | 0x10000 | 0x8 | 0x1, group='agents')
                for profile in agents.CLIENTS.values():
                    send_string(app_list, 0x180, profile.name)
                user.SendMessageW(app_list, 0x185, 1, 0)
                label('project_label', text['project'], 352, 95, 570)
                control('project', 'EDIT', '', 352, 123, 420, 28, style=0x10000 | 0x80, group='agents')
                control('browse', 'BUTTON', text['browse'], 782, 122, 140, 30, style=0x10000, group='agents')
                label('scope_label', text['scope'], 352, 168, 260)
                scope = control('scope', 'COMBOBOX', '', 352, 196, 260, 150, style=0x10000 | 3 | 0x200000, group='agents')
                for item in ('project_scope', 'user_scope'):
                    send_string(scope, 0x143, text[item])
                user.SendMessageW(scope, 0x14E, 0, 0)
                label('rights_label', text['rights'], 632, 168, 290)
                rights = control('rights', 'COMBOBOX', '', 632, 196, 290, 150, style=0x10000 | 3 | 0x200000, group='agents')
                for item in ('read', 'edit'):
                    send_string(rights, 0x143, text[item])
                user.SendMessageW(rights, 0x14E, 0, 0)
                for key, y in (('skill', 238), ('mcp', 270)):
                    handle = control(key, 'BUTTON', text[key], 352, y, 570, 26, style=0x10000 | 3, group='agents')
                    user.SendMessageW(handle, 0xF1, 1, 0)
                control('preview', 'BUTTON', text['preview'], 22, 308, 230, 32, style=0x10000, group='agents')
                control('apply', 'BUTTON', text['apply'], 262, 308, 160, 32, style=0x10000, group='agents')
                control('preview_text', 'EDIT', '', 22, 352, 900, 90, style=0x200000 | 0x800 | 0x40 | 4 | 0x10000, group='agents')
                label('connection_label', text['connections'], 22, 452)
                control('connections', 'LISTBOX', '', 22, 480, 900, 65, style=0x200000 | 0x10000 | 0x1, group='agents')
                for key, x, width in (('refresh', 22, 305), ('remove', 337, 330), ('reload', 677, 245)):
                    control(key, 'BUTTON', text[key], x, 556, width, 30, style=0x10000, group='agents')
                label('hint', text['hint'], 22, 596, 900, 38)
                label('status', text['ready'], 22, 642, 900, 36)
                label('update_status', text['ready_update'], 22, 108, 900, 55, group='updates')
                control('release_notes', 'EDIT', '', 22, 180, 900, 320, style=0x200000 | 0x800 | 0x40 | 4 | 0x10000, group='updates')
                flag = control('refresh_updates', 'BUTTON', text['refresh_updates'], 22, 523, 900, 28, style=0x10000 | 3, group='updates')
                user.SendMessageW(flag, 0xF1, 1, 0)
                for key, x, width in (('check', 22, 225), ('install', 257, 255), ('release', 522, 180), ('cancel', 712, 210)):
                    control(key, 'BUTTON', text[key], x, 575, width, 34, style=0x10000, group='updates')
                switch(active_page)
                scroll_range(hwnd)
                enabled()
                return 0
            if message == 0x111:
                key = identities.get(int(wparam) & 0xFFFF)
                notification = (int(wparam) >> 16) & 0xFFFF
                if key in ('project', 'scope', 'rights', 'skill', 'mcp', 'apps'):
                    if (key == 'project' and notification == 0x300) or (key in ('scope', 'rights', 'apps') and notification == 1) or (key in ('skill', 'mcp') and notification == 0):
                        plan = None
                        enabled()
                    return 0
                if notification != 0:
                    return 0
                if key in ('agents_tab', 'updates_tab'):
                    switch('agents' if key == 'agents_tab' else 'updates')
                    scroll_range(hwnd)
                    scroll_to(hwnd, 0)
                elif key == 'browse':
                    choose_folder(hwnd)
                elif key == 'language':
                    if any(workers.values()):
                        return 0
                    language = 'en' if args.language == 'fr' else 'fr'
                    old = json.loads(agents.shared.bounded(preference)) if preference.exists() else {}
                    if old and old.get('format') != 'cycode-manager-preferences/1':
                        raise ValueError('Unknown manager preferences preserved')
                    updates.atomic_json(preference, {**old, 'format': 'cycode-manager-preferences/1', 'language': language})
                    entry = context.home / 'apps/CyCodeManager/main.py'
                    if not entry.is_file():
                        raise ValueError('Manager is not present in the selected development payload')
                    destination = ['--installation-root', str(context.root)] if (context.root / 'current.json').is_file() else ['--home', str(context.home)]
                    subprocess.Popen([str(context.home / 'tools/python/pythonw.exe'), '-B', str(entry),
                        *destination, '--language', language, '--page', active_page], close_fds=True)
                    user.PostMessageW(hwnd, 0x10, 0, 0)
                elif key == 'preview':
                    clients = [name for index, name in enumerate(agents.CLIENTS) if user.SendMessageW(controls['apps'], 0x187, index, 0) > 0]
                    project = get('project')
                    scope = 'user' if user.SendMessageW(controls['scope'], 0x147, 0, 0) == 1 else 'project'
                    access = 'edit' if user.SendMessageW(controls['rights'], 0x147, 0, 0) == 1 else 'read'
                    skill = user.SendMessageW(controls['skill'], 0xF0, 0, 0) == 1
                    mcp = user.SendMessageW(controls['mcp'], 0xF0, 0, 0) == 1
                    start(hwnd, 'agents', 'prepare', lambda: context.prepare(clients, Path(project) if project else None, scope=scope, skill=skill, mcp=mcp, access=access))
                elif key == 'apply' and plan is not None:
                    reviewed = plan
                    start(hwnd, 'agents', 'apply', lambda: context.apply(reviewed))
                elif key == 'refresh':
                    start(hwnd, 'agents', 'prepare', context.prepare_refresh)
                elif key == 'remove':
                    index = user.SendMessageW(controls['connections'], 0x188, 0, 0)
                    if 0 <= index < len(rows):
                        identity = rows[index]['id']
                        start(hwnd, 'agents', 'prepare', lambda: context.prepare_remove(identity))
                elif key == 'reload':
                    inventory()
                elif key == 'check':
                    start(hwnd, 'updates', 'check', lambda: updates.check(context.home, cache=cache, force=True))
                elif key == 'release' and selected:
                    webbrowser.open(selected['page'])
                elif key == 'install' and selected:
                    if workers['agents'] is not None:
                        return 0
                    if user.MessageBoxW(hwnd, text['confirm'].format(latest=selected['version']), text['title'], 0x124) == 6:
                        plan = None
                        offer = selected
                        refresh = user.SendMessageW(controls['refresh_updates'], 0xF0, 0, 0) == 1
                        stop.clear()
                        start(hwnd, 'updates', 'install', lambda: updates.install_update(context.home, offer, cache=cache,
                              language=args.language, refresh=refresh, cancelled=stop.is_set,
                              progress=lambda value: progress(hwnd, value), manager_state=context.state))
                elif key == 'cancel':
                    stop.set()
                    set_text('update_status', text['cancelled'])
                return 0
            if message == WM_RESULT:
                group, kind, value, error = results.get_nowait()
                if kind == 'progress':
                    set_text('update_status', text['installer'])
                    user.EnableWindow(controls['cancel'], False)
                    return 0
                workers[group] = None
                kinds[group] = None
                if error:
                    set_text('status' if group == 'agents' else 'update_status', text['error'].format(error=error))
                elif group == 'agents':
                    if kind == 'prepare':
                        plan = value
                        set_text('preview_text', describe(plan))
                        set_text('status', text['ready'])
                    else:
                        plan = None
                        set_text('status', text['done'])
                        inventory()
                elif kind == 'check':
                    selected = value['release']
                    status = text['available' if selected else 'current'].format(current=value['installedVersion'], latest=selected['version'] if selected else '')
                    set_text('update_status', status)
                    set_text('heading', 'CyCodeCore ' + value['installedVersion'] + ('  —  ' + selected['version'] if selected else ''))
                    set_text('release_notes', selected['notes'] if selected else '')
                elif kind == 'install':
                    set_text('update_status', text['installed'].format(latest=value['version']))
                    if value['connections'].get('skipped'):
                        user.MessageBoxW(hwnd, text['skipped'] + '\n' + '\n'.join(value['connections']['skipped']), text['title'], 0x40)
                    new_home = Path(value['home'])
                    subprocess.Popen([str(new_home / 'tools/python/pythonw.exe'), '-B', str(new_home / 'apps/CyCodeManager/main.py'),
                        '--installation-root', str(context.root), '--language', args.language], close_fds=True)
                    user.PostMessageW(hwnd, 0x10, 0, 0)
                enabled()
                return 0
            if message == 0x115:
                code = int(wparam) & 0xFFFF
                page = max(px(30), px(600) - scroll_max)
                value = {0: scroll - px(30), 1: scroll + px(30), 2: scroll - page,
                         3: scroll + page, 4: int(wparam) >> 16, 5: int(wparam) >> 16,
                         6: 0, 7: scroll_max}.get(code, scroll)
                scroll_to(hwnd, value)
                return 0
            if message == 0x20A:
                delta = ctypes.c_short(int(wparam) >> 16).value
                scroll_to(hwnd, scroll - int(delta / 120) * px(60))
                return 0
            if message == 0x10:
                if any(workers.values()):
                    stop.set()
                    set_text('update_status' if workers['updates'] else 'status', text['busy_close'])
                    return 0
                user.DestroyWindow(hwnd)
                return 0
            if message == 2:
                user.PostQuitMessage(0)
                return 0
        except Exception as error:
            if 'status' in controls:
                set_text('status' if active_page == 'agents' else 'update_status', text['error'].format(error=str(error)))
            else:
                return -1
        return user.DefWindowProcW(hwnd, message, wparam, lparam)

    icon = user.LoadImageW(None, str(context.home / 'icons/CyCode.ico'), 1, 0, 0, 0x50)
    descriptor = WindowClass(0, procedure, 0, 0, instance, icon, None, 6, None, 'CyCodeCoreManagerWindow')
    if not user.RegisterClassW(ctypes.byref(descriptor)):
        raise OSError('Cannot register manager window')
    work = W.RECT()
    user.SystemParametersInfoW(0x30, 0, ctypes.byref(work), 0)
    height = min(px(735), work.bottom - work.top - 12)
    hwnd = user.CreateWindowExW(0x10000 | (0x80 if args.smoke_ui else 0), descriptor.name, text['title'], 0x00EA0000,
                              max(work.left + 6, work.left + (work.right - work.left - px(980)) // 2), work.top + 6,
                              px(980), height, None, None, instance, None)
    if not hwnd:
        raise OSError('Cannot create manager window')
    if not args.smoke_ui:
        try:
            inventory()
        except (ValueError, OSError) as error:
            set_text('status', text['error'].format(error=error))
    if args.smoke_ui:
        for page in ('updates', 'agents'):
            switch(page)
            scroll_range(hwnd)
        switch(args.page or 'agents')
        scroll_range(hwnd)
        assert {'apps', 'preview', 'apply', 'connections', 'check', 'install'} <= controls.keys()
        assert user.SendMessageW(controls['scope'], 0x146, 0, 0) == 2
        assert user.SendMessageW(controls['scope'], 0x147, 0, 0) == 0
        assert get('scope') == text['project_scope'] and get('rights') == text['read']
        if args.snapshot:
            # Paint this tool window outside the desktop work area, without
            # activation or a taskbar item; native comboboxes need a paint pass.
            user.SetWindowPos(hwnd, None, -32000, -32000, 0, 0, 0x15)
            user.ShowWindow(hwnd, 4)
            user.UpdateWindow(hwnd)
            capture_window(user, gdi, hwnd, args.snapshot)
            user.ShowWindow(hwnd, 0)
        user.PostMessageW(hwnd, 0x10, 0, 0)
    else:
        user.ShowWindow(hwnd, 1)
        start(hwnd, 'updates', 'check', lambda: updates.check(context.home, cache=cache, force=True))
    message = W.MSG()
    try:
        while user.GetMessageW(ctypes.byref(message), None, 0, 0) > 0:
            if message.message == 0x100 and message.wParam == 65 and user.GetKeyState(17) < 0:
                focused = user.GetFocus()
                if focused in [controls[k] for k in ('project', 'preview_text', 'release_notes')]:
                    user.SendMessageW(focused, 0xB1, 0, -1)
                    continue
            if not user.IsDialogMessageW(hwnd, ctypes.byref(message)):
                user.TranslateMessage(ctypes.byref(message))
                user.DispatchMessageW(ctypes.byref(message))
            elif message.message == 0x100 and message.wParam == 9:
                focused = user.GetFocus()
                if focused in controls.values():
                    rectangle, client = W.RECT(), W.RECT()
                    user.GetWindowRect(focused, ctypes.byref(rectangle))
                    user.MapWindowPoints(None, hwnd, ctypes.byref(rectangle), 2)
                    user.GetClientRect(hwnd, ctypes.byref(client))
                    if rectangle.top < 0:
                        scroll_to(hwnd, scroll + rectangle.top - px(8))
                    elif rectangle.bottom > client.bottom:
                        scroll_to(hwnd, scroll + rectangle.bottom - client.bottom + px(8))
    finally:
        stop.set()
        for worker in workers.values():
            if worker:
                worker.join(timeout=20)
        if font:
            gdi.DeleteObject.argtypes = [W.HGDIOBJ]
            gdi.DeleteObject(font)
        if icon:
            user.DestroyIcon(icon)
    if args.smoke_ui:
        print(json.dumps({'state': 'manager-controls-created-and-closed', 'language': args.language,
                          'scale': scale, 'controls': len(controls), 'networkUsed': False,
                          'clientSettingsChanged': False, 'coreStarted': False}))
    return 0


def capture_window(user, gdi, hwnd, target):
    """Render this owned hidden window only, never capture the user's desktop."""
    user.GetWindowRect.argtypes = [W.HWND, ctypes.POINTER(W.RECT)]
    rectangle = W.RECT()
    user.GetWindowRect(hwnd, ctypes.byref(rectangle))
    width, height = rectangle.right - rectangle.left, rectangle.bottom - rectangle.top
    user.GetWindowDC.argtypes = [W.HWND]
    user.GetWindowDC.restype = W.HDC
    user.ReleaseDC.argtypes = [W.HWND, W.HDC]
    user.PrintWindow.argtypes = [W.HWND, W.HDC, W.UINT]
    gdi.CreateCompatibleDC.argtypes = [W.HDC]
    gdi.CreateCompatibleDC.restype = W.HDC
    gdi.CreateCompatibleBitmap.argtypes = [W.HDC, ctypes.c_int, ctypes.c_int]
    gdi.CreateCompatibleBitmap.restype = W.HBITMAP
    gdi.SelectObject.argtypes = [W.HDC, W.HGDIOBJ]
    gdi.SelectObject.restype = W.HGDIOBJ
    gdi.GetDIBits.argtypes = [W.HDC, W.HBITMAP, W.UINT, W.UINT, W.LPVOID, W.LPVOID, W.UINT]
    gdi.DeleteDC.argtypes = [W.HDC]
    gdi.DeleteObject.argtypes = [W.HGDIOBJ]
    dc = user.GetWindowDC(hwnd)
    memory = gdi.CreateCompatibleDC(dc)
    bitmap = gdi.CreateCompatibleBitmap(dc, width, height)
    previous = gdi.SelectObject(memory, bitmap)
    try:
        user.SendMessageW(hwnd, 0x317, memory, 0x1E)  # WM_PRINT, including child controls; no visibility check.
        header = struct.pack('<IiiHHIIiiII', 40, width, -height, 1, 32, 0, width * height * 4, 0, 0, 0, 0)
        info = ctypes.create_string_buffer(header)
        pixels = ctypes.create_string_buffer(width * height * 4)
        gdi.SelectObject(memory, previous)
        if not gdi.GetDIBits(memory, bitmap, 0, height, pixels, info, 0):
            raise OSError('Cannot read manager rendering')
        raw = pixels.raw
        scanlines = bytearray()
        for y in range(height):
            scanlines.append(0)
            line = raw[y * width * 4:(y + 1) * width * 4]
            for x in range(0, len(line), 4):
                scanlines.extend((line[x + 2], line[x + 1], line[x]))
        def chunk(kind, data):
            return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(scanlines)) + chunk(b'IEND', b''))
    finally:
        gdi.SelectObject(memory, previous)
        gdi.DeleteObject(bitmap)
        gdi.DeleteDC(memory)
        user.ReleaseDC(hwnd, dc)
