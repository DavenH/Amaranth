#!/usr/bin/env python3
"""Exercise the installed executable's native movie worker (no automation server)."""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import time
import wave

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    root = args.output_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg:
        raise RuntimeError('ffmpeg is required')
    preset = json.loads((ROOT / 'cycle-v2/content/presets/bright-lead-7.cyclegraph').read_text())
    preset['presetPresentation']['timeSurfaceStyle'] = 'icy-hot-raw'
    preset['presetPresentation']['spyNoteOffTimes'] = {'default-output': .4}
    template = {'preset': json.dumps(preset), 'sequence': {'durationSeconds': .2,
        'notes': [{'pitch': 60, 'velocity': 100, 'startSeconds': 0, 'durationSeconds': .15}],
        'controls': [{'controller': 1, 'value': 90, 'timeSeconds': 0}]},
        'ffmpeg': ffmpeg, 'gradient': '', 'width': 320, 'height': 180,
        'fps': 10, 'bitrateMbps': 2, 'tailSeconds': .1}
    results = []
    cases = ['success with spaces', 'existing', 'missing-ffmpeg', 'encoder-error', 'cancel', 'invalid', 'blocked-parent']
    if os.name != 'nt':
        cases.append('encoding-cancel')
    for case in cases:
        folder = root / case
        folder.mkdir()
        job = json.loads(json.dumps(template))
        output = folder / 'movie'
        job['outputDirectory'] = str(output)
        if case == 'existing':
            output.mkdir()
            (output / 'sentinel').write_text('keep')
        elif case == 'missing-ffmpeg':
            job['ffmpeg'] = str(folder / 'missing')
        elif case == 'encoder-error':
            # An existing executable that rejects ffmpeg's arguments.
            job['ffmpeg'] = os.sys.executable
        elif case == 'invalid':
            job['width'] = 321
        elif case == 'blocked-parent':
            blocker = folder / 'blocker'
            blocker.write_text('keep')
            output = blocker / 'movie'
            job['outputDirectory'] = str(output)
        elif case == 'encoding-cancel':
            encoder = folder / 'test-encoder'
            encoder.write_text('#!' + os.sys.executable + '\nimport os,time\nfrom pathlib import Path\nPath(__file__+".pid").write_text(str(os.getpid()))\ntime.sleep(60)\n')
            encoder.chmod(0o755)
            job['ffmpeg'] = str(encoder)
        elif case == 'cancel':
            job['sequence']['durationSeconds'] = 2
            job['fps'] = 60
        (folder / 'job.json').write_text(json.dumps(job))
        environment = os.environ.copy()
        environment['PATH'] = ''  # Worker needs neither Python nor shell utilities.
        with (folder / 'worker.log').open('w') as log:
            process = subprocess.Popen([str(args.executable.resolve()), '--export-movie-job',
                str(folder / 'job.json')], cwd=folder, env=environment, stdout=log, stderr=log)
            deadline = time.monotonic() + 120
            requested_cancel = False
            while process.poll() is None and time.monotonic() < deadline:
                status_path = folder / 'status.json'
                if case in ('cancel', 'encoding-cancel') and status_path.exists():
                    status = json.loads(status_path.read_text())
                    ready = status.get('completedFrames', 0) > 0 if case == 'cancel' else (folder / 'test-encoder.pid').exists()
                    if ready:
                        (folder / 'cancel').write_text('cancel')
                        requested_cancel = True
                time.sleep(.02)
            if process.poll() is None:
                process.kill()
                raise RuntimeError(f'Worker timed out: {case}')
        status = json.loads((folder / 'status.json').read_text())
        expected = 'complete' if case == 'success with spaces' else 'cancelled' if case in ('cancel', 'encoding-cancel') else 'failed'
        assert status['status'] == expected, (case, status)
        assert (process.returncode == 0) == (expected == 'complete')
        if expected == 'complete':
            frames = sorted(output.glob('frame-*.png'))
            assert [p.name for p in frames] == [f'frame-{i:06d}.png' for i in range(3)]
            for frame in frames:
                assert struct.unpack('>II', frame.read_bytes()[16:24]) == (320, 180)
            with wave.open(str(output / 'audio.wav')) as audio:
                assert (audio.getframerate(), audio.getnchannels(), audio.getnframes()) == (48000, 2, 14400)
            streams = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-show_streams',
                '-of', 'json', str(output / 'sequence.mp4')]))['streams']
            assert streams[0]['codec_name'] == 'h264' and int(streams[0]['nb_frames']) == 3
            assert streams[1]['codec_name'] == 'aac'
            assert abs(float(streams[0]['duration']) - .3) < .001
            assert status['gradient'] == 'icy-hot-raw'
        elif case == 'existing':
            assert (output / 'sentinel').read_text() == 'keep'
            assert len(list(output.iterdir())) == 1
        elif case == 'encoder-error':
            assert len(list(output.glob('frame-*.png'))) == 3
            assert 'ffmpeg' in status['message']
        elif case == 'cancel':
            assert requested_cancel and 0 < status['completedFrames'] < status['totalFrames']
            assert len(list(output.glob('frame-*.png'))) == status['completedFrames']
            assert not (output / 'sequence.mp4').exists()
        elif case == 'blocked-parent':
            assert (folder / 'blocker').read_text() == 'keep'
        elif case == 'encoding-cancel':
            assert requested_cancel and not (output / 'sequence.mp4').exists()
            pid = int((folder / 'test-encoder.pid').read_text())
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                pass
            else:
                raise AssertionError('Cancelled encoder survived')
        results.append({'case': case, 'status': status['status']})
        print(case, status['status'], flush=True)
    (root / 'verification.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
