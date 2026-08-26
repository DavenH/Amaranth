#pragma once

#include "Nodes/Curve/Editor/CurveExpandedEditorComponent.h"

#include <memory>

namespace CycleV2 {

std::unique_ptr<CurveExpandedEditorComponent> createCurveNodeEditor(
        NodeKind kind,
        CurveEditorWidget& widget);

}
