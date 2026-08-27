#pragma once

#include <memory>

#include "UI/NodeEditorHost.h"

namespace CycleV2 {

std::unique_ptr<NodeEditorFactory> createVoiceContextNodeEditorFactory();

}
