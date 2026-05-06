#pragma once

#include <vector>

#include "../RasterizerConversion.h"
#include "../../Vertex.h"

namespace Rasterization {
    class VertexListSource {
    public:
        VertexListSource() = default;

        explicit VertexListSource(std::vector<Vertex*>* vertices) :
                vertices(vertices) {
        }

        bool empty() const {
            return vertices == nullptr || vertices->empty();
        }

        int size() const {
            return vertices == nullptr ? 0 : (int) vertices->size();
        }

        RasterPoint pointAt(int index) const {
            Vertex* vertex = vertexAt(index);
            float* values = vertex->values;

            RasterPoint point;
            point.x = values[xDimension];
            point.y = values[yDimension];
            point.sharpness = values[Vertex::Curve];
            point.adjustedX = point.x;
            point.source = RasterPointSource::fxVertex(index);

            return point;
        }

        void copyInterceptsTo(std::vector<Intercept>& intercepts) const {
            intercepts.clear();

            if (vertices == nullptr) {
                return;
            }

            intercepts.reserve(vertices->size());

            for (int i = 0; i < (int) vertices->size(); ++i) {
                if ((*vertices)[i] != nullptr) {
                    intercepts.emplace_back(toIntercept(pointAt(i)));
                }
            }
        }

        Vertex* vertexAt(int index) const {
            jassert(vertices != nullptr);
            jassert(index >= 0 && index < (int) vertices->size());
            return (*vertices)[index];
        }

        void setDimensions(int newXDimension, int newYDimension) {
            xDimension = newXDimension;
            yDimension = newYDimension;
        }

        void setVertices(std::vector<Vertex*>* newVertices) {
            vertices = newVertices;
        }

        void clear() {
            vertices = nullptr;
        }

    private:
        int xDimension { Vertex::Phase };
        int yDimension { Vertex::Amp };
        std::vector<Vertex*>* vertices {};
    };
}
