#pragma once
#include "JuceHeader.h"

using namespace juce;

class DummyOutputStream : public OutputStream {
public:
    DummyOutputStream() {}
    virtual ~DummyOutputStream() {}
    void flush()                                                                {}
    bool setPosition (int64 newPosition)                                        { return false; }
    int64 getPosition()                                                         { return 0; }
    bool write (const void* dataToWrite, size_t howManyBytes)                   { return false; }

    bool writeBool (bool boolValue)                                             { return true; }
    bool writeByte (char byte)                                                  { return true; }
    bool writeCompressedInt (int value)                                         { return true; }
    bool writeDouble (double value)                                             { return true; }
    bool writeDoubleBigEndian (double value)                                    { return true; }
    bool writeFloat (float value)                                               { return true; }
    bool writeFloatBigEndian (float value)                                      { return true; }
    bool writeInt (int value)                                                   { return true; }
    bool writeInt64 (int64 value)                                               { return true; }
    bool writeInt64BigEndian (int64 value)                                      { return true; }
    bool writeIntBigEndian (int value)                                          { return true; }
    bool writeRepeatedByte (uint8 byte, size_t numTimesToRepeat)                { return true; }
    bool writeShort (short value)                                               { return true; }
    bool writeShortBigEndian (short value)                                      { return true; }
    bool writeString (const String& text)                                       { return true; }
    bool writeText (const String& text, bool asUTF16, bool writeUTF16ByteOrderMark) { return true; }
    int64 writeFromInputStream (InputStream& source, int64 maxNumBytesToWrite)  { return 0; }

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DummyOutputStream);
};