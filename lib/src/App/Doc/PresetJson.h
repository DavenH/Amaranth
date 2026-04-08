#pragma once

#include "JuceHeader.h"

namespace PresetJson {
    using Object = juce::DynamicObject::Ptr;

    inline Object object() {
        return new juce::DynamicObject();
    }

    inline juce::var array() {
        return juce::var(juce::Array<juce::var>());
    }

    inline juce::var toVar(const Object& obj) {
        return juce::var(obj.get());
    }

    inline juce::DynamicObject* getObject(juce::var& value) {
        return value.getDynamicObject();
    }

    inline const juce::DynamicObject* getObject(const juce::var& value) {
        return value.getDynamicObject();
    }

    inline juce::Array<juce::var>* getArray(juce::var& value) {
        return value.getArray();
    }

    inline const juce::Array<juce::var>* getArray(const juce::var& value) {
        return value.getArray();
    }

    inline juce::var property(const juce::var& value, const juce::Identifier& name) {
        if (const auto* obj = getObject(value)) {
            return obj->getProperty(name);
        }

        return {};
    }

    inline juce::String stringProperty(const juce::var& value, const juce::Identifier& name,
                                       const juce::String& fallback = {}) {
        juce::var prop = property(value, name);
        return prop.isVoid() ? fallback : prop.toString();
    }

    inline int intProperty(const juce::var& value, const juce::Identifier& name, int fallback = 0) {
        juce::var prop = property(value, name);
        return prop.isVoid() ? fallback : int(prop);
    }

    inline double doubleProperty(const juce::var& value, const juce::Identifier& name, double fallback = 0.0) {
        juce::var prop = property(value, name);
        return prop.isVoid() ? fallback : double(prop);
    }

    inline bool boolProperty(const juce::var& value, const juce::Identifier& name, bool fallback = false) {
        juce::var prop = property(value, name);
        return prop.isVoid() ? fallback : bool(prop);
    }

    inline juce::StringArray stringArray(const juce::var& value) {
        juce::StringArray strings;

        if (const auto* values = getArray(value)) {
            for (const auto& entry : *values) {
                strings.add(entry.toString());
            }
        }

        return strings;
    }

    inline juce::var fromStringArray(const juce::StringArray& strings) {
        juce::Array<juce::var> values;

        for (const auto& string : strings) {
            values.add(string);
        }

        return juce::var(values);
    }
}
