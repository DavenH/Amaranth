#include "MainComponent.h"

#include <Algo/Resampling.h>

MainComponent::MainComponent() {
    keyboard = std::make_unique<PianoComponent>([this](float freq) {
        processor.setTargetFrequency(freq);
    });
    addAndMakeVisible(keyboard.get());

    historyImage = Image(Image::RGB, kHistoryFrames, kImageHeight, true);
    setSize(1280, 960);

    startTimer(100); // 20fps update rate
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
    keyboard->setBounds(area.removeFromBottom(150));
    plotBounds = area.reduced(20);
}

void MainComponent::updateHistoryImage() {
    const std::vector<Buffer<float>>& periods = processor.getAudioPeriods();

    // Shift history image left
    auto oldImage = historyImage.createCopy();
    Graphics g(historyImage);
    g.drawImageAt(oldImage, -periods.size(), 0);

    g.setColour(Colours::black);
    g.fillRect(kHistoryFrames - periods.size(), 0, 1, kImageHeight);

    Image::BitmapData pixelData(
        historyImage,
        jmax(0, kHistoryFrames - (int) periods.size()), 0,
        periods.size(), kImageHeight,
        Image::BitmapData::writeOnly
    );

    // Map audio samples to colors
    int j = 0;
    for (const auto& columnBuff: periods) {
        Resampling::linResample(columnBuff, resampleBuffer);

        for (int i = 0; i < kImageHeight; ++i) {
            float value = resampleBuffer[i];
            auto color  = Colour::fromHSV(0.5f + value * 0.5f, 0.8f, 0.8f, 1.0f);
            pixelData.setPixelColour(j, i, color);
        }
        ++j;
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
