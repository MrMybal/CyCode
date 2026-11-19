"""Select a clean, exact CyMCPSDK source; preserve any unrelated local checkout."""
import json
import os
from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parents[2]


def ensure_source(explicit=None):
    lock = json.loads((ROOT/'app/cymcp-source.lock.json').read_text('utf-8'))
    local = Path(explicit or os.environ.get('CYCODE_CYMCP_SOURCE') or ROOT.parent/'CyMCPSDK')
    source = local if explicit or local.exists() else ROOT/'runtime/deps'/('cymcp-'+lock['revision'])
    source = source.absolute()
    git = ['git', '-c', 'safe.directory='+source.as_posix(), '-C', str(source)]
    if not source.exists():
        if explicit: raise FileNotFoundError('Requested local SDK source is unavailable: '+str(source))
        source.parent.mkdir(parents=True, exist_ok=True)
        environment = {**os.environ, 'GIT_TERMINAL_PROMPT':'0', 'GCM_INTERACTIVE':'never'}
        subprocess.run(['git','-c','credential.interactive=false','clone','--no-checkout',lock['repository'],str(source)],env=environment,check=True)
        subprocess.run([*git,'checkout','--detach',lock['revision']],env=environment,check=True)
    observed = subprocess.check_output([*git,'rev-parse','HEAD'],text=True).strip()
    dirty = subprocess.check_output([*git,'status','--porcelain','--untracked-files=no'],text=True).strip()
    if observed != lock['revision'] or dirty:
        raise ValueError('SDK differs from the exact clean pin; local changes are preserved: '+str(source))
    return source
