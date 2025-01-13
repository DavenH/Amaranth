#pragma once
#include <JuceHeader.h>

using namespace juce;

class AppSettings {
public:
    PropertiesFile& getSettings() { return *settings; }
    JUCE_DECLARE_SINGLETON_INLINE(AppSettings, true)

private:
    AppSettings() {
        PropertiesFile::Options options;
        options.applicationName = "Oscillo";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        
        settings = std::make_unique<PropertiesFile>(options);
    }

    std::unique_ptr<PropertiesFile> settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppSettings)
};