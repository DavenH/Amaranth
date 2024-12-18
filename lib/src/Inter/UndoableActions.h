#pragma once

#include <vector>
#include "../App/SingletonAccessor.h"
#include "../Curve/Mesh.h"
#include "JuceHeader.h"
#include "../Curve/Vertex.h"
#include "../Curve/VertCube.h"

class Interactor;
class Mesh;

using std::vector;

class NamedUndoableAction : public UndoableAction {
public:
    NamedUndoableAction() : haveUndone(false) {}

    bool undo() override;
    bool perform() override;

    virtual void performDelegate() 	= 0;
    virtual void undoDelegate() 	= 0;

    virtual void undoExtra() 	{}
    virtual void performExtra() {}

    const String& getDescription() { return description; }

protected:
    bool haveUndone;
    String description;
};

/* ----------------------------------------------------------------------------- */

class ResponsiveUndoableAction :
        public NamedUndoableAction
    ,	public AsyncUpdater
    ,	public SingletonAccessor {
public:
    ResponsiveUndoableAction(SingletonRepo* repo, int updateCode);

    void handleAsyncUpdate() override;

    virtual void doPreUpdateCheck()  {}
    virtual void doPostUpdateCheck() {}

    void performDelegate() 	override = 0;
    void undoDelegate() 	override = 0;

    void undoExtra() override;
    void performExtra() override;
protected:

    int updateCode;
};

/* ----------------------------------------------------------------------------- */

class TransformVertexAction : public ResponsiveUndoableAction {
public:
    TransformVertexAction(
            SingletonRepo* repo
        ,	int updateCode
        ,	Vertex* vertex
        ,	Vertex before
        ,	Vertex after);

    void performDelegate() override;
    void undoDelegate() override;

private:
    Vertex* vertex;
    Vertex before, after;
};

/* ----------------------------------------------------------------------------- */

class TransformVerticesAction : public ResponsiveUndoableAction {
public:
    TransformVerticesAction(
            SingletonRepo* repo
        ,	int updateCode
        ,	vector<Vertex*>* vertices
        ,	const vector<Vertex>& original
        ,	const vector<Vertex>& future);

    void performDelegate() override;
    void undoDelegate() override;

private:
    vector<Vertex*>* vertices;
    vector<Vertex> before;
    vector<Vertex> after;
};

/* ----------------------------------------------------------------------------- */

class UpdateVertexVectorAction : public ResponsiveUndoableAction {
public:
    enum VertexAction { Deletion, Addition };

    UpdateVertexVectorAction(
            Interactor* interactor
        ,	vector<Vertex*>* vertices
        ,	const vector<Vertex*>& before
        ,	const vector<Vertex*>& after
        ,	bool doUpdate);

    void doPreUpdateCheck() override;
    void performDelegate() override;
    void undoDelegate() override;

private:
    vector<Vertex*>* vertices;
    vector<Vertex*> before;
    vector<Vertex*> after;
    Interactor* itr;

    VertexAction action;
};

/* ----------------------------------------------------------------------------- */

class UpdateCubeVectorAction : public ResponsiveUndoableAction {
public:
    UpdateCubeVectorAction(
            Interactor* interactor
        , 	vector<VertCube*>* elements
        ,	const vector<VertCube*>& before
        , 	const vector<VertCube*>& after
        ,	bool shouldClearLines = true);

    void doPreUpdateCheck() override;
    void performDelegate() override;
    void undoDelegate() override;

private:
    bool shouldClearLines;

    Interactor* itr;
    vector<VertCube*>* elements;
    vector<VertCube*> before;
    vector<VertCube*> after;
};

/* ----------------------------------------------------------------------------- */

class SliderValueChangedAction : public ResponsiveUndoableAction {
public:
    SliderValueChangedAction(
            SingletonRepo* repo
        ,	int updateCode
        , 	Slider* slider
        , 	double startingValue);

    void performDelegate() override;
    void undoDelegate() override;

private:
    double before, after;
    Slider* slider;
};

/* ----------------------------------------------------------------------------- */

class DeformerAssignment : public ResponsiveUndoableAction {
public:
    DeformerAssignment(
            Interactor* itr
        ,	Mesh* mesh
        ,	const vector<VertCube*>& lines
        ,	vector<int> previousMappings
        , 	int thisMapping, int channel);

    void doPreUpdateCheck() override;
    void doPostUpdateCheck() override;
    void performDelegate() override;
    void undoDelegate() override;

private:
    int currentMapping, channel;
    Interactor* itr;
    Mesh* mesh;
    vector<VertCube*> affectedLines;
    vector<int> previousMappings;
};

/* ----------------------------------------------------------------------------- */

class ComboboxChangeAction :
        public NamedUndoableAction {
public:
    ComboboxChangeAction(ComboBox* box, int previousId);
    void performDelegate() override;
    void undoDelegate() override;

private:
    int previousId, currentId;
    ComboBox* box;
};

/* ----------------------------------------------------------------------------- */

class LayerMoveAction :
        public SingletonAccessor
    ,	public NamedUndoableAction {
public:
    LayerMoveAction(SingletonRepo* repo, int layerType, int fromIndex, int toIndex)
            : SingletonAccessor(repo, "LayerMoveAction"),
            layerType(layerType)
        , 	fromIndex(fromIndex)
        , 	toIndex(toIndex) {
        description = "Layer Move";
    }

    void performDelegate() override;
    void undoDelegate() override;

private:
    int layerType, toIndex, fromIndex;
};

/* ----------------------------------------------------------------------------- */

class VertexOwnershipAction :
        public NamedUndoableAction {
public:

    VertexOwnershipAction(VertCube* cube, bool undoRemoves, const Array<Vertex*>& toChange);
    void performDelegate() override;
    void undoDelegate() override;

    bool undoRemoves;
    VertCube* cube;
    Array<Vertex*> toChange;
};

/* ----------------------------------------------------------------------------- */

class VertexCubePropertyAction :
        public NamedUndoableAction {
public:
    VertexCubePropertyAction(VertCube* cube, const VertCube& oldProps, const VertCube& newProps);
    void performDelegate() override;
    void undoDelegate() override;

private:
    VertCube* cube;
    VertCube oldProps, newProps;
};


class IconButton;

class ButtonToggleAction : public NamedUndoableAction {
    explicit ButtonToggleAction(IconButton* button);

    void performDelegate() override;
    void undoDelegate() override;
    IconButton* button;
};
