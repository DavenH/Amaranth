#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Curve/VertCube.h>

#include "CubeDisplay.h"
#include "MorphPanel.h"
#include "Util/CommonEnums.h"
#include "../../VisualDsp.h"

CubeDisplay::CubeDisplay(SingletonRepo* repo) :
		SingletonAccessor(repo, "CubeDisplay")
	,	selectedIdx(-1)
	,	isEnvelope(false)
	,	lastScratchChannel(CommonEnums::Null) {
    float scale = 0.75f;

	mat[0][0] = scale * 0.7;
	mat[0][1] = scale * 0.4;
	mat[1][0] = scale * 0.0;
	mat[1][1] = scale * 0.9;
	mat[2][0] = scale * 0.9;
	mat[2][1] = scale * -0.3;
	mat[3][0] = scale * 0.25;	// offset x
	mat[3][1] = scale * 0.4;	// offset y

	float c[8][3] = {
		{ 0, 0, 0 },
		{ 1, 0, 0 },
		{ 0, 1, 0 },
		{ 1, 1, 0 },
		{ 0, 0, 1 },
		{ 1, 0, 1 },
		{ 0, 1, 1 },
		{ 1, 1, 1 }
	};

	for(int i = 0; i < 8; ++i)
		for(int j = 0; j < 3; ++j)
			corners[i][j] = c[i][j];

	int f[6][4] = {
		{ 0, 2, 3, 1 },	// front	// key/mod
		{ 4, 5, 7, 6 },	// back
		{ 0, 4, 5, 1 }, // top		// time/key
		{ 2, 6, 7, 3 }, // bttm
		{ 0, 2, 6, 4 }, // left		// mod/time
		{ 3, 7, 6, 2 }, // right
	};

	for(int i = 0; i < 6; ++i)
		for(int j = 0; j < 4; ++j)
			faces[i][j] = f[i][j];

    for (int i = 0; i < VertCube::numVerts; ++i) {
        Vertex2& vert1 = unit[i];
		Vertex2& vert2 = expUnit[i];
		Vertex2& vert3 = expUnitB[i];

		convolve(i, vert1, 0);
		convolve(i, vert2, 0.1f);
		convolve(i, vert3, 0.2f);
	}
}


void CubeDisplay::convolve(int i, Vertex2& vert, float z) {
    float halfZ = z * 0.5f;
	float oneZ	= 1 + z;

	vert.x = 0;
	vert.y = 0;

	vert.x += mat[0][0] * (oneZ * corners[i][0] - halfZ);
	vert.x += mat[1][0] * (oneZ * corners[i][1] - halfZ);
	vert.x += mat[2][0] * (oneZ * corners[i][2] - halfZ);
	vert.x += mat[3][0];

	vert.y += mat[0][1] * (oneZ * corners[i][0] - halfZ);
	vert.y += mat[1][1] * (oneZ * corners[i][1] - halfZ);
	vert.y += mat[2][1] * (oneZ * corners[i][2] - halfZ);
	vert.y += mat[3][1];
}


void CubeDisplay::paint(Graphics& g) {
    g.drawImageAt(backImage, 0, 0);

    Colour clrs[] = {
			Colour(0.65f, 0.65f, 0.64f, 1.f),	// blue
			Colour(0.95f, 0.65f, 0.4f,	1.f),	// red
			Colour(0.11f, 0.65f, 0.45f, 1.f),	// yellow
	};

	int axes[3][2] = {
		{ 0, 2 },
		{ 2, 3 },
		{ 3, 7 }
	};

    Vertex2 modPointA = scaledExp[0] * pos.blue + scaledExp[2] * (1 - pos.blue);
    Vertex2 keyPointA = scaledExp[3] * pos.red + scaledExp[2] * (1 - pos.red);
    Vertex2 timePointA = scaledExp[7] * pos.time + scaledExp[3] * (1 - pos.time);

	// axes
	for(int i = 0; i < 3; ++i) {
		g.setColour(clrs[i]);
		g.drawLine(scaledExp[axes[i][0]].x, scaledExp[axes[i][0]].y,
				   scaledExp[axes[i][1]].x, scaledExp[axes[i][1]].y, 3);
	}

	Vertex2 midPoint = ((scaledUnit[2] * (1 - pos.blue) + scaledUnit[0] * pos.blue) * (1 - pos.red) +
					    (scaledUnit[3] * (1 - pos.blue) + scaledUnit[1] * pos.blue) * pos.red) * (1 - pos.time) +
					   ((scaledUnit[6] * (1 - pos.blue) + scaledUnit[4] * pos.blue) * (1 - pos.red) +
					    (scaledUnit[7] * (1 - pos.blue) + scaledUnit[5] * pos.blue) * pos.red) * pos.time;

	g.setColour(Colours::white);
    g.fillEllipse(midPoint.x - 1.f, midPoint.y - 1.f, 2.f, 2.f);

    if (cubeVerts[0].x != -1.f || cubeVerts[0].y != -1.f) {
        Path facePaths[6];

        for (int i = 0; i < 6; ++i) {
			Path& face = facePaths[i];
			face.startNewSubPath(scaledCube[faces[i][0]].x, scaledCube[faces[i][0]].y);
			face.lineTo(scaledCube[faces[i][1]].x, scaledCube[faces[i][1]].y);
			face.lineTo(scaledCube[faces[i][2]].x, scaledCube[faces[i][2]].y);
			face.lineTo(scaledCube[faces[i][3]].x, scaledCube[faces[i][3]].y);
			face.closeSubPath();
		}

        for (int i = 0; i < 6; ++i) {
            Path& face = facePaths[i];

			int clrIdx = (i == 0 || i == 1) ? 1 :
						 (i == 2 || i == 3) ? 0 : 2;

			g.setGradientFill(ColourGradient(clrs[clrIdx].withAlpha(0.7f), scaledCube[faces[i][1]].x, scaledCube[faces[i][1]].y,
			                                 clrs[clrIdx].withAlpha(0.3f), scaledCube[faces[i][3]].x, scaledCube[faces[i][3]].x, true));
			g.fillPath(face);
		}

        for (int i = 0; i < (isEnvelope ? 4 : 8); ++i) {
			float sz = 4.f;

			g.setColour(Colours::black);
			g.fillEllipse(scaledCube[i].x - 0.5f * sz,
			              scaledCube[i].y - 0.5f * sz, sz, sz);

			sz = 1.5f;
			g.setColour(selected[i] ? Colours::red : Colours::white);
			g.fillEllipse(scaledCube[i].x - 0.5f * sz,
			              scaledCube[i].y - 0.5f * sz, sz, sz);
		}
	}
}

void CubeDisplay::resized() {
    float size = jmin(getWidth(), getHeight()) * 0.65;

    for (int i = 0; i < 8; ++i) {
		scaledUnit[i] = unit[i] * size;

		scaledExp[i].x 	= expUnit[i].x * size;
		scaledExp[i].y 	= expUnit[i].y * size;

		scaledExpB[i].x = expUnitB[i].x * size;
		scaledExpB[i].y = expUnitB[i].y * size;
	}

    Path unitPaths[6];

    for (int i = 0; i < 6; ++i) {
		Path& face = unitPaths[i];
		face.startNewSubPath(scaledUnit[faces[i][0]].x, scaledUnit[faces[i][0]].y);
		face.lineTo(scaledUnit[faces[i][1]].x, scaledUnit[faces[i][1]].y);
		face.lineTo(scaledUnit[faces[i][2]].x, scaledUnit[faces[i][2]].y);
		face.lineTo(scaledUnit[faces[i][3]].x, scaledUnit[faces[i][3]].y);
		face.closeSubPath();
	}

	backImage = Image(Image::ARGB, getWidth(), getHeight(), true);
	Graphics g(backImage);

    for (int i = 0; i < 6; ++i) {
        g.setGradientFill(ColourGradient(Colour::greyLevel(0.25f), 					scaledUnit[faces[i][0]].x, scaledUnit[faces[i][0]].y,
		                                 Colour::greyLevel(0.25f).withAlpha(0.f), 	scaledUnit[faces[i][2]].x, scaledUnit[faces[i][2]].x, true));

		g.fillPath(unitPaths[i]);
	}

	refreshCube();
}

void CubeDisplay::update(VertCube* cube, int selectedIdx, int scratchChannel, bool isEnvelope) {
    this->isEnvelope = isEnvelope;
	this->lastScratchChannel = scratchChannel;

	if(cube == nullptr)	{
		for(auto & cubeVert : cubeVerts)
			cubeVert = Vertex2(-1, -1);

		this->selectedIdx = -1;
	} else {
        this->selectedIdx = selectedIdx;

        for (int i = 0; i < VertCube::numVerts; ++i) {
			float key 	= cube->lineVerts[i]->values[Vertex::Red];
			float mod 	= 1 - cube->lineVerts[i]->values[Vertex::Blue];
			float time 	= isEnvelope ? 0 : cube->lineVerts[i]->values[Vertex::Time];

			Vertex2& vert = cubeVerts[i];

			vert.x = 0;
			vert.y = 0;

			vert.x += mat[0][0] * key;
			vert.x += mat[1][0] * mod;
			vert.x += mat[2][0] * time;
			vert.x += mat[3][0];

			vert.y += mat[0][1] * key;
			vert.y += mat[1][1] * mod;
			vert.y += mat[2][1] * time;
			vert.y += mat[3][1];
		}
	}

	linkingChanged();
	refreshCube();
	repaint();
}

void CubeDisplay::linkingChanged() {
    bool LinkYellow = getSetting(LinkYellow) == 1;
	bool LinkRed 	= getSetting(LinkRed) == 1;
	bool LinkBlue 	= getSetting(LinkBlue) == 1;

	int numLinks = int(LinkYellow) + int(LinkRed) + int(LinkBlue);

	for(bool& i : selected) {
		i = false;
	}

	if(selectedIdx < 0) {
		return;
	}

	int idx = selectedIdx;

	if(idx >= 0) {
		selected[idx] = true;
	}

    if (numLinks == 3) {
        for (bool& i : selected) {
	        i = true;
        }
    } else if (numLinks == 2) {
        if (!LinkBlue) {
            int addand = (idx == VertCube::y0r0b0 || idx == VertCube::y0r1b0 || idx == VertCube::y1r0b0 || idx == VertCube::y1r1b0) ? 0 : 1;
			selected[0 + addand] = true;
			selected[2 + addand] = true;
			selected[4 + addand] = true;
			selected[6 + addand] = true;
        } else if (!LinkRed) {
            int addand = (idx == VertCube::y0r0b0 || idx == VertCube::y0r0b1 || idx == VertCube::y1r0b0 || idx == VertCube::y1r0b1) ? 0 : 2;
			selected[0 + addand] = true;
			selected[4 + addand] = true;
			selected[5 + addand] = true;
            selected[1 + addand] = true;
        } else if (!LinkYellow) {
            int addand = (idx == VertCube::y0r0b0 || idx == VertCube::y0r0b1 || idx == VertCube::y0r1b0 || idx == VertCube::y0r1b1) ? 0 : 4;
			selected[0 + addand] = true;
			selected[1 + addand] = true;
			selected[2 + addand] = true;
			selected[3 + addand] = true;
		}
	} else if (numLinks == 1) {
		int addand = 0;

		if(LinkYellow)
			addand = (idx == VertCube::y0r0b0 || idx == VertCube::y0r0b1 || idx == VertCube::y0r1b0 || idx == VertCube::y0r1b1) ? 	4 : -4;
		else if(LinkRed)
			addand = (idx == VertCube::y0r0b0 || idx == VertCube::y0r0b1 || idx == VertCube::y1r0b0 || idx == VertCube::y1r0b1) ? 	2 : -2;
		else if(LinkBlue)
			addand = (idx == VertCube::y0r0b0 || idx == VertCube::y0r1b0 || idx == VertCube::y1r0b0 || idx == VertCube::y1r1b0) ?  	1 : -1;

		selected[idx + addand] = true;
	}

	repaint();
}

void CubeDisplay::refreshCube() {
    float size = jmin(getWidth(), getHeight()) * 0.65;

    for (int i = 0; i < 8; ++i) {
		scaledCube[i] = cubeVerts[i] * size;
	}
}

void CubeDisplay::handleAsyncUpdate() {
    pos = getObj(MorphPanel).getMorphPosition();

    if (isEnvelope) {
        pos.time = 0;
    } else if (lastScratchChannel != CommonEnums::Null) {
        pos.time = getObj(VisualDsp).getScratchPosition(lastScratchChannel);
	}

	repaint();
}
