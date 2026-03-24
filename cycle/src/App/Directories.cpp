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

void Directories::init() {
    contentDir = getObj(Settings).getProperty("ContentDir");

    if (contentDir.isEmpty()) {
        String productName(getStrConstant(ProductName));

        contentDir = File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName()
                     + File::getSeparatorString() + ProjectInfo::companyName
                     + File::getSeparatorString() + productName
                     + File::getSeparatorString();

        if (!File(contentDir).exists()) {
            (void) File(contentDir).createDirectory();
        }

        getObj(Settings).setProperty("ContentDir", contentDir);
    }

    File userPresets(getUserPresetDir());
    if (!userPresets.exists()) {
        (void) userPresets.createDirectory();
    }

    File userMesh(getUserMeshDir());

    if (!userMesh.exists()) {
        (void) userMesh.createDirectory();
    }

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
