#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
repo_root="$(cd -- "${script_dir}/.." >/dev/null 2>&1 && pwd)"
env_path="${repo_root}/.env"

if [ -f "$env_path" ]; then
  set -a
  # shellcheck disable=SC1090
  . "$env_path"
  set +a
fi

if [ -z "${JUCE_ROOT:-}" ]; then
  if [ -n "${JUCE_MODULES_DIR:-}" ]; then
    JUCE_ROOT="$(cd -- "${JUCE_MODULES_DIR}/.." >/dev/null 2>&1 && pwd)"
  else
    echo "JUCE_ROOT or JUCE_MODULES_DIR must be set in .env." >&2
    echo "Run scripts/make_env.sh to populate local SDK paths." >&2
    exit 1
  fi
fi

if [ ! -f "${JUCE_ROOT}/CMakeLists.txt" ] || [ ! -d "${JUCE_ROOT}/extras/AudioPluginHost" ]; then
  echo "JUCE root does not contain extras/AudioPluginHost: ${JUCE_ROOT}" >&2
  exit 1
fi

config="${AUDIO_PLUGIN_HOST_CONFIG:-Debug}"
build_dir="${AUDIO_PLUGIN_HOST_BUILD_DIR:-${repo_root}/build/audio-plugin-host}"
jobs="${JOBS:-10}"

cmake -S "$JUCE_ROOT" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE="$config" \
  -DJUCE_BUILD_EXTRAS=ON \
  -DJUCE_BUILD_EXAMPLES=OFF

cmake --build "$build_dir" --target AudioPluginHost --parallel "$jobs"

if [ "$(uname -s)" = "Darwin" ]; then
  host_path="${build_dir}/extras/AudioPluginHost/AudioPluginHost_artefacts/${config}/AudioPluginHost.app"
else
  host_path="${build_dir}/extras/AudioPluginHost/AudioPluginHost_artefacts/${config}/AudioPluginHost"
fi

if [ ! -e "$host_path" ]; then
  echo "AudioPluginHost target built, but expected artifact was not found: ${host_path}" >&2
  exit 1
fi

echo "$host_path"
