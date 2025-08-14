#pragma once

#include "../Array/ScopedAlloc.h"
#include "../Wireframe/Rasterizer/Rasterizer2D.h"
#include "../Util/Geometry.h"

class Interactor2D;

class AutoModeller {
public:
    AutoModeller();

    void modelToInteractor(
        const Buffer<float> &buffer,
        Interactor2D *interactor,
        bool cyclic,
        float leftSamplingOffset,
        float reduction
    );

    vector<Intercept> modelToPath(
            vector<Vertex2>& path,
            float reductionLevel,
            bool useInflections
    );

    template<class T>
    static vector<T> reducePath(const vector<T>& path, float tolerance) {
        vector<T> decimated;

        if(path.size() < 2)
            return decimated;

        float diff       = path[1].x - path[0].x;
        float areaThresh = tolerance * diff;

        decimated.push_back(path[0]);

        for (int i = 1; i < (int) path.size() - 1;) {
            int end = i;
            int middle = i;
            bool areaAboveThresh = false;

            while (end < path.size() - 1) {
                middle = (end + i) / 2;
                float area = Geometry::getTriangleArea(path[i - 1], path[middle], path[end + 1]);

                if (area >= areaThresh) {
                    areaAboveThresh = true;
                    break;
                }

                ++end;
            }

            if(areaAboveThresh) {
                decimated.push_back(path[middle]);
            }

            if (middle > i) {
                i = middle;
            } else {
                ++i;
            }
        }

        if(! (decimated.back() == path.back())) {
            decimated.push_back(path.back());
        }

        return decimated;
    }

private:
    bool useInflections;
    float leftSamplingOffset, rightSamplingOffset, reductionLevel;

    Random random;
    Rasterizer2D rasterizer;

    ScopedAlloc<Float32> rastMem;
    ScopedAlloc<Float32> srcSamples;

    vector<Intercept> points;

    void amplifyCloseVerts();
    void fit();
    void preparePoints();
    void removeUselessPoints();

    float performFitness(int curveIdx, const Intercept& icpt, int sampleStart,
                         int sampleSize, float invLength, float left, bool doUpdate = true);

    JUCE_DECLARE_NON_COPYABLE(AutoModeller);
};