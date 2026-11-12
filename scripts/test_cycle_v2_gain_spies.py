#!/usr/bin/env python3
"""Run through cycle_automation.py exec --product cycle-v2 --clean --foreground."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

from cycle_automation_support.session import Session
from cycle_automation_support.profiles import ROOT
from cycle_automation_support.lifecycle import compiled_helper


def main():
    with Session(os.environ['CYCLE_AGENT_SESSION'], timeout=90) as session:
        def command(**payload):
            return session.command(payload)['result'].get('data', {})

        command(command='openGraph', path=str(ROOT / 'cycle-v2/content/presets/sitar-pad.cyclegraph'))
        command(command='waitForIdle', idleDelayMs=300)
        graph = json.loads(command(command='exportGraph')['json'])
        gain = next(n for n in graph['nodes'] if n['kind'] == 'gain')['id']
        command(command='setNodeParameter', nodeId=gain, parameterId='gain', label='Gain', value='0.5')
        command(command='waitForIdle', idleDelayMs=300)
        output = ROOT / '.context/gain-spy-native'
        output.mkdir(exist_ok=True)

        def render(name):
            path = output / (name + '.png')
            path.unlink(missing_ok=True)
            command(command='renderSpyFrame', probeId='default-output', width=640, height=240,
                    gradient='greyscale', controllers=[0] * 128, path=str(path))
            return hashlib.sha256(path.read_bytes()).hexdigest()

        unity = render('unity')
        subprocess.run([str(compiled_helper('launch_cycle_app.swift')), '--focus', os.environ['CYCLE_APP_PID']], check=True)
        time.sleep(.3)
        targets = command(command='inspectPointerTargets')['targets']
        knob = next(t['bounds'] for t in targets if t['id'] == 'outputGain:' + gain)
        origin = command(command='inspectTargets', area='workspace')['targets'][0]['screenBounds']
        x = round(origin['x'] + knob['x'] + knob['width'] / 2)
        y = round(origin['y'] + knob['y'] + knob['height'] / 2)
        for event in (f'm:{x},{y}', f'dd:{x},{y}', f'dm:{x},{y+20}',
                      f'dm:{x},{y+42}', f'du:{x},{y+42}'):
            subprocess.run(['/opt/homebrew/bin/cliclick', event], check=True)
            time.sleep(.1)
        command(command='waitForIdle', idleDelayMs=300)
        graph = json.loads(command(command='exportGraph')['json'])
        value = next(n for n in graph['nodes'] if n['id'] == gain)['parameters']['gain']
        assert value < .3, value
        quiet = render('quiet')
        assert quiet != unity
        command(command='undo')
        command(command='waitForIdle', idleDelayMs=300)
        restored = render('undo')
        assert restored == unity
        result = {'nativeGainValue': value, 'spyPixelsChange': True, 'undoRestoresSpyPixels': True}
        (output / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result))


if __name__ == '__main__':
    main()
