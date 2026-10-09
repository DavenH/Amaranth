#include <iterator>

#include "UI/NodePalette.h"

namespace CycleV2 {

namespace {

constexpr float kX = 18.f;
constexpr float kY = 74.f;
constexpr float kTileWidth = 40.f;
constexpr float kTileHeight = 40.f;
constexpr float kGap = 3.f;
constexpr float kHeadingHeight = 16.f;
constexpr float kGroupGap = 6.f;
constexpr int kColumns = 3;

const NodePalette::Entry kContextEntries[] = {
        { NodeKind::VoiceContext, "Voice" }
};

const NodePalette::Entry kTransformEntries[] = {
        { NodeKind::Fft, "Time → Freq" },
        { NodeKind::Ifft, "Freq → Time" }
};

const NodePalette::Entry kMathEntries[] = {
        { NodeKind::Add, "Add" },
        { NodeKind::Multiply, "Multiply" }
};

const NodePalette::Entry kSourceEntries[] = {
        { NodeKind::TrilinearMesh, "Mesh" },
        { NodeKind::ImageSource, "Image" },
        { NodeKind::WaveSource, "Wave" }
};

const NodePalette::Entry kControlEntries[] = {
        { NodeKind::ModulationSource, "Modulation" },
        { NodeKind::ModulationTriple, "Mod Triple" },
        { NodeKind::Envelope, "Envelope" },
        { NodeKind::ScratchDefaultOverride, "Ignore Scratch" }
};

const NodePalette::Entry kFxEntries[] = {
        { NodeKind::ImpulseResponse, "IR" },
        { NodeKind::Waveshaper, "Waveshaper" },
        { NodeKind::Unison, "Unison" },
        { NodeKind::Reverb, "Reverb" },
        { NodeKind::Delay, "Delay" },
        { NodeKind::Equalizer, "EQ" }
};

const NodePalette::Section kSections[] = {
        { "Context", kContextEntries, (int) std::size(kContextEntries) },
        { "Transform", kTransformEntries, (int) std::size(kTransformEntries) },
        { "Math", kMathEntries, (int) std::size(kMathEntries) },
        { "Source", kSourceEntries, (int) std::size(kSourceEntries) },
        { "Control", kControlEntries, (int) std::size(kControlEntries) },
        { "FX", kFxEntries, (int) std::size(kFxEntries) }
};

float groupHeight(const NodePalette::Section& section) {
    const int rows = (section.entryCount + kColumns - 1) / kColumns;
    return kHeadingHeight + (float) rows * (kTileHeight + kGap) - kGap;
}

}

int NodePalette::sectionCount() const {
    return (int) std::size(kSections);
}

const NodePalette::Section& NodePalette::section(int sectionIndex) const {
    jassert(isPositiveAndBelow(sectionIndex, sectionCount()));
    return kSections[(size_t) sectionIndex];
}

Rectangle<float> NodePalette::railBounds() const {
    return { kX, kY, kColumns * (kTileWidth + kGap) - kGap,
            groupBounds(sectionCount() - 1).getBottom() - kY };
}

Rectangle<float> NodePalette::groupBounds(int sectionIndex) const {
    float y = kY;
    for (int index = 0; index < sectionIndex; ++index) {
        y += groupHeight(section(index)) + kGroupGap;
    }

    const int columns = jmin(kColumns, section(sectionIndex).entryCount);
    return { kX, y, (float) columns * (kTileWidth + kGap) - kGap,
            groupHeight(section(sectionIndex)) };
}

Rectangle<float> NodePalette::entryBounds(int sectionIndex, int entryIndex) const {
    const auto group = groupBounds(sectionIndex);
    const int row = entryIndex / kColumns;
    const int column = entryIndex % kColumns;
    return { group.getX() + (float) column * (kTileWidth + kGap),
            group.getY() + kHeadingHeight + (float) row * (kTileHeight + kGap),
            kTileWidth, kTileHeight };
}

int NodePalette::findSectionAt(Point<float> screenPosition) const {
    for (int sectionIndex = 0; sectionIndex < sectionCount(); ++sectionIndex) {
        const auto heading = groupBounds(sectionIndex).withHeight(kHeadingHeight);
        if (heading.contains(screenPosition)) {
            return sectionIndex;
        }
        for (int entryIndex = 0; entryIndex < section(sectionIndex).entryCount; ++entryIndex) {
            if (entryBounds(sectionIndex, entryIndex).contains(screenPosition)) {
                return sectionIndex;
            }
        }
    }
    return -1;
}

bool NodePalette::findKindAt(Point<float> screenPosition, NodeKind& kind) const {
    const int sectionIndex = findSectionAt(screenPosition);

    if (sectionIndex < 0) {
        return false;
    }

    const auto& active = section(sectionIndex);

    for (int entryIndex = 0; entryIndex < active.entryCount; ++entryIndex) {
        if (entryBounds(sectionIndex, entryIndex).contains(screenPosition)) {
            kind = active.entries[entryIndex].kind;
            return true;
        }
    }

    return false;
}

bool NodePalette::updateHover(Point<float> screenPosition) {
    const int previousSection = activeSectionIndex;
    const int previousEntry = activeEntryIndex;
    activeSectionIndex = findSectionAt(screenPosition);
    activeEntryIndex = -1;
    if (activeSectionIndex >= 0) {
        for (int index = 0; index < section(activeSectionIndex).entryCount; ++index) {
            if (entryBounds(activeSectionIndex, index).contains(screenPosition)) {
                activeEntryIndex = index;
                break;
            }
        }
    }
    return activeSectionIndex != previousSection || activeEntryIndex != previousEntry;
}

bool NodePalette::close() {
    if (activeSectionIndex < 0) {
        return false;
    }

    activeSectionIndex = -1;
    activeEntryIndex = -1;
    return true;
}

}
