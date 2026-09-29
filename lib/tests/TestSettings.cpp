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

TEST_CASE("Preset style settings reset missing values instead of inheriting another preset",
        "[settings][surface-program]") {
    Settings settings(nullptr);
    settings.addDocumentSetting(12, "TimeSurfaceStyle", 2, true);
    settings.addDocumentSetting(13, "UnchangedLegacySetting", 7);
    settings.getDocumentSetting(12) = 13;
    const var bullion = settings.writeJSON();
    XmlElement encoded("Preset");
    settings.writeXML(&encoded);
    REQUIRE(settings.readJSON(var(new DynamicObject())));
    REQUIRE(settings.getDocumentSettingValue(12) == 2);
    REQUIRE(settings.readJSON(bullion));
    REQUIRE(settings.getDocumentSettingValue(12) == 13);
    XmlElement oldPreset("Preset");
    REQUIRE_FALSE(settings.readXML(&oldPreset));
    REQUIRE(settings.getDocumentSettingValue(12) == 2);
    REQUIRE(settings.readXML(&encoded));
    REQUIRE(settings.getDocumentSettingValue(12) == 13);
    settings.readMissingJSON();
    REQUIRE(settings.getDocumentSettingValue(12) == 2);
    REQUIRE(settings.getDocumentSettingValue(13) == 7);
}
