#pragma once

#include <JuceHeader.h>

#include "UI/CanvasChromeMetrics.h"

namespace CycleV2 {

namespace EditorChromeLayoutDetail {

template <typename Value>
juce::Rectangle<Value> trailingSquare(
        juce::Rectangle<Value> header,
        Value size,
        Value rightInset) {
    return juce::Rectangle<Value>(size, size).withCentre({
            header.getRight() - rightInset - size / static_cast<Value>(2),
            header.getCentreY()
    });
}

template <typename Value>
juce::Rectangle<Value> titleBounds(
        juce::Rectangle<Value> header,
        Value horizontalInset,
        Value verticalInset,
        Value right) {
    const Value x = header.getX() + horizontalInset;
    return {
            x,
            header.getY() + verticalInset,
            juce::jmax(static_cast<Value>(0), right - x),
            header.getHeight() - verticalInset * static_cast<Value>(2)
    };
}

}

struct FullEditorHeaderLayout {
    juce::Rectangle<int> header;
    juce::Rectangle<int> title;
    juce::Rectangle<int> enabled;
    juce::Rectangle<int> close;
};

inline FullEditorHeaderLayout fullEditorHeaderLayout(
        juce::Rectangle<int> editor,
        bool showsEnabled) {
    FullEditorHeaderLayout layout;
    layout.header = editor.removeFromTop(CanvasChromeMetrics::fullEditorHeaderHeight);
    layout.close = EditorChromeLayoutDetail::trailingSquare(
            layout.header,
            CanvasChromeMetrics::fullEditorCloseButtonSize,
            CanvasChromeMetrics::fullEditorCloseButtonRightInset);

    if (showsEnabled) {
        layout.enabled = {
                layout.close.getX()
                        - CanvasChromeMetrics::fullEditorActionGap
                        - CanvasChromeMetrics::fullEditorEnabledWidth,
                layout.header.getCentreY()
                        - CanvasChromeMetrics::fullEditorEnabledHeight / 2,
                CanvasChromeMetrics::fullEditorEnabledWidth,
                CanvasChromeMetrics::fullEditorEnabledHeight
        };
    }

    const int firstActionX = showsEnabled
            ? layout.enabled.getX()
            : layout.close.getX();
    layout.title = EditorChromeLayoutDetail::titleBounds(
            layout.header,
            CanvasChromeMetrics::fullEditorHorizontalInset,
            CanvasChromeMetrics::fullEditorTitleVerticalInset,
            firstActionX - CanvasChromeMetrics::fullEditorActionGap);
    return layout;
}

struct EmbeddedEditorHeaderLayout {
    juce::Rectangle<float> header;
    juce::Rectangle<float> title;
    juce::Rectangle<float> enabled;
    juce::Rectangle<float> close;
};

inline EmbeddedEditorHeaderLayout embeddedEditorHeaderLayout(
        juce::Rectangle<float> editor,
        bool showsEnabled = false) {
    EmbeddedEditorHeaderLayout layout;
    layout.header = editor.removeFromTop(CanvasChromeMetrics::embeddedEditorHeaderHeight);
    layout.close = EditorChromeLayoutDetail::trailingSquare(
            layout.header,
            CanvasChromeMetrics::embeddedEditorCloseButtonSize,
            CanvasChromeMetrics::embeddedEditorCloseButtonRightInset);
    if (showsEnabled) {
        layout.enabled = EditorChromeLayoutDetail::trailingSquare(
                layout.header,
                static_cast<float>(CanvasChromeMetrics::fullEditorEnabledWidth),
                layout.header.getRight() - layout.close.getX()
                        + static_cast<float>(CanvasChromeMetrics::embeddedEditorActionGap));
    }
    const float firstActionX = showsEnabled
            ? layout.enabled.getX()
            : layout.close.getX();
    layout.title = EditorChromeLayoutDetail::titleBounds(
            layout.header,
            CanvasChromeMetrics::embeddedEditorHorizontalInset,
            CanvasChromeMetrics::embeddedEditorTitleVerticalInset,
            firstActionX - CanvasChromeMetrics::embeddedEditorActionGap);
    return layout;
}

}
