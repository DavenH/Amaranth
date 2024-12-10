#ifndef _e3interactor_h
#define _e3interactor_h

#include <Inter/Interactor3D.h>
#include "../UI/Widgets/Controls/MeshSelectionClient3D.h"

class EnvelopeInter3D :
        public Interactor3D {
public:
    EnvelopeInter3D(SingletonRepo* repo);
    virtual ~EnvelopeInter3D();
    void init();
    void transferLineProperties(VertCube* from, VertCube* to1, VertCube* to2);
    Mesh* getMesh();
};

#endif
