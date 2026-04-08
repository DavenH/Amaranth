#pragma once

#include <vector>
#include "DocumentDetails.h"
#include "../SingletonAccessor.h"
#include "../IValidator.h"
#include "JuceHeader.h"

class Document : public SingletonAccessor {
public:
    static const int headerSizeBytes = 1024;

    class Listener {
    public:
        virtual ~Listener() = default;

        virtual void documentAboutToLoad() = 0;
        virtual void documentHasLoaded()   = 0;
    };

    explicit Document(SingletonRepo* repo);

    bool open(const String& filename);
    bool open(InputStream* stream);
    bool saveHeaderValidated(DocumentDetails& updatedDetails);

    void save(const String& filename);
    void save(OutputStream* outStream);

  #ifdef JUCE_DEBUG
    String getPresetString();
  #endif

    void addListener(Listener* listener)        { listeners.add(listener);      }
    void registerSavable(Savable* toSave)       { savableItems.add(toSave);     }
    void setValidator(IValidator* validator)    { this->validator = validator;  }

    static bool readHeader(InputStream* stream, DocumentDetails& deets, int magicValue);

    static bool saveHeader(OutputStream* stream,
                           DocumentDetails& updatedDetails,
                           int magicValue,
                           bool preserveRevision = false,
                           bool preserveDate = false);

    DocumentDetails& getDetails()       { return details; }
    virtual String getHeaderString()    { return {}; }
    const String& getDocumentName()     { return details.getName(); }
    double getVersionValue()            { return details.getProductVersion(); }

protected:
    bool validate();

    DocumentDetails details;
    ListenerList<Listener> listeners;

    IValidator* validator;
    Array<Savable*> savableItems;

    JUCE_LEAK_DETECTOR(Document);
};
