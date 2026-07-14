#include "CurveNodeEditors.h"

namespace CycleV2 {

std::unique_ptr<Effect2DExpandedEditorComponent> createCurveNodeEditor(
        NodeKind kind,
        Effect2DWidget& widget) {
    switch (kind) {
        case NodeKind::Waveshaper:       return std::make_unique<WaveshaperEditorComponent>(widget);
        case NodeKind::ImpulseResponse:  return std::make_unique<ImpulseResponseEditorComponent>(widget);
        case NodeKind::GuideCurve:       return std::make_unique<GuideCurveEditorComponent>(widget);
        case NodeKind::Envelope:         return std::make_unique<EnvelopeEditorComponent>(widget);
        default:                         return {};
    }
}

}
