#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

enum class CanvasChromeControlState {
    Resting,
    Hovered,
    Selected,
    Focused
};

struct CanvasChromeControlColours {
    juce::Colour surface;
    juce::Colour border;
    juce::Colour text;
};

namespace CanvasChromePalette {

inline const juce::Colour canvasBackground { 0xff101318 };
inline const juce::Colour gridMajor { 0x2f5b6370 };
inline const juce::Colour gridMinor { 0x182f363f };
inline const juce::Colour insetBackground { 0xff11171d };
inline const juce::Colour surface { 0xff171d24 };
inline const juce::Colour dockSurface { 0xff18212a };
inline const juce::Colour raisedSurface { 0xff202833 };
inline const juce::Colour restingControlSurface { 0xff151b24 };
inline const juce::Colour hoveredControlSurface { 0xff1d2631 };
inline const juce::Colour border { 0xff3d4a58 };
inline const juce::Colour strongBorder { 0xff8290a2 };
inline const juce::Colour text { 0xffe2e8ef };
inline const juce::Colour mutedText { 0xff8793a1 };
inline const juce::Colour focus { 0xff79b8ff };
inline const juce::Colour navigationAccent { 0xff35d6d2 };

inline CanvasChromeControlColours control(CanvasChromeControlState state) {
    switch (state) {
        case CanvasChromeControlState::Hovered:
            return {
                    hoveredControlSurface,
                    strongBorder.withAlpha(0.76f),
                    text.withAlpha(0.96f)
            };
        case CanvasChromeControlState::Selected:
            return {
                    raisedSurface,
                    strongBorder.withAlpha(0.86f),
                    text.withAlpha(0.96f)
            };
        case CanvasChromeControlState::Focused:
            return {
                    restingControlSurface,
                    focus,
                    text.withAlpha(0.96f)
            };
        case CanvasChromeControlState::Resting:
            return {
                    restingControlSurface,
                    border.withAlpha(0.72f),
                    text.withAlpha(0.82f)
            };
    }

    return {};
}

}

}
