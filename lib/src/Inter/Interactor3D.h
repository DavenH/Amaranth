#pragma once
#include <vector>
#include "Interactor.h"
#include "../Array/Column.h"
#include "../Curve/SimpleIcpt.h"
#include "../Design/Updating/Updater.h"
#include "JuceHeader.h"

using std::vector;

typedef vector<SimpleIcpt>::iterator SimpleIter;
typedef vector<SimpleIcpt>::const_iterator ConstSimpleIter;

class Interactor3D:
    public Interactor {
public:
    enum MergeActionType {
            MergeAtFirst
        ,	MergeAtSecond
        ,	MergeAtCentre
    };

    Interactor3D(
            SingletonRepo* repo,
            const String& name,
            const Dimensions& d = Dimensions(Vertex::Time, Vertex::Phase, Vertex::Red, Vertex::Blue));

    void commitPath(const MouseEvent& e) override;
    void copyVertices();
    void doAxe(const MouseEvent& e);
    void doDragPaintTool(const MouseEvent& e) override;
    void doExtraMouseDrag(const MouseEvent & e) override;
    void doFurtherReset() override;
    void extrudeVertices();
    void mergeSelectedVerts();
    void mergeVertices(Vertex* a, Vertex* b, MergeActionType mergeAction);
    void primaryDimensionChanged();
    void shallowUpdate();
    void glueCubes();
    void glueCubes(VertCube* cubeOne, VertCube* cubeTwo);
    void sliceLines();
    void sliceLines(const Vertex2& start, const Vertex2& end);
    void performUpdate(UpdateType updateType) override;
    void updateDepthVerts() override {}
    void updateIntercepts();
    void updateInterceptsWithMesh(Mesh* mesh);

    bool connectSelected();
    bool connectVertices(Vertex* a, Vertex* b);
    bool doCreateVertex() override;
    bool locateClosestElement() override;

    int getCapabilities();
    VertCube* getLineContaining(Vertex* a, Vertex* b);

    virtual int getTableIndexY(float y, int size);
    virtual String getYString(float yVal, int yIndex, const Column& col, float fundFreq);
    virtual String getZString(float tableValue, int xIndex);
    virtual void doCommitPencilEditPath();
    void doExtraMouseDown(const MouseEvent& e) override;
    void doExtraMouseUp() override;
    virtual void updateRastDims();

    template<class T>
    void removeFromVector(vector<T*>& vect, T* t) {
        typename vector<T*>::iterator toErase = std::find(vect.begin(), vect.end(), t);

        if (toErase != vect.end())
            vect.erase(toErase);
    }

    bool is3DInteractor() override { return true; }
    const vector<SimpleIcpt>& getInterceptPairs() { return interceptPairs; }

protected:
    void removeDuplicateVerts(vector<Vertex*>& affectedVerts, vector<VertCube*>& affectedCubes, bool deleteDupes);
    void moveVertsAndTest(const Array<Vertex*>& arr, float diff);
    void mergeVerticesOneOwner(Vertex* firstSelected, Vertex* secondSelected, MergeActionType mergeAction);
    void mergeVerticesBothOneOwner(Vertex* firstSelected, Vertex* secondSelected, MergeActionType mergeAction);
    void mergeVerticesOneAndManyOwners(Vertex* firstSelected, Vertex* secondSelected, MergeActionType mergeAction);
    void mergeVerticesManyOwners(Vertex* firstSelected, Vertex* secondSelected, MergeActionType mergeAction);
    float getAverageDistanceFromCentre();

    bool canDoMiddleScroll;
    vector<SimpleIcpt> interceptPairs;
};

