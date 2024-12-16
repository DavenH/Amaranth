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
	~Document() override = default;

	bool open(const String& filename);
	bool open(InputStream* stream);
	bool saveHeaderValidated(DocumentDetails& updatedDetails);

	void save(const String& filename);
	void save(OutputStream* outStream);

  #ifdef JUCE_DEBUG
	String getPresetString();
  #endif

	void addListener(Listener* listener) 		{ listeners.add(listener); 		}
	void registerSavable(Savable* toSave) 		{ savableItems.add(toSave); 	}
	void setValidator(IValidator* validator)	{ this->validator = validator; 	}

	static bool readHeader(InputStream* stream, DocumentDetails& deets, int magicValue) {
        if (stream == nullptr)
            return false;

        int code = stream->readInt();

        if (code == magicValue) {
			int deetsSize = stream->readInt();
			jassert(deetsSize > 0 && deetsSize <= headerSizeBytes - 2);

			String detailsString(stream->readString());
			XmlDocument deetsDoc(detailsString);
			std::unique_ptr deetsElem(deetsDoc.getDocumentElement());

			jassert(deetsDoc.getLastParseError().isEmpty());
			jassert(deetsElem != nullptr);

			if(deetsElem == nullptr)
				return false;

			stream->setPosition((int64) headerSizeBytes);

			deets.readXML(deetsElem.get());
			return true;
		}

		return false;
	}

	static bool saveHeader(OutputStream* stream,
						   DocumentDetails& updatedDetails,
						   int magicValue,
						   bool preserveRevision = false,
						   bool preserveDate = false) {
		const String& filename = updatedDetails.getFilename();

		File file(filename);
		if(! file.existsAsFile()) {
            jassertfalse;
			return false;
		}

	  #ifndef DEMO_VERSION
		std::unique_ptr<InputStream> in(file.createInputStream());

		int firstByte = in->readInt();
		if(firstByte != magicValue)
			return false;

		ScopedValueSetter revisionSuppressor(updatedDetails.getSuppressRevFlag(), ! preserveRevision, false);
		ScopedValueSetter dateSuppressor(updatedDetails.getSuppressDateFlag(), 	! preserveDate, 	false);
		std::unique_ptr<XmlElement> detailsElem(new XmlElement("PresetDetails"));

		updatedDetails.writeXML(detailsElem.get());

		String detailsString = detailsElem->toString();
		int deetsSize = detailsString.getNumBytesAsUTF8() + 1;

        std::unique_ptr out(file.createOutputStream());

		bool didSet = out->setPosition(0);
		jassert(didSet);

		out->writeInt(magicValue);
		detailsElem->writeTo(*out);
		out->writeInt(deetsSize);
		out->writeString(detailsString);

		int remainder = jmax(0, headerSizeBytes - 2 - deetsSize);

		out->writeRepeatedByte(0, remainder);
		jassert(out->getPosition() == headerSizeBytes);
	  #endif

		return true;
	}

	DocumentDetails& getDetails() 		{ return details; }
	virtual String getHeaderString() 	{ return {}; }
	const String& getDocumentName() 	{ return details.getName(); }
	double getVersionValue()			{ return details.getProductVersion(); }

protected:
	bool validate();

	DocumentDetails details;
	ListenerList<Listener> listeners;

	IValidator* validator;
	Array<Savable*> savableItems;

    JUCE_LEAK_DETECTOR(Document);
};