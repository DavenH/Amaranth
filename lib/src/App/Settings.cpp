#include <App/Settings.h>
#include "SingletonRepo.h"
#include "../Util/Util.h"
#include "../Inter/Interactor.h"
#include "../Audio/AudioHub.h"

#define addSetting(X, Y) globalSettingsMap[X] = Setting(#X, Y)


Settings::Settings(SingletonRepo* repo, ClientPaths paths) :
    	SingletonAccessor	(repo, "Settings")
	,	paths				(paths) {
}


Settings::~Settings() {
    writePropertiesFile();

    propsElem = nullptr;
    settingsFileElem = nullptr;
}


void Settings::init() {
	initialiseSettings();
	createPropertiesFile();
	createSettingsFile();

	// TODO need to decouple this with a listener
    XmlElement* midiSettings = settingsFileElem ? settingsFileElem->getChildByName("DEVICESETUP") : 0;
    getObj(AudioHub).initialiseAudioDevice(midiSettings);
}


void Settings::initialiseSettings() {
	using namespace AppSettings;

	addSetting(CurrentMorphAxis, 		Vertex::Time);
	addSetting(DrawScales, 				true);
	addSetting(FirstLaunch, 			true);
	addSetting(IgnoringEditMessages, 	false);
	addSetting(LastPopupClickedHorz, 	false);
	addSetting(LastPopupClickedTransp, 	false);
	addSetting(LinkBlue, 				true);
	addSetting(LinkRed, 				true);
	addSetting(LinkYellow, 				true);
	addSetting(PointSizeScale, 			ScaleSizes::ScaleSmall);
	addSetting(SelectWithRight, 		false);
	addSetting(Tool, 					Tools::Selector);
	addSetting(UpdateGfxRealtime, 	 	true);
	addSetting(ViewVertsOnlyOnHover, 	false);
}


void Settings::readGlobalSettings(XmlElement* settingsDocElem) {
    XmlElement* bodyElem = formatSplit(settingsFileElem.get(), settingsDocElem->getChildByName("SettingsDocument"));

    XmlElement* settingsElem = bodyElem ? bodyElem->getChildByName("Settings") : nullptr;

    if (settingsElem != nullptr) {
        for (int i = 0; i < (int) globalSettingsMap.size(); ++i) {
			Setting& setting = globalSettingsMap[i];
			setting.value = settingsElem->getIntAttribute(setting.key, setting.value);
		}
	}
}


void Settings::saveGlobalSettings(XmlElement* pluginElem) {
	XmlElement* settingsElem = new XmlElement("Settings");

    for (int i = 0; i < (int) globalSettingsMap.size(); ++i) {
		Setting& setting = globalSettingsMap[i];
		settingsElem->setAttribute(setting.key, setting.value);
	}

    std::unique_ptr<XmlElement> settingsDocElem(new XmlElement("SettingsDocument"));
	settingsDocElem->addChildElement(settingsElem);

  #if PLUGIN_MODE
	pluginElem->addChildElement(settingsDocElem);
  #else

	if(settingsFile.existsAsFile())
		settingsFile.deleteFile();

    std::unique_ptr<FileOutputStream> outStream(settingsFile.createOutputStream());

    if (outStream == nullptr) {
		showMsg("Problem saving settings file");
		return;
	}

	String filedata = settingsDocElem->toString();
	outStream->writeString(filedata);
  #endif
}


void Settings::writeXML(XmlElement* topElement) const {
  #ifndef DEMO_VERSION
	XmlElement* prstElem = new XmlElement("Settings");

    for (int i = 0; i < (int) documentSettingsMap.size(); ++i) {
		const Setting& setting = documentSettingsMap.at(i);
		prstElem->setAttribute(setting.key, setting.value);
	}

	topElement->addChildElement(prstElem);
  #endif
}


bool Settings::readXML(const XmlElement* element) {
    XmlElement* prstElem = element->getChildByName("Settings");

    if (prstElem == nullptr)
        return false;

    for (int i = 0; i < (int) documentSettingsMap.size(); ++i) {
		Setting& setting = documentSettingsMap[i];
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
    if (propsElem != nullptr)
		propsElem->setAttribute(key, value);
}


void Settings::createSettingsFile() {
    settingsFile = File(paths.propertiesPath);

	if(! settingsFile.existsAsFile())
		return;

    std::unique_ptr<FileInputStream> stream(settingsFile.createInputStream());
	XmlDocument xmlDoc(stream->readEntireStreamAsString());
	settingsFileElem = xmlDoc.getDocumentElement();
}


void Settings::createPropertiesFile() {
    propertiesFile = File(paths.propertiesPath);

    if (!propertiesFile.existsAsFile())
        propertiesFile.create();

    std::unique_ptr<FileInputStream> fileStream(propertiesFile.createInputStream());

    if (fileStream != nullptr) {
        XmlDocument xmlDoc(fileStream->readEntireStreamAsString());
        propsElem = xmlDoc.getDocumentElement();
    }

    if (propsElem == nullptr)
        propsElem = std::unique_ptr<XmlElement>(new XmlElement("Properties"));
}


void Settings::writePropertiesFile() {
    bool deleted = propertiesFile.deleteFile();

    if (deleted) {
        std::unique_ptr<FileOutputStream> fileStream(propertiesFile.createOutputStream());

        jassert(fileStream != nullptr);
        jassert(propsElem != nullptr);

        if (fileStream != nullptr) {
            String docString = propsElem->toString();
            fileStream->writeString(docString);
            fileStream->flush();
        }
    }
}

