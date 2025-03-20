#include "MainComponent.h"

#include <Algo/Resampling.h>

MainComponent::MainComponent()
    : workBuffer(4096),
    viridis(kNumColours, GradientColourMap::Palette::Viridis),
    inferno(kNumColours, GradientColourMap::Palette::Inferno),
    bipolar(kNumColours, GradientColourMap::Palette::Bipolar) // Add this line
{
    keyboardState.addListener(this);
    keyboard = std::make_unique<MidiKeyboardComponent>(keyboardState, MidiKeyboardComponent::horizontalKeyboard);
    keyboard->setKeyWidth(24.0f);
    keyboard->setAvailableRange(21, 108);
    keyboard->setOctaveForMiddleC(4);
    addAndMakeVisible(keyboard.get());

    temperamentControls = std::make_unique<TemperamentControls>();
    addAndMakeVisible(temperamentControls.get());
    temperamentControls->onTemperamentChanged = [this] {
        updateCurrentNote();
    };
    temperamentControls->onCentsOffsetChanged = [this] {
        updateCurrentNote();
    };

    transform.allocate(kImageHeight, Transform::DivFwdByN, true);
    cyclogram   = Image(Image::RGB, kHistoryFrames, kImageHeight, true);
    spectrogram = Image(Image::RGB, kHistoryFrames, kImageHeight / 8, true);
    phasigram   = Image(Image::RGB, kHistoryFrames, kNumPhasePartials, true);
    phaseVelocityBar = Image(Image::RGB, 1, kNumPhasePartials, true);

    phaseVelocity.zero();
    prevPhases.zero();
    phaseDiff.zero();
    avgMagnitudes.zero();

    startTimer(100);

    DBG(String::formatted("Main component constructor - Thread ID: %d", Thread::getCurrentThreadId()));
    DBG(String("Is message thread? ") + (MessageManager::getInstance()->isThisTheMessageThread() ? "yes" : "no"));

    processor.start();
    updateCurrentNote();
}

MainComponent::~MainComponent() {
    stopTimer();
    processor.stop();
}

void MainComponent::paint(Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    drawHistoryImage(g);
}

void MainComponent::drawPhaseVelocityBarChart(Graphics& g, const Rectangle<int>& area) {
    const int numHarmonics = phaseVelocity.size();
    if (numHarmonics == 0) return;

    // Draw background
    g.setColour(Colours::black);
    g.fillRect(area);

    // Draw axes
    g.setColour(Colours::grey);

    // Vertical center line (zero velocity)
    const int centerX = area.getX() + area.getWidth() / 2;
    g.drawVerticalLine(centerX, area.getY(), area.getBottom());

    // Draw grid lines for velocity
    g.setColour(Colours::darkgrey.withAlpha(0.5f));
    // Quarter divisions
    g.drawVerticalLine(area.getX() + area.getWidth() / 4, area.getY(), area.getBottom());
    g.drawVerticalLine(area.getX() + area.getWidth() * 3 / 4, area.getY(), area.getBottom());

    // Calculate bar height
    const float barHeight = (float)area.getHeight() / numHarmonics;

    for (int i = 0; i < numHarmonics; ++i) {
        const float velocity = phaseVelocity[i];
        const float normalizedVelocity = jlimit(-1.0f, 1.0f, velocity / kBarChartMaxVelocity);

        // Map the harmonic to y position (reversed to match phasigram)
        const int y = area.getBottom() - (int)((i + 1) * barHeight);

        // Calculate bar width and position
        int barWidth, barX;
        if (normalizedVelocity >= 0) {
            barWidth = (int)(normalizedVelocity * area.getWidth() / 2);
            barX = centerX;
        } else {
            barWidth = (int)(-normalizedVelocity * area.getWidth() / 2);
            barX = centerX - barWidth;
        }

        // Choose color based on velocity direction (blue for negative, red for positive)
        Colour barColor;
        if (normalizedVelocity >= 0) {
            barColor = Colours::red
            .withMultipliedSaturation(normalizedVelocity)
            .withBrightness(jmin(1.f, avgMagnitudes[i]));
        } else {
            barColor = Colours::blue
            .withMultipliedSaturation(-normalizedVelocity)
            .withBrightness(jmin(1.f, avgMagnitudes[i]));
        }

        // Draw the bar
        g.setColour(barColor);
        g.fillRect(barX, y, barWidth, (int)barHeight - 1);

        // Draw harmonic label every 4 harmonics
        if (i % 4 == 0) {
            g.setColour(Colours::white);
            g.setFont(10.0f);
            g.drawText(String(i + 1), area.getX() - 25, y, 20, (int)barHeight, Justification::centredRight);
        }
    }

    // Draw title
    g.setColour(Colours::white);
    g.setFont(14.0f);
    g.drawText("Phase Velocity", area.getX(), area.getY() - 20, area.getWidth(), 20, Justification::centred);
}

void MainComponent::resized() {
    auto area = getLocalBounds();
    std::cout << "Resized " << area.getWidth() << " " << area.getHeight() << std::endl;
    keyboard->setBounds(area.removeFromBottom(100));
    auto row = area.removeFromBottom(100);
    temperamentControls->setBounds(row.reduced(10));
    plotBounds = area.reduced(10);
}

void MainComponent::updateHistoryImage() {
    const std::vector<Buffer<float>>& periods = processor.getAudioPeriods();
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

    // auto oldSpectrogram = spectrogram.createCopy();
    // Graphics g2(spectrogram);
    // g2.drawImageAt(oldSpectrogram, -effectiveColumns, 0);

    auto oldPhasigram = phasigram.createCopy();
    Graphics g3(phasigram);
    g3.drawImageAt(oldPhasigram, -effectiveColumns, 0);

    g.setColour(Colours::black);
    g.fillRect(kHistoryFrames - effectiveColumns, 0, effectiveColumns, cyclogram.getHeight());
    // g2.setColour(Colours::black);
    // g2.fillRect(kHistoryFrames - effectiveColumns, 0, effectiveColumns, spectrogram.getHeight());
    g3.setColour(Colours::black);
    g3.fillRect(kHistoryFrames - effectiveColumns, 0, effectiveColumns, phasigram.getHeight());

    Image::BitmapData pixelData(
        cyclogram,
        jmax(0, kHistoryFrames - effectiveColumns), 0,
        effectiveColumns, cyclogram.getHeight(),
        Image::BitmapData::writeOnly
    );

    // Image::BitmapData spectData(
    //     spectrogram,
    //     jmax(0, kHistoryFrames - effectiveColumns), 0,
    //     effectiveColumns, spectrogram.getHeight(),
    //     Image::BitmapData::writeOnly
    // );

    Image::BitmapData phaseData(
        phasigram,
        jmax(0, kHistoryFrames - effectiveColumns), 0,
        effectiveColumns, phasigram.getHeight(),
        Image::BitmapData::writeOnly
    );

    // Image::BitmapData phaseVelocityData(
    //     phaseVelocityBar,
    //     0, 0,
    //     1, phaseVelocityBar.getHeight(),
    //     Image::BitmapData::writeOnly
    // );

    // const float smoothingK = powf(kPhaseSmoothing, 1.0 / (float) effectiveColumns);

    avgMagnitudes.zero();

    // Process each column
    for (int col = 0; col < effectiveColumns; ++col) {
        // Calculate range of periods to combine for this column
        const int startPeriod = col * periodsPerColumn;
        const int endPeriod   = std::min((col + 1) * periodsPerColumn, static_cast<int>(periods.size()));

        // Combine multiple periods if needed
        workBuffer.zero();
        int samplesInColumn = 0;

        for (int p = startPeriod; p < endPeriod; ++p) {
            const auto& period = periods[p];
            samplesInColumn = std::max(samplesInColumn, period.size());
            workBuffer.addProduct(period, 1.0f / (endPeriod - startPeriod));
        }

        Resampling::linResample(workBuffer.withSize(samplesInColumn), resampleBuffer);
        resampleBuffer.clip(-1.f, 1.f);

        transform.forward(resampleBuffer);
        Buffer<float> magnitudes = transform
            .getMagnitudes().section(0, spectrogram.getHeight())
            .mul(30).add(1).ln();

        Buffer<float> phases = transform.getPhases()
            .section(0, phasigram.getHeight())
            .add(M_PI).mul(0.5 / M_PI);

        VecOps::addProd(
            magnitudes.section(0, kNumPhasePartials).get(),
            1.f / (effectiveColumns),
            avgMagnitudes.get(),
            kNumPhasePartials
        );

        VecOps::sub(prevPhases, phases, phaseDiff);

        for (float &diff : phaseDiff) {
            while (diff > 0.5f) diff -= 1.0f;
            while (diff < -0.5f) diff += 1.0f;
        }
        phaseVelocity *= (1.f - kPhaseSmoothing);
        phaseVelocity += phaseDiff.mul(2 * kPhaseSmoothing);
        phaseVelocity.clip(-0.5f, 0.5f);

        phases.copyTo(prevPhases);

        // Map to colors
        for (int y = 0; y < kImageHeight; ++y) {
            float value = resampleBuffer[y];
            auto color  = inferno.getColour(static_cast<int>((value * 0.5f + 0.5f) * (kNumColours - 0.01)));
            pixelData.setPixelColour(col, y, color);
        }

        // scale it
        magnitudes.mul(3).tanh();

        for (int y = 0; y < phases.size(); ++y) {
            float value = phases[y];
            auto invY = phasigram.getHeight() - 1 - y;

            auto color  = viridis
                .getColour(static_cast<int>(value * (kNumColours - 0.01)))
                .withBrightness(magnitudes[y]);
            phaseData.setPixelColour(col, invY, color);
        }
    }

    processor.resetPeriods();
}

void MainComponent::drawHistoryImage(Graphics& g) {
    if (plotBounds.isEmpty()) return;

    Rectangle<int> local = plotBounds;

    // Left quarter for phasigram
    Rectangle<int> phasigram_area = local.removeFromLeft(local.getWidth() / 4);

    // Second quarter for phase velocity bar chart
    Rectangle<int> phaseVelArea = local.removeFromLeft(local.getWidth() / 3);
    phaseVelArea.reduce(10, 10); // Add margins

    // Right half for cyclogram
    const Rectangle<int> right = local;

    // Draw phasigram
    g.setImageResamplingQuality(Graphics::lowResamplingQuality);
    g.drawImage(phasigram, phasigram_area.toFloat());

    // Draw phase velocity bar chart
    drawPhaseVelocityBarChart(g, phaseVelArea);

    // Draw cyclogram
    g.setImageResamplingQuality(Graphics::lowResamplingQuality);
    g.drawImage(cyclogram, right.toFloat());

    // Draw border
    g.setColour(Colours::white);
    g.drawRect(plotBounds);
}

void MainComponent::timerCallback() {
    // std::cout << "Timer callback" << std::endl;
    updateHistoryImage();
    repaint();
}

void MainComponent::handleNoteOn(MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float /*velocity*/) {
    lastClickedMidiNote = midiNoteNumber;
    updateCurrentNote();
}

void MainComponent::handleNoteOff(MidiKeyboardState*, int /*midiChannel*/, int /*midiNoteNumber*/, float /*velocity*/) {
    // Optionally handle note off
}

void MainComponent::updateCurrentNote() {
    temperamentControls->setCurrentNote(lastClickedMidiNote);

    auto baseFreq = MidiMessage::getMidiNoteInHertz(lastClickedMidiNote);
    auto multiplier = temperamentControls->getFrequencyMultiplier(lastClickedMidiNote);
    processor.setTargetFrequency(baseFreq * multiplier);
}