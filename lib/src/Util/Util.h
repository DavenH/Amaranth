#pragma once
#include "JuceHeader.h"
#include <vector>
#include <cxxabi.h>
#include <typeinfo>

using namespace juce;
using std::vector;
class PitchedSample;
class Savable;

class Util {
public:
    template<class T, class S>
    static bool assignAndWereDifferent(T& a, S b) {
        T castedB(static_cast<T>(b));

        if(a == castedB) {
            return false;
        }

        a = castedB;
        return true;
    }

    static File relativeFile(const String& path) {
        return File(File::getCurrentWorkingDirectory().getChildFile(path));
    }

    static String getFilePositionString(int line, const char* function, const char* filename) {
        return /* String(filename).fromLastOccurrenceOf("\\", false, true) + ", " + */ String(line) + "\t" + String(function);
    }

    static bool saveXml(const File& file, Savable const* savable, const String& name);
    static bool readXml(const File& file, Savable* savable, const String& name);

    static String getDecibelString(float amp);

    template<class T, class C>
    static void removeElement(Array <T, C>& arr, const T& val) {
        arr.removeFirstMatchingValue(val);
    }


    template<class T>
    static T* findParentComponentOfClass(Component* comp) {
        return comp->findParentComponentOfClass<T>();
    }

    template<class T>
    static void removeModalParent(Component* comp) {
        T* callout = findParentComponentOfClass<T>(comp);

        if (callout != nullptr) {
            callout->exitModalState(0);
            callout->setVisible(false);
        }
    }

    static void
    drawTextLine(Graphics& g, const String& text, int x, int y, const Justification& just = Justification::centred) {
        g.drawSingleLineText(text, x, y, just);
    }

    template<class T>
    static Rectangle <T> withSizeKeepingCentre(const Rectangle <T>& rect, T width, T height) {
        return Rectangle<T>(rect.getX() + (rect.getWidth() - width) / (T) 2,
                            rect.getY() + (rect.getHeight() - height) / (T) 2, width, height);
    }

    static int extractVelocityFromFilename(const String& str);
    static int extractPitchFromFilename(const String& str, bool useWeakMatch = true);
    static int findKey(HashMap<String, int>& map, const String& str);
    static int pitchAwareComparison(const String& a, const String& b);
    static vector<int> getIntegersInString(const String& str);
    static float getStringWidth(const Font& font, const String& text);

    template<typename T>
    static std::string className(const T *ptr) {
        int status;
        char *demangled    = abi::__cxa_demangle(typeid(*ptr).name(), nullptr, nullptr, &status);
        std::string result = (status == 0) ? demangled : typeid(*ptr).name();
        free(demangled);
        return result;
    }

    template <typename T>
    static auto reverseIter(T& container) {
        struct Wrapper {
            T& container;
            auto begin() { return container.rbegin(); }
            auto end()   { return container.rend(); }
        };
        return Wrapper{container};
    }
};
