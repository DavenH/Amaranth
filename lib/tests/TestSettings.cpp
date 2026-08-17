#include <catch2/catch_test_macros.hpp>

#include <App/Settings.h>

using namespace juce;

TEST_CASE("Transient settings do not require properties persistence",
        "[settings][lifecycle]") {
    Settings settings(nullptr);
    settings.initialiseSettings();
    settings.writePropertiesFile();

    REQUIRE(settings.getGlobalSettingValue(AppSettings::PreviewVoiceLengthMilliseconds)
            == 1000);
}

TEST_CASE("Configured settings persist properties",
        "[settings][lifecycle]") {
    const File settingsFile = File::getSpecialLocation(File::tempDirectory)
            .getNonexistentChildFile("amaranth-settings-test", ".xml");
    {
        Settings settings(nullptr);
        settings.createPropertiesFile(settingsFile.getFullPathName());
        settings.setProperty("testKey", "testValue");
        settings.writePropertiesFile();
    }

    const std::unique_ptr<XmlElement> document = XmlDocument::parse(settingsFile);
    REQUIRE(document != nullptr);
    REQUIRE(document->getStringAttribute("testKey") == "testValue");
    REQUIRE(settingsFile.deleteFile());
}
