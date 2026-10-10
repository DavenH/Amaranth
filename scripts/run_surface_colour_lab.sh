#!/bin/sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
lab_venv="$script_dir/../build/surface-colour-lab-venv"
if [ ! -x "$lab_venv/bin/python" ]; then
    python3 -m venv "$lab_venv"
fi
if ! "$lab_venv/bin/python" -c 'import numpy, scipy, PIL' 2>/dev/null; then
    "$lab_venv/bin/python" -m pip install -r "$script_dir/surface_colour_lab_requirements.txt"
fi
exec "$lab_venv/bin/python" "$script_dir/surface_colour_lab.py" "$@"
