#include <UI/Panels/TimeSurfaceStyles.h>

#include "UI/SpyMovieExportWindow.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {
using namespace juce;

SpyMovieExportWindow::SpyMovieExportWindow(Component& parent, Snapshot snapshot) :
        DocumentWindow("Export Movie", CanvasChromePalette::dockSurface, closeButton)
    ,   capture(std::move(snapshot)) {
    lookAndFeel.setColour(Slider::textBoxBackgroundColourId, lookAndFeel.findColour(TextEditor::backgroundColourId));
    lookAndFeel.setColour(Slider::textBoxTextColourId, lookAndFeel.findColour(TextEditor::textColourId));
    lookAndFeel.setColour(Slider::textBoxOutlineColourId, lookAndFeel.findColour(TextEditor::outlineColourId));
    lookAndFeel.setColour(ProgressBar::backgroundColourId, CanvasChromePalette::restingControlSurface);
    lookAndFeel.setColour(ProgressBar::foregroundColourId, CanvasChromePalette::focus);
    setLookAndFeel(&lookAndFeel);
    setColour(ResizableWindow::backgroundColourId, CanvasChromePalette::dockSurface);
    setName("SpyMovieExport");
    setUsingNativeTitleBar(true);
    setContentNonOwned(&content, false);
    const auto label = [&](Label& target, const String& text) {
        content.addAndMakeVisible(target);
        target.setText(text, dontSendNotification);
        target.setColour(Label::textColourId, CanvasChromePalette::text);
    };
    label(heading, "Export an automated Cyclogram movie");
    heading.setFont(FontOptions(18.f, Font::bold));
    label(detail, "Current preset + linked phrase | synth audio | playback ripples");
    detail.setFont(FontOptions(12.f));
    label(gradientLabel, "Gradient");
    label(resolutionLabel, "Resolution");
    label(fpsLabel, "Frame rate");
    label(bitrateLabel, "Video bitrate (Mbps)");
    label(tailLabel, "Release tail (seconds)");
    label(outputLabel, "New output folder");
    label(encoderLabel, "ffmpeg executable");
    for (Component* component : std::vector<Component*> {&gradient, &resolution, &fps, &bitrate, &tail,
            &destination, &ffmpeg, &outputBrowse, &encoderBrowse, &start, &cancel, &reveal, &progressBar}) {
        content.addAndMakeVisible(component);
    }
    gradient.addItem("Saved preset style", 1);
    int index = 2;
    for (const auto& entry : TimeSurfaceStyles::entries) {
        gradient.addItem(String(entry.label) + " (" + entry.id + ")", index++);
    }
    gradient.setSelectedId(1);
    resolution.addItemList({"1280 x 720", "1920 x 1080", "2560 x 1440", "3840 x 2160"}, 1);
    resolution.setSelectedId(2);
    const auto number = [](Slider& control, double minimum, double maximum, double step, double value) {
        control.setSliderStyle(Slider::IncDecButtons);
        control.setTextBoxStyle(Slider::TextBoxLeft, false, 74, 26);
        control.setRange(minimum, maximum, step);
        control.setValue(value, dontSendNotification);
    };
    number(fps, 1, 120, 1, 30);
    number(bitrate, 0.1, 200, 0.1, 8);
    number(tail, 0, 30, 0.1, 1);
    destination.setText(File::getSpecialLocation(File::userDocumentsDirectory)
            .getChildFile("Cycle Movie " + Time::getCurrentTime().formatted("%Y-%m-%d %H-%M-%S")).getFullPathName());
    ffmpeg.setText(SpyMovieExportSettings::findFFmpeg().getFullPathName());
    destination.setName("SpyMovieExport.Output");
    ffmpeg.setName("SpyMovieExport.FFmpeg");
    start.setName("SpyMovieExport.Start");
    cancel.setName("SpyMovieExport.Cancel");
    outputBrowse.onClick = [this] { chooseFile(false); };
    encoderBrowse.onClick = [this] { chooseFile(true); };
    start.onClick = [this] { beginExport(); };
    cancel.onClick = [this] { closeButtonPressed(); };
    reveal.onClick = [this] { outputDirectory.revealToUser(); };
    reveal.setEnabled(false);
    setSize(620, 530);
    centreAroundComponent(&parent, getWidth(), getHeight());
    setVisible(true);
    toFront(true);
}

SpyMovieExportWindow::~SpyMovieExportWindow() {
    cancelExport();
    clearContentComponent();
    setLookAndFeel(nullptr);
}

void SpyMovieExportWindow::resized() {
    DocumentWindow::resized();
    if (getContentComponent() != &content) {
        return;
    }
    const int width = content.getWidth();
    heading.setBounds(24, 18, width - 48, 28);
    detail.setBounds(24, 48, width - 48, 40);
    Label* labels[] {&gradientLabel, &resolutionLabel, &fpsLabel, &bitrateLabel, &tailLabel};
    Component* controls[] {&gradient, &resolution, &fps, &bitrate, &tail};
    for (int index = 0; index < 5; ++index) {
        labels[index]->setBounds(24, 98 + index * 38, 190, 28);
        controls[index]->setBounds(222, 98 + index * 38, index < 2 ? width - 246 : 154, 28);
    }
    outputLabel.setBounds(24, 294, 180, 24);
    destination.setBounds(24, 320, width - 130, 27);
    outputBrowse.setBounds(width - 94, 320, 70, 27);
    encoderLabel.setBounds(24, 354, 180, 24);
    ffmpeg.setBounds(24, 380, width - 130, 27);
    encoderBrowse.setBounds(width - 94, 380, 70, 27);
    progressBar.setBounds(24, 418, width - 48, 18);
    cancel.setBounds(24, 452, 80, 28);
    reveal.setBounds(116, 452, 116, 28);
    start.setBounds(width - 166, 452, 142, 28);
}

void SpyMovieExportWindow::chooseFile(bool executable) {
    chooser = std::make_unique<FileChooser>(executable ? "Choose ffmpeg" : "Choose a new output folder",
            executable ? File::getCurrentWorkingDirectory().getChildFile(ffmpeg.getText())
                    : File::getSpecialLocation(File::userDocumentsDirectory).getChildFile(destination.getText()));
    const Component::SafePointer<SpyMovieExportWindow> safe(this);
    chooser->launchAsync(executable ? FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles
            : FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
            [safe, executable](const FileChooser& selected) {
                if (safe != nullptr && selected.getResult() != File()) {
                    const auto result = executable ? selected.getResult() : selected.getResult().getChildFile(
                            File::getSpecialLocation(File::userDocumentsDirectory)
                                    .getChildFile(safe->destination.getText()).getFileName());
                    (executable ? safe->ffmpeg : safe->destination).setText(result.getFullPathName());
                }
            });
}

void SpyMovieExportWindow::setRunning(bool active) {
    running = active;
    for (Component* component : std::vector<Component*> {&gradient, &resolution, &fps, &bitrate, &tail,
            &destination, &ffmpeg, &outputBrowse, &encoderBrowse, &start}) {
        component->setEnabled(!active);
    }
    cancel.setButtonText(active ? "Cancel" : "Close");
}

void SpyMovieExportWindow::beginExport() {
    const auto [snapshot, phrase] = capture();
    SpyMovieExportSettings settings;
    settings.outputDirectory = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile(destination.getText());
    settings.ffmpeg = File::getCurrentWorkingDirectory().getChildFile(ffmpeg.getText());
    const int widths[] {1280, 1920, 2560, 3840};
    const int heights[] {720, 1080, 1440, 2160};
    const int selected = jlimit(0, 3, resolution.getSelectedId() - 1);
    settings.width = widths[selected];
    settings.height = heights[selected];
    settings.fps = (int) fps.getValue();
    settings.bitrateMbps = bitrate.getValue();
    settings.tailSeconds = tail.getValue();
    if (gradient.getSelectedId() > 1) {
        settings.gradient = TimeSurfaceStyles::entries[(size_t) gradient.getSelectedId() - 2].id;
    }
    if (const auto valid = settings.validate(phrase); valid.failed()) {
        detail.setText(valid.getErrorMessage(), dontSendNotification);
        return;
    }
    jobDirectory = File::getSpecialLocation(File::tempDirectory).getChildFile("cycle-movie-" + Uuid().toString());
    const auto job = jobDirectory.getChildFile("job.json");
    if (!jobDirectory.createDirectory() || !job.replaceWithText(JSON::toString(settings.toJSON(snapshot, phrase)))) {
        detail.setText("Could not write the export job.", dontSendNotification);
        return;
    }
    outputDirectory = settings.outputDirectory;
    progress = -1;
    reveal.setEnabled(false);
    if (!worker.start(StringArray {File::getSpecialLocation(File::currentExecutableFile).getFullPathName(),
                "--export-movie-job", job.getFullPathName()}, 0)) {
        detail.setText("Could not launch the export worker.", dontSendNotification);
        return;
    }
    setRunning(true);
    detail.setText("Preparing export. You can continue editing.", dontSendNotification);
    startTimerHz(5);
}

void SpyMovieExportWindow::cancelExport() {
    if (running) {
        jobDirectory.getChildFile("cancel").replaceWithText("cancel");
        detail.setText("Cancelling export...", dontSendNotification);
    }
}

void SpyMovieExportWindow::closeButtonPressed() {
    if (running) {
        cancelExport();
    } else {
        setVisible(false);
    }
}

void SpyMovieExportWindow::timerCallback() {
    const auto status = JSON::parse(jobDirectory.getChildFile("status.json"));
    if (status.isObject()) {
        detail.setText(status["message"].toString(), dontSendNotification);
        const int count = status["totalFrames"];
        progress = status["status"].toString() == "frames" && count > 0
                ? (double) status["completedFrames"] / count : -1.0;
    }
    if (!worker.isRunning()) {
        stopTimer();
        const bool complete = status["status"].toString() == "complete" && worker.getExitCode() == 0;
        progress = complete ? 1.0 : 0.0;
        const auto result = status["status"].toString();
        if (result != "complete" && result != "cancelled" && result != "failed") {
            detail.setText("The export worker stopped unexpectedly. Completed files were retained.", dontSendNotification);
        }
        setRunning(false);
        reveal.setEnabled(outputDirectory.isDirectory());
    }
}

}
