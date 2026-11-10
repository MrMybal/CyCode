"""Correlate explicit native/host/AV evidence locally. Never execute Core or upload.
Order is evidence, not attribution to a proprietary antivirus rule.
"""
import argparse
import calendar
from datetime import datetime
import json
from pathlib import Path
import re

from analyse_native_trace import analyse_trace, MAX_INPUT_BYTES, _unique_object
from antivirus_evidence import parse_kaspersky_event

MAX_PHASE_BYTES = 512 * 1024
LIFECYCLE_EVENTS = {'service.drain', 'engine.shutdown', 'recovery.scan', 'recovery.store.shutdown'}


def utc_nanoseconds(value):
    if not isinstance(value, str):
        raise ValueError('Expected a UTC timestamp')
    match = re.fullmatch(r'(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})(?:\.(\d{1,9}))?(?:Z|\+00:00)', value)
    if not match:
        raise ValueError('Expected an explicit UTC timestamp with up to nine fractional digits')
    try:
        base = datetime.strptime(match[1], '%Y-%m-%dT%H:%M:%S')
    except ValueError:
        raise ValueError('Invalid UTC date/time') from None
    return calendar.timegm(base.timetuple()) * 1_000_000_000 + int((match[2] or '').ljust(9, '0') or '0')


def read_bounded(path, limit):
    if path.is_symlink() or not path.is_file():
        raise ValueError('Expected an explicit regular evidence file')
    before = path.stat()
    with path.open('rb') as stream:
        data = stream.read(limit + 1)
    after = path.stat()
    if len(data) > limit or (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError('Evidence is too large or changed during inspection')
    return data.decode('utf-8-sig')


def correlate(native_text, phase_text, events, expected_sha256):
    if not isinstance(expected_sha256, str) or not re.fullmatch(r'[0-9a-f]{64}', expected_sha256):
        raise ValueError('Expected an exact lower-case sample SHA-256')
    native = analyse_trace(native_text)
    if not native['recordCount']:
        raise ValueError('No observed native records to correlate')
    if len(phase_text.encode('utf-8')) > MAX_PHASE_BYTES:
        raise ValueError('Host phase input exceeds 512 KiB')
    if not 1 <= len(events) <= 16:
        raise ValueError('Expected one to sixteen explicit AV events')
    av = []
    for event in events:
        if (event.get('provider') != 'avp' or str(event.get('eventId')) != '4662' or
                event.get('processSha256') != expected_sha256):
            raise ValueError('AV event does not identify the exact selected sample')
        record_id = str(event.get('recordId', ''))
        if not re.fullmatch(r'\d{1,20}', record_id):
            raise ValueError('Invalid AV record identity')
        stamp = event.get('utcTime')
        av.append({'recordId': record_id, 'utcTime': stamp, '_ns': utc_nanoseconds(stamp)})
    av.sort(key=lambda row: row['_ns'])
    if len({row['recordId'] for row in av}) != len(av):
        raise ValueError('Repeated AV event')
    first = av[0]['_ns']
    shutdowns = []
    lines = phase_text.splitlines()
    if len(lines) > 2048:
        raise ValueError('Too many host phase records')
    for line in lines:
        if len(line.encode('utf-8')) > 8192:
            raise ValueError('Host phase record exceeds 8 KiB')
        if not line.strip():
            continue
        try:
            row = json.loads(line, object_pairs_hook=_unique_object)
        except ValueError:
            raise ValueError('Malformed host phase record') from None
        if not isinstance(row, dict):
            raise ValueError('Expected host phase objects')
        if row.get('stage') != 'native-shutdown-start':
            continue
        stamp = row.get('utc')
        delta = first - utc_nanoseconds(stamp)
        shutdowns.append({'utcTime': stamp, 'eventMinusShutdownMilliseconds': delta / 1_000_000,
            'avPublicationOrder': 'after-request' if delta > 0 else 'before-request' if delta < 0 else 'same-time'})
    lifecycle = [row for row in native['closedSpans'] if row['event'] in LIFECYCLE_EVENTS]
    return {
        'format': 'cycode-antivirus-correlation/1', 'sampleSha256': expected_sha256,
        'avEvents': [{key: value for key, value in row.items() if key != '_ns'} for row in av],
        'nativeRecordCount': native['recordCount'], 'lastNativeRecord': native['lastRecord'],
        'eventMinusLastNativeMilliseconds': (first - native['lastRecord']['utcMicros'] * 1000) / 1_000_000,
        'hostShutdownRequests': shutdowns,
        'nativeLifecycleObserved': bool(lifecycle or any(row['event'] in LIFECYCLE_EVENTS for row in native['openSpans'])),
        'completedLifecycleSpans': lifecycle, 'openSpans': native['openSpans'],
        'failedSpans': native['failedSpans'], 'clockRegressionObserved': native['clockRegressionObserved'],
        'traceTruncated': native['truncatedByLimit'], 'partialLastLine': native['partialLastLine'],
        'unknownParents': native['unknownParents'], 'unmatchedEnds': native['unmatchedEnds'],
        'attribution': 'not-established',
        'limits': ['AV publication time is not the triggering activity time.',
            'Sample hash matching does not by itself bind an event to one PID or prove a false positive.',
            'Absent lifecycle spans can mean older instrumentation, truncation or interruption; they do not prove shutdown did not occur.',
            'UTC timestamps can be adjusted; clock regression disables reliable elapsed-time interpretation.'],
        'nativeExecutedByAnalysis': False, 'uploaded': False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--trace', type=Path, required=True, help='One explicitly selected native process log')
    parser.add_argument('--phases', type=Path, required=True, help='Captured host phases for that diagnostic')
    parser.add_argument('--kaspersky-event', type=Path, action='append', required=True)
    parser.add_argument('--sha256', required=True)
    parser.add_argument('--output', type=Path, required=True, help='New local report; never overwrite')
    args = parser.parse_args()
    try:
        result = correlate(read_bounded(args.trace, MAX_INPUT_BYTES), read_bounded(args.phases, MAX_PHASE_BYTES),
            [parse_kaspersky_event(path, 'cycode_cli.exe') for path in args.kaspersky_event], args.sha256)
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    if args.output.is_symlink():
        parser.error('Refuse a redirected output')
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print('Local evidence correlation saved; no Core execution or upload.')


if __name__ == '__main__':
    main()
