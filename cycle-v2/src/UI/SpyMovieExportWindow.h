#pragma once

#include <JuceHeader.h>
#include <UI/AmaranthLookAndFeel.h>
#include "App/SpyMovieExport.h"

namespace CycleV2 {

class SpyMovieExportWindow final : public juce::DocumentWindow, private juce::Timer {
public:
    using Snapshot = std::function<std::pair<juce::String, PresetMidiSequence>()>;

    SpyMovieExportWindow(juce::Component& parent, Snapshot capture);
    ~SpyMovieExportWindow() override;
    void closeButtonPressed() override;
    void resized() override;

private:
    void timerCallback() override;
    void beginExport();
    void cancelExport();
    void chooseFile(bool executable);
    void setRunning(bool active);

    AmaranthLookAndFeel lookAndFeel {nullptr, false};
    Snapshot capture;
    juce::Component content;
    juce::ComboBox gradient;
    juce::ComboBox resolution;
    juce::Slider fps;
    juce::Slider bitrate;
    juce::Slider tail;
    juce::TextEditor destination;
    juce::TextEditor ffmpeg;
    juce::Label heading;
    juce::Label detail;
    juce::Label gradientLabel;
    juce::Label resolutionLabel;
    juce::Label fpsLabel;
    juce::Label bitrateLabel;
    juce::Label tailLabel;
    juce::Label outputLabel;
    juce::Label encoderLabel;
    juce::TextButton outputBrowse { "Browse" };
    juce::TextButton encoderBrowse { "Browse" };
    juce::TextButton start { "Export Movie" };
    juce::TextButton cancel { "Close" };
    juce::TextButton reveal { "Show Folder" };
    double progress {};
    juce::ProgressBar progressBar {progress};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ChildProcess worker;
    juce::File jobDirectory, outputDirectory;
    bool running {};
};

}
