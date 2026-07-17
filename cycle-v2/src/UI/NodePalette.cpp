#include "NodePalette.h"

#include <array>
#include <iterator>

namespace CycleV2 {

namespace {

constexpr float kWidth = 72.f;
constexpr float kX = 18.f;
constexpr float kY = 74.f;
constexpr float kRowHeight = 78.f;
constexpr float kPulloutWidth = 144.f;
constexpr float kPulloutRowHeight = 52.f;

const NodePalette::Entry kContextEntries[] = {
        { NodeKind::VoiceContext, "Voice Context" }
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
        { NodeKind::Envelope, "Envelope" },
        { NodeKind::GuideCurve, "Guide" }
};

const NodePalette::Entry kFxEntries[] = {
        { NodeKind::ImpulseResponse, "IR" },
        { NodeKind::Waveshaper, "Waveshaper" },
        { NodeKind::Reverb, "Reverb" },
        { NodeKind::Delay, "Delay" }
};

const NodePalette::Entry kChannelEntries[] = {
        { NodeKind::Spy, "Spy" },
        { NodeKind::StereoSplit, "Split" },
        { NodeKind::StereoJoin, "Join" },
        { NodeKind::Output, "Output" }
};

const NodePalette::Section kSections[] = {
        {
                "Context",
                "Context",
                PortDomain::VoiceControlSignal,
                kContextEntries,
                (int) std::size(kContextEntries)
        },
        {
                "Transform",
                "Transform",
                PortDomain::SpectralMagnitudeSignal,
                kTransformEntries,
                (int) std::size(kTransformEntries)
        },
        {
                "Math",
                "Math",
                PortDomain::ControlSignal,
                kMathEntries,
                (int) std::size(kMathEntries)
        },
        {
                "Source",
                "Source",
                PortDomain::TimeSignal,
                kSourceEntries,
                (int) std::size(kSourceEntries)
        },
        {
                "Control",
                "Control",
                PortDomain::EnvelopeSignal,
                kControlEntries,
                (int) std::size(kControlEntries)
        },
        {
                "FX",
                "FX",
                PortDomain::SpectralPhaseSignal,
                kFxEntries,
                (int) std::size(kFxEntries)
        },
        {
                "Channel",
                "Channel",
                PortDomain::TimeSignal,
                kChannelEntries,
                (int) std::size(kChannelEntries)
        }
};

}

int NodePalette::sectionCount() const {
    return (int) std::size(kSections);
}

const NodePalette::Section& NodePalette::section(int sectionIndex) const {
    jassert(isPositiveAndBelow(sectionIndex, sectionCount()));
    return kSections[(size_t) sectionIndex];
}

Rectangle<float> NodePalette::railBounds() const {
    return { kX, kY, kWidth, 12.f + (float) sectionCount() * kRowHeight };
}

Rectangle<float> NodePalette::groupBounds(int sectionIndex) const {
    return {
            kX + 7.f,
            kY + 7.f + (float) sectionIndex * kRowHeight,
            kWidth - 14.f,
            kRowHeight - 9.f
    };
}

Rectangle<float> NodePalette::pulloutBounds(int sectionIndex) const {
    const auto group = groupBounds(sectionIndex);
    const float height = (float) section(sectionIndex).entryCount * kPulloutRowHeight;

    return {
            railBounds().getRight() + 10.f,
            group.getY(),
            kPulloutWidth,
            height
    };
}

Rectangle<float> NodePalette::entryBounds(int sectionIndex, int entryIndex) const {
    const auto panel = pulloutBounds(sectionIndex);

    return {
            panel.getX(),
            panel.getY() + (float) entryIndex * kPulloutRowHeight,
            panel.getWidth(),
            kPulloutRowHeight - 7.f
    };
}

Rectangle<float> NodePalette::hoverBounds(int sectionIndex) const {
    const auto group = groupBounds(sectionIndex);
    const auto pullout = pulloutBounds(sectionIndex);
    const float top = jmin(group.getY(), pullout.getY()) - 22.f;
    const float bottom = jmax(group.getBottom(), pullout.getBottom()) + 22.f;

    return {
            group.getX() - 6.f,
            top,
            pullout.getRight() - group.getX() + 12.f,
            bottom - top
    };
}

int NodePalette::groupIndexAt(Point<float> screenPosition) const {
    for (int sectionIndex = 0; sectionIndex < sectionCount(); ++sectionIndex) {
        if (groupBounds(sectionIndex).expanded(3.f).contains(screenPosition)) {
            return sectionIndex;
        }
    }

    return -1;
}

int NodePalette::findSectionAt(Point<float> screenPosition) const {
    const int groupIndex = groupIndexAt(screenPosition);

    if (groupIndex >= 0) {
        return groupIndex;
    }

    if (activeSectionIndex >= 0 && hoverBounds(activeSectionIndex).contains(screenPosition)) {
        return activeSectionIndex;
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

    if (activeSectionIndex >= 0 && hoverBounds(activeSectionIndex).contains(screenPosition)) {
        const int groupIndex = groupIndexAt(screenPosition);

        if (groupIndex >= 0
                && groupIndex != activeSectionIndex
                && groupBounds(groupIndex).contains(screenPosition)) {
            activeSectionIndex = groupIndex;
        }
    } else {
        activeSectionIndex = groupIndexAt(screenPosition);
    }

    return activeSectionIndex != previousSection;
}

bool NodePalette::close() {
    if (activeSectionIndex < 0) {
        return false;
    }

    activeSectionIndex = -1;
    return true;
}

}
