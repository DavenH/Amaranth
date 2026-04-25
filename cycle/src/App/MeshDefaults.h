#pragma once

class Mesh;
class SingletonRepo;

namespace MeshDefaults {
    void initialiseIfNeeded(SingletonRepo* repo, int layerType, Mesh* mesh);
    void migrateLegacyPaddingIfNeeded(SingletonRepo* repo, int layerType, Mesh* mesh);
}
