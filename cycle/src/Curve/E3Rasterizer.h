#pragma once

#include <App/SingletonAccessor.h>
#include <App/SingletonRepo.h>
#include <Array/Column.h>
#include <Array/ScopedAlloc.h>
#include <Curve/MeshRasterizer.h>
#include <vector>

using std::vector;

class E3Rasterizer :
        public SingletonAccessor
    ,   public Updateable
    ,   public Rasterization::GuideCurveBindableRasterizer
    ,   public Rasterization::MeshBindableRasterizer
    ,   public Rasterization::RasterizerSampler
    ,   public Rasterization::RasterizerSnapshotProvider
    ,   public Rasterization::RasterizerUpdateTarget
    ,   public Rasterization::RasterizerVertexDomain {
public:
    explicit E3Rasterizer(SingletonRepo* repo);
    ~E3Rasterizer() override = default;
    int getIncrement();
    void init() override;
    void performUpdate(UpdateType updateType) override;
    void cleanUp() override { rasterizer.cleanUp(); }
    void reset() override { rasterizer.reset(); }
    void updateRasterizer(UpdateType updateType) override { update(updateType); }

    bool hasEnoughCubesForCrossSection() override { return rasterizer.hasEnoughCubesForCrossSection(); }
    bool isSampleable() override { return rasterizer.isSampleable(); }
    bool isSampleableAt(float x) override { return rasterizer.isSampleableAt(x); }
    bool wrapsVertices() const override { return rasterizer.wrapsVertices(); }

    float sampleAt(double angle) override { return rasterizer.sampleAt(angle); }
    float sampleAt(double angle, int& currentIndex) override { return rasterizer.sampleAt(angle, currentIndex); }
    float samplePerfectly(double delta, Buffer<float> buffer, double phase) override {
        return rasterizer.samplePerfectly(delta, buffer, phase);
    }

    Mesh* getMesh() override { return rasterizer.getMesh(); }
    void setMesh(Mesh* mesh) override { rasterizer.setMesh(mesh); }
    int getPaddingSize() const override { return rasterizer.getPaddingSize(); }
    GuideCurveProvider* getGuideCurveProvider() const override { return rasterizer.getGuideCurveProvider(); }
    RasterizerData& getRasterizerData() override { return rasterizer.getRasterizerData(); }
    const RasterizerData& getRasterizerData() const override { return rasterizer.getRasterizerData(); }

    vector<Column>& getColumns() { return columns; }
    Buffer<float> getArray() { return columnArray; }

    CriticalSection& getArrayLock() { return arrayLock; }
    MorphPosition& getMorphPosition() { return rasterizer.getMorphPosition(); }
    void setDims(const Dimensions& dims) override { rasterizer.setDims(dims); }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { rasterizer.setGuideCurveProvider(provider); }
    void setMorphPosition(const MorphPosition& morph) { rasterizer.setMorphPosition(morph); }

    enum { E3LockId = 0x17b1eed5 };
private:
    MeshRasterizer rasterizer;
    vector<Column> columns;
    ScopedAlloc<Float32> columnArray;

    CriticalSection arrayLock;
};
