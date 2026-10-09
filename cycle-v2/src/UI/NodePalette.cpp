#include <iterator>

#include "UI/NodePalette.h"
#include "Graph/NodeDefinition.h"

namespace CycleV2 {

namespace {

constexpr float kY = 64.f;
constexpr float kGap = 6.f;
constexpr float kHeadingHeight = 20.f;
constexpr float kGroupGap = 24.f;
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
        { "Context", kContextEntries, (int) std::size(kContextEntries), 0xffb7a16f },
        { "Transform", kTransformEntries, (int) std::size(kTransformEntries), 0xff778fad },
        { "Math", kMathEntries, (int) std::size(kMathEntries), 0xff9b86ae },
        { "Source", kSourceEntries, (int) std::size(kSourceEntries), 0xff7b9e8a },
        { "Control", kControlEntries, (int) std::size(kControlEntries), 0xffb1818c },
        { "FX", kFxEntries, (int) std::size(kFxEntries), 0xffb38d6f }
};

}

float NodePalette::tileWidth() const {
    return 0.8f * (float) jmax(40, (int) ((workspace.getWidth() - 28.f - 2.f * kGap) / kColumns));
}

float NodePalette::tileHeight() const {
    int totalRows = 0;
    for (int index = 0; index < sectionCount(); ++index) {
        totalRows += (section(index).entryCount + kColumns - 1) / kColumns;
    }
    const float fixedHeight = kY + 16.f + sectionCount() * kHeadingHeight
            + (sectionCount() - 1) * kGroupGap + (totalRows - sectionCount()) * kGap;
    return 0.8f * (float) jlimit(52, 78, (int) ((workspace.getHeight() - fixedHeight) / totalRows));
}

float NodePalette::groupHeight(int sectionIndex) const {
    const int rows = (section(sectionIndex).entryCount + kColumns - 1) / kColumns;
    return kHeadingHeight + (float) rows * (tileHeight() + kGap) - kGap;
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
    const float width = kColumns * (tileWidth() + kGap) - kGap;
    return { first.getX() - 10.f, first.getY(), width + 10.f,
            groupBounds(sectionCount() - 1).getBottom() - first.getY() };
}

Rectangle<float> NodePalette::groupBounds(int sectionIndex) const {
    float y = workspace.getY() + kY;
    for (int index = 0; index < sectionIndex; ++index) {
        y += groupHeight(index) + kGroupGap;
    }

    const float width = kColumns * (tileWidth() + kGap) - kGap;
    const float x = workspace.getCentreX() - width * 0.5f;
    return { x, y, width,
            groupHeight(sectionIndex) };
}

Rectangle<float> NodePalette::entryBounds(int sectionIndex, int entryIndex) const {
    const auto group = groupBounds(sectionIndex);
    const int row = entryIndex / kColumns;
    const int column = entryIndex % kColumns;
    return { group.getX() + (float) column * (tileWidth() + kGap),
            group.getY() + kHeadingHeight + (float) row * (tileHeight() + kGap),
            tileWidth(), tileHeight() };
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
