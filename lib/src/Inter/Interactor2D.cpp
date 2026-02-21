#include "Interactor2D.h"
#include "../Algo/AutoModeller.h"
#include "../Algo/Resampling.h"
#include "../App/Settings.h"
#include "../App/SingletonRepo.h"
#include "../Array/BufferXY.h"
#include "../Curve/VertCube.h"
#include "../Inter/UndoableMeshProcess.h"
#include "../Thread/LockTracer.h"
#include "../UI/Panels/Panel.h"
#include "../Util/NumberUtils.h"
#include "../Util/ScopedBooleanSwitcher.h"
#include "../Definitions.h"

Interactor2D::Interactor2D(SingletonRepo* repo, const String& name, const Dimensions& d) :
        Interactor(repo, name, d)
    ,   SingletonAccessor(repo, name) {
}

bool Interactor2D::locateClosestElement() {
    RasterizerData& rastData = getRasterizer()->getRastData();

    if (rastData.intercepts.empty() && depthVerts.empty()) {
        return Interactor::locateClosestElement();
    }

    const vector<Intercept>& icpts = rastData.intercepts;

    float dist = 1e7f;
    float x = state.currentMouse.x;

    int oldIcptIdx = state.currentIcpt;
    int oldFreeIdx = state.currentFreeVert;

    int icptIdx = -1;
    int freeIdx = -1;

    int i = 0;
    for (auto& icpt : icpts) {
        if (fabsf(icpt.x - x) < dist) {
            dist = fabsf(icpt.x - x);
            icptIdx = i;
        }
        ++i;
    }

    i = 0;
    for (auto& vert : depthVerts) {
        if (fabsf(vert.x - x) < dist) {
            dist = fabsf(vert.x - x);
            freeIdx = i;
        }
    }

    ScopedLock sl(vertexLock);

    Vertex* lastCurrent = state.currentVertex;
    state.currentCube = nullptr;

    if (freeIdx != -1) {
        state.currentVertex     = depthVerts[freeIdx].vert;
        state.currentFreeVert   = freeIdx;
        state.currentIcpt       = -1;
    } else if (icptIdx != -1) {
        state.currentCube       = icpts[icptIdx].cube;

        if (state.currentCube == nullptr && icptIdx > 0) {
            state.currentCube = icpts[icptIdx - 1].cube;
        }

        state.currentFreeVert   = -1;
        state.currentIcpt       = icptIdx;

        if (state.currentCube == nullptr) {
            Mesh* mesh = getMesh();

            for (auto& vert : mesh->getVerts()) {
                if (fabsf(vert->values[dims.x] - icpts[state.currentIcpt].x) < 0.0001f) {
                    // when does this ever happen???
                    state.currentVertex = vert;
                    break;
                }
            }
        }
    }

    if (icptIdx != -1) {
        getStateValue(CurrentCurve) = icptIdx + rasterizer->getPaddingSize();
    }

    if (icptIdx != oldIcptIdx || freeIdx != oldFreeIdx) {
        flag(SimpleRepaint) = true;
    }

    // call virtual function that subclasses can use if wanted
    setExtraElements(x);

    return state.currentVertex != lastCurrent;
}

void Interactor2D::doExtraMouseMove(const MouseEvent& e) {
    ScopedLock sl(vertexLock);

    const float distThresPX = 7.f;

    RasterizerData& rastData = rasterizer->getRastData();

    Buffer<Float32> waveX = rastData.waveX;
    Buffer<Float32> waveY = rastData.waveY;
    bool inSelection     = finalSelection.contains(e.getPosition());

    if (inSelection || waveX.empty() || waveY.empty()) {
        return;
    }

    Vertex2 scaledMouse(panel->sx(state.currentMouse.x), panel->sy(state.currentMouse.y));

    int startIndex  = 0;
    int endIndex    = 0;
    float lowBound  = jlimit(waveX.front(), waveX.back(), panel->invertScaleX(scaledMouse.x - 3 * distThresPX));
    float highBound = jlimit(waveX.front(), waveX.back(), panel->invertScaleX(scaledMouse.x + 3 * distThresPX));

    startIndex      = Arithmetic::binarySearch(lowBound, waveX);
    endIndex        = Arithmetic::binarySearch(highBound, waveX);

    if (endIndex - startIndex < 10) {
        startIndex  = jmax(0, startIndex - 5);
        endIndex    = jmin(waveX.size() - 1, endIndex + 5);
    }

    int size = endIndex - startIndex;
    ScopedAlloc<Float32> scaleMem(2 * size);

    BufferXY xy;
    xy.x = scaleMem.place(size);
    xy.y = scaleMem.place(size);

    waveX.offset(startIndex).copyTo(xy.x);
    waveY.offset(startIndex).copyTo(xy.y);

    panel->applyScaleX(xy.x);
    panel->applyScaleY(xy.y);

    float distToCurve;

    mouseFlag(WithinReshapeThresh) = false;

    const Vertex2& c(scaledMouse);

    for (int i = 0; i < size - 1; ++i) {
        Vertex2 a(xy[i]);
        Vertex2 b(xy[i + 1]);

        float diff = a.dist2(b);
        if (diff == 0) {
            distToCurve = b.dist2(c);
        } else {
            float t = (c - a).dot(b - a) / diff;
            if (t < 0) {
                distToCurve = c.dist2(a);
            } else if (t > 1) {
                distToCurve = c.dist2(b);
            } else {
                Vertex2 vec = a + (b - a) * t;

                distToCurve = c.dist2(vec);
            }
        }

        distToCurve = sqrtf(distToCurve);

        if (distToCurve < distThresPX) {
            mouseFlag(WithinReshapeThresh) = true;
            break;
        }
    }

    flag(SimpleRepaint) = true;

//  progressMark
}

void Interactor2D::doExtraMouseDrag(const MouseEvent& e) {
    if (actionIs(PaintingEdit)) {
        commitPath(e);
    }
}

// needed to resolve case when an objectively closer vertex to the mouse belongs to
// another line, yet the mouse is closer yet to the a line's intercept in the 2D panel.
// intuitively the intercept's closest vertex along it's parent line should be selected
void Interactor2D::setExtraElements(float /*x*/) {
    ScopedLock sl(vertexLock);

    if (state.currentCube) {
        state.currentVertex = findLinesClosestVertex(state.currentCube, state.currentMouse);
        jassert(state.currentVertex->owners.contains(state.currentCube));
    }
}

void Interactor2D::commitPath(const MouseEvent& e) {
    MorphPosition morphPos = getModPosition();

    if (actionIs(PaintingCreate)) {
        vector<Intercept> reducedPath = getObj(AutoModeller).modelToPath(pencilPath, 1.f, false);

        if (reducedPath.size() < 2) {
            return;
        }

        UndoableMeshProcess commitPathProcess(this, "Pencil Draw");
        ScopedBooleanSwitcher sbs(suspendUndo);

        removeLinesInRange(Range(reducedPath.front().x, reducedPath.back().x), morphPos);

        float startTime = getYellow();

        for (auto& it : reducedPath) {
            addNewCube(startTime, it.x, it.y, it.shp);
        }

        flag(DidMeshChange) = true;
    } else if (actionIs(PaintingEdit)) {
        const vector<Intercept>& icpts = rasterizer->getRastData().intercepts;

        float diff;

        if (pencilPath.size() < 2) {
            return;
        }

        Vertex pos;
        pos[Vertex::Time]   = morphPos.time;
        pos[Vertex::Red]    = morphPos.red;
        pos[Vertex::Blue]   = morphPos.blue;
        pos[Vertex::Amp]    = -1;
        pos[Vertex::Phase]  = -1;

        // just use the last two
        int end = pencilPath.size() - 1;
        Vertex2 a(pencilPath[end - 1]);
        Vertex2 b(pencilPath[end]);

        if (a.x > b.x) {
            std::swap(a, b);
        }

        bool haveChanged = false;
        bool useLines = shouldDoDimensionCheck();

        if (useLines) {
            for (const auto & icpt : icpts) {
                if (! NumberUtils::within(icpt.x, a.x, b.x)) {
                    continue;
                }

                float curveY = Resampling::lerp(a.x, a.y, b.x, b.y, icpt.x);
                diff = curveY - icpt.y;

                VertCube* cube = icpt.cube;

                if (cube != nullptr) {
                    Vertex* vNear = findLinesClosestVertex(cube, state.currentMouse, pos);
                    Array<Vertex*> innerMovingVerts = getVerticesToMove(cube, vNear);

                    for (auto* vert : innerMovingVerts) {
                        vert->values[dims.y] += diff;

                        NumberUtils::constrain(vert->values[dims.y], vertexLimits[dims.y]);
                    }

                    haveChanged = true;
                } else {
                    cout << "don't know which cube to use for pencil edit" << "\n";
                }
            }
        } else if (Mesh* mesh = getMesh()) {
            if (pencilPath.size() < 2) {
                return;
            }

            // only use the last line segment of path each update
            int size  = (int) pencilPath.size();
            Vertex2 c = pencilPath[size - 2];
            Vertex2 d = pencilPath[size - 1];

            if (c.x > d.x) {
                std::swap(c, d);
            }

            if (c.x == d.x) {
                return;
            }

            for (auto& vert : mesh->getVerts()) {
                Vertex2 icpt(vert->values[dims.x], vert->values[dims.y]);

                if (NumberUtils::within(icpt.x, c.x, d.x)) {
                    float curveY = Resampling::lerp(c.x, c.y, d.x, d.y, icpt.x);
                    NumberUtils::constrain(curveY, 0.f, 1.f);

                    vert->values[dims.y] = curveY;
                    haveChanged = true;
                }
            }
        }

        flag(DidMeshChange) |= haveChanged;
    }
}

void Interactor2D::doReshapeCurve(const MouseEvent& e) {
    RasterizerData& data = rasterizer->getRastData();

    if (data.curves.empty()) {
        return;
    }

    flag(LoweredRes) = true;

    const vector<Curve>& curves = data.curves;

    {
        ScopedLock sl(vertexLock);

        vector<Vertex*>& selected = getSelected();
        selected.clear();

        if (state.currentVertex != nullptr) {
            selected.push_back(state.currentVertex);
            updateSelectionFrames();
        }
    }

    resetFinalSelection();

    Array<Vertex*> movingVerts = getVerticesToMove(state.currentCube, state.currentVertex);

    float dragScale = getDragMovementScale(state.currentCube);
    float diffY     = (state.currentMouse.y - state.lastMouse.y) / sqrtf(panel->getZoomPanel()->rect.h);
    float diff      = diffY * dragScale / (0.1f + curves[getStateValue(CurrentCurve)].tp.scaleY);
    int pole        = curves[getStateValue(CurrentCurve)].tp.ypole;

    for (auto& vert : movingVerts) {
        float& weight = vert->values[Vertex::Curve];
        weight += diff * pole;

        NumberUtils::constrain(weight, 0.f, 1.f);
    }

    listeners.call(&InteractorListener::selectionChanged, getMesh(), state.selectedFrame);

    flag(DidMeshChange) |= fabs(diff) > 0;
}

void Interactor2D::removeLinesInRange(Range<float> phsRange, const MorphPosition& pos) {
    Mesh* mesh = getMesh();
    vector<Vertex*> vertsToDelete;

    // remove interfering lines
    if (dims.numHidden() > 0) {
        bool overlapsAll;

        vector<VertCube*> linesToDelete;

        for (auto& cube : mesh->getCubes()) {
            cube->getFinalIntercept(reduceData, pos);

            if (reduceData.pointOverlaps && phsRange.contains(reduceData.v.values[dims.x])) {
                linesToDelete.push_back(cube);
}
        }

        listeners.call(&InteractorListener::cubesRemoved, linesToDelete);

        for (auto& cube : linesToDelete) {
            if (mesh->removeCube(cube)) {
                for (auto vert : cube->lineVerts) {
                    if (vert->getNumOwners() == 1) {
                        mesh->removeVert(vert);
                    }
                }
            }
        }
    } else {
        for (auto& vert : mesh->getVerts()) {
            if (phsRange.contains(vert->values[dims.x])) {
                mesh->removeVert(vert);
            }
        }
    }
}

bool Interactor2D::doCreateVertex() {
    progressMark

    ScopedLock sl(vertexLock);

    vector<Vertex*>& selected = getSelected();
    selected.clear();

    float startTime = getYellow();
    // todo what is the curve shape?
    bool succeeded = addNewCube(startTime, state.currentMouse.x, state.currentMouse.y, 0.f);

    if (state.currentVertex != nullptr) {
        selected.push_back(state.currentVertex);
        flag(SimpleRepaint) = true;
    }

    updateSelectionFrames();

    return succeeded;
}

void Interactor2D::mouseDoubleClick(const MouseEvent& e) {
}

float Interactor2D::getVertexClickProximityThres() {
    return 0.15f;
}

Range<float> Interactor2D::getVertexPhaseLimits(Vertex* vert) {
    vector<Vertex*>& selected   = getSelected();
    RasterizerData& rastData    = getRasterizer()->getRastData();
    ModifierKeys keys           = ModifierKeys::getCurrentModifiers();

    bool testAdjacent = keys.isAltDown() && selected.size() == 1 && ! rastData.intercepts.empty();

    if (testAdjacent) {
        float maximum = panel->getZoomPanel()->rect.xMaximum;

        const vector<Intercept>& icpts = rastData.intercepts;
        Range<float> limits = vertexLimits[dims.x];

        for (int i = 0; i < (int) icpts.size(); ++i) {
            const Intercept& icpt = icpts[i];

            if (NumberUtils::within(icpt.x, 0.f, maximum) && vert->isOwnedBy(icpt.cube)) {
                if (i < icpts.size() - 1) {
                    int j = i + 1;

                    while (j + 1 < icpts.size()) {
                        if (icpts[j].cube != nullptr) {
                            break;
                        }

                        ++j;
                    }

                    limits.setEnd(icpts[j].x - 0.0005f);
                }

                if (i > 0) {
                    int j = i - 1;
                    while (j - 1 > 0) {
                        if (icpts[j].cube != nullptr) {
                            break;
                        }

                        --j;
                    }
                    limits.setStart(icpts[j].x + 0.0005f);
                }

                return limits;
            }

            if (vert->getNumOwners() == 0 && fabsf(icpt.x - vert->values[dims.x]) < 0.0001f) {
                if (i < icpts.size() - 1) {
                    limits.setEnd(icpts[i + 1].x - 0.0005f);
                }

                if (i > 0) {
                    limits.setStart(icpts[i - 1].x + 0.0005f);
                }

                return limits;
            }
        }
    }

    return Interactor::getVertexPhaseLimits(vert);
}
