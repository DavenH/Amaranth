#include "Nodes/Trimesh/Panel/TrimeshPanelEnvironment.h"

#include <App/AppConstants.h>
#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <Curve/Mesh/PathRepo.h>
#include <Curve/Mesh/Vertex.h>
#include <Design/Updating/Updater.h>
#include <UI/MiscGraphics.h>
#include <Util/LogRegions.h>

namespace CycleV2 {

TrimeshPanelEnvironment::TrimeshPanelEnvironment() :
        console(&repo) {
    repo.add(new AppConstants(&repo));
    repo.add(new MiscGraphics(&repo));
    repo.add(new Settings(&repo));
    repo.add(new MeshLibrary(&repo));
    repo.add(new PathRepo(&repo));
    repo.add(new EditWatcher(&repo));
    repo.add(new Updater(&repo));
    repo.add(new LogRegions(&repo));

    auto& constants = repo.get<AppConstants>("AppConstants");
    constants.setConstant(Constants::FontFace, String("Verdana"));
    constants.setConstant(Constants::MinLineLength, 0.001);
    constants.setConstant(Constants::LogFreqTensionScale, 0.5);

    repo.get<MiscGraphics>("MiscGraphics").init();
    repo.get<PathRepo>("PathRepo").init();
    repo.get<LogRegions>("LogRegions").init();

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

void TrimeshPanelEnvironment::setAxisLinks(
        bool yellow,
        bool red,
        bool blue) {
    auto& settings = repo.get<Settings>("Settings");
    settings.getGlobalSetting(AppSettings::LinkYellow) = yellow;
    settings.getGlobalSetting(AppSettings::LinkRed) = red;
    settings.getGlobalSetting(AppSettings::LinkBlue) = blue;
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
