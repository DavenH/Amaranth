#!/usr/bin/env python3
"""Check grouped sidebar navigation, footer help, and native canvas captures."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from cycle_automation_support.session import Session

ROOT = Path(__file__).resolve().parent.parent


def check():
    from cycle_automation_support.registry import registry
    from test_cycle_v2_native_edit_smoke import NativeEditSmoke
    output = ROOT / '.context/console-sidebar'
    output.mkdir(parents=True, exist_ok=True)
    native = NativeEditSmoke()
    native.app_pid = int(os.environ['CYCLE_APP_PID'])
    with registry() as records:
        native.record = next(r for r in records['instances'].values()
                if r['pid'] == native.app_pid and r['socket'] == os.environ['CYCLE_AGENT_SESSION'])
    os.environ['CYCLE_V2_NATIVE_CAPTURE_DIR'] = str(output)
    with Session(os.environ['CYCLE_AGENT_SESSION'], timeout=120) as session:
        def send(**payload):
            result = session.command(payload)
            assert result.get('ok') and result['result'].get('ok'), result
            return result['result'].get('data', {})

        def targets():
            return {t['id']: t for t in send(command='inspectPointerTargets')['targets']}

        opened = send(command='openGraph', path=str(ROOT / 'cycle-v2/content/presets/sitar-pad.cyclegraph'))
        assert opened['guideCount'] > 0
        send(command='resizeWindow', width=1728, height=994)
        native.focus_app()
        observed = targets()
        ids = ['assets', 'presets', 'patterns', 'nodes', 'curves']
        tabs = [observed['workspace.sidebar.' + name]['bounds'] for name in ids]
        assert all(tabs[i]['x'] < tabs[i+1]['x'] for i in range(4)), tabs
        assert tabs[3]['x'] - tabs[2]['x'] - tabs[2]['width'] == 16, tabs
        origin = observed['comparisonWorkspace']['screenBounds']
        info = observed['workspace.presetInformation']['bounds']
        # The canvas footer sits immediately above the reference tray.
        assert origin['height'] - 80 < info['y'] < origin['height'] - 28, (origin, info)
        evidence = {'tabs': tabs, 'informationButton': info}
        for tab in ('nodes', 'curves', 'presets'):
            send(command='pointer', targetId='workspace.sidebar.' + tab, event='click')
            time.sleep(.25)
            native.capture(tab, origin)
        mod = targets()['PerformanceKeyboard.ModWheel']['bounds']
        point = (round(origin['x'] + mod['x'] + mod['width']/2),
                round(origin['y'] + mod['y'] + mod['height']/2))
        native.click(f'm:{point[0]},{point[1]}')
        time.sleep(.3)
        current = send(command='snapshotState')
        assert 'MOD' in current['consoleText'], current['consoleText']
        assert not any(c in current['consoleText'] for c in '\r\n\t')
        evidence['hoverState'] = current
        native.capture('console-hover', origin)
        send(command='resizeWindow', width=800, height=700)
        time.sleep(.2)
        native.capture('compact', targets()['comparisonWorkspace']['screenBounds'])
        send(command='resizeWindow', width=1728, height=994)
        (output / 'results.json').write_text(json.dumps(evidence, indent=2) + '\n')
        print(json.dumps({'ok': True, 'artifacts': str(output)}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--no-build', action='store_true')
    parser.add_argument('--in-session', action='store_true')
    args = parser.parse_args()
    if args.in_session:
        check()
        return 0
    command = [sys.executable, str(ROOT / 'scripts/cycle_automation.py'), 'exec',
            '--product', 'cycle-v2', '--clean', '--foreground']
    if args.no_build:
        command.append('--no-build')
    return subprocess.run(command + ['--', sys.executable, str(Path(__file__).resolve()),
            '--in-session'], cwd=ROOT).returncode


if __name__ == '__main__':
    sys.exit(main())
