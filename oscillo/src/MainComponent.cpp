#include "MainComponent.h"

#include <Algo/Resampling.h>

MainComponent::MainComponent()
    : workBuffer(4096),
    viridis(24, GradientColourMap::Palette::Viridis),
    inferno(24, GradientColourMap::Palette::Inferno)
{
    keyboardState.addListener(this);
    keyboard = std::make_unique<MidiKeyboardComponent>(keyboardState, MidiKeyboardComponent::horizontalKeyboard);
    keyboard->setKeyWidth(24.0f);
    keyboard->setAvailableRange(21, 108);
    keyboard->setOctaveForMiddleC(4);
    addAndMakeVisible(keyboard.get());

    transform.allocate(kImageHeight, true);
    cyclogram   = Image(Image::RGB, kHistoryFrames, kImageHeight, true);
    spectrogram = Image(Image::RGB, kHistoryFrames, kImageHeight / 8, true);
    setSize(1280, 960);
    startTimer(50);
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
    const std::vector<Buffer<float> >& periods = processor.getAudioPeriods();
    if (periods.empty()) return;

    // C4 = MIDI note 60, frequency ≈ 261.63 Hz
    const float targetPeriod = processor.getTargetPeriod();
    const float c4Period     = 2 * (float) processor.getCurrentSampleRate() / 261.63f;

    // Calculate periods per column relative to C4
    const int periodsPerColumn = jmax(1, roundToInt(c4Period / targetPeriod));
    const int effectiveColumns = std::ceil(periods.size() / periodsPerColumn);

    // Shift history image left by effective columns
    auto oldCyclogram = cyclogram.createCopy();
    Graphics g(cyclogram);
    g.drawImageAt(oldCyclogram, -effectiveColumns, 0);

    auto oldSpectrogram = spectrogram.createCopy();
    Graphics g2(spectrogram);
    g2.drawImageAt(oldSpectrogram, -effectiveColumns, 0);

    g.setColour(Colours::black);
    g.fillRect(kHistoryFrames - effectiveColumns, 0, effectiveColumns, kImageHeight);
    g2.setColour(Colours::black);
    g2.fillRect(kHistoryFrames - effectiveColumns, 0, effectiveColumns, kImageHeight / 2);

    Image::BitmapData pixelData(
        cyclogram,
        jmax(0, kHistoryFrames - effectiveColumns), 0,
        effectiveColumns, kImageHeight,
        Image::BitmapData::writeOnly
    );

    Image::BitmapData spectData(
        spectrogram,
        jmax(0, kHistoryFrames - effectiveColumns), 0,
        effectiveColumns, kImageHeight / 8,
        Image::BitmapData::writeOnly
    );

    // Process each column
    for (int col = 0; col < effectiveColumns; ++col) {
        // Calculate range of periods to combine for this column
        const int startPeriod = col * periodsPerColumn;
        const int endPeriod   = std::min((col + 1) * periodsPerColumn, (int) periods.size());

        // Combine multiple periods if needed
        workBuffer.zero();
        int samplesInColumn = 0;

        for (int p = startPeriod; p < endPeriod; ++p) {
            const auto& period = periods[p];
            samplesInColumn    = std::max(samplesInColumn, period.size());
            workBuffer.addProduct(period, 1.0f / (endPeriod - startPeriod));
        }

        Resampling::linResample(workBuffer.withSize(samplesInColumn), resampleBuffer);
        resampleBuffer.threshLT(-1.f).threshGT(1.0f);

        transform.forward(resampleBuffer);
        Buffer<float> magnitudes = transform
            .getMagnitudes().section(0, kImageHeight / 8)
            .add(1).ln().mul(5).tanh();

        // Map to colors
        for (int y = 0; y < kImageHeight; ++y) {
            float value = resampleBuffer[y];
            auto color  = viridis.getColour(static_cast<int>((value * 0.5f + 0.5f) * (kNumColours - 0.01)));
            pixelData.setPixelColour(col, y, color);
        }

        for (int y = 0; y < magnitudes.size(); ++y) {
            float value = magnitudes[y];
            auto color  = inferno.getColour(static_cast<int>(value * (kNumColours - 0.01)));
            spectData.setPixelColour(col, kImageHeight / 8 - 1 - y, color);
        }

    }

    processor.resetPeriods();
}

void MainComponent::drawHistoryImage(Graphics& g) {
    if (plotBounds.isEmpty()) return;

    Rectangle<int> local = plotBounds;
    Rectangle<int> left  = local.removeFromLeft((plotBounds.getWidth() - 20) / 2);
    Rectangle<int> right = local.removeFromRight((plotBounds.getWidth() - 20) / 2);

    g.setImageResamplingQuality(Graphics::lowResamplingQuality);
    g.drawImage(cyclogram, right.toFloat());

    g.setImageResamplingQuality(Graphics::lowResamplingQuality);
    g.drawImage(spectrogram, left.toFloat());

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
