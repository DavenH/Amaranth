#pragma once

#include "Effect2DExpandedEditorComponent.h"

#include <memory>

namespace CycleV2 {

class WaveshaperEditorComponent final : public Effect2DExpandedEditorComponent {
public:
    explicit WaveshaperEditorComponent(Effect2DWidget& widget) : Effect2DExpandedEditorComponent(widget) {}
    static StringArray controlIds() { return { "enabled", "pre", "post", "aaFactor" }; }
};

class ImpulseResponseEditorComponent final : public Effect2DExpandedEditorComponent {
public:
    explicit ImpulseResponseEditorComponent(Effect2DWidget& widget) : Effect2DExpandedEditorComponent(widget) {}
    static StringArray controlIds() { return { "enabled", "size", "post", "highPass" }; }
};

class GuideCurveEditorComponent final : public Effect2DExpandedEditorComponent {
public:
    explicit GuideCurveEditorComponent(Effect2DWidget& widget) : Effect2DExpandedEditorComponent(widget) {}
    static StringArray controlIds() { return { "enabled", "noise", "dcOffset", "phase" }; }
};

class EnvelopeEditorComponent final : public Effect2DExpandedEditorComponent {
public:
    explicit EnvelopeEditorComponent(Effect2DWidget& widget) : Effect2DExpandedEditorComponent(widget) {}
    static StringArray controlIds() { return { "logarithmic", "red", "blue", "level" }; }
};

std::unique_ptr<Effect2DExpandedEditorComponent> createCurveNodeEditor(
        NodeKind kind,
        Effect2DWidget& widget);

}
