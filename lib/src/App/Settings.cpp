#include <App/Settings.h>

#include <memory>
#include <utility>
#include "SingletonRepo.h"
#include "../Inter/Interactor.h"
#include "../Audio/AudioHub.h"

#define addSetting(X, Y) globalSettingsMap[X] = Setting(#X, Y)

Settings::Settings(SingletonRepo* repo) : SingletonAccessor(repo, "Settings") {
}

Settings::~Settings() {
    writePropertiesFile();

    propsElem = nullptr;
    settingsFileElem = nullptr;
}

void Settings::init() {
    initialiseSettings();

    String appDir(File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName());
    const String path = appDir + propertiesPath + "/settings.json";
    createPropertiesFile(path);
    createSettingsFile(path);

    // TODO need to decouple this with a listener
    XmlElement* midiSettings = settingsFileElem ? settingsFileElem->getChildByName("DEVICESETUP") : nullptr;
    getObj(AudioHub).initialiseAudioDevice(midiSettings);
}

void Settings::initialiseSettings() {
    using namespace AppSettings;

    addSetting(CurrentMorphAxis,        Vertex::Time);
    addSetting(DrawScales,              true);
    addSetting(FirstLaunch,             true);
    addSetting(IgnoringEditMessages,    false);
    addSetting(LastPopupClickedHorz,    false);
    addSetting(LastPopupClickedTransp,  false);
    addSetting(LinkBlue,                true);
    addSetting(LinkRed,                 true);
    addSetting(LinkYellow,              true);
    addSetting(PointSizeScale,          ScaleSizes::ScaleSmall);
    addSetting(SelectWithRight,         false);
    addSetting(Tool,                    Tools::Selector);
    addSetting(UpdateGfxRealtime,       true);
    addSetting(ViewVertsOnlyOnHover,    false);
}

void Settings::readGlobalSettings(XmlElement* settingsDocElem) {
    XmlElement* bodyElem = formatSplit(settingsFileElem.get(), settingsDocElem->getChildByName("SettingsDocument"));

    XmlElement* settingsElem = bodyElem ? bodyElem->getChildByName("Settings") : nullptr;

    if (settingsElem != nullptr) {
        for (auto & settingPair : globalSettingsMap) {
            Setting& setting = settingPair.second;
            setting.value = settingsElem->getIntAttribute(setting.key, setting.value);
        }
    }
}

void Settings::saveGlobalSettings(XmlElement* pluginElem) {
    auto* settingsElem = new XmlElement("Settings");

    for (auto& settingPair : globalSettingsMap) {
        const Setting& setting = settingPair.second;

        settingsElem->setAttribute(setting.key, setting.value);
    }

    std::unique_ptr<XmlElement> settingsDocElem(new XmlElement("SettingsDocument"));
    settingsDocElem->addChildElement(settingsElem);

  #if PLUGIN_MODE
    pluginElem->addChildElement(settingsDocElem.get());
  #else

    if(settingsFile.existsAsFile()) {
        (void) settingsFile.deleteFile();
    }

    std::unique_ptr outStream(settingsFile.createOutputStream());

    if (outStream == nullptr) {
        showConsoleMsg("Problem saving settings file");
        return;
    }

    String filedata = settingsDocElem->toString();
    outStream->writeString(filedata);
  #endif
}

void Settings::writeXML(XmlElement* topElement) const {
    auto* prstElem = new XmlElement("Settings");

    for (const auto& settingPair : documentSettingsMap) {
        const Setting& setting = settingPair.second;
        prstElem->setAttribute(setting.key, setting.value);
    }

    topElement->addChildElement(prstElem);
}

bool Settings::readXML(const XmlElement* element) {
    XmlElement* prstElem = element->getChildByName("Settings");

    if (prstElem == nullptr) {
        return false;
    }

    for (auto& settingPair : documentSettingsMap) {
        Setting& setting = settingPair.second;

        setting.value = prstElem->getIntAttribute(setting.key, setting.value);
    }

    return true;
}

String Settings::getProperty(const String& key, const String& defaultStr) {
    jassert(propsElem != nullptr);

    return propsElem == nullptr ? defaultStr : propsElem->getStringAttribute(key, defaultStr);
}

void Settings::setProperty(const String& key,
                           const String& value) {
    if (propsElem != nullptr) {
        propsElem->setAttribute(key, value);
    }
}

void Settings::createSettingsFile(const String& path) {
    if(path.isEmpty()) {
        throw std::runtime_error("No properties file specified");
    }
    settingsFile = File(path);

    if(! settingsFile.existsAsFile()) {
        return;
    }

    std::unique_ptr stream(settingsFile.createInputStream());
    XmlDocument xmlDoc(stream->readEntireStreamAsString());
    settingsFileElem = xmlDoc.getDocumentElement();
}

void Settings::createPropertiesFile(const String& path) {
    if(path.isEmpty()) {
        throw std::runtime_error("No properties file specified");
    }

    propertiesFile = File(path);

    if (!propertiesFile.existsAsFile()) {
        (void) propertiesFile.create();
    }

    std::unique_ptr fileStream(propertiesFile.createInputStream());

    if (fileStream != nullptr) {
        XmlDocument xmlDoc(fileStream->readEntireStreamAsString());
        propsElem = xmlDoc.getDocumentElement();
    }

    if (propsElem == nullptr) {
        propsElem = std::make_unique<XmlElement>("Properties");
    }
}

void Settings::writePropertiesFile() {
    bool deleted = propertiesFile.deleteFile();

    if (deleted) {
        std::unique_ptr fileStream(propertiesFile.createOutputStream());

        jassert(fileStream != nullptr);
        jassert(propsElem != nullptr);

        if (fileStream != nullptr) {
            String docString = propsElem->toString();
            fileStream->writeString(docString);
            fileStream->flush();
        }
    }
}

