#pragma once

#include <JuceHeader.h>

namespace CycleV2::AutomationProtocol {

juce::var makeObject();
juce::DynamicObject* objectFor(juce::var& value);
const juce::DynamicObject* objectFor(const juce::var& value);
juce::String stringProperty(
        const juce::var& value,
        const juce::Identifier& property,
        const juce::String& fallback = {});
bool boolProperty(
        const juce::var& value,
        const juce::Identifier& property,
        bool fallback = false);
int intProperty(
        const juce::var& value,
        const juce::Identifier& property,
        int fallback = 0);
float floatProperty(
        const juce::var& value,
        const juce::Identifier& property,
        float fallback = 0.f);
juce::var okResult(const juce::String& type, juce::var data = {});
juce::var failedResult(const juce::String& type, const juce::String& message);
juce::var rectangleToVar(juce::Rectangle<int> bounds);
juce::Rectangle<float> rectangleFromVar(const juce::var& value);
juce::String cursorName(const juce::MouseCursor& cursor);
bool getPathValue(
        const juce::var& root,
        const juce::String& path,
        juce::var& result);
void flattenPaths(
        const juce::var& value,
        const juce::String& prefix,
        juce::Array<juce::var>& paths);
bool compareValues(
        const juce::var& actual,
        const juce::String& op,
        const juce::var& expected);

}
