#pragma once

#include <Inter/Interactor3D.h>
#include "../UI/Widgets/Controls/MeshSelectionClient3D.h"

class EnvelopeInter3D :
        public Interactor3D {
public:
    explicit EnvelopeInter3D(SingletonRepo* repo);

    void init() override;
    void transferLineProperties(TrilinearCube* from, TrilinearCube* to1, TrilinearCube* to2);
    Mesh* getMesh() override;
};
