#pragma once
#include "Interactor.h"
#include "../Obj/MorphPosition.h"

class Panel2D;

class Interactor2D:
	public Interactor {
public:
	Interactor2D(SingletonRepo* repo, const String& name, const Dimensions& d);

	void doExtraMouseMove(const MouseEvent& e) override;
	void removeLinesInRange(Range<float> phsRange, const MorphPosition& pos);
	void setSuspendUndo(bool suspend) { suspendUndo = suspend; }
	float getVertexClickProximityThres() override;

	bool doCreateVertex() override;
	bool locateClosestElement() override;
	Range<float> getVertexPhaseLimits(Vertex* vert) override;

	void commitPath(const MouseEvent& e) override;
	void doExtraMouseDrag(const MouseEvent& e) override;
	void doReshapeCurve(const MouseEvent& e) override;
	void mouseDoubleClick (const MouseEvent& e) override;
	virtual void setExtraElements(float x);

protected:
};