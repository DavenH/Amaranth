#include <App/AppConstants.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>

#include "Directories.h"

#include "../CycleDefs.h"
#include "../UI/Dialogs/PresetPage.h"

Directories::Directories(SingletonRepo* repo)
	: 	SingletonAccessor(repo, "Directories")
	, 	homeUrl("http://www.amaranthaudio.com")
	, 	loadedWave(String::empty)
{
}

void Directories::init()
{
	contentDir = getObj(Settings).getProperty("ContentDir");

	if(contentDir.isEmpty())
	{
		String productName(editionSplit("Cycle", "Cycle-BE"));

		contentDir = File::getSpecialLocation(File::userDocumentsDirectory).getFullPathName()
				+ File::separatorString + "Amaranth Audio"
				+ File::separatorString + productName
				+ File::separatorString;

		if(! File(contentDir).exists())
		{
			File(contentDir).createDirectory();
		}

		getObj(Settings).setProperty("ContentDir", contentDir);
	}

	File userPresets(getUserPresetDir());
	if(! userPresets.exists())
		userPresets.createDirectory();

	File userMesh(getUserMeshDir());

	if(! userMesh.exists())
		userMesh.createDirectory();

	jassert(! contentDir.isEmpty());
}


String Directories::getPresetDir()
{
	return contentDir + (contentDir.endsWithChar(File::separator) ? String::empty : File::separatorString)
			+ "presets" + File::separator;
}


String Directories::getUserPresetDir()
{
	return getPresetDir() + "user" + File::separatorString;
}


String Directories::getMeshDir()
{
	return contentDir + (contentDir.endsWithChar(File::separator) ? String::empty : File::separatorString)
			+ "mesh" + File::separatorString;
}


String Directories::getUserMeshDir()
{
	return getMeshDir() + "user" + File::separatorString;
}


String Directories::getTutorialDir()
{
	return contentDir + (contentDir.endsWithChar(File::separator) ? String::empty : File::separatorString)
			+ "tutorials";
}
