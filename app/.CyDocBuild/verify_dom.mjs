// Component checks only: no browser navigation, script evaluation, resource
// loading, server or native application. This does not validate file:// policy.
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

const work = resolve(import.meta.dirname);
const site = resolve(process.argv[2] || `${work}/../Documentation`);
const lock = JSON.parse(await readFile(`${work}/cydoc.lock.json`, 'utf8'));
const library = `${work}/.cache/source-${lock.revision.slice(0,7)}`;
const { Window } = await import(pathToFileURL(`${library}/node_modules/happy-dom/lib/index.js`));
const { mountViewer } = await import('cydoc/viewer');
const { createDocumentation } = await import('cydoc');
const image = (await import('cydoc/modules/image')).default;
const gallery = (await import('cydoc/modules/gallery')).default;
const data = JSON.parse(await readFile(`${site}/DocData/manifest.json`, 'utf8'));
const docs = createDocumentation(data);
assert.equal(docs.resolve('overview', 'fr').fallback, false);
const api = data.pages.find(page => page.id.startsWith('operation-'));
assert.equal(docs.resolve(api.id, 'fr').fallback, false);
assert.ok(docs.search('plugin', 'fr').length);

const window = new Window({settings: {
  disableJavaScriptFileLoading: true, enableJavaScriptEvaluation: false,
  enableImageFileLoading: false, enableCSSFileLoading: false
}});
const route = {hash: '#/fr/tour/interface?media=interface-image', href: '#/fr/tour/interface?media=interface-image'};
let scrollTop = 0;
Object.defineProperty(window, 'scrollY', {configurable: true, get: () => scrollTop});
window.scrollTo = options => { scrollTop = options.top; };
for (const [name, value] of Object.entries({
  window, document: window.document, localStorage: window.localStorage,
  location: route, navigator: window.navigator, ResizeObserver: window.ResizeObserver,
  IntersectionObserver: window.IntersectionObserver, matchMedia: window.matchMedia.bind(window),
  requestAnimationFrame: window.requestAnimationFrame.bind(window),
  cancelAnimationFrame: window.cancelAnimationFrame.bind(window)
})) Object.defineProperty(globalThis, name, {configurable: true, value});
const root = document.createElement('div'); document.body.append(root);
let requests = 0;
globalThis.fetch = window.fetch = async () => { requests++; throw new Error('No resource requests in the DOM fixture'); };
const loads = [];
const viewer = mountViewer(root, data, {modules: {
  image: async () => { loads.push('image'); return image; },
  gallery: async () => { loads.push('gallery'); return gallery; }
}});
const settle = async () => { await new Promise(resolve => setImmediate(resolve)); };
const input = async hash => { route.hash = hash; route.href = hash; window.dispatchEvent(new window.Event('hashchange')); await settle(); };
try {
  await settle();
  assert.equal(document.documentElement.lang, 'fr');
  assert.match(root.querySelector('h1').textContent, /Se repérer/);
  assert.equal(root.querySelector('main #interface').tabIndex, -1);
  assert.ok(root.querySelector('.skip-link'));
  assert.ok(root.querySelector('main[tabindex="-1"]'));
  const media = root.querySelector('[data-cydoc-id="interface-image"]');
  const expand = media.querySelector('[data-action="expand"]');
  assert.ok(expand?.getAttribute('aria-label'));
  expand.click(); assert.ok(media.classList.contains('media-expanded'));
  document.dispatchEvent(new window.KeyboardEvent('keydown', {key:'Escape', bubbles:true}));
  assert.equal(media.classList.contains('media-expanded'), false);
  assert.equal(document.querySelectorAll('.media-placeholder').length, 0);
  for (const theme of ['dark','light','midnight','nord','paper','contrast']) {
    root.querySelector(`[data-set-theme="${theme}"]`).click();
    assert.equal(root.dataset.theme, theme);
  }
  root.querySelector('[data-menu]').click(); assert.equal(root.classList.contains('menu-open'), true);
  root.querySelector('[data-dismiss-menu]').click(); assert.equal(root.classList.contains('menu-open'), false);
  // Exercise this manual's actual gallery model in an isolated component fixture.
  const config = JSON.parse(root.querySelector('[data-cydoc-id="interface-gallery"]').dataset.config);
  const galleryHost = document.createElement('div'); document.body.append(galleryHost);
  const abort = new AbortController();
  const disposeGallery = gallery.mount(galleryHost, config, {
    locale:'fr', storageKey:`${data.name}:gallery-fixture`, signal:abort.signal,
    loadModule:async kind => { assert.equal(kind,'image'); return image; }
  });
  await settle();
  galleryHost.querySelector('[data-gallery="next"]').click(); await settle();
  assert.match(galleryHost.querySelector('output').textContent, /^2 \/ /);
  assert.equal(galleryHost.querySelectorAll('[role="tab"][aria-selected="true"]').length, 1);
  disposeGallery(); abort.abort(); galleryHost.remove();
  scrollTop = 420; root.querySelector('.side-nav').scrollTop = 90;
  await input('#/fr/plugins');
  assert.equal(scrollTop, 0);
  assert.equal(root.querySelector('.side-nav').scrollTop, 90);
  await input('#/fr/tour'); assert.equal(scrollTop, 420);
  await input('#/fr/plugins');
  document.dispatchEvent(new window.KeyboardEvent('keydown', {key:'k', ctrlKey:true, bubbles:true}));
  assert.equal(root.querySelector('.search-dialog').open, true);
  assert.ok(root.querySelector('[data-header-search]'));
  assert.ok(root.querySelectorAll('.nav-group .nav-children').length > 20);
  const link = root.querySelector('[data-page="plugin-api"]');
  assert.ok(link.dataset.tooltipTitle && link.dataset.tooltipDescription);
  const search = root.querySelector('.search-form input');
  search.value = 'plugin'; search.dispatchEvent(new window.Event('input', {bubbles:true}));
  assert.ok(root.querySelectorAll('.search-result').length);
  await input(`#/fr/${api.id}`); assert.equal(root.querySelectorAll('.translation-warning').length, 0);
  assert.equal(root.querySelector('article').lang, 'fr');
  await input('#/en/tour/interface'); assert.equal(document.documentElement.lang, 'en');
  await input('#/fr/getting-started'); assert.match(root.querySelector('h1').textContent, /Ouvrir son premier projet/);

  for (const locale of ['en','fr']) {
    for (const page of ['refactoring','plugin-api']) {
      await input(`#/${locale}/${page}/user-snippets`);
      assert.equal(root.querySelector('#user-snippets').tabIndex,-1);
      assert.ok(root.querySelector('article').textContent.includes('settings.write'));
      assert.equal(root.querySelectorAll('.translation-warning').length,0);
    }
    for (const op of ['read','save','draft']) {
      const id=`operation-snippets-user-${op}-snippets-user-${op}`;
      assert.equal(docs.resolve(id,locale).fallback,false);
      await input(`#/${locale}/${id}`);
      assert.ok(root.querySelector('article').textContent.includes(`op_snippets_user_${op}`));
      assert.ok(root.querySelector('article').textContent.includes('generation'));
      assert.equal(root.querySelectorAll('.translation-warning').length,0);
    }
  }
  for (const locale of ['en','fr']) {
    for (const page of ['refactoring','plugin-api']) {
      await input(`#/${locale}/${page}/capture-snippet`);
      assert.equal(root.querySelector('#capture-snippet').tabIndex,-1);
      assert.ok(root.querySelector('article').textContent.includes('snippets_user_draft'));
      assert.equal(root.querySelectorAll('.translation-warning').length,0);
    }
    assert.ok(docs.search('snippets_user_draft',locale).length);
  }
  for (const locale of ['en','fr']) {
    await input(`#/${locale}/navigation/header-source`);
    assert.equal(root.querySelector('#header-source').tabIndex,-1);
    assert.ok(root.querySelector('article').textContent.includes('Alt+O'));
    assert.ok(root.querySelector('article').textContent.includes('Ctrl+K, Ctrl+O'));
    assert.ok(root.querySelector('article').textContent.includes('include/library/Widget.hpp'));
    assert.ok(root.querySelector('article').textContent.includes(locale==='en'?'Workspace index':'Index du projet'));
    assert.equal(root.querySelectorAll('.translation-warning').length,0);
    const id='operation-workspace-counterparts-workspace-counterparts';
    assert.equal(docs.resolve(id,locale).fallback,false);
    await input(`#/${locale}/${id}`);
    assert.ok(root.querySelector('article').textContent.includes('expectedDocumentId'));
    assert.ok(root.querySelector('article').textContent.includes('unreadable'));
    for (const field of ['workspace-index','generation','missing','busy','stale','ready','partial','changed'])
      assert.ok(root.querySelector('article').textContent.includes(field),field);
    assert.equal(root.querySelectorAll('.translation-warning').length,0);
    assert.ok(docs.search('workspace_counterparts',locale).length);
  }
  for (const locale of ['en','fr']) {
    await input(`#/${locale}/navigation/open-include`);
    assert.equal(root.querySelector('#open-include').tabIndex,-1);
    assert.ok(root.querySelector('article').textContent.includes('include/library/Widget.inc'));
    assert.equal(root.querySelectorAll('.translation-warning').length,0);
    const id='operation-workspace-include-workspace-include';
    assert.equal(docs.resolve(id,locale).fallback,false);
    await input(`#/${locale}/${id}`);
    for(const field of ['expectedDocumentId','position','source-relative','angled','generation'])
      assert.ok(root.querySelector('article').textContent.includes(field),field);
    assert.equal(root.querySelectorAll('.translation-warning').length,0);
    assert.ok(docs.search('workspace_include',locale).length);
  }
  for (const locale of ['en','fr']) {
    await input(`#/${locale}/navigation/related-results`);
    assert.equal(document.documentElement.lang,locale);
    assert.equal(root.querySelector('#related-results').tabIndex,-1);
    assert.equal(root.querySelectorAll('.translation-warning').length,0);
    const article=root.querySelector('article').textContent;
    for(const phrase of locale==='en'
      ? ['Find related C/C++ files...', 'Retry this lookup', 'Refresh lookup index and retry',
         'Copy all result paths', 'Copy lookup report (JSON)', 'Help for this navigation', 'Shift+F1']
      : ['Rechercher les fichiers C/C++ associés...', 'Relancer cette recherche',
         'Actualiser l’index de recherche et relancer', 'Copier tous les chemins des résultats',
         'Copier le rapport de recherche (JSON)', 'Aide pour cette navigation', 'Maj+F1'])
      assert.ok(article.includes(phrase),phrase);
    for(const word of ['indexRevision','generation','30','32','200'])assert.ok(article.includes(word),word);
    const physical=[...root.querySelectorAll('pre code')].some(code =>
      code.textContent.includes('# /* directive */ in') &&
      code.textContent.includes('clude /* header */') &&
      code.textContent.includes('"library/Widget.inc"'));
    assert.ok(physical,'The documented continued literal stays in a code block');
    const id='operation-workspace-files-workspace-files';
    assert.equal(docs.resolve(id,locale).fallback,false);
    await input(`#/${locale}/${id}`);
    const contract=root.querySelector('article').textContent;
    for(const field of ['indexRevision','generation','minimum','nextOffset'])assert.ok(contract.includes(field),field);
    assert.equal(root.querySelectorAll('.translation-warning').length,0);
    assert.ok(docs.search('workspace_files',locale).length);
  }
  assert.ok(docs.search('snippets_user_save','fr').length);
  assert.equal(docs.resolve('operation-snippets-complete-snippets-complete','fr').fallback, false);
  assert.ok(docs.search('snippets_complete','fr').length);
  for (const locale of ['en','fr']) {
    await input(`#/${locale}/refactoring/snippet-catalogue`);
    assert.equal(document.documentElement.lang,locale);
    assert.equal(root.querySelector('#snippet-catalogue').tabIndex,-1);
    assert.ok(root.querySelector('article').textContent.includes('.cycode/snippets.code-snippets'));
    assert.ok(root.querySelector('article').textContent.includes('Ctrl+J'));
    assert.ok(root.querySelector('article').textContent.includes('Ctrl+K, Ctrl+X'));
    assert.equal(root.querySelectorAll('.translation-warning').length,0);
    await input(`#/${locale}/operation-snippets-complete-snippets-complete`);
    assert.ok(root.querySelector('article').textContent.includes('snippets_complete'));
    assert.equal(root.querySelectorAll('.translation-warning').length,0);
  }
  for (const locale of ['en','fr']) {
    assert.ok(docs.search('TM_SELECTED_TEXT',locale).length);
    await input(`#/${locale}/refactoring/snippet-selection`);
    assert.equal(document.documentElement.lang,locale);
    assert.equal(root.querySelector('#snippet-selection').tabIndex,-1);
    assert.ok(root.querySelector('article').textContent.includes('TM_SELECTED_TEXT'));
    assert.ok(root.querySelector('article').textContent.includes('Ctrl+Alt+J'));
    assert.equal(root.querySelectorAll('.translation-warning').length,0);
    await input(`#/${locale}/operation-snippets-complete-snippets-complete`);
    assert.ok(root.querySelector('article').textContent.includes('selection'));
    assert.ok(root.querySelector('article').textContent.includes('TM_SELECTED_TEXT'));
    assert.equal(root.querySelectorAll('.translation-warning').length,0);
  }
  for (const locale of ['en','fr']) {
    for (const [page,anchor] of [['refactoring','plugin-snippets'],['plugins','snippet-catalogues'],
        ['plugin-api','snippet-catalogues'],['unreal','code-snippets']]) {
      await input(`#/${locale}/${page}/${anchor}`);
      assert.equal(document.documentElement.lang,locale);
      assert.equal(root.querySelector('#'+anchor).tabIndex,-1);
      assert.equal(root.querySelectorAll('.translation-warning').length,0);
    }
    assert.ok(root.querySelector('article').textContent.includes('ue_log'));
    assert.ok(docs.search('CYCODE_HOST_FEATURE_SNIPPETS_V2',locale).length);
    await input(`#/${locale}/operation-snippets-complete-snippets-complete`);
    assert.ok(root.querySelector('article').textContent.includes('plugin'));
    assert.equal(root.querySelectorAll('.translation-warning').length,0);
  }
  const chapters={
    'workspace-files':['files','preview','trash','monitoring'],
    'project-templates':['create','destinations','plugins'],
    'project-tasks':['tasks','output','cancel'],
    'symbols':['structure','references','hierarchy'],
    'diagnostics':['problems','output'],
    'debug-execution':['stepping','step-targets','destinations'],
    'debug-breakpoints':['data','instructions'],
    'update-storage':['downloads','storage','history'],
    'custom-vcs':['provider','routing'],
    'reader':['open','navigate','media']
  };
  for(const locale of ['en','fr']) {
    for(const [page,anchors] of Object.entries(chapters)) {
      for(const anchor of anchors) {
        await input(`#/${locale}/${page}/${anchor}`);
        assert.equal(document.documentElement.lang,locale);
        assert.equal(root.querySelector('#'+anchor).tabIndex,-1,`${page}#${anchor}`);
        assert.equal(root.querySelectorAll('.translation-warning').length,0);
      }
    }
    const canonical=docs.canonicalRoute({locale,pageId:'reference-app-docs-resources'});
    assert.equal(canonical.pageId,'workspace-files');assert.equal(canonical.anchor,'files');
    await input(`#/${locale}/reference-app-docs-resources`);
    assert.equal(root.querySelector('#files').tabIndex,-1);
    await input(`#/${locale}/refactoring/user-snippets`);
    assert.ok(!root.querySelector('article').textContent.includes(locale==='en'?'global user snippet storage and':'le stockage global d’extraits utilisateur et'));
    await input(`#/${locale}/workspace-files/preview`);
    const graphic=root.querySelector('article svg[role="img"]');
    assert.ok(graphic?.getAttribute('aria-label') || graphic?.querySelector('title'));
  }
  const native=await readFile(`${work}/../src/HelpTopics.h`,'utf8');
  const help=[...native.matchAll(/\{"([a-zA-Z]+)", "([a-z0-9-]+)", "([a-z0-9-]+)"\}/g)];
  assert.equal(help.length,26);
  for(const locale of ['en','fr'])for(const [,panel,page,anchor] of help) {
    const route=docs.resolve(page,locale);assert.equal(route.fallback,false,panel);
    assert.ok(route.page.headings.some(heading=>heading.id===anchor),panel);
  }
  assert.equal(requests, 0);
  assert.ok(loads.includes('image'));
  assert.ok(loads.every(name => name === 'image' || name === 'gallery'));
  console.log(`${data.name}: DOM routing, complete translations, local search, six themes, media/Escape and accessible controls passed. No browser/native application opened.`);
} finally {
  viewer.destroy(); await window.happyDOM.abort(); window.close();
}
