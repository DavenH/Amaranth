#pragma once

#ifdef BUILD_TESTING

#include <JuceHeader.h>
#include "../src/Array/Buffer.h"
using namespace juce;

class SignalDebugger {
public:
    // Configuration struct for plot dimensions and styling
    struct PlotConfig {
        int width                 = 1280;
        int height                = 1280;
        int marginLeft            = 60; // Increased for y-axis labels
        int marginRight           = 90; // Increased for value labels
        int marginTop             = 30; // Increased for title
        int marginBottom          = 40; // Increased for x-axis labels
        float gridDivisions       = 10.0f;
        float labelFontSize       = 14.0f;
        float signalLineThickness = 2.0f;
        float valuePadding        = 0.1f; // 10% padding around min/max values

        int numXTicks = 10;  // Number of ticks on x-axis
        int numYTicks = 8;   // Number of ticks on y-axis
        float tickLength = 5.0f;  // Length of tick marks in pixels
        float tickLabelPadding = 5.0f;  // Space between tick and label
    };

    static SignalDebugger& instance() {
        static SignalDebugger instance;
        return instance;
    }

    SignalDebugger() : workBuffer(1024 * 1024 * 5) {}

    struct PlotData {
        std::vector<Buffer<float>> gridData;
        PlotData() {}
    };

    void setConfig(const PlotConfig& newConfig) {
        config = newConfig;
    }

    void drawAxisTicks(Graphics& g, const Rectangle<int>& plotArea,
                       float minX, float maxX, float minY, float maxY) {
        g.setColour(Colours::black);
        g.setFont(config.labelFontSize);

        const float xTickStep = plotArea.getWidth() / (config.numXTicks - 1);
        const float timeStep  = (maxX - minX) / (config.numXTicks - 1.0f);

        for (int i = 0; i < config.numXTicks; ++i) {
            float x = plotArea.getX() + i * xTickStep;
            g.drawVerticalLine(roundToInt(x),
                               plotArea.getBottom(),
                               plotArea.getBottom() + config.tickLength);

            int sampleIndex = roundToInt(i * timeStep);
            String label = String(sampleIndex);
            g.drawText(label,
                       roundToInt(x - 25),
                       plotArea.getBottom() + config.tickLength + config.tickLabelPadding,
                       50, 20,
                       Justification::centred);
        }

        const float yTickStep = plotArea.getHeight() / (config.numYTicks - 1);
        const float freqStep  = (maxY - minY) / (config.numYTicks - 1.0f);

        for (int i = 0; i < config.numYTicks; ++i) {
            float y = plotArea.getY() + i * yTickStep;
            g.drawHorizontalLine(roundToInt(y),
                                 plotArea.getX() - config.tickLength,
                                 plotArea.getX());

            int freqIndex = (maxY) - 1 - roundToInt(i * freqStep);
            String label  = String(freqIndex);
            g.drawText(label,
                       plotArea.getX() - 50 - config.tickLabelPadding,
                       roundToInt(y - 10),
                       45, 20,
                       Justification::right);
        }
    }

    void plotSignal(
        const Buffer<float>& data,
        const String& label) {
        // Create image and graphics context
        Image image(Image::RGB, config.width, config.height, true);
        Graphics g(image);

        // Fill background
        g.fillAll(Colours::white);

        // Calculate plot area dimensions
        const Rectangle plotArea(
            config.marginLeft,
            config.marginTop,
            config.width - config.marginLeft - config.marginRight,
            config.height - config.marginTop - config.marginBottom
        );

        // Draw grid
        g.setColour(Colours::lightgrey);
        float gridStepX = plotArea.getWidth() / config.gridDivisions;
        float gridStepY = plotArea.getHeight() / config.gridDivisions;

        for (int i = 0; i <= config.gridDivisions; ++i) {
            float y = plotArea.getY() + i * gridStepY;
            float x = plotArea.getX() + i * gridStepX;
            g.drawHorizontalLine((int) y, plotArea.getX(), plotArea.getRight());
            g.drawVerticalLine((int) x, plotArea.getY(), plotArea.getBottom());
        }

        // Find min/max values
        float minVal = std::numeric_limits<float>::max();
        float maxVal = std::numeric_limits<float>::lowest();
        for (int i = 0; i < data.size(); ++i) {
            minVal = std::min(minVal, data[i]);
            maxVal = std::max(maxVal, data[i]);
        }

        // Add padding to range
        float range   = maxVal - minVal;
        float padding = range * config.valuePadding;
        float minV = minVal;
        float maxV = maxVal;
        minVal -= padding;
        maxVal += padding;
        range = maxVal - minVal;

        // Create path for the signal
        Path signalPath;
        float xScale = plotArea.getWidth() / (data.size() - 1);
        float yScale = plotArea.getHeight() / range;

        signalPath.startNewSubPath(plotArea.getX(),
                                   plotArea.getBottom() - (data[0] - minVal) * yScale);

        for (int i = 1; i < data.size(); ++i) {
            float x = plotArea.getX() + i * xScale;
            float y = plotArea.getBottom() - (data[i] - minVal) * yScale;
            signalPath.lineTo(x, y);
        }

        // Draw signal
        g.setColour(Colours::blue);
        g.strokePath(signalPath, PathStrokeType(config.signalLineThickness));

        // Draw plot border
        g.setColour(Colours::black);
        g.drawRect(plotArea, 1);

        // Add labels with proper positioning
        g.setFont(config.labelFontSize);
        g.setColour(Colours::black);

        // Value labels with proper alignment
        g.drawText(String(maxVal, 2),
                   plotArea.getRight(), plotArea.getY() - 20,
                   config.marginRight - 5, 20,
                   Justification::right);

        g.drawText(String(minVal, 2),
                   plotArea.getRight(), plotArea.getBottom(),
                   config.marginRight - 5, 20,
                   Justification::right);

        g.drawText(label,
                   plotArea.getX(), plotArea.getBottom() + 5,
                   plotArea.getWidth(), config.marginBottom - 10,
                   Justification::centred);

        drawAxisTicks(g, plotArea,
                      0, data.size(),   // X-axis range (time)
                      minVal, maxVal);

        auto timestamp = Time::currentTimeMillis();
        auto safeLabel = File::createLegalFileName(label);
        auto filename  = "lineplot_" + safeLabel + "_" + String(timestamp) + ".png";

        File outputDir(LIB_ROOT "/tests/debug_output");
        File outputFile = outputDir.getChildFile(filename);
        FileOutputStream stream(outputFile);

        if (stream.openedOk()) {
            PNGImageFormat pngWriter;
            pngWriter.writeImageToStream(image, stream);
        }
    }

    void plotHeatmap(const String& label) {
        PlotData& plotData = dataMap.getReference(label);
        vector<Buffer<float>>& gridData = plotData.gridData;

        if (plotData.gridData.empty()) {
            return;
        }

        Image image(Image::RGB, config.width, config.height, true);
        Graphics g(image);

        g.fillAll(Colours::white);

        const Rectangle<int> plotArea(
            config.marginLeft,
            config.marginTop,
            config.width - config.marginLeft - config.marginRight,
            config.height - config.marginTop - config.marginBottom
        );

        float minVal = std::numeric_limits<float>::max();
        float maxVal = std::numeric_limits<float>::lowest();

        for (const auto& buffer: gridData) {
            for (int i = 0; i < buffer.size(); ++i) {
                minVal = std::min(minVal, buffer[i]);
                maxVal = std::max(maxVal, buffer[i]);
            }
        }

        Image::BitmapData bitmap(image, Image::BitmapData::writeOnly);

        const int numTimeSteps = gridData.size();          // This will be the x-axis now
        const int numFrequencies = gridData[0].size();     // This will be the y-axis

        const float xScale = (float)plotArea.getWidth() / numTimeSteps;

        for (int x = 0; x < plotArea.getWidth(); ++x) {
            int timeIndex = (int)(x / xScale);
            timeIndex = jlimit(0, numTimeSteps - 1, timeIndex);

            const Buffer<float>& buffer = gridData[timeIndex];
            const float yScale = (float)plotArea.getHeight() / buffer.size();

            for (int y = 0; y < plotArea.getHeight(); ++y) {
                int freqIndex = buffer.size() - 1 - (int)(y / yScale);
                freqIndex = jlimit(0, buffer.size() - 1, freqIndex);

                float value = (buffer[freqIndex] - minVal) / (maxVal - minVal);

                uint8 r, g, b;
                if (value < 0.5f) {
                    float v = value * 2.0f;
                    r = g = static_cast<uint8>(v * 255.0f);
                    b = 255;
                } else {
                    float v = (value - 0.5f) * 2.0f;
                    r = 255;
                    g = b = static_cast<uint8>((1.0f - v) * 255.0f);
                }

                const int pixelX = plotArea.getX() + x;
                const int pixelY = plotArea.getY() + y;

                uint8* pixel = bitmap.getPixelPointer(pixelX, pixelY);
                pixel[0] = b;  // Blue
                pixel[1] = g;  // Green
                pixel[2] = r;  // Red
            }
        }

        // Draw border
        g.setColour(Colours::black);
        g.drawRect(plotArea, 1);

        // Add labels
        g.setFont(config.labelFontSize);

        // Draw colorbar
        const int colorbarWidth = 20;
        const Rectangle colorbar(
            plotArea.getRight() + 10,
            plotArea.getY(),
            colorbarWidth,
            plotArea.getHeight()
        );

        // Draw colorbar gradient
        for (int y = 0; y < colorbar.getHeight(); ++y) {
            float normalizedValue = 1.0f - (float) y / colorbar.getHeight();
            Colour cellColor;
            if (normalizedValue < 0.5f) {
                cellColor = Colour::fromRGB(
                    (uint8) (255 * (normalizedValue * 2)),
                    (uint8) (255 * (normalizedValue * 2)),
                    255
                );
            } else {
                cellColor = Colour::fromRGB(
                    255,
                    (uint8) (255 * (2 - normalizedValue * 2)),
                    (uint8) (255 * (2 - normalizedValue * 2))
                );
            }
            g.setColour(cellColor);
            g.drawHorizontalLine(colorbar.getY() + y,
                                 colorbar.getX(),
                                 colorbar.getRight());
        }

        drawAxisTicks(g, plotArea,
                     0, numTimeSteps - 1,  // X-axis range (time)
                     0, numFrequencies - 1);

        // Draw colorbar border and labels
        g.setColour(Colours::black);
        g.drawRect(colorbar, 1);
        g.drawText(String(maxVal, 2),
                   colorbar.getRight() + 5, colorbar.getY() - 10,
                   40, 20, Justification::left);
        g.drawText(String(minVal, 2),
                   colorbar.getRight() + 5, colorbar.getBottom() - 10,
                   40, 20, Justification::left);

        // Save image (same as before)
        auto timestamp = Time::currentTimeMillis();
        auto safeLabel = File::createLegalFileName(label);
        auto filename  = "heatmap_" + safeLabel + "_" + String(timestamp) + ".png";

        File outputDir(LIB_ROOT "/tests/debug_output");
        File outputFile = outputDir.getChildFile(filename);
        FileOutputStream stream(outputFile);

        plotData.gridData.clear();

        if (stream.openedOk()) {
            PNGImageFormat pngWriter;
            pngWriter.writeImageToStream(image, stream);
        }
    }

    void plotPeriodHeatmap(const PitchedSample& sample, const String& label) {
        Image image(Image::RGB, config.width, config.height, true);
        Graphics g(image);

        g.fillAll(Colours::white);

        const Rectangle<int> plotArea(
            config.marginLeft,
            config.marginTop,
            config.width - config.marginLeft - config.marginRight,
            config.height - config.marginTop - config.marginBottom
        );

        float minVal = std::numeric_limits<float>::max();
        float maxVal = std::numeric_limits<float>::min();
        for (int i = 0; i < sample.audio.left.size(); ++i) {
            minVal = std::min(minVal, sample.audio.left[i]);
            maxVal = std::max(maxVal, sample.audio.left[i]);
        }

        float valueRange = maxVal - minVal;
        minVal -= valueRange * config.valuePadding;
        maxVal += valueRange * config.valuePadding;
        valueRange = maxVal - minVal;

        float columnWidth = static_cast<float>(plotArea.getWidth()) / sample.periods.size();

        int i = 0;
        for (auto& frame : sample.periods) {
            float xPos = plotArea.getX() + i++ * columnWidth;

            int startSample  = frame.sampleOffset;
            int periodLength = static_cast<int>(std::round(frame.period));

            if (startSample + periodLength >= sample.audio.left.size()) {
                continue;
            }

            Buffer<float> section = sample.audio.left.sectionAtMost(startSample, periodLength);

            for (int y = 0; y < plotArea.getHeight(); ++y) {
                float sampleFraction = static_cast<float>(y) / plotArea.getHeight();
                int sampleIdx        = static_cast<int>(sampleFraction * periodLength);

                float value = section[sampleIdx];
                float norm  = (value - minVal) / valueRange;

                Colour color;
                if (norm < 0.5f) {
                    float t = norm * 2.0f;
                    color   = Colours::blue.interpolatedWith(Colours::white, t);
                } else {
                    float t = (norm - 0.5f) * 2.0f;
                    color   = Colours::white.interpolatedWith(Colours::red, t);
                }

                for (int x = 0; x < static_cast<int>(columnWidth); ++x) {
                    image.setPixelAt(static_cast<int>(xPos) + x,
                                     plotArea.getY() + y,
                                     color);
                }
            }
        }
        auto timestamp = Time::currentTimeMillis();
        auto safeLabel = File::createLegalFileName(label);
        auto filename  = "periods_" + safeLabel + "_" + String(timestamp) + ".png";

        File outputDir(LIB_ROOT "/tests/debug_output");
        File outputFile = outputDir.getChildFile(filename);
        FileOutputStream stream(outputFile);

        if (stream.openedOk()) {
            PNGImageFormat pngWriter;
            pngWriter.writeImageToStream(image, stream);
        }
    }

    void addBufferToGrid(const String& label, const Buffer<float>& data) {
        if (! dataMap.contains(label)) {
            dataMap.set(label, PlotData());
        }
        PlotData& plotData = dataMap.getReference(label);
        if (! workBuffer.hasSizeFor(data.size()) || plotData.gridData.size() >= config.width) {
            plotHeatmap(label);
            workBuffer.resetPlacement();
        }

        Buffer<float> copy = workBuffer.place(data.size());
        data.copyTo(copy);
        plotData.gridData.push_back(copy);
    }

    void clearPlots() {
        File outputDir(LIB_ROOT "/tests/debug_output");
        (void) outputDir.deleteRecursively();
        (void) outputDir.createDirectory();
    }

private:
    ScopedAlloc<float> workBuffer;
    HashMap<String, PlotData> dataMap;

    PlotConfig config;
    ~SignalDebugger() = default;
};

#endif
