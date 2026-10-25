#include "UI/ReferenceAssetBrowser.h"

#include "App/StandaloneAudioEngine.h"
#include "UI/CanvasChromePalette.h"
#include "UI/SidebarMediaRow.h"
#include "UI/LibraryIconRenderer.h"
#include "UI/ReferenceWaveformPreview.h"
#include "UI/LibraryThumbnailCache.h"
#include "Runtime/PerformanceDistribution.h"

namespace CycleV2 {

namespace {

constexpr int rowHeight = 82;
const juce::String dragPrefix { "cycle-reference:" };

juce::Rectangle<float> playBounds(juce::Rectangle<float> row) {
    return SidebarMediaRow::cardBounds(row).reduced(5.f)
            .removeFromTop(24.f).removeFromRight(26.f);
}

}

class ReferenceAssetBrowser::List final : public juce::Component {
public:
    explicit List(ReferenceAssetBrowser& browser) : owner(browser) {
        setWantsKeyboardFocus(true);
        setComponentID("workspace.sidebar.assetList");
        images.setReadyCallback([safeThis = juce::Component::SafePointer<List>(this)] {
            if (safeThis != nullptr) {
                safeThis->repaint();
            }
        });
    }

    void refresh() {
        entries.clear();
        for (const auto& record : owner.library.records()) {
            entries.push_back({ record, record.audioFile.getLastModificationTime().toMilliseconds() });
        }
        filter();
    }

    void moved() override { preload(); }
    void resized() override { preload(); }

    void preload() {
        if (!owner.isShowing() || visible.empty()) {
            return;
        }
        const int first = owner.viewport.getViewPositionY() / rowHeight;
        const int page = juce::jmax(1, (owner.viewport.getViewHeight() + rowHeight - 1) / rowHeight);
        const auto request = [&](int start, int end) {
            for (int index = juce::jmax(0, start); index < juce::jmin((int) visible.size(), end); ++index) {
                const auto& entry = entries[visible[(size_t) index]];
                images.imageFor(entry.record.audioFile, entry.modificationTime);
            }
        };
        request(first, first + page + 1);
        request(first + page + 1, first + 2 * page + 1);
        request(first - page, first);
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
        preload();
        repaint();
    }

    void paint(juce::Graphics& graphics) override {
        const double started = juce::Time::getMillisecondCounterHiRes();
        const auto loadsBeforePaint = images.loadCount();
        if (visible.empty()) {
            graphics.setColour(CanvasChromePalette::mutedText);
            graphics.setFont(13.f);
            graphics.drawText("No matching audio assets", getLocalBounds().reduced(8),
                    juce::Justification::centredTop);
        }
        const auto clip = graphics.getClipBounds();
        const int first = juce::jmax(0, clip.getY() / rowHeight);
        const int last = juce::jmin((int) visible.size(), (clip.getBottom() + rowHeight - 1) / rowHeight);
        for (int index = first; index < last; ++index) {
            ++rowVisits;
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
            const auto image = images.cachedImageFor(entry.record.audioFile, entry.modificationTime);
            if (image.isValid()) {
                ++imageDraws;
                graphics.drawImage(image, preview.reduced(3.f), juce::RectanglePlacement::stretchToFit);
            }
        }
        paintDecodes += images.loadCount() - loadsBeforePaint;
        recordPerformanceSample(paintDuration, (uint64_t)
                ((juce::Time::getMillisecondCounterHiRes() - started) * 1000.0));
    }

    juce::var diagnostics() const {
        auto result = std::make_unique<juce::DynamicObject>();
        result->setProperty("paintDuration", performanceDistributionToVar(paintDuration));
        result->setProperty("rowVisits", (juce::int64) rowVisits);
        result->setProperty("thumbnailDraws", 0);
        result->setProperty("imageDraws", (juce::int64) imageDraws);
        result->setProperty("paintDecodes", (juce::int64) paintDecodes);
        result->setProperty("imageBuilds", (juce::int64) images.loadCount());
        result->setProperty("cachedImages", (int) images.cachedImageCount());
        result->setProperty("visibleRows", juce::jmin((int) visible.size(),
                (owner.viewport.getViewHeight() + rowHeight - 1) / rowHeight + 1));
        result->setProperty("rowCount", (int) visible.size());
        result->setProperty("viewY", owner.viewport.getViewPositionY());
        result->setProperty("viewHeight", owner.viewport.getViewHeight());
        result->setProperty("pending", (int) images.pendingCount());
        return { result.release() };
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
        juce::int64 modificationTime {};
    };

    juce::Rectangle<float> row(int index) const {
        return { 0.f, (float) (index * rowHeight), (float) getWidth(), (float) rowHeight };
    }

    ReferenceAssetBrowser& owner;
    LibraryThumbnailCache images { loadReferenceWaveformHeatmap, 128 };
    std::vector<Entry> entries;
    std::vector<size_t> visible;
    PerformanceDistribution paintDuration;
    uint64_t rowVisits {};
    uint64_t imageDraws {};
    uint64_t paintDecodes {};
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

juce::var ReferenceAssetBrowser::previewDiagnostics() const { return list->diagnostics(); }

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
    targets.emplace_back("workspace.sidebar.assetList", viewport.getBounds().toFloat());
    for (const auto& [id, bounds] : toolbar.pointerTargetsForAutomation()) {
        if (id.isNotEmpty()) {
            targets.push_back({ id, bounds.translated((float) toolbar.getX(), (float) toolbar.getY()) });
        }
    }
    return targets;
}

void ReferenceAssetBrowser::visibilityChanged() { list->preload(); }

void ReferenceAssetBrowser::resized() {
    auto bounds = getLocalBounds();
    toolbar.setBounds(bounds.removeFromTop(SidebarLibraryToolbar::height));
    viewport.setBounds(bounds.reduced(6, 0));
    list->setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()), list->getHeight());
    list->preload();
}

}
