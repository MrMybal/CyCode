"""Repeatable process-inclusive path-index and literal-search baseline."""
import argparse
import json
from pathlib import Path
import statistics
import subprocess
import tempfile
import time
ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--cli', type=Path, required=True)
parser.add_argument('--files', type=int, default=10000)
args = parser.parse_args()
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
    result = {'files':args.files, 'bytes':bytes_written, 'scope':'Includes native process startup and JSON serialization; synthetic local corpus.'}
    for method, parameters in [('workspace.files',{}),('search.text',{'query':'cycode_benchmark_needle'})]:
        timings = []
        for _ in range(5):
            before = time.perf_counter()
            process = subprocess.run([executable,str(workspace),method,json.dumps(parameters)], capture_output=True, check=True)
            timings.append((time.perf_counter()-before)*1000)
            payload = json.loads(process.stdout)
            if method == 'workspace.files' and len(payload['files']) != args.files:
                raise RuntimeError('Index did not cover the corpus')
            if method == 'search.text' and len(payload['matches']) != 1:
                raise RuntimeError('Search did not find the unique target')
        result[method] = {'samplesMs':timings, 'medianMs':statistics.median(timings), 'maxMs':max(timings)}
    report = ROOT/'data/reports/benchmark.json'
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result, indent=2))
