#pragma once

#include <App/SingletonAccessor.h>
#include <App/SingletonRepo.h>
#include <Array/Column.h>
#include <Array/ScopedAlloc.h>
#include <Curve/Rasterization/MeshWaveformRasterizer.h>
#include <Curve/Rasterization/Builders/RasterizerSnapshotBuilder.h>
#include <Curve/Rasterization/Interfaces/GuideCurveBindableRasterizer.h>
#include <Curve/Rasterization/Interfaces/MeshBindableRasterizer.h>
#include <Curve/Rasterization/Interfaces/RasterizerSampler.h>
#include <Curve/Rasterization/Interfaces/RasterizerSnapshotProvider.h>
#include <Curve/Rasterization/Interfaces/RasterizerUpdateTarget.h>
#include <Curve/Rasterization/Interfaces/RasterizerVertexDomain.h>
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
    void cleanUp() override;
    void reset() override { cleanUp(); }
    void updateRasterizer(UpdateType updateType) override { update(updateType); }

    bool hasEnoughCubesForCrossSection() override;
    bool isSampleable() override;
    bool isSampleableAt(float x) override;
    bool wrapsVertices() const override { return rasterizer.getRequest().cyclic; }

    float sampleAt(double angle) override;
    float sampleAt(double angle, int& currentIndex) override;
    float samplePerfectly(double delta, Buffer<float> buffer, double phase) override;

    Mesh* getMesh() override { return mesh; }
    void setMesh(Mesh* mesh) override { this->mesh = mesh; }
    int getPaddingSize() const override;
    GuideCurveProvider* getGuideCurveProvider() const override { return rasterizer.getGuideCurveProvider(); }
    RasterizerData& getRasterizerData() override { return rasterizerData; }
    const RasterizerData& getRasterizerData() const override { return rasterizerData; }

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
