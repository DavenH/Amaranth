#include "UI/PerformanceKeyboard.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

namespace {

Image keyGrain() {
    Image texture(Image::ARGB, 512, 128, true);
    Random random(0x61a3b7);
    {
        Image::BitmapData pixels(texture, Image::BitmapData::writeOnly);
        for (int y = 0; y < texture.getHeight(); ++y) {
            for (int x = 0; x < texture.getWidth(); ++x) {
                const auto tone = random.nextBool() ? Colours::white : Colours::black;
                pixels.setPixelColour(x, y, tone.withAlpha((juce::uint8) random.nextInt(12)));
            }
        }
    }
    Graphics graphics(texture);
    for (int wash = 0; wash < 8; ++wash) {
        const float x = wash * 67.f - 20.f;
        const float y = random.nextFloat() * 100.f;
        const auto tone = wash % 3 == 0 ? Colour(0xffe5d4b8) : Colour(0xff9ab8d6);
        graphics.setGradientFill(ColourGradient(tone.withAlpha(0.085f), x, y,
                tone.withAlpha(0.f), x + 70.f, y + 40.f, true));
        graphics.fillEllipse(x - 70.f, y - 50.f, 140.f, 100.f);
    }
    return texture;
}

}


PerformanceKeyboard::PerformanceKeyboard(
        MidiKeyboardState& state,
        MidiEventSink& sink) :
        AmaranthMidiKeyboard(state, MidiKeyboardComponent::horizontalKeyboard)
    ,   keyboardState(state)
    ,   eventSink(sink)
    ,   stateListener(*this) {
    keyTexture = keyGrain();
    setName("PerformanceKeyboard");
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    setScrollButtonsVisible(false);
    setUseVectorKeys(true);
    // The JUCE backing remains visible through antialiased key edges and corners.
    setColour(MidiKeyboardComponent::whiteNoteColourId, CanvasChromePalette::restingControlSurface);
    setVectorKeyPalette({ Colour(0xff465361), Colour(0xff232c36),
            Colour(0xff171f28), Colour(0xff080e15),
            Colour(0xff3296b5), Colour(0xff12445e),
            Colour(0xff27627d), Colour(0xff142d40) });
    setMidiChannel(1);
    setVelocity(1.f, true);
    setAvailableRange(rangeStart, rangeStart + visibleSemitones);
    setLowestVisibleKey(rangeStart);
    keyboardState.addListener(&stateListener);
}

PerformanceKeyboard::~PerformanceKeyboard() {
    releaseAllNotes();
    keyboardState.removeListener(&stateListener);
}

Rectangle<float> PerformanceKeyboard::noteBounds(int noteNumber) const {
    if (noteNumber < rangeStart || noteNumber > rangeStart + visibleSemitones) {
        return {};
    }
    return getRectangleForKey(noteNumber).getIntersection(getLocalBounds().toFloat());
}

String PerformanceKeyboard::noteLabel(int noteNumber) const {
    return noteNumber % 12 == 0
            ? AmaranthMidiKeyboard::getText(noteNumber)
            : String {};
}

void PerformanceKeyboard::shiftOctave(int octaveDelta) {
    setRangeStart(rangeStart + octaveDelta * 12);
}

void PerformanceKeyboard::revealNote(int midiNote) {
    const int selectedNote = jlimit(0, 127, midiNote);
    int nextStart = rangeStart;
    while (selectedNote < nextStart) {
        nextStart = jmax(0, nextStart - 12);
    }
    while (selectedNote > nextStart + visibleSemitones) {
        nextStart = jmin(127 - visibleSemitones, nextStart + 12);
    }
    setRangeStart(nextStart);
}

void PerformanceKeyboard::revealRange(int lowest, int highest) {
    int bestStart = rangeStart;
    int bestOverlap = -1;
    int bestOffset = 128;
    for (int candidate = 0; candidate <= 103; candidate = candidate == 96 ? 103 : candidate + 12) {
        const int overlap = jmax(0,
                jmin(highest, candidate + visibleSemitones)
                        - jmax(lowest, candidate) + 1);
        const int centresDelta = lowest + highest - 2 * (candidate + 12);
        const int offset = centresDelta < 0 ? -centresDelta : centresDelta;
        if (overlap > bestOverlap || (overlap == bestOverlap && offset < bestOffset)) {
            bestStart = candidate;
            bestOverlap = overlap;
            bestOffset = offset;
        }
    }
    setRangeStart(bestStart);
}

void PerformanceKeyboard::setRangeStart(int noteNumber) {
    const int nextStart = jlimit(0, 127 - visibleSemitones, noteNumber);
    if (nextStart == rangeStart) {
        return;
    }

    releaseAllNotes();
    rangeStart = nextStart;
    setAvailableRange(rangeStart, rangeStart + visibleSemitones);
    setLowestVisibleKey(rangeStart);
    resized();
    repaint();
}

void PerformanceKeyboard::releaseAllNotes() {
    keyboardState.allNotesOff(1);
    eventSink.releaseMidiSource(MidiEventSource::PerformanceKeyboard);
    currentHeldNote = -1;
    currentVelocity = 0.f;
}

void PerformanceKeyboard::mirrorSequenceNote(
        int noteNumber, float velocity, bool noteOn) {
    const ScopedValueSetter<bool> visualOnly(mirroringSequenceNote, true);
    if (noteOn) {
        keyboardState.noteOn(1, noteNumber, velocity);
    } else {
        keyboardState.noteOff(1, noteNumber, velocity);
    }
}

void PerformanceKeyboard::resized() {
    constexpr int whiteKeyCount = 15;
    if (getWidth() <= 0) {
        return;
    }
    setKeyWidth((float) getWidth() / (float) whiteKeyCount);
    AmaranthMidiKeyboard::resized();
}

void PerformanceKeyboard::drawWhiteNote(
        int midiNoteNumber,
        Graphics& graphics,
        Rectangle<float> area,
        bool isDown,
        bool isOver,
        Colour lineColour,
        Colour textColour) {
    AmaranthMidiKeyboard::drawWhiteNote(
            midiNoteNumber,
            graphics,
            area,
            isDown,
            isOver,
            lineColour,
            textColour);

    paintKeyTexture(graphics, area, midiNoteNumber, false);
    const String label = noteLabel(midiNoteNumber);
    if (label.isEmpty()) {
        return;
    }
    const float labelHeight = jlimit(10.f, 14.f, area.getHeight() * 0.2f);
    const Rectangle<float> labelBounds = area
            .removeFromBottom(labelHeight + 3.f)
            .reduced(2.f, 0.f);
    graphics.setColour(Colours::white.withAlpha(isDown ? 0.92f : 0.72f));
    graphics.setFont(FontOptions(labelHeight * 0.72f, Font::plain));
    graphics.drawText(label, labelBounds, Justification::centred, false);
}

void PerformanceKeyboard::drawBlackNote(int note, Graphics& graphics, Rectangle<float> area,
        bool down, bool over, Colour fill) {
    AmaranthMidiKeyboard::drawBlackNote(note, graphics, area, down, over, fill);
    paintKeyTexture(graphics, area, note, true);
}

void PerformanceKeyboard::paintKeyTexture(Graphics& graphics, Rectangle<float> area, int note, bool black) {
    Graphics::ScopedSaveState save(graphics);
    const auto interior = area.reduced(1.5f);
    graphics.reduceClipRegion(interior.toNearestInt());
    graphics.setOpacity(black ? 0.35f : 0.7f);
    const int width = jlimit(1, 128, roundToInt(area.getWidth()));
    const int offset = (note * 97) % (keyTexture.getWidth() - width);
    graphics.drawImage(keyTexture, interior.getX(), interior.getY(), interior.getWidth(), interior.getHeight(),
            offset, 0, width, keyTexture.getHeight());
}

bool PerformanceKeyboard::mouseDownOnKey(
        int midiNoteNumber,
        const MouseEvent& event) {
    if (event.mods.isRightButtonDown() || event.mods.isPopupMenu()) {
        if (previewNoteSelected) {
            previewNoteSelected(midiNoteNumber);
        }
        return false;
    }
    if (primaryGestureStarted) {
        primaryGestureStarted();
    }
    return true;
}

void PerformanceKeyboard::StateListener::handleNoteOn(
        MidiKeyboardState*,
        int midiChannel,
        int midiNoteNumber,
        float velocity) {
    owner.handleNoteOn(midiChannel, midiNoteNumber, velocity);
}

void PerformanceKeyboard::StateListener::handleNoteOff(
        MidiKeyboardState*,
        int midiChannel,
        int midiNoteNumber,
        float velocity) {
    owner.handleNoteOff(midiChannel, midiNoteNumber, velocity);
}

void PerformanceKeyboard::handleNoteOn(
        int midiChannel,
        int midiNoteNumber,
        float velocity) {
    currentHeldNote = midiNoteNumber;
    currentVelocity = velocity;
    if (mirroringSequenceNote) {
        return;
    }
    eventSink.enqueueMidiMessage(
            MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity),
            MidiEventSource::PerformanceKeyboard);
}

void PerformanceKeyboard::handleNoteOff(
        int midiChannel,
        int midiNoteNumber,
        float velocity) {
    if (!mirroringSequenceNote) {
        eventSink.enqueueMidiMessage(
                MidiMessage::noteOff(midiChannel, midiNoteNumber, velocity),
                MidiEventSource::PerformanceKeyboard);
    }
    if (currentHeldNote == midiNoteNumber) {
        currentHeldNote = -1;
        currentVelocity = 0.f;
    }
}

}
