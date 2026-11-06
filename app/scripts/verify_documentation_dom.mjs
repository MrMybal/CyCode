// Component checks only: no browser navigation, script evaluation, resource
// loading, server or native application. This does not validate file:// policy.
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

const [siteArg, libraryArg] = process.argv.slice(2);
if (!siteArg || !libraryArg) throw new Error('Usage: node verify_documentation_dom.mjs SITE CYDOC_CACHE');
const site = resolve(siteArg), library = resolve(libraryArg);
const { Window } = await import(pathToFileURL(`${library}/node_modules/happy-dom/lib/index.js`));
const { mountViewer } = await import(pathToFileURL(`${library}/dist/viewer.js`));
const { createDocumentation } = await import(pathToFileURL(`${library}/dist/index.js`));
const image = (await import(pathToFileURL(`${library}/dist/modules/image.js`))).default;
const gallery = (await import(pathToFileURL(`${library}/dist/modules/gallery.js`))).default;
const data = JSON.parse(await readFile(`${site}/manifest.json`, 'utf8'));
const docs = createDocumentation(data);
assert.equal(docs.resolve('overview', 'fr').fallback, false);
const api = data.pages.find(page => page.id.startsWith('operation-'));
assert.equal(docs.resolve(api.id, 'fr').fallback, true);
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
  assert.match(root.querySelector('h1').textContent, /Visite/);
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
  for (const theme of ['dark','light']) {
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
  const search = root.querySelector('.search-form input');
  search.value = 'plugin'; search.dispatchEvent(new window.Event('input', {bubbles:true}));
  assert.ok(root.querySelectorAll('.search-result').length);
  await input(`#/fr/${api.id}`); assert.ok(root.querySelector('.callout'));
  assert.equal(root.querySelector('article').lang, 'en');
  await input('#/en/tour/interface'); assert.equal(document.documentElement.lang, 'en');
  await input('#/fr/getting-started'); assert.match(root.querySelector('h1').textContent, /Premiers pas/);
  assert.equal(requests, 0);
  assert.ok(loads.includes('image'));
  assert.ok(loads.every(name => name === 'image' || name === 'gallery'));
  console.log(`${data.name}: DOM routing, fallback, local search, themes, media/Escape and accessible controls passed. No browser/native application opened.`);
} finally {
  viewer.destroy(); await window.happyDOM.abort(); window.close();
}
