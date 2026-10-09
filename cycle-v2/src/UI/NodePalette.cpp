#include <iterator>

#include "UI/NodePalette.h"
#include "Graph/NodeDefinition.h"

namespace CycleV2 {

namespace {

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

void NodePalette::setWorkspaceBounds(Rectangle<float> bounds) {
    workspace = bounds;
    close();
}

void NodePalette::setVisible(bool value) {
    visible = value;
    close();
}

int NodePalette::sectionCount() const {
    return (int) std::size(kSections);
}

const NodePalette::Section& NodePalette::section(int sectionIndex) const {
    jassert(isPositiveAndBelow(sectionIndex, sectionCount()));
    return kSections[(size_t) sectionIndex];
}

std::vector<std::pair<String, Rectangle<float>>> NodePalette::pointerTargets() const {
    std::vector<std::pair<String, Rectangle<float>>> targets;
    if (!visible) {
        return targets;
    }
    for (int sectionIndex = 0; sectionIndex < sectionCount(); ++sectionIndex) {
        const auto& group = section(sectionIndex);
        for (int index = 0; index < group.entryCount; ++index) {
            const auto* definition = NodeDefinitionRegistry::instance().find(group.entries[index].kind);
            if (definition != nullptr) {
                targets.emplace_back("palette:" + definition->typeId, entryBounds(sectionIndex, index));
            }
        }
    }
    return targets;
}

Rectangle<float> NodePalette::railBounds() const {
    const auto first = groupBounds(0);
    const float width = kColumns * (kTileWidth + kGap) - kGap;
    return { first.getRight() - width, first.getY(), width,
            groupBounds(sectionCount() - 1).getBottom() - first.getY() };
}

Rectangle<float> NodePalette::groupBounds(int sectionIndex) const {
    float y = workspace.getY() + kY;
    for (int index = 0; index < sectionIndex; ++index) {
        y += groupHeight(section(index)) + kGroupGap;
    }

    const int columns = jmin(kColumns, section(sectionIndex).entryCount);
    const float width = kColumns * (kTileWidth + kGap) - kGap;
    const float x = jmax(workspace.getX(), workspace.getCentreX() - width * 0.5f);
    return { x + (kColumns - columns) * (kTileWidth + kGap), y,
            (float) columns * (kTileWidth + kGap) - kGap,
            groupHeight(section(sectionIndex)) };
}

Rectangle<float> NodePalette::entryBounds(int sectionIndex, int entryIndex) const {
    const auto group = groupBounds(sectionIndex);
    const int row = entryIndex / kColumns;
    const int column = entryIndex % kColumns;
    const int rowColumns = jmin(kColumns, section(sectionIndex).entryCount - row * kColumns);
    return { group.getRight() - (float) (rowColumns - column) * (kTileWidth + kGap) + kGap,
            group.getY() + kHeadingHeight + (float) row * (kTileHeight + kGap),
            kTileWidth, kTileHeight };
}

int NodePalette::findSectionAt(Point<float> screenPosition) const {
    if (!visible) {
        return -1;
    }
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
