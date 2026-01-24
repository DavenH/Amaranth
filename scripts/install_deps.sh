#!/usr/bin/env bash
# Cross-platform, container-friendly dependency installer
# - No GUI/browser opens
# - Non-interactive compatible (NONINTERACTIVE=1 or CI)
# - Avoids sudo if already root or sudo missing
# - Detects package manager (apt/yum/dnf/pacman/apk/brew)
# - Skips IPP on macOS (uses Accelerate/vDSP with Xcode CLT)

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

mkdir -p "$INSTALL_ROOT"

is_root() { [ "$(id -u)" = "0" ]; }
have_cmd() { command -v "$1" >/dev/null 2>&1; }

maybe_sudo() {
  if is_root; then
    "$@"
  elif have_cmd sudo; then
    sudo "$@"
  else
    # try without sudo (container users often are root anyway)
    "$@"
  fi
}

confirm() {
  local prompt="$1"
  if [ -n "$NONINTERACTIVE" ] && [ "$NONINTERACTIVE" != "0" ]; then
    return 0
  fi
  read -p "$prompt (y/n) " -n 1 -r
  echo
  [[ $REPLY =~ ^[Yy]$ ]]
}

detect_pkg() {
  if $IS_MACOS; then echo "brew"; return; fi
  for pm in apt-get dnf yum pacman apk; do
    if have_cmd "$pm"; then echo "$pm"; return; fi
  done
  echo "none"
}

pkg_install() {
  local pm="$1"; shift
  local pkgs=("$@")
  case "$pm" in
    apt-get)
      maybe_sudo apt-get update -y
      maybe_sudo apt-get install -y "${pkgs[@]}"
      ;;
    dnf)    maybe_sudo dnf install -y "${pkgs[@]}";;
    yum)    maybe_sudo yum install -y "${pkgs[@]}";;
    pacman) maybe_sudo pacman -Sy --noconfirm "${pkgs[@]}";;
    apk)    maybe_sudo apk add --no-cache "${pkgs[@]}";;
    brew)
      if ! have_cmd brew; then
        echo -e "${BLUE}Installing Homebrew...${NC}"
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        eval "$(/opt/homebrew/bin/brew shellenv 2>/dev/null || true)"
        eval "$(/usr/local/bin/brew shellenv 2>/dev/null || true)"
      fi
      brew install "${pkgs[@]}" || true
      ;;
    *)
      echo -e "${RED}No supported package manager detected. Install prerequisites manually: ${pkgs[*]}${NC}"
      ;;
  esac
}

clone_or_update() {
  local repo_url="$1"
  local dir_name="$2"
  local recursive="${3:-false}"

  cd "$INSTALL_ROOT"
  if [ -d "$dir_name/.git" ]; then
    echo -e "${BLUE}Updating $dir_name...${NC}"
    git -C "$dir_name" pull --ff-only
    if [ "$recursive" = "true" ]; then
      git -C "$dir_name" submodule update --init --recursive
    fi
  else
    echo -e "${BLUE}Cloning $dir_name...${NC}"
    if [ "$recursive" = "true" ]; then
      git clone --depth=1 --recursive "$repo_url" "$dir_name"
    else
      git clone --depth=1 "$repo_url" "$dir_name"
    fi
  fi
}

install_prerequisites() {
  local need=(git cmake)
  # wget is optional; curl can substitute
  if ! have_cmd curl && ! have_cmd wget; then need+=(curl); fi

  local missing=()
  for c in "${need[@]}"; do
    have_cmd "$c" || missing+=("$c")
  done
  if [ ${#missing[@]} -eq 0 ]; then return 0; fi

  local pm
  pm="$(detect_pkg)"
  echo -e "${BLUE}Installing prerequisites via ${pm}...${NC}"
  pkg_install "$pm" "${missing[@]}"

  # macOS: ensure Xcode Command Line Tools for headers/Accelerate
  if $IS_MACOS && ! xcode-select -p >/dev/null 2>&1; then
    echo -e "${BLUE}Installing Xcode Command Line Tools (required for vDSP/Accelerate)...${NC}"
    # In CI/headless macs this might need manual provisioning; this command is best-effort:
    xcode-select --install || true
  fi
}

install_compiler() {
  # Prefer system compiler unless you truly need g++-11.
  if ! $IS_MACOS; then
    if ! have_cmd g++ && ! have_cmd g++-11; then
      if confirm "Install a C++ compiler (g++)?"; then
        local pm; pm="$(detect_pkg)"
        case "$pm" in
          apt-get) pkg_install "$pm" build-essential g++ ;;
          dnf|yum) pkg_install "$pm" gcc-c++ make ;;
          pacman)  pkg_install "$pm" base-devel gcc ;;
          apk)     pkg_install "$pm" build-base g++ ;;
          *)       echo -e "${RED}Please install a C++ compiler manually.${NC}" ;;
        esac
      fi
    fi
  fi
}

install_catch2() {
  if confirm "Install/Update Catch2?"; then
    clone_or_update "https://github.com/catchorg/Catch2.git" "Catch2" "false"
    cmake -S "${INSTALL_ROOT}/Catch2" -B "${INSTALL_ROOT}/Catch2/build" -DBUILD_TESTING=OFF
    cmake --build "${INSTALL_ROOT}/Catch2/build" -j
    # Try to install to default prefix; fallback without sudo
    if $IS_MACOS; then
      maybe_sudo cmake --install "${INSTALL_ROOT}/Catch2/build"
    else
      maybe_sudo cmake --install "${INSTALL_ROOT}/Catch2/build"
    fi
  fi
}

install_juce() {
  if confirm "Install/Update JUCE?"; then
    clone_or_update "https://github.com/juce-framework/JUCE.git" "JUCE" "false"
    cmake -S "${INSTALL_ROOT}/JUCE" -B "${INSTALL_ROOT}/JUCE/build" -DJUCE_BUILD_EXAMPLES=OFF -DJUCE_BUILD_EXTRAS=OFF
    cmake --build "${INSTALL_ROOT}/JUCE/build" -j
  fi
}

install_vst3() {
  if confirm "Install/Update VST3 SDK?"; then
    clone_or_update "https://github.com/steinbergmedia/vst3sdk.git" "vst3sdk" "true"
    cmake -S "${INSTALL_ROOT}/vst3sdk" -B "${INSTALL_ROOT}/vst3sdk/build"
    cmake --build "${INSTALL_ROOT}/vst3sdk/build" -j
  fi
}

install_ipp() {
  if $IS_MACOS; then
    echo -e "${GREEN}Skipping IPP on macOS.${NC} Using Accelerate/vDSP from Xcode/CLT."
    return 0
  fi
  if confirm "Install Intel IPP (Linux only)?"; then
    # 1) Download
    wget -q https://registrationcenter-download.intel.com/akdlm/IRC_NAS/d9649232-67ed-489e-8cd8-2c4c54b06135/intel-ipp-2022.2.0.583_offline.sh

    # 2) Install silently to $HOME/intel/oneapi
    sh ./intel-ipp-2022.2.0.583_offline.sh -a --silent --eula accept --install-dir "$HOME/intel/oneapi"

    # 3) Load env vars for this session (and put this in your build script)
    . "$HOME/intel/oneapi/ipp/latest/env/vars.sh"   # sets IPPROOT, LD_LIBRARY_PATH, etc.
  fi
}

install_prerequisites
install_compiler
install_catch2
install_juce
install_vst3
install_ipp

echo -e "${GREEN}Dependency setup complete.${NC}"
echo "Tip: run NONINTERACTIVE=1 ./make_env.sh to generate a .env without prompts."
