#pragma once

#include <App/SingletonAccessor.h>
#include <App/SingletonRepo.h>
#include <Array/Column.h>
#include <Array/ScopedAlloc.h>
#include <Curve/Rasterization/Builders/RasterizerSnapshotBuilder.h>
#include <Curve/Rasterization/Rasterizer.h>
#include <Curve/Rasterization/MeshWaveformRasterizer.h>
#include <vector>

using std::vector;

class E3Rasterizer :
        public SingletonAccessor
    ,   public Updateable
    ,   public Rasterization::Rasterizer {
public:
    explicit E3Rasterizer(SingletonRepo* repo);
    ~E3Rasterizer() override = default;
    int getIncrement();
    void init() override;
    void performUpdate(UpdateType updateType) override;
    void cleanUp();
    void reset() override { cleanUp(); }

    bool canRasterizeWaveform();
    bool wrapsVertices() const override { return rasterizer.getRequest().cyclic; }

    Rasterization::SamplerView samplerView() const override { return rasterizer.samplerView(); }
    Rasterization::SnapshotView snapshotView() override { return Rasterization::SnapshotView(rasterizerData); }

    void setMesh(Mesh* mesh) override { this->mesh = mesh; }
    int getPaddingSize() const override;
    GuideCurveProvider* getGuideCurveProvider() const override { return rasterizer.getGuideCurveProvider(); }

    vector<Column>& getColumns() { return columns; }
    Buffer<float> getArray() { return columnArray; }

    CriticalSection& getArrayLock() { return arrayLock; }
    MorphPosition& getMorphPosition() { return rasterizer.getRequest().morph; }
    void setDims(const Dimensions& dims) override { rasterizer.getRequest().dims = dims; }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { rasterizer.setGuideCurveProvider(provider); }
    void setMorphPosition(const MorphPosition& morph) { rasterizer.getRequest().morph = morph; }

    enum { E3LockId = 0x17b1eed5 };
private:
    void renderMesh(Mesh* mesh);
    void publishSnapshot();

    Mesh* mesh {};
    ::Rasterization::MeshWaveformRasterizer rasterizer;
    RasterizerData rasterizerData;

    vector<Column> columns;
    ScopedAlloc<Float32> columnArray;

    CriticalSection arrayLock;
};
