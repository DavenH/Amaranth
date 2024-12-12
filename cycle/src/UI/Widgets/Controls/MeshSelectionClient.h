#pragma once

class Mesh;

template<class MeshType>
class MeshSelectionClient {
public:
	virtual ~MeshSelectionClient() = default;

	virtual void setCurrentMesh(MeshType* mesh) 	= 0;
	virtual void previewMesh(MeshType* mesh) 		= 0;
	virtual void previewMeshEnded(MeshType* mesh) 	= 0;
	virtual MeshType* getCurrentMesh() 				= 0;
	virtual int getLayerType()	= 0;
	virtual void enterClientLock() 					= 0;
	virtual void exitClientLock() 					= 0;

	virtual String getDefaultFolder() 				{ return {}; }
	virtual void prepareForPopup() 					{}
	virtual void doubleMesh()						{}
};