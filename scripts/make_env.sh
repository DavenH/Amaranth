#!/usr/bin/env bash
# Robust env writer for local/dev/CI/containers
# - Skips IPP requirement on macOS (uses vDSP/Accelerate instead)
# - Non-interactive mode if NONINTERACTIVE=1 or CI is set
# - Writes .env next to repo root (one level up from this script)

set -euo pipefail

DEFAULT_INSTALL_ROOT="${HOME}/SDKs"
INSTALL_ROOT="${INSTALL_ROOT:-$DEFAULT_INSTALL_ROOT}"

OS_NAME="$(uname -s || echo unknown)"
IS_MACOS=false
if echo "$OS_NAME" | grep -qi "darwin"; then IS_MACOS=true; fi

# Colors (no-op if not TTY)
if [ -t 1 ]; then
  RED='\033[0;31m'; GREEN='\033[0;32m'; BLUE='\033[0;34m'; NC='\033[0m'
else
  RED=''; GREEN=''; BLUE=''; NC=''
fi

NONINTERACTIVE="${NONINTERACTIVE:-${CI:-0}}"

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
env_path="${script_dir}/../.env"

prompt_directory() {
  local prompt="$1"
  local default="$2"
  local result=""

  if [ -n "$NONINTERACTIVE" ] && [ "$NONINTERACTIVE" != "0" ]; then
    echo "$default"
    return 0
  fi

  echo -e "${BLUE}${prompt}${NC}"
  echo -n "Default [${default}]: "
  read -r result
  if [ -z "$result" ]; then
    echo "$default"
  else
    echo "$result"
  fi
}

validate_directory() {
  local dir="$1"
  local name="$2"
  local required="${3:-}"

  if [ ! -d "$dir" ]; then
    echo -e "${RED}Warning:${NC} ${name} directory does not exist: ${dir}"
    if [ "$required" = "required" ]; then
      echo -e "${RED}This is a required directory for builds on this platform.${NC}"
      return 1
    fi
    return 0
  fi
  return 0
}

echo -e "${BLUE}Setting up Amaranth development environment${NC}"
echo "This script writes a .env with SDK locations"
echo

# Defaults (same for both OS except IPP is unused on macOS)
DEFAULT_CATCH2_DIR="/usr/local/lib/cmake/Catch2"
DEFAULT_IPP_DIR="/opt/intel/oneapi/ipp/latest"   # ignored on macOS
DEFAULT_JUCE_MODULES_DIR="${INSTALL_ROOT}/JUCE/modules"
DEFAULT_VST3_DIR="${INSTALL_ROOT}/vst3sdk"

JUCE_DIR="$(prompt_directory "Enter JUCE modules directory" "${DEFAULT_JUCE_MODULES_DIR}")"
VST3_DIR="$(prompt_directory "Enter VST3 SDK directory" "${DEFAULT_VST3_DIR}")"

if $IS_MACOS; then
  # IPP is not used on macOS; vDSP/Accelerate is part of Xcode/CLT
  IPP_DIR=""
  CATCH2_DIR="$(prompt_directory "Enter Catch2 directory" "${DEFAULT_CATCH2_DIR}")"
else
  IPP_DIR="$(prompt_directory "Enter Intel IPP directory (Linux only)" "${DEFAULT_IPP_DIR}")"
  CATCH2_DIR="$(prompt_directory "Enter Catch2 directory" "${DEFAULT_CATCH2_DIR}")"
fi

mkdir -p "$(dirname "$INSTALL_ROOT")"

# Write .env
{
  echo "# Amaranth SDK Locations"
  echo "# Generated on $(date)"
  echo "# Platform: ${OS_NAME}"
  echo
  echo "JUCE_MODULES_DIR=${JUCE_DIR}"
  echo "VST3_SDK_DIR=${VST3_DIR}"
  echo "CATCH2_CMAKE_DIR=${CATCH2_DIR}"
  if $IS_MACOS; then
    echo "MACOS=1"
    echo "VDSP=1               # macOS uses Accelerate/vDSP"
    echo "IPP_DIR=             # unused on macOS"
  else
    echo "LINUX=1"
    echo "IPP_DIR=${IPP_DIR}"
  fi
} > "${env_path}"

echo -e "\n${GREEN}Environment file created:${NC} ${env_path}"
echo
echo "Validating directories..."
ERRORS=0

validate_directory "$JUCE_DIR" "JUCE modules" "required" || ((ERRORS++))
validate_directory "$VST3_DIR" "VST3 SDK" "required" || ((ERRORS++))
validate_directory "$CATCH2_DIR" "Catch2" "required" || ((ERRORS++))

if ! $IS_MACOS; then
  # On Linux, IPP may be required if you actually use it; keep as "optional" here,
  # and let CMake feature-detect (safer for CI/containers).
  validate_directory "$IPP_DIR" "Intel IPP" "" || true
fi

if [ $ERRORS -gt 0 ]; then
  echo -e "\n${RED}Some required directories are missing.${NC}"
  echo "Run ./install_deps.sh (optionally NONINTERACTIVE=1) to set up missing dependencies."
else
  echo -e "\n${GREEN}All required directories validated!${NC}"
fi
