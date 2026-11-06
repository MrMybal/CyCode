// Use CyDoc's public structured API importer; never index implementation files.
import { importApi } from 'cydoc/api-import';
import { readFile, writeFile, mkdir, readdir, unlink } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
const base = dirname(fileURLToPath(import.meta.url));
for (const locale of ['en', 'fr']) {
  const input = JSON.parse(await readFile(join(base, `.cache/api-${locale}.json`), 'utf8'));
  const pages = await importApi([{format: 'cydoc', path: `.cache/api-${locale}.json`, prefix: 'operation', locale}], {base, locales: [{code: locale}]});
  const destination = join(base, 'manual', locale, 'api-generated');
  await mkdir(destination, {recursive: true});
  const expected = new Set();
  for (const page of pages) {
    const symbol = input.symbols.find(item => item.name === page.symbol);
    const filename = `${page.id}.md`; expected.add(filename);
    const meta = [`id: ${page.id}`, `title: ${page.title}`, `description: ${symbol.summary}`, `section: ${locale === 'fr' ? 'API et extensions' : 'API and extensions'}`, `parent: api-${symbol.group}`, 'order: 100', 'kind: api', `${locale === 'fr' ? 'sourceRevision' : 'revision'}: ${symbol.revision}`];
    await writeFile(join(destination, filename), `---\n${meta.join('\n')}\n---\n\n${page.markdown}\n${symbol.schemas}\n`, 'utf8');
  }
  for (const name of await readdir(destination)) {
    if (!/^operation-[a-z0-9-]+\.md$/.test(name)) throw new Error(`Preserve unexpected generated API input: ${name}`);
    if (!expected.has(name)) await unlink(join(destination, name));
  }
  console.log(`${locale}: imported ${pages.length} public operation contracts`);
}
