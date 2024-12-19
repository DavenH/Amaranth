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
        String productName(ProjectInfo::projectName);

        contentDir = File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName()
                     + File::getSeparatorString() + "Amaranth Audio"
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

String Directories::getPresetDir() {
    return contentDir + (contentDir.endsWithChar(File::getSeparatorChar()) ? String() : File::getSeparatorString()) +
           "presets" + File::getSeparatorString();
}

String Directories::getUserPresetDir() {
    return getPresetDir() + "user" + File::getSeparatorString();
}

String Directories::getMeshDir() {
    return contentDir + (contentDir.endsWithChar(File::getSeparatorChar()) ? String() : File::getSeparatorString()) +
           "mesh" + File::getSeparatorString();
}

String Directories::getUserMeshDir() {
    return getMeshDir() + "user" + File::getSeparatorString();
}

String Directories::getTutorialDir() {
    return contentDir + (contentDir.endsWithChar(File::getSeparatorChar()) ? String() : File::getSeparatorString()) +
           "tutorials";
}
