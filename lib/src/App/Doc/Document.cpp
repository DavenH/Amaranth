#include <iostream>
#include "Document.h"

#include <Util/ScopedFunction.h>

#include "Savable.h"
#include "../AppConstants.h"
#include "../SingletonRepo.h"
#include "../../Util/Util.h"
#include "../../UI/IConsole.h"
#include "../../Definitions.h"

Document::Document(SingletonRepo* repo) : SingletonAccessor(repo, "Document"), validator(nullptr) {
}

bool Document::open(const String& filename) {
    File file(filename);

    info("opening " << file.getFileName() << "\n");

    if (!file.existsAsFile()) {
        info("file " << filename << " does not exist\n");
        return false;
    }

    std::unique_ptr<InputStream> stream(file.createInputStream());
    return open(stream.get());
}

bool Document::validate() {
    if (validator == nullptr) {
        return true;
    }

    if (validator->isDemoMode()) {
        showConsoleMsg("Saving presets is disabled in this demo");
        return false;
    }

    return true;
}

void Document::save(const String& filename) {
    if (!validate()) {
        return;
    }

    File saveFile(filename);

    if (saveFile.existsAsFile()) {
        (void) saveFile.deleteFile();
    }

    std::unique_ptr outStream(saveFile.createOutputStream());

    save(outStream.get());
}

void Document::save(OutputStream* outStream) {
    if (!validate()) {
        return;
    }

    if (outStream == nullptr) {
        showConsoleMsg("Problem saving preset");
        return;
    }

    std::unique_ptr<XmlElement> topelem(new XmlElement("Preset"));
    topelem->setAttribute("VersionValue", getRealConstant(ProductVersion));

    for (auto savableItem: savableItems) {
        savableItem->writeXML(topelem.get());
    }

    saveHeader(outStream, details, getConstant(DocMagicCode));

    String docString = topelem->toString();
    CharPointer_UTF8 utf8Data = docString.toUTF8();

    GZIPCompressorOutputStream gzipStream(outStream, 5);
    gzipStream.write(utf8Data, utf8Data.sizeInBytes());
    gzipStream.flush();
}

#ifdef JUCE_DEBUG
String Document::getPresetString() {
    std::unique_ptr<XmlElement> topelem(new XmlElement("Preset"));
    topelem->setAttribute("VersionValue", getRealConstant(ProductVersion));

    for (auto savableItem: savableItems) {
        savableItem->writeXML(topelem.get());
    }

    return topelem->toString();
}
#endif

bool Document::open(InputStream* stream) {
    // stream can have additional header info
    // from plugin's config settings
    int64 startPosition = stream->getPosition();

    if (!readHeader(stream, details, Constants::DocMagicCode)) {
        return false;
    }

    ScopedLambda loadToggle(
        [this] { listeners.call(&Listener::documentAboutToLoad); },
        [this] { listeners.call(&Listener::documentHasLoaded); }
    );

    stream->setPosition(startPosition + (int64) headerSizeBytes);

    GZIPDecompressorInputStream decompStream(stream, false);
    String presetDocString(decompStream.readEntireStreamAsString());

    XmlDocument presetDoc(presetDocString);
    std::unique_ptr topelem(presetDoc.getDocumentElement());

    jassert(topelem != nullptr);

    if (topelem) {
        for (auto savableItem: savableItems) {
            (void) savableItem->readXML(topelem.get());
        }
    }

    return true;
}

bool Document::saveHeaderValidated(DocumentDetails& updatedDetails) {
    if (!validate()) {
        return false;
    }

    File file(updatedDetails.getFilename());
    std::unique_ptr out(file.createOutputStream());

    return saveHeader(out.get(), updatedDetails, getConstant(DocMagicCode), true, true);
}
