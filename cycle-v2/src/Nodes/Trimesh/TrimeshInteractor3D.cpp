#include "TrimeshInteractor3D.h"

namespace CycleV2 {

TrimeshInteractor3D::TrimeshInteractor3D(SingletonRepo* repo, const String& name) :
        SingletonAccessor(repo, name)
    ,   Interactor3D(repo, name) {}

}
