#!/bin/bash

DEFAULT_INSTALL_ROOT="$HOME/SDKs"
INSTALL_ROOT=${INSTALL_ROOT:-$DEFAULT_INSTALL_ROOT}
IS_MACOS=$(uname -s | grep -i "darwin" > /dev/null && echo "true" || echo "false")

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

prompt_directory() {
    local prompt="$1"
    local default="$2"
    local result

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

    if [ ! -d "$dir" ]; then
        echo -e "${RED}Warning: ${name} directory does not exist: ${dir}${NC}"
        if [ -n "${3:-}" ] && [ "$3" = "required" ]; then
            echo -e "${RED}This is a required directory. Please ensure it exists before using the environment file.${NC}"
        fi
        return 1
    fi
    return 0
}

echo -e "${BLUE}Setting up Amaranth development environment${NC}"
echo "This script will create a .env file with SDK locations"
echo

# Set platform-specific defaults
if $IS_MACOS; then
    DEFAULT_IPP_DIR="/opt/intel/oneapi/ipp/latest"
    DEFAULT_CATCH2_DIR="/usr/local/lib/cmake/Catch2"
else
    DEFAULT_IPP_DIR="/opt/intel/oneapi/ipp/latest"
    DEFAULT_CATCH2_DIR="/usr/local/lib/cmake/Catch2"
fi

# Prompt for each SDK location
JUCE_DIR=$(prompt_directory "Enter JUCE modules directory" "${INSTALL_ROOT}/JUCE/modules")
VST3_DIR=$(prompt_directory "Enter VST3 SDK directory" "${INSTALL_ROOT}/vst3sdk")
IPP_DIR=$(prompt_directory "Enter Intel IPP directory" "$DEFAULT_IPP_DIR")
CATCH2_DIR=$(prompt_directory "Enter Catch2 directory" "$DEFAULT_CATCH2_DIR")

# Create output directory if it doesn't exist
mkdir -p "$(dirname "$INSTALL_ROOT")"

# Create the .env file with platform-specific settings
cat > ../.env << EOF
# Amaranth SDK Locations
# Generated on $(date)
# Platform: $(uname -s)

JUCE_MODULES_DIR=${JUCE_DIR}
VST3_SDK_DIR=${VST3_DIR}
IPP_DIR=${IPP_DIR}
CATCH2_CMAKE_DIR=${CATCH2_DIR}

# Platform-specific settings
$($IS_MACOS && echo "MACOS=1" || echo "LINUX=1")
EOF

echo -e "\n${GREEN}Environment file created: .env${NC}"
echo
echo "Validating directories..."

# Validate with platform-specific messaging
ERRORS=0
validate_directory "$JUCE_DIR" "JUCE modules" "required" || ((ERRORS++))
validate_directory "$VST3_DIR" "VST3 SDK" "required" || ((ERRORS++))
validate_directory "$IPP_DIR" "Intel IPP" "required" || ((ERRORS++))
validate_directory "$CATCH2_DIR" "Catch2" "required" || ((ERRORS++))

if [ $ERRORS -gt 0 ]; then
    echo -e "\n${RED}Warning: Some directories are missing. Please install missing SDKs before building.${NC}"
    echo "Run ./install_deps.sh to set up missing dependencies."
    if $IS_MACOS; then
        echo "Note: On macOS, some paths might differ if using Homebrew installations."
    fi
else
    echo -e "\n${GREEN}All directories validated successfully!${NC}"
fi