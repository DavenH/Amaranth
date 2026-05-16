#pragma once

#include <App/SingletonAccessor.h>
#include <App/SingletonRepo.h>
#include <Array/Column.h>
#include <Array/ScopedAlloc.h>
#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>
#include <vector>

using std::vector;

class E3Rasterizer :
        public SingletonAccessor
    ,   public Rasterization::TrilinearMeshRasterizer {
public:
    explicit E3Rasterizer(SingletonRepo* repo);
    ~E3Rasterizer() override = default;
    int getIncrement();
    void init() override;
    void performUpdate(UpdateType updateType) override;
    void cleanUp();
    void reset() override { cleanUp(); }

    vector<Column>& getColumns() { return columns; }
    Buffer<float> getArray() { return columnArray; }

    CriticalSection& getArrayLock() { return arrayLock; }

    enum { E3LockId = 0x17b1eed5 };
private:
    void renderMesh(Mesh* mesh);

    vector<Column> columns;
    ScopedAlloc<Float32> columnArray;

    CriticalSection arrayLock;
};
