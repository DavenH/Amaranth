#pragma once

#include <App/SingletonAccessor.h>
#include <App/SingletonRepo.h>
#include <Array/Column.h>
#include <Array/ScopedAlloc.h>
#include <Curve/MeshRasterizer.h>
#include <vector>

using std::vector;

class E3Rasterizer	:
	public SingletonAccessor, public MeshRasterizer
{
public:
	explicit E3Rasterizer(SingletonRepo* repo);
	~E3Rasterizer() override = default;
	int getIncrement();
	void init() override;
	void performUpdate(int updateType) override;
	float& getPrimaryDimensionVar() override;

	vector<Column>& getColumns() { return columns; }
	Buffer<float> getArray() { return columnArray; }

	CriticalSection& getArrayLock() { return arrayLock; }

	enum { E3LockId = 0x17b1eed5 };
private:
	vector<Column> columns;
	ScopedAlloc<Ipp32f> columnArray;

	CriticalSection arrayLock;
};