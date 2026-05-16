#pragma once

#include <JuceHeader.h>

using namespace juce;

struct InstallerDestination {
    String id;
    String name;
    String path;
    bool enabled = true;
};

struct InstallerArtifact {
    String id;
    String name;
    String source;
    bool required = false;
    Array<InstallerDestination> destinations;
};

class InstallerManifest {
public:
    Result loadFromFile(const File& manifestFile);

    String resolvePath(const String& path) const;
    String getPlatformKey() const;

    const String& getProductId() const      { return productId; }
    const String& getProductName() const    { return productName; }
    const String& getVersion() const        { return version; }
    const String& getCompanyName() const    { return companyName; }
    const String& getPackageName() const    { return packageName; }
    const String& getZipName() const        { return zipName; }
    const File&   getManifestFile() const   { return sourceFile; }

    const Array<InstallerArtifact>& getArtifacts() const { return artifacts; }
    const StringArray& getEulaDefinitions() const        { return eulaDefinitions; }
    const StringArray& getEulaRestrictions() const       { return eulaRestrictions; }
    const StringArray& getEulaGrants() const             { return eulaGrants; }

private:
    Result parseArtifact(const var& artifactVar, InstallerArtifact& artifact) const;
    Result parseDestinations(const var& destinationsVar, InstallerArtifact& artifact) const;
    void parseEula(const DynamicObject& object);

    static String requireString(const DynamicObject& object, const Identifier& key);
    static StringArray optionalStringArray(const var& value);

    File sourceFile;
    String productId;
    String productName;
    String version;
    String companyName;
    String packageName;
    String zipName;
    StringArray eulaDefinitions;
    StringArray eulaRestrictions;
    StringArray eulaGrants;
    Array<InstallerArtifact> artifacts;
};
