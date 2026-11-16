#!/usr/bin/env python3
"""Run under cycle_automation.py exec --product cycle-v2 --clean --foreground."""
import json
import os
from pathlib import Path
import subprocess
import time

from cycle_automation_support.lifecycle import compiled_helper
from cycle_automation_support.profiles import ROOT
from cycle_automation_support.session import Session


def main():
    output = ROOT / '.context/curve-transport'
    output.mkdir(exist_ok=True)
    with Session(os.environ['CYCLE_AGENT_SESSION'], timeout=60) as session:
        def command(**payload):
            return session.command(payload)['result'].get('data', {})

        def state():
            return command(command='snapshotState')

        def capture(name):
            command(command='waitForIdle', idleDelayMs=250)
            bounds = command(command='inspectTargets', area='workspace')['targets'][0]['screenBounds']
            region = ','.join(str(round(bounds[k])) for k in ('x', 'y', 'width', 'height'))
            subprocess.run(['screencapture', '-x', '-R' + region, str(output / (name + '.png'))], check=True)

        def click(identifier):
            targets = command(command='inspectPointerTargets')['targets']
            bounds = next(t['bounds'] for t in targets if t['id'] == identifier)
            origin = command(command='inspectTargets', area='workspace')['targets'][0]['screenBounds']
            x = round(origin['x'] + bounds['x'] + bounds['width'] / 2)
            y = round(origin['y'] + bounds['y'] + bounds['height'] / 2)
            subprocess.run(['/opt/homebrew/bin/cliclick', f'c:{x},{y}'], check=True)
            time.sleep(.12)

        def check_count(expected):
            assert state()['guideCount'] == expected
            script = f'''tell application "System Events"
set p to first application process whose unix id is {os.environ['CYCLE_APP_PID']}
set xs to entire contents of first window of p
repeat with x in xs
try
if role of x is "AXButton" and name of x starts with "Curves (" then return name of x
end try
end repeat
end tell'''
            label = subprocess.check_output(['osascript', '-e', script], text=True).strip()
            assert label == f'Curves ({expected} in this preset)', label

        command(command='openGraph', path=str(ROOT / 'cycle-v2/content/presets/sitar-pad.cyclegraph'))
        command(command='waitForIdle', idleDelayMs=350)
        subprocess.run([str(compiled_helper('launch_cycle_app.swift')), '--focus', os.environ['CYCLE_APP_PID']], check=True)
        time.sleep(.2)
        def check_list_alignment():
            click('workspace.sidebar.presets')
            preset_targets = command(command='inspectPointerTargets')['targets']
            preset_slot = next(t['bounds'] for t in preset_targets
                               if t['id'].startswith('workspace.sidebar.preset.'))
            click('workspace.sidebar.curves')
            curve_targets = command(command='inspectPointerTargets')['targets']
            first_curve = next(t['bounds'] for t in curve_targets if t['id'] == 'guide:guide1')
            curve_tab = next(t['bounds'] for t in curve_targets if t['id'] == 'workspace.sidebar.curves')
            assert abs(first_curve['x'] - (preset_slot['x'] + 2)) < .001
            shelf = next(t['bounds'] for t in curve_targets if t['id'] == 'guideShelf')
            assert abs(first_curve['x'] + first_curve['width']
                       - (shelf['x'] + shelf['width'] - 8)) < .001
            assert abs(first_curve['y'] - (curve_tab['y'] + curve_tab['height'] + 8)) < .001

        baseline_count = len(state()['guides'])
        check_list_alignment()
        check_count(baseline_count)
        capture('curves-two')
        created = command(command='createGuideCurve', expectedDocumentRevision=state()['documentRevision'])['guideId']
        check_count(baseline_count + 1)
        capture('curves-three')
        command(command='undo')
        check_count(baseline_count)
        command(command='redo')
        check_count(baseline_count + 1)
        command(command='deleteGuideCurve', guideId=created)
        check_count(baseline_count)
        command(command='pointer', targetId='guide:guide1', event='doubleClick')
        assert state()['expandedGuideId'] == 'guide1'
        capture('expanded-guide-handles')
        command(command='key', area='canvas', key='escape')
        assert state()['expandedGuideId'] == ''
        before = state()['performance']['baseNote']
        click('PerformanceKeyboard.OctaveUp')
        assert state()['performance']['baseNote'] == before + 12
        capture('octave-hover')
        click('PerformanceKeyboard.OctaveDown')
        assert state()['performance']['baseNote'] == before
        command(command='openGraph', path=str(ROOT / 'cycle-v2/resources/fft-sawtooth.cyclegraph'))
        check_count(0)
        capture('curves-zero')
        command(command='openGraph', path=str(ROOT / 'cycle-v2/content/presets/sitar-pad.cyclegraph'))
        check_count(baseline_count)
        command(command='setPresetSequence', sequence={
            'durationSeconds': 10., 'tempoBpm': 120.,
            'notes': [{'pitch': 60, 'velocity': 100, 'startSeconds': 0., 'durationSeconds': .2}],
            'controls': [{'controller': 1, 'value': 20, 'timeSeconds': 0.},
                         {'controller': 1, 'value': 100, 'timeSeconds': 1.2}]})
        click('PerformanceKeyboard.PlaySequence')
        time.sleep(.45)
        after_note = state()['performance']
        assert after_note['previewPlaying']
        assert .2 < after_note['previewProgress'] < .8, after_note
        deadline = time.monotonic() + 3.
        previous = after_note['previewProgress']
        while True:
            observed = state()['performance']
            assert observed['previewPlaying'], observed
            if observed['previewProgress'] < previous:
                break
            assert time.monotonic() < deadline
            previous = observed['previewProgress']
            time.sleep(.04)
        capture('transport-looping')
        click('PerformanceKeyboard.PlaySequence')
        assert not state()['performance']['previewPlaying']
        command(command='resizeWindow', width=1000, height=700)
        check_list_alignment()
        capture('curves-compact')
        result = {'countLoadAddDeleteUndoRedo': True, 'octaveNativeClicks': True,
                  'playingAfterLastNote': after_note['previewProgress'], 'loopsAtLastNoteMeasure': True}
        (output / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result))


if __name__ == '__main__':
    main()
