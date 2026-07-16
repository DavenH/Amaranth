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
    ,   IgnoringEditMessages
    ,   SelectWithRight
    ,   ViewVertsOnlyOnHover
    ,   Tool
    ,   SnapMode
    ,   PointSizeScale
    ,   DrawScales
    ,   CurrentMorphAxis
    ,   FirstLaunch
    ,   LinkYellow
    ,   LinkRed
    ,   LinkBlue
    ,   LastPopupClickedTransp
    ,   LastPopupClickedHorz
    ,   CollisionDetection

    ,   numSettings
    };
}

class Settings :
        public Savable
    ,   public SingletonAccessor {
public:
    explicit Settings(SingletonRepo* repo);
    ~Settings() override;

    void readGlobalSettings(XmlElement* parentElem);
    void saveGlobalSettings(XmlElement* parentElem);
    bool readXML(const XmlElement* element) override;
    void writeXML(XmlElement* element) const override;
    var writeJSON() const override;
    bool readJSON(const var& object) override;
    XmlElement* getMidiSettingsElement();

    void init() override;
    void createPropertiesFile(const String& path);
    void createSettingsFile(const String& path);
    void initialiseSettings();
    void writePropertiesFile();

    String getProperty(const String& key, const String& defaultStr = String());
    void setProperty(const String& key, const String& value);

    int& getGlobalSetting(int setting) { return globalSettingsMap[setting].value;       }
    int& getDocumentSetting(int setting) { return documentSettingsMap[setting].value;   }
    [[nodiscard]] int getGlobalSettingValue(int setting) const { return globalSettingsMap.at(setting).value; }
    [[nodiscard]] int getDocumentSettingValue(int setting) const { return documentSettingsMap.at(setting).value; }
    void setPropertiesPath(const String& path) { this->propertiesPath = path; }
    void addDocumentSetting(int setting, const String& key, int defaultValue);

protected:
    class Setting {
    public:
        Setting() : value(0) {}
        Setting(String  key, int value) : key(std::move(key)), value(value) {}

        String key;
        int value;
    };

    /* ----------------------------------------------------------------------------- */

    String propertiesPath;

    File settingsFile, propertiesFile;
    std::map<int, Setting> globalSettingsMap, documentSettingsMap;

    std::unique_ptr<XmlElement> propsElem;
    std::unique_ptr<XmlElement> settingsFileElem;
};
