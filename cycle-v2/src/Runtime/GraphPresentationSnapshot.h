#pragma once

#include <array>
#include <memory>
#include <map>

#include "Graph/GraphCompiler.h"
#include "Runtime/GraphPreviewExecutor.h"
#include "Runtime/GraphRuntime.h"
#include "Runtime/SignalProbeWaveformCapture.h"

namespace CycleV2 {

class GraphPresentationFacts;

struct GraphPresentationSnapshot {
    uint64_t graphRevision {};
    int previewMidiNote { 48 };
    PresetPreviewView outputSpyView { PresetPreviewView::Spectrum };
    std::array<int, 128> previewControllerValues {};
    GraphCompileResult compileResult;
    RuntimeProcessTrace runtimeTrace;
    GraphPreviewResult previewResult;
    std::shared_ptr<SignalProbeWaveformCapture> waveformCaptures {
            std::make_shared<SignalProbeWaveformCapture>() };
    std::map<String, double> probeNoteOffTimes;
    std::shared_ptr<const GraphPresentationFacts> facts;
};

}
