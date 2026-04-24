#pragma once

class Mesh;
class SingletonRepo;

namespace EffectMeshDefaults {
    void initialiseIfNeeded(SingletonRepo* repo, int layerType, Mesh* mesh);
    void migrateLegacyPaddingIfNeeded(SingletonRepo* repo, int layerType, Mesh* mesh);
}
