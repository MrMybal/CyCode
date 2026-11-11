"""Read a completed, explicitly captured native diagnostic log. Never launch the
Core or upload data. Report observations, not an antivirus root-cause verdict.
"""
import argparse
import json
from pathlib import Path

FORMAT = 'cycode-native-trace/1'
MAX_INPUT_BYTES = 128 * 1024
EVENTS = {
    'document.save', 'document.replace', 'recovery.retire', 'recovery.checkpoint',
    'recovery.erase', 'recovery.retain', 'recovery.index.commit',
    'recovery.payload.text.remove', 'recovery.payload.baseline.remove',
    'file.atomic.write', 'file.temporary.create', 'file.temporary.write',
    'file.temporary.flush', 'file.temporary.close', 'file.replace.existing',
    'file.publish.new', 'file.temporary.cleanup', 'file.temporary.retained', 'trace.limit',
    'service.drain', 'engine.shutdown', 'recovery.scan', 'recovery.store.shutdown',
    'recovery.cleanup.pause',
}
FIELDS = {'format', 'seq', 'utcMicros', 'span', 'parent', 'event', 'phase', 'outcome', 'systemError'}
OUTCOMES = {'returned', 'success', 'failure', 'missing', 'skipped', 'exception'}


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate JSON field')
        result[key] = value
    return result


def analyse_trace(text):
    if len(text.encode('utf-8')) > MAX_INPUT_BYTES:
        raise ValueError('Diagnostic input exceeds 128 KiB')
    opened, closed, seen = {}, [], set()
    ignored = count = previous = unknown_parents = unmatched_ends = 0
    previous_time = None
    clock_regression = False
    capped = partial = False
    last = None
    lines = text.splitlines(keepends=True)
    for line_number, line in enumerate(lines, 1):
        if not line.strip():
            continue
        if len(line.encode('utf-8')) > 4096:
            raise ValueError('Diagnostic line exceeds 4 KiB')
        try:
            row = json.loads(line, object_pairs_hook=_unique_object)
        except ValueError:
            if line_number == len(lines) and not line.endswith('\n'):
                partial = True
                break
            if FORMAT in line:
                raise ValueError('Malformed diagnostic JSON at line ' + str(line_number)) from None
            ignored += 1
            continue
        if not isinstance(row, dict) or row.get('format') != FORMAT:
            ignored += 1
            continue
        if set(row) != FIELDS:
            raise ValueError('Unexpected diagnostic fields at line ' + str(line_number))
        if any(type(row[key]) is not str for key in ('event', 'phase', 'outcome')):
            raise ValueError('Invalid diagnostic label at line ' + str(line_number))
        for key in ('seq', 'utcMicros', 'span', 'parent', 'systemError'):
            if type(row[key]) is not int or not 0 <= row[key] <= 2**64 - 1:
                raise ValueError('Invalid diagnostic number at line ' + str(line_number))
        if row['seq'] <= previous or not row['utcMicros']:
            raise ValueError('Non-monotonic sequence or missing diagnostic time')
        if row['event'] not in EVENTS or capped:
            raise ValueError('Unknown event or diagnostic record after trace limit')
        previous = row['seq']
        if previous_time is not None and row['utcMicros'] < previous_time:
            clock_regression = True
        previous_time = row['utcMicros']
        count += 1
        last = row.copy()
        if row['event'] == 'trace.limit':
            if (row['phase'], row['outcome'], row['span'], row['parent'], row['systemError']) != ('limit', 'truncated', 0, 0, 0):
                raise ValueError('Invalid trace-limit record')
            capped = True
            continue
        if not row['span'] or row['span'] == row['parent']:
            raise ValueError('Invalid diagnostic span identity')
        if row['phase'] == 'begin':
            if row['outcome'] != 'begin' or row['systemError'] or row['span'] in seen:
                raise ValueError('Invalid/reused diagnostic span start')
            seen.add(row['span'])
            if row['parent'] and row['parent'] not in opened:
                unknown_parents += 1
            opened[row['span']] = row
        elif row['phase'] == 'end':
            if row['outcome'] not in OUTCOMES:
                raise ValueError('Unknown diagnostic outcome')
            start = opened.pop(row['span'], None)
            if start is None:
                if row['span'] in seen:
                    raise ValueError('Repeated diagnostic span end')
                # A retained log tail may legitimately start in the middle of a span.
                unmatched_ends += 1
                seen.add(row['span'])
            elif (start['parent'], start['event']) != (row['parent'], row['event']):
                raise ValueError('Mismatched diagnostic span end')
            duration = row['utcMicros'] - start['utcMicros'] if start else None
            closed.append({'span': row['span'], 'parent': row['parent'], 'event': row['event'],
                'outcome': row['outcome'], 'systemError': row['systemError'],
                'startSeq': start['seq'] if start else None, 'endSeq': row['seq'],
                'durationMicros': duration if duration is not None and duration >= 0 else None})
        else:
            raise ValueError('Unknown diagnostic phase')
    return {
        'format': 'cycode-native-trace-analysis/1', 'recordCount': count,
        'state': 'observed' if count else 'no-trace-records', 'ignoredLogLines': ignored,
        'truncatedByLimit': capped, 'partialLastLine': partial,
        'clockRegressionObserved': clock_regression,
        'unknownParents': unknown_parents, 'unmatchedEnds': unmatched_ends,
        'lastRecord': last, 'closedSpans': closed,
        'openSpans': [{'span': row['span'], 'parent': row['parent'], 'event': row['event'],
                       'startSeq': row['seq']} for row in opened.values()],
        'failedSpans': [row for row in closed if row['outcome'] in ('failure', 'exception')],
        'nativeExecuted': False, 'uploaded': False,
        'causality': 'Event order and open spans are observations only. Antivirus event publication can lag the triggering activity; the final recorded call does not identify a proprietary rule.',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='New JSON report; never overwrite')
    args = parser.parse_args()
    if args.input.is_symlink() or not args.input.is_file():
        parser.error('Expected an explicit regular diagnostic log')
    before = args.input.stat()
    with args.input.open('rb') as stream:
        raw = stream.read(MAX_INPUT_BYTES + 1)
    after = args.input.stat()
    if len(raw) > MAX_INPUT_BYTES or (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        parser.error('Diagnostic log is too large or changed during inspection')
    try:
        result = analyse_trace(raw.decode('utf-8-sig', errors='replace'))
    except ValueError as error:
        parser.error(str(error))
    if args.output.is_symlink():
        parser.error('Refuse a redirected report')
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print('Local trace analysis written; no Core execution or upload.')


if __name__ == '__main__':
    main()
