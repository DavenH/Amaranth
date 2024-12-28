#pragma once

#include "JuceHeader.h"
#include "SingletonAccessor.h"

using namespace juce;

namespace Constants {
    enum {
        DeformTableSize     = 8192
    ,   LogTension          = 50
    ,   LowestMidiNote      = 20
    ,   TitleBarHeight      = 24
    ,   HighestMidiNote     = 127
    ,   MeshFormatVersion   = 2
    };

    enum {
        DocSettingsDir
    ,   DocumentName
    ,   ProductVersion
    ,   DocumentExt
    ,   DocMagicCode
    ,   FontFace
    ,   MinLineLength
    ,   ProductName
    ,   DocumentsDir
    ,   PropertiesPath

    ,   numAppConstants
    };

}

class AppConstants: public SingletonAccessor  {
public:
    explicit AppConstants(SingletonRepo* repo);
    ~AppConstants() override = default;

    void setConstant(int key, int value)            { values    .set(key, value);   }
    void setConstant(int key, double value)         { realValues.set(key, value);   }
    void setConstant(int key, const String& value)  { strValues .set(key, value);   }

    [[nodiscard]] int getAppConstant(int key) const;
    [[nodiscard]] double getRealAppConstant(int key) const;
    [[nodiscard]] String getStringAppConstant(int key) const;

protected:

    HashMap<int, int>    values;
    HashMap<int, double> realValues;
    HashMap<int, String> strValues;
};
