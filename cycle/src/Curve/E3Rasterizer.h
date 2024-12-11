#pragma once

#include <App/SingletonAccessor.h>
#include <App/SingletonRepo.h>
#include <Array/Column.h>
#include <Array/ScopedAlloc.h>
#include <Curve/MeshRasterizer.h>
#include <Obj/Ref.h>
#include <vector>

using std::vector;

class E3Rasterizer	:
		public MeshRasterizer
{
public:
	E3Rasterizer(SingletonRepo* repo);
	virtual ~E3Rasterizer();
	int getIncrement();
	void init();
	void performUpdate(int updateType);
	float& getPrimaryDimensionVar();

	vector<Column>& getColumns() { return columns; }
	Buffer<float> getArray() { return columnArray; }

	CriticalSection& getArrayLock() { return arrayLock; }

	enum { E3LockId = 0x17b1eed5 };
private:
	vector<Column> columns;
	ScopedAlloc<Ipp32f> columnArray;

	CriticalSection arrayLock;
};