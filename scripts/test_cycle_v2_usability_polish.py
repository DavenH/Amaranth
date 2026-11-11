#!/usr/bin/env python3
"""Run under cycle_automation.py exec --product cycle-v2 --clean --foreground."""
import json
import os
import subprocess
import time

from cycle_automation_support.lifecycle import compiled_helper
from cycle_automation_support.profiles import ROOT
from cycle_automation_support.session import Session


def main():
    with Session(os.environ['CYCLE_AGENT_SESSION'], timeout=60) as session:
        def command(**payload):
            return session.command(payload)['result'].get('data', {})

        command(command='openGraph', path=str(ROOT / 'cycle-v2/content/presets/sitar-pad.cyclegraph'))
        command(command='waitForIdle', idleDelayMs=400)
        graph = json.loads(command(command='exportGraph')['json'])
        assert graph['formatVersion'] == 10
        gain = next(n for n in graph['nodes'] if n['kind'] == 'gain')
        output = next(n for n in graph['nodes'] if n['kind'] == 'globalOutput')
        assert not output['parameters']
        outgoing = {e['sourceNodeId']: e['destNodeId'] for e in graph['edges']
                    if e.get('connectionKind', 'signal') == 'signal'}
        delay = next(n for n in graph['nodes'] if n['kind'] == 'delay')
        assert outgoing[gain['id']] == delay['id']
        assert outgoing[delay['id']] == output['id']
        deletion = session.command({'command': 'deleteNode', 'nodeId': output['id']}, require_ok=False)
        assert not deletion['result']['ok']
        assert json.loads(command(command='exportGraph')['json']) == graph

        subprocess.run([str(compiled_helper('launch_cycle_app.swift')), '--focus', os.environ['CYCLE_APP_PID']], check=True)
        time.sleep(.1)

        def targets():
            return command(command='inspectPointerTargets')['targets']

        def target(identifier):
            return next(t for t in targets() if t['id'] == identifier)

        def area(identifier):
            return command(command='inspectTargets', area=identifier)['targets'][0]

        def native_click(identifier):
            bounds = target(identifier)['bounds']
            origin = area('workspace')['screenBounds']
            x = round(origin['x'] + bounds['x'] + bounds['width'] / 2)
            y = round(origin['y'] + bounds['y'] + bounds['height'] / 2)
            subprocess.run(['/opt/homebrew/bin/cliclick', f'c:{x},{y}'], check=True)
            time.sleep(.15)

        def capture(name):
            bounds = area('workspace')['screenBounds']
            region = ','.join(str(round(bounds[k])) for k in ('x', 'y', 'width', 'height'))
            subprocess.run(['screencapture', '-x', '-R' + region, str(ROOT / '.context' / name)], check=True)

        closed = area('canvas')['bounds']
        assert closed['height'] == area('workspace')['bounds']['height']
        assert target('comparison.drop')['bounds']['width'] == 190
        native_click('comparison.toggle')
        assert command(command='snapshotState')['comparison']['visible']
        assert any(t['id'] == 'comparison.assets' for t in targets())
        capture('polish-tray-expanded.png')
        native_click('comparison.toggle')
        assert not command(command='snapshotState')['comparison']['visible']
        assert area('canvas')['bounds']['height'] == closed['height']
        capture('polish-final.png')
        trash = next(t for t in targets() if t['id'].startswith('workspace.sidebar.presetDelete.'))
        origin = area('workspace')['screenBounds']
        b = trash['bounds']
        x = round(origin['x'] + b['x'] + b['width'] / 2)
        y = round(origin['y'] + b['y'] + b['height'] / 2)
        subprocess.run(['/opt/homebrew/bin/cliclick', f'm:{x},{y}'], check=True)
        time.sleep(.1)
        capture('polish-preset-trash.png')
        native_click(trash['id'])
        command(command='screenshot', area='modal', path=str(ROOT / '.context/polish-delete-confirmation.png'))
        command(command='key', area='modal', key='escape')
        assert any(t['id'] == trash['id'] for t in targets())
        command(command='sendMidi', type='noteOn', note=60, velocity=.7)
        time.sleep(.15)
        held = command(command='snapshotState')['performance']['waveform']
        assert held['cursorCount'] > 0 and not any(held['cursorReleased'])
        command(command='sendMidi', type='noteOff', note=60)
        time.sleep(.05)
        released = command(command='snapshotState')['performance']['waveform']
        assert any(released['cursorReleased'])
        for key in ('captureRequests', 'capturePublications', 'sourceBuilds', 'heatBuilds', 'geometryBuilds'):
            assert released[key] == held[key], (key, held, released)
        capture('polish-released-cursor.png')
        command(command='sendMidi', type='allNotesOff')
        command(command='resizeWindow', width=1000, height=700)
        command(command='openGraph', path=str(ROOT / 'cycle-v2/content/presets/sitar-pad.cyclegraph'))
        command(command='waitForIdle', idleDelayMs=400)
        assert area('canvas')['bounds']['height'] == area('workspace')['bounds']['height']
        capture('polish-compact.png')
        result = {'trayCollapsedHeight': closed['height'], 'trayReservesHeight': False,
                  'nativeToggle': True, 'nativeTrashCancel': True,
                  'explicitGainBeforeWetFx': True, 'globalOutProtected': True,
                  'releaseCursorWithoutRebuild': True}
        (ROOT / '.context/polish-native-results.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result))


if __name__ == '__main__':
    main()
