#pragma once

#include <App/SingletonAccessor.h>
#include <Curve/Vertex.h>
#include <Curve/Vertex2.h>
#include <Obj/MorphPosition.h>

class VertCube;

class CubeDisplay :
        public Component
    ,	public AsyncUpdater
    , 	public SingletonAccessor {
public:
    explicit CubeDisplay(SingletonRepo* repo);
    void paint(Graphics& g) override;
    void update(VertCube* cube, int selectedIdx, int scratchChannel, bool isEnvelope);
    void convolve(int i, Vertex2& vert, float zFactor);
    void resized() override;
    void handleAsyncUpdate() override;
    void linkingChanged();

private:
    void refreshCube();

    MorphPosition pos;
    int lastScratchChannel;
    int selectedIdx;
    bool isEnvelope;

    bool		selected	[8];
    float 		corners		[8][3];
    float 		mat			[4][2];
    int 		faces		[6][4];
    Vertex2 	unit		[8];
    Vertex2 	scaledUnit	[8];
    Vertex2 	scaledCube	[8];
    Vertex2 	scaledExp	[8];
    Vertex2 	scaledExpB	[8];
    Vertex2 	expUnit		[8];
    Vertex2 	expUnitB	[8];
    Vertex2 	cubeVerts	[8];

    Image backImage;
};
