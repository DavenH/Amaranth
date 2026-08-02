#pragma once

#include <memory>

namespace CycleV2 {

class NodeEditorFactory;

std::unique_ptr<NodeEditorFactory> createUnisonNodeEditorFactory();

}
