#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

#include "Nodes/Envelope/EnvelopePurpose.h"

namespace CycleV2 {

class EnvelopePurposeSelector final : public Component {
public:
    EnvelopePurposeSelector();
    ~EnvelopePurposeSelector() override;

    void setPurpose(
            EnvelopePurpose purpose,
            NotificationType notification = dontSendNotification);
    EnvelopePurpose purpose() const { return selectedPurpose; }
    Rectangle<float> optionBounds(EnvelopePurpose purpose) const;
    bool isOptionHovered(EnvelopePurpose purpose) const;

    void paint(Graphics& graphics) override;
    void resized() override;

    std::function<void(EnvelopePurpose)> onChange;

private:
    class PurposeButton;

    PurposeButton* buttonFor(EnvelopePurpose purpose) const;

    EnvelopePurpose selectedPurpose { EnvelopePurpose::Control };
    std::vector<std::unique_ptr<PurposeButton>> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopePurposeSelector)
};

}
