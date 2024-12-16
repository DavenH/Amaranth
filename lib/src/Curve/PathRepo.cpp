#include "PathRepo.h"
#include "EnvelopeMesh.h"
#include "../App/AppConstants.h"
#include "../App/MeshLibrary.h"
#include "../Algo/Resampling.h"
#include "../App/SingletonRepo.h"
#include "../Design/Updating/Updater.h"
#include "../Util/CommonEnums.h"
#include "../Definitions.h"

PathRepo::PathRepo(SingletonRepo* repo) : SingletonAccessor(repo, "PathRepo") {
}

void PathRepo::init() {
    meshes = &getObj(MeshLibrary);
}

const PathRepo::ScratchContext& PathRepo::getScratchContext(int scratchChannel) {
    if (scratchChannel == CommonEnums::Null || scratchChannel >= (int) contexts.size()) {
        return dummyContext;
    }

    return contexts[scratchChannel];
}

float PathRepo::getScratchPosition(int scratchChannel, float defaultPos) {
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

void PathRepo::performUpdate(int updateType) {
}

void PathRepo::updateCache(int groupId, int layer) {
    int numGroups = meshes->getNumGroups();

	MeshLibrary::LayerGroup& group = meshes->getGroup(groupId);
	if(group.meshType == LayerGroups::GroupScratch)
		return;

	MeshLibrary::Layer& lyr = group.layers[layer];

	if(lyr.props == nullptr || lyr.props->scratchChan == CommonEnums::Null) {
		return;
	}

	for(auto cube : lyr.mesh->getCubes()) {
    	// TODO what is the rest of this?
	}
}

void PathRepo::resetCaches() {
    caches.clear();
}

PathRepo::PathCache& PathRepo::getPathCache(VertCube* cube) {
    if (caches.find(cube) == caches.end()) {
	    return dummyCache;
    }

	return caches[cube];
}

void PathRepo::documentAboutToLoad() {
    resetCaches();
}

void PathRepo::documentHasLoaded() {
    performUpdate(Update);
}

void PathRepo::cubesRemoved(const vector<VertCube*>& cubes) {
	for(auto cube : cubes) {
		caches[cube] = PathCache();
	}
}

void PathRepo::cubesAdded(const vector<VertCube*>& cubes) {
	// TODO
	for(auto cube : cubes) {
		caches[cube] = PathCache();
	}
}

