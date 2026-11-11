#!/usr/bin/env python3
"""Verify the performance waveform's voice overlays, cache, commit and resize behavior."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from cycle_automation_support.session import Session

ROOT = Path(__file__).resolve().parent.parent


def check(output):
    output.mkdir(parents=True, exist_ok=True)
    with Session(os.environ['CYCLE_AGENT_SESSION'], timeout=120) as session:
        def send(**payload):
            response = session.command(payload)
            assert response.get('ok') and response['result'].get('ok'), response
            return response['result'].get('data', {})

        def state():
            return send(command='snapshotState')

        def wait(predicate, timeout=15):
            deadline = time.monotonic() + timeout
            while True:
                current = state()
                if predicate(current):
                    return current
                assert time.monotonic() < deadline, current['performance']['waveform']
                time.sleep(.05)

        send(command='openGraph', path=str(ROOT / 'cycle-v2/resources/default.cyclegraph'))
        first = wait(lambda s: s['performance']['waveform']['ready'])
        assert first['performance']['highestNote'] - first['performance']['baseNote'] == 36
        last_note = first['performance']['highestNote']
        send(command='pointer', targetId=f'PerformanceKeyboard.Note{last_note}', event='down', targetY=.85)
        assert state()['performance']['heldNote'] == last_note
        send(command='pointer', targetId=f'PerformanceKeyboard.Note{last_note}', event='up', targetY=.85)
        wait(lambda s: s['performance']['waveform']['cursorCount'] == 0
                and s['performance']['waveform']['ready'])
        time.sleep(.3)
        first = state()
        before = first['performance']['waveform']
        assert before['visible'] and before['colourStyle'] == 'blues', before
        assert before['background'] == 'toolbar'
        assert before['pixelWidth'] > 0 and before['heatColumns'] % before['pixelWidth'] == 0, before
        assert first['performance']['whiteKeyHeight'] == 50
        assert first['performance']['screenHeight'] == 56
        send(command='sendMidi', type='noteOn', note=60, velocity=.7)
        time.sleep(.12)
        send(command='sendMidi', type='noteOn', note=64, velocity=.7)
        playing = wait(lambda s: s['performance']['waveform']['cursorCount'] == 2)
        positions = playing['performance']['waveform']['cursorPositions']
        time.sleep(.08)
        playing = state()
        during = playing['performance']['waveform']
        assert during['cursorPositions'][0] > positions[0]
        assert during['cursorPositions'][0] > during['cursorPositions'][1]
        for key in ('captureRequests', 'capturePublications', 'sourceBuilds', 'heatBuilds', 'geometryBuilds'):
            assert during[key] == before[key], (key, before, during)
        assert not playing['documentDirty']
        send(command='sendMidi', type='allNotesOff')
        wait(lambda s: s['performance']['waveform']['cursorCount'] == 0)

        send(command='setNodeParameter', nodeId='voice', parameterId='octave', label='Octave', value='1')
        edited = wait(lambda s: s['performance']['waveform']['capturePublications']
                > before['capturePublications'] and s['performance']['waveform']['ready'])
        assert edited['documentDirty']
        send(command='undo')
        restored = wait(lambda s: s['performance']['waveform']['capturePublications']
                > edited['performance']['waveform']['capturePublications'] and s['performance']['waveform']['ready'])
        assert not restored['documentDirty']
        requests = restored['performance']['waveform']['captureRequests']
        send(command='resizeWindow', width=800, height=700)
        small = wait(lambda s: not s['performance']['waveform']['visible'])
        assert small['performance']['whiteKeyHeight'] == 50
        send(command='resizeWindow', width=1728, height=994)
        large = wait(lambda s: s['performance']['waveform']['visible'] and s['performance']['waveform']['ready'])
        assert large['performance']['waveform']['captureRequests'] == requests
        assert large['performance']['waveform']['sourceBuilds'] == restored['performance']['waveform']['sourceBuilds']

        from test_cycle_v2_native_edit_smoke import NativeEditSmoke
        from cycle_automation_support.registry import registry
        native = NativeEditSmoke()
        native.app_pid = int(os.environ['CYCLE_APP_PID'])
        with registry() as records:
            native.record = next((record for record in records['instances'].values()
                    if record['pid'] == native.app_pid
                    and record['socket'] == os.environ['CYCLE_AGENT_SESSION']), None)
        native.focus_app()
        targets = send(command='inspectPointerTargets')['targets']
        origin = next(t for t in targets if t['id'] == 'comparisonWorkspace')['screenBounds']
        last = next(t for t in targets if t['id'] == f'PerformanceKeyboard.Note{last_note}')['bounds']
        last_key = (round(origin['x'] + last['x'] + last['width'] / 2),
                round(origin['y'] + last['y'] + last['height'] * .85))
        try:
            native.begin_drag(last_key)
            wait(lambda s: s['performance']['heldNote'] == last_note)
        finally:
            native.release_drag(last_key)
        wait(lambda s: s['performance']['heldNote'] == -1
                and s['performance']['waveform']['cursorCount'] == 0)
        control = next(t for t in targets if t['id'] == 'PerformanceKeyboard.ModWheel')['bounds']
        source = (round(origin['x'] + control['x'] + control['width'] / 2),
                round(origin['y'] + control['y'] + control['height'] / 2))
        target = (source[0], source[1] - 80)
        initial = state()['performance']['waveform']['captureRequests']
        try:
            native.drag_and_hold(source, target)
            time.sleep(.2)
            assert state()['performance']['waveform']['captureRequests'] == initial
        finally:
            native.release_drag(target)
        wait(lambda s: s['performance']['waveform']['captureRequests'] > initial
                and s['performance']['waveform']['ready'])
        os.environ['CYCLE_V2_NATIVE_CAPTURE_DIR'] = str(output)
        send(command='sendMidi', type='noteOn', note=60, velocity=.7)
        time.sleep(.15)
        send(command='sendMidi', type='noteOn', note=64, velocity=.7)
        wait(lambda s: s['performance']['waveform']['cursorCount'] == 2)
        targets = send(command='inspectPointerTargets')['targets']
        bounds = next(t for t in targets if t['id'] == 'comparisonWorkspace')['screenBounds']
        native.capture('performance-waveform-playing', bounds)
        send(command='sendMidi', type='allNotesOff')
        wait(lambda s: s['performance']['waveform']['cursorCount'] == 0)
        native.capture('performance-waveform-idle', bounds)
        final = state()
        evidence = {'ok': True, 'before': before, 'playing': during,
                'edited': edited['performance']['waveform'],
                'restored': restored['performance']['waveform'], 'final': final['performance']['waveform']}
        (output / 'results.json').write_text(json.dumps(evidence, indent=2) + '\n')
        print(json.dumps({'ok': True, 'results': str(output / 'results.json')}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT / '.context/performance-waveform')
    parser.add_argument('--no-build', action='store_true')
    parser.add_argument('--in-session', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.in_session:
        check(args.output_dir.resolve())
        return 0
    coordinator = [sys.executable, str(ROOT / 'scripts/cycle_automation.py'),
            'exec', '--product', 'cycle-v2', '--clean', '--foreground']
    if args.no_build:
        coordinator.append('--no-build')
    return subprocess.run(coordinator + ['--', sys.executable, str(Path(__file__).resolve()),
            '--in-session', '--output-dir', str(args.output_dir.resolve())], cwd=ROOT).returncode


if __name__ == '__main__':
    sys.exit(main())
