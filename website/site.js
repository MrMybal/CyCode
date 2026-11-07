'use strict';
(() => {
  const copy = JSON.parse(document.querySelector('#site-content').textContent);
  const config = JSON.parse(document.querySelector('#site-config').textContent);
  const dialog = document.querySelector('#image-dialog');
  let language = new URL(location.href).searchParams.get('lang') === 'fr' ? 'fr' : 'en';
  let activeImage = null;
  let previousFocus = null;
  const text = key => copy[key]?.[language] ?? copy[key]?.en ?? key;
  const imageButtons = () => [...document.querySelectorAll('[data-zoom]')];
  const asset = file => config.assets[file];
  function updateDialog() {
    if (!activeImage) return;
    const buttons = imageButtons();
    const original = activeImage.querySelector('img');
    dialog.querySelector('img').src = original.currentSrc || original.src;
    dialog.querySelector('img').alt = text(activeImage.dataset.zoom);
    dialog.querySelector('#image-caption').textContent = text(activeImage.dataset.zoom);
    dialog.querySelector('.image-counter').textContent = `${buttons.indexOf(activeImage) + 1} / ${buttons.length}`;
  }
  function translate(next, updateAddress = false) {
    language = next === 'fr' ? 'fr' : 'en';
    document.documentElement.lang = language;
    document.title = text('title');
    document.querySelector('meta[name="description"]').content = text('description');
    document.querySelector('meta[property="og:title"]').content = text('title');
    document.querySelector('meta[property="og:description"]').content = text('description');
    for (const node of document.querySelectorAll('[data-i18n]')) node.textContent = text(node.dataset.i18n);
    for (const node of document.querySelectorAll('[data-i18n-alt]')) node.alt = text(node.dataset.i18nAlt);
    for (const node of document.querySelectorAll('[data-i18n-aria]')) node.setAttribute('aria-label', text(node.dataset.i18nAria));
    for (const button of document.querySelectorAll('[data-lang]')) button.setAttribute('aria-pressed', String(button.dataset.lang === language));
    for (const link of document.querySelectorAll('[data-doc]')) link.href = `${config.documentation}#/${language}/${link.dataset.doc}`;
    document.querySelector('.primary').setAttribute('aria-label', language === 'fr' ? 'Navigation principale' : 'Main navigation');
    if (updateAddress) {
      const url = new URL(location.href); url.searchParams.set('lang', language);
      history.replaceState(null, '', url);
    }
    if (dialog.open) updateDialog();
  }
  for (const button of document.querySelectorAll('[data-lang]')) button.addEventListener('click', () => translate(button.dataset.lang, true));
  for (const gallery of document.querySelectorAll('[data-gallery]')) {
    const tabs = [...gallery.querySelectorAll('[role="tab"]')];
    const panel = gallery.querySelector('[role="tabpanel"]');
    function choose(tab) {
      for (const other of tabs) { other.setAttribute('aria-selected', String(other === tab)); other.tabIndex = other === tab ? 0 : -1; }
      panel.setAttribute('aria-labelledby', tab.id);
      const button = panel.querySelector('[data-zoom]');
      button.dataset.zoom = tab.dataset.galleryCaption;
      const image = panel.querySelector('img'); image.src = asset(tab.dataset.galleryFile);
      image.dataset.i18nAlt = tab.dataset.galleryCaption;
      panel.querySelector('figcaption').dataset.i18n = tab.dataset.galleryCaption;
      translate(language);
    }
    for (const tab of tabs) {
      tab.addEventListener('click', () => choose(tab));
      tab.addEventListener('keydown', event => {
        if (!['ArrowLeft','ArrowRight','Home','End'].includes(event.key)) return;
        event.preventDefault();
        const index = event.key === 'Home' ? 0 : event.key === 'End' ? tabs.length - 1 :
          (tabs.indexOf(tab) + (event.key === 'ArrowRight' ? 1 : -1) + tabs.length) % tabs.length;
        choose(tabs[index]); tabs[index].focus();
      });
    }
  }
  for (const button of imageButtons()) button.addEventListener('click', () => {
    previousFocus = button; activeImage = button; updateDialog();
    dialog.showModal(); document.body.classList.add('modal-open');
    dialog.querySelector('[data-close]').focus();
  });
  const close = () => { if (dialog.open) dialog.close(); };
  dialog.querySelector('[data-close]').addEventListener('click', close);
  function move(direction) {
    const buttons = imageButtons();
    activeImage = buttons[(buttons.indexOf(activeImage) + direction + buttons.length) % buttons.length];
    updateDialog();
  }
  dialog.querySelector('[data-previous]').addEventListener('click', () => move(-1));
  dialog.querySelector('[data-next]').addEventListener('click', () => move(1));
  dialog.addEventListener('keydown', event => {
    if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') { event.preventDefault(); move(event.key === 'ArrowRight' ? 1 : -1); }
  });
  dialog.addEventListener('click', event => { if (event.target === dialog) close(); });
  dialog.addEventListener('close', () => { document.body.classList.remove('modal-open'); previousFocus?.focus(); activeImage = null; });
  const chapters = [...document.querySelectorAll('.chapter-inner a')];
  if ('IntersectionObserver' in window) {
    const observer = new IntersectionObserver(entries => {
      for (const entry of entries) if (entry.isIntersecting) {
        for (const link of chapters) {
          if (link.hash === `#${entry.target.id}`) link.setAttribute('aria-current', 'true'); else link.removeAttribute('aria-current');
        }
      }
    }, {rootMargin:'-20% 0px -60% 0px'});
    for (const link of chapters) { const section = document.querySelector(link.hash); if (section) observer.observe(section); }
  }
  translate(language);
})();
