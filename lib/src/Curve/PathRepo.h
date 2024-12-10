#pragma once
#include <map>
#include <vector>
#include "../App/Doc/Document.h"
#include "../App/SingletonAccessor.h"
#include "../Array/Buffer.h"
#include "../Design/Updating/Updateable.h"
#include "../Inter/InteractorListener.h"
#include "../Obj/Ref.h"

class VertCube;
class MeshLibrary;

using std::map;
using std::vector;

class PathRepo :
		public SingletonAccessor
	,	public Updateable
	,	public Document::Listener
	,	public InteractorListener {
public:
    struct ScratchContext {
		Buffer<float> gridBuffer;
		Buffer<float> panelBuffer;
	};

    struct PathCache {
		vector<Vertex2> path;
		VertCube* cube;
	};

    /* ----------------------------------------------------------------------------- */

	explicit PathRepo(SingletonRepo* repo);
	~PathRepo() {}

	void init() override;
	void performUpdate(int updateType) override;
	void updateCache(int group, int layer);
	void resetCaches();
	float getScratchPosition(int scratchChannel, float defaultPos = 0.f);
	const ScratchContext& getScratchContext(int scratchChannel);
	PathCache& getPathCache(VertCube* cube);

	void documentAboutToLoad();
	void documentHasLoaded();

	void cubesRemoved(const vector<VertCube*>& cube);
	void cubesAdded(const vector<VertCube*>& cube);

	CriticalSection& getLock() { return calcLock; }

protected:
	CriticalSection calcLock;
	Ref<MeshLibrary> meshes;

	PathCache dummyCache;
	ScratchContext dummyContext;
	map<VertCube*, PathCache> caches;
	vector<ScratchContext> contexts;
};
