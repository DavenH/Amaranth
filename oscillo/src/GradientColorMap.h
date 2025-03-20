#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

class GradientColourMap {
public:
    enum class Palette {
        Viridis, Inferno, Bipolar
    };

    GradientColourMap(int numColors, Palette type = Palette::Viridis)
        : k(numColors) {
        generateGradient(type);
    }

    [[nodiscard]] Colour getColour(int index) const {
        jassert(index >= 0 && index < k);
        return colours[index];
    }

    [[nodiscard]] const std::vector<Colour>& getColours() const { return colours; }
    [[nodiscard]] int size() const { return k; }

private:
    void generateGradient(Palette type) {
        colours.resize(k);
        ColourGradient gradient;
        gradient.clearColours();

        switch (type) {
            case Palette::Viridis:
                gradient.addColour(0.0, Colour::fromRGB(68, 1, 84));     // Dark purple
                gradient.addColour(0.2, Colour::fromRGB(72, 40, 120));   // Purple
                gradient.addColour(0.4, Colour::fromRGB(62, 83, 160));   // Blue
                gradient.addColour(0.6, Colour::fromRGB(49, 104, 142));  // Light blue
                gradient.addColour(0.8, Colour::fromRGB(53, 183, 121));  // Green
                gradient.addColour(1.0, Colour::fromRGB(253, 231, 37));  // Yellow
                break;

            case Palette::Inferno:
                gradient.addColour(0.0, Colour::fromRGB(0, 0, 4));       // Black
                gradient.addColour(0.2, Colour::fromRGB(40, 11, 84));    // Deep purple
                gradient.addColour(0.4, Colour::fromRGB(101, 21, 110));  // Magenta
                gradient.addColour(0.6, Colour::fromRGB(159, 42, 99));   // Pink
                gradient.addColour(0.8, Colour::fromRGB(229, 107, 36));  // Orange
                gradient.addColour(1.0, Colour::fromRGB(252, 255, 164)); // Light yellow
                break;

            case Palette::Bipolar:
                gradient.addColour(0.0, Colour::fromRGB(255, 0, 0));     // Deep red
                gradient.addColour(0.25, Colour::fromRGB(180, 100, 100));     // Deep red
                gradient.addColour(0.5, Colour::fromRGB(100, 100, 100));       // Black (zero)
                gradient.addColour(0.75, Colour::fromRGB(100, 100, 180));       // Black (zero)
                gradient.addColour(1.0, Colour::fromRGB(0, 0, 255));     // Deep blue
                break;
        }

        // Use the gradient's getColourAtPosition to populate our colour array
        for (int i = 0; i < k; ++i) {
            float position = i / static_cast<float>(k - 1);
            colours[i] = gradient.getColourAtPosition(position);
        }
    }

    int k;
    std::vector<Colour> colours;
};
