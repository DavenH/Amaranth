#pragma once

#include <unordered_map>
#include <variant>

#include "JuceHeader.h"
#include "SingletonAccessor.h"

using namespace juce;

namespace Constants {
    enum {
        GuideCurveTableSize = 8192
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
    ,   FreqLogTension  // the log spacing of frequency bins

    ,   numAppConstants
    };

}

class AppConstants: public SingletonAccessor  {
public:
    using ConstantValue = std::variant<int, double, String>;

    explicit AppConstants(SingletonRepo* repo);
    ~AppConstants() override = default;

    void setConstant(int key, int value)            { values[key] = ConstantValue(value);   }
    void setConstant(int key, double value)         { values[key] = ConstantValue(value);   }
    void setConstant(int key, const String& value)  { values[key] = ConstantValue(value);   }

    [[nodiscard]] int getAppConstant(int key) const;
    [[nodiscard]] double getRealAppConstant(int key) const;
    [[nodiscard]] String getStringAppConstant(int key) const;

protected:

    [[nodiscard]] const ConstantValue& getStoredValue(int key) const;

    std::unordered_map<int, ConstantValue> values;
};
