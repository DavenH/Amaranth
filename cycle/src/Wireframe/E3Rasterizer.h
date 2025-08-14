#pragma once

#include <App/SingletonAccessor.h>
#include <App/SingletonRepo.h>
#include <Array/Column.h>
#include <Array/ScopedAlloc.h>
#include <Wireframe/MeshRasterizer.h>
#include <vector>

#include "Wireframe/OldMeshRasterizer.h"

using std::vector;

class E3Rasterizer	:
    public SingletonAccessor, public OldMeshRasterizer
{
public:
    explicit E3Rasterizer(SingletonRepo* repo);
    ~E3Rasterizer() override = default;
    int getIncrement();
    void init() override;
    void performUpdate(UpdateType updateType) override;
    int getPrimaryViewDimension() override;

    vector<Column>& getColumns() { return columns; }
    Buffer<float> getArray() { return columnArray; }

    CriticalSection& getArrayLock() { return arrayLock; }

    enum { E3LockId = 0x17b1eed5 };
private:
    vector<Column> columns;
    ScopedAlloc<Float32> columnArray;

    CriticalSection arrayLock;
};