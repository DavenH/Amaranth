#pragma once

#include <map>
#include <vector>

#include "../../App/Doc/Document.h"
#include "../../App/SingletonAccessor.h"
#include "../../Array/Buffer.h"
#include "../../Design/Updating/Updateable.h"
#include "../../Inter/InteractorListener.h"
#include "../../Obj/Ref.h"
#include "../Vertex/Vertex2.h"

class TrilinearCube;
class MeshLibrary;

using std::map;
using std::vector;

class PathInventory :
        public SingletonAccessor
    ,   public Updateable
    ,   public Document::Listener
    ,   public InteractorListener {
public:
    struct ScratchContext {
        Buffer<float> gridBuffer;
        Buffer<float> panelBuffer;
    };

    struct PathCache {
        vector<Vertex2> path;
        TrilinearCube* cube;
    };

    /* ----------------------------------------------------------------------------- */

    explicit PathInventory(SingletonRepo* repo);
    ~PathInventory() override = default;

    void init() override;
    void performUpdate(UpdateType updateType) override;
    void updateCache(int group, int layer);
    void resetCaches();
    float getScratchPosition(int scratchChannel, float defaultPos = 0.f);
    const ScratchContext& getScratchContext(int scratchChannel);
    PathCache& getPathCache(TrilinearCube* cube);

    void documentAboutToLoad() override;
    void documentHasLoaded() override;

    void cubesRemoved(const vector<TrilinearCube*>& cube) override;
    void cubesAdded(const vector<TrilinearCube*>& cube) override;

    CriticalSection& getLock() { return calcLock; }

protected:
    CriticalSection calcLock;
    Ref<MeshLibrary> meshes;

    PathCache dummyCache;
    ScratchContext dummyContext;
    map<TrilinearCube*, PathCache> caches;
    vector<ScratchContext> contexts;
};
