# Reconstruct the pinned calibration corpus

These are the original acquisition metadata and CC0 licence, with the exact
bytes hashed by `vcsl-calibration.json`. They contain source URLs, original
hashes/headers and the partition declared before retrieval. Audio stays outside
Git. Copy these three files into a fresh local corpus directory, then download
each manifest entry to its `localPath` and verify SHA256 before using the study.
The following example runs from the repository root:

```sh
python3 - <<'PY'
from pathlib import Path
import hashlib
import json
import shutil
import urllib.request

metadata = Path('cycle-v2/content/studies/expressive-sax/vcsl-corpus')
corpus = Path('.context/vcsl-calibration-corpus')
corpus.mkdir(parents=True, exist_ok=False)
for name in ('download-manifest.json', 'selection.json', 'LICENSE'):
    shutil.copy2(metadata / name, corpus / name)
manifest = json.loads((corpus / 'download-manifest.json').read_text())
for record in manifest['audioFiles']:
    destination = corpus / record['localPath']
    destination.parent.mkdir(parents=True, exist_ok=True)
    data = urllib.request.urlopen(record['url'], timeout=60).read()
    if hashlib.sha256(data).hexdigest() != record['sha256']:
        raise ValueError('Source identity changed: ' + record['id'])
    destination.write_bytes(data)
print(corpus.resolve())
PY
```

Supply that printed directory with `--corpus-root`. Downloading and hashing
reserved originals is permitted; decoding, listening or analysing them for
refinement before the audit would invalidate the declared separation. The
calibration validates every selected source against the pinned acquisition.
Extra web snapshots listed in the historical manifest are acquisition context;
the runner requires the three files here and the twelve original WAVs.
