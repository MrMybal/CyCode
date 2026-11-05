"""Repeatable process-inclusive path-index and literal-search baseline."""
import argparse
import json
from pathlib import Path
import statistics
import subprocess
import tempfile
import time
import sys
ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--cli', type=Path, required=True)
parser.add_argument('--files', type=int, default=10000)
parser.add_argument('--live', action='store_true', help='Keep one native Engine alive to measure its in-memory index cache (requires private Python/SDK).')
args = parser.parse_args()
if not 1 <= args.files <= 200000:
    parser.error('--files must be in 1..200000')
executable = str(args.cli.resolve())
with tempfile.TemporaryDirectory(prefix='cycode-benchmark-') as temporary:
    workspace = Path(temporary)
    bytes_written = 0
    for index in range(args.files):
        directory = workspace / f'source_{index // 100:04d}'
        directory.mkdir(exist_ok=True)
        content = '// deterministic corpus\n' + 'int example() { return 42; }\n' * 35
        if index == args.files // 2:
            content += '// cycode_benchmark_needle\n'
        (directory / f'file_{index:05d}.cpp').write_text(content, encoding='utf-8')
        bytes_written += len(content.encode('utf-8'))
    process = None
    try:
        if args.live:
            sys.path.insert(0, str(ROOT / 'app'))
            from tool import NativeClient
            descriptor = workspace.parent / (workspace.name + '-bridge.json')
            process = subprocess.Popen([executable, str(workspace), '--serve', str(descriptor)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            deadline = time.monotonic() + 15
            while not descriptor.exists():
                if process.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError('Benchmark native Engine did not start')
                time.sleep(.02)
            call = NativeClient(descriptor).call
        else:
            def call(method, parameters):
                completed = subprocess.run([executable, str(workspace), method, json.dumps(parameters)], capture_output=True, check=True)
                return json.loads(completed.stdout)
        result = {'files':args.files, 'bytes':bytes_written, 'scope': 'One live native Engine; IPC and JSON included; first sample cold, subsequent path reads cached.' if args.live else 'Includes native process startup and JSON serialization; synthetic local corpus.'}
        for method, parameters in [('workspace.files',{}),('search.text',{'query':'cycode_benchmark_needle'})]:
            timings = []
            for sample in range(5):
                before = time.perf_counter()
                payload = call(method, parameters)
                if method == 'workspace.files':
                    count = len(payload['files'])
                    generation = payload['indexRevision']
                    while payload['nextOffset'] is not None:
                        payload = call(method, {'offset':payload['nextOffset']})
                        if args.live and payload['indexRevision'] != generation:
                            raise RuntimeError('Corpus changed during benchmark pagination')
                        count += len(payload['files'])
                    if count != args.files or payload['truncated']:
                        raise RuntimeError('Index did not cover the corpus')
                elif len(payload['matches']) != 1:
                    raise RuntimeError('Search did not find the unique target')
                timings.append((time.perf_counter()-before)*1000)
            result[method] = {'samplesMs':timings, 'medianMs':statistics.median(timings), 'maxMs':max(timings)}
            if args.live and method == 'workspace.files':
                result[method]['coldMs'] = timings[0]
                result[method]['cachedMedianMs'] = statistics.median(timings[1:])
    finally:
        if process is not None:
            process.communicate(b'\n' if process.poll() is None else None, timeout=15)
    report = ROOT/'data/reports/benchmark.json'
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result, indent=2))
