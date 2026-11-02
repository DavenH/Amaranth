#include "UI/SignalProbeDetailPanel.h"

#include "UI/CanvasChromePalette.h"
#include "UiIconData.h"

namespace CycleV2 {

SignalProbeDetailPanel::SignalProbeDetailPanel(SignalProbeDetailState& stateToUse) :
        state(stateToUse)
    ,   modes({ { "Cyclogram", "time", "probeDetail.time", "Period-folded signal", {} },
                { "Spectrogram", "spectrum", "probeDetail.spectrum", "Harmonic spectrum over time", {} },
                { "Waveform", "waveform", "probeDetail.waveform", "Continuous time-domain signal", {} } })
    ,   zoomAttack("Zoom to attack", "Focus on the signal attack", "Zoom to attack", UiIconData::zoomAttack)
    ,   zoomFull("Full signal view", "Show the complete signal", "Full view", UiIconData::zoomFull) {
    setInterceptsMouseClicks(false, true);
    addChildComponent(waveform);
    addAndMakeVisible(modes);
    addChildComponent(zoomAttack);
    addChildComponent(zoomFull);
    waveform.setInterceptsMouseClicks(false, true);
    waveform.setPlotInterceptsMouseClicks(false);
    waveform.setZoomBarsVisible(true);
    waveform.onCameraChanged = [this](const ZoomRect& camera) {
        state.waveformViewport = camera;
        if (onCameraChanged) {
            onCameraChanged();
        }
    };
    waveform.setWaveformOverlay([this](juce::Graphics& graphics) {
        const auto camera = waveform.camera();
        const float split = (state.noteOffPosition() - camera.x) / camera.w;
        graphics.setColour(CanvasChromePalette::focus.withAlpha(0.5f));
        const auto plot = waveform.plotBounds().toFloat();
        graphics.drawVerticalLine(juce::roundToInt(split * plot.getWidth()), 0.f, plot.getHeight());
    });
    modes.onChange = [this](const juce::String& value) {
        if (onModeChanged) {
            onModeChanged(*presetPreviewViewForId(value));
        }
    };
    zoomAttack.setComponentID("probeDetail.zoomAttack");
    zoomFull.setComponentID("probeDetail.zoomFull");
    zoomAttack.onClick = [this] { waveform.zoomToAttack(); };
    zoomFull.onClick = [this] { waveform.zoomToFull(); };
}

void SignalProbeDetailPanel::sync(PresetPreviewView view, bool timeSignal) {
    modes.setVisible(timeSignal);
    modes.setSelectedValue(idForPresetPreviewView(view));
    const bool showWaveform = view == PresetPreviewView::Waveform && state.renderResult.waveform != nullptr;
    waveform.setVisible(showWaveform);
    zoomAttack.setVisible(showWaveform);
    zoomFull.setVisible(showWaveform);
    if (showWaveform) {
        waveform.setWaveform(state.renderResult.waveform);
        waveform.setCamera(state.waveformViewport);
    }
    resized();
}

void SignalProbeDetailPanel::resized() {
    modes.setBounds((getWidth() - 318) / 2, 12, 318, 26);
    const auto plot = NodePreviewRenderer::heatmapBounds(
            SignalProbeDetailView::plotBounds(getLocalBounds().toFloat()));
    waveform.setBounds(plot.toNearestInt());
    const int size = CanvasChromeMetrics::panelOverlayButtonSize;
    const int gap = CanvasChromeMetrics::panelOverlayButtonGap;
    const auto content = getLocalArea(&waveform, waveform.plotBounds());
    const int x = content.getRight() - 8 - 2 * size - gap;
    const int y = content.getY() + 8;
    zoomAttack.setBounds(x, y, size, size);
    zoomFull.setBounds(x + size + gap, y, size, size);
    zoomAttack.toFront(false);
    zoomFull.toFront(false);
}

juce::var SignalProbeDetailPanel::waveformDiagnostics() const {
    return waveform.isVisible() ? waveform.automationDiagnostics() : juce::var();
}

std::vector<std::pair<juce::String, juce::Rectangle<float>>> SignalProbeDetailPanel::pointerTargets() const {
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> result;
    if (modes.isVisible()) {
        const char* ids[] { "time", "spectrum", "waveform" };
        for (int index = 0; index < 3; ++index) {
            result.emplace_back("probeDetail." + juce::String(ids[index]),
                    modes.optionBounds(index).translated((float) modes.getX(), (float) modes.getY()));
        }
    }
    for (const auto* button : { &zoomAttack, &zoomFull }) {
        if (button->isVisible()) {
            result.emplace_back(button->getComponentID(), button->getBounds().toFloat());
        }
    }
    return result;
}

void SignalProbeDetailPanel::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
    if (waveform.isVisible()) {
        waveform.mouseWheelMove(event.getEventRelativeTo(&waveform), wheel);
    }
}

void SignalProbeDetailPanel::mouseMagnify(const juce::MouseEvent& event, float scaleFactor) {
    if (waveform.isVisible()) {
        waveform.mouseMagnify(event.getEventRelativeTo(&waveform), scaleFactor);
    }
}

}
