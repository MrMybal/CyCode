"""Trace-reader data contracts only. No native Core, compiler or SDK process."""
import json
from pathlib import Path
import re
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from analyse_native_trace import analyse_trace, EVENTS, MAX_INPUT_BYTES


def record(seq, span, event, phase='begin', parent=0, outcome=None, error=0):
    return {'format': 'cycode-native-trace/1', 'seq': seq, 'utcMicros': 1000000 + seq,
        'span': span, 'parent': parent, 'event': event, 'phase': phase,
        'outcome': outcome or ('begin' if phase == 'begin' else 'success'), 'systemError': error}


def encoded(*rows):
    return ''.join(json.dumps(row) + '\n' for row in rows)


class TraceAnalysisTests(unittest.TestCase):
    def test_nested_complete(self):
        result = analyse_trace(encoded(record(1, 1, 'document.save'),
            record(2, 2, 'recovery.retire', parent=1),
            record(3, 2, 'recovery.retire', 'end', parent=1), record(4, 1, 'document.save', 'end')))
        self.assertEqual(len(result['closedSpans']), 2)
        self.assertFalse(result['openSpans'])
        self.assertEqual(result['closedSpans'][0]['durationMicros'], 1)

    def test_interrupted_pause_does_not_invent_cleanup(self):
        result = analyse_trace(encoded(record(1, 1, 'recovery.checkpoint'),
            record(2, 2, 'recovery.index.commit', parent=1),
            record(3, 2, 'recovery.index.commit', 'end', parent=1),
            record(4, 3, 'recovery.cleanup.pause', parent=1)))
        self.assertEqual([s['event'] for s in result['openSpans']],
                         ['recovery.checkpoint', 'recovery.cleanup.pause'])
        self.assertFalse(any(s['event'].endswith('.remove') for s in result['closedSpans']))
        self.assertEqual(result['lastRecord']['event'], 'recovery.cleanup.pause')

    def test_completed_pause_and_cleanup_are_distinct(self):
        result = analyse_trace(encoded(record(1, 1, 'recovery.checkpoint'),
            record(2, 2, 'recovery.cleanup.pause', parent=1),
            record(3, 2, 'recovery.cleanup.pause', 'end', parent=1),
            record(4, 3, 'recovery.payload.text.remove', parent=1),
            record(5, 3, 'recovery.payload.text.remove', 'end', parent=1),
            record(6, 1, 'recovery.checkpoint', 'end')))
        self.assertFalse(result['openSpans'])
        pause, removal, checkpoint = result['closedSpans']
        self.assertLess(pause['endSeq'], removal['startSeq'])
        self.assertEqual(checkpoint['event'], 'recovery.checkpoint')

    def test_dirty_store_shutdown_is_observed_as_skipped(self):
        result = analyse_trace(encoded(record(1, 1, 'recovery.store.shutdown'),
            record(2, 1, 'recovery.store.shutdown', 'end', outcome='skipped')))
        self.assertEqual(result['closedSpans'][0]['outcome'], 'skipped')
        self.assertFalse(result['failedSpans'])

    def test_shutdown_body_and_store_are_separate_lifetimes(self):
        result = analyse_trace(encoded(record(1, 1, 'engine.shutdown'),
            record(2, 2, 'recovery.scan', parent=1), record(3, 2, 'recovery.scan', 'end', parent=1),
            record(4, 1, 'engine.shutdown', 'end'), record(5, 3, 'recovery.store.shutdown'),
            record(6, 3, 'recovery.store.shutdown', 'end', outcome='skipped')))
        self.assertFalse(result['openSpans'])
        self.assertEqual(len(result['closedSpans']), 3)

    def test_interrupted_preserves_open_spans(self):
        result = analyse_trace(encoded(record(1, 1, 'document.save'), record(2, 2, 'file.replace.existing', parent=1)))
        self.assertEqual([row['event'] for row in result['openSpans']], ['document.save', 'file.replace.existing'])

    def test_independent_threads_interleave(self):
        result = analyse_trace(encoded(record(1, 2, 'recovery.checkpoint'), record(2, 1, 'document.save'),
            record(3, 2, 'recovery.checkpoint', 'end'), record(4, 1, 'document.save', 'end')))
        self.assertEqual(result['recordCount'], 4)

    def test_failure_retains_numeric_code(self):
        result = analyse_trace(encoded(record(1, 1, 'file.replace.existing'),
            record(2, 1, 'file.replace.existing', 'end', outcome='failure', error=1176)))
        self.assertEqual(result['failedSpans'][0]['systemError'], 1176)

    def test_tail_does_not_invent_beginning(self):
        result = analyse_trace(encoded(record(8, 4, 'file.temporary.write', 'end', parent=3)))
        self.assertEqual(result['unmatchedEnds'], 1)
        self.assertIsNone(result['closedSpans'][0]['startSeq'])

    def test_limit_and_incomplete_last_line(self):
        limit = record(2, 0, 'trace.limit', 'limit', outcome='truncated')
        result = analyse_trace(encoded(record(1, 1, 'document.save'), limit) + '{"format":')
        self.assertTrue(result['truncatedByLimit'])
        self.assertTrue(result['partialLastLine'])
        self.assertEqual(len(result['openSpans']), 1)

    def test_rejects_private_extra_fields(self):
        row = record(1, 1, 'document.save'); row['token'] = 'private-value'
        with self.assertRaises(ValueError): analyse_trace(encoded(row))

    def test_ignores_other_logs_without_copying(self):
        result = analyse_trace('{"error":"private-file-name"}\n' + encoded(record(1, 1, 'document.save')))
        self.assertEqual(result['ignoredLogLines'], 1)
        self.assertNotIn('private-file-name', json.dumps(result))

    def test_rejects_duplicate_fields(self):
        text = json.dumps(record(1, 1, 'document.save')).replace('"span": 1', '"span": 1, "span": 2') + '\n'
        with self.assertRaises(ValueError): analyse_trace(text)

    def test_rejects_unknown_events(self):
        with self.assertRaises(ValueError): analyse_trace(encoded(record(1, 1, 'private.path')))

    def test_rejects_nonstring_labels(self):
        for field in ('event', 'phase', 'outcome'):
            row = record(1, 1, 'document.save'); row[field] = ['unexpected']
            with self.assertRaises(ValueError): analyse_trace(encoded(row))

    def test_rejects_duplicate_end(self):
        with self.assertRaises(ValueError):
            analyse_trace(encoded(record(1, 1, 'document.save'), record(2, 1, 'document.save', 'end'),
                record(3, 1, 'document.save', 'end')))

    def test_rejects_nonmonotonic_sequence(self):
        with self.assertRaises(ValueError): analyse_trace(encoded(record(2, 1, 'document.save'), record(1, 1, 'document.save', 'end')))

    def test_rejects_reused_identity_and_mismatch(self):
        with self.assertRaises(ValueError): analyse_trace(encoded(record(1, 1, 'document.save'), record(2, 1, 'document.save')))
        with self.assertRaises(ValueError): analyse_trace(encoded(record(1, 1, 'document.save'), record(2, 1, 'recovery.retire', 'end')))

    def test_input_bound(self):
        with self.assertRaises(ValueError): analyse_trace('x' * (MAX_INPUT_BYTES + 1))

    def test_native_event_labels_match_reader(self):
        source = Path(__file__).resolve().parents[2] / 'common/src/NativeTrace.cpp'
        enum_part = source.read_text('utf-8').split('const char *label(NativeTraceResult', 1)[0]
        names = set(re.findall(r'return "([a-z.]+)";', enum_part)) - {'trace.unknown'}
        self.assertEqual(names | {'trace.limit'}, EVENTS)


if __name__ == '__main__':
    unittest.main()
