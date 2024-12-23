#include <iterator>
#include <algorithm>
#include <utility>
#include "UndoableActions.h"
#include "Interactor.h"
#include "../App/MeshLibrary.h"
#include "../App/SingletonRepo.h"
#include "../Definitions.h"
#include "../Design/Updating/Updater.h"
#include "../UI/Widgets/IconButton.h"
#include "../Util/CommonEnums.h"

bool NamedUndoableAction::undo() {
    haveUndone = true;
    undoDelegate();
    undoExtra();

    return true;
}

bool NamedUndoableAction::perform() {
    if (!haveUndone) {
        return true;
    }

    performDelegate();
    performExtra();

    return true;
}

ResponsiveUndoableAction::ResponsiveUndoableAction(
        SingletonRepo* repo
    , 	int updateCode) :
            SingletonAccessor(repo, "ResponsiveUndoableAction" + String(updateCode))
        ,	updateCode(updateCode) {
}

void ResponsiveUndoableAction::undoExtra() {
    if (description.isNotEmpty()) {
        showMsg(String("Undoing ") + description);
    }

    triggerAsyncUpdate();
}

void ResponsiveUndoableAction::performExtra() {
    if (description.isNotEmpty()) {
        showMsg(description);
    }

    triggerAsyncUpdate();
}

void ResponsiveUndoableAction::handleAsyncUpdate() {
    doPreUpdateCheck();
    getObj(Updater).update(updateCode, Update);
    doPostUpdateCheck();
}

TransformVertexAction::TransformVertexAction(
        SingletonRepo* repo
    ,	int updateCode
    ,	Vertex* vertex
    ,	Vertex before
    ,	Vertex after) :
            ResponsiveUndoableAction(repo, updateCode)
        ,	vertex(vertex)
        ,	before(before)
        ,	after(after) {
    description = "Transform Vertex";
}

void TransformVertexAction::performDelegate() {
    *vertex = after;
}

void TransformVertexAction::undoDelegate() {
    *vertex = before;
}

TransformVerticesAction::TransformVerticesAction(
        SingletonRepo* repo
    , 	int updateCode
    ,	vector<Vertex*>* verts
    ,	const vector<Vertex>& _before
    ,	const vector<Vertex>& _after) :
            ResponsiveUndoableAction(repo, updateCode)
        ,	vertices(verts) {
    before = _before;
    after  = _after;

    jassert(before.size() == after.size());
    jassert(vertices->size() == before.size());

    description = "Transform Vertices";
}

void TransformVerticesAction::performDelegate() {
    if (vertices->size() != after.size()) {
        return;
    }

    for (int i = 0; i < vertices->size(); ++i) {
        *(*vertices)[i] = after[i];
    }
}

void TransformVerticesAction::undoDelegate() {
    if (vertices->size() != before.size()) {
        return;
    }

    for (int i = 0; i < vertices->size(); ++i) {
        *(*vertices)[i] = before[i];
    }
}

UpdateVertexVectorAction::UpdateVertexVectorAction(
        Interactor* itr
    ,	vector<Vertex*>* 		_elements
    ,	const vector<Vertex*>& _before
    ,	const vector<Vertex*>& _after
    ,	bool doUpdate) :
            ResponsiveUndoableAction(itr->getSingletonRepo(), itr->getUpdateSource())
        ,	itr(itr)
        ,	vertices(_elements)
        ,	before(_before)
        ,	after(_after) {
    updateCode = doUpdate ? itr->getUpdateSource() : CommonEnums::Null;

    int beforeSize 	 = _before.size();
    int afterSize 	 = _after.size();
    int difference 	 = afterSize - beforeSize;

    action = afterSize < beforeSize ? Deletion : Addition;

    int selectedSize = itr->getSelected().size();
    int numberDescribed = difference > 0 ? difference
            : difference < 0 ? -difference
            : selectedSize;

    String vertexStr = numberDescribed == 1 ? " vertex" : " vertices";
    String actionDesc = difference > 0 ? "Add "
            : difference < 0 ? "Remove "
            : "Transform ";

    description = actionDesc + String(numberDescribed) + vertexStr;
}

void UpdateVertexVectorAction::doPreUpdateCheck() {
    if (action == Deletion) {
        itr->clearSelectedAndRepaint();
    }

    itr->updateDspSync();
}

void UpdateVertexVectorAction::performDelegate() {
    ScopedLock sl(itr->getLock());

    *vertices = after;
}

void UpdateVertexVectorAction::undoDelegate() {
    ScopedLock sl(itr->getLock());

    *vertices = before;
}

UpdateCubeVectorAction::UpdateCubeVectorAction(
        Interactor* itr
    ,	vector<VertCube*>* _elements
    ,	const vector<VertCube*>& _before
    ,	const vector<VertCube*>& _after
    ,	bool						_shouldClearLines) :
            ResponsiveUndoableAction(itr->getSingletonRepo(), itr->getUpdateSource())
        ,	itr				(itr)
        ,	elements		(_elements)
        ,	before			(_before)
        ,	after			(_after)
        ,	shouldClearLines(_shouldClearLines) {
    description = String("Line ") + (before.size() < after.size() ? "addition" : "deletion");
}

void UpdateCubeVectorAction::performDelegate() {
    ScopedLock sl(itr->getLock());

    *elements = after;
}

void UpdateCubeVectorAction::doPreUpdateCheck() {
    if (shouldClearLines) {
        itr->clearSelectedAndRepaint();
    }

    itr->validateMesh();
}

void UpdateCubeVectorAction::undoDelegate() {
    ScopedLock sl(itr->getLock());

    *elements = before;
}

SliderValueChangedAction::SliderValueChangedAction(
        SingletonRepo* repo
    ,	int updateCode
    ,	Slider* slider
    ,	double startingValue) :
            ResponsiveUndoableAction(repo, updateCode)
        ,	slider(slider) {
    before = startingValue;
    after = slider->getValue();

    description = "Slider adjustment";
}

void SliderValueChangedAction::performDelegate() {
    slider->setValue(after);
}

void SliderValueChangedAction::undoDelegate() {
    slider->setValue(before);
}

DeformerAssignment::DeformerAssignment(
        Interactor* itr
    , 	Mesh* mesh
    ,	const vector<VertCube*>& selectedLines
    ,	vector<int> previousMappings
    ,	int thisMapping
    , 	int channel) :
            ResponsiveUndoableAction(itr->getSingletonRepo(), itr->getUpdateSource())
        , 	affectedLines	(selectedLines)
        ,	previousMappings(std::move(previousMappings))
        , 	currentMapping	(thisMapping)
        , 	channel			(channel)
        , 	mesh			(mesh) {
    description = "Line deformation";
}

void DeformerAssignment::performDelegate() {
    auto start 	= mesh->getCubes().begin();
    auto end 	= mesh->getCubes().end();

    for(auto line : affectedLines) {

        // check if line is still good
        if (line != nullptr && std::find(start, end, line) != end) {
            line->deformerAt(channel) = (char) currentMapping;

            // component curve assignment changes the sharpness / dfrm-gain
            if (channel == Vertex::Time) {
                bool doAdjustment = true;
                for (auto& lineVert : line->lineVerts) {
                    float& curve = lineVert->values[Vertex::Curve];

                    if (curve > 0.01f) {
                        doAdjustment = false;
                    }
                }

                if (doAdjustment) {
                    for (auto& lineVert : line->lineVerts) {
                        lineVert->setMaxSharpness();
                    }
                }
            }
        }
    }
}

void DeformerAssignment::undoDelegate() {
    jassert(affectedLines.size() == previousMappings.size());

    auto start 	= mesh->getCubes().begin();
    auto end 	= mesh->getCubes().end();

    for (int i = 0; i < affectedLines.size(); ++i) {
        VertCube* cube = affectedLines[i];

        if (cube != nullptr && std::find(start, end, cube) != end) {
            cube->deformerAt(channel) = previousMappings[i];
        }
    }
}

void DeformerAssignment::doPostUpdateCheck() {
    SingletonRepo* repo = itr->getSingletonRepo();
    getObj(MeshLibrary).layerChanged(LayerGroups::GroupDeformer, -1);
}

ComboboxChangeAction::ComboboxChangeAction(ComboBox* box, int previousId) :
        box(box)
    , 	previousId(previousId) {
    currentId = box->getSelectedId();

    description = "Combo box selection";
}

void ComboboxChangeAction::performDelegate() {
    box->setSelectedId(currentId, haveUndone ? sendNotificationSync :
                                               dontSendNotification);
}

void ComboboxChangeAction::undoDelegate() {
    box->setSelectedId(previousId, sendNotificationSync);
    haveUndone = true;
}

void LayerMoveAction::performDelegate() {
    getObj(MeshLibrary).moveLayer(layerType, fromIndex, toIndex);
}

void LayerMoveAction::undoDelegate() {
    getObj(MeshLibrary).moveLayer(layerType, toIndex, fromIndex);
}

VertexOwnershipAction::VertexOwnershipAction(
        VertCube* cube
    , 	bool undoRemoves
    , 	const Array<Vertex*>& toChange) :
            cube(cube)
        , 	undoRemoves(undoRemoves)
        , 	toChange(toChange) {
    description = "Vertex Ownership";
}

void VertexOwnershipAction::undoDelegate() {
    for(auto it : toChange) {
        undoRemoves ? it->removeOwner(cube) : it->addOwner(cube);
    }
}

void VertexOwnershipAction::performDelegate() {
    for(auto it : toChange) {
        undoRemoves ? it->addOwner(cube) : it->removeOwner(cube);
    }
}

VertexCubePropertyAction::VertexCubePropertyAction(
        VertCube* cube
    , 	const VertCube& oldProps
    , 	const VertCube& newProps) :
            cube(cube)
        , 	oldProps(oldProps)
        , 	newProps(newProps) {
    description = "Cube Property Change";
}

void VertexCubePropertyAction::undoDelegate() {
    cube->setPropertiesFrom(&oldProps);
}

void VertexCubePropertyAction::performDelegate() {
    cube->setPropertiesFrom(&newProps);
}

ButtonToggleAction::ButtonToggleAction(IconButton* button) : button(button) {}

void ButtonToggleAction::undoDelegate() {
    // TODO
    button->setHighlit(true);
}

void ButtonToggleAction::performDelegate() {
}
