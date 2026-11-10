"""Data-only AV/native/host timestamp and attribution contracts."""
from pathlib import Path
import json
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from correlate_antivirus_trace import correlate, utc_nanoseconds, MAX_PHASE_BYTES

SHA = 'a' * 64
BASE = utc_nanoseconds('2026-11-09T20:00:00Z')


def trace(event='recovery.checkpoint', backwards=False):
    rows = [{'format': 'cycode-native-trace/1', 'seq': i, 'utcMicros': BASE // 1000 + offset,
        'span': 1, 'parent': 0, 'event': event, 'phase': phase, 'outcome': outcome, 'systemError': 0}
        for i, offset, phase, outcome in ((1, 100 if backwards else 0, 'begin', 'begin'),
            (2, 0 if backwards else 100, 'end', 'success'))]
    return ''.join(json.dumps(row) + '\n' for row in rows)


def av(stamp='2026-11-09T20:00:00.2000001Z', sha=SHA):
    return [{'provider': 'avp', 'eventId': '4662', 'recordId': '15', 'utcTime': stamp, 'processSha256': sha}]


class CorrelationTests(unittest.TestCase):
    def test_preserves_windows_event_fraction(self):
        self.assertEqual(utc_nanoseconds('2026-11-09T20:00:00.0000001Z') - BASE, 100)
        self.assertEqual(utc_nanoseconds('2026-11-09T20:00:00+00:00'), BASE)

    def test_event_after_shutdown_is_not_a_cause_verdict(self):
        phases = json.dumps({'stage': 'native-shutdown-start', 'utc': '2026-11-09T20:00:00.100000Z'})
        result = correlate(trace(), phases, av(), SHA)
        self.assertEqual(result['hostShutdownRequests'][0]['avPublicationOrder'], 'after-request')
        self.assertAlmostEqual(result['hostShutdownRequests'][0]['eventMinusShutdownMilliseconds'], 100.0001)
        self.assertEqual(result['attribution'], 'not-established')
        self.assertFalse(result['nativeLifecycleObserved'])

    def test_event_before_shutdown_remains_observation(self):
        phases = json.dumps({'stage': 'native-shutdown-start', 'utc': '2026-11-09T20:00:00.300000Z'})
        result = correlate(trace(), phases, av(), SHA)
        self.assertEqual(result['hostShutdownRequests'][0]['avPublicationOrder'], 'before-request')
        self.assertEqual(result['attribution'], 'not-established')

    def test_shutdown_span_can_be_observed_explicitly(self):
        result = correlate(trace('engine.shutdown'), '', av(), SHA)
        self.assertTrue(result['nativeLifecycleObserved'])
        self.assertEqual(len(result['completedLifecycleSpans']), 1)

    def test_rejects_wrong_sample_and_duplicate_events(self):
        with self.assertRaises(ValueError): correlate(trace(), '', av(sha='b' * 64), SHA)
        with self.assertRaises(ValueError): correlate(trace(), '', av() * 2, SHA)

    def test_does_not_copy_private_phase_fields(self):
        phases = json.dumps({'stage': 'native-shutdown-start', 'utc': '2026-11-09T20:00:00Z',
            'token': 'private-token', 'workspace': 'private-workspace'})
        result = correlate(trace(), phases, av(), SHA)
        self.assertNotIn('private-token', json.dumps(result))
        self.assertNotIn('private-workspace', json.dumps(result))

    def test_rejects_invalid_time_and_unbounded_input(self):
        for stamp in ('2026-11-40T20:00:00Z', '2026-11-09T20:00:00+01:00', 'private-time'):
            with self.assertRaises(ValueError): utc_nanoseconds(stamp)
        with self.assertRaises(ValueError): correlate(trace(), 'x' * (MAX_PHASE_BYTES + 1), av(), SHA)
        with self.assertRaises(ValueError): correlate('', '', av(), SHA)

    def test_clock_regression_is_explicit(self):
        result = correlate(trace(backwards=True), '', av(), SHA)
        self.assertTrue(result['clockRegressionObserved'])
        self.assertEqual(result['attribution'], 'not-established')

    def test_rejects_ambiguous_host_timestamp(self):
        phases = '{"stage":"native-shutdown-start","utc":"2026-11-09T20:00:00Z","utc":"2026-11-09T20:00:01Z"}'
        with self.assertRaises(ValueError): correlate(trace(), phases, av(), SHA)

    def test_partial_native_tail_is_not_reparsed_as_evidence(self):
        result = correlate(trace() + '{"format":', '', av(), SHA)
        self.assertTrue(result['partialLastLine'])
        self.assertEqual(result['nativeRecordCount'], 2)


if __name__ == '__main__':
    unittest.main()
