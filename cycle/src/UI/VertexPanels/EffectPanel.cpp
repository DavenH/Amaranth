#include "EffectPanel.h"

#include <Definitions.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <App/Doc/Document.h>
#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <Curve/FXRasterizer.h>
#include <Util/Arithmetic.h>
#include <Thread/LockTracer.h>

#include "../../Audio/Effects/IrModeller.h"

EffectPanel::EffectPanel(SingletonRepo* repo, const String& name, bool haveVertZoom) :
		Panel2D			 (repo, name, true, haveVertZoom)
	,	Interactor2D	 (repo, name, Dimensions(Vertex::Phase, Vertex::Amp))
	,	SingletonAccessor(repo, name)
	,	localRasterizer	 (repo, name + "Rasterizer")
{
	vertPadding 	= 0;
	paddingLeft 	= 0;
	paddingRight 	= 0;

	backgroundTimeRelevant 	= false;
	speedApplicable			= false;
	deformApplicable		= false;

	colorA = Color(0.8f, 0.8, 0.9f);
	colorB = Color(0.8f, 0.8, 0.9f);

	rasterizer = &localRasterizer;
	interactor = this;

	vertexProps.sliderApplicable[Vertex::Time] 	= false;
	vertexProps.sliderApplicable[Vertex::Red] 	= false;
	vertexProps.sliderApplicable[Vertex::Blue] 	= false;
	vertexProps.ampVsPhaseApplicable 			= false;

	for(auto& flag : vertexProps.deformApplicable) {
		flag = false;
	}

	vertexProps.dimensionNames.set(Vertex::Time, 	{});
	vertexProps.dimensionNames.set(Vertex::Red, 	{});
	vertexProps.dimensionNames.set(Vertex::Blue, 	{});
	vertexProps.dimensionNames.set(Vertex::Phase,	"x");
	vertexProps.dimensionNames.set(Vertex::Amp, 	"y");
}

bool EffectPanel::doCreateVertex() {
	bool succeeded = addNewCube(0, state.currentMouse.x, state.currentMouse.y);

	vector<Vertex*>& selected = getSelected();

	if (state.currentVertex != nullptr) {
		selected.clear();
		selected.push_back(state.currentVertex);
		updateSelectionFrames();
	}

	flag(DidMeshChange) = succeeded;

	return succeeded;
}

float EffectPanel::getDragMovementScale() {
	return 1.f;
}

void EffectPanel::performUpdate(int updateType) {
	repaint();
}

bool EffectPanel::addNewCube(float startTime, float x, float y, float curve) {
	ScopedLock sl(vertexLock);

	vector<Vertex*>& verts = getMesh()->getVerts();
	vector<Vertex*> beforeVerts;

	if(! suspendUndo) {
		beforeVerts = verts;
	}

	Vertex* vertex = new Vertex(x, y);
	vertex->values[Vertex::Curve] = curve;
	state.currentVertex = vertex;

	verts.push_back(vertex);

	if(!suspendUndo) {
		vector<Vertex*>& afterVerts = verts;
		getObj(EditWatcher).addAction(new UpdateVertexVectorAction(this, &verts, beforeVerts, afterVerts, true));
	}

	return true;
}

bool EffectPanel::isMeshEnabled() const
{
	return isEffectEnabled();
}

