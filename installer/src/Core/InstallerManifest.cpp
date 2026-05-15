#include "InstallerManifest.h"

namespace {

String getPropertyString(const DynamicObject& object, const Identifier& key, const String& fallback = String()) {
    const var value = object.getProperty(key);
    return value.isString() ? value.toString() : fallback;
}

bool getPropertyBool(const DynamicObject& object, const Identifier& key, bool fallback) {
    const var value = object.getProperty(key);
    return value.isBool() ? static_cast<bool>(value) : fallback;
}

String replaceToken(String path, const String& token, const File& file) {
    return path.replace("${" + token + "}", file.getFullPathName());
}

}

Result InstallerManifest::loadFromFile(const File& manifestFile) {
    if (! manifestFile.existsAsFile()) {
        return Result::fail("Installer manifest does not exist: " + manifestFile.getFullPathName());
    }

    var root = JSON::parse(manifestFile.loadFileAsString());
    auto* object = root.getDynamicObject();
    if (object == nullptr) {
        return Result::fail("Installer manifest is not a JSON object.");
    }

    sourceFile = manifestFile;
    productId = requireString(*object, "productId");
    productName = requireString(*object, "productName");
    version = getPropertyString(*object, "version", "0.0.0");
    companyName = getPropertyString(*object, "companyName", "Amaranth Audio");
    packageName = getPropertyString(*object, "packageName", productName);
    zipName = getPropertyString(*object, "zipName", productName + ".zip");

    if (productId.isEmpty() || productName.isEmpty()) {
        return Result::fail("Installer manifest requires productId and productName.");
    }

    artifacts.clear();
    const var artifactList = object->getProperty("artifacts");
    auto* artifactArray = artifactList.getArray();
    if (artifactArray == nullptr || artifactArray->isEmpty()) {
        return Result::fail("Installer manifest requires a non-empty artifacts array.");
    }

    for (const auto& artifactVar : *artifactArray) {
        InstallerArtifact artifact;
        const Result result = parseArtifact(artifactVar, artifact);
        if (result.failed()) {
            return result;
        }
        artifacts.add(artifact);
    }

    return Result::ok();
}

String InstallerManifest::resolvePath(const String& path) const {
    String resolved = path;
    const File home = File::getSpecialLocation(File::userHomeDirectory);
    const File applications = File::getSpecialLocation(File::globalApplicationsDirectory);

    resolved = replaceToken(resolved, "userHome", home);
    resolved = replaceToken(resolved, "applications", applications);
    resolved = replaceToken(resolved, "userApplications", home.getChildFile("Applications"));

  #if JUCE_MAC
    resolved = resolved.replace("${systemAudioPlugIns}", "/Library/Audio/Plug-Ins");
    resolved = resolved.replace("${userAudioPlugIns}", home.getChildFile("Library/Audio/Plug-Ins").getFullPathName());
  #elif JUCE_LINUX
    resolved = resolved.replace("${systemAudioPlugIns}", "/usr/lib");
    resolved = resolved.replace("${userAudioPlugIns}", home.getChildFile(".local/lib").getFullPathName());
  #else
    resolved = resolved.replace("${systemAudioPlugIns}", home.getFullPathName());
    resolved = resolved.replace("${userAudioPlugIns}", home.getFullPathName());
  #endif

    return resolved.replaceCharacter('\\', File::getSeparatorChar());
}

String InstallerManifest::getPlatformKey() const {
  #if JUCE_MAC
    return "mac";
  #elif JUCE_WINDOWS
    return "windows";
  #elif JUCE_LINUX
    return "linux";
  #else
    return "unknown";
  #endif
}

Result InstallerManifest::parseArtifact(const var& artifactVar, InstallerArtifact& artifact) const {
    auto* object = artifactVar.getDynamicObject();
    if (object == nullptr) {
        return Result::fail("Artifact entry is not a JSON object.");
    }

    artifact.id = requireString(*object, "id");
    artifact.name = requireString(*object, "name");
    artifact.source = requireString(*object, "source");
    artifact.required = getPropertyBool(*object, "required", false);

    if (auto* sourcesObject = object->getProperty("sources").getDynamicObject()) {
        const String platformSource = getPropertyString(*sourcesObject, getPlatformKey());
        if (platformSource.isNotEmpty()) {
            artifact.source = platformSource;
        }
    }

    if (artifact.id.isEmpty() || artifact.name.isEmpty() || artifact.source.isEmpty()) {
        return Result::fail("Artifact entries require id, name, and source or platform sources.");
    }

    return parseDestinations(object->getProperty("destinations"), artifact);
}

Result InstallerManifest::parseDestinations(const var& destinationsVar, InstallerArtifact& artifact) const {
    auto* destinationsObject = destinationsVar.getDynamicObject();
    if (destinationsObject == nullptr) {
        return Result::fail("Artifact " + artifact.id + " has no destinations object.");
    }

    const var platformDestinations = destinationsObject->getProperty(getPlatformKey());
    auto* destinationArray = platformDestinations.getArray();
    if (destinationArray == nullptr) {
        return Result::ok();
    }

    for (const auto& destinationVar : *destinationArray) {
        auto* object = destinationVar.getDynamicObject();
        if (object == nullptr) {
            return Result::fail("Destination in artifact " + artifact.id + " is not an object.");
        }

        InstallerDestination destination;
        destination.id = requireString(*object, "id");
        destination.name = requireString(*object, "name");
        destination.path = requireString(*object, "path");
        destination.enabled = getPropertyBool(*object, "enabled", true);

        if (destination.id.isEmpty() || destination.name.isEmpty() || destination.path.isEmpty()) {
            return Result::fail("Destination entries require id, name, and path.");
        }

        artifact.destinations.add(destination);
    }

    return Result::ok();
}

String InstallerManifest::requireString(const DynamicObject& object, const Identifier& key) {
    return getPropertyString(object, key);
}
