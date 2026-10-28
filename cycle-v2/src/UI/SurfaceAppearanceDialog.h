#pragma once

#include <JuceHeader.h>
#include <memory>

namespace CycleV2 {

class NodeWorkspace;

class SurfaceAppearanceDialog final : public juce::Component {
public:
    explicit SurfaceAppearanceDialog(NodeWorkspace& workspace);
    ~SurfaceAppearanceDialog() override;
    bool selectStyle(const juce::String& section, const juce::String& styleId);
    void paint(juce::Graphics& graphics) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    class Selector;
    void close();
    std::unique_ptr<Selector> time;
    std::unique_ptr<Selector> spectral;
    std::unique_ptr<Selector> waveform;
    juce::TextButton done { "Done" };
};

}
