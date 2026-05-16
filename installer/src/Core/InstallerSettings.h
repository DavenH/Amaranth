#pragma once

#include <JuceHeader.h>

using namespace juce;

class InstallerSettings {
public:
    explicit InstallerSettings(const String& productId);

    String getString(const String& key, const String& fallback = String()) const;
    bool getBool(const String& key, bool fallback) const;

    void setString(const String& key, const String& value);
    void setBool(const String& key, bool value);
    void save();

private:
    std::unique_ptr<PropertiesFile> properties;
};
