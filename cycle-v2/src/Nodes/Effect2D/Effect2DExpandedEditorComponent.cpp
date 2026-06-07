#include "Effect2DExpandedEditorComponent.h"

#include <utility>

namespace CycleV2 {

namespace {

const Colour kText      { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kHeaderHeight = 34.f;

}

Effect2DExpandedEditorComponent::Effect2DExpandedEditorComponent(Effect2DWidget& targetWidget) :
        widget(targetWidget) {
    setOpaque(false);
    setInterceptsMouseClicks(true, true);
    addAndMakeVisible(controls.enabled);
    addAndMakeVisible(controls.firstSlider);
    addAndMakeVisible(controls.secondSlider);
    addAndMakeVisible(controls.thirdSlider);
    addAndMakeVisible(controls.menu);
    addAndMakeVisible(controls.firstButton);
    addAndMakeVisible(controls.secondButton);
    addAndMakeVisible(controls.thirdButton);
    addAndMakeVisible(controls.firstLabel);
    addAndMakeVisible(controls.secondLabel);
    addAndMakeVisible(controls.thirdLabel);
    addAndMakeVisible(controls.menuLabel);
}

Effect2DExpandedEditorComponent::~Effect2DExpandedEditorComponent() = default;

void Effect2DExpandedEditorComponent::setCallbacks(Callbacks nextCallbacks) {
    callbacks = std::move(nextCallbacks);

    auto safeThis = Component::SafePointer<Effect2DExpandedEditorComponent>(this);
    widget.setExpandedPanelCallbacks(
            [safeThis] {
                if (safeThis != nullptr) {
                    safeThis->repaint();

                    if (safeThis->callbacks.repaintOpenGL != nullptr) {
                        safeThis->callbacks.repaintOpenGL();
                    }
                }
            },
            [safeThis](const MouseCursor& cursor) {
                if (safeThis != nullptr) {
                    safeThis->setMouseCursor(cursor);
                }
            });
}

void Effect2DExpandedEditorComponent::setNode(const Node& nextNode) {
    node = nextNode;
    if (configuredKind != node.kind) {
        configureControls();
    }
    updatePanelHost();
    updateControlLayout();
    repaint();
}

void Effect2DExpandedEditorComponent::renderOpenGL(float scaleFactor) {
    if (node.kind != NodeKind::GuideCurve
            && node.kind != NodeKind::ImpulseResponse
            && node.kind != NodeKind::Waveshaper) {
        return;
    }

    widget.renderExpandedPanelOpenGL(
            node,
            panelBounds().translated((float) getX(), (float) getY()),
            scaleFactor);
}

void Effect2DExpandedEditorComponent::paint(Graphics& g) {
    Rectangle<float> panel = getLocalBounds().toFloat();
    const Rectangle<int> panelHole = panelBounds().toNearestInt();

    g.saveState();
    g.excludeClipRegion(panelHole);
    g.setColour(Colours::black.withAlpha(0.38f));
    g.fillRoundedRectangle(panel.translated(0.f, 10.f), 8.f);
    g.setColour(Colour(0xff141a21));
    g.fillRoundedRectangle(panel, 8.f);
    g.restoreState();

    g.setColour(Colour(0xff17d0c5).withAlpha(0.72f));
    g.drawRoundedRectangle(panel, 8.f, 1.5f);

    auto header = panel.removeFromTop(kHeaderHeight);
    g.setColour(Colour(0xff202833));
    g.fillRoundedRectangle(header, 8.f);
    g.fillRect(header.withTrimmedTop(header.getHeight() - 8.f));

    g.setColour(kText);
    g.setFont(FontOptions(14.f, Font::bold));
    g.drawText(node.title, header.reduced(13.f, 4.f), Justification::centredLeft);

    g.setColour(kMutedText);
    g.setFont(FontOptions(10.f));
    g.drawText(subtitleForNode(), header.reduced(13.f, 4.f), Justification::centredRight);

    Rectangle<float> closeButton = closeButtonBounds();
    g.setColour(Colour(0xff0e1318));
    g.fillEllipse(closeButton);
    g.setColour(Colour(0xff354050));
    g.drawEllipse(closeButton, 1.f);
    g.setColour(kText);
    g.drawLine(closeButton.getX() + 7.f, closeButton.getY() + 7.f,
               closeButton.getRight() - 7.f, closeButton.getBottom() - 7.f, 1.4f);
    g.drawLine(closeButton.getRight() - 7.f, closeButton.getY() + 7.f,
               closeButton.getX() + 7.f, closeButton.getBottom() - 7.f, 1.4f);
}

void Effect2DExpandedEditorComponent::resized() {
    updatePanelHost();
    updateControlLayout();
}

void Effect2DExpandedEditorComponent::mouseMove(const MouseEvent& event) {
    setMouseCursor(closeButtonBounds().contains(event.position)
            ? MouseCursor::PointingHandCursor
            : MouseCursor::NormalCursor);
}

void Effect2DExpandedEditorComponent::mouseDown(const MouseEvent& event) {
    if (closeButtonBounds().contains(event.position) && callbacks.close != nullptr) {
        callbacks.close();
    }
}

Rectangle<float> Effect2DExpandedEditorComponent::closeButtonBounds() const {
    const Rectangle<float> bounds = getLocalBounds().toFloat();
    return Rectangle<float>(22.f, 22.f).withCentre({ bounds.getRight() - 22.f, kHeaderHeight * 0.5f });
}

Rectangle<float> Effect2DExpandedEditorComponent::contentBounds() const {
    Rectangle<float> bounds = getLocalBounds().toFloat();
    bounds.removeFromTop(kHeaderHeight);
    return bounds.reduced(12.f, 10.f);
}

Rectangle<float> Effect2DExpandedEditorComponent::panelBounds() const {
    Rectangle<float> bounds = contentBounds().withTrimmedBottom(82.f);

    if (node.kind == NodeKind::Waveshaper) {
        const float size = jmin(bounds.getWidth(), bounds.getHeight());
        return Rectangle<float>(size, size).withCentre(bounds.getCentre());
    }

    return bounds;
}

Rectangle<int> Effect2DExpandedEditorComponent::controlBounds() const {
    Rectangle<float> bounds = contentBounds();
    bounds.removeFromTop(panelBounds().getBottom() - bounds.getY() + 12.f);
    return bounds.toNearestInt();
}

String Effect2DExpandedEditorComponent::subtitleForNode() const {
    if (node.kind == NodeKind::GuideCurve) {
        return "Guide Panel";
    }

    if (node.kind == NodeKind::ImpulseResponse) {
        return "IR Modeller";
    }

    if (node.kind == NodeKind::Waveshaper) {
        return "Waveshaper";
    }

    return {};
}

void Effect2DExpandedEditorComponent::configureControls() {
    configuredKind = node.kind;
    controls.enabled.setButtonText("Enable");
    controls.enabled.setToggleState(true, dontSendNotification);
    controls.enabled.setVisible(true);
    controls.enabled.onClick = [this] {
        repaint();
    };

    controls.firstSlider.setVisible(false);
    controls.secondSlider.setVisible(false);
    controls.thirdSlider.setVisible(false);
    controls.firstLabel.setVisible(false);
    controls.secondLabel.setVisible(false);
    controls.thirdLabel.setVisible(false);
    controls.menu.setVisible(false);
    controls.menuLabel.setVisible(false);
    controls.firstButton.setVisible(false);
    controls.secondButton.setVisible(false);
    controls.thirdButton.setVisible(false);

    if (node.kind == NodeKind::Waveshaper) {
        styleSlider(controls.firstSlider, controls.firstLabel, "Pre");
        styleSlider(controls.secondSlider, controls.secondLabel, "Post");
        controls.thirdSlider.setVisible(false);
        controls.thirdLabel.setVisible(false);
        styleMenu({ "1", "2", "4", "8" }, "1", "AA factor");
        controls.firstButton.setVisible(false);
        controls.secondButton.setVisible(false);
        controls.thirdButton.setVisible(false);
    } else if (node.kind == NodeKind::ImpulseResponse) {
        styleSlider(controls.firstSlider, controls.firstLabel, "Size");
        styleSlider(controls.secondSlider, controls.secondLabel, "Post");
        styleSlider(controls.thirdSlider, controls.thirdLabel, "HighPass");
        styleMenu({}, {}, {});
        styleButton(controls.firstButton, "Load");
        styleButton(controls.secondButton, "Unload");
        styleButton(controls.thirdButton, "Model");
    } else if (node.kind == NodeKind::GuideCurve) {
        styleSlider(controls.firstSlider, controls.firstLabel, "Noise");
        styleSlider(controls.secondSlider, controls.secondLabel, "DC Offset");
        styleSlider(controls.thirdSlider, controls.thirdLabel, "Phase");
        styleMenu({}, {}, {});
        styleButton(controls.firstButton, "+");
        styleButton(controls.secondButton, "-");
        controls.thirdButton.setVisible(false);
    }
}

void Effect2DExpandedEditorComponent::styleSlider(Slider& slider, Label& label, const String& text) {
    label.setText(text, dontSendNotification);
    label.setColour(Label::textColourId, kMutedText);
    label.setJustificationType(Justification::centred);
    label.setVisible(true);

    slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
    slider.setRange(0.0, 1.0, 0.001);
    if (slider.getValue() <= slider.getMinimum() || slider.getValue() >= slider.getMaximum()) {
        slider.setValue(0.5, dontSendNotification);
    }
    slider.setColour(Slider::rotarySliderOutlineColourId, Colour(0xff4a5361));
    slider.setColour(Slider::rotarySliderFillColourId, Colour(0xffd9dde5));
    slider.setColour(Slider::thumbColourId, Colour(0xfff2f4f7));
    slider.setVisible(true);
}

void Effect2DExpandedEditorComponent::styleButton(Button& button, const String& text) {
    button.setButtonText(text);
    button.setColour(TextButton::buttonColourId, Colour(0xff161d25));
    button.setColour(TextButton::buttonOnColourId, Colour(0xff252f3b));
    button.setColour(TextButton::textColourOffId, kText);
    button.setVisible(true);
}

void Effect2DExpandedEditorComponent::styleMenu(
        const StringArray& items,
        const String& selected,
        const String& labelText) {
    controls.menu.clear(dontSendNotification);
    controls.menuLabel.setText(labelText, dontSendNotification);
    controls.menuLabel.setColour(Label::textColourId, kMutedText);
    controls.menuLabel.setJustificationType(Justification::centred);

    for (int i = 0; i < items.size(); ++i) {
        controls.menu.addItem(items[i], i + 1);
        if (items[i] == selected) {
            controls.menu.setSelectedId(i + 1, dontSendNotification);
        }
    }

    controls.menu.setVisible(!items.isEmpty());
    controls.menuLabel.setVisible(!items.isEmpty());
}

void Effect2DExpandedEditorComponent::updatePanelHost() {
    if (getWidth() <= 0 || getHeight() <= 0) {
        return;
    }

    Component* panel = widget.getExpandedPanelComponentIfCreated();

    if (panel == nullptr) {
        panel = widget.prepareExpandedPanelComponent(node, contentBounds());
    }

    if (panel == nullptr) {
        return;
    }

    const Rectangle<int> bounds = panelBounds().toNearestInt();

    if (panel->getParentComponent() != this) {
        addAndMakeVisible(panel);
    }

    if (panel->getBounds() != bounds) {
        panel->setBounds(bounds);
    }

    panel->setVisible(true);
    panel->toFront(false);
}

void Effect2DExpandedEditorComponent::updateControlLayout() {
    Rectangle<int> bounds = controlBounds();
    bounds.reduce(2, 0);

    controls.enabled.setBounds(bounds.removeFromLeft(76));
    bounds.removeFromLeft(10);

    auto placeSlider = [&](Slider& slider, Label& label) {
        Rectangle<int> item = bounds.removeFromLeft(78);
        slider.setBounds(item.removeFromTop(54));
        label.setBounds(item);
        bounds.removeFromLeft(8);
    };

    placeSlider(controls.firstSlider, controls.firstLabel);
    placeSlider(controls.secondSlider, controls.secondLabel);

    if (controls.thirdSlider.isVisible()) {
        placeSlider(controls.thirdSlider, controls.thirdLabel);
    }

    if (controls.menu.isVisible()) {
        Rectangle<int> menuArea = bounds.removeFromLeft(104);
        controls.menu.setBounds(menuArea.removeFromTop(42).reduced(4, 3));
        controls.menuLabel.setBounds(menuArea);
        bounds.removeFromLeft(6);
    }

    auto placeButton = [&](Button& button) {
        if (!button.isVisible()) {
            return;
        }

        button.setBounds(bounds.removeFromLeft(66).reduced(3, 10));
        bounds.removeFromLeft(4);
    };

    placeButton(controls.firstButton);
    placeButton(controls.secondButton);
    placeButton(controls.thirdButton);
}

}
