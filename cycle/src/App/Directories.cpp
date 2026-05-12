#include <App/AppConstants.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>

#include "Directories.h"

#include "../CycleDefs.h"
#include "../UI/Dialogs/PresetPage.h"

Directories::Directories(SingletonRepo* repo)
    : 	SingletonAccessor(repo, "Directories")
    , 	homeUrl("http://www.amaranthaudio.com")
    , 	loadedWave(String())
{
}

namespace {
    String getPresetSettingsPath() {
        String appDataDir = File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName();

      #ifdef JUCE_WINDOWS
        return appDataDir + "/Amaranth Audio/" + ProjectInfo::projectName + "/presets.xml";
      #else
        return appDataDir + "/Preferences/com.amaranthaudio." + ProjectInfo::projectName + ".presets.xml";
      #endif
    }

    String getDefaultContentDir() {
      #ifdef JUCE_MAC
        File baseDir = File::getSpecialLocation(File::userApplicationDataDirectory)
                .getChildFile("Application Support");
      #else
        File baseDir = File::getSpecialLocation(File::userDocumentsDirectory);
      #endif

        return baseDir.getFullPathName()
               + File::getSeparatorString() + ProjectInfo::companyName
               + File::getSeparatorString() + ProjectInfo::projectName
               + File::getSeparatorString();
    }

    String getLegacyDocumentsContentDir() {
        File documentsDir = File::getSpecialLocation(File::userDocumentsDirectory);

        return documentsDir.getFullPathName()
               + File::getSeparatorString() + ProjectInfo::companyName
               + File::getSeparatorString() + ProjectInfo::projectName
               + File::getSeparatorString();
    }

    String getLegacyLibraryContentDir() {
        File libraryDir = File::getSpecialLocation(File::userApplicationDataDirectory);

        return libraryDir.getFullPathName()
               + File::getSeparatorString() + ProjectInfo::companyName
               + File::getSeparatorString() + ProjectInfo::projectName
               + File::getSeparatorString();
    }
}

void Directories::init() {
    contentDir = getObj(Settings).getProperty("ContentDir");

  #ifdef JUCE_MAC
    if (contentDir == getLegacyDocumentsContentDir() || contentDir == getLegacyLibraryContentDir()) {
        contentDir = {};
    }
  #endif

    if (contentDir.isEmpty()) {
        contentDir = getDefaultContentDir();
        if (!File(contentDir).exists()) {
            (void) File(contentDir).createDirectory();
        }

        getObj(Settings).setProperty("ContentDir", contentDir);
    }

    File presets(getPresetDir());

    if (!presets.exists()) {
        (void) presets.createDirectory();
    }

    File userPresets(getUserPresetDir());
    if (!userPresets.exists()) {
        (void) userPresets.createDirectory();
    }

    File userMesh(getUserMeshDir());

    if (!userMesh.exists()) {
        (void) userMesh.createDirectory();
    }

    getObj(AppConstants).setConstant(Constants::DocumentsDir, getPresetDir());
    getObj(AppConstants).setConstant(Constants::DocSettingsDir, getPresetSettingsPath());

    jassert(! contentDir.isEmpty());
}

String Directories::getRepoPresetDir() const {
    File repoPresetDir = File(String(CYCLE_SOURCE_DIR)).getChildFile("content").getChildFile("presets");

    if (!repoPresetDir.isDirectory()) {
        return {};
    }

    return repoPresetDir.getFullPathName() + File::getSeparatorString();
}

String Directories::getPresetDir() const {
    String path = contentDir;
    if (!path.endsWithChar(File::getSeparatorChar())) {
        path += File::getSeparatorString();
    }
    return path + "presets" + File::getSeparatorString();
}

StringArray Directories::getPresetSearchDirs() const {
    StringArray dirs;

    dirs.add(getPresetDir());

    String repoPresetDir = getRepoPresetDir();
    if (repoPresetDir.isNotEmpty()) {
        dirs.addIfNotAlreadyThere(repoPresetDir);
    }

    dirs.add(getUserPresetDir());
    DBG("Directories::getPresetSearchDirs -> " + dirs.joinIntoString(" | "));
    return dirs;
}

String Directories::getUserPresetDir() const {
    return getPresetDir() + "user" + File::getSeparatorString();
}

String Directories::getMeshDir() const {
    String path = contentDir;
    if (!path.endsWithChar(File::getSeparatorChar())) {
        path += File::getSeparatorString();
    }
    return path + "mesh" + File::getSeparatorString();
}

String Directories::getRepoMeshDir() const {
    File repoMeshDir = File(String(CYCLE_SOURCE_DIR)).getChildFile("content").getChildFile("mesh");

    if (!repoMeshDir.isDirectory()) {
        return {};
    }

    return repoMeshDir.getFullPathName() + File::getSeparatorString();
}

String Directories::getUserMeshDir() const {
    return getMeshDir() + "user" + File::getSeparatorString();
}

String Directories::getTutorialDir() const {
    String path = contentDir;
    if (!path.endsWithChar(File::getSeparatorChar())) {
        path += File::getSeparatorString();
    }
    return path + "tutorials" + File::getSeparatorString();
}
