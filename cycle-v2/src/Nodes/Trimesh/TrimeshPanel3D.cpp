#include "TrimeshPanel3D.h"

namespace CycleV2 {

TrimeshPanel3D::TrimeshPanel3D(SingletonRepo* repo, TrimeshPanelDataSource& source) :
        SingletonAccessor  (repo, "CycleV2TrimeshPanel3D")
    ,   Panel3D            (repo, "CycleV2TrimeshPanel3D", &source, 0, false, true)
    ,   dataSource  (source)
{}

}
