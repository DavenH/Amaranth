#!/bin/bash

## NOTE this hasn't been tested yet. Catch2 needs a different install procedure

DEFAULT_INSTALL_ROOT="$HOME/SDKs"
INSTALL_ROOT=${INSTALL_ROOT:-$DEFAULT_INSTALL_ROOT}

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

mkdir -p "$INSTALL_ROOT"

echo -e "${BLUE}Setting up development SDKs in ${INSTALL_ROOT}${NC}\n"

confirm() {
    read -p "$1 (y/n) " -n 1 -r
    echo
    [[ $REPLY =~ ^[Yy]$ ]]
}

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check for required tools
for cmd in git cmake wget; do
    if ! command_exists $cmd; then
        echo -e "${RED}Error: $cmd is required but not installed.${NC}"
        exit 1
    fi
done

install_gcc11() {
    if command_exists g++-11; then
        echo -e "${GREEN}g++-11 is already installed${NC}"
        return 0
    fi

    if confirm "Install g++-11?"; then
        echo -e "${BLUE}Installing g++-11...${NC}"

        # Ubuntu/Debian
        if command_exists apt; then
            sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
            sudo apt update
            sudo apt install -y g++-11
        # Fedora
        elif command_exists dnf; then
            sudo dnf install -y gcc-c++.x86_64 gcc-toolset-11
        else
            echo -e "${RED}Unsupported package manager. Please install g++-11 manually.${NC}"
            return 1
        fi

        echo -e "${GREEN}g++-11 installed successfully${NC}"
    fi
}

install_catch2() {
    if confirm "Install Catch2?"; then
        echo -e "${BLUE}Installing Catch2...${NC}"
        cd "$INSTALL_ROOT"
        git clone https://github.com/catchorg/Catch2.git
        cd Catch2
        git checkout master
        cmake -Bbuild -H. -DBUILD_TESTING=OFF
        cmake --build build/ --target install
        echo -e "${GREEN}Catch2 installed successfully${NC}"
    fi
}

install_juce() {
    if confirm "Install JUCE?"; then
        echo -e "${BLUE}Installing JUCE...${NC}"
        cd "$INSTALL_ROOT"
        git clone https://github.com/juce-framework/JUCE.git
        cd JUCE
        git checkout master
        cmake -Bbuild -H. -DJUCE_BUILD_EXAMPLES=OFF -DJUCE_BUILD_EXTRAS=OFF
        cmake --build build
        echo -e "${GREEN}JUCE installed successfully${NC}"
    fi
}

install_vst3() {
    if confirm "Install VST3 SDK?"; then
        echo -e "${BLUE}Installing VST3 SDK...${NC}"
        cd "$INSTALL_ROOT"
        git clone --recursive https://github.com/steinbergmedia/vst3sdk.git
        cd vst3sdk
        git checkout master
        cmake -Bbuild -H.
        cmake --build build
        echo -e "${GREEN}VST3 SDK installed successfully${NC}"
    fi
}

install_ipp() {
    if confirm "Install Intel IPP?"; then
        echo -e "${BLUE}Installing Intel IPP...${NC}"
        echo "Please note: Intel IPP requires manual download and installation from:"
        echo "https://www.intel.com/content/www/us/en/developer/tools/oneapi/ipp-download.html"
        echo "After installation, the default path will be: /opt/intel/oneapi/ipp/latest"
        if confirm "Would you like to proceed with manual installation?"; then
            xdg-open "https://www.intel.com/content/www/us/en/developer/tools/oneapi/ipp-download.html"
        fi
    fi
}

install_gcc11
install_catch2
install_juce
install_vst3
install_ipp
