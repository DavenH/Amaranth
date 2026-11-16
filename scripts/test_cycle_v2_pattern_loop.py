#!/usr/bin/env python3
"""Verify measure-aligned pattern loops and native Shift+A in an isolated app.

Run with cycle_automation.py exec --product cycle-v2 --clean --foreground --
python3 scripts/test_cycle_v2_pattern_loop.py after building CycleV2.
"""
import json
import os
import subprocess
import time

from cycle_automation_support.lifecycle import compiled_helper
from cycle_automation_support.profiles import ROOT
from cycle_automation_support.session import Session


def main():
    output = ROOT / '.context/pattern-loop'
    output.mkdir(exist_ok=True)
    with Session(os.environ['CYCLE_AGENT_SESSION'], timeout=90) as session:
        def command(**payload):
            return session.command(payload)['result'].get('data', {})

        def performance():
            return command(command='snapshotState')['performance']

        command(command='openGraph', path=str(ROOT / 'cycle-v2/content/presets/trombone.cyclegraph'))
        command(command='setPresetSequence', sequence={
            'durationSeconds': 30., 'tempoBpm': 120.,
            'notes': [{'pitch': pitch, 'velocity': 90, 'startSeconds': start, 'durationSeconds': .2}
                      for pitch, start in [(60, 0.), (64, .5), (67, 1.)]],
            'controls': [{'controller': 1, 'value': 20, 'timeSeconds': 0.},
                         {'controller': 1, 'value': 100, 'timeSeconds': 20.}]})
        command(command='pointer', targetId='PerformanceKeyboard.PlaySequence', event='click')
        time.sleep(1.4)
        resting = performance()
        assert resting['previewPlaying'] and .6 < resting['previewProgress'] < .9, resting
        deadline = time.monotonic() + 1.5
        previous = resting['previewProgress']
        while True:
            observed = performance()
            assert observed['previewPlaying'], observed
            if observed['previewProgress'] < previous:
                break
            assert time.monotonic() < deadline, observed
            previous = observed['previewProgress']
            time.sleep(.03)
        command(command='pointer', targetId='PerformanceKeyboard.PlaySequence', event='click')
        assert not performance()['previewPlaying']
        command(command='pointer', targetId='PerformanceKeyboard.EditSequence', event='click')
        subprocess.run([str(compiled_helper('launch_cycle_app.swift')), '--focus', os.environ['CYCLE_APP_PID']], check=True)
        bounds = command(command='inspectTargets', area='presetMidiEditor')['targets'][0]['screenBounds']
        # Activate the editor through its note grid, then exercise the real shortcut.
        point = f"{round(bounds['x'] + 900)},{round(bounds['y'] + 250)}"
        subprocess.run(['/opt/homebrew/bin/cliclick', 'c:' + point], check=True)
        subprocess.run(['osascript', '-e',
                        'tell application "System Events" to keystroke "a" using shift down'], check=True)
        time.sleep(.15)
        region = ','.join(str(round(bounds[k])) for k in ('x', 'y', 'width', 'height'))
        subprocess.run(['screencapture', '-x', '-R' + region, str(output / 'selected-notes.png')], check=True)
        command(command='key', area='presetMidiEditor', key='delete')
        assert performance()['previewSequenceNoteCount'] == 0
        result = {'restProgress': resting['previewProgress'],
                  'loopProgress': observed['previewProgress'], 'shiftASelectsAll': True,
                  'explicitStop': True}
        (output / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result))


if __name__ == '__main__':
    main()
