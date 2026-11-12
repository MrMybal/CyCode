"""Installation-scoped GitHub updates for the optional CyCodeUpdater application."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

REPOSITORY = 'MrMybal/CyCode'
API = 'https://api.github.com/repos/' + REPOSITORY
TAG_PREFIX = 'core-v'
MAX_SETUP_BYTES = 256 * 1024 * 1024
MAX_METADATA_BYTES = 2 * 1024 * 1024
CHECK_INTERVAL_SECONDS = 24 * 60 * 60
VERSION_RE = re.compile(r'(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})')
REDIRECT_HOSTS = {'api.github.com', 'github.com', 'objects.githubusercontent.com',
                  'release-assets.githubusercontent.com'}


class UpdateError(RuntimeError):
    pass


def version(value):
    if not isinstance(value, str) or not VERSION_RE.fullmatch(value):
        raise UpdateError('Unsupported Core version')
    return tuple(map(int, value.split('.')))


def ordinary(path):
    path = Path(path).absolute()
    for part in (path, *path.parents):
        if part.exists() or part.is_symlink():
            stat = part.lstat()
            if part.is_symlink() or getattr(stat, 'st_file_attributes', 0) & 1024:
                raise UpdateError('Redirected update path refused')
    return path


def installation(home):
    home = ordinary(home)
    manifest = ordinary(home / 'core-distribution.json')
    if not manifest.is_file() or manifest.stat().st_size > MAX_METADATA_BYTES:
        raise UpdateError('Complete Core installation not found')
    data = json.loads(manifest.read_text('utf-8'))
    if data.get('product') != 'CyCodeCore' or data.get('state') != 'complete':
        raise UpdateError('Only a complete CyCodeCore installation can be updated')
    version(data.get('version'))
    return data


def cache_directory():
    if os.name == 'nt':
        base = Path(os.environ.get('LOCALAPPDATA', Path.home() / 'AppData/Local'))
    elif __import__('sys').platform == 'darwin':
        base = Path.home() / 'Library/Caches'
    else:
        base = Path(os.environ.get('XDG_CACHE_HOME', Path.home() / '.cache'))
    return ordinary(base / 'CyCodeCore/updates')


def atomic_json(path, data):
    path = ordinary(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, name = tempfile.mkstemp(prefix='.cycode-updater-', suffix='.tmp', dir=path.parent)
    temporary = Path(name)
    try:
        with os.fdopen(descriptor, 'w', encoding='utf-8') as stream:
            json.dump(data, stream, ensure_ascii=False, indent=2)
            stream.write('\n')
            stream.flush()
            os.fsync(stream.fileno())
        ordinary(path)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def read_state(cache):
    path = ordinary(Path(cache) / 'state.json')
    if not path.is_file():
        return {}
    if path.stat().st_size > 64 * 1024:
        raise UpdateError('Oversized update state')
    data = json.loads(path.read_text('utf-8'))
    return data if isinstance(data, dict) else {}


def github_token():
    for name in ('CYCODE_GITHUB_TOKEN', 'GH_TOKEN', 'GITHUB_TOKEN'):
        value = os.environ.get(name, '').strip()
        if value and '\r' not in value and '\n' not in value:
            return value
    git = shutil.which('git')
    if not git:
        return None
    # Existing Git authentication only; never prompt, store or log credentials.
    environment = dict(os.environ, GIT_TERMINAL_PROMPT='0', GCM_INTERACTIVE='Never')
    try:
        result = subprocess.run([git, 'credential', 'fill'], input='protocol=https\nhost=github.com\n\n',
            capture_output=True, text=True, timeout=8, env=environment,
            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        if result.returncode or len(result.stdout) > 65536:
            return None
        fields = dict(line.split('=', 1) for line in result.stdout.splitlines() if '=' in line)
        return fields.get('password') or None
    except (OSError, subprocess.SubprocessError):
        return None


class SafeRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, new_url):
        parsed = urllib.parse.urlsplit(new_url)
        if (parsed.scheme != 'https' or parsed.hostname not in REDIRECT_HOSTS or
                parsed.username or parsed.password or parsed.port not in (None, 443)):
            raise UpdateError('Untrusted GitHub download redirect')
        redirected = super().redirect_request(request, fp, code, message, headers, new_url)
        if redirected is not None:
            redirected.remove_header('Authorization')
            if parsed.hostname == 'api.github.com':
                credential = request.get_header('Authorization')
                if credential:
                    redirected.add_header('Authorization', credential)
        return redirected


class GitHub:
    def __init__(self, token=None):
        self.token = token
        self.opener = urllib.request.build_opener(SafeRedirect())

    def open(self, url, binary=False):
        parsed = urllib.parse.urlsplit(url)
        if (parsed.scheme != 'https' or parsed.netloc != 'api.github.com' or
                not parsed.path.startswith('/repos/' + REPOSITORY + '/') or parsed.fragment):
            raise UpdateError('Unexpected update endpoint')
        headers = {'Accept': 'application/octet-stream' if binary else 'application/vnd.github+json',
                   'User-Agent': 'CyCodeUpdater', 'X-GitHub-Api-Version': '2022-11-28'}
        if self.token:
            headers['Authorization'] = 'Bearer ' + self.token
        try:
            return self.opener.open(urllib.request.Request(url, headers=headers), timeout=15)
        except urllib.error.HTTPError as error:
            if error.code in (401, 403, 404):
                raise UpdateError('GitHub access unavailable (HTTP %d). Private releases require existing GitHub authentication.' % error.code) from None
            raise UpdateError('GitHub request failed (HTTP %d)' % error.code) from None
        except (urllib.error.URLError, TimeoutError, OSError):
            raise UpdateError('Cannot reach GitHub. The installed version is unchanged.') from None

    def read(self, url, maximum, binary=False):
        with self.open(url, binary) as response:
            payload = response.read(maximum + 1)
        if len(payload) > maximum:
            raise UpdateError('Oversized update response')
        return payload

    def releases(self):
        data = json.loads(self.read(API + '/releases?per_page=100', MAX_METADATA_BYTES))
        if not isinstance(data, list):
            raise UpdateError('Invalid GitHub release response')
        return data


def asset(row, name, maximum):
    if not isinstance(row, dict) or row.get('name') != name:
        return None
    identity = row.get('id')
    size = row.get('size')
    expected_url = API + '/releases/assets/' + str(identity)
    if (type(identity) is not int or identity <= 0 or type(size) is not int or not 0 < size <= maximum or
            row.get('url') != expected_url or row.get('state', 'uploaded') != 'uploaded'):
        return None
    digest = row.get('digest')
    if digest is not None and not re.fullmatch(r'sha256:[0-9a-f]{64}', digest):
        return None
    return {'name': name, 'url': expected_url, 'size': size, 'digest': digest}


def select_release(releases, current, channel='preview'):
    current_version = version(current)
    candidates = []
    for release in releases:
        if not isinstance(release, dict) or release.get('draft'):
            continue
        if release.get('prerelease') and channel != 'preview':
            continue
        tag = release.get('tag_name', '')
        if not isinstance(tag, str) or not tag.startswith(TAG_PREFIX):
            continue
        new = tag[len(TAG_PREFIX):]
        try:
            number = version(new)
        except UpdateError:
            continue
        if number <= current_version:
            continue
        page = 'https://github.com/' + REPOSITORY + '/releases/tag/' + tag
        if release.get('html_url') != page:
            continue
        files = release.get('assets', [])
        if not isinstance(files, list):
            continue
        setup_name = 'CyCodeCore-' + new + '-windows-x64-setup.exe'
        setups = [found for row in files if (found := asset(row, setup_name, MAX_SETUP_BYTES))]
        checksums = [found for row in files if (found := asset(row, 'SHA256SUMS.txt', 128 * 1024))]
        if len(setups) != 1 or len(checksums) != 1:
            continue
        body = release.get('body')
        candidates.append((number, {'version': new, 'page': page, 'setup': setups[0],
                                    'checksums': checksums[0], 'notes': body[:4096] if isinstance(body, str) else ''}))
    return max(candidates, key=lambda item: item[0])[1] if candidates else None


def check(home, *, cache=None, force=False, client=None, now=None):
    manifest = installation(home)
    cache = ordinary(cache or cache_directory())
    stamp = time.time() if now is None else now
    previous = read_state(cache)
    age = stamp - previous.get('checkedAt', 0) if isinstance(previous.get('checkedAt', 0), (int, float)) else -1
    installation_id = hashlib.sha256(str(ordinary(home)).encode('utf-8')).hexdigest()
    if (not force and previous.get('installationId') == installation_id and previous.get('installedVersion') == manifest['version'] and
            previous.get('channel') == manifest.get('channel', 'preview') and
            previous.get('status') in ('available', 'up-to-date') and 0 <= age < CHECK_INTERVAL_SECONDS):
        return previous
    client = client or GitHub(github_token())
    selected = select_release(client.releases(), manifest['version'], manifest.get('channel', 'preview'))
    result = {'format': 'cycode-core-update-state/1', 'installedVersion': manifest['version'], 'installationId': installation_id,
              'channel': manifest.get('channel', 'preview'), 'checkedAt': stamp,
              'status': 'available' if selected else 'up-to-date', 'release': selected}
    atomic_json(cache / 'state.json', result)
    return result


def checksum(payload, name):
    matches = []
    for line in payload.decode('utf-8-sig').splitlines():
        match = re.fullmatch(r'([0-9a-fA-F]{64})[ \t]+\*?([^/\\\r\n]+)', line)
        if match and match.group(2) == name:
            matches.append(match.group(1).lower())
    if len(matches) != 1:
        raise UpdateError('Missing or ambiguous installer checksum')
    return matches[0]


def download(home, selected, *, cache=None, client=None, cancelled=lambda: False, progress=None):
    manifest = installation(home)
    client = client or GitHub(github_token())
    # A changed release/asset requires another check and another installation offer.
    fresh = select_release(client.releases(), manifest['version'], manifest.get('channel', 'preview'))
    if fresh != selected:
        raise UpdateError('The offered release changed. Check again before installing.')
    setup = selected['setup']
    sums = selected['checksums']
    expected = checksum(client.read(sums['url'], 128 * 1024, True), setup['name'])
    if setup.get('digest') and setup['digest'] != 'sha256:' + expected:
        raise UpdateError('Release checksum and GitHub asset digest disagree')
    cache = ordinary(cache or cache_directory())
    folder = ordinary(cache / 'downloads')
    folder.mkdir(parents=True, exist_ok=True)
    target = ordinary(folder / setup['name'])
    if target.exists():
        if target.stat().st_size == setup['size'] and hashlib.sha256(target.read_bytes()).hexdigest() == expected:
            return target
        raise UpdateError('An existing download differs and was preserved')
    descriptor, name = tempfile.mkstemp(prefix='.cycode-download-', suffix='.tmp', dir=folder)
    temporary = Path(name)
    digest = hashlib.sha256()
    total = 0
    deadline = time.monotonic() + 600
    try:
        with os.fdopen(descriptor, 'wb') as output, client.open(setup['url'], True) as stream:
            while block := stream.read(128 * 1024):
                if cancelled():
                    raise UpdateError('Download cancelled')
                if time.monotonic() > deadline:
                    raise UpdateError('Download deadline exceeded')
                total += len(block)
                if total > setup['size'] or total > MAX_SETUP_BYTES:
                    raise UpdateError('Installer exceeds its advertised size')
                digest.update(block)
                output.write(block)
                if progress:
                    progress({'phase': 'download', 'bytes': total, 'total': setup['size']})
            output.flush()
            os.fsync(output.fileno())
        if total != setup['size'] or digest.hexdigest() != expected:
            raise UpdateError('Installer checksum or size verification failed')
        if cancelled():
            raise UpdateError('Download cancelled')
        # Publish without replacing any concurrent download.
        if target.exists():
            raise UpdateError('A concurrent download was preserved; check again')
        if os.name == 'nt':
            os.rename(temporary, target)
        else:
            os.link(temporary, target)
            temporary.unlink()
        return target
    finally:
        temporary.unlink(missing_ok=True)


def install_update(home, selected, *, cache=None, client=None, language='en', refresh=True,
                   cancelled=lambda: False, progress=None, runner=None, manager_state=None):
    """Install into a fresh generation and verify activation before reporting success.

    Only called after the manager's explicit installation confirmation. Existing
    Core/MCP processes are never terminated, and no client is launched here.
    """
    from core_installation import installation_root, current_payload, verify_payload
    from core_agents import Manager
    root = installation_root(home)
    setup = download(home, selected, cache=cache, client=client, cancelled=cancelled, progress=progress)
    if cancelled():
        raise UpdateError('Update cancelled before installation')
    if progress:
        progress({'phase': 'installer'})
    command = [str(setup), '/DIR=' + str(root), '/MANAGERUPDATE=1', '/NORESTART',
               '/LANG=' + ('french' if language == 'fr' else 'english')]
    if runner is None:
        if os.name != 'nt':
            raise UpdateError('Automatic installation currently requires Windows')
        runner = lambda argv: subprocess.run(argv, check=False, timeout=3600).returncode
    code = runner(command)
    if code != 0:
        raise UpdateError('Installer did not complete (exit %s). Check the selected Core version before retrying.' % code)
    selected_home = current_payload(root)
    verify_payload(selected_home, selected['version'])
    if ordinary(selected_home) == ordinary(home):
        raise UpdateError('The installer did not select a new Core payload')
    configuration = {'changes': [], 'skipped': []}
    if refresh:
        manager = Manager(selected_home, root=root, state=manager_state)
        try:
            configuration = manager.apply(manager.prepare_refresh())
        except (ValueError, OSError) as error:
            # Activation succeeded. Report configuration work separately instead
            # of claiming that the verified Core installation itself failed.
            configuration = {'changes': [], 'skipped': [str(error)], 'applied': False}
    return {'status': 'installed', 'version': selected['version'], 'root': str(root),
            'home': str(selected_home), 'connections': configuration, 'reloadClientsRequired': True}
