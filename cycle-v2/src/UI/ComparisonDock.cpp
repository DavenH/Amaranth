#include <algorithm>

#include "UI/ComparisonDock.h"

#include "App/StandaloneAudioEngine.h"

#include "UI/ComparisonSurfaceView.h"

namespace CycleV2 {

namespace {

constexpr float kCardWidth = 300.f;
constexpr float kCardGap = 12.f;
constexpr float kCardInset = 12.f;
constexpr float kHeaderHeight = 42.f;
constexpr float kCardTitleHeight = 30.f;
constexpr float kPitchHeight = 44.f;

juce::Rectangle<float> cardBounds(int index, int height) {
    return {
            kCardInset + (float) index * (kCardWidth + kCardGap),
            8.f,
            kCardWidth,
            (float) height - 16.f
    };
}

}

class ComparisonDock::CardStrip : public juce::Component {
public:
    explicit CardStrip(ComparisonDock& ownerToUse) : owner(ownerToUse) {}

    void paint(juce::Graphics& graphics) override {
        for (int index = 0; index < (int) owner.cards.size(); ++index) {
            owner.paintCard(graphics, owner.cards[(size_t) index],
                    cardBounds(index, getHeight()));
        }
    }

    void mouseDown(const juce::MouseEvent& event) override {
        const auto* analysis = owner.library.selectedAnalysis();
        if (analysis == nullptr || owner.cards.empty()
                || !owner.cards.front().sample) {
            return;
        }
        const auto graph = ComparisonDock::pitchBounds(cardBounds(0, getHeight()));
        if (!graph.contains(event.position)) {
            return;
        }
        float nearestDistance = 10.f;
        for (size_t index = 0; index < analysis->pitchPath.size(); ++index) {
            const auto& point = analysis->pitchPath[index];
            const juce::Point<float> pixel(
                    graph.getX() + point.x * graph.getWidth(),
                    graph.getBottom() - point.y * graph.getHeight());
            const float distance = pixel.getDistanceFrom(event.position);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                draggedPoint = index;
                draggedPosition = { point.x, point.y };
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override {
        if (!draggedPoint.has_value()) {
            return;
        }
        const auto graph = ComparisonDock::pitchBounds(cardBounds(0, getHeight()));
        draggedPosition = {
                juce::jlimit(0.f, 1.f,
                        (event.position.x - graph.getX()) / graph.getWidth()),
                juce::jlimit(0.f, 1.f,
                        (graph.getBottom() - event.position.y) / graph.getHeight())
        };
        repaint(graph.toNearestInt());
    }

    void mouseUp(const juce::MouseEvent& event) override {
        if (draggedPoint.has_value()) {
            owner.commitPitchPoint(*draggedPoint, draggedPosition);
            draggedPoint.reset();
            return;
        }
        const int index = (int) ((event.position.x - kCardInset) / (kCardWidth + kCardGap));
        if (index < 0 || index >= (int) owner.cards.size()) {
            return;
        }
        const auto& card = owner.cards[(size_t) index];
        const auto close = cardBounds(index, getHeight()).removeFromRight(26.f)
                .removeFromTop(kCardTitleHeight);
        if (close.contains(event.position)) {
            if (card.sample) {
                owner.library.close(card.id);
                owner.refreshCaptures();
            } else {
                owner.canvas.unpinComparisonSpy(card.id);
            }
        } else if (card.sample && card.id != owner.library.selectedId()) {
            owner.selectReference(card.id);
        }
    }

    std::optional<size_t> draggedPoint;
    juce::Point<float> draggedPosition;

private:
    ComparisonDock& owner;
};

ComparisonDock::ComparisonDock(
        NodeCanvas& canvasToUse,
        ReferenceLibrary& libraryToUse,
        StandaloneAudioEngine& engine) :
        canvas(canvasToUse)
    ,   library(libraryToUse)
    ,   audioEngine(engine)
    ,   strip(std::make_unique<CardStrip>(*this)) {
    setOpaque(true);
    addAndMakeVisible(importButton);
    addAndMakeVisible(referenceButton);
    addAndMakeVisible(spyButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(undoButton);
    addAndMakeVisible(redoButton);
    addAndMakeVisible(noteSlider);
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(strip.get(), false);
    viewport.setScrollBarsShown(false, true, false, false);
    viewport.setScrollBarThickness(7);

    importButton.onClick = [this] { importReference(); };
    referenceButton.onClick = [this] { showReferenceMenu(); };
    spyButton.onClick = [this] { showSpyMenu(); };
    playButton.onClick = [this] {
        if (playButton.getButtonText() == "Stop") {
            audioEngine.stopReferenceAudition();
            playButton.setButtonText("Play");
        } else if (const auto* analysis = library.selectedAnalysis()) {
            if (audioEngine.startReferenceAudition(*analysis->sample)) {
                playButton.setButtonText("Stop");
            }
        }
    };
    undoButton.onClick = [this] {
        if (library.undoPitchEdit()) {
            refreshCaptures();
        }
    };
    redoButton.onClick = [this] {
        if (library.redoPitchEdit()) {
            refreshCaptures();
        }
    };
    noteSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    noteSlider.setRange(0, 127, 1);
    noteSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 22);
    noteSlider.setChangeNotificationOnlyOnRelease(true);
    noteSlider.onValueChange = [this] {
        if (library.setFundamental((int) noteSlider.getValue())) {
            refreshCaptures();
        }
    };
    updateNoteControl();
    updateCards();
}

ComparisonDock::~ComparisonDock() {
    audioEngine.stopReferenceAudition();
    surfaceViews.clear();
    retiredViews.clear();
    viewport.setViewedComponent(nullptr, false);
}

juce::PopupMenu::Options ComparisonDock::menuOptionsFor(juce::Component& target) {
    const auto origin = target.localPointToGlobal(juce::Point<int>());
    return juce::PopupMenu::Options().withTargetComponent(&target)
            .withTargetScreenArea({
                    origin.x, origin.y, target.getWidth(), target.getHeight()
            });
}

void ComparisonDock::importReference() {
    chooser = std::make_unique<juce::FileChooser>(
            "Import wave reference", juce::File(),
            "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectFiles,
            [safeThis = juce::Component::SafePointer<ComparisonDock>(this)](
                    const juce::FileChooser& selected) {
                if (safeThis == nullptr) {
                    return;
                }
                const juce::File file = selected.getResult();
                if (file == juce::File()) {
                    return;
                }
                juce::String error;
                if (!safeThis->library.importFile(file, error)) {
                    juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Reference import failed", error);
                    return;
                }
                safeThis->refreshCaptures();
            });
}

void ComparisonDock::showReferenceMenu() {
    juce::PopupMenu menu;
    int item = 1;
    for (const auto& record : library.records()) {
        menu.addItem(item++, record.name,
                true, record.id == library.selectedId());
    }
    const auto records = library.records();
    menu.showMenuAsync(menuOptionsFor(referenceButton),
            [safeThis = juce::Component::SafePointer<ComparisonDock>(this), records](int choice) {
                if (safeThis != nullptr && choice > 0
                        && choice <= (int) records.size()) {
                    safeThis->selectReference(records[(size_t) choice - 1].id);
                }
            });
}

void ComparisonDock::showSpyMenu() {
    juce::PopupMenu menu;
    std::vector<ComparisonSpyOption> options = canvas.comparisonSpyOptions();
    const auto createOptions = canvas.unprobedTrimeshOutputs();
    int item = 1;
    for (const auto& option : options) {
        const auto& pins = canvas.comparisonSpyPins();
        const bool pinned = std::find(pins.begin(), pins.end(), option.id) != pins.end();
        menu.addItem(item++, option.label, true, pinned);
    }
    if (!createOptions.empty()) {
        menu.addSeparator();
        for (const auto& option : createOptions) {
            menu.addItem(item++, "Create Spy on " + option.label);
        }
    }
    menu.showMenuAsync(menuOptionsFor(spyButton),
            [safeThis = juce::Component::SafePointer<ComparisonDock>(this),
                    options, createOptions](int choice) {
                if (safeThis == nullptr || choice <= 0) {
                    return;
                }
                if (choice <= (int) options.size()) {
                    const auto& id = options[(size_t) choice - 1].id;
                    const auto& pins = safeThis->canvas.comparisonSpyPins();
                    const bool pinned = std::find(pins.begin(), pins.end(), id) != pins.end();
                    if (pinned) {
                        safeThis->canvas.unpinComparisonSpy(id);
                    } else {
                        safeThis->canvas.pinComparisonSpy(id);
                    }
                } else {
                    const int createIndex = choice - (int) options.size() - 1;
                    if (juce::isPositiveAndBelow(createIndex, (int) createOptions.size())) {
                        safeThis->canvas.createAndPinTrimeshOutputSpy(
                                createOptions[(size_t) createIndex].id);
                    }
                }
            });
}

void ComparisonDock::selectReference(const juce::String& id) {
    juce::String error;
    if (!library.select(id, error)) {
        juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Reference unavailable", error);
        return;
    }
    audioEngine.stopReferenceAudition();
    playButton.setButtonText("Play");
    refreshCaptures();
}

void ComparisonDock::refreshCaptures() {
    if (library.selectedAnalysis() == nullptr && !library.records().empty()) {
        juce::String error;
        library.select(library.records().front().id, error);
    }
    updateNoteControl();
    updateCards();
}

void ComparisonDock::syncAuditionState() {
    if (!audioEngine.isReferenceAuditioning()
            && playButton.getButtonText() == "Stop") {
        playButton.setButtonText("Play");
    }
}

void ComparisonDock::updateNoteControl() {
    const auto* analysis = library.selectedAnalysis();
    noteSlider.setEnabled(analysis != nullptr);
    playButton.setEnabled(analysis != nullptr);
    undoButton.setEnabled(analysis != nullptr);
    redoButton.setEnabled(analysis != nullptr);
    if (analysis != nullptr) {
        noteSlider.setValue(analysis->sample->fundNote,
                juce::NotificationType::dontSendNotification);
    }
}

void ComparisonDock::updateCards() {
    const juce::ScopedLock lock(surfaceLock);
    auto previousCards = std::move(cards);
    const auto* analysis = library.selectedAnalysis();
    auto open = library.openReferenceIds();
    const auto selected = std::find(open.begin(), open.end(), library.selectedId());
    if (selected != open.end() && selected != open.begin()) {
        std::rotate(open.begin(), selected, selected + 1);
    }
    for (const auto& id : open) {
        const auto* sampleAnalysis = library.analysisFor(id);
        const auto record = std::find_if(library.records().begin(), library.records().end(),
                [&](const auto& candidate) { return candidate.id == id; });
        if (sampleAnalysis == nullptr || record == library.records().end()) {
            continue;
        }
        Card card;
        card.id = record->id;
        card.title = record->name;
        card.detail = juce::String(sampleAnalysis->sample->lengthSeconds(), 2) + " s  ·  MIDI "
                + juce::String(sampleAnalysis->sample->fundNote)
                + (sampleAnalysis->lowConfidence ? "  ·  Check pitch" : "");
        card.sampleSurface = &sampleAnalysis->cyclogram;
        card.sampleIdentity = sampleAnalysis->sample.get();
        card.surfaceRevision = sampleAnalysis->cyclogramRevision;
        card.columns = (size_t) sampleAnalysis->columns;
        card.rows = (size_t) sampleAnalysis->rows;
        card.midiNote = sampleAnalysis->sample->fundNote;
        card.sample = true;
        card.connected = true;
        cards.push_back(std::move(card));
    }

    const int note = analysis != nullptr
            ? analysis->sample->fundNote : canvas.previewMidiNote();
    if (!canvas.comparisonSpyPins().empty()) {
        ++captureCount;
    }
    const auto previews = canvas.captureComparisonSpies(note);
    const auto& pins = canvas.comparisonSpyPins();
    const auto options = canvas.comparisonSpyOptions();
    for (size_t index = 0; index < pins.size(); ++index) {
        Card card;
        card.id = pins[index];
        const auto found = std::find_if(options.begin(), options.end(), [&](const auto& option) {
            return option.id == card.id;
        });
        card.title = found != options.end() ? found->label : "Disconnected Spy";
        const double captureSeconds = (double) GraphPreviewExecutor::sourceFrameCountForMidiNote(note)
                / 44100.0;
        card.detail = juce::String(captureSeconds * 1000.0, 1) + " ms capture  ·  MIDI "
                + juce::String(note);
        if (index < previews.size() && previews[index].has_value()) {
            const auto& preview = *previews[index];
            card.connected = preview.connected;
            card.spectral = preview.domain != PortDomain::TimeSignal;
            card.detail += card.spectral ? "  ·  Spectral" : "  ·  Time";
            card.surface = preview.values;
            card.columns = preview.gridColumns;
            card.rows = preview.gridRows;
            card.domain = preview.domain;
            card.midiNote = note;
        }
        cards.push_back(std::move(card));
    }
    const int width = juce::jmax(viewport.getWidth(),
            (int) (kCardInset * 2.f + (float) cards.size() * (kCardWidth + kCardGap)));
    ZoomRect camera;
    if (!surfaceViews.empty() && surfaceViews.front() != nullptr) {
        camera = surfaceViews.front()->camera();
    }
    auto previousViews = std::move(surfaceViews);
    for (size_t index = 0; index < cards.size(); ++index) {
        const auto& card = cards[index];
        if (!card.connected || card.surfaceValues().empty()) {
            surfaceViews.push_back(nullptr);
            continue;
        }
        auto found = std::find_if(previousViews.begin(), previousViews.end(),
                [&](const auto& candidate) {
                    return candidate != nullptr && candidate->referenceId == card.id;
                });
        const bool viewWasVisible = found != previousViews.end();
        std::unique_ptr<ComparisonSurfaceView> view;
        if (found != previousViews.end()) {
            view = std::move(*found);
        } else {
            const auto retired = std::find_if(retiredViews.begin(), retiredViews.end(),
                    [&](const auto& candidate) {
                        return candidate != nullptr && candidate->referenceId == card.id;
                    });
            if (retired != retiredViews.end()) {
                view = std::move(*retired);
                retiredViews.erase(retired);
                view->setVisible(true);
            } else {
                view = std::make_unique<ComparisonSurfaceView>();
                strip->addAndMakeVisible(*view);
            }
        }
        view->referenceId = card.id;
        const auto oldCard = std::find_if(previousCards.begin(), previousCards.end(),
                [&](const Card& candidate) { return candidate.id == card.id; });
        const bool sampleUnchanged = viewWasVisible && card.sample
                && oldCard != previousCards.end()
                && oldCard->sampleIdentity == card.sampleIdentity
                && oldCard->surfaceRevision == card.surfaceRevision;
        if (!sampleUnchanged) {
            view->setGrid(card.surfaceValues(), card.columns, card.rows,
                    card.domain, card.midiNote);
        }
        view->onCameraChanged = [this, source = index](const ZoomRect& camera) {
            for (size_t target = 0; target < surfaceViews.size(); ++target) {
                if (target != source && surfaceViews[target] != nullptr
                        && cards[target].domain == cards[source].domain) {
                    surfaceViews[target]->setCamera(camera);
                }
            }
            canvas.requestComparisonRendering();
        };
        const auto bounds = cardBounds((int) index, viewport.getHeight());
        auto surface = bounds.withTrimmedTop(kCardTitleHeight + 3.f)
                .withTrimmedBottom(27.f + (card.sample ? kPitchHeight + 5.f : 0.f))
                .reduced(8.f, 1.f);
        view->setBounds(surface.toNearestInt());
        surfaceViews.push_back(std::move(view));
    }
    for (auto& view : previousViews) {
        if (view != nullptr) {
            view->setVisible(false);
            retiredViews.push_back(std::move(view));
        }
    }
    for (size_t index = 0; index < surfaceViews.size(); ++index) {
        if (surfaceViews[index] != nullptr
                && cards[index].domain == PortDomain::TimeSignal) {
            surfaceViews[index]->setCamera(camera);
        }
    }
    strip->setSize(width, viewport.getHeight());
    strip->repaint();
    repaint();
    canvas.requestComparisonRendering();
}

void ComparisonDock::renderOpenGL(float scaleFactor) {
    const juce::ScopedLock lock(surfaceLock);
    if (!isVisible()) {
        return;
    }
    for (const auto& view : surfaceViews) {
        if (view != nullptr) {
            if (view->renderOpenGL(scaleFactor)) {
                juce::Component::SafePointer<ComparisonSurfaceView> safeView(view.get());
                juce::MessageManager::callAsync([safeView] {
                    if (safeView != nullptr) {
                        safeView->repaint();
                    }
                });
            }
        }
    }
}

void ComparisonDock::releaseOpenGLResources() {
    const juce::ScopedLock lock(surfaceLock);
    for (const auto& view : surfaceViews) {
        if (view != nullptr) {
            view->releaseOpenGLResources();
        }
    }
    for (const auto& view : retiredViews) {
        if (view != nullptr) {
            view->releaseOpenGLResources();
        }
    }
}

juce::var ComparisonDock::exportAutomationState() const {
    auto state = std::make_unique<juce::DynamicObject>();
    state->setProperty("visible", isVisible());
    state->setProperty("height", getHeight());
    state->setProperty("cardCount", (int) cards.size());
    state->setProperty("captureCount", (int64) captureCount);
    state->setProperty("selectedReferenceId", library.selectedId());
    if (const auto* analysis = library.selectedAnalysis()) {
        state->setProperty("comparisonMidiNote", analysis->sample->fundNote);
    }
    juce::Array<juce::var> items;
    for (size_t index = 0; index < cards.size(); ++index) {
        const auto& card = cards[index];
        auto item = std::make_unique<juce::DynamicObject>();
        item->setProperty("id", card.id);
        item->setProperty("sample", card.sample);
        item->setProperty("connected", card.connected);
        item->setProperty("spectral", card.spectral);
        item->setProperty("hasSurface", !card.surfaceValues().empty());
        if (index < surfaceViews.size() && surfaceViews[index] != nullptr) {
            item->setProperty("surfaceView",
                    surfaceViews[index]->automationDiagnostics());
        }
        items.add(juce::var(item.release()));
    }
    state->setProperty("cards", std::move(items));
    return { state.release() };
}

void ComparisonDock::paintCard(
        juce::Graphics& graphics,
        const Card& card,
        juce::Rectangle<float> bounds) {
    const juce::Rectangle<float> fullBounds = bounds;
    graphics.setColour(juce::Colour(0xff202c36));
    graphics.fillRoundedRectangle(bounds, 7.f);
    graphics.setColour(juce::Colour(0xff4c6370));
    graphics.drawRoundedRectangle(bounds, 7.f, 1.f);
    graphics.setColour(juce::Colour(0xffecf0ef));
    graphics.setFont(14.f);
    const auto titleBounds = bounds.removeFromTop(kCardTitleHeight);
    graphics.drawText(card.title, titleBounds.reduced(10.f, 2.f),
            juce::Justification::centredLeft, true);
    auto closeBounds = titleBounds;
    graphics.drawText(juce::String::charToString(0x00d7),
            closeBounds.removeFromRight(26.f),
            juce::Justification::centred);
    if (!card.connected || card.surfaceValues().empty()) {
        graphics.setColour(juce::Colour(0xffaeb9bb));
        graphics.drawText("Disconnected", bounds,
                juce::Justification::centred);
        return;
    }

    const auto detailBounds = bounds.removeFromBottom(24.f);
    if (card.sample) {
        bounds.removeFromBottom(kPitchHeight + 5.f);
    }
    graphics.setColour(juce::Colour(0xff101b24));
    graphics.fillRect(bounds.reduced(8.f, 2.f));
    graphics.setColour(juce::Colour(0xffbed0d1));
    graphics.setFont(11.f);
    graphics.drawText(card.detail, detailBounds.reduced(9.f, 1.f),
            juce::Justification::centredLeft, true);

    if (card.sample) {
        const auto graph = pitchBounds(fullBounds);
        graphics.setColour(juce::Colour(0xff111c25));
        graphics.fillRect(graph);
        const auto* analysis = library.selectedAnalysis();
        if (analysis != nullptr) {
            juce::Path path;
            for (size_t index = 0; index < analysis->pitchPath.size(); ++index) {
                const auto& point = analysis->pitchPath[index];
                const juce::Point<float> pixel(
                        graph.getX() + point.x * graph.getWidth(),
                        graph.getBottom() - point.y * graph.getHeight());
                if (index == 0) {
                    path.startNewSubPath(pixel);
                } else {
                    path.lineTo(pixel);
                }
                graphics.setColour(juce::Colour(0xff65d3d6));
                graphics.fillEllipse(pixel.x - 2.f, pixel.y - 2.f, 4.f, 4.f);
            }
            graphics.setColour(juce::Colour(0xff65d3d6));
            graphics.strokePath(path, juce::PathStrokeType(1.4f));
        }
    }
}

juce::Rectangle<float> ComparisonDock::pitchBounds(juce::Rectangle<float> bounds) {
    bounds.removeFromBottom(24.f);
    return bounds.removeFromBottom(kPitchHeight + 5.f).reduced(8.f, 3.f);
}

void ComparisonDock::commitPitchPoint(size_t pointIndex, juce::Point<float> position) {
    if (library.movePitchPoint(pointIndex, position)) {
        refreshCaptures();
    }
}

void ComparisonDock::paint(juce::Graphics& graphics) {
    graphics.fillAll(juce::Colour(0xff14212b));
    graphics.setColour(juce::Colour(0xff4b626e));
    graphics.fillRect(0, 0, getWidth(), 2);
    graphics.setColour(juce::Colour(0xff81a1a8));
    graphics.fillRoundedRectangle(
            (float) getWidth() * 0.5f - 22.f, 3.f, 44.f, 3.f, 2.f);
    graphics.setColour(juce::Colour(0xffe5ecec));
    graphics.setFont(13.f);
    graphics.drawText("COMPARE", 12, 10, 85, 28, juce::Justification::centredLeft);
    graphics.drawText("NOTE", getWidth() - 197, 11, 42, 26,
            juce::Justification::centredLeft);
}

void ComparisonDock::resized() {
    const juce::ScopedLock lock(surfaceLock);
    importButton.setBounds(104, 9, 132, 26);
    referenceButton.setBounds(244, 9, 105, 26);
    spyButton.setBounds(357, 9, 82, 26);
    playButton.setBounds(447, 9, 62, 26);
    undoButton.setBounds(517, 9, 59, 26);
    redoButton.setBounds(584, 9, 59, 26);
    noteSlider.setBounds(juce::jmax(654, getWidth() - 155), 9,
            juce::jlimit(0, 150, getWidth() - 658), 26);
    viewport.setBounds(0, (int) kHeaderHeight, getWidth(),
            juce::jmax(0, getHeight() - (int) kHeaderHeight));
    strip->setSize(juce::jmax(viewport.getWidth(),
                    (int) (kCardInset * 2.f
                            + (float) cards.size() * (kCardWidth + kCardGap))),
            viewport.getHeight());
    for (size_t index = 0; index < surfaceViews.size(); ++index) {
        if (surfaceViews[index] == nullptr) {
            continue;
        }
        const auto bounds = cardBounds((int) index, viewport.getHeight());
        auto surface = bounds.withTrimmedTop(kCardTitleHeight + 3.f)
                .withTrimmedBottom(27.f + (cards[index].sample ? kPitchHeight + 5.f : 0.f))
                .reduced(8.f, 1.f);
        surfaceViews[index]->setBounds(surface.toNearestInt());
    }
    canvas.requestComparisonRendering();
}

void ComparisonDock::mouseDown(const juce::MouseEvent& event) {
    if (event.position.y <= 8.f) {
        resizeStartHeight = getHeight();
        resizing = true;
    }
}

void ComparisonDock::mouseDrag(const juce::MouseEvent& event) {
    if (resizing && heightChangeCallback) {
        heightChangeCallback(juce::jlimit(165, 320,
                resizeStartHeight - event.getDistanceFromDragStartY()));
    }
}

void ComparisonDock::mouseUp(const juce::MouseEvent&) {
    resizing = false;
}

}
