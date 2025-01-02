#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

class GradientColourMap {
public:
    enum class Palette {
        Viridis, Inferno
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

        // Base colors for each palette type
        std::array<juce::Colour, 6> baseColours;

        switch (type) {
            case Palette::Viridis:
                baseColours = {
                    Colour::fromRGB(68, 1, 84),    // Dark purple
                    Colour::fromRGB(72, 40, 120),  // Purple
                    Colour::fromRGB(62, 83, 160),  // Blue
                    Colour::fromRGB(49, 104, 142), // Light blue
                    Colour::fromRGB(53, 183, 121), // Green
                    ::Colour::fromRGB(253, 231, 37)  // Yellow
                };
                break;

            case Palette::Inferno:
                baseColours = {
                    Colour::fromRGB(0, 0, 4),      // Black
                    Colour::fromRGB(40, 11, 84),   // Deep purple
                    Colour::fromRGB(101, 21, 110), // Magenta
                    Colour::fromRGB(159, 42, 99),  // Pink
                    Colour::fromRGB(229, 107, 36), // Orange
                    Colour::fromRGB(252, 255, 164) // Light yellow
                };
                break;
        }

        for (int i = 0; i < k; ++i) {
            float position  = i / static_cast<float>(k - 1);
            float baseIndex = position * (baseColours.size() - 1);
            int index1      = static_cast<int>(baseIndex);
            int index2      = std::min(index1 + 1, static_cast<int>(baseColours.size() - 1));
            float alpha     = baseIndex - index1;

            colours[i] = baseColours[index1].interpolatedWith(baseColours[index2], alpha);
        }
    }

    int k;
    std::vector<Colour> colours;
};

// Example usage:
/*
    // Create a viridis gradient with 10 colors
    GradientColourMap viridis(10, GradientColourMap::Palette::Viridis);
    
    // Create an inferno gradient with 5 colors
    GradientColourMap inferno(5, GradientColourMap::Palette::Inferno);
    
    // Get a specific color
    juce::Colour color = viridis.getColour(3);
    
    // Get all colors
    auto allColors = viridis.getColours();
    
    // Use in painting
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        float width = bounds.getWidth() / static_cast<float>(viridis.size());
        
        for (int i = 0; i < viridis.size(); ++i)
        {
            g.setColour(viridis.getColour(i));
            g.fillRect(bounds.getX() + i * width, bounds.getY(), 
                      width, bounds.getHeight());
        }
    }
*/