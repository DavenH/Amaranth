#include "PathInventory.h"
#include "../Env/EnvelopeMesh.h"
#include "../../App/AppConstants.h"
#include "../../App/MeshLibrary.h"
#include "../../Algo/Resampling.h"
#include "../../App/SingletonRepo.h"
#include "../../Design/Updating/Updater.h"
#include "../../Util/CommonEnums.h"
#include "../../Definitions.h"

PathInventory::PathInventory(SingletonRepo* repo) : SingletonAccessor(repo, "PathInventory") {
}

void PathInventory::init() {
    meshes = &getObj(MeshLibrary);
}

const PathInventory::ScratchContext& PathInventory::getScratchContext(int scratchChannel) {
    if (scratchChannel == CommonEnums::Null || scratchChannel >= (int) contexts.size()) {
        return dummyContext;
    }

    return contexts[scratchChannel];
}

float PathInventory::getScratchPosition(int scratchChannel, float defaultPos) {
    const ScratchContext& context = getScratchContext(scratchChannel);
    Buffer<float> buffer = context.gridBuffer;

    if(buffer.empty() || scratchChannel == CommonEnums::Null) {
        return defaultPos;
    }

    MeshLibrary::Layer& layer = meshes->getLayer(LayerGroups::GroupScratch, scratchChannel);
    auto* scratchMesh = dynamic_cast<EnvelopeMesh*>(layer.mesh);
    MeshLibrary::Properties* props = layer.props;

    if(scratchChannel >= (int) contexts.size() || scratchMesh == nullptr || props == nullptr) {
        return defaultPos;
    }

    jassert(scratchChannel < (int) contexts.size());

    bool haveScratch = props->active && scratchMesh->hasEnoughCubesForCrossSection();
    return haveScratch ? Resampling::lerpC(buffer, defaultPos) : defaultPos;
}

void PathInventory::performUpdate(UpdateType updateType) {
}

void PathInventory::updateCache(int groupId, int layer) {
    int numGroups = meshes->getNumGroups();

    MeshLibrary::LayerGroup& group = meshes->getLayerGroup(groupId);
    if(group.meshType == LayerGroups::GroupScratch) {
        return;
    }

    MeshLibrary::Layer& lyr = group.layers[layer];

    if(lyr.props == nullptr || lyr.props->scratchChan == CommonEnums::Null) {
        return;
    }

    for(auto cube : lyr.mesh->getCubes()) {
        // TODO what is the rest of this?
    }
}

void PathInventory::resetCaches() {
    caches.clear();
}

PathInventory::PathCache& PathInventory::getPathCache(TrilinearCube* cube) {
    if (caches.find(cube) == caches.end()) {
        return dummyCache;
    }

    return caches[cube];
}

void PathInventory::documentAboutToLoad() {
    resetCaches();
}

void PathInventory::documentHasLoaded() {
    performUpdate(Update);
}

void PathInventory::cubesRemoved(const vector<TrilinearCube*>& cubes) {
    for(auto cube : cubes) {
        caches[cube] = PathCache();
    }
}

void PathInventory::cubesAdded(const vector<TrilinearCube*>& cubes) {
    // TODO
    for(auto cube : cubes) {
        caches[cube] = PathCache();
    }
}

