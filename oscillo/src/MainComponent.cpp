#include "MainComponent.h"

#include <Algo/Resampling.h>

MainComponent::MainComponent() : workBuffer(4096) {

    keyboardState.addListener(this);
    keyboard = std::make_unique<MidiKeyboardComponent>(keyboardState, MidiKeyboardComponent::horizontalKeyboard);
    keyboard->setKeyWidth(24.0f);
    keyboard->setAvailableRange(21, 108); // Full 88-key range
    keyboard->setOctaveForMiddleC(4);
    addAndMakeVisible(keyboard.get());

    historyImage = Image(Image::RGB, kHistoryFrames, kImageHeight, true);
    setSize(1280, 960);
    startTimer(50); // 20fps update rate
    processor.start();
}

MainComponent::~MainComponent() {
    stopTimer();
    processor.stop();
}

void MainComponent::paint(Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    drawHistoryImage(g);
}

void MainComponent::resized() {
    auto area = getLocalBounds();
    keyboard->setBounds(area.removeFromBottom(100));
    plotBounds = area.reduced(20);
}


void MainComponent::updateHistoryImage() {
    const std::vector<Buffer<float>>& periods = processor.getAudioPeriods();
    if (periods.empty()) return;

    // C4 = MIDI note 60, frequency ≈ 261.63 Hz
    const float targetPeriod = processor.getTargetPeriod();
    const float c4Period = 2 * (float) processor.getCurrentSampleRate() / 261.63f;

    // Calculate periods per column relative to C4
    const int periodsPerColumn = jmax(1, roundToInt(c4Period / targetPeriod));
    const int effectiveColumns = std::ceil(periods.size() / periodsPerColumn);

    // Shift history image left by effective columns
    auto oldImage = historyImage.createCopy();
    Graphics g(historyImage);
    g.drawImageAt(oldImage, -effectiveColumns, 0);

    g.setColour(Colours::black);
    g.fillRect(kHistoryFrames - effectiveColumns, 0, effectiveColumns, kImageHeight);

    Image::BitmapData pixelData(
        historyImage,
        jmax(0, kHistoryFrames - effectiveColumns), 0,
        effectiveColumns, kImageHeight,
        Image::BitmapData::writeOnly
    );

    // Process each column
    for (int col = 0; col < effectiveColumns; ++col) {
        // Calculate range of periods to combine for this column
        const int startPeriod = col * periodsPerColumn;
        const int endPeriod = std::min((col + 1) * periodsPerColumn, (int)periods.size());

        // Combine multiple periods if needed
        workBuffer.zero();
        int samplesInColumn = 0;

        for (int p = startPeriod; p < endPeriod; ++p) {
            const auto& period = periods[p];
            samplesInColumn = std::max(samplesInColumn, period.size());
            workBuffer.addProduct(period, 1.0f / (endPeriod - startPeriod));
        }

        Resampling::linResample(workBuffer.withSize(samplesInColumn), resampleBuffer);

        // Map to colors
        for (int y = 0; y < kImageHeight; ++y) {
            float value = resampleBuffer[y];
            auto color = Colour::fromHSV(0.5f + value * 0.5f, 0.8f, 0.8f, 1.0f);
            pixelData.setPixelColour(col, y, color);
        }
    }

    processor.resetPeriods();
}

void MainComponent::drawHistoryImage(Graphics& g) {
    if (plotBounds.isEmpty()) return;

    g.setImageResamplingQuality(Graphics::lowResamplingQuality);
    g.drawImage(historyImage, plotBounds.toFloat());

    g.setColour(Colours::white);
    g.drawRect(plotBounds);
}

void MainComponent::timerCallback() {
    updateHistoryImage();
    repaint();
}

void MainComponent::handleNoteOn(MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float /*velocity*/) {
    auto freq = MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    processor.setTargetFrequency(freq);
}

void MainComponent::handleNoteOff(MidiKeyboardState*, int /*midiChannel*/, int /*midiNoteNumber*/, float /*velocity*/) {
    // Optionally handle note off
}