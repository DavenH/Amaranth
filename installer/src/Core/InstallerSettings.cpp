#include "InstallerSettings.h"

InstallerSettings::InstallerSettings(const String& productId) {
    PropertiesFile::Options options;
    options.applicationName = "installer-" + productId;
    options.folderName = "Amaranth Audio/Installer";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = PropertiesFile::storeAsXML;

    properties = std::make_unique<PropertiesFile>(options);
}

String InstallerSettings::getString(const String& key, const String& fallback) const {
    return properties != nullptr ? properties->getValue(key, fallback) : fallback;
}

bool InstallerSettings::getBool(const String& key, bool fallback) const {
    return properties != nullptr ? properties->getBoolValue(key, fallback) : fallback;
}

void InstallerSettings::setString(const String& key, const String& value) {
    if (properties != nullptr) {
        properties->setValue(key, value);
    }
}

void InstallerSettings::setBool(const String& key, bool value) {
    if (properties != nullptr) {
        properties->setValue(key, value);
    }
}

void InstallerSettings::save() {
    if (properties != nullptr) {
        properties->saveIfNeeded();
    }
}
