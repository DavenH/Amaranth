#pragma once

#include <map>
#include <utility>
#include "JuceHeader.h"
#include "SingletonAccessor.h"
#include "Doc/Savable.h"

using std::map;

namespace AppSettings {
    enum {
        UpdateGfxRealtime
    ,	IgnoringEditMessages
    ,	SelectWithRight
    ,	ViewVertsOnlyOnHover
    ,	Tool
    ,	SnapMode
    ,	PointSizeScale
    ,	DrawScales
    ,	CurrentMorphAxis
    ,	FirstLaunch
    ,	LinkYellow
    ,	LinkRed
    ,	LinkBlue
    ,	LastPopupClickedTransp
    ,	LastPopupClickedHorz

    ,	numSettings
    };
}

class Settings :
        public Savable
    ,	public SingletonAccessor {
public:
    struct ClientPaths {
        String propertiesPath;
    };

    Settings(SingletonRepo* repo, ClientPaths paths);
    ~Settings() override;

    void readGlobalSettings(XmlElement* parentElem);
    void saveGlobalSettings(XmlElement* parentElem);
    bool readXML(const XmlElement* element) override;
    void writeXML(XmlElement* element) const override;
    XmlElement* getMidiSettingsElement();

    void init() override;
    void createPropertiesFile();
    void createSettingsFile();
    void initialiseSettings();
    void writePropertiesFile();

    String getProperty(const String& key, const String& defaultStr = String());
    void setProperty(const String& key, const String& value);

    int& getGlobalSetting(int setting) 		{ return globalSettingsMap[setting].value;		}
    int& getDocumentSetting(int setting) 	{ return documentSettingsMap[setting].value; 	}

protected:
    class Setting {
    public:
        Setting() : value(0) {}
        Setting(String  key, int value) : key(std::move(key)), value(value) {}

        String key;
        int value;
    };

    /* ----------------------------------------------------------------------------- */

    ClientPaths paths;
    File settingsFile, propertiesFile;
    std::map<int, Setting> globalSettingsMap, documentSettingsMap;

    std::unique_ptr<XmlElement> propsElem;
    std::unique_ptr<XmlElement> settingsFileElem;
};