#include "Nodes/Curve/Editor/CurveNodeEditorFactory.h"

#include "Nodes/Envelope/Editor/EnvelopeEditorComponent.h"
#include "Nodes/ImpulseResponse/Editor/ImpulseResponseEditorComponent.h"
#include "Nodes/Waveshaper/Editor/WaveshaperEditorComponent.h"

namespace CycleV2 {

std::unique_ptr<CurveExpandedEditorComponent> createCurveNodeEditor(
        NodeKind kind,
        CurveEditorWidget& widget) {
    switch (kind) {
        case NodeKind::Waveshaper:
            return std::make_unique<WaveshaperEditorComponent>(widget);
        case NodeKind::ImpulseResponse:
            return std::make_unique<ImpulseResponseEditorComponent>(widget);
        case NodeKind::Envelope:
            return std::make_unique<EnvelopeEditorComponent>(widget);
        default:
            return {};
    }
}

}
