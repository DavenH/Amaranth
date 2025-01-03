#!/bin/bash

DEFAULT_INSTALL_ROOT="$HOME/SDKs"
INSTALL_ROOT=${INSTALL_ROOT:-$DEFAULT_INSTALL_ROOT}
IS_MACOS=$(uname -s | grep -i "darwin" > /dev/null && echo "true" || echo "false")

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

mkdir -p "$INSTALL_ROOT"

confirm() { read -p "$1 (y/n) " -n 1 -r; echo; [[ $REPLY =~ ^[Yy]$ ]]; }
command_exists() { command -v "$1" >/dev/null 2>&1; }

clone_or_update() {
    local repo_url="$1"
    local dir_name="$2"
    local recursive="$3"

    cd "$INSTALL_ROOT"
    if [ -d "$dir_name" ]; then
        echo -e "${BLUE}Updating $dir_name...${NC}"
        cd "$dir_name"
        git pull
        if [ "$recursive" = "true" ]; then
            git submodule update --init --recursive
        fi
    else
        echo -e "${BLUE}Cloning $dir_name...${NC}"
        if [ "$recursive" = "true" ]; then
            git clone --recursive "$repo_url" "$dir_name"
        else
            git clone "$repo_url" "$dir_name"
        fi
        cd "$dir_name"
    fi
}

install_prerequisites() {
    local missing_tools=()
    for cmd in git cmake wget; do
        if ! command_exists "$cmd"; then
            missing_tools+=("$cmd")
        fi
    done

    if [ ${#missing_tools[@]} -eq 0 ]; then
        return 0
    fi

    if $IS_MACOS; then
        if ! command_exists brew; then
            echo -e "${BLUE}Installing Homebrew...${NC}"
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        fi
        for tool in "${missing_tools[@]}"; do
            brew install "$tool"
        done
    else
        sudo apt-get update
        sudo apt-get install -y "${missing_tools[@]}"
    fi
}

install_compiler() {
    if ! $IS_MACOS && ! command_exists g++-11; then
        if confirm "Install g++-11?"; then
            sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
            sudo apt update
            sudo apt install -y g++-11
        fi
    fi
}

install_catch2() {
    if confirm "Install/Update Catch2?"; then
        clone_or_update "https://github.com/catchorg/Catch2.git" "Catch2" "false"
        git checkout master
        cmake -Bbuild -H. -DBUILD_TESTING=OFF
        cmake --build build/
        if $IS_MACOS; then
            sudo cmake --build build/ --target install
        else
            cmake --build build/ --target install
        fi
    fi
}

install_juce() {
    if confirm "Install/Update JUCE?"; then
        clone_or_update "https://github.com/juce-framework/JUCE.git" "JUCE" "false"
        git checkout master
        cmake -Bbuild -H. -DJUCE_BUILD_EXAMPLES=OFF -DJUCE_BUILD_EXTRAS=OFF
        cmake --build build
    fi
}

install_vst3() {
    if confirm "Install/Update VST3 SDK?"; then
        clone_or_update "https://github.com/steinbergmedia/vst3sdk.git" "vst3sdk" "true"
        git checkout master
        cmake -Bbuild -H.
        cmake --build build
    fi
}

install_ipp() {
    if confirm "Install Intel IPP?"; then
        echo -e "${BLUE}Installing Intel IPP...${NC}"
        if $IS_MACOS; then
            echo "For Apple Silicon, use the macOS ARM64 version"
            open "https://www.intel.com/content/www/us/en/developer/tools/oneapi/ipp-download.html"
        else
            echo "After installation, the default path will be: /opt/intel/oneapi/ipp/latest"
            xdg-open "https://www.intel.com/content/www/us/en/developer/tools/oneapi/ipp-download.html"
        fi
    fi
}

install_prerequisites
install_compiler
install_catch2
install_juce
install_vst3
install_ipp