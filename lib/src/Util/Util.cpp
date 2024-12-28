#include "Util.h"

#include <utility>
#include "../App/Doc/Savable.h"
#include "../Util/NumberUtils.h"


bool Util::saveXml(const File& file, const Savable* savable, const String& name) {
    if(file.existsAsFile()) {
        (void) file.deleteFile();
    }

    std::unique_ptr<XmlElement> topelem(new XmlElement(name));
    savable->writeXML(topelem.get());

    String filedata = topelem->toString();
    std::unique_ptr<FileOutputStream> outStream = file.createOutputStream();

    if(! outStream) {
        return false;
    }

    GZIPCompressorOutputStream gzipStream(outStream.get(), 5);
    CharPointer_UTF8 utf8Data = filedata.toUTF8();

    gzipStream.write(utf8Data.getAddress(), utf8Data.sizeInBytes());
    gzipStream.flush();

    return true;
}

bool Util::readXml(const File& file, Savable* savable, const String& name) {
    std::unique_ptr stream(file.createInputStream());
    GZIPDecompressorInputStream decompStream(stream.get(), false);
    String presetDocString(decompStream.readEntireStreamAsString());
    XmlDocument presetDoc(presetDocString);
    std::unique_ptr topelem(presetDoc.getDocumentElement());

    if(topelem == nullptr) {
        return false;
    }

    bool succeeded = savable->readXML(topelem.get());

    return succeeded;
}

String Util::getDecibelString(float amp) {
    return (amp < 1e-6) ? String(L" -\u221edB") :
                          String(NumberUtils::toDecibels(amp), 2) + "dB";
}

int Util::findKey(HashMap<String, int>& map, const String& str) {
    int val = 0;
    HashMap<String, int>::Iterator iter(map);

    while (iter.next()) {
        if (str.containsIgnoreCase(iter.getKey())) {
            val = iter.getValue();
            break;
        }
    }

    return val;
}

int Util::extractVelocityFromFilename(const String& str) {
    struct Loudness {
        String str;
        int topVelocity;

        Loudness(String  str, int vel) : str(std::move(str)), topVelocity(vel) {}
    };

    static vector<Loudness> strings;
    static bool havePopulated = false;

    if (!havePopulated) {
        havePopulated = true;

        strings.emplace_back("soft", 57);
        strings.emplace_back("med",  93);
        strings.emplace_back("mell", 93);
        strings.emplace_back("hard", 127);
        strings.emplace_back("loud", 127);

        strings.emplace_back("mp",  57);
        strings.emplace_back("mf",  93);
        strings.emplace_back("ff",  127);
    }

    for (auto& loudness : strings) {
        if (str.containsIgnoreCase(loudness.str)) {
            return loudness.topVelocity;
        }
    }

    return -1;
}

int Util::extractPitchFromFilename(const String& str, bool useWeakMatch) {
    // build hashmap
    static HashMap<String, int> noteCodeMap;
    static HashMap<String, int> weakCodeMap;
    static bool havePopulated = false;

    if (!havePopulated) {
        String notes[]  = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "H" };
        String cxters[] = { " ", "_", "" };
        String preps[]  = { " ", "_" };
        String octs[]   = { "0",    "1",    "2",    "3",    "4",    "5",    "6",    "7",    "8",
                            "00",   "01",   "02",   "03",   "04",   "05",   "06",   "07",   "08",
                            "000",  "001",  "002",  "003",  "004",  "005",  "006",  "007",  "008" };

        for (int key = 0; key < numElementsInArray(notes); ++key) {
            // H is same as B
            int realKey = jmin(11, key);

            for (int oct = 0; oct < numElementsInArray(octs); ++oct) {
                for (const auto& cxter : cxters) {
                    String note     = notes[key] + cxter + octs[oct];
                    int midiNote    = 12 * ((oct % 9) + 1)  + realKey;
                    noteCodeMap.set(note, midiNote);
                }
            }

            weakCodeMap.set(" " + notes[key] + " ", 12 * 5 + realKey);
            weakCodeMap.set("_" + notes[key], 12 * 5 + realKey);
        }

        havePopulated = true;
    }

    int midiNote = 0;
    for(int i = 0; i < 5; ++i) {
        midiNote = jmax(midiNote, noteCodeMap[str.getLastCharacters(i + 2)]);
    }

    if (midiNote == 0 && str.containsAnyOf("CDEFGABH#cdefgabh")) {
        midiNote = findKey(noteCodeMap, str);

        if(midiNote == 0 && useWeakMatch) {
            midiNote = findKey(weakCodeMap, str);
        }
    }

    return midiNote;
}

int Util::pitchAwareComparison(const String& a, const String& b) {
    StringArray tokensA, tokensB;
    tokensA.addTokens(a, " _");
    tokensB.addTokens(b, " _");

    String common;

    for (const auto& i : tokensA) {
        for (const auto& j : tokensB) {
            if (i == j) {
                if(i.length() > jmax(1, common.length())) {
                    common = i;
                }
            }
        }
    }

    int midiNoteA, midiNoteB;
    String source;

    int index = a.indexOfWholeWord(common);
    if (index >= 0) {
        String composite = a.substring(0, index) + a.substring(index + common.length());
        source = composite;
    } else {
        source = a;
    }

    midiNoteA = extractPitchFromFilename(source);
    index = b.indexOfWholeWord(common);

    if (index >= 0) {
        String composite = b.substring(0, index) + b.substring(index + common.length());
        source = composite;
    } else {
        source = b;
    }

    midiNoteB = extractPitchFromFilename(source);

    if (midiNoteA == midiNoteB) {
        return a.compare(b);
    }

    return midiNoteA < midiNoteB ? -1 : 1;
}

vector<int> Util::getIntegersInString(const String& str) {
    vector<int> ints;

    int start = -1;
    for (int i = 0; i < str.length(); ++i) {
        juce_wchar ch = str[i];

        if (NumberUtils::within<juce_wchar>(ch, '0', '9')) {
            start = i;
            while(i < str.length() && NumberUtils::within<juce_wchar>(str[i], '0', '9')) {
                ++i;
            }

            String sub(str.substring(start, i));
            int val = sub.getIntValue();

            if(val > 0) {
                ints.push_back(val);
            }
        }
    }

    return ints;
}

float Util::getStringWidth(const Font& font, const String& text) {
    if (auto typeface = font.getTypefacePtr()) {
        const auto w = typeface->getStringWidth (font.getMetricsKind(), text, font.getHeight(), font.getHorizontalScale());
        return w + (font.getHeight() * font.getHorizontalScale() * font.getExtraKerningFactor() * (float) text.length());
    }
    return 0;
}

