#include "TrimeshPanelEnvironment.h"

#include <App/AppConstants.h>
#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <Curve/Mesh/Vertex.h>
#include <Design/Updating/Updater.h>
#include <UI/MiscGraphics.h>

namespace CycleV2 {

TrimeshPanelEnvironment::TrimeshPanelEnvironment() :
        console(&repo) {
    repo.add(new AppConstants(&repo));
    repo.add(new MiscGraphics(&repo));
    repo.add(new Settings(&repo));
    repo.add(new MeshLibrary(&repo));
    repo.add(new EditWatcher(&repo));
    repo.add(new Updater(&repo));

    auto& constants = repo.get<AppConstants>("AppConstants");
    constants.setConstant(Constants::FontFace, String("Verdana"));

    repo.get<MiscGraphics>("MiscGraphics").init();

    auto& settings = repo.get<Settings>("Settings");
    settings.initialiseSettings();
    settings.createPropertiesFile(
            File::getSpecialLocation(File::tempDirectory)
                    .getChildFile("cycle-v2-trimesh-bridge-settings.xml")
                    .getFullPathName());
    settings.getGlobalSetting(AppSettings::DrawScales) = false;
    repo.setConsole(&console);
    repo.setMorphPositioner(&morphPositioner);
}

void TrimeshPanelEnvironment::setMorphPosition(
        const MorphPosition& position,
        int primaryAxis) {
    morphPositioner.setPosition(position, primaryAxis);
}

TrimeshPanelEnvironment::NullConsole::NullConsole(SingletonRepo* repo) :
        IConsole(repo, "CycleV2TrimeshNullConsole") {
}

void TrimeshPanelEnvironment::NodeMorphPositioner::setPosition(
        const MorphPosition& position,
        int primaryAxis) {
    morph = position;
    primaryDimension = primaryAxis;
}

float TrimeshPanelEnvironment::NodeMorphPositioner::getValue(int dim) {
    switch (dim) {
        case Vertex::Time: return morph.time.getCurrentValue();
        case Vertex::Red:  return morph.red.getCurrentValue();
        case Vertex::Blue: return morph.blue.getCurrentValue();
        default:           return 0.f;
    }
}

}
