#include <algorithm>

#include "UI/ComparisonDock.h"

#include "App/StandaloneAudioEngine.h"
#include "Graph/DefaultOutputProbeResolver.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
#include "UI/ComparisonSurfaceView.h"
#include "UI/ReferencePitchEditor.h"
#include "UI/WorkspaceDock.h"

namespace CycleV2 {

namespace {

constexpr float kCardWidth = 300.f;
constexpr float kCardGap = 8.f;
constexpr float kCardInset = 8.f;
constexpr int kResizeGripHeight = 6;
constexpr int kSourceControlWidth = 42;
constexpr float kCardTitleHeight = 28.f;

juce::Rectangle<float> cardBounds(int index, int height) {
    return {
            kCardInset + (float) index * (kCardWidth + kCardGap),
            5.f,
            kCardWidth,
            (float) height - 10.f
    };
}

}

class ComparisonDock::AddButton final : public juce::Button {
public:
    AddButton() : juce::Button("Add comparison source") {
        setTooltip("Open or import a wave reference, or pin a Spy");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
    }

    void paintButton(juce::Graphics& graphics, bool hovered, bool pressed) override {
        WorkspaceDock::paintIconButton(
                graphics, getLocalBounds().toFloat(), WorkspaceDockIcon::Add,
                hasKeyboardFocus(false));
        if (hovered || pressed) {
            graphics.setColour(CanvasChromePalette::text.withAlpha(
                    pressed ? 0.12f : 0.06f));
            graphics.fillRoundedRectangle(getLocalBounds().toFloat(),
                    CanvasChromeMetrics::controlCornerRadius);
        }
    }
};

class ComparisonDock::CardStrip : public juce::Component {
public:
    explicit CardStrip(ComparisonDock& ownerToUse) : owner(ownerToUse) {}

    void paint(juce::Graphics& graphics) override {
        for (int index = 0; index < (int) owner.cards.size(); ++index) {
            owner.paintCard(graphics, owner.cards[(size_t) index],
                    cardBounds(index, getHeight()));
        }
    }

    void mouseUp(const juce::MouseEvent& event) override {
        const int index = (int) ((event.position.x - kCardInset) / (kCardWidth + kCardGap));
        if (index < 0 || index >= (int) owner.cards.size()) {
            return;
        }
        const auto& card = owner.cards[(size_t) index];
        if (event.mods.isPopupMenu()) {
            owner.showCardMenu(card, event.position.toInt());
            return;
        }
        const auto close = cardBounds(index, getHeight()).removeFromRight(26.f)
                .removeFromTop(kCardTitleHeight);
        if (close.contains(event.position)) {
            if (card.sample) {
                owner.library.close(card.id);
                owner.refreshCaptures();
            } else {
                owner.canvas.unpinComparisonSpy(card.id);
            }
        } else if (card.sample) {
            auto play = cardBounds(index, getHeight());
            play.removeFromRight(26.f);
            play = play.removeFromRight(26.f).removeFromTop(kCardTitleHeight);
            if (play.contains(event.position)) {
                if (card.id != owner.library.selectedId()) {
                    owner.selectReference(card.id);
                }
                owner.toggleAudition();
            } else if (card.id != owner.library.selectedId()) {
                owner.selectReference(card.id);
            }
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override {
        const int index = (int) ((event.position.x - kCardInset) / (kCardWidth + kCardGap));
        if (juce::isPositiveAndBelow(index, (int) owner.cards.size())
                && owner.cards[(size_t) index].sample) {
            owner.openPitchEditor(owner.cards[(size_t) index].id);
        }
    }

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
    ,   addButton(std::make_unique<AddButton>())
    ,   strip(std::make_unique<CardStrip>(*this))
    ,   pitchEditor(std::make_unique<ReferencePitchEditor>(libraryToUse)) {
    setOpaque(true);
    addAndMakeVisible(*addButton);
    addAndMakeVisible(viewport);
    addAndMakeVisible(*pitchEditor);
    pitchEditor->setVisible(false);
    pitchEditor->onEdited = [this] { refreshCaptures(); };
    pitchEditor->onClose = [this] { closePitchEditor(); };
    pitchEditor->onRenderRequested = [this] { canvas.requestComparisonRendering(); };
    viewport.setViewedComponent(strip.get(), false);
    viewport.setScrollBarsShown(false, true, false, true);
    viewport.setScrollBarThickness(7);

    addButton->onClick = [this] { showAddMenu(); };
    updateCards();
}

ComparisonDock::~ComparisonDock() {
    audioEngine.stopReferenceAudition();
    pitchEditor.reset();
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

void ComparisonDock::showAddMenu() {
    juce::PopupMenu menu;
    menu.addItem(1, "Import Wave Reference...");
    const auto records = library.records();
    if (!records.empty()) {
        menu.addSeparator();
        menu.addSectionHeader("Wave References");
        for (size_t index = 0; index < records.size(); ++index) {
            const auto& record = records[index];
            menu.addItem(100 + (int) index,
                    record.name + (record.factory ? "  /  Factory" : ""),
                    true, record.id == library.selectedId());
        }
    }
    const std::vector<ComparisonSpyOption> options = canvas.comparisonSpyOptions();
    const auto createOptions = canvas.unprobedTrimeshOutputs();
    if (!options.empty() || !createOptions.empty()) {
        menu.addSeparator();
        menu.addSectionHeader("Pin Spy");
    }
    for (size_t index = 0; index < options.size(); ++index) {
        const auto& option = options[index];
        const auto& pins = canvas.comparisonSpyPins();
        const bool pinned = std::find(pins.begin(), pins.end(), option.id) != pins.end();
        menu.addItem(1000 + (int) index, option.label, true, pinned);
    }
    if (!createOptions.empty()) {
        for (size_t index = 0; index < createOptions.size(); ++index) {
            menu.addItem(2000 + (int) index,
                    "Create Spy on " + createOptions[index].label);
        }
    }
    menu.showMenuAsync(menuOptionsFor(*addButton),
            [safeThis = juce::Component::SafePointer<ComparisonDock>(this),
                    records, options, createOptions](int choice) {
                if (safeThis == nullptr || choice <= 0) {
                    return;
                }
                if (choice == 1) {
                    safeThis->importReference();
                } else if (choice >= 100 && choice < 100 + (int) records.size()) {
                    safeThis->selectReference(records[(size_t) choice - 100].id);
                } else if (choice >= 1000 && choice < 1000 + (int) options.size()) {
                    const auto& id = options[(size_t) choice - 1000].id;
                    const auto& pins = safeThis->canvas.comparisonSpyPins();
                    const bool pinned = std::find(pins.begin(), pins.end(), id) != pins.end();
                    if (pinned) {
                        safeThis->canvas.unpinComparisonSpy(id);
                    } else {
                        safeThis->canvas.pinComparisonSpy(id);
                    }
                } else if (choice >= 2000
                        && choice < 2000 + (int) createOptions.size()) {
                    safeThis->canvas.createAndPinTrimeshOutputSpy(
                            createOptions[(size_t) choice - 2000].id);
                }
            });
}

void ComparisonDock::showCardMenu(const Card& card, juce::Point<int> position) {
    juce::PopupMenu menu;
    if (card.sample) {
        menu.addItem(1, audioEngine.isReferenceAuditioning()
                && card.id == library.selectedId() ? "Stop Audition" : "Audition Sample");
        if (card.id != library.selectedId()) {
            menu.addItem(2, "Use as Comparison Target");
        }
        menu.addSeparator();
        menu.addItem(7, "Edit Pitch Envelope...");
        menu.addItem(3, "Undo Pitch Edit", card.id == library.selectedId());
        menu.addItem(4, "Redo Pitch Edit", card.id == library.selectedId());
        menu.addSeparator();
        menu.addItem(5, "Close Reference");
    } else {
        menu.addItem(6, "Unpin Spy");
    }
    const auto screen = strip->localPointToGlobal(position);
    const auto target = juce::PopupMenu::Options()
            .withTargetComponent(strip.get())
            .withTargetScreenArea({ screen.x, screen.y, 1, 1 });
    const juce::String id = card.id;
    menu.showMenuAsync(target,
            [safeThis = juce::Component::SafePointer<ComparisonDock>(this), id](int choice) {
                if (safeThis == nullptr) {
                    return;
                }
                switch (choice) {
                    case 1:
                        if (id != safeThis->library.selectedId()) {
                            safeThis->selectReference(id);
                        }
                        safeThis->toggleAudition();
                        break;
                    case 2: safeThis->selectReference(id); break;
                    case 3:
                        if (safeThis->library.undoPitchEdit()) {
                            safeThis->refreshCaptures();
                        }
                        break;
                    case 4:
                        if (safeThis->library.redoPitchEdit()) {
                            safeThis->refreshCaptures();
                        }
                        break;
                    case 5:
                        safeThis->library.close(id);
                        safeThis->refreshCaptures();
                        break;
                    case 6: safeThis->canvas.unpinComparisonSpy(id); break;
                    case 7: safeThis->openPitchEditor(id); break;
                    default: break;
                }
            });
}

void ComparisonDock::openPitchEditor(const juce::String& id) {
    if (id != library.selectedId()) {
        selectReference(id);
    }
    if (!pitchEditor->open(id)) {
        return;
    }
    viewport.setVisible(false);
    addButton->setVisible(false);
    pitchEditor->setVisible(true);
    pitchEditor->toFront(true);
    pitchEditor->setBounds(0, kResizeGripHeight, getWidth(),
            juce::jmax(0, getHeight() - kResizeGripHeight));
    canvas.requestComparisonRendering();
}

bool ComparisonDock::openPitchEditorForAutomation(const juce::String& id) {
    openPitchEditor(id);
    return pitchEditor->isVisible() && library.selectedId() == id;
}

juce::Rectangle<int> ComparisonDock::pitchCurveBoundsForAutomation() const {
    return pitchEditor->isVisible()
            ? pitchEditor->curveBounds().translated(
                    pitchEditor->getX(), pitchEditor->getY())
            : juce::Rectangle<int>();
}

void ComparisonDock::closePitchEditor() {
    pitchEditor->setVisible(false);
    viewport.setVisible(true);
    addButton->setVisible(true);
    canvas.requestComparisonRendering();
}

void ComparisonDock::toggleAudition() {
    if (audioEngine.isReferenceAuditioning()) {
        audioEngine.stopReferenceAudition();
    } else if (const auto* analysis = library.selectedAnalysis()) {
        audioEngine.startReferenceAudition(*analysis->sample);
    }
    lastAuditioning = audioEngine.isReferenceAuditioning();
    strip->repaint();
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
    if (pitchEditor->isVisible()) {
        pitchEditor->open(id);
    }
    refreshCaptures();
}

void ComparisonDock::refreshCaptures() {
    if (library.selectedAnalysis() == nullptr && !library.records().empty()) {
        juce::String error;
        library.select(library.records().front().id, error);
    }
    updateCards();
}

void ComparisonDock::syncAuditionState() {
    const bool auditioning = audioEngine.isReferenceAuditioning();
    if (lastAuditioning != auditioning) {
        lastAuditioning = auditioning;
        strip->repaint();
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
        card.duration = juce::String(sampleAnalysis->sample->lengthSeconds(), 2) + " s";
        card.displayGain = 2.f;
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
        card.duration = juce::String(captureSeconds * 1000.0, 1) + " ms";
        card.displayGain = card.id == DefaultOutputProbeResolver::probeId
                ? 1.f / 3.f : 1.f;
        if (index < previews.size() && previews[index].has_value()) {
            const auto& preview = *previews[index];
            card.connected = preview.connected;
            card.spectral = preview.domain != PortDomain::TimeSignal;
            if (card.spectral) {
                card.title += "  /  Spectral";
            }
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
                    card.domain, card.midiNote, card.displayGain);
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
                .reduced(6.f, 5.f);
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
    if (pitchEditor->isVisible()) {
        pitchEditor->renderOpenGL(scaleFactor);
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
    pitchEditor->releaseOpenGLResources();
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
    state->setProperty("pitchEditor", pitchEditor->exportAutomationState());
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
    const bool selected = card.sample && card.id == library.selectedId();
    graphics.setColour(CanvasChromePalette::insetBackground);
    graphics.fillRoundedRectangle(bounds, CanvasChromeMetrics::tileCornerRadius);
    graphics.setColour(selected
            ? CanvasChromePalette::selectionOutline
            : CanvasChromePalette::border.withAlpha(0.72f));
    graphics.drawRoundedRectangle(bounds, CanvasChromeMetrics::tileCornerRadius,
            selected ? CanvasChromeMetrics::activeBorderWidth
                    : CanvasChromeMetrics::restingBorderWidth);

    auto titleBounds = bounds.removeFromTop(kCardTitleHeight);
    auto textBounds = titleBounds.reduced(9.f, 2.f);
    textBounds.removeFromRight(card.sample ? 100.f : 56.f);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(13.f);
    graphics.drawText(card.title, textBounds,
            juce::Justification::centredLeft, true);
    auto closeBounds = titleBounds.removeFromRight(26.f);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(16.f);
    graphics.drawText(juce::String::charToString(0x00d7), closeBounds,
            juce::Justification::centred);
    auto durationBounds = titleBounds.removeFromRight(card.sample ? 67.f : 55.f);
    if (card.sample) {
        const bool playing = selected && audioEngine.isReferenceAuditioning();
        const auto playBounds = titleBounds.removeFromRight(26.f);
        graphics.setColour(selected ? CanvasChromePalette::text
                : CanvasChromePalette::mutedText);
        if (playing) {
            graphics.fillRect(playBounds.withSizeKeepingCentre(8.f, 8.f));
        } else {
            juce::Path play;
            play.addTriangle(
                    playBounds.getCentreX() - 3.f, playBounds.getCentreY() - 5.f,
                    playBounds.getCentreX() + 5.f, playBounds.getCentreY(),
                    playBounds.getCentreX() - 3.f, playBounds.getCentreY() + 5.f);
            graphics.fillPath(play);
        }
    }
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(11.f);
    graphics.drawText(card.duration, durationBounds,
            juce::Justification::centredRight, true);

    if (!card.connected || card.surfaceValues().empty()) {
        graphics.setColour(CanvasChromePalette::mutedText);
        graphics.drawText("Disconnected", bounds,
                juce::Justification::centred);
        return;
    }
    graphics.setColour(CanvasChromePalette::canvasBackground);
    graphics.fillRect(bounds.reduced(6.f, 5.f));
}

void ComparisonDock::paint(juce::Graphics& graphics) {
    graphics.fillAll(CanvasChromePalette::canvasBackground);
    graphics.setColour(CanvasChromePalette::border);
    graphics.fillRect(0, 0, getWidth(), 1);
    graphics.setColour(CanvasChromePalette::strongBorder.withAlpha(0.7f));
    graphics.fillRoundedRectangle(
            (float) getWidth() * 0.5f - 22.f, 2.f, 44.f, 2.f, 1.f);
}

void ComparisonDock::resized() {
    const juce::ScopedLock lock(surfaceLock);
    addButton->setBounds(8, kResizeGripHeight + 7, 26, 24);
    viewport.setBounds(kSourceControlWidth, kResizeGripHeight,
            juce::jmax(0, getWidth() - kSourceControlWidth),
            juce::jmax(0, getHeight() - kResizeGripHeight));
    pitchEditor->setBounds(0, kResizeGripHeight, getWidth(),
            juce::jmax(0, getHeight() - kResizeGripHeight));
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
                .reduced(6.f, 5.f);
        surfaceViews[index]->setBounds(surface.toNearestInt());
    }
    canvas.requestComparisonRendering();
}

void ComparisonDock::mouseDown(const juce::MouseEvent& event) {
    if (event.position.y < (float) kResizeGripHeight) {
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
