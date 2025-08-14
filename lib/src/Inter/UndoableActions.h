#pragma once

#include <vector>
#include "../App/SingletonAccessor.h"
#include "../Wireframe/Mesh.h"
#include "JuceHeader.h"
#include "../Wireframe/Interpolator/Trilinear/TrilinearVertex.h"
#include "../Wireframe/Interpolator/Trilinear/TrilinearCube.h"

class Interactor;
class Mesh;

using std::vector;

class NamedUndoableAction : public UndoableAction {
public:
    NamedUndoableAction() : haveUndone(false) {}
    ~NamedUndoableAction() override = default;
    bool undo() override;
    bool perform() override;

    virtual void performDelegate()  = 0;
    virtual void undoDelegate()     = 0;

    virtual void undoExtra()    {}
    virtual void performExtra() {}

    const String& getDescription() { return description; }

protected:
    bool haveUndone;
    String description;
};

/* ----------------------------------------------------------------------------- */

class ResponsiveUndoableAction :
        public NamedUndoableAction
    ,   public AsyncUpdater
    ,   public SingletonAccessor {
public:
    ResponsiveUndoableAction(SingletonRepo* repo, int updateCode);

    void handleAsyncUpdate() override;

    virtual void doPreUpdateCheck()  {}
    virtual void doPostUpdateCheck() {}

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
        ,   int updateCode
        ,   Vertex* vertex
        ,   Vertex before
        ,   Vertex after);

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
        ,   int updateCode
        ,   vector<Vertex*>* vertices
        ,   const vector<Vertex>& original
        ,   const vector<Vertex>& future);

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
        ,   vector<Vertex*>* vertices
        ,   const vector<Vertex*>& before
        ,   const vector<Vertex*>& after
        ,   bool doUpdate);

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
        ,   vector<TrilinearCube*>* elements
        ,   const vector<TrilinearCube*>& before
        ,   const vector<TrilinearCube*>& after
        ,   bool shouldClearLines = true);

    void doPreUpdateCheck() override;
    void performDelegate() override;
    void undoDelegate() override;

private:
    bool shouldClearLines;

    Interactor* itr;
    vector<TrilinearCube*>* elements;
    vector<TrilinearCube*> before;
    vector<TrilinearCube*> after;
};

/* ----------------------------------------------------------------------------- */

class SliderValueChangedAction : public ResponsiveUndoableAction {
public:
    SliderValueChangedAction(
            SingletonRepo* repo
        ,   int updateCode
        ,   Slider* slider
        ,   double startingValue);

    void performDelegate() override;
    void undoDelegate() override;

private:
    double before, after;
    Slider* slider;
};

/* ----------------------------------------------------------------------------- */

class PathAssignment : public ResponsiveUndoableAction {
public:
    PathAssignment(
            SingletonRepo* repo
        ,   int updateSource
        ,   Mesh* mesh
        ,   const vector<TrilinearCube*>& lines
        ,   vector<int> previousMappings
        ,   int thisMapping, int channel);

    void doPostUpdateCheck() override;
    void performDelegate() override;
    void undoDelegate() override;

private:
    int currentMapping, channel;
    // Interactor* itr;
    Mesh* mesh;
    vector<TrilinearCube*> affectedLines;
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
    ,   public NamedUndoableAction {
public:
    LayerMoveAction(SingletonRepo* repo, int layerType, int fromIndex, int toIndex)
            : SingletonAccessor(repo, "LayerMoveAction"),
            layerType(layerType)
        ,   fromIndex(fromIndex)
        ,   toIndex(toIndex) {
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

    VertexOwnershipAction(TrilinearCube* cube, bool undoRemoves, const Array<Vertex*>& toChange);
    void performDelegate() override;
    void undoDelegate() override;

    bool undoRemoves;
    TrilinearCube* cube;
    Array<Vertex*> toChange;
};

/* ----------------------------------------------------------------------------- */

class VertexCubePropertyAction :
        public NamedUndoableAction {
public:
    VertexCubePropertyAction(TrilinearCube* cube, const TrilinearCube& oldProps, const TrilinearCube& newProps);
    void performDelegate() override;
    void undoDelegate() override;

private:
    TrilinearCube* cube;
    TrilinearCube oldProps, newProps;
};


class IconButton;

class ButtonToggleAction : public NamedUndoableAction {
    explicit ButtonToggleAction(IconButton* button);

    void performDelegate() override;
    void undoDelegate() override;
    IconButton* button;
};
