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

bool Document::readHeader(InputStream* stream, DocumentDetails& deets, int magicValue) {
    if (stream == nullptr) {
        return false;
    }

    int code = stream->readInt();

    if (code == magicValue) {
        int deetsSize = stream->readInt();
        jassert(deetsSize > 0 && deetsSize <= headerSizeBytes - 2);

        String detailsString(stream->readString());
        XmlDocument deetsDoc(detailsString);
        std::unique_ptr deetsElem(deetsDoc.getDocumentElement());

        jassert(deetsDoc.getLastParseError().isEmpty());
        jassert(deetsElem != nullptr);

        if (deetsElem == nullptr) {
            return false;
        }

        stream->setPosition(headerSizeBytes);

        deets.readXML(deetsElem.get());
        return true;
    }

    return false;
}

bool Document::saveHeader(OutputStream* stream,
                          DocumentDetails& updatedDetails,
                          int magicValue,
                          bool preserveRevision,
                          bool preserveDate) {
    const String& filename = updatedDetails.getFilename();

    File file(filename);
    if (! file.existsAsFile()) {
        jassertfalse;
        return false;
    }

    std::unique_ptr<InputStream> in(file.createInputStream());

    int firstByte = in->readInt();
    if (firstByte != magicValue) {
        return false;
    }

    ScopedValueSetter revisionSuppressor(updatedDetails.getSuppressRevFlag(), ! preserveRevision, false);
    ScopedValueSetter dateSuppressor(updatedDetails.getSuppressDateFlag(), ! preserveDate, false);
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

    return true;
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

    if (!readHeader(stream, details, getConstant(DocMagicCode))) {
        return false;
    }

    ScopedLambda loadToggle(
        [this] { listeners.call(&Listener::documentAboutToLoad); },
        [this] { listeners.call(&Listener::documentHasLoaded); }
    );

    stream->setPosition(startPosition + (int64) headerSizeBytes);

    GZIPDecompressorInputStream decompStream(stream, false);
    String presetDocString(decompStream.readEntireStreamAsString());
    DBG("Document::open preset XML:\n" + presetDocString);

    XmlDocument presetDoc(presetDocString);
    std::unique_ptr topelem(presetDoc.getDocumentElement());

    String parseError = presetDoc.getLastParseError();
    if (parseError.isNotEmpty()) {
        DBG("Document::open parse error: " + parseError);
    }

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
