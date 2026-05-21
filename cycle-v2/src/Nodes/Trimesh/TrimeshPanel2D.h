#pragma once

#include <UI/Panels/Panel2D.h>

namespace CycleV2 {

class TrimeshPanel2D : public Panel2D {
public:
    explicit TrimeshPanel2D(SingletonRepo* repo);

    void panelResized() override;
};

}
