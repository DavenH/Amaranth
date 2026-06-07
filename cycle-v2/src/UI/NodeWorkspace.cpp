#include "NodeWorkspace.h"

namespace CycleV2 {

NodeWorkspace::NodeWorkspace() {
    setOpaque(true);
    addAndMakeVisible(canvas);
}

bool NodeWorkspace::saveGraphToFile(const File& file) {
    return canvas.saveGraphToFile(file);
}

bool NodeWorkspace::loadGraphFromFile(const File& file) {
    return canvas.loadGraphFromFile(file);
}

void NodeWorkspace::resized() {
    canvas.setBounds(getLocalBounds());
}

}
