#pragma once

#include "DocumentDetails.h"
#include "JuceHeader.h"

class PresetMigrator {
public:
    static var migrateV1XmlToCurrentJson(const XmlElement* presetElement, const DocumentDetails& details);
    static var migrateXmlToCurrentJson(const XmlElement* presetElement, const DocumentDetails& details);
};
