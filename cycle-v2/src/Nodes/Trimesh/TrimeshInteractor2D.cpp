#include "TrimeshInteractor2D.h"

namespace CycleV2 {

TrimeshInteractor2D::TrimeshInteractor2D(
        SingletonRepo* repo,
        const String& name,
        const Dimensions& dimensions) :
        SingletonAccessor(repo, name)
    ,   Interactor2D(repo, name, dimensions) {}

}
