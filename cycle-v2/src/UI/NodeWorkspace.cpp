#include "NodeWorkspace.h"

namespace CycleV2 {

NodeWorkspace::NodeWorkspace() {
    setOpaque(true);
    addAndMakeVisible(canvas);
}

void NodeWorkspace::resized() {
    canvas.setBounds(getLocalBounds());
}

}
