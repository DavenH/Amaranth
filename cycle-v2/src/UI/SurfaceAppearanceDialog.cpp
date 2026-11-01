#include "UI/SurfaceAppearanceDialog.h"

#include <UI/Panels/TimeSurfaceStyles.h>
#include <algorithm>

#include "UI/CanvasChromePalette.h"
#include "UI/NodeWorkspace.h"
#include "UI/WaveformColourStyles.h"

namespace CycleV2 {

namespace {

struct Choice {
    juce::String id;
    juce::String label;
    juce::Image gradient;
};

void paintChoice(juce::Graphics& graphics, juce::Rectangle<int> bounds, const Choice& choice) {
    auto content = bounds.reduced(12, 0);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(13.f);
    graphics.drawText(choice.label, content.removeFromLeft(180), juce::Justification::centredLeft);
    content.removeFromRight(22);
    const auto strip = content.withSizeKeepingCentre(content.getWidth(), 16);
    graphics.drawImage(choice.gradient, strip.toFloat(), juce::RectanglePlacement::stretchToFit);
    graphics.setColour(CanvasChromePalette::border);
    graphics.drawRect(strip);
}

class GradientMenuItem final : public juce::PopupMenu::CustomComponent {
public:
    GradientMenuItem(Choice choiceToUse, bool selectedToUse) :
            CustomComponent(true)
        ,   choice(std::move(choiceToUse))
        ,   selected(selectedToUse) {}

    void getIdealSize(int& width, int& height) override {
        width = 430;
        height = 34;
    }

    void paint(juce::Graphics& graphics) override {
        graphics.fillAll(isItemHighlighted() ? CanvasChromePalette::raisedSurface : CanvasChromePalette::surface);
        paintChoice(graphics, getLocalBounds().withTrimmedLeft(20), choice);
        if (selected) {
            graphics.setColour(CanvasChromePalette::text);
            graphics.setFont(14.f);
            graphics.drawText(juce::String::charToString(0x2713), juce::Rectangle<int>(5, 0, 22, getHeight()), juce::Justification::centred);
        }
    }

private:
    Choice choice;
    bool selected {};
};

Choice timeChoice(ScalarSurfaceTimeStyle style) {
    const auto* entry = TimeSurfaceStyles::find(TimeSurfaceStyles::id(style));
    return { entry->id, entry->label, ScalarSurfaceMaterialEvaluator::createGradientImage(
            ScalarSurfaceMaterial::forTimeStyle(style)) };
}

Choice blueAmberChoice() {
    return { "default", "Blue / Amber", ScalarSurfaceMaterialEvaluator::createGradientImage(
            ScalarSurfaceMaterial::defaultBipolarMagnitude()) };
}

Choice waveformChoice(WaveformColourStyles::Style style) {
    const auto* entry = WaveformColourStyles::find(WaveformColourStyles::id(style));
    return { entry->id, entry->label, WaveformColourStyles::gradient(style) };
}

}

class SurfaceAppearanceDialog::Selector final : public juce::Button {
public:
    Selector(juce::String name, std::vector<Choice> options, Choice selectedChoice,
            std::function<void(const juce::String&)> change) :
            Button(name)
        ,   choices(std::move(options))
        ,   selected(std::move(selectedChoice))
        ,   onChange(std::move(change)) {
        setName(name);
        setTitle(name.fromLastOccurrenceOf(".", false, false) + " colours");
        setWantsKeyboardFocus(choices.size() > 1);
        setDescription("Selected: " + selected.label);
        onClick = [this] { showChoices(); };
    }

    bool selectChoice(const juce::String& id) {
        const auto found = std::find_if(choices.begin(), choices.end(), [&](const auto& choice) { return choice.id == id; });
        if (found == choices.end()) {
            return false;
        }
        selected = *found;
        setDescription("Selected: " + found->label);
        if (onChange) {
            onChange(selected.id);
        }
        repaint();
        return true;
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey
                || key == juce::KeyPress::downKey) {
            showChoices();
            return true;
        }
        return Button::keyPressed(key);
    }

    void paintButton(juce::Graphics& graphics, bool over, bool down) override {
        const bool interactive = choices.size() > 1;
        graphics.fillAll(interactive && (over || down)
                ? CanvasChromePalette::hoveredControlSurface : CanvasChromePalette::restingControlSurface);
        paintChoice(graphics, getLocalBounds(), selected);
        if (!interactive) {
            return;
        }
        graphics.setColour(hasKeyboardFocus(true) ? CanvasChromePalette::focus : CanvasChromePalette::border);
        graphics.drawRect(getLocalBounds());
        juce::Path arrow;
        const float x = getWidth() - 17.f;
        const float y = getHeight() * 0.5f;
        arrow.addTriangle(x - 4.f, y - 2.f, x + 4.f, y - 2.f, x, y + 3.f);
        graphics.fillPath(arrow);
    }

private:
    void showChoices() {
        if (choices.size() <= 1) {
            return;
        }
        juce::PopupMenu menu;
        for (size_t index = 0; index < choices.size(); ++index) {
            const auto& choice = choices[index];
            menu.addCustomItem((int) index + 1,
                    std::make_unique<GradientMenuItem>(choice, choice.id == selected.id), nullptr, choice.label);
        }
        const auto screen = getScreenBounds();
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                .withTargetScreenArea(screen).withMinimumWidth(getWidth()),
                [safe = juce::Component::SafePointer<Selector>(this)](int result) {
                    if (safe != nullptr && result > 0 && result <= (int) safe->choices.size()) {
                        safe->selectChoice(safe->choices[(size_t) result - 1].id);
                    }
                });
    }

    std::vector<Choice> choices;
    Choice selected;
    std::function<void(const juce::String&)> onChange;
};

SurfaceAppearanceDialog::SurfaceAppearanceDialog(NodeWorkspace& workspace) {
    setName("surfaceAppearance");
    setWantsKeyboardFocus(true);
    const auto owner = juce::Component::SafePointer<NodeWorkspace>(&workspace);
    time = std::make_unique<Selector>("surfaceAppearance.time",
            std::vector<Choice> { timeChoice(ScalarSurfaceTimeStyle::BlueDepth),
                    timeChoice(ScalarSurfaceTimeStyle::IcyHotRaw), timeChoice(ScalarSurfaceTimeStyle::Greyscale) },
            timeChoice(workspace.timeSurfaceStyle()), [owner](const auto& id) {
                if (owner != nullptr) {
                    owner->setTimeSurfaceStyle(TimeSurfaceStyles::fromId(id));
                }
            });
    const auto spectralStyle = workspace.bipolarSpectralSurfaceStyle();
    spectral = std::make_unique<Selector>("surfaceAppearance.spectral",
            std::vector<Choice> { timeChoice(ScalarSurfaceTimeStyle::IcyHotRaw), blueAmberChoice() },
            spectralStyle.has_value() ? timeChoice(*spectralStyle) : blueAmberChoice(), [owner](const auto& id) {
                if (owner != nullptr) {
                    const auto* entry = TimeSurfaceStyles::find(id);
                    owner->setBipolarSpectralSurfaceStyle(entry != nullptr
                            ? std::optional<ScalarSurfaceTimeStyle>(entry->style) : std::nullopt);
                }
            });
    using WaveformStyle = WaveformColourStyles::Style;
    waveform = std::make_unique<Selector>("surfaceAppearance.waveform",
            std::vector<Choice> { waveformChoice(WaveformStyle::BurntAlum),
                    waveformChoice(WaveformStyle::Blues), waveformChoice(WaveformStyle::Greyscale) },
            waveformChoice(workspace.waveformColourStyle()), [owner](const auto& id) {
                if (owner != nullptr) {
                    owner->setWaveformColourStyle(WaveformColourStyles::fromId(id));
                }
            });
    const Choice magnitude { "burnt-alum", "Burnt Alum", ScalarSurfaceMaterialEvaluator::createGradientImage(
            ScalarSurfaceMaterial::unipolarMagnitude()) };
    magnitudeFixed = std::make_unique<Selector>("surfaceAppearance.magnitude",
            std::vector<Choice> { magnitude }, magnitude, nullptr);
    const Choice phase { "phase", "Orange / Violet", ScalarSurfaceMaterialEvaluator::createGradientImage(
            ScalarSurfaceMaterial::bipolarPhase()) };
    phaseFixed = std::make_unique<Selector>("surfaceAppearance.phase",
            std::vector<Choice> { phase }, phase, nullptr);
    for (auto* selector : { time.get(), spectral.get(), magnitudeFixed.get(), phaseFixed.get(), waveform.get() }) {
        addAndMakeVisible(*selector);
    }
    done.setName("surfaceAppearance.done");
    done.onClick = [this] { close(); };
    addAndMakeVisible(done);
    setSize(620, 490);
}

bool SurfaceAppearanceDialog::selectStyle(const juce::String& section, const juce::String& styleId) {
    for (auto* selector : { time.get(), spectral.get(), waveform.get() }) {
        if (selector->getName() == "surfaceAppearance." + section) {
            return selector->selectChoice(styleId);
        }
    }
    return false;
}

SurfaceAppearanceDialog::~SurfaceAppearanceDialog() = default;

void SurfaceAppearanceDialog::paint(juce::Graphics& graphics) {
    graphics.fillAll(CanvasChromePalette::surface);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(14.f);
    const char* labels[] { "Time", "Bipolar magnitude", "Unipolar magnitude", "Bipolar phase", "Waveform density" };
    for (int index = 0; index < 5; ++index) {
        graphics.drawText(labels[index], 24, 18 + index * 84, getWidth() - 48, 20, juce::Justification::centredLeft);
    }
}

void SurfaceAppearanceDialog::resized() {
    int y = 42;
    for (auto* selector : { time.get(), spectral.get(), magnitudeFixed.get(), phaseFixed.get(), waveform.get() }) {
        selector->setBounds(24, y, getWidth() - 48, 38);
        y += 84;
    }
    done.setBounds(getWidth() - 104, getHeight() - 44, 80, 28);
}

bool SurfaceAppearanceDialog::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey) {
        close();
        return true;
    }
    return false;
}

void SurfaceAppearanceDialog::close() {
    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>()) {
        dialog->exitModalState(0);
    }
}

}
