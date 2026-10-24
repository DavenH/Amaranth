#include "UI/ReferenceAssetBrowser.h"

#include "App/StandaloneAudioEngine.h"
#include "UI/CanvasChromePalette.h"
#include "UI/SidebarMediaRow.h"
#include "UI/LibraryIconRenderer.h"
#include "UI/ReferenceWaveformPreview.h"

namespace CycleV2 {

namespace {

constexpr int rowHeight = 82;
const juce::String dragPrefix { "cycle-reference:" };

juce::Rectangle<float> playBounds(juce::Rectangle<float> row) {
    return SidebarMediaRow::cardBounds(row).reduced(5.f)
            .removeFromTop(24.f).removeFromRight(26.f);
}

}

class ReferenceAssetBrowser::List final : public juce::Component,
                                          private juce::ChangeListener {
    class WaveformJob final : public juce::ThreadPoolJob {
    public:
        WaveformJob(List& list, const ReferenceRecord& record) :
                ThreadPoolJob("Reference waveform")
            ,   owner(&list)
            ,   file(record.audioFile)
            ,   id(record.id) {}

        JobStatus runJob() override {
            PitchedSample sample;
            if (shouldExit() || sample.load(file.getFullPathName()) < 0) {
                return jobHasFinished;
            }
            auto mono = std::make_shared<juce::AudioBuffer<float>>(
                    referenceWaveformPreview(sample.audio));
            if (!shouldExit()) {
                juce::MessageManager::callAsync([safeOwner = owner, id = id, mono] {
                    if (safeOwner != nullptr) {
                        safeOwner->publishWaveform(id, *mono);
                    }
                });
            }
            return jobHasFinished;
        }

    private:
        juce::Component::SafePointer<List> owner;
        juce::File file;
        juce::String id;
    };

public:
    explicit List(ReferenceAssetBrowser& browser) : owner(browser) {
        formats.registerBasicFormats();
        setWantsKeyboardFocus(true);
        setComponentID("workspace.sidebar.assetList");
    }

    ~List() override {
        worker.removeAllJobs(true, 2000);
        for (auto& entry : entries) {
            if (entry.thumbnail != nullptr) {
                entry.thumbnail->removeChangeListener(this);
            }
        }
    }

    void refresh() {
        for (const auto& record : owner.library.records()) {
            const auto found = std::find_if(entries.begin(), entries.end(),
                    [&](const Entry& entry) { return entry.record.id == record.id; });
            if (found != entries.end()) {
                continue;
            }
            Entry entry;
            entry.record = record;
            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(record.audioFile));
            if (reader != nullptr) {
                entry.seconds = (double) reader->lengthInSamples / reader->sampleRate;
                if (entry.seconds <= 4.0) {
                    entry.thumbnail = std::make_unique<juce::AudioThumbnail>(128, formats, cache);
                    entry.thumbnail->addChangeListener(this);
                    entry.thumbnail->reset(1, reader->sampleRate, reader->lengthInSamples);
                }
            }
            const bool prepareWaveform = entry.thumbnail != nullptr;
            entries.push_back(std::move(entry));
            if (prepareWaveform) {
                worker.addJob(new WaveformJob(*this, record), true);
            }
        }
        filter();
    }

    void filter() {
        visible.clear();
        const auto words = juce::StringArray::fromTokens(
                owner.toolbar.searchField().getText().toLowerCase(), true);
        for (size_t index = 0; index < entries.size(); ++index) {
            const auto name = entries[index].record.name.toLowerCase();
            if (std::all_of(words.begin(), words.end(),
                    [&](const auto& word) { return name.contains(word); })) {
                visible.push_back(index);
            }
        }
        if (std::none_of(visible.begin(), visible.end(),
                [&](size_t index) { return entries[index].record.id == selectedId; })) {
            selectedId = visible.empty() ? juce::String() : entries[visible.front()].record.id;
        }
        owner.toolbar.editButton().setEnabled(selectedId.isNotEmpty());
        setSize(getWidth(), juce::jmax(rowHeight, (int) visible.size() * rowHeight));
        repaint();
    }

    void paint(juce::Graphics& graphics) override {
        if (visible.empty()) {
            graphics.setColour(CanvasChromePalette::mutedText);
            graphics.setFont(13.f);
            graphics.drawText("No matching audio assets", getLocalBounds().reduced(8),
                    juce::Justification::centredTop);
        }
        for (int index = 0; index < (int) visible.size(); ++index) {
            auto& entry = entries[visible[(size_t) index]];
            const auto slot = row(index);
            if (!graphics.getClipBounds().toFloat().intersects(slot)) {
                continue;
            }
            const auto preview = SidebarMediaRow::paintPatternFrame(
                    graphics, slot, entry.record.id == selectedId);
            auto title = SidebarMediaRow::cardBounds(slot).reduced(7.f, 3.f)
                    .removeFromTop(24.f).withTrimmedRight(30.f);
            graphics.setColour(CanvasChromePalette::text);
            graphics.setFont(13.f);
            graphics.drawText(entry.record.name, title, juce::Justification::centredLeft, true);
            const auto play = playBounds(slot);
            if (owner.engine.isReferenceAuditioningFile(entry.record.audioFile.getFullPathName())) {
                graphics.fillRect(play.withSizeKeepingCentre(8.f, 8.f));
            } else {
                juce::Path triangle;
                triangle.addTriangle(play.getCentreX() - 3.f, play.getCentreY() - 5.f,
                        play.getCentreX() + 5.f, play.getCentreY(),
                        play.getCentreX() - 3.f, play.getCentreY() + 5.f);
                graphics.fillPath(triangle);
            }
            graphics.setColour(CanvasChromePalette::navigationAccent);
            if (entry.thumbnail != nullptr) {
                entry.thumbnail->drawChannel(graphics, preview.reduced(3.f).toNearestInt(),
                        0.0, entry.seconds, 0, 1.f);
            } else {
                graphics.setColour(CanvasChromePalette::mutedText);
                graphics.setFont(11.f);
                graphics.drawText(juce::String(entry.seconds, 1) + " s", preview,
                        juce::Justification::centredLeft);
            }
        }
    }

    void mouseDown(const juce::MouseEvent& event) override {
        const int index = event.y / rowHeight;
        if (!juce::isPositiveAndBelow(index, (int) visible.size())) {
            return;
        }
        selectedId = entries[visible[(size_t) index]].record.id;
        owner.toolbar.editButton().setEnabled(true);
        grabKeyboardFocus();
        repaint();
    }

    void mouseUp(const juce::MouseEvent& event) override {
        const int index = event.y / rowHeight;
        if (!event.mouseWasDraggedSinceMouseDown()
                && juce::isPositiveAndBelow(index, (int) visible.size())
                && playBounds(row(index)).contains(event.position)) {
            owner.audition(entries[visible[(size_t) index]].record.id);
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override {
        const int index = event.y / rowHeight;
        if (juce::isPositiveAndBelow(index, (int) visible.size())
                && !playBounds(row(index)).contains(event.position)) {
            owner.addSelected();
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override {
        if (event.getDistanceFromDragStart() < 5 || selectedId.isEmpty()) {
            return;
        }
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this)) {
            if (!container->isDragAndDropActive()) {
                const int index = (int) std::distance(visible.begin(),
                        std::find_if(visible.begin(), visible.end(), [&](size_t candidate) {
                            return entries[candidate].record.id == selectedId;
                        }));
                auto image = createComponentSnapshot(row(index).toNearestInt(), true, 2.f);
                image.multiplyAllAlphas(0.9f);
                const juce::Point<int> offset(-getWidth() / 2, -rowHeight);
                // A desktop image stays above the canvas's native OpenGL view.
                container->startDragging(dragDescription(selectedId), this,
                        juce::ScaledImage(image, 2.0), true, &offset, &event.source);
            }
        }
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::returnKey) {
            owner.addSelected();
        } else if (key == juce::KeyPress::spaceKey) {
            owner.audition(selectedId);
        } else if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey) {
            const auto current = std::find_if(visible.begin(), visible.end(),
                    [&](size_t index) { return entries[index].record.id == selectedId; });
            const int index = juce::jlimit(0, juce::jmax(0, (int) visible.size() - 1),
                    (int) std::distance(visible.begin(), current)
                            + (key == juce::KeyPress::upKey ? -1 : 1));
            if (!visible.empty()) {
                selectedId = entries[visible[(size_t) index]].record.id;
                owner.viewport.setViewPosition(0, index * rowHeight);
                repaint();
            }
        } else {
            return false;
        }
        return true;
    }

    std::vector<std::pair<juce::String, juce::Rectangle<float>>> targets() const {
        std::vector<std::pair<juce::String, juce::Rectangle<float>>> result;
        for (int index = 0; index < (int) visible.size(); ++index) {
            const auto bounds = row(index).translated((float) owner.viewport.getX(),
                    (float) owner.viewport.getY() - owner.viewport.getViewPositionY());
            if (!owner.viewport.getBounds().toFloat().intersects(bounds)) {
                continue;
            }
            const auto& id = entries[visible[(size_t) index]].record.id;
            result.push_back({ "workspace.sidebar.asset." + id, bounds });
            result.push_back({ "workspace.sidebar.assetPlay." + id, playBounds(bounds) });
        }
        return result;
    }

    juce::String selectedId;

private:
    struct Entry {
        ReferenceRecord record;
        double seconds {};
        std::unique_ptr<juce::AudioThumbnail> thumbnail;
    };

    juce::Rectangle<float> row(int index) const {
        return { 0.f, (float) (index * rowHeight), (float) getWidth(), (float) rowHeight };
    }

    void publishWaveform(const juce::String& id, const juce::AudioBuffer<float>& mono) {
        const auto found = std::find_if(entries.begin(), entries.end(),
                [&](const Entry& entry) { return entry.record.id == id; });
        if (found != entries.end() && found->thumbnail != nullptr) {
            found->thumbnail->addBlock(0, mono, 0, mono.getNumSamples());
            repaint();
        }
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }

    ReferenceAssetBrowser& owner;
    juce::AudioFormatManager formats;
    juce::AudioThumbnailCache cache { 64 };
    juce::ThreadPool worker { 1 };
    std::vector<Entry> entries;
    std::vector<size_t> visible;
};

ReferenceAssetBrowser::ReferenceAssetBrowser(ReferenceLibrary& libraryToUse,
        StandaloneAudioEngine& engineToUse) :
        library(libraryToUse)
    ,   engine(engineToUse)
    ,   list(std::make_unique<List>(*this)) {
    addAndMakeVisible(toolbar);
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(list.get(), false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(7);
    viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId,
            CanvasChromePalette::mutedText);
    auto& search = toolbar.searchField();
    search.setComponentID("workspace.sidebar.assetSearch");
    search.onTextChange = [this] { list->filter(); };
    LibraryIconRenderer::configure(toolbar.newButton(), LibraryIcon::Import, "Import audio...");
    toolbar.newButton().setComponentID("workspace.sidebar.assetImport");
    toolbar.newButton().onClick = [this] {
        if (onImport) {
            onImport();
        }
    };
    LibraryIconRenderer::configure(toolbar.editButton(), LibraryIcon::AddReference, "Add selected audio to Wave References");
    toolbar.editButton().setComponentID("workspace.sidebar.assetAdd");
    toolbar.editButton().onClick = [this] { addSelected(); };
    LibraryIconRenderer::configure(toolbar.renameButton(), LibraryIcon::Tray, "Show Wave References");
    toolbar.renameButton().setComponentID("workspace.sidebar.assetTray");
    toolbar.renameButton().onClick = [this] {
        if (onShowTray) {
            onShowTray();
        }
    };
    toolbar.deleteButton().setVisible(false);
    refresh();
    startTimerHz(10);
}

ReferenceAssetBrowser::~ReferenceAssetBrowser() {
    viewport.setViewedComponent(nullptr, false);
}

void ReferenceAssetBrowser::refresh() { list->refresh(); }

void ReferenceAssetBrowser::timerCallback() {
    if (!isShowing()) {
        return;
    }
    const bool auditioning = engine.isReferenceAuditioning();
    const auto& path = engine.referenceAuditionSourcePath();
    if (lastAuditioning != auditioning || lastAuditionPath != path) {
        lastAuditioning = auditioning;
        lastAuditionPath = path;
        list->repaint();
    }
}

void ReferenceAssetBrowser::addSelected() {
    if (onAdd && list->selectedId.isNotEmpty()) {
        onAdd(list->selectedId);
    }
}

void ReferenceAssetBrowser::audition(const juce::String& id) {
    const auto found = std::find_if(library.records().begin(), library.records().end(),
            [&](const auto& record) { return record.id == id; });
    if (found == library.records().end()) {
        return;
    }
    if (engine.isReferenceAuditioningFile(found->audioFile.getFullPathName())) {
        engine.stopReferenceAudition();
        return;
    }
    PitchedSample sample;
    if (sample.load(found->audioFile.getFullPathName()) < 0
            || !engine.startReferenceAudition(sample)) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Cannot audition audio", "Check that the file is available and an audio output device is enabled.");
    }
}

juce::String ReferenceAssetBrowser::dragDescription(const juce::String& id) {
    return dragPrefix + id;
}

juce::String ReferenceAssetBrowser::referenceIdFromDrag(const juce::var& description) {
    const auto text = description.toString();
    return text.startsWith(dragPrefix) ? text.substring(dragPrefix.length()) : juce::String();
}

std::vector<std::pair<juce::String, juce::Rectangle<float>>>
ReferenceAssetBrowser::pointerTargetsForAutomation() const {
    auto targets = list->targets();
    for (const auto& [id, bounds] : toolbar.pointerTargetsForAutomation()) {
        if (id.isNotEmpty()) {
            targets.push_back({ id, bounds.translated((float) toolbar.getX(), (float) toolbar.getY()) });
        }
    }
    return targets;
}

void ReferenceAssetBrowser::resized() {
    auto bounds = getLocalBounds();
    toolbar.setBounds(bounds.removeFromTop(SidebarLibraryToolbar::height));
    viewport.setBounds(bounds.reduced(6, 0));
    list->setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()), list->getHeight());
}

}
