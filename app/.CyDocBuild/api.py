"""Prepare bilingual public contracts for CyDoc's official neutral API importer."""
import hashlib
import json
import re
from pathlib import Path

WORK = Path(__file__).resolve().parent
GROUPS = {
    'unreal': ('Unreal Engine', 'Unreal Engine'), 'vcs': ('Version control', 'Contrôle de versions'),
    'build': ('CMake builds', 'Compilation CMake'), 'language': ('Language services', 'Services de langage'),
    'packages': ('Extension packages', 'Paquets d’extension'), 'events': ('Events', 'Événements'),
    'tests': ('Project tests', 'Tests du projet'), 'templates': ('Project templates', 'Modèles de projets'),
    'resources': ('Workspace resources', 'Ressources du projet'), 'project': ('Project tasks', 'Tâches du projet'),
    'msbuild': ('Solutions and MSBuild', 'Solutions et MSBuild'), 'documents': ('Documents', 'Documents'),
    'debug': ('Debugging', 'Débogage'), 'terminal': ('Terminals', 'Terminaux'), 'sources': ('Source access', 'Accès aux sources'),
    'changes': ('Change transactions', 'Transactions de modification'), 'workspace': ('Workspace', 'Espace de travail'),
    'search': ('Text search', 'Recherche de texte'), 'recovery': ('Document recovery', 'Récupération des documents'),
    'toolchains': ('Toolchains', 'Chaînes d’outils'), 'plugins': ('Native extensions', 'Extensions natives'),
    'snippets': ('Insertion snippets', 'Extraits personnalisés'),
    'service': ('Core service', 'Service Core'), 'analysis': ('Analysis preparation', 'Préparation de l’analyse')
}
FIELD = {
    'path': ('Document or resource path in the permitted workspace.', 'Chemin du document ou de la ressource dans l’espace de travail autorisé.'),
    'expectedRevision': ('Current revision returned by a preceding read; a stale revision rejects the change.', 'Révision courante retournée par une lecture préalable ; une révision périmée fait rejeter la modification.'),
    'selection': ('Selected UTF-16 start/end range. For project snippets, it must be ordered, include the caret at an endpoint and contain at most 8 KiB of source; TM_SELECTED_TEXT owns the captured text.', 'Plage sélectionnée start/end en UTF-16. Pour les extraits du projet, elle doit être ordonnée, avoir le curseur à une extrémité et contenir au plus 8 Kio de source ; TM_SELECTED_TEXT reçoit le texte capturé.'),
    'revision': ('Revision issued by the native service; preserve its exact value.', 'Révision émise par le service natif ; conservez sa valeur exacte.'),
    'previewId': ('Identifier issued by the matching preview operation.', 'Identifiant émis par l’opération de prévisualisation correspondante.'),
    'id': ('Service-issued identifier for the selected object.', 'Identifiant émis par le service pour l’objet sélectionné.'),
    'content': ('UTF-8 document content.', 'Contenu du document en UTF-8.'),
    'text': ('Text supplied to this operation.', 'Texte fourni à cette opération.'),
    'offset': ('Start offset within the bounded result.', 'Position de départ dans le résultat borné.'),
    'limit': ('Maximum number of items or characters requested.', 'Nombre maximal d’éléments ou de caractères demandé.'),
    'line': ('Line position in the coordinate system exposed by this service.', 'Numéro de ligne dans le système de coordonnées exposé par ce service.'),
    'column': ('Column position in the coordinate system exposed by this service.', 'Numéro de colonne dans le système de coordonnées exposé par ce service.'),
    'repository': ('Selected repository identity or path.', 'Identité ou chemin du dépôt sélectionné.'),
    'timeoutSeconds': ('Process deadline in seconds.', 'Délai maximal du processus en secondes.'),
    'engineRoot': ('Root of the Unreal Engine installation to use.', 'Racine de l’installation Unreal Engine à utiliser.'),
    'target': ('Target selected from the discovered project or service results.', 'Cible sélectionnée parmi les résultats du projet ou du service.'),
    'configuration': ('Configuration supported by the selected project.', 'Configuration prise en charge par le projet sélectionné.'),
    'platform': ('Platform supported by the selected toolchain.', 'Plateforme prise en charge par la chaîne d’outils choisie.'),
    'cursor': ('Continuation cursor returned by the preceding response.', 'Curseur de continuation retourné par la réponse précédente.')
}


def technical(schema):
    if isinstance(schema, list): return [technical(item) for item in schema]
    if isinstance(schema, dict):
        return {key: technical(value) for key, value in schema.items()
                if not (key in {'description', 'title', '$comment'} and isinstance(value, str))}
    return schema


def generate():
    descriptor = json.loads((WORK.parent / 'CyTool.json').read_text('utf-8'))
    operations = descriptor['operations']
    translated = json.loads((WORK / 'api-fr.json').read_text('utf-8'))
    assert set(translated) == {op['id'] for op in operations}, 'Review all operation translations'
    cache = WORK / '.cache'; cache.mkdir(exist_ok=True)
    for locale in ('en', 'fr'):
        fr = locale == 'fr'
        t = lambda en, french: french if fr else en
        symbols = []
        for op in operations:
            identity = op['id']; group = identity.split('_')[0]
            summary = translated[identity] if fr else re.split(r'(?<=\.)\s', op['description'], maxsplit=1)[0]
            # Descriptions are concise prose; the complete executable constraints
            # remain in the two public JSON Schemas below, never in source listings.
            summary = re.sub(r'\s+', ' ', summary).strip().replace(': ', ' — ')
            inputs, outputs = technical(op['inputSchema']), technical(op['outputSchema'])
            parameters = []
            for key, schema in inputs.get('properties', {}).items():
                constraint = {k: v for k, v in schema.items() if k not in {'type', 'properties', 'items'}}
                prose = FIELD.get(key, ('Value accepted by the input schema below.', 'Valeur acceptée par le schéma d’entrée ci-dessous.'))[int(fr)]
                if constraint: prose += ' ' + t('Constraints: ', 'Contraintes : ') + '`' + json.dumps(constraint, ensure_ascii=False, separators=(',', ':')) + '`'
                parameters.append({'name': key, 'type': str(schema.get('type', 'JSON')), 'optional': key not in inputs.get('required', []), 'default': schema.get('default', ''), 'description': prose})
            stem = identity.rsplit('_', 1)[0]
            relations = [{'key': other['id'], 'name': other['id']} for other in operations if other['id'] != identity and other['id'].rsplit('_', 1)[0] == stem][:8]
            signature = identity + '({' + ', '.join(p['name'] + ('?' if p['optional'] else '') + ': ' + p['type'] for p in parameters) + '}) -> JSON'
            contract = t('Use the official MCP tool `op_' + identity + '` through a connected CyToolsCore client. This is an operation contract, not a C++ function. Read the [shared request and transaction rules](doc:api-links#contract) before making changes.', 'Utilisez l’outil MCP officiel `op_' + identity + '` depuis un client CyToolsCore connecté. Il s’agit d’un contrat d’opération, pas d’une fonction C++. Consultez les [règles communes des requêtes et transactions](doc:api-links#contract) avant toute modification.')
            # Large schemas remain complete and readable, but do not need tens
            # of thousands of Prism spans in every locale's startup payload.
            def block(schema):
                text = json.dumps(schema, ensure_ascii=False, indent=2)
                return '\n\n```' + ('json' if len(text) < 2500 else 'text') + '\n' + text + '\n```\n'
            schemas = '\n## ' + t('Input schema', 'Schéma d’entrée') + ' {#input-schema}' + block(inputs) + '\n## ' + t('Result schema', 'Schéma du résultat') + ' {#result-schema}' + block(outputs)
            symbols.append({'name': identity, 'key': identity, 'group': group, 'kind': 'operation', 'language': 'text', 'summary': summary, 'description': summary + '\n\n' + contract, 'signature': signature, 'parameters': parameters, 'returnType': 'JSON', 'returns': t('Result described below. For an asynchronous call, poll the returned job with `cy_get_job`; completion of the request does not by itself prove that an external process succeeded.', 'Résultat décrit ci-dessous. Pour un appel asynchrone, suivez la tâche retournée avec `cy_get_job` ; la fin de la requête ne prouve pas à elle seule la réussite d’un processus externe.'), 'references': relations, 'schemas': schemas, 'revision': hashlib.sha256(json.dumps(op, sort_keys=True).encode()).hexdigest()[:12]})
        (cache / ('api-' + locale + '.json')).write_text(json.dumps({'symbols': symbols}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        for index, (group, names) in enumerate(GROUPS.items()):
            selected = [symbol for symbol in symbols if symbol['group'] == group]
            title = names[int(fr)]
            description = t('Public operations for ', 'Opérations publiques pour ') + title + '.'
            body = description + '\n\n' + t('These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.', 'Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.')
            body += '\n\n## ' + t('Operations', 'Opérations') + ' {#operations}\n\n'
            body += '\n'.join('- [`' + s['name'] + '`](doc:operation-' + s['name'].replace('_', '-') + '-' + s['name'].replace('_', '-') + ') — ' + s['summary'] for s in selected)
            front = f'id: api-{group}\ntitle: {title}\ndescription: {description}\nsection: ' + t('API and extensions', 'API et extensions') + f'\nparent: api-links\norder: {100 + index}\n' + ('sourceRevision' if fr else 'revision') + ': 2'
            (WORK / 'manual' / locale / ('api-' + group + '.md')).write_text('---\n' + front + '\n---\n\n' + body + '\n', encoding='utf-8')


if __name__ == '__main__': generate()
