"""Migrate application-owned sources and compile a complete local CyDoc manual."""
import argparse, hashlib, json, re, shutil, subprocess, unicodedata
from pathlib import Path
from urllib.parse import unquote, urlsplit
from cydoc_dependency import resolve

DOC=Path(__file__).resolve().parents[1]/'documentation'

def slug(value):
    value=unicodedata.normalize('NFD',value)
    value=''.join(c for c in value if not unicodedata.combining(c))
    return re.sub('[^a-z0-9]+','-',value.lower()).strip('-') or 'section'

def publish(source,destination):
    destination=destination.resolve()
    marker=destination/'cydoc-export.json'
    # An exported directory may itself be the input (for Pages publication).
    # The ownership marker describes payload files, never its own changing hash.
    source_files={p.relative_to(source).as_posix():p for p in source.rglob('*') if p.is_file() and p.relative_to(source).as_posix()!='cydoc-export.json'}
    if destination==source: return
    previous={}
    if destination.exists() and any(destination.iterdir()):
        if not marker.is_file(): raise RuntimeError('Refuse to replace an unrecognized documentation directory')
        previous=json.loads(marker.read_text())['files']
        actual={p.relative_to(destination).as_posix() for p in destination.rglob('*') if p.is_file()}
        if actual-set(previous)-{'cydoc-export.json'}: raise RuntimeError('Preserve unknown files in the documentation output')
        for name,digest in previous.items():
            path=destination/name
            if path.is_symlink() or not path.resolve().is_relative_to(destination): raise RuntimeError('Unsafe previous documentation path')
            if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
                raise RuntimeError('Preserve local changes to the generated documentation: '+name)
    destination.mkdir(parents=True,exist_ok=True)
    for name,path in source_files.items():
        target=destination/name
        if path.is_symlink() or not target.resolve().is_relative_to(destination): raise RuntimeError('Unsafe documentation output path')
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
    for name in set(previous)-set(source_files):
        target=destination/name
        if not target.resolve().is_relative_to(destination) or target.is_symlink(): raise RuntimeError('Unsafe previous documentation path')
        target.unlink(missing_ok=True)
    marker.write_text(json.dumps({'format':1,'generator':'CyDoc application help','files':{n:hashlib.sha256(p.read_bytes()).hexdigest() for n,p in source_files.items()}},indent=2)+'\n',encoding='utf-8')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cydoc-source',type=Path,help='Clean optional checkout matching the source lock')
    parser.add_argument('--cydoc-cache',type=Path,help='Prepared pinned generator cache')
    parser.add_argument('--output',type=Path,action='append',help='Publish the complete export into this owned directory')
    args=parser.parse_args()
    settings=json.loads((DOC/'project.json').read_text(encoding='utf-8'))
    root=(DOC/settings['repositoryRoot']).resolve()
    lock=json.loads((DOC/'cydoc.lock.json').read_text(encoding='utf-8'))
    if args.cydoc_cache:
        library=args.cydoc_cache.resolve()
        if (library/'.cydoc-revision').read_text().strip()!=lock['revision']: raise RuntimeError('Wrong CyDoc generator revision')
        node=shutil.which('node')
        if not node or int(subprocess.check_output([node,'--version'],text=True).strip().lstrip('v').split('.')[0])<22:
            raise RuntimeError('Node.js 22+ is required')
    else: node,library=resolve(root,lock,args.cydoc_source)
    descriptor=json.loads((root/settings['descriptor']).read_text(encoding='utf-8'))
    originals=sorted({p.resolve() for pattern in settings['documents'] for p in root.glob(pattern) if p.is_file()},key=lambda p:p.relative_to(root).as_posix().casefold())
    authored=sorted((DOC/'manual').rglob('*.md'))
    assets=sorted(p for p in (DOC/'assets').rglob('*') if p.is_file())
    source_files=[root/name for name in settings['sources']]
    media_files=[(root/item['source'],item['target']) for item in settings.get('media',[])]
    inputs=originals+authored+assets+source_files+[p for p,_ in media_files]+[root/settings['descriptor'],DOC/'project.json',DOC/'cydoc.lock.json',Path(__file__),Path(__file__).with_name('cydoc_dependency.py')]
    fingerprint=hashlib.sha256()
    for path in inputs:
        if path.is_symlink() or not path.is_file(): raise RuntimeError('Missing/linked documentation input: '+str(path))
        fingerprint.update(path.name.encode());fingerprint.update(path.read_bytes())
    # Original Markdown can refer to distributed local figures/code. Include
    # their bytes in the input key before preparing a fresh export.
    linked=set()
    for path in originals:
        for ref in re.findall(r'!?\[[^\]]*\]\(([^)]+)\)',path.read_text(encoding='utf-8')):
            url=ref.strip('<>').split(' "')[0]
            if not urlsplit(url).scheme and not url.startswith('#'):
                candidate=(path.parent/unquote(url.split('#')[0])).resolve()
                if candidate.is_file() and candidate.is_relative_to(root): linked.add(candidate)
    for path in sorted(linked): fingerprint.update(path.read_bytes())
    key=fingerprint.hexdigest()
    project=root/'runtime/documentation'/('build-'+key[:16])
    output=project/'site'
    receipt=project/'build.json'
    if receipt.is_file() and (output/'index.html').is_file():
        for destination in args.output or []: publish(output,destination)
        publish(output,root/'runtime/documentation/site')
        print('CyDoc: current export '+str(args.output or output));return
    if project.exists(): raise RuntimeError('Preserve incomplete documentation build; inspect '+str(project))
    project.mkdir(parents=True)
    for locale in ['en','fr']: (project/'manual'/locale).mkdir(parents=True)
    shutil.copytree(DOC/'assets',project/'assets',dirs_exist_ok=True)
    for path,name in media_files:
        target=project/'assets'/name
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
    for path in authored:
        target=project/'manual'/path.relative_to(DOC/'manual');target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
    mapping={path:'reference-'+slug(path.relative_to(root).as_posix().removesuffix('.md')) for path in originals}
    if root/'LISEZ-MOI.md' in mapping and root/'README.md' in mapping:
        mapping[root/'LISEZ-MOI.md']=mapping[root/'README.md']
    missing=[];resources={}
    def asset(path):
        existing=next((name for source,name in media_files if source.resolve()==path),None)
        if existing: return 'assets/'+existing
        name='imported/'+path.relative_to(root).as_posix()
        target=project/'assets'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
        resources[path]=name;return 'assets/'+name
    def rewrite(path,text):
        in_fence=False;result=[]
        for line in text.splitlines():
            if re.match(r'^\s*(`{3,}|~{3,})',line): in_fence=not in_fence
            if not in_fence:
                def html_image(match):
                    attrs=dict(re.findall(r'([a-z]+)=[\"\']([^\"\']*)[\"\']',match[0],re.I))
                    return '!['+attrs.get('alt','')+']('+attrs.get('src','')+')'
                line=re.sub(r'<img\b[^>]*>',html_image,line,flags=re.I)
                line=re.sub(r'</?p\b[^>]*>','',line,flags=re.I)
                # README thumbnail links become CyDoc's in-window image viewer.
                line=re.sub(r'\[(!\[[^\]]*\]\([^)]+\))\]\([^)]+\)',r'\1',line)
                def link(match):
                    image,label,url=match.group(1),match.group(2),match.group(3)
                    target_url=url.strip('<>').split(' "')[0]
                    remote=settings.get('remoteAssetAliases',{}).get(target_url.split('?')[0])
                    if image and remote:
                        local=(root/remote).resolve()
                        if not local.is_relative_to(root) or not local.is_file(): raise RuntimeError('Missing local image counterpart: '+remote)
                        return f'![{label}]({asset(local)})'
                    if urlsplit(target_url).scheme or target_url.startswith('#'): return match[0]
                    name,_,anchor=target_url.partition('#')
                    target=(path.parent/unquote(name)).resolve()
                    if target in mapping: return f'[{label}](doc:{mapping[target]}'+('#'+anchor if anchor else '')+')'
                    selected={ (root/source).resolve(): 'source-'+slug('api-sources/'+Path(source).name) for source in settings['sources'] }
                    if target in selected: return f'[{label}](doc:{selected[target]}'+('#'+anchor if anchor else '')+')'
                    if target.is_file() and target.is_relative_to(root):
                        if image: return f'![{label}]({asset(target)})'
                        # Source files not selected for indexing stay available
                        # through an immutable repository reference, without
                        # importing private dependency trees into the manual.
                        resources[target]='external-source-reference'
                        return f'[{label}]({settings["repository"]}/blob/{settings["sourceRevision"]}/{target.relative_to(root).as_posix()}'+('#'+anchor if anchor else '')+')'
                    missing.append({'page':mapping[path],'link':target_url})
                    return f'{label} (`{target_url}`; legacy local reference)'
                line=re.sub(r'(!?)\[([^\]]*)\]\(([^)]+)\)',link,line)
            result.append(line)
        return '\n'.join(result)+'\n'
    for index,path in enumerate(originals):
        text=path.read_text(encoding='utf-8-sig')
        title=re.search(r'^#\s+(.+)',text,re.M)
        title=(title[1] if title else path.stem).replace(':',' —').replace('\n',' ')
        page=mapping[path]
        french=path.name=='LISEZ-MOI.md'
        metadata=f'---\nid: {page}\ntitle: {title}\nsection: Technical reference\norder: {100+index}\n'+('sourceRevision: 0' if french else 'revision: '+hashlib.sha256(text.encode()).hexdigest()[:12])+'\n---\n\n'
        body=rewrite(path,text) if path.suffix.lower()=='.md' else '```text\n'+text+'\n```\n'
        body+=settings.get('legacyPageSections',{}).get(path.relative_to(root).as_posix(),'')
        if path.name.lower()=='validation.md': body='> Historical validation evidence. Read the dates and the remaining limitations before relying on a result.\n\n'+body
        (project/'manual'/('fr' if french else 'en')/f'{page}.md').write_text(metadata+body,encoding='utf-8')
    references='\n'.join(f'- [{path.stem}](doc:{mapping[path]})' for path in originals)
    for locale in ['en','fr']:
        title='Technical reference' if locale=='en' else 'Références techniques'
        intro='Original application documentation, preserved in English. Missing translations fall back to English.' if locale=='en' else 'Documentation originale conservée en anglais. Les traductions absentes utilisent le repli anglais.'
        (project/'manual'/locale/'reference.md').write_text(f'---\nid: reference\ntitle: {title}\nsection: Reference\norder: 80\n'+('revision: 1' if locale=='en' else 'sourceRevision: 1')+f'\n---\n\n{intro}\n\n{references}\n',encoding='utf-8')
    for name in settings['sources']:
        source=root/name;target=project/'api-sources'/source.name
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,target)
    symbols=[]
    for operation in descriptor['operations']:
        schema=operation.get('inputSchema',{})
        symbols.append({'key':operation['id'],'name':operation['id'],'kind':'operation','language':'json',
            'signature':json.dumps({'operation':operation['id'],'arguments':'See the input schema below'},indent=2),
            'description':operation.get('description','')+'\n\n```details\n'+json.dumps({'title':'Input and output schemas','body':'```text\n'+json.dumps({'input':schema,'output':operation.get('outputSchema',{})},indent=2,ensure_ascii=False)+'\n```'},ensure_ascii=False)+'\n```',
            'parameters':[{'name':name,'type':str(value.get('type','schema')),'optional':name not in schema.get('required',[]),'description':value.get('description','')} for name,value in schema.get('properties',{}).items()],
            'returnType':'JSON','returns':'The output schema is defined above. Jobs, identities and permissions belong to CyToolsCore.'})
    (project/'api.json').write_text(json.dumps({'symbols':symbols},ensure_ascii=False)+'\n',encoding='utf-8')
    api_links='\n'.join(f'- [`{s["name"]}`](doc:operation-{slug(s["name"])}-{slug(s["key"])})' for s in symbols)
    for locale in ['en','fr']:
        (project/'manual'/locale/'api-links.md').write_text(f'---\nid: api-links\ntitle: API / MCP\nsection: Reference\norder: 70\n'+('revision: 1' if locale=='en' else 'sourceRevision: 1')+f'\n---\n\n'+('Generated from the application descriptor. This is a schema reference, not a C++ compiler index.\n\n' if locale=='en' else 'Référence générée depuis le descripteur de l’application. Les schémas anglais utilisent le repli de langue ; cet index ne remplace pas un compilateur C++.\n\n')+api_links+'\n',encoding='utf-8')
    config={'name':settings['name'],'version':descriptor['version'],'defaultLocale':'en','homePage':'overview','locales':[{'code':'en','label':'English'},{'code':'fr','label':'Français'}],
        'content':'manual','assets':'assets','out':'site','modules':['image','gallery'],'translationReport':True,
        'sources':['api-sources'],'sourceIndex':{'enabled':True,'includeDirs':['api-sources']},
        'apiImports':[{'format':'cydoc','path':'api.json','prefix':'operation','locale':'en'}],
        'aliases':settings.get('aliases',{})}
    (project/'cydoc.config.json').write_text(json.dumps(config,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    subprocess.run([node,str(library/'tools/cli.mjs'),'check',str(project/'cydoc.config.json')],check=True)
    shutil.copyfile(DOC/'cydoc.lock.json',output/'cydoc.lock.json')
    notices=output/'licenses';notices.mkdir()
    for name,path in {'CyDoc.txt':library/'LICENSE','Marked.txt':library/'node_modules/marked/LICENSE.md','PrismJS.txt':library/'node_modules/prismjs/LICENSE','esbuild.txt':library/'node_modules/esbuild/LICENSE.md'}.items():
        if not path.is_file(): raise RuntimeError('Missing dependency notice: '+str(path))
        shutil.copyfile(path,notices/name)
    migration={'format':1,'cydocRevision':lock['revision'],'modules':config['modules'],'originalPages':len(originals),'apiOperations':len(symbols),
               'sourceFiles':settings['sources'],'legacyMissingLinks':missing,'pageMap':{p.relative_to(root).as_posix():i for p,i in mapping.items()},'fingerprint':key}
    (output/'migration-report.json').write_text(json.dumps(migration,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    receipt.write_text(json.dumps(migration,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    publish(output,root/'runtime/documentation/site')
    for destination in args.output or []: publish(output,destination)
    print('CyDoc export ready: '+str(args.output or output))
if __name__=='__main__': main()
