#include <iostream>
#include "Document.h"

#include <Util/ScopedFunction.h>

#include "PresetJson.h"
#include "Savable.h"
#include "../AppConstants.h"
#include "../SingletonRepo.h"
#include "../../Util/Util.h"
#include "../../UI/IConsole.h"
#include "../../Definitions.h"

Document::Document(SingletonRepo* repo) : SingletonAccessor(repo, "Document"), validator(nullptr) {
}

Identifier Document::getJsonSectionKey(Savable* savableItem) {
    auto* accessor = dynamic_cast<SingletonAccessor*>(savableItem);

    if (accessor == nullptr) {
        return {};
    }

    const String& name = accessor->getName();

    if (name == "MeshLibrary") {
        return "meshLibrary";
    }

    if (name == "OscControlPanel") {
        return "oscControls";
    }

    if (name == "EffectGuiRegistry") {
        return "effects";
    }

    if (name == "Settings") {
        return "settings";
    }

    if (name == "MainPanel") {
        return "mainPanel";
    }

    if (name == "ModMatrixPanel") {
        return "modMatrix";
    }

    if (name == "GuideCurvePanel") {
        return "guideCurveProps";
    }

    if (name == "MorphPanel") {
        return "morphPanel";
    }

    if (name == "Envelope2D") {
        return "envelopeProps";
    }

    if (name == "Multisample") {
        return "multisample";
    }

    return {};
}

var Document::createJsonRoot(DocumentDetails& details, const Array<Savable*>& savableItems) {
    auto root = PresetJson::object();
    auto preset = PresetJson::object();

    root->setProperty("format", "amaranth-preset");
    root->setProperty("schemaVersion", 2);
    preset->setProperty("details", details.writeJSON());

    for (auto savableItem : savableItems) {
        Identifier sectionKey = getJsonSectionKey(savableItem);

        if (sectionKey.isNull()) {
            continue;
        }

        var sectionJson = savableItem->writeJSON();

        if (!sectionJson.isVoid()) {
            preset->setProperty(sectionKey, sectionJson);
        }
    }

    root->setProperty("preset", PresetJson::toVar(preset));
    return PresetJson::toVar(root);
}

bool Document::applyJsonRoot(const var& root) {
    var preset = PresetJson::property(root, "preset");

    if (PresetJson::getObject(root) == nullptr || PresetJson::getObject(preset) == nullptr) {
        return false;
    }

    if (var detailsJson = PresetJson::property(preset, "details"); !detailsJson.isVoid()) {
        (void) details.readJSON(detailsJson);
    }

    for (auto savableItem : savableItems) {
        Identifier sectionKey = getJsonSectionKey(savableItem);

        if (sectionKey.isNull()) {
            continue;
        }

        var sectionJson = PresetJson::property(preset, sectionKey);

        if (!sectionJson.isVoid()) {
            (void) savableItem->readJSON(sectionJson);
        }
    }

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

    saveHeader(outStream, details, getConstant(DocMagicCode));
    String docString = JSON::toString(createJsonRoot(details, savableItems), true);
    CharPointer_UTF8 utf8Data = docString.toUTF8();

    GZIPCompressorOutputStream gzipStream(outStream, 5);
    gzipStream.write(utf8Data, utf8Data.sizeInBytes());
    gzipStream.flush();
}

#ifdef JUCE_DEBUG
String Document::getPresetString() {
    return JSON::toString(createJsonRoot(details, savableItems), true);
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
    String trimmed = presetDocString.trimStart();
    var jsonRoot;

    if (trimmed.startsWithChar('{')) {
        jsonRoot = JSON::parse(trimmed);
    } else {
        XmlDocument presetDoc(presetDocString);
        std::unique_ptr<XmlElement> topelem(presetDoc.getDocumentElement());

        if (topelem == nullptr) {
            return false;
        }

        jsonRoot = PresetMigrator::migrateXmlToCurrentJson(topelem.get(), details);
    }

    return applyJsonRoot(jsonRoot);
}

bool Document::saveHeaderValidated(DocumentDetails& updatedDetails) {
    if (!validate()) {
        return false;
    }

    File file(updatedDetails.getFilename());
    std::unique_ptr out(file.createOutputStream());

    return saveHeader(out.get(), updatedDetails, getConstant(DocMagicCode), true, true);
}
