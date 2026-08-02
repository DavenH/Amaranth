#pragma once

#include <JuceHeader.h>

#include <optional>

#include "../Graph/NodeGraph.h"

namespace CycleV2 {

struct VoiceContextEdit {
    enum class Control {
        Domain,
        Octave,
        Pitch,
        VoiceLength,
        Portamento,
        Oversampling
    };

    Control control {};
    String value;
};

class VoiceContextCompactEditor {
public:
    static Rectangle<float> expandedContentBounds(Rectangle<float> panel);
    static Rectangle<float> nodeSelectorBounds(Rectangle<float> nodeBounds, float zoom);
    static Rectangle<float> octaveControlBounds(Rectangle<float> panel);
    static Rectangle<float> voiceLengthControlBounds(Rectangle<float> panel);
    static Rectangle<float> pitchControlBounds(Rectangle<float> panel);
    static Rectangle<float> oversamplingControlBounds(Rectangle<float> panel);
    static std::optional<VoiceContextEdit> sliderEditAt(
            VoiceContextEdit::Control control,
            Rectangle<float> panel,
            float positionX);
    static double voiceLengthAt(Rectangle<float> panel, float positionX);

    static String domain(const Node& node);
    static String domainLabel(const Node& node);
    static String nextDomain(const Node& node);
    static String summaryLabel(const Node& node, double voiceDurationSeconds);

    static void paintExpanded(
            Graphics& graphics,
            Rectangle<float> panel,
            const Node& node,
            double voiceDurationSeconds);
    static void paintNodeSelector(
            Graphics& graphics,
            Rectangle<float> nodeBounds,
            float zoom,
            const Node& node);
    static void paintNodeSummary(
            Graphics& graphics,
            Rectangle<float> nodeBounds,
            float zoom,
            const Node& node,
            double voiceDurationSeconds);
    static bool hitNodeSelector(
            Rectangle<float> nodeBounds,
            float zoom,
            Point<float> position);
    static std::optional<VoiceContextEdit> editAt(
            const Node& node,
            Rectangle<float> panel,
            Point<float> position);
};

}
