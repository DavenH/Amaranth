#include "TrimeshPanel2D.h"

namespace CycleV2 {

TrimeshPanel2D::TrimeshPanel2D(SingletonRepo* repo) :
        SingletonAccessor  (repo, "CycleV2TrimeshPanel2D")
    ,   Panel2D            (repo, "CycleV2TrimeshPanel2D", true, true)
{}

}
