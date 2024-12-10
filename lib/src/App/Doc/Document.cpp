#include <iostream>
#include "Document.h"
#include "Savable.h"
#include "../Settings.h"
#include "../AppConstants.h"
#include "../SingletonRepo.h"
#include "../../Util/Util.h"
#include "../../UI/IConsole.h"
#include "../../Definitions.h"


bool Document::open(const String& filename) {
    File file(filename);

	dout << "opening " << file.getFileName() << "\n";

	if(! file.existsAsFile())
	{
		dout << "file " << filename << " does not exist\n";
		return false;
	}

    std::unique_ptr<InputStream> stream(file.createInputStream());
	return open(stream.get());
}


bool Document::validate() {
    if (validator == nullptr)
		return true;

	if(validator->isDemoMode())
	{
		showMsg("Saving presets is disabled in this demo");
		return false;
	}

	if(validator->isBetaMode())
	{
		int64 smallTime = Time::currentTimeMillis() / 1000;
		if(smallTime > Constants::BetaExpirySecs)
		{
			showMsg("Beta expired, please update to the latest");
			return false;
		}
	}

	return true;
}

void Document::save(const String& filename) {
    if (!validate())
        return;

	File saveFile(filename);

	if(saveFile.existsAsFile())
		(void) saveFile.deleteFile();

    std::unique_ptr outStream(saveFile.createOutputStream());

	save(outStream.get());
}


void Document::save(OutputStream* outStream) {
    if (!validate())
		return;

  #ifndef DEMO_VERSION
    if (outStream == nullptr) {
		showMsg("Problem saving preset");
		return;
	}

	std::unique_ptr<XmlElement> topelem(new XmlElement("Preset"));
	topelem->setAttribute("VersionValue", getRealConstant(ProductVersion));

	for(auto savableItem : savableItems)
		savableItem->writeXML(topelem.get());

	saveHeader(outStream, details, Constants::DocMagicCode);

	String docString = topelem->toString();
	CharPointer_UTF8 utf8Data = docString.toUTF8();

	GZIPCompressorOutputStream gzipStream(outStream, 5);
	gzipStream.write(utf8Data, utf8Data.sizeInBytes());
	gzipStream.flush();
  #endif
}

#ifdef JUCE_DEBUG
String Document::getPresetString()
{
	std::unique_ptr<XmlElement> topelem(new XmlElement("Preset"));
	topelem->setAttribute("VersionValue", getRealConstant(ProductVersion));

	for(auto savableItem : savableItems)
		savableItem->writeXML(topelem);

	return topelem->toString();
}
#endif


bool Document::open(InputStream* stream) {
    // stream can have additional header info
	// from plugin's config settings
	int64 startPosition = stream->getPosition();

	if(! readHeader(stream, details, Constants::DocMagicCode))
		return false;
		
	listeners.call(&Listener::documentAboutToLoad);

	stream->setPosition(startPosition + (int64) headerSizeBytes);

	GZIPDecompressorInputStream decompStream(stream, false);
	String presetDocString(decompStream.readEntireStreamAsString());

	XmlDocument presetDoc(presetDocString);
	std::unique_ptr topelem(presetDoc.getDocumentElement());

	jassert(topelem != nullptr);

    if (topelem) {
        for (auto savableItem : savableItems) {
            if (!savableItem->readXML(topelem.get()))
				continue;
		}
	}

	listeners.call(&Listener::documentHasLoaded);

	return true;
}


bool Document::saveHeaderValidated(DocumentDetails& updatedDetails) {
	if(! validate())
		return false;

	File file(updatedDetails.getFilename());
    std::unique_ptr<FileOutputStream> out(file.createOutputStream());

	return saveHeader(out.get(), updatedDetails, getConstant(DocMagicCode), true, true);
}
