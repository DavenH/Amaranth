#pragma once

#include <vector>

#include "../../Intercept.h"
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

        Intercept interceptAt(int index) const {
            Vertex* vertex = vertexAt(index);
            float* values = vertex->values;

            Intercept intercept(
                    values[xDimension],
                    values[yDimension],
                    nullptr,
                    values[Vertex::Curve]);
            intercept.adjustedX = intercept.x;

            return intercept;
        }

        void copyInterceptsTo(std::vector<Intercept>& intercepts) const {
            intercepts.clear();

            if (vertices == nullptr) {
                return;
            }

            intercepts.reserve(vertices->size());

            for (int i = 0; i < (int) vertices->size(); ++i) {
                if ((*vertices)[i] != nullptr) {
                    intercepts.emplace_back(interceptAt(i));
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
