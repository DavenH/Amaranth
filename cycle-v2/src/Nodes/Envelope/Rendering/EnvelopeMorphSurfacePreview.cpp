#include "Nodes/Envelope/Rendering/EnvelopeMorphSurfacePreview.h"

#include "Nodes/Trimesh/Rendering/TrimeshSurfaceRenderer.h"

namespace CycleV2 {

void EnvelopeMorphSurfacePreview::paint(
        Graphics& graphics, Rectangle<float> bounds, Mesh& mesh,
        GuideCurveProvider* guides, uint64_t revision,
        const MorphPosition& morph, int axis, int dimensions) {
    if (bounds.isEmpty() || dimensions == 0) {
        return;
    }
    bool changed = cachedRevision != revision || cachedAxis != axis
            || cachedDimensions != dimensions || !image.isValid();
    for (const int coordinate : MorphDimensions::axes) {
        changed |= coordinate != axis && cachedMorph[coordinate] != morph[coordinate];
    }
    const Point<int> size(roundToInt(bounds.getWidth()), roundToInt(bounds.getHeight()));
    if (changed) {
        constexpr int rows = 128;
        constexpr int columns = 48;
        data.rows = rows;
        data.columns = columns;
        data.cyclic = false;
        data.surface.resize(rows * columns);
        std::vector<Column> slices;
        slices.reserve(columns);
        for (int column = 0; column < columns; ++column) {
            slices.emplace_back(data.surface.data() + column * rows, rows,
                    (float) column / (columns - 1), 60);
        }
        rasterizer.setDimensionality(dimensions);
        rasterizer.setGuideCurveProvider(guides);
        rasterizer.renderSurface(&mesh, slices, rows, axis, morph);
        cachedRevision = revision;
        cachedMorph = morph;
        cachedAxis = axis;
        cachedDimensions = dimensions;
    }
    if (changed || cachedSize != size) {
        const auto profile = TrimeshRenderProfile::fromDomain(PortDomain::EnvelopeSignal);
        image = TrimeshSurfaceRenderer::createTrimeshImage(data, profile,
                bounds.getWidth() / jmax(1.f, bounds.getHeight()), size.y);
        cachedSize = size;
    }
    graphics.setImageResamplingQuality(Graphics::lowResamplingQuality);
    graphics.drawImage(image, bounds);
}

}
