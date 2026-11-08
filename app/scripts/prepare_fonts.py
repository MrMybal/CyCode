"""Verify offline UI resources; --regenerate explicitly rebuilds the pinned icon subset.

Ordinary builds consume checked-in TTFs and never download fonts or use fontTools.
Regeneration only: python -m pip install fonttools==4.60.1
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile
import urllib.request

DIRECTORY=Path(__file__).resolve().parents[1]/'resources/fonts'

def digest(data): return hashlib.sha256(data).hexdigest()

def verify():
    lock=json.loads((DIRECTORY/'font-resources.json').read_text('utf-8'))
    expected={'Roboto-Regular.ttf','Roboto-Bold.ttf','JetBrainsMono-Regular.ttf',
              'CyCodeControls-Regular.ttf','Roboto-LICENSE.txt','JetBrainsMono-OFL.txt','Codicons-LICENSE.txt'}
    if set(lock['files'])!=expected: raise RuntimeError('Unexpected font resource inventory')
    for name,expected_hash in lock['files'].items():
        path=DIRECTORY/name
        if path.is_symlink() or digest(path.read_bytes())!=expected_hash:
            raise RuntimeError('Changed font resource: '+name)
    print('Verified offline font resources and original license notices.')
    return lock

def regenerate(lock):
    import fontTools
    from fontTools.ttLib import TTFont
    from fontTools import subset
    if 'fonttools=='+fontTools.__version__!=lock['generator']:
        raise RuntimeError('Install exactly '+lock['generator']+' for reproducible resource authoring')
    def fetch(item):
        request=urllib.request.Request(item['url'],headers={'User-Agent':'CyCode-font-authoring'})
        with urllib.request.urlopen(request,timeout=60) as response: data=response.read(4*1024*1024+1)
        if len(data)>4*1024*1024 or digest(data)!=item['sha256']:
            raise RuntimeError('Pinned font source digest mismatch')
        return data
    outputs={name:fetch(item) for name,item in lock['sources'].items() if name!='codicons'}
    codicons=lock['sources']['codicons']
    archive=fetch(codicons)
    with tarfile.open(fileobj=io.BytesIO(archive),mode='r:gz') as package:
        def member(name):
            entry=package.getmember(name)
            if not entry.isfile() or entry.size>1024*1024: raise RuntimeError('Unexpected icon archive member')
            return package.extractfile(entry).read()
        data=member('package/dist/codicon.ttf')
        css=member('package/dist/codicon.css')
        notice=member('package/LICENSE')
    if (digest(data)!=codicons['fontSha256'] or digest(css)!=codicons['cssSha256'] or
        digest(notice)!=codicons['licenseSha256']): raise RuntimeError('Unexpected icon font, mapping or notice')
    outputs['Codicons-LICENSE.txt']=notice
    font=TTFont(io.BytesIO(data),recalcTimestamp=False)
    original=font.getBestCmap()
    mapped={item['codepoint']:original[item['sourceCodepoint']] for item in lock['icons'].values()}
    for table in font['cmap'].tables:
        if table.isUnicode(): table.cmap=dict(mapped)
    options=subset.Options(); options.recalc_timestamp=False
    smaller=subset.Subsetter(options=options); smaller.populate(unicodes=mapped); smaller.subset(font)
    names={1:'CyCode Controls',2:'Regular',3:'CyCode Controls 1.0',4:'CyCode Controls',6:'CyCodeControls-Regular',
           16:'CyCode Controls',17:'Regular',13:'Adapted from Microsoft Codicons 0.0.36, CC BY 4.0. Glyphs remapped to CyUI public icon identifiers; subset and family renamed.'}
    for identity,value in names.items(): font['name'].setName(value,identity,3,1,0x409)
    result=io.BytesIO(); font.save(result); outputs['CyCodeControls-Regular.ttf']=result.getvalue()
    if set(outputs)!=set(lock['files']): raise RuntimeError('Incomplete font reconstruction')
    for name,data in outputs.items():
        if digest(data)!=lock['files'][name]: raise RuntimeError('Reconstruction differs: '+name)
    # Check every output before replacing any resource. No directory is removed.
    for name,data in outputs.items():
        path=DIRECTORY/name
        if path.is_symlink(): raise RuntimeError('Font resource must not be a symbolic link')
        path.write_bytes(data)
    verify()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--regenerate',action='store_true',help='Explicitly download verified upstream sources and reproduce assets')
    args=parser.parse_args()
    lock=json.loads((DIRECTORY/'font-resources.json').read_text('utf-8'))
    if args.regenerate: regenerate(lock)
    else: verify()

if __name__=='__main__': main()
